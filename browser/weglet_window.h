// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_window.h

#ifndef WEGLET_BROWSER_WEGLET_WINDOW_H_
#define WEGLET_BROWSER_WEGLET_WINDOW_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/views/widget/widget_delegate.h"

class GURL;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace views {
class View;
class WebView;
class Widget;
}  // namespace views

namespace weglet {

class WegletBridge;
class WegletSecurityGuard;
class WegletStateService;
class WegletTabObserver;

// One browser window: a toolbar above, the active tab's page below.
//
// The toolbar is a WebContents of its own showing chrome://weglet/toolbar.html,
// not native views. That keeps one design system for the whole interface --
// the same tokens and the same TypeScript as the other pages -- at the cost
// of a second renderer process. The privilege split that makes this safe is
// content's, not ours: only a page the WebUI factory claims gets a channel
// to the browser.
//
// Deliberately NOT a views::WidgetDelegateView. That class seals its friend
// list because being a delegate and a view at once makes ownership
// ambiguous; here the delegate is this object and the contents view is a
// plain views::View it hands over.
class WegletWindow : public views::WidgetDelegate,
                     public content::WebContentsDelegate,
                     public ui::AcceleratorTarget {
 public:
  // The window a WebContents belongs to, or nullptr for one that is not a
  // Weglet page in a Weglet window.
  static WegletWindow* FromWebContents(content::WebContents* contents);

  // Creates a window for `window_id` -- a window the model already knows
  // about -- shows it, and opens the tabs the model says are in it.
  static void CreateAndShow(content::BrowserContext* browser_context,
                            WegletBridge* bridge,
                            uint64_t window_id);

  // Opens a window the model does not have yet: asks the model for one,
  // then shows it. Null-safe about the ceiling, which it reports.
  static void OpenNewWindow(content::BrowserContext* browser_context,
                            WegletBridge* bridge);

  uint64_t window_id() const { return window_id_; }

  static int window_count();
  static void SetQuitClosure(base::OnceClosure quit);

  WegletWindow(const WegletWindow&) = delete;
  WegletWindow& operator=(const WegletWindow&) = delete;
  ~WegletWindow() override;

  // Called by a page's message handler, and only for the things that are
  // a window's business: which tab is in front and where it goes.
  //
  // Settings, shortcuts and the block list used to arrive here too, purely
  // because the window owned the code that told the pages about a change.
  // That code is WegletStateService now, so those messages go straight
  // from the handler to the model and never mention a window.
  void NavigateActiveTabFromOmnibox(const std::string& input);
  void GoBack();
  void GoForward();
  void Reload();
  void OpenNewTab();
  void CloseTab(uint64_t id);
  void ActivateTab(uint64_t id);
  void OpenSettings();

  // Puts the notice page in `contents`, which is the tab whose navigation
  // was stopped. Called by the throttle, which has already recorded why
  // with the security guard.
  void ShowSecurityNotice(content::WebContents* contents);

  // The notice's two answers, from the page showing it.
  //
  // Dismiss goes back where the tab was. Proceed asks the guard for a
  // one-shot allowance and navigates again -- and refuses outright when
  // the notice was a hard block, because a page that is not offering the
  // choice must not be able to take it by sending the message anyway.
  void DismissSecurityNotice(content::WebContents* contents);
  void ProceedPastSecurityNotice(content::WebContents* contents);

  // Called by the per-tab observer when the engine reports a change.
  void OnTabNavigated(uint64_t id, const GURL& url, bool same_document);
  void OnTabTitleChanged(uint64_t id, const std::string& title);
  void OnTabLoadingChanged(uint64_t id, bool loading);

  // ui::AcceleratorTarget:
  //
  // The keyboard shortcuts. The model has had CycleTab, ActivateTabAt and
  // ReorderTab since it was written and nothing could reach them: there
  // was no accelerator anywhere in the browser, so Ctrl+Tab and Ctrl+1..9
  // existed only as methods.
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  // views::WidgetDelegate:
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  // content::WebContentsDelegate:
  void CloseContents(content::WebContents* source) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  bool IsWebContentsCreationOverridden(
      content::RenderFrameHost* opener,
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) override;

 private:
  // Everything owned per tab. The observer is what turns engine events into
  // bridge calls, and it has to outlive neither more nor less than the
  // contents it watches.
  struct Tab {
    Tab();
    Tab(Tab&&);
    Tab& operator=(Tab&&);
    ~Tab();

    std::unique_ptr<content::WebContents> contents;
    std::unique_ptr<WegletTabObserver> observer;
  };

  WegletWindow(content::BrowserContext* browser_context,
               WegletBridge* bridge,
               uint64_t window_id);

  void Init();
  void OnWindowClosing();

  // Which tab a WebContents is, or 0 if it is not one of this window's.
  uint64_t TabIdFor(content::WebContents* contents) const;

  // Ctrl+L: focus the toolbar and ask it for the address bar.
  void FocusOmnibox();

  // Registers every shortcut with the widget's focus manager. Called once,
  // after the widget exists.
  void RegisterAccelerators();

  // Tells the state service that the tab model changed. One call site
  // shape for every event that can change it.
  void Notify();

  static void QuitIfLastWindow();

  // Builds a WebContents for `id` and shows it. Called for a tab that does
  // not have one yet -- on startup for the restored active tab, and lazily
  // when another restored tab is first activated.
  content::WebContents* EnsureContentsFor(uint64_t id);

  // Points the visible area at the active tab's contents, creating them if
  // this is the first time that tab is shown.
  void ShowActiveTab();

  // The URL to actually load for an address the model holds. Weglet's own
  // addresses are stored as the user sees them and resolved here, so
  // "weglet://settings" never reaches the engine.
  static GURL ResolveForEngine(const std::string& address);

  static int window_count_;

  const raw_ptr<content::BrowserContext> browser_context_;
  const raw_ptr<WegletBridge> bridge_;
  // Which window in the model this is. Every tab question this class asks
  // is asked about it, so two windows cannot disturb each other.
  const uint64_t window_id_;
  // Owned by the browser context, which outlives every window.
  const raw_ptr<WegletStateService> state_service_;
  // Owned by the browser context, like the state service.
  const raw_ptr<WegletSecurityGuard> security_guard_;

  // CLIENT_OWNS_WIDGET, so the widget is ours. Declared before the contents
  // it holds so it is torn down first.
  std::unique_ptr<views::Widget> widget_;

  // Owned by the widget's view tree.
  raw_ptr<views::View> contents_view_ = nullptr;
  raw_ptr<views::WebView> toolbar_view_ = nullptr;
  raw_ptr<views::WebView> page_view_ = nullptr;

  std::unique_ptr<content::WebContents> toolbar_contents_;
  std::map<uint64_t, Tab> tabs_;
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_WINDOW_H_