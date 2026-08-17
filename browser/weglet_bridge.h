// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_bridge.h

#ifndef WEGLET_BROWSER_WEGLET_BRIDGE_H_
#define WEGLET_BROWSER_WEGLET_BRIDGE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/memory/raw_ptr_exclusion.h"
#include "base/sequence_checker.h"

class GURL;

// The opaque handle from rust/weglet_ffi.h, forward-declared at global
// scope where the FFI header puts it. Declaring it inside namespace
// weglet instead would silently create a second, unrelated type -- which
// is exactly what it did.
//
// Declared rather than included so weglet_ffi.h stays confined to
// weglet_bridge.cc.
extern "C" {
struct WegletState;
}

namespace weglet {

// What the browser knows about its own tabs, held in Rust.
//
// Everything past this class is raw pointers and manual frees. Nothing
// outside this file includes weglet_ffi.h, so if the boundary ever needs
// changing there is exactly one place to look.
class WegletBridge {
 public:
  // 0 = nothing to say, 1 = warn, 2 = block. Mirrors the integers the
  // Rust side returns; see weglet_assess_navigation.
  enum class Risk {
    kNone = 0,
    kWarning = 1,
    kBlock = 2,
  };

  struct TabInfo {
    uint64_t id = 0;
    std::string url;
    // What to show in the tab strip: the page title, or its host until a
    // title arrives, or a name for one of our own pages.
    std::string label;
    bool can_go_back = false;
    bool can_go_forward = false;
  };

  WegletBridge();
  WegletBridge(const WegletBridge&) = delete;
  WegletBridge& operator=(const WegletBridge&) = delete;
  ~WegletBridge();

  // Tabs.
  std::vector<TabInfo> Tabs() const;
  uint64_t ActiveTabId() const;
  std::string TabUrl(uint64_t id) const;

  // 0 when the tab ceiling is reached.
  uint64_t OpenTab(const std::string& url);
  bool CloseTab(uint64_t id);
  bool ActivateTab(uint64_t id);
  void CycleTab(bool forward);
  // One-based, as on the keyboard. 9 means the last tab.
  bool ActivateTabAt(size_t position);
  bool ReorderTab(uint64_t id, size_t target);

  // Navigation. Call Navigated for a new page and UrlReplaced for a
  // redirect: the second does not add a history entry, so Back does not
  // land the user back on the page that redirected them.
  void Navigated(uint64_t id, const GURL& url);
  void UrlReplaced(uint64_t id, const GURL& url);
  void TitleChanged(uint64_t id, const std::string& title);
  void LoadingChanged(uint64_t id, bool loading);

  // Empty when there is nowhere to go.
  std::string GoBack(uint64_t id);
  std::string GoForward(uint64_t id);

  // What the user typed in the address bar, resolved to a URL. Search
  // terms come back as a search URL for the configured engine.
  std::string ResolveOmnibox(const std::string& input) const;

  // Security. Advisory: a kWarning may be dismissed, a kBlock may not.
  Risk AssessNavigation(const GURL& url) const;
  std::string RiskTitle(const GURL& url) const;
  std::string RiskReason(const GURL& url) const;
  bool IsHostBlocked(const std::string& host) const;

  // Settings and session.
  bool TermsAccepted() const;
  void AcceptTerms();
  // False when the session could not be written; the caller logs it,
  // because losing the open tabs quietly is how a user finds out late.
  bool SaveSession() const;

 private:
  // Takes a string from Rust and frees it. Every char* the FFI returns
  // goes through here, so none of them can leak by being forgotten.
  static std::string TakeString(char* owned);

  // Opaque handle into the Rust side.
  //
  // RAW_PTR_EXCLUSION: this points into Rust's allocator, not Chromium's.
  // raw_ptr instruments allocations made by PartitionAlloc; wrapping a
  // pointer it never allocated is at best meaningless and at worst a
  // false report. It is freed by weglet_state_free, not by delete, and it
  // lives exactly as long as this object.
  RAW_PTR_EXCLUSION ::WegletState* state_ = nullptr;

  // The Rust side has no locking, because the state it holds belongs to
  // one thread by design. This is what catches a call from the wrong one
  // in a debug build instead of at 3am in the field.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_BRIDGE_H_