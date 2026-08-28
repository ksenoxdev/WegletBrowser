// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// The dropdown a right click opens.

#ifndef WEGLET_BROWSER_WEGLET_CONTEXT_MENU_H_
#define WEGLET_BROWSER_WEGLET_CONTEXT_MENU_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/gfx/geometry/point.h"
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

// A real second top-level widget, same reasoning as WegletMenuPopup --
// but positioned at the click point rather than a fixed corner, and
// with a different item list every time it opens.
class WegletContextMenu : public views::WidgetDelegate,
                          public content::WebContentsDelegate {
 public:
  using Item = WegletStateService::PendingContextMenuItem;

  // `owner` and `state` must outlive this popup.
  WegletContextMenu(WegletWindow* owner, WegletStateService* state);
  WegletContextMenu(const WegletContextMenu&) = delete;
  WegletContextMenu& operator=(const WegletContextMenu&) = delete;
  ~WegletContextMenu() override;

  // `screen_point` is where the click landed, in screen coordinates.
  void Show(const gfx::Point& screen_point, std::vector<Item> items);
  void Hide();
  bool IsVisible() const;

  // views::WidgetDelegate:
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  // content::WebContentsDelegate:
  void CloseContents(content::WebContents* source) override;

 private:
  void EnsureWidget();
  gfx::Rect BoundsFor(const gfx::Point& screen_point, int item_count) const;

  const raw_ptr<WegletWindow> owner_;
  const raw_ptr<WegletStateService> state_;

  // CLIENT_OWNS_WIDGET, matching WegletWindow's own widget.
  std::unique_ptr<views::Widget> widget_;
  // Owned by the widget's view tree.
  raw_ptr<views::WebView> web_view_ = nullptr;
  std::unique_ptr<content::WebContents> contents_;
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_CONTEXT_MENU_H_
