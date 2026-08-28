// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// Ctrl+F's floating bar.

#ifndef WEGLET_BROWSER_WEGLET_FIND_BAR_H_
#define WEGLET_BROWSER_WEGLET_FIND_BAR_H_

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

// A real second top-level widget, same reasoning as WegletMenuPopup.
// Unlike the other popups here, it does not close when it loses focus --
// clicking a match in the page underneath is the normal way to use one of
// these, and the reference every other popup follows would close it on
// exactly that click.
class WegletFindBar : public views::WidgetDelegate,
                      public content::WebContentsDelegate {
 public:
  // `owner` and `state` must outlive this popup.
  WegletFindBar(WegletWindow* owner, WegletStateService* state);
  WegletFindBar(const WegletFindBar&) = delete;
  WegletFindBar& operator=(const WegletFindBar&) = delete;
  ~WegletFindBar() override;

  bool IsVisible() const;
  // Creates the widget on first use; otherwise just re-shows and refocuses
  // it. Never reloads: unlike the other popups, a second Ctrl+F should
  // find the query the user already typed still sitting in the field.
  void Show();
  void Hide();

  // The tab's WebContents, not this popup's own -- see WegletWindow::
  // FindReply. Pushes to whichever page kind::kFindBar is currently open.
  void UpdateResult(const std::string& query, int match_count, int active_ordinal);

  content::WebContents* contents() const { return contents_.get(); }

  // views::WidgetDelegate:
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  // content::WebContentsDelegate:
  void CloseContents(content::WebContents* source) override;

 private:
  void EnsureWidget();
  gfx::Rect BoundsFor() const;

  const raw_ptr<WegletWindow> owner_;
  const raw_ptr<WegletStateService> state_;

  // CLIENT_OWNS_WIDGET, matching WegletWindow's own widget.
  std::unique_ptr<views::Widget> widget_;
  // Owned by the widget's view tree.
  raw_ptr<views::WebView> web_view_ = nullptr;
  std::unique_ptr<content::WebContents> contents_;
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_FIND_BAR_H_
