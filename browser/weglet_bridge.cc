// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_bridge.cc

#include "weglet/browser/weglet_bridge.h"

#include "base/check.h"
#include "url/gurl.h"
#include "weglet/rust/weglet_ffi.h"

namespace weglet {

WegletBridge::WegletBridge() : state_(weglet_state_new()) {
  // Nothing downstream can work without it, and every method would have
  // to check. Failing here means failing at startup, where the browser
  // main parts can still put a message on screen.
  CHECK(state_) << "could not create the Weglet state";
}

WegletBridge::~WegletBridge() {
  weglet_state_free(state_);
  state_ = nullptr;
}

// static
std::string WegletBridge::TakeString(char* owned) {
  if (!owned) {
    return std::string();
  }
  std::string copy(owned);
  weglet_string_free(owned);
  return copy;
}

std::vector<uint64_t> WegletBridge::Windows() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const size_t count = weglet_window_count(state_);
  std::vector<uint64_t> windows;
  windows.reserve(count);
  for (size_t index = 0; index < count; ++index) {
    windows.push_back(weglet_window_id_at(state_, index));
  }
  return windows;
}

uint64_t WegletBridge::OpenWindow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_open_window(state_);
}

bool WegletBridge::CloseWindow(uint64_t window) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_close_window(state_, window);
}

uint64_t WegletBridge::TabWindow(uint64_t id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_tab_window(state_, id);
}

std::vector<WegletBridge::TabInfo> WegletBridge::Tabs(uint64_t window) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const size_t count = weglet_tab_count(state_, window);
  std::vector<TabInfo> tabs;
  tabs.reserve(count);
  for (size_t index = 0; index < count; ++index) {
    TabInfo info;
    info.id = weglet_tab_id_at(state_, window, index);
    info.url = TakeString(weglet_tab_url(state_, info.id));
    info.label = TakeString(weglet_tab_label(state_, info.id));
    info.can_go_back = weglet_tab_can_go_back(state_, info.id);
    info.can_go_forward = weglet_tab_can_go_forward(state_, info.id);
    info.loading = weglet_tab_loading(state_, info.id);
    tabs.push_back(std::move(info));
  }
  return tabs;
}

uint64_t WegletBridge::ActiveTabId(uint64_t window) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_active_tab_id(state_, window);
}

std::string WegletBridge::TabUrl(uint64_t id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return TakeString(weglet_tab_url(state_, id));
}

uint64_t WegletBridge::OpenTab(uint64_t window, const std::string& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_open_tab(state_, window, url.c_str());
}

bool WegletBridge::CloseTab(uint64_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_close_tab(state_, id);
}

bool WegletBridge::ActivateTab(uint64_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_activate_tab(state_, id);
}

void WegletBridge::CycleTab(uint64_t window, bool forward) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weglet_cycle_tab(state_, window, forward);
}

bool WegletBridge::ActivateTabAt(uint64_t window, size_t position) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_activate_tab_at(state_, window, position);
}

bool WegletBridge::ReorderTab(uint64_t id, size_t target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_reorder_tab(state_, id, target);
}

void WegletBridge::Navigated(uint64_t id, const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weglet_tab_navigated(state_, id, url.spec().c_str());
}

void WegletBridge::UrlReplaced(uint64_t id, const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weglet_tab_url_replaced(state_, id, url.spec().c_str());
}

void WegletBridge::TitleChanged(uint64_t id, const std::string& title) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weglet_tab_title_changed(state_, id, title.c_str());
}

void WegletBridge::LoadingChanged(uint64_t id, bool loading) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weglet_tab_loading_changed(state_, id, loading);
}

std::string WegletBridge::GoBack(uint64_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return TakeString(weglet_tab_go_back(state_, id));
}

std::string WegletBridge::GoForward(uint64_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return TakeString(weglet_tab_go_forward(state_, id));
}

std::vector<WegletBridge::Shortcut> WegletBridge::Shortcuts() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const size_t count = weglet_shortcut_count(state_);
  std::vector<Shortcut> shortcuts;
  shortcuts.reserve(count);
  for (size_t index = 0; index < count; ++index) {
    shortcuts.push_back({
        TakeString(weglet_shortcut_title(state_, index)),
        TakeString(weglet_shortcut_url(state_, index)),
    });
  }
  return shortcuts;
}

std::string WegletBridge::NewTabHint() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return TakeString(weglet_new_tab_hint(state_));
}

bool WegletBridge::AddShortcut(const std::string& title, const std::string& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_add_shortcut(state_, title.c_str(), url.c_str());
}

bool WegletBridge::EditShortcut(size_t index,
                                const std::string& title,
                                const std::string& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_edit_shortcut(state_, index, title.c_str(), url.c_str());
}

bool WegletBridge::RemoveShortcut(size_t index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_remove_shortcut(state_, index);
}

std::string WegletBridge::ResolveOmnibox(const std::string& input) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return TakeString(weglet_omnibox_resolve(state_, input.c_str()));
}

