// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_bridge.h

#ifndef WEGLET_BROWSER_WEGLET_BRIDGE_H_
#define WEGLET_BROWSER_WEGLET_BRIDGE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "base/memory/raw_ptr_exclusion.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"

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
    bool loading = false;
  };

  struct Shortcut {
    std::string title;
    std::string url;
  };

  struct EngineChoice {
    std::string id;
    std::string label;
  };

  struct SettingsSnapshot {
    std::vector<EngineChoice> engines;
    std::string search_engine;
    std::string custom_search_url;
    bool restore_session = false;
    std::string accent_color;
    // "pill" | "rounded" | "square".
    std::string address_bar_shape;
    std::vector<std::string> blocked_hosts;
  };

  WegletBridge();
  WegletBridge(const WegletBridge&) = delete;
  WegletBridge& operator=(const WegletBridge&) = delete;
  ~WegletBridge();

  // Windows. Every tab question is asked about one: the model used to
  // hold a single flat tab list and a single active tab, so a second
  // window would have shown the same tabs and disagreed about which was
  // in front -- while this side already counted windows and quit when the
  // last one closed.
  std::vector<uint64_t> Windows() const;
  // 0 when the window ceiling is reached.
  uint64_t OpenWindow();
  bool CloseWindow(uint64_t window);
  // Which window a tab is in. 0 is a real id, so ask about the tab first
  // if you need to tell it from "no such tab".
  uint64_t TabWindow(uint64_t id) const;

  // Tabs, within one window. An id is global once you have it.
  std::vector<TabInfo> Tabs(uint64_t window) const;
  uint64_t ActiveTabId(uint64_t window) const;
  std::string TabUrl(uint64_t id) const;

  // 0 when the tab ceiling is reached.
  uint64_t OpenTab(uint64_t window, const std::string& url);
  bool CloseTab(uint64_t id);
  bool ActivateTab(uint64_t id);
  void CycleTab(uint64_t window, bool forward);
  // One-based, as on the keyboard. 9 means the last tab.
  bool ActivateTabAt(uint64_t window, size_t position);
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

  // Shortcuts: the pinned sites on the new tab page.
  std::vector<Shortcut> Shortcuts() const;
  std::string NewTabHint() const;
  // False when the dock is full.
  bool AddShortcut(const std::string& title, const std::string& url);
  bool EditShortcut(size_t index, const std::string& title, const std::string& url);
  bool RemoveShortcut(size_t index);

  // What the user typed in the address bar, resolved to a URL. Search
  // terms come back as a search URL for the configured engine.
  std::string ResolveOmnibox(const std::string& input) const;

  // What an assessment says. Advisory: a kWarning may be dismissed, a
  // kBlock may not.
  struct RiskAssessment {
    Risk level = Risk::kNone;
    std::string title;
    std::string reason;
    // Empty when the assessment has no host to show -- an unparseable
    // URL, for instance.
    std::string host;
  };

  // One call. There were four, and each re-ran the whole assessment from
  // the URL: punycode, the public suffix list, the skeleton fold and every
  // brand rule, four times over for one navigation.
  RiskAssessment AssessNavigation(const GURL& url) const;
  // Just the level, for a caller that only has to decide. Skips building
  // the three strings.
  Risk AssessNavigationLevel(const GURL& url) const;

  bool IsHostBlocked(const std::string& host) const;

  // The user's block list and the built-in one, asked about a whole URL.
  // The host is pulled out on the Rust side, where the parser that knows
  // a backslash ends the authority already lives.
  //
  // Shaped like AssessNavigation because the notice page cannot tell the
  // two apart and should not have to: `level` is kBlock or kNone, and the
  // wording comes from the same place as the heuristics'.
  RiskAssessment CheckBlockList(const GURL& url) const;
  bool IsUrlBlocked(const GURL& url) const;

  // Settings, gathered for the settings page in one call rather than a
  // dozen -- storage's own advice, and the page needs all of it every time
  // it renders anyway.
  SettingsSnapshot Settings() const;
  bool SetSearchEngine(const std::string& id);
  void SetCustomSearchUrl(const std::string& url);
  void SetRestoreSession(bool on);
  bool SetAccentColor(const std::string& color);
  bool SetAddressBarShape(const std::string& shape);
  // False if the host was already blocked or could not be canonicalised.
  bool BlockHost(const std::string& host);
  bool UnblockHost(size_t index);

  // Settings and session.
  bool TermsAccepted() const;
  void AcceptTerms();
  // False when the session could not be written; the caller logs it,
  // because losing the open tabs quietly is how a user finds out late.
  bool SaveSession() const;

  // Settings changes are marked, not written -- an atomic write ends in
  // fsync, and doing that inside the click that flipped a toggle put the
  // disk on the UI thread. FlushSettings does the write; the browser main
  // parts call it on a timer and again at shutdown. False when there was
  // something to write and it could not be written.
  bool SettingsDirty() const;
  bool FlushSettings();

  // How often to call the two above. From the profile and bounded there:
  // the right value depends on the machine, and neither a busy loop nor
  // "never" is reachable from the settings file.
  base::TimeDelta SettingsFlushInterval() const;
  base::TimeDelta SessionSaveInterval() const;

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
