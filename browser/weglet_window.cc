// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_window.cc

#include "weglet/browser/weglet_window.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace weglet {
namespace {

base::OnceClosure& QuitClosure() {
  static base::NoDestructor<base::OnceClosure> quit;
  return *quit;
}

}  // namespace

int WegletWindow::window_count_ = 0;

// static
int WegletWindow::window_count() {
  return window_count_;
}

// static
void WegletWindow::SetQuitClosure(base::OnceClosure quit) {
  QuitClosure() = std::move(quit);
}

// static
void WegletWindow::QuitIfLastWindow() {
  if (window_count_ == 0 && QuitClosure()) {
    std::move(QuitClosure()).Run();
  }
}

// static
void WegletWindow::CreateAndShow(content::BrowserContext* browser_context,
                                 const GURL& url) {
  // Self-owned; see OnWindowClosing.
  auto* window = new WegletWindow(browser_context);
  window->Init(browser_context, url);
}

WegletWindow::WegletWindow(content::BrowserContext* browser_context) {
  ++window_count_;

  content::WebContents::CreateParams params(browser_context);
  web_contents_ = content::WebContents::Create(params);
  web_contents_->SetDelegate(this);
  Observe(web_contents_.get());
}

WegletWindow::~WegletWindow() {
  --window_count_;

  // Order matters here. web_view_ is owned by the widget's view tree, and
  // it holds a bare pointer to web_contents_. So: detach the contents
  // while the view is still alive, drop the widget (which destroys the
  // view), and only then drop the contents.
  if (web_view_) {
    web_view_->SetWebContents(nullptr);
    web_view_ = nullptr;
  }
  widget_.reset();
  web_contents_.reset();

  QuitIfLastWindow();
}

void WegletWindow::Init(content::BrowserContext* browser_context,
                        const GURL& url) {
  SetCanResize(true);
  SetCanMaximize(true);
  SetCanMinimize(true);
  SetTitle(u"Weglet");

  // The WebView IS the contents view. An intermediate View with a
  // FillLayout would work too, but it is one more thing whose bounds have
  // to be right, and getting them wrong renders the page into a sliver.
  //
  // Set before Widget::Init, which asks the delegate for its contents
  // view while building the view tree.
  web_view_ = SetContentsView(std::make_unique<views::WebView>(browser_context));

  RegisterWindowClosingCallback(
      base::BindOnce(&WegletWindow::OnWindowClosing, base::Unretained(this)));

  views::Widget::InitParams init_params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  init_params.delegate = this;
  init_params.bounds = gfx::Rect(0, 0, 1280, 800);

  widget_ = std::make_unique<views::Widget>();

#if defined(USE_AURA)
  // Without this the widget is built as a plain NativeWidgetAura, which
  // is an Aura window inside somebody else's window tree and DCHECKs for
  // a parent it will never have. DesktopNativeWidgetAura is the one that
  // owns a real top-level OS window.
  init_params.native_widget =
      new views::DesktopNativeWidgetAura(widget_.get());
#endif

  widget_->Init(std::move(init_params));
  widget_->Show();

  // AFTER the widget exists. WebView holds a NativeViewHost, and that
  // needs a live widget to parent the web contents' native view into --
  // attaching earlier leaves it unparented and unsized, which is what
  // made pages render as a one-character-wide column.
  web_view_->SetWebContents(web_contents_.get());

  LoadURL(url);
}

void WegletWindow::OnWindowClosing() {
  // The widget is still unwinding at this point, so deleting inline would
  // pull the ground out from under it.
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                               this);
}

views::Widget* WegletWindow::GetWidget() {
  return widget_.get();
}

const views::Widget* WegletWindow::GetWidget() const {
  return widget_.get();
}

void WegletWindow::LoadURL(const GURL& url) {
  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PAGE_TRANSITION_TYPED;
  web_contents_->GetController().LoadURLWithParams(params);
  web_contents_->Focus();
}

void WegletWindow::CloseContents(content::WebContents* source) {
  if (widget_) {
    widget_->Close();
  }
}

content::WebContents* WegletWindow::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  // One window, one WebContents for now: every disposition lands in this
  // window rather than silently doing nothing. Tabs come with the Rust
  // bridge in phase 2.
  auto handle = web_contents_->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(params));
  if (navigation_handle_callback && handle) {
    std::move(navigation_handle_callback).Run(*handle);
  }
  return web_contents_.get();
}

bool WegletWindow::IsWebContentsCreationOverridden(
    content::RenderFrameHost* opener,
    content::SiteInstance* source_site_instance,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url) {
  // True means "the embedder handles this", which stops the content layer
  // from creating a bare popup window we do not control. The navigation
  // arrives through OpenURLFromTab instead.
  return true;
}

void WegletWindow::TitleWasSet(content::NavigationEntry* entry) {
  const std::u16string title = entry ? entry->GetTitle() : std::u16string();
  SetTitle(title.empty() ? u"Weglet" : title);
}

}  // namespace weglet