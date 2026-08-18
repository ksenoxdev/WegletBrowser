// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_window.cc

#include "weglet/browser/weglet_window.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#include "weglet/browser/weglet_bridge.h"
#include "weglet/browser/weglet_security_guard.h"
#include "weglet/browser/weglet_state_service.h"
#include "weglet/browser/weglet_tab_observer.h"
#include "weglet/common/weglet_host.h"
#include "weglet/ui/generated_contract.h"
#include "weglet/ui/weglet_tokens.h"

namespace weglet {
namespace {

base::OnceClosure& QuitClosure() {
  static base::NoDestructor<base::OnceClosure> quit;
  return *quit;
}

// Attaches a window to each of its WebContents so the WebUI factory can find
// it. A holder rather than making the window itself user data: the window is
// a WidgetDelegate with its own lifetime, and WebContentsUserData would take
// ownership of it.
class WindowHolder : public content::WebContentsUserData<WindowHolder> {
 public:
  WindowHolder(content::WebContents* contents, WegletWindow* window)
      : content::WebContentsUserData<WindowHolder>(*contents),
        window_(window) {}

  WegletWindow* window() const { return window_; }

 private:
  friend class content::WebContentsUserData<WindowHolder>;

  const raw_ptr<WegletWindow> window_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(WindowHolder);

}  // namespace

int WegletWindow::window_count_ = 0;

WegletWindow::Tab::Tab() = default;
WegletWindow::Tab::Tab(Tab&&) = default;
WegletWindow::Tab& WegletWindow::Tab::operator=(Tab&&) = default;
WegletWindow::Tab::~Tab() = default;

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
WegletWindow* WegletWindow::FromWebContents(content::WebContents* contents) {
  if (!contents) {
    return nullptr;
  }
  WindowHolder* holder = WindowHolder::FromWebContents(contents);
  return holder ? holder->window() : nullptr;
}

// static
void WegletWindow::CreateAndShow(content::BrowserContext* browser_context,
                                 WegletBridge* bridge,
                                 uint64_t window_id) {
  // Self-owned; see OnWindowClosing.
  auto* window = new WegletWindow(browser_context, bridge, window_id);
  window->Init();
}

// static
void WegletWindow::OpenNewWindow(content::BrowserContext* browser_context,
                                 WegletBridge* bridge) {
  const uint64_t id = bridge->OpenWindow();
  if (id == 0) {
    // The model's own ceiling. Reported rather than silently ignored.
    LOG(WARNING) << "window limit reached";
    return;
  }
  CreateAndShow(browser_context, bridge, id);
}

WegletWindow::WegletWindow(content::BrowserContext* browser_context,
                           WegletBridge* bridge,
                           uint64_t window_id)
    : browser_context_(browser_context),
      bridge_(bridge),
      window_id_(window_id),
      state_service_(
          WegletStateService::FromBrowserContext(browser_context)),
      security_guard_(
          WegletSecurityGuard::FromBrowserContext(browser_context)) {
  ++window_count_;
}

WegletWindow::~WegletWindow() {
  --window_count_;

  // The WebViews hold bare pointers to their contents. Detach them while the
  // views are still alive, drop the widget, then drop the contents.
  if (toolbar_view_) {
    toolbar_view_->SetWebContents(nullptr);
    toolbar_view_ = nullptr;
  }
  if (page_view_) {
    page_view_->SetWebContents(nullptr);
    page_view_ = nullptr;
  }
  contents_view_ = nullptr;
  widget_.reset();

  // Observers before contents: an observer's destructor touches the contents
  // it watches.
  for (auto& [id, tab] : tabs_) {
    if (security_guard_) {
      security_guard_->Forget(tab.contents.get());
    }
    tab.observer.reset();
  }
  tabs_.clear();
  toolbar_contents_.reset();

  // And the model stops knowing about this window. Its tabs go with it --
  // there is nothing left to show them in.
  if (bridge_) {
    bridge_->CloseWindow(window_id_);
  }

  QuitIfLastWindow();
}

void WegletWindow::Init() {
  SetCanResize(true);
  SetCanMaximize(true);
  SetCanMinimize(true);
  SetTitle(u"Weglet");

  // Vertical stack: toolbar at its own height, page taking the rest. The
  // heights come from weglet/ui/tokens.json through the generated header, so
  // the markup that fills the toolbar and the view that sizes it cannot
  // disagree -- which is how a toolbar ends up clipped.
  auto container = std::make_unique<views::View>();
  auto* layout = container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  auto toolbar = std::make_unique<views::WebView>(browser_context_);
  // The toolbar's own markup is a tab strip, a gap, and the toolbar row,
  // inside padding that is halved at the bottom. Adding it up from the same
  // tokens the CSS uses is the whole reason the tokens reach C++ at all: get
  // this wrong and the bottom row is clipped with no error anywhere.
  toolbar->SetPreferredSize(gfx::Size(
      0, tokens::kLayoutChromePadding + tokens::kLayoutTabHeight +
             tokens::kLayoutChromeGap + tokens::kLayoutToolbarRowHeight +
             tokens::kLayoutChromePaddingBottom));
  views::WebView* toolbar_view = container->AddChildView(std::move(toolbar));

  views::WebView* page_view =
      container->AddChildView(std::make_unique<views::WebView>(browser_context_));
  // Flex 1 on the page only: the toolbar keeps its preferred height and the
  // page absorbs every resize.
  layout->SetFlexForView(page_view, 1);

  contents_view_ = SetContentsView(std::move(container));
  toolbar_view_ = toolbar_view;
  page_view_ = page_view;

  RegisterWindowClosingCallback(
      base::BindOnce(&WegletWindow::OnWindowClosing, base::Unretained(this)));

  views::Widget::InitParams init_params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  init_params.delegate = this;
  // From tokens.json, like every other dimension in the window. A size
  // written here is the one number nobody would think to look for in a
  // file called tokens.
  init_params.bounds =
      gfx::Rect(0, 0, tokens::kLayoutWindowWidth, tokens::kLayoutWindowHeight);

  widget_ = std::make_unique<views::Widget>();
#if defined(USE_AURA)
  // Without this the widget is a plain NativeWidgetAura -- an Aura window
  // inside somebody else's window tree, which DCHECKs for a parent it will
  // never have. DesktopNativeWidgetAura owns a real top-level OS window.
  init_params.native_widget = new views::DesktopNativeWidgetAura(widget_.get());
#endif
  widget_->Init(std::move(init_params));
  widget_->Show();

  // After the widget exists: a WebView's NativeViewHost needs a live widget
  // to parent the contents' native view into. Attaching earlier leaves it
  // unparented and unsized, and the page renders as a sliver.
  content::WebContents::CreateParams toolbar_params(browser_context_);
  toolbar_contents_ = content::WebContents::Create(toolbar_params);
  toolbar_contents_->SetDelegate(this);
  WindowHolder::CreateForWebContents(toolbar_contents_.get(), this);
  toolbar_view_->SetWebContents(toolbar_contents_.get());

  content::NavigationController::LoadURLParams toolbar_url(
      PageUrl(contract::kToolbarPath));
  toolbar_url.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  toolbar_contents_->GetController().LoadURLWithParams(toolbar_url);

  RegisterAccelerators();
  ShowActiveTab();
}

void WegletWindow::RegisterAccelerators() {
  views::FocusManager* focus = widget_->GetFocusManager();
  if (!focus) {
    return;
  }
  // kNormalPriority throughout: these are ordinary browser shortcuts, and
  // a page that has taken the key for itself should keep it.
  // kHighPriority is for keys a page must not be able to swallow, which
  // none of these are.
  const ui::Accelerator accelerators[] = {
      ui::Accelerator(ui::VKEY_T, ui::EF_CONTROL_DOWN),
      ui::Accelerator(ui::VKEY_W, ui::EF_CONTROL_DOWN),
      ui::Accelerator(ui::VKEY_N, ui::EF_CONTROL_DOWN),
      ui::Accelerator(ui::VKEY_R, ui::EF_CONTROL_DOWN),
      ui::Accelerator(ui::VKEY_L, ui::EF_CONTROL_DOWN),
      ui::Accelerator(ui::VKEY_TAB, ui::EF_CONTROL_DOWN),
      ui::Accelerator(ui::VKEY_TAB, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN),
      ui::Accelerator(ui::VKEY_NEXT, ui::EF_CONTROL_DOWN),
      ui::Accelerator(ui::VKEY_PRIOR, ui::EF_CONTROL_DOWN),
      ui::Accelerator(ui::VKEY_F5, ui::EF_NONE),
      ui::Accelerator(ui::VKEY_LEFT, ui::EF_ALT_DOWN),
      ui::Accelerator(ui::VKEY_RIGHT, ui::EF_ALT_DOWN),
      ui::Accelerator(ui::VKEY_BROWSER_BACK, ui::EF_NONE),
      ui::Accelerator(ui::VKEY_BROWSER_FORWARD, ui::EF_NONE),
      ui::Accelerator(ui::VKEY_BROWSER_REFRESH, ui::EF_NONE),
  };
  for (const ui::Accelerator& accelerator : accelerators) {
    focus->RegisterAccelerator(accelerator,
                               ui::AcceleratorManager::kNormalPriority, this);
  }
  // Ctrl+1..9. Nine means the last tab however many there are -- what the
  // model already does, and what every other browser does.
  for (int key = ui::VKEY_1; key <= ui::VKEY_9; ++key) {
    focus->RegisterAccelerator(
        ui::Accelerator(static_cast<ui::KeyboardCode>(key),
                        ui::EF_CONTROL_DOWN),
        ui::AcceleratorManager::kNormalPriority, this);
  }
}

bool WegletWindow::CanHandleAccelerators() const {
  // False once the window is going away: the tab model below is being
  // torn down, and a shortcut arriving then would ask about tabs that are
  // half gone.
  return widget_ != nullptr;
}

bool WegletWindow::AcceleratorPressed(const ui::Accelerator& accelerator) {
  const int modifiers = accelerator.modifiers();
  const bool shift = (modifiers & ui::EF_SHIFT_DOWN) != 0;
  const bool ctrl = (modifiers & ui::EF_CONTROL_DOWN) != 0;

  switch (accelerator.key_code()) {
    case ui::VKEY_T:
      OpenNewTab();
      return true;
    case ui::VKEY_W:
      CloseTab(bridge_->ActiveTabId(window_id_));
      return true;
    case ui::VKEY_N:
      // A second window, which the model can now represent: its own tabs
      // and its own active one.
      OpenNewWindow(browser_context_, bridge_);
      return true;
    case ui::VKEY_R:
    case ui::VKEY_F5:
    case ui::VKEY_BROWSER_REFRESH:
      Reload();
      return true;
    case ui::VKEY_L:
      FocusOmnibox();
      return true;
    case ui::VKEY_TAB:
      // Shift reverses it, which is why both are registered.
      bridge_->CycleTab(window_id_, !shift);
      ShowActiveTab();
      return true;
    case ui::VKEY_NEXT:
      bridge_->CycleTab(window_id_, true);
      ShowActiveTab();
      return true;
    case ui::VKEY_PRIOR:
      bridge_->CycleTab(window_id_, false);
      ShowActiveTab();
      return true;
    case ui::VKEY_LEFT:
    case ui::VKEY_BROWSER_BACK:
      GoBack();
      return true;
    case ui::VKEY_RIGHT:
    case ui::VKEY_BROWSER_FORWARD:
      GoForward();
      return true;
    default:
      break;
  }

  if (ctrl && accelerator.key_code() >= ui::VKEY_1 &&
      accelerator.key_code() <= ui::VKEY_9) {
    const size_t position =
        static_cast<size_t>(accelerator.key_code() - ui::VKEY_1) + 1;
    if (bridge_->ActivateTabAt(window_id_, position)) {
      ShowActiveTab();
    }
    return true;
  }
  // Not ours. Returning false lets the focus manager pass it on rather
  // than swallowing a key the page may want.
  return false;
}

void WegletWindow::OnWindowClosing() {
  // Before the widget goes: the focus manager holds a raw pointer to this
  // as a target.
  if (widget_) {
    if (views::FocusManager* focus = widget_->GetFocusManager()) {
      focus->UnregisterAccelerators(this);
    }
  }

  // The widget is still unwinding, so deleting inline would pull the ground
  // out from under it.
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                               this);
}

views::Widget* WegletWindow::GetWidget() {
  return widget_.get();
}

const views::Widget* WegletWindow::GetWidget() const {
  return widget_.get();
}

// static
GURL WegletWindow::ResolveForEngine(const std::string& address) {
  // Weglet's own addresses are stored the way the user sees them and mapped
  // here, so "weglet://settings" never reaches the engine -- it is not a
  // scheme the engine knows.
  //
  // The table itself is generated from weglet/ui/contract.json, the same
  // file the Rust side generates its own copy of these five addresses
  // from. They went out of step once when each side declared them by
  // hand: three existed on the Rust side with nothing here to resolve
  // them, so a tab could carry a human label like "Bookmarks" while the
  // engine was handed a scheme it had never heard of.
  for (const contract::InternalAddress& entry : contract::kInternalAddresses) {
    if (address == entry.address) {
      return PageUrl(entry.page);
    }
  }
  return GURL(address);
}

content::WebContents* WegletWindow::EnsureContentsFor(uint64_t id) {
  auto found = tabs_.find(id);
  if (found != tabs_.end()) {
    return found->second.contents.get();
  }

  Tab tab;
  content::WebContents::CreateParams params(browser_context_);
  tab.contents = content::WebContents::Create(params);
  tab.contents->SetDelegate(this);
  WindowHolder::CreateForWebContents(tab.contents.get(), this);
  tab.observer =
      std::make_unique<WegletTabObserver>(this, id, tab.contents.get());

  content::WebContents* contents = tab.contents.get();
  tabs_.emplace(id, std::move(tab));

  // No check here any more. A restored tab's URL comes off disk and may
  // have been blocked since the session was written -- but the throttle
  // sees this navigation like any other, so checking again here would be
  // a second opinion that can only disagree.
  content::NavigationController::LoadURLParams load(
      ResolveForEngine(bridge_->TabUrl(id)));
  load.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  contents->GetController().LoadURLWithParams(load);
  return contents;
}

void WegletWindow::ShowActiveTab() {
  content::WebContents* contents = EnsureContentsFor(bridge_->ActiveTabId(window_id_));
  if (page_view_) {
    page_view_->SetWebContents(contents);
    contents->Focus();
  }
  Notify();
}

void WegletWindow::NavigateActiveTabFromOmnibox(const std::string& input) {
  // The model decides what the text means -- an address or a search -- and
  // returns the URL to load. Doing that here would put a second parser next
  // to the tested one.
  const std::string resolved = bridge_->ResolveOmnibox(input);
  if (resolved.empty()) {
    return;
  }

  const GURL url(resolved);
  if (!url.is_valid()) {
    LOG(WARNING) << "omnibox produced an invalid URL, ignored";
    return;
  }

  const uint64_t id = bridge_->ActiveTabId(window_id_);
  content::WebContents* contents = EnsureContentsFor(id);
  content::NavigationController::LoadURLParams load(url);
  load.transition_type = ui::PAGE_TRANSITION_TYPED;
  contents->GetController().LoadURLWithParams(load);
}

void WegletWindow::GoBack() {
  const uint64_t id = bridge_->ActiveTabId(window_id_);
  const std::string url = bridge_->GoBack(id);
  if (url.empty()) {
    return;
  }
  content::WebContents* contents = EnsureContentsFor(id);
  content::NavigationController::LoadURLParams load(ResolveForEngine(url));
  load.transition_type = ui::PAGE_TRANSITION_FORWARD_BACK;
  contents->GetController().LoadURLWithParams(load);
  Notify();
}

void WegletWindow::GoForward() {
  const uint64_t id = bridge_->ActiveTabId(window_id_);
  const std::string url = bridge_->GoForward(id);
  if (url.empty()) {
    return;
  }
  content::WebContents* contents = EnsureContentsFor(id);
  content::NavigationController::LoadURLParams load(ResolveForEngine(url));
  load.transition_type = ui::PAGE_TRANSITION_FORWARD_BACK;
  contents->GetController().LoadURLWithParams(load);
  Notify();
}

void WegletWindow::Reload() {
  auto found = tabs_.find(bridge_->ActiveTabId(window_id_));
  if (found == tabs_.end()) {
    return;
  }
  found->second.contents->GetController().Reload(
      content::ReloadType::NORMAL, /*check_for_repost=*/true);
}

void WegletWindow::OpenNewTab() {
  if (bridge_->OpenTab(window_id_, "about:blank") == 0) {
    // The model's own ceiling. Reported rather than silently ignored.
    LOG(WARNING) << "tab limit reached";
    return;
  }
  ShowActiveTab();
}

void WegletWindow::CloseTab(uint64_t id) {
  if (!bridge_->CloseTab(id)) {
    return;
  }
  auto found = tabs_.find(id);
  if (found != tabs_.end()) {
    // Detached unconditionally: WebView has no public accessor for its
    // contents, and clearing it when it was already pointing elsewhere costs
    // nothing -- ShowActiveTab below sets it again either way.
    if (page_view_) {
      page_view_->SetWebContents(nullptr);
    }
    if (security_guard_) {
      security_guard_->Forget(found->second.contents.get());
    }
    // Order matters: the observer's destructor touches the contents.
    found->second.observer.reset();
    tabs_.erase(found);
  }
  // Closing the last tab leaves a fresh one in the model, so there is always
  // something to show.
  ShowActiveTab();
}

void WegletWindow::ActivateTab(uint64_t id) {
  if (bridge_->ActivateTab(id)) {
    ShowActiveTab();
  }
}

void WegletWindow::OpenSettings() {
  if (bridge_->OpenTab(window_id_, "weglet://settings") == 0) {
    LOG(WARNING) << "tab limit reached";
    return;
  }
  ShowActiveTab();
}

void WegletWindow::ShowSecurityNotice(content::WebContents* contents) {
  if (!contents) {
    return;
  }
  content::NavigationController::LoadURLParams load(
      PageUrl(contract::kSecurityNoticePath));
  load.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  contents->GetController().LoadURLWithParams(load);
}

void WegletWindow::DismissSecurityNotice(content::WebContents* contents) {
  if (security_guard_) {
    security_guard_->ClearPendingNotice(contents);
  }
  // Back to wherever the tab was. The stopped navigation left no history
  // entry -- the throttle cancelled it rather than committing an error
  // page -- so this is the page the user was on before.
  const uint64_t id = TabIdFor(contents);
  if (id == 0 && contents != nullptr) {
    return;
  }
  const std::string url = bridge_->GoBack(id);
  content::NavigationController::LoadURLParams load(
      url.empty() ? GURL(url::kAboutBlankURL) : ResolveForEngine(url));
  load.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  contents->GetController().LoadURLWithParams(load);
  Notify();
}

void WegletWindow::ProceedPastSecurityNotice(content::WebContents* contents) {
  if (!security_guard_ || !contents) {
    return;
  }
  const WegletSecurityGuard::Notice* notice =
      security_guard_->PendingNotice(contents);
  if (!notice) {
    LOG(WARNING) << "proceed requested with no notice pending";
    return;
  }
  // A hard block offers no way through, and the page hides the button --
  // but the page runs in a renderer, and a compromised one can send the
  // message anyway. The decision is made here, where it cannot be
  // skipped, not there.
  if (notice->blocking) {
    LOG(WARNING) << "proceed refused: this notice is not dismissible";
    return;
  }

  const GURL target = notice->target;
  security_guard_->ClearPendingNotice(contents);
  // One navigation, this address. The guard consumes the allowance the
  // moment the throttle asks.
  security_guard_->AllowOnce(contents, target);

  content::NavigationController::LoadURLParams load(target);
  load.transition_type = ui::PAGE_TRANSITION_TYPED;
  contents->GetController().LoadURLWithParams(load);
}

// Which tab a WebContents is, or 0 if it is not one of this window's.
uint64_t WegletWindow::TabIdFor(content::WebContents* contents) const {
  for (const auto& [id, tab] : tabs_) {
    if (tab.contents.get() == contents) {
      return id;
    }
  }
  return 0;
}

// One place that says "something the pages show has changed".
//
// The service decides which pages care and coalesces several changes in
// one task into a single push. Everything this class touches is the tab
// model, so kTabs is the only thing it ever reports.
// Ctrl+L. The widget takes focus to the toolbar's own WebContents; which
// element inside it gets the caret is the page's decision, so the state
// service carries a one-shot request alongside the next push.
void WegletWindow::FocusOmnibox() {
  if (toolbar_contents_) {
    toolbar_contents_->Focus();
  }
  if (state_service_) {
    state_service_->RequestOmniboxFocus(window_id_);
  }
}

void WegletWindow::Notify() {
  if (state_service_) {
    state_service_->Notify(WegletStateService::kTabs);
  }
}

void WegletWindow::OnTabNavigated(uint64_t id,
                                  const GURL& url,
                                  bool same_document) {
  // A same-document navigation is pushState or a fragment change: the engine
  // already owns that history entry, so recording another one here would
  // make Back walk it twice.
  if (same_document) {
    bridge_->UrlReplaced(id, url);
  } else {
    bridge_->Navigated(id, url);
  }
  Notify();
}

void WegletWindow::OnTabTitleChanged(uint64_t id, const std::string& title) {
  bridge_->TitleChanged(id, title);
  Notify();
}

void WegletWindow::OnTabLoadingChanged(uint64_t id, bool loading) {
  bridge_->LoadingChanged(id, loading);
  Notify();
}

void WegletWindow::CloseContents(content::WebContents* source) {
  // window.close() from a page: close that tab, not the window.
  for (const auto& [id, tab] : tabs_) {
    if (tab.contents.get() == source) {
      CloseTab(id);
      return;
    }
  }
  if (widget_) {
    widget_->Close();
  }
}

content::WebContents* WegletWindow::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  // Every disposition lands in a new tab rather than a window we do not
  // control. Which tab it should be -- new, background, current -- is a
  // refinement; losing the navigation entirely is not.
  const uint64_t id = bridge_->OpenTab(window_id_, params.url.spec());
  if (id == 0) {
    LOG(WARNING) << "tab limit reached, dropping window.open";
    return nullptr;
  }
  content::WebContents* contents = EnsureContentsFor(id);
  ShowActiveTab();
  return contents;
}

bool WegletWindow::IsWebContentsCreationOverridden(
    content::RenderFrameHost* opener,
    content::SiteInstance* source_site_instance,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url) {
  // True means the embedder handles this, which stops the content layer from
  // creating a bare popup we do not control. The navigation arrives through
  // OpenURLFromTab instead.
  return true;
}

}  // namespace weglet
