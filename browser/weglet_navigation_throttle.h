// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// Runs the security guard on every navigation and redirect.

#ifndef WEGLET_BROWSER_WEGLET_NAVIGATION_THROTTLE_H_
#define WEGLET_BROWSER_WEGLET_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationThrottleRegistry;
}

namespace weglet {

// Asks WegletSecurityGuard about every navigation the network stack
// starts, and about every redirect within one -- which is how a shortener
// resolving to a lookalike arrives.
//
// Registered by WegletContentBrowserClient::CreateThrottlesForNavigation.
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
