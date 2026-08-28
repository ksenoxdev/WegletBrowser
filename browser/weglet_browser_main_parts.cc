// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// Browser-process startup and shutdown.

#include "weglet/browser/weglet_browser_main_parts.h"

#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/spellcheck/browser/windows_spell_checker.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/result_codes.h"
#include "weglet/browser/weglet_bridge.h"
#include "weglet/browser/weglet_browser_context.h"
#include "weglet/browser/weglet_devtools_manager_delegate.h"
#include "weglet/browser/weglet_download_observer.h"
#include "weglet/browser/weglet_security_guard.h"
#include "weglet/browser/weglet_state_service.h"
#include "weglet/browser/weglet_web_ui_controller_factory.h"
#include "weglet/browser/weglet_web_ui_data_source.h"
#include "weglet/browser/weglet_window.h"

#if defined(USE_AURA)
#include "ui/display/screen.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/wm/core/wm_state.h"
#endif

namespace weglet {
namespace {

#if defined(USE_AURA)
// Every method has a usable default; this exists because something has to
// be installed before the first Widget.
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


int WegletBrowserMainParts::PreMainMessageLoopRun() {
#if defined(USE_AURA)
  wm_state_ = std::make_unique<wm::WMState>();
  views_delegate_ = std::make_unique<WegletViewsDelegate>();
  screen_ = views::CreateDesktopScreen();
#endif

  browser_context_ = std::make_unique<WegletBrowserContext>(
      /*off_the_record=*/false);

  // Native Windows spellchecker -- see WegletSpellCheckHost. Created
  // unconditionally like real Chrome's own InitializePlatformSpellchecker:
  // cheap until a language checker is actually requested below.
  spell_checker_ = std::make_unique<WindowsSpellChecker>(
      base::ThreadPool::CreateCOMSTATaskRunner({base::MayBlock()}));
  spell_checker_->RetrieveSpellcheckLanguages(
      base::BindOnce(&WegletBrowserMainParts::OnSpellcheckLanguagesRetrieved,
                     weak_factory_.GetWeakPtr()));

  // Serves the DevTools front-end tab loads -- see WegletWindow::OpenDevTools.
  WegletDevToolsManagerDelegate::StartHttpHandler(browser_context_.get());

  // chrome://weglet/ has to exist before the first window navigates to it.
  AddWegletWebUIDataSource(browser_context_.get());

  // Registered before that navigation is asked whether it is a WebUI:
  // that answer decides whether the page gets a channel to the browser.
  WegletWebUIControllerFactory::Register();

  bridge_ = std::make_unique<WegletBridge>();

  // Before the first window: a page's controller reaches for both as soon
  // as it is created. The guard first -- the state service holds it.
  WegletSecurityGuard::CreateForBrowserContext(browser_context_.get(),
                                               bridge_.get());
  WegletStateService::CreateForBrowserContext(
      browser_context_.get(), bridge_.get(),
      WegletSecurityGuard::FromBrowserContext(browser_context_.get()),
      browser_context_->permission_delegate(), spell_checker_.get());

  download_observer_ = std::make_unique<WegletDownloadObserver>(
      browser_context_->GetDownloadManager(), bridge_.get(),
      WegletStateService::FromBrowserContext(browser_context_.get()));

  settings_flush_timer_.Start(
      FROM_HERE, bridge_->SettingsFlushInterval(),
      base::BindRepeating(&WegletBrowserMainParts::FlushSettings,
                          base::Unretained(this)));
  session_save_timer_.Start(
      FROM_HERE, bridge_->SessionSaveInterval(),
      base::BindRepeating(&WegletBrowserMainParts::SaveSession,
                          base::Unretained(this)));

  // One WegletWindow per window the model restored.
  for (uint64_t window : bridge_->Windows()) {
    WegletWindow::CreateAndShow(browser_context_.get(), bridge_.get(), window);
  }

  // Anything other than RESULT_CODE_NORMAL_EXIT aborts startup.
  return content::RESULT_CODE_NORMAL_EXIT;
}

void WegletBrowserMainParts::WillRunMainMessageLoop(
    std::unique_ptr<base::RunLoop>& run_loop) {
  // Handing the quit closure to the window layer is what makes closing the
  // last window end the process.
  WegletWindow::SetQuitClosure(run_loop->QuitClosure());
}

void WegletBrowserMainParts::OnSpellcheckLanguagesRetrieved(
    const std::vector<std::string>& lang_tags) {
  // One native checker per language pack the OS actually has installed.
  // Which one("s") is irrelevant to WegletSpellCheckHost::RequestTextCheck,
  // which checks against every created checker regardless of what language
  // the renderer thinks it asked for -- see InitializeDictionaries.
  for (const std::string& lang_tag : lang_tags) {
    spell_checker_->CreateSpellChecker(lang_tag, base::DoNothing());
  }
}

void WegletBrowserMainParts::FlushSettings() {
  if (bridge_ && bridge_->SettingsDirty() && !bridge_->FlushSettings()) {
    // Settings that quietly stop being written look exactly like settings
    // that are being written.
    LOG(ERROR) << "could not write settings -- changes are not being saved";
  }
}

void WegletBrowserMainParts::SaveSession() {
  if (bridge_ && !bridge_->SaveSession()) {
    LOG(ERROR) << "could not save the session";
  }
}

void WegletBrowserMainParts::PostMainMessageLoopRun() {
  WegletDevToolsManagerDelegate::StopHttpHandler();

  // Stopped first: neither should fire while the model is going away.
  settings_flush_timer_.Stop();
  session_save_timer_.Stop();

  // Not when the window closed: the tab list was still being torn down.
  if (bridge_ && !bridge_->SaveSession()) {
    LOG(ERROR) << "could not save the session -- open tabs will be lost";
  }
  // Last chance for anything changed since the last tick.
  if (bridge_ && bridge_->SettingsDirty() && !bridge_->FlushSettings()) {
    LOG(ERROR) << "could not write settings -- the last changes are lost";
  }

  // Before the bridge: the state service is owned by the context and
  // holds a pointer to it.
  browser_context_.reset();
  bridge_.reset();
  spell_checker_.reset();

#if defined(USE_AURA)
  screen_.reset();
  views_delegate_.reset();
  wm_state_.reset();
#endif
}

}  // namespace weglet
