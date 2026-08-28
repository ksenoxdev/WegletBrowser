// Copyright 2026 Weglet - Licensed under Apache 2.0

#include "weglet/browser/weglet_toast_popup.h"

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
#include "weglet/ui/weglet_tokens.h"

namespace weglet {

namespace {

constexpr int kWidth = 280;
constexpr int kItemHeight = 40;
constexpr int kChrome = 8;
// Same margin WegletShortcutPopup uses below its own anchor.
constexpr int kMargin = 6;

// The toolbar's own total height -- see WegletWindow::Init, which sizes
// the toolbar's WebView from these same tokens.
constexpr int kToolbarHeight = tokens::kLayoutChromePadding + tokens::kLayoutTabHeight +
                              tokens::kLayoutChromeGap + tokens::kLayoutToolbarRowHeight +
                              tokens::kLayoutChromePaddingBottom;

}  // namespace

WegletToastPopup::WegletToastPopup(WegletWindow* owner, WegletStateService* state)
    : owner_(owner), state_(state) {}

WegletToastPopup::~WegletToastPopup() {
  if (state_ && contents_) {
    state_->ClearPendingToasts(contents_.get());
  }
  if (web_view_) {
    web_view_->SetWebContents(nullptr);
    web_view_ = nullptr;
  }
  widget_.reset();
}

gfx::Rect WegletToastPopup::BoundsFor(int item_count) const {
  const gfx::Rect client = owner_->GetWidget()->GetClientAreaBoundsInScreen();
  const int width = std::min(kWidth, client.width());
  const int height =
      std::min(kItemHeight * std::max(item_count, 1) + kChrome, client.height());
  const int x = client.x() + (client.width() - width) / 2;
  const int max_y = client.y() + client.height() - height;
  const int y = std::clamp(client.y() + kToolbarHeight + kMargin, client.y(), std::max(client.y(), max_y));
  return gfx::Rect(x, y, width, height);
}

void WegletToastPopup::EnsureWidget() {
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
  init_params.bounds = BoundsFor(1);
  // Never wants keyboard focus or to raise itself over the window it
  // reports on -- it just needs to accept clicks on its own close buttons.
  init_params.activatable = views::Widget::InitParams::Activatable::kNo;

  widget_ = std::make_unique<views::Widget>();
  widget_->Init(std::move(init_params));
  widget_->SetContentsView(std::move(web_view));
  web_view_ = web_view_ptr;
  web_view_->SetWebContents(contents_.get());
}

void WegletToastPopup::Show(std::vector<Toast> toasts) {
  EnsureWidget();

  const int item_count = static_cast<int>(toasts.size());
  state_->SetPendingToasts(contents_.get(), std::move(toasts));

  // Reloaded so the page's own script re-runs and restarts each notice's
  // 5s auto-dismiss timer -- see toast.ts.
  content::NavigationController::LoadURLParams load(PageUrl(contract::kToastPath));
  load.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  contents_->GetController().LoadURLWithParams(load);

  widget_->SetBounds(BoundsFor(item_count));
  widget_->Show();
}

void WegletToastPopup::Hide() {
  if (!widget_) {
    return;
  }
  if (contents_) {
    state_->ClearPendingToasts(contents_.get());
  }
  widget_->Hide();
}

bool WegletToastPopup::IsVisible() const {
  return widget_ && widget_->IsVisible();
}

views::Widget* WegletToastPopup::GetWidget() {
  return widget_.get();
}

const views::Widget* WegletToastPopup::GetWidget() const {
  return widget_.get();
}

void WegletToastPopup::CloseContents(content::WebContents* source) {
  Hide();
}

}  // namespace weglet
