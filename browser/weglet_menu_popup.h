// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// The dropdown opened by the toolbar's "⋮" button.

#ifndef WEGLET_BROWSER_WEGLET_MENU_POPUP_H_
#define WEGLET_BROWSER_WEGLET_MENU_POPUP_H_

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

// A real second top-level widget, not DOM inside the toolbar's own page
// -- same reasoning as WegletShortcutPopup: the toolbar's native view
// is clipped to the toolbar strip. Unlike that popup, this one anchors
// to the window's own top-right corner rather than to the button that
// opened it: there is only ever one place it can open.
class WegletMenuPopup : public views::WidgetDelegate,
                        public content::WebContentsDelegate {
 public:
  // `owner` must outlive this popup.
  explicit WegletMenuPopup(WegletWindow* owner);
  WegletMenuPopup(const WegletMenuPopup&) = delete;
  WegletMenuPopup& operator=(const WegletMenuPopup&) = delete;
  ~WegletMenuPopup() override;

  bool IsVisible() const;
  // Creates the widget on first use; otherwise just repositions,
  // reloads (so a stale hover state does not carry over) and shows it.
  void Show();
  void Hide();

  // views::WidgetDelegate:
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  // content::WebContentsDelegate:
  void CloseContents(content::WebContents* source) override;

 private:
  void EnsureWidget();
  gfx::Rect BoundsFor() const;

  const raw_ptr<WegletWindow> owner_;

  // CLIENT_OWNS_WIDGET, matching WegletWindow's own widget.
  std::unique_ptr<views::Widget> widget_;
  // Owned by the widget's view tree.
  raw_ptr<views::WebView> web_view_ = nullptr;
  std::unique_ptr<content::WebContents> contents_;
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_MENU_POPUP_H_
