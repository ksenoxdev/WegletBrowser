// Copyright 2026 Weglet - Licensed under Apache 2.0

#include "weglet/browser/weglet_find_bar.h"

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
#include "weglet/ui/weglet_tokens.h"

namespace weglet {

namespace {

// Hangs from the window's top-right corner, under the tab strip, same
// margin as WegletMenuPopup.
constexpr int kWidth = 340;
constexpr int kHeight = 48;
constexpr int kMargin = 8;

}  // namespace

WegletFindBar::WegletFindBar(WegletWindow* owner, WegletStateService* state)
    : owner_(owner), state_(state) {}

WegletFindBar::~WegletFindBar() {
  if (state_ && contents_) {
    state_->ClearPendingFindResult(contents_.get());
  }
  if (web_view_) {
    web_view_->SetWebContents(nullptr);
    web_view_ = nullptr;
  }
  widget_.reset();
}

gfx::Rect WegletFindBar::BoundsFor() const {
  const gfx::Rect client = owner_->GetWidget()->GetClientAreaBoundsInScreen();
  const int toolbar_height = tokens::kLayoutChromePadding + tokens::kLayoutTabHeight +
                             tokens::kLayoutChromeGap + tokens::kLayoutToolbarRowHeight +
                             tokens::kLayoutChromePaddingBottom;
  const int width = std::min(kWidth, client.width());
  const int available_height = std::max(0, client.height() - toolbar_height);
  const int height = std::min(kHeight, available_height);
  const int x = std::max(client.x(), client.x() + client.width() - width - kMargin);
  const int y = client.y() + toolbar_height + kMargin;
  return gfx::Rect(x, y, width, height);
}

void WegletFindBar::EnsureWidget() {
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
  init_params.bounds = BoundsFor();
  init_params.activatable = views::Widget::InitParams::Activatable::kYes;

  widget_ = std::make_unique<views::Widget>();
  widget_->Init(std::move(init_params));
  widget_->SetContentsView(std::move(web_view));
  web_view_ = web_view_ptr;
  web_view_->SetWebContents(contents_.get());

  content::NavigationController::LoadURLParams load(PageUrl(contract::kFindBarPath));
  load.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  contents_->GetController().LoadURLWithParams(load);
}

bool WegletFindBar::IsVisible() const {
  return widget_ && widget_->IsVisible();
}

void WegletFindBar::Show() {
  EnsureWidget();
  widget_->SetBounds(BoundsFor());
  widget_->Show();
  contents_->Focus();
}

void WegletFindBar::Hide() {
  if (!widget_) {
    return;
  }
  widget_->Hide();
}

void WegletFindBar::UpdateResult(const std::string& query, int match_count,
                                 int active_ordinal) {
  if (!contents_) {
    return;
  }
  state_->SetPendingFindResult(contents_.get(), {query, match_count, active_ordinal});
  state_->Notify(WegletStateService::kFindResult);
}

views::Widget* WegletFindBar::GetWidget() {
  return widget_.get();
}

const views::Widget* WegletFindBar::GetWidget() const {
  return widget_.get();
}

void WegletFindBar::CloseContents(content::WebContents* source) {
  Hide();
}

}  // namespace weglet
