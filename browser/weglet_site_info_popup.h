// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// The dropdown opened by the toolbar's site-info button.

#ifndef WEGLET_BROWSER_WEGLET_SITE_INFO_POPUP_H_
#define WEGLET_BROWSER_WEGLET_SITE_INFO_POPUP_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget_delegate.h"

namespace content {
class WebContents;
}

namespace views {
class WebView;
class Widget;
}  // namespace views

namespace weglet {

class WegletWindow;

// A real second top-level widget, same reasoning as WegletShortcutPopup,
// which this mirrors exactly: anchored to the button that opened it, and
// reloaded on every Show() so a permission changed elsewhere is never
// shown stale.
class WegletSiteInfoPopup : public views::WidgetDelegate,
                            public content::WebContentsDelegate {
 public:
  // `owner` must outlive this popup.
  explicit WegletSiteInfoPopup(WegletWindow* owner);
  WegletSiteInfoPopup(const WegletSiteInfoPopup&) = delete;
  WegletSiteInfoPopup& operator=(const WegletSiteInfoPopup&) = delete;
  ~WegletSiteInfoPopup() override;

  bool IsVisible() const;
  // `anchor_right`/`anchor_bottom` are in the toolbar page's own viewport
  // coordinates -- see WegletShortcutPopup::Show.
  void Show(int anchor_right, int anchor_bottom);
  void Hide();
  // Re-runs Show() at the same spot -- see WegletWindow::SetPermissionDecision.
  void Refresh();

  // views::WidgetDelegate:
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  // content::WebContentsDelegate:
  void CloseContents(content::WebContents* source) override;

 private:
  void EnsureWidget();
  gfx::Rect BoundsFor(int anchor_right, int anchor_bottom) const;

  const raw_ptr<WegletWindow> owner_;

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::WebView> web_view_ = nullptr;
  std::unique_ptr<content::WebContents> contents_;

  // Remembered so a permission change can re-Show() at the same spot --
  // see WegletWindow::SetPermissionDecision.
  int last_anchor_right_ = 0;
  int last_anchor_bottom_ = 0;
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_SITE_INFO_POPUP_H_
