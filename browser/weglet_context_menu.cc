// Copyright 2026 Weglet - Licensed under Apache 2.0

#include "weglet/browser/weglet_context_menu.h"

#include <algorithm>
#include <utility>

#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "weglet/browser/weglet_window.h"
#include "weglet/common/weglet_host.h"
#include "weglet/ui/generated_contract.h"

namespace weglet {

namespace {

constexpr int kWidth = 220;
constexpr int kItemHeight = 32;
// Matches menu.css's 6px border-box padding, top and bottom.
constexpr int kChrome = 12;

}  // namespace

WegletContextMenu::WegletContextMenu(WegletWindow* owner, WegletStateService* state)
    : owner_(owner), state_(state) {}

WegletContextMenu::~WegletContextMenu() {
  if (state_ && contents_) {
    state_->ClearPendingContextMenu(contents_.get());
  }
  if (web_view_) {
    web_view_->SetWebContents(nullptr);
    web_view_ = nullptr;
  }
  widget_.reset();
}

gfx::Rect WegletContextMenu::BoundsFor(const gfx::Point& screen_point,
                                       int item_count) const {
  const gfx::Rect client = owner_->GetWidget()->GetClientAreaBoundsInScreen();
  const int width = std::min(kWidth, client.width());
  const int height =
      std::min(kItemHeight * std::max(item_count, 1) + kChrome, client.height());
  const int min_x = client.x();
  const int max_x = client.x() + client.width() - width;
  const int min_y = client.y();
  const int max_y = client.y() + client.height() - height;
  const int x = std::clamp(screen_point.x(), min_x, std::max(min_x, max_x));
  const int y = std::clamp(screen_point.y(), min_y, std::max(min_y, max_y));
  return gfx::Rect(x, y, width, height);
}

void WegletContextMenu::EnsureWidget() {
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
  init_params.bounds = BoundsFor(gfx::Point(), 1);
  init_params.activatable = views::Widget::InitParams::Activatable::kYes;

  widget_ = std::make_unique<views::Widget>();
  widget_->Init(std::move(init_params));
  widget_->SetContentsView(std::move(web_view));
  web_view_ = web_view_ptr;
  web_view_->SetWebContents(contents_.get());
}

void WegletContextMenu::Show(const gfx::Point& screen_point, std::vector<Item> items) {
  EnsureWidget();

  const int item_count = static_cast<int>(items.size());
  state_->SetPendingContextMenu(contents_.get(), std::move(items));

  // Reloaded so the page's own script re-runs with the new item list.
  content::NavigationController::LoadURLParams load(
      PageUrl(contract::kContextMenuPath));
  load.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  contents_->GetController().LoadURLWithParams(load);

  widget_->SetBounds(BoundsFor(screen_point, item_count));
  widget_->Show();
  contents_->Focus();
}

void WegletContextMenu::Hide() {
  if (!widget_) {
    return;
  }
  if (contents_) {
    state_->ClearPendingContextMenu(contents_.get());
  }
  widget_->Hide();
}

bool WegletContextMenu::IsVisible() const {
  return widget_ && widget_->IsVisible();
}

views::Widget* WegletContextMenu::GetWidget() {
  return widget_.get();
}

const views::Widget* WegletContextMenu::GetWidget() const {
  return widget_.get();
}

void WegletContextMenu::CloseContents(content::WebContents* source) {
  Hide();
}

}  // namespace weglet
