// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// The floating popup asking Allow/Block for camera, microphone,
// location or notifications -- see WegletPermissionDelegate.

#ifndef WEGLET_BROWSER_WEGLET_PERMISSION_PROMPT_H_
#define WEGLET_BROWSER_WEGLET_PERMISSION_PROMPT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
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
class WegletPermissionPrompt : public views::WidgetDelegate,
                               public content::WebContentsDelegate {
 public:
  // Called with true for Allow, false for Block -- including a block when
  // the request is abandoned (a second request arrives, or the tab/window
  // this one was for goes away) rather than ever leaving the page waiting.
  using ResultCallback = base::OnceCallback<void(bool)>;

  // `owner` and `state` must outlive this popup.
  WegletPermissionPrompt(WegletWindow* owner, WegletStateService* state);
  WegletPermissionPrompt(const WegletPermissionPrompt&) = delete;
  WegletPermissionPrompt& operator=(const WegletPermissionPrompt&) = delete;
  ~WegletPermissionPrompt() override;

  // `origin` is the already-formatted, human-readable string to show;
  // `types` are ids like "camera" -- see permission_prompt.ts for the
  // i18n keys they map to.
  void Show(const std::string& origin, std::vector<std::string> types,
           ResultCallback callback);
  void Hide();
  void Answer(bool allow);

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

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::WebView> web_view_ = nullptr;
  std::unique_ptr<content::WebContents> contents_;
  ResultCallback callback_;
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_PERMISSION_PROMPT_H_
