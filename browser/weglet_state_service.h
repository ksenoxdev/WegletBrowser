// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_state_service.h

#ifndef WEGLET_BROWSER_WEGLET_STATE_SERVICE_H_
#define WEGLET_BROWSER_WEGLET_STATE_SERVICE_H_

#include <cstdint>
#include <optional>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/values.h"
#include "weglet/ui/generated_contract.h"

namespace content {
class BrowserContext;
class WebContents;
class WebUI;
}  // namespace content

namespace weglet {

class WegletBridge;
class WegletSecurityGuard;

// Who tells the pages that something changed.
//
// This used to live on WegletWindow, and that was the reason everything
// went through the window: changing the accent colour is not a window's
// business, but the window was the only thing that could tell the settings
// page about it. Pulling it out is what lets a settings message go
// straight from the handler to the model.
//
// Attached to the BrowserContext rather than owned by a window, because
// the pages outlive any one window and the state they show belongs to the
// profile, not to a frame.
//
// Two things it does that the window did not:
//
//   * A page says what kind it is when it registers, once, instead of the
//     browser re-deriving it by string-comparing the committed URL on
//     every push to every page.
//
//   * Changes are coalesced into one flush per turn of the message loop,
//     and only reach the pages that care. A title change used to
//     re-serialise the whole tab list, the whole shortcut list and the
//     whole settings snapshot to every open page -- and DidStartLoading,
//     DidStopLoading and TitleWasSet fire constantly, on every tab.
class WegletStateService : public base::SupportsUserData::Data {
 public:
  // What changed. A bitmask: several changes between two flushes are one
  // flush carrying all of them.
  enum Change : uint32_t {
    kNone = 0,
    // Which tabs exist, which is active, where each one is.
    kTabs = 1 << 0,
    // The new tab page's dock.
    kShortcuts = 1 << 1,
    // Anything in the profile's settings.
    kSettings = 1 << 2,
  };

  // Creates the service and hands it to `browser_context`, which owns it.
  // `bridge` must outlive the context.
  static void CreateForBrowserContext(content::BrowserContext* browser_context,
                                      WegletBridge* bridge,
                                      WegletSecurityGuard* guard);
  // Null if nothing was created for this context.
  static WegletStateService* FromBrowserContext(
      content::BrowserContext* browser_context);

  WegletStateService(WegletBridge* bridge, WegletSecurityGuard* guard);
  WegletStateService(const WegletStateService&) = delete;
  WegletStateService& operator=(const WegletStateService&) = delete;
  ~WegletStateService() override;

  // Called by a page's WebUI controller when it is created and destroyed.
  // `kind` comes from the URL the controller was created for, so it is
  // decided once rather than re-derived per push.
  // `window` is which window's tabs this page shows, and is meaningful
  // only for the toolbar -- every other page shows profile state, which
  // is the same in every window.
  void AddPage(content::WebUI* web_ui,
               contract::PageKind kind,
               uint64_t window = 0);
  void RemovePage(content::WebUI* web_ui);

  // Asks this window's toolbar to put the caret in the address bar.
  //
  // Ctrl+L is a keystroke the browser process sees and the page has to
  // act on -- focus lives in the DOM. Carried as a one-shot flag in the
  // toolbar's next state rather than as a message of its own, because the
  // contract allows one push function per page and a second one would
  // mean a second global to install and keep in step.
  //
  // Read and cleared by the push that carries it, so a later repaint for
  // an unrelated reason does not steal focus again.
  void RequestOmniboxFocus(uint64_t window);

  // Sends this page its state now, without waiting for a flush. For a
  // page that just registered, or one asking again after its own reload.
  void PushTo(content::WebUI* web_ui);

  // Records what changed and schedules one flush. Cheap to call often --
  // that is the point.
  void Notify(uint32_t changes);

  // The model these pages show. Handed out so a page's controller can
  // give it to the message handler without the factory having to reach
  // for the browser main parts.
  WegletBridge* bridge() { return bridge_; }

  // Sends every pending change immediately. Only needed when something
  // has to be on screen before the caller returns; ordinary callers use
  // Notify.
  void FlushNow();

 private:
  struct Entry {
    // Owned by the WebContents. Removed in RemovePage before it goes.
    raw_ptr<content::WebUI> web_ui = nullptr;
    contract::PageKind kind = contract::PageKind::kOther;
    uint64_t window = 0;
  };

  // True when a page of this kind shows anything affected by `changes`.
  static bool Affects(uint32_t changes, contract::PageKind kind);

  // The payload for a page of this kind, or nullopt when there is nothing
  // to send it.
  // `contents` is the page being built for. Only the security notice
  // needs it -- what it shows is about that tab, not about the profile.
  // Not const: building the toolbar's state consumes a pending focus
  // request.
  std::optional<base::DictValue> BuildFor(const Entry& entry,
                                          content::WebContents* contents);

  void SendTo(const Entry& entry);

  // Windows whose toolbar should take focus on its next push.
  std::set<uint64_t> omnibox_focus_pending_;

  const raw_ptr<WegletBridge> bridge_;
  const raw_ptr<WegletSecurityGuard> guard_;

  std::vector<Entry> pages_;

  // What has changed since the last flush, and whether one is already on
  // its way. Without the flag a hundred tab events in one task would post
  // a hundred flushes.
  uint32_t pending_ = kNone;
  bool flush_scheduled_ = false;

  base::WeakPtrFactory<WegletStateService> weak_factory_{this};
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_STATE_SERVICE_H_
