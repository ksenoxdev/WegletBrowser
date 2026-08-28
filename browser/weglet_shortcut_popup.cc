// Copyright 2026 Weglet - Licensed under Apache 2.0

#include "weglet/browser/weglet_shortcut_popup.h"

#include <algorithm>

#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "weglet/browser/weglet_state_service.h"
#include "weglet/browser/weglet_window.h"
#include "weglet/common/weglet_host.h"
#include "weglet/ui/generated_contract.h"

namespace weglet {

namespace {

// Straight from the reference design: a 260x170 card, its right edge
// under the button's own right edge, 6px below it.
constexpr int kWidth = 260;
constexpr int kHeight = 170;
constexpr int kMargin = 6;

}  // namespace

WegletShortcutPopup::WegletShortcutPopup(WegletWindow* owner,
                                         WegletStateService* state)
    : owner_(owner), state_(state) {}

WegletShortcutPopup::~WegletShortcutPopup() {
  if (state_ && contents_) {
    state_->ClearPendingShortcut(contents_.get());
  }
  if (web_view_) {
    web_view_->SetWebContents(nullptr);
    web_view_ = nullptr;
  }
  widget_.reset();
}

gfx::Rect WegletShortcutPopup::BoundsFor(int anchor_right,
                                         int anchor_bottom) const {
  const gfx::Rect client = owner_->GetWidget()->GetClientAreaBoundsInScreen();
  const int width = std::min(kWidth, client.width());
  const int height = std::min(kHeight, client.height());
  const int min_x = client.x();
  const int max_x = client.x() + client.width() - width;
  const int min_y = client.y();
  const int max_y = client.y() + client.height() - height;
  const int x = std::clamp(client.x() + anchor_right - width, min_x, std::max(min_x, max_x));
  const int y = std::clamp(client.y() + anchor_bottom + kMargin, min_y, std::max(min_y, max_y));
  return gfx::Rect(x, y, width, height);
}

void WegletShortcutPopup::EnsureWidget() {
  if (widget_) {
    return;
  }

  content::WebContents::CreateParams params(owner_->browser_context());
  contents_ = content::WebContents::Create(params);
  contents_->SetDelegate(this);
  owner_->RegisterWebContents(contents_.get());

  auto web_view = std::make_unique<views::WebView>(owner_->browser_context());
  views::WebView* web_view_ptr = web_view.get();

  views::Widget::InitParams init_params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  init_params.delegate = this;
  init_params.parent = owner_->GetWidget()->GetNativeView();
  init_params.bounds = BoundsFor(0, 0);
  init_params.activatable = views::Widget::InitParams::Activatable::kYes;

  widget_ = std::make_unique<views::Widget>();
  widget_->Init(std::move(init_params));
  widget_->SetContentsView(std::move(web_view));
  web_view_ = web_view_ptr;
  web_view_->SetWebContents(contents_.get());
}

void WegletShortcutPopup::Show(int anchor_right,
                               int anchor_bottom,
                               const std::string& url,
                               const std::string& suggested_name) {
  EnsureWidget();

  state_->SetPendingShortcut(contents_.get(), {url, suggested_name});

  // Reloaded so the page's own script re-runs and asks for this state,
  // starting the form blank instead of carrying over the last click's.
  content::NavigationController::LoadURLParams load(
      PageUrl(contract::kSaveShortcutPath));
  load.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  contents_->GetController().LoadURLWithParams(load);

  widget_->SetBounds(BoundsFor(anchor_right, anchor_bottom));
  widget_->Show();
  contents_->Focus();
}

void WegletShortcutPopup::Hide() {
  if (!widget_) {
    return;
  }
  if (contents_) {
    state_->ClearPendingShortcut(contents_.get());
  }
  widget_->Hide();
}

views::Widget* WegletShortcutPopup::GetWidget() {
  return widget_.get();
}

const views::Widget* WegletShortcutPopup::GetWidget() const {
  return widget_.get();
}

void WegletShortcutPopup::CloseContents(content::WebContents* source) {
  Hide();
}

}  // namespace weglet
