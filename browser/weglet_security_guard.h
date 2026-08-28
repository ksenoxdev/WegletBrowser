// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// Decides whether a navigation may proceed, and remembers why it did not.

#ifndef WEGLET_BROWSER_WEGLET_SECURITY_GUARD_H_
#define WEGLET_BROWSER_WEGLET_SECURITY_GUARD_H_

#include <map>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "url/gurl.h"
#include "weglet/browser/weglet_bridge.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace weglet {

// The one place that answers "may this navigation happen" -- asked by
// WegletNavigationThrottle for anything the network stack starts and by
// WegletWindow for URLs it hands the engine directly. Lives on the
// BrowserContext: a decision is about a profile's block list, not a
// window.
class WegletSecurityGuard : public base::SupportsUserData::Data {
 public:
  // Why a navigation was stopped. Kept until the notice page asks for it.
  struct Notice {
    // What the user was trying to reach.
    GURL target;
    std::string title;
    std::string reason;
    // The host the risk is about; may be empty.
    std::string host;
    // A warning may be dismissed, a block may not.
    bool blocking = true;
  };

  static void CreateForBrowserContext(content::BrowserContext* browser_context,
                                      WegletBridge* bridge);
  static WegletSecurityGuard* FromBrowserContext(
      content::BrowserContext* browser_context);

  explicit WegletSecurityGuard(WegletBridge* bridge);
  WegletSecurityGuard(const WegletSecurityGuard&) = delete;
  WegletSecurityGuard& operator=(const WegletSecurityGuard&) = delete;
  ~WegletSecurityGuard() override;

  // The verdict for `url` in `contents`. Nullopt means carry on.
  //
  // A one-shot allowance from AllowOnce is consumed here, so proceeding
  // past a notice works once, for that address only.
  std::optional<Notice> Check(content::WebContents* contents, const GURL& url);

  // Remembers why this tab is about to show the notice page.
  void SetPendingNotice(content::WebContents* contents, Notice notice);
  const Notice* PendingNotice(content::WebContents* contents) const;
  void ClearPendingNotice(content::WebContents* contents);

  // Lets `url` through once in `contents`. Only for a notice that offered
  // the choice.
  void AllowOnce(content::WebContents* contents, const GURL& url);

  // Called when a WebContents goes away, so neither map keeps a dangling
  // key.
  void Forget(content::WebContents* contents);

 private:
  const raw_ptr<WegletBridge> bridge_;

  // Keyed by WebContents and never dereferenced, so a stale entry cannot
  // be a use-after-free. Forget() removes them anyway.
  std::map<content::WebContents*, Notice> pending_;
  std::map<content::WebContents*, GURL> allowed_once_;
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_SECURITY_GUARD_H_
