// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_state_service.cc

#include "weglet/browser/weglet_state_service.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "weglet/browser/weglet_bridge.h"
#include "weglet/browser/weglet_security_guard.h"

namespace weglet {
namespace {

// The address is the key; the contents are never read.
const char kUserDataKey[] = "weglet_state_service";

base::ListValue TabsToValue(const std::vector<WegletBridge::TabInfo>& tabs) {
  // base::ListValue and base::DictValue, not the nested Value::List and
  // Value::Dict: they are top-level classes in this version.
  base::ListValue list;
  for (const WegletBridge::TabInfo& info : tabs) {
    base::DictValue entry;
    // Ids cross as strings: a JavaScript number is a double and cannot
    // hold every u64 exactly, and a silently rounded tab id shows up as
    // "the wrong tab closed".
    entry.Set("id", base::NumberToString(info.id));
    entry.Set("url", info.url);
    entry.Set("label", info.label);
    entry.Set("canGoBack", info.can_go_back);
    entry.Set("canGoForward", info.can_go_forward);
    entry.Set("loading", info.loading);
    list.Append(base::Value(std::move(entry)));
  }
  return list;
}

}  // namespace

// static
void WegletStateService::CreateForBrowserContext(
    content::BrowserContext* browser_context,
    WegletBridge* bridge,
    WegletSecurityGuard* guard) {
  browser_context->SetUserData(
      kUserDataKey, std::make_unique<WegletStateService>(bridge, guard));
}

// static
WegletStateService* WegletStateService::FromBrowserContext(
    content::BrowserContext* browser_context) {
  if (!browser_context) {
    return nullptr;
  }
  return static_cast<WegletStateService*>(
      browser_context->GetUserData(kUserDataKey));
}

WegletStateService::WegletStateService(WegletBridge* bridge,
                                      WegletSecurityGuard* guard)
    : bridge_(bridge), guard_(guard) {}

WegletStateService::~WegletStateService() = default;

void WegletStateService::AddPage(content::WebUI* web_ui,
                                 contract::PageKind kind,
                                 uint64_t window) {
  if (!web_ui || kind == contract::PageKind::kOther) {
    return;
  }
  pages_.push_back(Entry{web_ui, kind, window});
}

void WegletStateService::RemovePage(content::WebUI* web_ui) {
  std::erase_if(pages_, [web_ui](const Entry& entry) {
    return entry.web_ui == web_ui;
  });
}

// static
bool WegletStateService::Affects(uint32_t changes, contract::PageKind kind) {
  switch (kind) {
    case contract::PageKind::kToolbar:
      return (changes & kTabs) != 0;
    case contract::PageKind::kNewtab:
      // Settings as well as shortcuts: the line under the search field
      // names the configured engine, so changing the engine changes what
      // this page says.
      return (changes & (kShortcuts | kSettings)) != 0;
    case contract::PageKind::kSettings:
      return (changes & kSettings) != 0;
    case contract::PageKind::kSecurityNotice:
      // Pushed when the page asks, not on a change: what it shows belongs
      // to one stopped navigation and never changes under it.
      return false;
    case contract::PageKind::kOther:
      return false;
  }
  return false;
}

void WegletStateService::RequestOmniboxFocus(uint64_t window) {
  omnibox_focus_pending_.insert(window);
  Notify(kTabs);
}

std::optional<base::DictValue> WegletStateService::BuildFor(
    const Entry& entry,
    content::WebContents* contents) {
  switch (entry.kind) {
    case contract::PageKind::kToolbar: {
      // Its own window's tabs, not every tab that exists. This is where a
      // flat model would have shown one window's strip in both.
      base::DictValue state;
      state.Set("tabs", base::Value(TabsToValue(bridge_->Tabs(entry.window))));
      state.Set("activeId",
                base::NumberToString(bridge_->ActiveTabId(entry.window)));
      // Consumed here: one keystroke, one focus. Leaving it set would
      // pull focus back on the next title change of any tab.
      state.Set("focusOmnibox",
                omnibox_focus_pending_.erase(entry.window) > 0);
      return state;
    }
    case contract::PageKind::kNewtab: {
      base::ListValue shortcuts;
      for (const WegletBridge::Shortcut& shortcut : bridge_->Shortcuts()) {
        base::DictValue item;
        item.Set("title", shortcut.title);
        item.Set("url", shortcut.url);
        shortcuts.Append(base::Value(std::move(item)));
      }
      base::DictValue state;
      state.Set("shortcuts", base::Value(std::move(shortcuts)));
      state.Set("hint", bridge_->NewTabHint());
      return state;
    }
    case contract::PageKind::kSettings: {
      const WegletBridge::SettingsSnapshot snapshot = bridge_->Settings();
      base::ListValue engines;
      for (const WegletBridge::EngineChoice& engine : snapshot.engines) {
        base::DictValue item;
        item.Set("id", engine.id);
        item.Set("label", engine.label);
        engines.Append(base::Value(std::move(item)));
      }
      base::ListValue blocked;
      for (const std::string& host : snapshot.blocked_hosts) {
        blocked.Append(base::Value(host));
      }
      base::DictValue state;
      state.Set("engines", base::Value(std::move(engines)));
      state.Set("searchEngine", snapshot.search_engine);
      state.Set("customSearchUrl", snapshot.custom_search_url);
      state.Set("restoreSession", snapshot.restore_session);
      state.Set("blockedHosts", base::Value(std::move(blocked)));
      state.Set("accentColor", snapshot.accent_color);
      state.Set("addressBarShape", snapshot.address_bar_shape);
      return state;
    }
    case contract::PageKind::kSecurityNotice: {
      if (!guard_) {
        return std::nullopt;
      }
      const WegletSecurityGuard::Notice* notice =
          guard_->PendingNotice(contents);
      if (!notice) {
        return std::nullopt;
      }
      base::DictValue state;
      // The level decides whether the page offers a way through at all,
      // so it is sent as the word the page checks rather than as a flag
      // it would have to interpret.
      state.Set("level", notice->blocking ? "block" : "warning");
      state.Set("title", notice->title);
      state.Set("reason", notice->reason);
      state.Set("host", notice->host);
      state.Set("target", notice->target.spec());
      return state;
    }
    case contract::PageKind::kOther:
      return std::nullopt;
  }
  return std::nullopt;
}

void WegletStateService::SendTo(const Entry& entry) {
  if (!entry.web_ui || !entry.web_ui->CanCallJavascript()) {
    return;
  }
  const std::string_view function = contract::PushFunctionFor(entry.kind);
  if (function.empty()) {
    return;
  }
  std::optional<base::DictValue> state =
      BuildFor(entry, entry.web_ui->GetWebContents());
  if (!state.has_value()) {
    return;
  }
  // The function name comes from the generated contract, which the page's
  // own TypeScript installs from -- so the two spellings are one string.
  entry.web_ui->CallJavascriptFunctionUnsafe(
      function, base::Value(std::move(*state)));
}

void WegletStateService::PushTo(content::WebUI* web_ui) {
  for (const Entry& entry : pages_) {
    if (entry.web_ui == web_ui) {
      SendTo(entry);
      return;
    }
  }
}

void WegletStateService::Notify(uint32_t changes) {
  if (changes == kNone) {
    return;
  }
  pending_ |= changes;
  if (flush_scheduled_) {
    return;
  }
  flush_scheduled_ = true;
  // Coalesced to the end of the current task. A single navigation fires
  // DidStartLoading, DidFinishNavigation, TitleWasSet and DidStopLoading;
  // pushing on each of them sent the whole tab list four times for one
  // page load.
  //
  // WeakPtr because the profile can go away between the post and the run.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&WegletStateService::FlushNow,
                                weak_factory_.GetWeakPtr()));
}

void WegletStateService::FlushNow() {
  flush_scheduled_ = false;
  const uint32_t changes = pending_;
  pending_ = kNone;
  if (changes == kNone) {
    return;
  }
  // Copied: sending calls into a renderer, which can destroy a page and
  // so mutate pages_ underneath the loop.
  const std::vector<Entry> snapshot = pages_;
  for (const Entry& entry : snapshot) {
    if (!Affects(changes, entry.kind)) {
      continue;
    }
    // Still registered? SendTo checks the WebUI, but the entry itself may
    // have been removed by an earlier iteration.
    const bool present = std::any_of(
        pages_.begin(), pages_.end(), [&entry](const Entry& live) {
          return live.web_ui == entry.web_ui;
        });
    if (present) {
      SendTo(entry);
    }
  }
}

}  // namespace weglet