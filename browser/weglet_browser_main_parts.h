// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_browser_main_parts.h

#ifndef WEGLET_BROWSER_WEGLET_BROWSER_MAIN_PARTS_H_
#define WEGLET_BROWSER_WEGLET_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "build/build_config.h"
#include "content/public/browser/browser_main_parts.h"
#include "url/gurl.h"

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

  // content::BrowserMainParts:
  int PreMainMessageLoopRun() override;
  void WillRunMainMessageLoop(
      std::unique_ptr<base::RunLoop>& run_loop) override;
  void PostMainMessageLoopRun() override;

 private:
  // The URL to open at startup: --url=... if given, otherwise a blank
  // page. A real home page arrives with the settings bridge in phase 2.
  static GURL StartupURL();

  std::unique_ptr<WegletBrowserContext> browser_context_;

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
