// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_navigation_throttle.cc

#include "weglet/browser/weglet_navigation_throttle.h"

#include <memory>
#include <optional>

#include "base/logging.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "weglet/browser/weglet_security_guard.h"
#include "weglet/browser/weglet_window.h"

namespace weglet {

// static
void WegletNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  // Subframes are not where the address bar points, and stopping one would
  // block an ad slot without telling anyone. Whether to block subresources
  // is a different feature with a different answer.
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
  // The reason this class exists. A shortened link is judged on the
  // shortener at WillStartRequest and on the destination here, which is
  // the only point at which the destination is known.
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
    // No guard means no profile behind this navigation. Proceeding is the
    // right default: failing closed here would make the browser refuse to
    // load pages because of a wiring mistake.
    return PROCEED;
  }

  const GURL& url = navigation_handle()->GetURL();
  std::optional<WegletSecurityGuard::Notice> notice =
      guard->Check(contents, url);
  if (!notice.has_value()) {
    return PROCEED;
  }

  LOG(WARNING) << "stopped navigation: " << notice->title;

  // Remembered before the notice page is opened: that page asks for this
  // as soon as its script runs.
  guard->SetPendingNotice(contents, *notice);

  // The window puts the notice on screen. Without one -- a page open
  // outside a Weglet window -- the navigation is still stopped; it just
  // gets the engine's own blocked page rather than ours, which is worse
  // to look at and no less correct.
  if (WegletWindow* window = WegletWindow::FromWebContents(contents)) {
    window->ShowSecurityNotice(contents);
  }

  // CANCEL rather than BLOCK_REQUEST: the tab is being sent somewhere
  // else, so there is no error page to show and no entry to leave in the
  // history for Back to land on.
  return {content::NavigationThrottle::CANCEL, net::ERR_BLOCKED_BY_CLIENT};
}

}  // namespace weglet
