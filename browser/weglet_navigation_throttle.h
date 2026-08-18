// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_navigation_throttle.h

#ifndef WEGLET_BROWSER_WEGLET_NAVIGATION_THROTTLE_H_
#define WEGLET_BROWSER_WEGLET_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationThrottleRegistry;
}

namespace weglet {

// Asks WegletSecurityGuard about every navigation the network stack
// starts, and about every redirect within one.
//
// This is the extension point that was named in a comment and never
// used. Until it existed, the assessment ran in exactly one place -- the
// omnibox -- so a link, a redirect, or a tab restored from the last
// session was never checked. A shortener that resolves to a lookalike is
// precisely the case the heuristics were written for, and precisely the
// case that arrives as a redirect.
//
// Registered by WegletContentBrowserClient::CreateThrottlesForNavigation,
// which is content's own hook: no patch to the Chromium tree.
class WegletNavigationThrottle : public content::NavigationThrottle {
 public:
  // Adds one to `registry` if this navigation is worth watching.
  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);

  explicit WegletNavigationThrottle(
      content::NavigationThrottleRegistry& registry);
  WegletNavigationThrottle(const WegletNavigationThrottle&) = delete;
  WegletNavigationThrottle& operator=(const WegletNavigationThrottle&) = delete;
  ~WegletNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  // Both events do the same thing to a different URL.
  ThrottleCheckResult Check();
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_NAVIGATION_THROTTLE_H_
