// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_window.h

#ifndef WEGLET_BROWSER_WEGLET_WINDOW_H_
#define WEGLET_BROWSER_WEGLET_WINDOW_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/widget/widget_delegate.h"

class GURL;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace views {
class WebView;
class Widget;
}  // namespace views

namespace weglet {

// One top-level window holding one WebContents.
//
// Deliberately NOT a views::WidgetDelegateView. That class seals its
// friend list with a "DO NOT ADD TO THIS LIST" comment because being a
// delegate and being a view at once makes ownership ambiguous. So the
// delegate is this object, the contents view is a plain views::View it
// hands over through SetContentsView, and the two lifetimes stay
// separate.
//
// On desktop Chromium draws through Aura, so a WebContents cannot be
// parented to a bare HWND -- it has to live inside a views::Widget.
class WegletWindow : public views::WidgetDelegate,
                     public content::WebContentsDelegate,
                     public content::WebContentsObserver {
 public:
  // Creates the window, shows it and navigates to `url`. The window owns
  // itself and schedules its own deletion when closed.
  static void CreateAndShow(content::BrowserContext* browser_context,
                            const GURL& url);

  // Closing the last window quits. Incremented on creation, decremented
  // on destruction.
  static int window_count();

  // Handed the message loop's quit closure by the browser main parts.
  // Without it the process keeps running with no window and no way to
  // reach it.
  static void SetQuitClosure(base::OnceClosure quit);

  WegletWindow(const WegletWindow&) = delete;
  WegletWindow& operator=(const WegletWindow&) = delete;
  ~WegletWindow() override;

  void LoadURL(const GURL& url);

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

  // content::WebContentsObserver:
  void TitleWasSet(content::NavigationEntry* entry) override;

 private:
  explicit WegletWindow(content::BrowserContext* browser_context);

  void Init(content::BrowserContext* browser_context, const GURL& url);

  // Registered with the widget rather than deleting inline: the widget is
  // still unwinding when this runs.
  void OnWindowClosing();

  // Called by the last window on its way out.
  static void QuitIfLastWindow();

  static int window_count_;

  // CLIENT_OWNS_WIDGET, so the widget is ours to destroy. Declared before
  // web_contents_ so it is torn down first.
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<content::WebContents> web_contents_;

  // Owned by the contents view, which is owned by the widget.
  raw_ptr<views::WebView> web_view_ = nullptr;
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_WINDOW_H_
