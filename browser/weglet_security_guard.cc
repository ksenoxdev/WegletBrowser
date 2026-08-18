// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_security_guard.cc

#include "weglet/browser/weglet_security_guard.h"

#include <memory>
#include <utility>

#include "content/public/browser/browser_context.h"

namespace weglet {
namespace {

// The address is the key; the contents are never read.
const char kUserDataKey[] = "weglet_security_guard";

}  // namespace

// static
void WegletSecurityGuard::CreateForBrowserContext(
    content::BrowserContext* browser_context,
    WegletBridge* bridge) {
  browser_context->SetUserData(kUserDataKey,
                               std::make_unique<WegletSecurityGuard>(bridge));
}

// static
WegletSecurityGuard* WegletSecurityGuard::FromBrowserContext(
    content::BrowserContext* browser_context) {
  if (!browser_context) {
    return nullptr;
  }
  return static_cast<WegletSecurityGuard*>(
      browser_context->GetUserData(kUserDataKey));
}

WegletSecurityGuard::WegletSecurityGuard(WegletBridge* bridge)
    : bridge_(bridge) {}

WegletSecurityGuard::~WegletSecurityGuard() = default;

std::optional<WegletSecurityGuard::Notice> WegletSecurityGuard::Check(
    content::WebContents* contents,
    const GURL& url) {
  // Our own pages, and anything with no host to judge. A page of ours must
  // never be stoppable by an entry in a list the user typed, or opening
  // settings to remove a mistaken block would be blocked by the mistake.
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return std::nullopt;
  }

  // A one-shot allowance, consumed whether or not it matches -- it was
  // granted for one navigation, and the navigation happened.
  if (contents) {
    auto allowed = allowed_once_.find(contents);
    if (allowed != allowed_once_.end()) {
      const bool matches = allowed->second == url;
      allowed_once_.erase(allowed);
      if (matches) {
        return std::nullopt;
      }
    }
  }

  // The user's own list first: it is the one they can point at and
  // explain, it is not a heuristic, and it costs a hash lookup. Not
  // dismissible -- the user asked for this, and offering to override it
  // here would make the setting advice rather than a setting.
  WegletBridge::RiskAssessment assessment = bridge_->CheckBlockList(url);
  if (assessment.level == WegletBridge::Risk::kNone) {
    assessment = bridge_->AssessNavigation(url);
  }
  if (assessment.level == WegletBridge::Risk::kNone) {
    return std::nullopt;
  }

  // No wording here. Every word the notice shows comes from the model,
  // which is where the heuristics' own titles and reasons already lived.
  Notice notice;
  notice.target = url;
  notice.title = std::move(assessment.title);
  notice.reason = std::move(assessment.reason);
  notice.host = std::move(assessment.host);
  notice.blocking = assessment.level == WegletBridge::Risk::kBlock;
  return notice;
}

void WegletSecurityGuard::SetPendingNotice(content::WebContents* contents,
                                           Notice notice) {
  if (!contents) {
    return;
  }
  pending_[contents] = std::move(notice);
}

const WegletSecurityGuard::Notice* WegletSecurityGuard::PendingNotice(
    content::WebContents* contents) const {
  auto found = pending_.find(contents);
  return found == pending_.end() ? nullptr : &found->second;
}

void WegletSecurityGuard::ClearPendingNotice(content::WebContents* contents) {
  pending_.erase(contents);
}

void WegletSecurityGuard::AllowOnce(content::WebContents* contents,
                                    const GURL& url) {
  if (contents) {
    allowed_once_[contents] = url;
  }
}

void WegletSecurityGuard::Forget(content::WebContents* contents) {
  pending_.erase(contents);
  allowed_once_.erase(contents);
}

}  // namespace weglet
