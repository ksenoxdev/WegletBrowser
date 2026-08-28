// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// The floating popup opened by the toolbar's "keep" button.

#ifndef WEGLET_BROWSER_WEGLET_SHORTCUT_POPUP_H_
#define WEGLET_BROWSER_WEGLET_SHORTCUT_POPUP_H_

#include <memory>
#include <string>

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

class WegletStateService;
class WegletWindow;

// A real second top-level widget, not DOM inside the toolbar's own
// page: the toolbar's native view is clipped to the toolbar strip (see
// WegletWindow::Init), so anything drawn inside it can never overlap
// the tab content below.
class WegletShortcutPopup : public views::WidgetDelegate,
                            public content::WebContentsDelegate {
 public:
  // `owner` and `state` must outlive this popup.
  WegletShortcutPopup(WegletWindow* owner, WegletStateService* state);
  WegletShortcutPopup(const WegletShortcutPopup&) = delete;
  WegletShortcutPopup& operator=(const WegletShortcutPopup&) = delete;
  ~WegletShortcutPopup() override;

  // Creates the widget on first use; otherwise repositions, reloads
  // (fresh form) and re-shows it. `anchor_right`/`anchor_bottom` are in
  // the toolbar page's own viewport coordinates, which is also the
  // window's client area coordinate space -- the toolbar sits at its
  // top-left.
  void Show(int anchor_right,
            int anchor_bottom,
            const std::string& url,
            const std::string& suggested_name);
  void Hide();

  // views::WidgetDelegate:
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  // content::WebContentsDelegate:
  void CloseContents(content::WebContents* source) override;

 private:
  void EnsureWidget();
  gfx::Rect BoundsFor(int anchor_right, int anchor_bottom) const;

  const raw_ptr<WegletWindow> owner_;
  const raw_ptr<WegletStateService> state_;

  // CLIENT_OWNS_WIDGET, matching WegletWindow's own widget.
  std::unique_ptr<views::Widget> widget_;
  // Owned by the widget's view tree.
  raw_ptr<views::WebView> web_view_ = nullptr;
  std::unique_ptr<content::WebContents> contents_;
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_SHORTCUT_POPUP_H_