namespace {

// The integers weglet_assess_navigation returns. Anything unrecognised is
// "nothing to say" rather than a block: a mismatch between the two sides
// must not make the browser refuse to load pages.
WegletBridge::Risk RiskFromLevel(uint32_t level) {
  switch (level) {
    case 2:
      return WegletBridge::Risk::kBlock;
    case 1:
      return WegletBridge::Risk::kWarning;
    default:
      return WegletBridge::Risk::kNone;
  }
}

}  // namespace

WegletBridge::RiskAssessment WegletBridge::AssessNavigation(
    const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  char* title = nullptr;
  char* reason = nullptr;
  char* host = nullptr;
  const uint32_t level =
      weglet_assess_navigation(url.spec().c_str(), &title, &reason, &host);

  RiskAssessment assessment;
  assessment.level = RiskFromLevel(level);
  // Taken unconditionally: the Rust side writes every non-null out-param
  // even on a "nothing to say" answer, so all three are owned strings
  // that have to be freed whatever the level was.
  assessment.title = TakeString(title);
  assessment.reason = TakeString(reason);
  assessment.host = TakeString(host);
  return assessment;
}

WegletBridge::Risk WegletBridge::AssessNavigationLevel(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Nulls: the caller wants the decision, not the wording.
  return RiskFromLevel(weglet_assess_navigation(url.spec().c_str(), nullptr,
                                                nullptr, nullptr));
}

bool WegletBridge::IsHostBlocked(const std::string& host) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_is_host_blocked(state_, host.c_str());
}

WegletBridge::SettingsSnapshot WegletBridge::Settings() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SettingsSnapshot snapshot;

  const size_t engine_count = weglet_engine_count();
  snapshot.engines.reserve(engine_count);
  for (size_t index = 0; index < engine_count; ++index) {
    snapshot.engines.push_back({
        TakeString(weglet_engine_id_at(index)),
        TakeString(weglet_engine_label_at(index)),
    });
  }

  snapshot.search_engine = TakeString(weglet_search_engine(state_));
  snapshot.custom_search_url = TakeString(weglet_custom_search_url(state_));
  snapshot.restore_session = weglet_restore_session(state_);
  snapshot.accent_color = TakeString(weglet_accent_color(state_));
  snapshot.address_bar_shape = TakeString(weglet_address_bar_shape(state_));

  const size_t blocked_count = weglet_blocked_host_count(state_);
  snapshot.blocked_hosts.reserve(blocked_count);
  for (size_t index = 0; index < blocked_count; ++index) {
    snapshot.blocked_hosts.push_back(
        TakeString(weglet_blocked_host_at(state_, index)));
  }

  return snapshot;
}

bool WegletBridge::SetSearchEngine(const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_set_search_engine(state_, id.c_str());
}

void WegletBridge::SetCustomSearchUrl(const std::string& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weglet_set_custom_search_url(state_, url.c_str());
}

void WegletBridge::SetRestoreSession(bool on) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weglet_set_restore_session(state_, on);
}

bool WegletBridge::SetAccentColor(const std::string& color) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_set_accent_color(state_, color.c_str());
}

bool WegletBridge::SetAddressBarShape(const std::string& shape) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_set_address_bar_shape(state_, shape.c_str());
}

bool WegletBridge::BlockHost(const std::string& host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_block_host(state_, host.c_str());
}

bool WegletBridge::UnblockHost(size_t index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_unblock_host(state_, index);
}

WegletBridge::RiskAssessment WegletBridge::CheckBlockList(
    const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  char* title = nullptr;
  char* reason = nullptr;
  const bool blocked =
      weglet_is_url_blocked(state_, url.spec().c_str(), &title, &reason);

  RiskAssessment assessment;
  assessment.level = blocked ? Risk::kBlock : Risk::kNone;
  // Taken whatever the answer was: both are written either way, so both
  // are owned strings that have to be freed.
  assessment.title = TakeString(title);
  assessment.reason = TakeString(reason);
  assessment.host = std::string(url.host());
  return assessment;
}

bool WegletBridge::IsUrlBlocked(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Nulls: the caller wants the decision, not the wording.
  return weglet_is_url_blocked(state_, url.spec().c_str(), nullptr, nullptr);
}

bool WegletBridge::TermsAccepted() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_terms_accepted(state_);
}

void WegletBridge::AcceptTerms() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weglet_accept_terms(state_);
}

bool WegletBridge::SaveSession() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_save_session(state_);
}

bool WegletBridge::SettingsDirty() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_settings_dirty(state_);
}

base::TimeDelta WegletBridge::SettingsFlushInterval() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Seconds(weglet_settings_flush_seconds(state_));
}

base::TimeDelta WegletBridge::SessionSaveInterval() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Seconds(weglet_session_save_seconds(state_));
}

bool WegletBridge::FlushSettings() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_flush_settings(state_);
}

}  // namespace weglet