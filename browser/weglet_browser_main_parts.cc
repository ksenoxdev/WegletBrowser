// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_browser_main_parts.cc

#include "weglet/browser/weglet_browser_main_parts.h"

#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "content/public/common/result_codes.h"
#include "url/gurl.h"
#include "weglet/browser/weglet_browser_context.h"
#include "weglet/browser/weglet_window.h"

#if defined(USE_AURA)
#include "ui/display/screen.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/wm/core/wm_state.h"
#endif

namespace weglet {
namespace {

constexpr char kUrlSwitch[] = "url";

#if defined(USE_AURA)
// views::ViewsDelegate is abstract but every method has a usable
// default; this exists only because something has to be installed
// before the first Widget is created.
class WegletViewsDelegate : public views::ViewsDelegate {
 public:
  WegletViewsDelegate() = default;
  WegletViewsDelegate(const WegletViewsDelegate&) = delete;
  WegletViewsDelegate& operator=(const WegletViewsDelegate&) = delete;
  ~WegletViewsDelegate() override = default;
};
#endif

}  // namespace

WegletBrowserMainParts::WegletBrowserMainParts() = default;
WegletBrowserMainParts::~WegletBrowserMainParts() = default;

// static
GURL WegletBrowserMainParts::StartupURL() {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kUrlSwitch)) {
    GURL url(command_line->GetSwitchValueASCII(kUrlSwitch));
    if (url.is_valid()) {
      return url;
    }
    LOG(ERROR) << "--url is not a valid address, opening a blank page instead";
  }
  return GURL("about:blank");
}

int WegletBrowserMainParts::PreMainMessageLoopRun() {
#if defined(USE_AURA)
  wm_state_ = std::make_unique<wm::WMState>();
  views_delegate_ = std::make_unique<WegletViewsDelegate>();
  screen_ = views::CreateDesktopScreen();
#endif

  browser_context_ = std::make_unique<WegletBrowserContext>(
      /*off_the_record=*/false);

  WegletWindow::CreateAndShow(browser_context_.get(), StartupURL());

  // Anything other than RESULT_CODE_NORMAL_EXIT here aborts startup.
  return content::RESULT_CODE_NORMAL_EXIT;
}

void WegletBrowserMainParts::WillRunMainMessageLoop(
    std::unique_ptr<base::RunLoop>& run_loop) {
  // Handing the quit closure to the window layer is what makes closing
  // the last window end the process, rather than leaving a headless
  // browser running with no way to reach it.
  WegletWindow::SetQuitClosure(run_loop->QuitClosure());
}

void WegletBrowserMainParts::PostMainMessageLoopRun() {
  // Must happen before the message loop is gone: tearing down the
  // profile posts tasks of its own.
  browser_context_.reset();

#if defined(USE_AURA)
  screen_.reset();
  views_delegate_.reset();
  wm_state_.reset();
#endif
}

}  // namespace weglet