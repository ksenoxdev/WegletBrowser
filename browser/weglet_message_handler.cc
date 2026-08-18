// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_message_handler.cc

#include "weglet/browser/weglet_message_handler.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "weglet/browser/weglet_bridge.h"
#include "weglet/browser/weglet_state_service.h"
#include "weglet/browser/weglet_window.h"

namespace weglet {
namespace {

// Read only after Validate has passed.
const std::string& EmptyString() {
  static const base::NoDestructor<std::string> empty;
  return *empty;
}

}  // namespace

WegletMessageHandler::WegletMessageHandler(WegletWindow* window,
                                          WegletBridge* bridge,
                                          WegletStateService* state)
    : window_(window), bridge_(bridge), state_(state) {}

WegletMessageHandler::~WegletMessageHandler() = default;

void WegletMessageHandler::RegisterMessages() {
  // Every message in the contract, registered from the contract.
  //
  // Registering all of them is not optional: content answers an
  // unregistered chrome.send() by crashing the browser process, so a name
  // the page can send with no callback here is a button that closes the
  // browser rather than one that does nothing. Driving the loop off
  // kMessages is what makes that structural -- there is no second list to
  // forget to add to, and no DCHECK needed to compare two lists that can
  // no longer disagree.
  for (const contract::MessageSpec& spec : contract::kMessages) {
    web_ui()->RegisterMessageCallback(
        spec.name, base::BindRepeating(&WegletMessageHandler::OnMessage,
                                       base::Unretained(this), &spec));
  }
}

// static
bool WegletMessageHandler::Validate(const contract::MessageSpec& spec,
                                    const base::ListValue& args) {
  if (args.size() != spec.arity) {
    LOG(WARNING) << spec.name << " sent " << args.size()
                 << " arguments, expected " << spec.arity << " -- ignored";
    return false;
  }
  for (size_t index = 0; index < spec.arity; ++index) {
    const base::Value& value = args[index];
    bool ok = false;
    switch (spec.args[index]) {
      case contract::ArgKind::kString:
        ok = value.GetIfString() != nullptr;
        break;
      case contract::ArgKind::kNumber:
        // Non-negative: every number in the contract is a position in a
        // list the page was shown, and a negative one is not one.
        ok = value.GetIfInt().value_or(-1) >= 0;
        break;
      case contract::ArgKind::kBoolean:
        ok = value.GetIfBool().has_value();
        break;
      case contract::ArgKind::kTabId: {
        // Ids cross as strings because a JavaScript number is a double
        // and cannot hold every u64 exactly; a silently rounded tab id
        // shows up as "the wrong tab closed".
        const std::string* text = value.GetIfString();
        uint64_t ignored = 0;
        ok = text && base::StringToUint64(*text, &ignored);
        break;
      }
    }
    if (!ok) {
      LOG(WARNING) << spec.name << " argument " << index
                   << " is not the type the contract declares -- ignored";
      return false;
    }
  }
  return true;
}

void WegletMessageHandler::OnMessage(const contract::MessageSpec* spec,
                                     const base::ListValue& args) {
  if (!Validate(*spec, args)) {
    return;
  }
  if (!spec->implemented) {
    // Registered, not acted on. The gap is in the log rather than felt as
    // a crash, and contract.json says which messages these are.
    LOG(WARNING) << "chrome.send(\"" << spec->name
                 << "\") is registered but not implemented yet";
    return;
  }
  Dispatch(spec->name, args);
}

// static
const std::string& WegletMessageHandler::StringAt(const base::ListValue& args,
                                                  size_t index) {
  const std::string* value = args[index].GetIfString();
  return value ? *value : EmptyString();
}

// static
uint64_t WegletMessageHandler::TabIdAt(const base::ListValue& args,
                                       size_t index) {
  uint64_t id = 0;
  base::StringToUint64(StringAt(args, index), &id);
  return id;
}

// static
size_t WegletMessageHandler::IndexAt(const base::ListValue& args,
                                     size_t index) {
  return static_cast<size_t>(args[index].GetIfInt().value_or(0));
}

// static
bool WegletMessageHandler::BoolAt(const base::ListValue& args, size_t index) {
  return args[index].GetIfBool().value_or(false);
}

void WegletMessageHandler::Dispatch(std::string_view name,
                                    const base::ListValue& args) {
  // A page asking for a repaint -- after its own reload, or because it
  // just registered and has nothing yet. Answered directly rather than
  // through a coalesced notify: the page is waiting for it.
  if (name == "requestState") {
    if (state_) {
      state_->PushTo(web_ui());
    }
    return;
  }

  // --- settings and the profile -------------------------------------
  //
  // None of these mention a window. They used to, only because the window
  // owned the code that told the pages about a change; that is the state
  // service now.
  if (name == "addShortcut") {
    if (bridge_->AddShortcut(StringAt(args, 0), StringAt(args, 1))) {
      state_->Notify(WegletStateService::kShortcuts);
    }
    return;
  }
  if (name == "editShortcut") {
    if (bridge_->EditShortcut(IndexAt(args, 0), StringAt(args, 1),
                              StringAt(args, 2))) {
      state_->Notify(WegletStateService::kShortcuts);
    }
    return;
  }
  if (name == "removeShortcut") {
    if (bridge_->RemoveShortcut(IndexAt(args, 0))) {
      state_->Notify(WegletStateService::kShortcuts);
    }
    return;
  }
  if (name == "setSearchEngine") {
    if (bridge_->SetSearchEngine(StringAt(args, 0))) {
      state_->Notify(WegletStateService::kSettings);
    }
    return;
  }
  if (name == "setCustomSearchUrl") {
    bridge_->SetCustomSearchUrl(StringAt(args, 0));
    state_->Notify(WegletStateService::kSettings);
    return;
  }
  if (name == "setRestoreSession") {
    bridge_->SetRestoreSession(BoolAt(args, 0));
    state_->Notify(WegletStateService::kSettings);
    return;
  }
  if (name == "setAccentColor") {
    if (bridge_->SetAccentColor(StringAt(args, 0))) {
      state_->Notify(WegletStateService::kSettings);
    }
    return;
  }
  if (name == "setAddressBarShape") {
    if (bridge_->SetAddressBarShape(StringAt(args, 0))) {
      state_->Notify(WegletStateService::kSettings);
    }
    return;
  }
  if (name == "blockHost") {
    if (bridge_->BlockHost(StringAt(args, 0))) {
      state_->Notify(WegletStateService::kSettings);
    }
    return;
  }
  if (name == "unblockHost") {
    if (bridge_->UnblockHost(IndexAt(args, 0))) {
      state_->Notify(WegletStateService::kSettings);
    }
    return;
  }

  // --- tabs, navigation and the notice -------------------------------
  //
  // These are a window's business, and a page open outside one has no
  // window to ask.
  if (!window_) {
    LOG(WARNING) << name << " needs a window and this page is not in one";
    return;
  }
  if (name == "navigate") {
    window_->NavigateActiveTabFromOmnibox(StringAt(args, 0));
  } else if (name == "goBack") {
    window_->GoBack();
  } else if (name == "goForward") {
    window_->GoForward();
  } else if (name == "reload") {
    window_->Reload();
  } else if (name == "newTab") {
    window_->OpenNewTab();
  } else if (name == "closeTab") {
    window_->CloseTab(TabIdAt(args, 0));
  } else if (name == "activateTab") {
    window_->ActivateTab(TabIdAt(args, 0));
  } else if (name == "openSettings") {
    window_->OpenSettings();
  } else if (name == "securityDismiss") {
    window_->DismissSecurityNotice(web_ui()->GetWebContents());
  } else if (name == "securityProceed") {
    window_->ProceedPastSecurityNotice(web_ui()->GetWebContents());
  } else {
    // Reachable only if contract.json gained a message marked implemented
    // with nothing added here. Logged rather than ignored: the page will
    // look like it did nothing, and this says why.
    LOG(ERROR) << name
               << " is marked implemented in contract.json but has no case "
                  "in WegletMessageHandler::Dispatch";
  }
}

}  // namespace weglet
