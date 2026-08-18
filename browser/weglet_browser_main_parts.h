// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_browser_main_parts.h

#ifndef WEGLET_BROWSER_WEGLET_BROWSER_MAIN_PARTS_H_
#define WEGLET_BROWSER_WEGLET_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/public/browser/browser_main_parts.h"

#if defined(USE_AURA)
namespace display {
class Screen;
}
namespace views {
class ViewsDelegate;
}
namespace wm {
class WMState;
}
#endif

namespace weglet {

class WegletBridge;
class WegletBrowserContext;

// Runs only in the browser process, and only once. This is where the
// profile is opened and the first window appears.
class WegletBrowserMainParts : public content::BrowserMainParts {
 public:
  WegletBrowserMainParts();
  WegletBrowserMainParts(const WegletBrowserMainParts&) = delete;
  WegletBrowserMainParts& operator=(const WegletBrowserMainParts&) = delete;
  ~WegletBrowserMainParts() override;

  WegletBrowserContext* browser_context() { return browser_context_.get(); }
  WegletBridge* bridge() { return bridge_.get(); }

  // content::BrowserMainParts:
  int PreMainMessageLoopRun() override;
  void WillRunMainMessageLoop(
      std::unique_ptr<base::RunLoop>& run_loop) override;
  void PostMainMessageLoopRun() override;

 private:
  std::unique_ptr<WegletBrowserContext> browser_context_;

  // The browser's own model -- tabs, history, settings, security -- living
  // in Rust. Created before the window, which reads it to know what to open,
  // and destroyed after it.
  std::unique_ptr<WegletBridge> bridge_;

  // Settings changes are marked in the model rather than written, so the
  // fsync an atomic write ends in does not land on the UI thread inside
  // the click that made the change. This is what actually writes them.
  // Cheap and a no-op when nothing changed.
  base::RepeatingTimer settings_flush_timer_;

  // The session used to be written only in PostMainMessageLoopRun, so a
  // crash or a kill lost every open tab -- while restore-session defaults
  // to on, which is a promise kept only on a clean exit.
  base::RepeatingTimer session_save_timer_;

  void FlushSettings();
  void SaveSession();

#if defined(USE_AURA)
  // Aura needs all three before any Widget can be created, and they
  // have to outlive every window.
  std::unique_ptr<wm::WMState> wm_state_;
  std::unique_ptr<views::ViewsDelegate> views_delegate_;
  std::unique_ptr<display::Screen> screen_;
#endif
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_BROWSER_MAIN_PARTS_H_
