// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// Runs the security guard on every navigation and redirect.

#include "weglet/browser/weglet_navigation_throttle.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "weglet/browser/weglet_security_guard.h"
#include "weglet/browser/weglet_window.h"

namespace weglet {
namespace {

// The tab may have navigated again, closed, or lost its window by the
// time this runs.
void ShowSecurityNoticeIfAlive(base::WeakPtr<content::WebContents> contents) {
  if (!contents) {
    return;
  }
  if (WegletWindow* window = WegletWindow::FromWebContents(contents.get())) {
    window->ShowSecurityNotice(contents.get());
  }
}

}  // namespace

// static
void WegletNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  // Subframes are not where the address bar points. Whether to block
  // subresources is a different feature.
  if (!handle.IsInPrimaryMainFrame()) {
    return;
  }
  registry.AddThrottle(std::make_unique<WegletNavigationThrottle>(registry));
}

WegletNavigationThrottle::WegletNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

WegletNavigationThrottle::~WegletNavigationThrottle() = default;

const char* WegletNavigationThrottle::GetNameForLogging() {
  return "WegletNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
WegletNavigationThrottle::WillStartRequest() {
  return Check();
}

content::NavigationThrottle::ThrottleCheckResult
WegletNavigationThrottle::WillRedirectRequest() {
  // A shortened link is judged on the shortener at WillStartRequest and on
  // the destination here, where it is first known.
  return Check();
}

content::NavigationThrottle::ThrottleCheckResult
WegletNavigationThrottle::Check() {
  content::WebContents* contents = navigation_handle()->GetWebContents();
  if (!contents) {
    return PROCEED;
  }
  WegletSecurityGuard* guard = WegletSecurityGuard::FromBrowserContext(
      contents->GetBrowserContext());
  if (!guard) {
    // No guard means no profile behind this navigation. Failing closed
    // would make a wiring mistake stop pages from loading.
    return PROCEED;
  }

  const GURL& url = navigation_handle()->GetURL();
  std::optional<WegletSecurityGuard::Notice> notice =
      guard->Check(contents, url);
  if (!notice.has_value()) {
    return PROCEED;
  }

  LOG(WARNING) << "stopped navigation: " << notice->title;

  // Remembered before the notice page opens: it asks for this as soon as
  // its script runs.
  guard->SetPendingNotice(contents, *notice);

  // Posted, not called directly: ShowSecurityNotice starts a new
  // navigation on this WebContents, and doing that synchronously reaches
  // back into the NavigationRequest running this throttle and crashes
  // the browser process.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ShowSecurityNoticeIfAlive, contents->GetWeakPtr()));

  // CANCEL rather than BLOCK_REQUEST: the tab is being sent elsewhere, so
  // there is no error page and no history entry for Back to land on.
  return {content::NavigationThrottle::CANCEL, net::ERR_BLOCKED_BY_CLIENT};
}

}  // namespace weglet
