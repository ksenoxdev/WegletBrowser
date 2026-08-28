// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// Browser-process startup and shutdown.

#ifndef WEGLET_BROWSER_WEGLET_BROWSER_MAIN_PARTS_H_
#define WEGLET_BROWSER_WEGLET_BROWSER_MAIN_PARTS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/public/browser/browser_main_parts.h"

class WindowsSpellChecker;

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
class WegletDownloadObserver;

// Runs once, in the browser process. Opens the profile and the first
// window.
class WegletBrowserMainParts : public content::BrowserMainParts {
 public:
  WegletBrowserMainParts();
  WegletBrowserMainParts(const WegletBrowserMainParts&) = delete;
  WegletBrowserMainParts& operator=(const WegletBrowserMainParts&) = delete;
  ~WegletBrowserMainParts() override;

  WegletBrowserContext* browser_context() { return browser_context_.get(); }
  WegletBridge* bridge() { return bridge_.get(); }
  WindowsSpellChecker* spell_checker() { return spell_checker_.get(); }

  // content::BrowserMainParts:
  int PreMainMessageLoopRun() override;
  void WillRunMainMessageLoop(
      std::unique_ptr<base::RunLoop>& run_loop) override;
  void PostMainMessageLoopRun() override;

 private:
  std::unique_ptr<WegletBrowserContext> browser_context_;

  // The browser's own model, in Rust. Created before the window that
  // reads it, destroyed after.
  std::unique_ptr<WegletBridge> bridge_;

  // Forwards content::DownloadManager's real downloads into the profile.
  std::unique_ptr<WegletDownloadObserver> download_observer_;

  // Wraps the Windows native spellchecker (ISpellCheckerFactory), so
  // WegletSpellCheckHost::RequestTextCheck has something to call into.
  // Owned here rather than per-BrowserContext: it is not profile data,
  // just a handle onto an OS service.
  std::unique_ptr<WindowsSpellChecker> spell_checker_;

  // Settings changes are marked in the model rather than written, so the
  // fsync does not land on the UI thread. This does the write, and is a
  // no-op when nothing changed.
  base::RepeatingTimer settings_flush_timer_;

  // On a timer as well as at shutdown: a crash between the two loses
  // every open tab, and restore-session defaults to on.
  base::RepeatingTimer session_save_timer_;

  void FlushSettings();
  void SaveSession();
  void OnSpellcheckLanguagesRetrieved(const std::vector<std::string>& lang_tags);

#if defined(USE_AURA)
  // Aura needs all three before any Widget is created, and they outlive
  // every window.
  std::unique_ptr<wm::WMState> wm_state_;
  std::unique_ptr<views::ViewsDelegate> views_delegate_;
  std::unique_ptr<display::Screen> screen_;
#endif

  base::WeakPtrFactory<WegletBrowserMainParts> weak_factory_{this};
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_BROWSER_MAIN_PARTS_H_
