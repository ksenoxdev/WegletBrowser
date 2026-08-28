// Copyright 2026 Weglet - Licensed under Apache 2.0

#include "weglet/browser/weglet_permission_prompt.h"

#include <algorithm>
#include <utility>

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

constexpr int kWidth = 320;
constexpr int kMargin = 8;
// Enough for the origin line, one or two permission lines, and the
// Allow/Block row -- the page itself does not scroll.
constexpr int kHeight = 150;

}  // namespace

WegletPermissionPrompt::WegletPermissionPrompt(WegletWindow* owner,
                                               WegletStateService* state)
    : owner_(owner), state_(state) {}

WegletPermissionPrompt::~WegletPermissionPrompt() {
  if (callback_) {
    std::move(callback_).Run(false);
  }
  if (state_ && contents_) {
    state_->ClearPendingPermissionPrompt(contents_.get());
  }
  if (web_view_) {
    web_view_->SetWebContents(nullptr);
    web_view_ = nullptr;
  }
  widget_.reset();
}

gfx::Rect WegletPermissionPrompt::BoundsFor() const {
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

void WegletPermissionPrompt::EnsureWidget() {
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
}

void WegletPermissionPrompt::Show(const std::string& origin,
                                  std::vector<std::string> types,
                                  ResultCallback callback) {
  EnsureWidget();

  // A second request arriving before the first was answered: the first
  // never reaches the page again, so answer it now rather than dropping
  // it silently.
  if (callback_) {
    std::move(callback_).Run(false);
  }
  callback_ = std::move(callback);

  state_->SetPendingPermissionPrompt(contents_.get(), {origin, std::move(types)});

  content::NavigationController::LoadURLParams load(
      PageUrl(contract::kPermissionPromptPath));
  load.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  contents_->GetController().LoadURLWithParams(load);

  widget_->SetBounds(BoundsFor());
  widget_->Show();
  contents_->Focus();
}

void WegletPermissionPrompt::Hide() {
  if (!widget_) {
    return;
  }
  widget_->Hide();
}

void WegletPermissionPrompt::Answer(bool allow) {
  if (callback_) {
    std::move(callback_).Run(allow);
  }
  Hide();
}

views::Widget* WegletPermissionPrompt::GetWidget() {
  return widget_.get();
}

const views::Widget* WegletPermissionPrompt::GetWidget() const {
  return widget_.get();
}

void WegletPermissionPrompt::CloseContents(content::WebContents* source) {
  Answer(false);
}

}  // namespace weglet
