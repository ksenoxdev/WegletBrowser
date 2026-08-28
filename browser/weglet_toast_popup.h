// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// The small stack of dismissable notices under the address bar (link
// copied, bookmark added/removed, ...).

#ifndef WEGLET_BROWSER_WEGLET_TOAST_POPUP_H_
#define WEGLET_BROWSER_WEGLET_TOAST_POPUP_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget_delegate.h"
#include "weglet/browser/weglet_state_service.h"

namespace content {
class WebContents;
}

namespace views {
class WebView;
class Widget;
}  // namespace views

namespace weglet {

class WegletWindow;

// A real second top-level widget, same reasoning as WegletShortcutPopup --
// anchored under the toolbar chrome rather than a button, and with a
// height that grows and shrinks with how many notices are queued.
class WegletToastPopup : public views::WidgetDelegate,
                         public content::WebContentsDelegate {
 public:
  using Toast = WegletStateService::PendingToast;

  // `owner` and `state` must outlive this popup.
  WegletToastPopup(WegletWindow* owner, WegletStateService* state);
  WegletToastPopup(const WegletToastPopup&) = delete;
  WegletToastPopup& operator=(const WegletToastPopup&) = delete;
  ~WegletToastPopup() override;

  // Creates the widget on first use; otherwise repositions, reloads
  // (each notice's own on-page 5s timer starts fresh) and re-shows it.
  // `toasts` must not be empty -- call Hide() instead.
  void Show(std::vector<Toast> toasts);
  void Hide();
  bool IsVisible() const;

  // views::WidgetDelegate:
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  // content::WebContentsDelegate:
  void CloseContents(content::WebContents* source) override;

 private:
  void EnsureWidget();
  gfx::Rect BoundsFor(int item_count) const;

  const raw_ptr<WegletWindow> owner_;
  const raw_ptr<WegletStateService> state_;

  // CLIENT_OWNS_WIDGET, matching WegletWindow's own widget.
  std::unique_ptr<views::Widget> widget_;
  // Owned by the widget's view tree.
  raw_ptr<views::WebView> web_view_ = nullptr;
  std::unique_ptr<content::WebContents> contents_;
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_TOAST_POPUP_H_
