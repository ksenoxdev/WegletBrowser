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

std::vector<WegletBridge::TabInfo> WegletBridge::Tabs() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const size_t count = weglet_tab_count(state_);
  std::vector<TabInfo> tabs;
  tabs.reserve(count);
  for (size_t index = 0; index < count; ++index) {
    TabInfo info;
    info.id = weglet_tab_id_at(state_, index);
    info.url = TakeString(weglet_tab_url(state_, info.id));
    info.label = TakeString(weglet_tab_label(state_, info.id));
    info.can_go_back = weglet_tab_can_go_back(state_, info.id);
    info.can_go_forward = weglet_tab_can_go_forward(state_, info.id);
    tabs.push_back(std::move(info));
  }
  return tabs;
}

uint64_t WegletBridge::ActiveTabId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_active_tab_id(state_);
}

std::string WegletBridge::TabUrl(uint64_t id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return TakeString(weglet_tab_url(state_, id));
}

uint64_t WegletBridge::OpenTab(const std::string& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_open_tab(state_, url.c_str());
}

bool WegletBridge::CloseTab(uint64_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_close_tab(state_, id);
}

bool WegletBridge::ActivateTab(uint64_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_activate_tab(state_, id);
}

void WegletBridge::CycleTab(bool forward) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weglet_cycle_tab(state_, forward);
}

bool WegletBridge::ActivateTabAt(size_t position) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_activate_tab_at(state_, position);
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

std::string WegletBridge::ResolveOmnibox(const std::string& input) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return TakeString(weglet_omnibox_resolve(state_, input.c_str()));
}

WegletBridge::Risk WegletBridge::AssessNavigation(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (weglet_assess_navigation(url.spec().c_str())) {
    case 2:
      return Risk::kBlock;
    case 1:
      return Risk::kWarning;
    default:
      // Anything unrecognised is treated as "nothing to say" rather than
      // as a block: a mismatch between the two sides must not make the
      // browser refuse to load pages.
      return Risk::kNone;
  }
}

std::string WegletBridge::RiskTitle(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return TakeString(weglet_risk_title(url.spec().c_str()));
}

std::string WegletBridge::RiskReason(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return TakeString(weglet_risk_reason(url.spec().c_str()));
}

bool WegletBridge::IsHostBlocked(const std::string& host) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weglet_is_host_blocked(state_, host.c_str());
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

}  // namespace weglet
