// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_web_ui_controller_factory.cc

#include "weglet/browser/weglet_web_ui_controller_factory.h"

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"
#include "weglet/browser/weglet_message_handler.h"
#include "weglet/browser/weglet_state_service.h"
#include "weglet/browser/weglet_window.h"
#include "weglet/common/weglet_host.h"
#include "weglet/ui/generated_contract.h"

namespace weglet {
namespace {

// True only for chrome://weglet/... -- an exact scheme and an exact host.
//
// SchemeIs and host() rather than a prefix test on the spec: a prefix
// test would accept "chrome://weglet.evil.example/" and hand it a channel
// into the browser.
bool IsWegletPage(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) &&
         url.host() == kHost;
}

// Which of our pages this URL is. Worked out once, here, and carried by
// the controller from then on -- the browser used to re-derive it by
// string-comparing the committed path on every push to every page.
contract::PageKind KindOf(const GURL& url) {
  // path() returns a string_view; kept as one rather than copied into a
  // std::string that would only be sliced again.
  const std::string_view path = url.path();
  // path() carries the leading slash; the generated paths do not. An
  // empty path is the page the data source serves by default.
  const std::string_view relative =
      path.size() > 1 ? path.substr(1) : std::string_view(contract::kNewtabPath);
  return contract::KindForPath(relative);
}

// Gives a Weglet page its message channel. One controller per page; the
// handler is what the page's chrome.send() reaches.
//
// It is also what registers the page with the state service, and what
// unregisters it again -- the controller's lifetime is the page's, which
// is exactly the window in which pushing to it is meaningful.
class WegletWebUIController : public content::WebUIController {
 public:
  WegletWebUIController(content::WebUI* web_ui,
                        WegletWindow* window,
                        WegletStateService* state,
                        contract::PageKind kind)
      : content::WebUIController(web_ui), state_(state), web_ui_(web_ui) {
    // No service means no profile behind this page, which should not
    // happen -- but a page with no handler is a page whose buttons do
    // nothing, and a page whose chrome.send() is unregistered crashes the
    // browser. So the handler is added either way; it drops what it
    // cannot do.
    if (state_) {
      // The toolbar shows one window's tabs; which one is decided here,
      // once, from the window the page lives in.
      state_->AddPage(web_ui, kind, window ? window->window_id() : 0);
    }
    web_ui->AddMessageHandler(std::make_unique<WegletMessageHandler>(
        window, state_ ? state_->bridge() : nullptr, state_));
  }

  WegletWebUIController(const WegletWebUIController&) = delete;
  WegletWebUIController& operator=(const WegletWebUIController&) = delete;

  ~WegletWebUIController() override {
    if (state_) {
      state_->RemovePage(web_ui_);
    }
  }

 private:
  const raw_ptr<WegletStateService> state_;
  const raw_ptr<content::WebUI> web_ui_;
};

}  // namespace

// static
void WegletWebUIControllerFactory::Register() {
  // NoDestructor: content holds this pointer for the life of the process, so
  // it must outlive everything and must not be torn down at exit. It
  // constructs in place, so it needs the default constructor rather than a
  // unique_ptr, and it needs to be a friend to reach it.
  static base::NoDestructor<WegletWebUIControllerFactory> factory;
  static bool registered = false;
  if (!registered) {
    content::WebUIControllerFactory::RegisterFactory(factory.get());
    registered = true;
  }
}

std::unique_ptr<content::WebUIController>
WegletWebUIControllerFactory::CreateWebUIControllerForURL(
    content::WebUI* web_ui,
    const GURL& url) {
  if (!IsWegletPage(url)) {
    return nullptr;
  }
  // The window this page belongs to, if it belongs to one. Looked up from
  // the WebContents rather than remembered here: a factory outlives every
  // window, so it must not hold one. Null for a page open outside a
  // window; the handler drops the messages that need one.
  content::WebContents* contents = web_ui->GetWebContents();
  WegletWindow* window = WegletWindow::FromWebContents(contents);
  WegletStateService* state =
      contents ? WegletStateService::FromBrowserContext(
                     contents->GetBrowserContext())
               : nullptr;
  return std::make_unique<WegletWebUIController>(web_ui, window, state,
                                                 KindOf(url));
}

content::WebUI::TypeID WegletWebUIControllerFactory::GetWebUIType(
    content::BrowserContext* browser_context,
    const GURL& url) {
  // Any non-null value distinguishes "a WebUI of this type" from "not a
  // WebUI"; content compares these for identity, so the address of a static
  // is exactly what is wanted. All Weglet pages share one type: they share
  // one origin and one set of bindings, and content uses the type to decide
  // whether two pages may share a process.
  static const int kWegletWebUIType = 0;
  return IsWegletPage(url) ? &kWegletWebUIType : content::WebUI::kNoWebUI;
}

bool WegletWebUIControllerFactory::UseWebUIForURL(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return IsWegletPage(url);
}

}  // namespace weglet