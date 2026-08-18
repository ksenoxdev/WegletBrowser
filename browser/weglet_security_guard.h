// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_security_guard.h

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

// The one place that answers "may this navigation happen".
//
// It used to be answered in exactly one place too -- inside the omnibox
// handler -- which meant links, redirects and restored tabs were never
// asked at all. The heuristics in rust/weglet-security are the most
// tested code in the repository and they were wired to a single input
// path.
//
// Now every path asks this object: WegletNavigationThrottle for anything
// the network stack starts, including redirects, and WegletWindow for the
// URLs it hands the engine directly.
//
// Lives on the BrowserContext because a decision is about a profile's
// block list, not about a window.
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
    // A warning may be dismissed with "proceed anyway". A block may not,
    // and the page hides the button rather than disabling it.
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

  // The verdict for `url` in `contents`. Nullopt means "carry on".
  //
  // A one-shot allowance granted by AllowOnce is consumed here, so
  // proceeding past a notice works exactly once for exactly that address
  // -- a bypass that outlived its navigation would be a way to turn the
  // check off by visiting a warned-about page and then anything else.
  std::optional<Notice> Check(content::WebContents* contents, const GURL& url);

  // Remembers why this tab is about to show the notice page. Cleared when
  // the notice is answered or the tab navigates elsewhere.
  void SetPendingNotice(content::WebContents* contents, Notice notice);
  const Notice* PendingNotice(content::WebContents* contents) const;
  void ClearPendingNotice(content::WebContents* contents);

  // Lets `url` through once in `contents`. Only ever called for a notice
  // the user was actually shown and that offered the choice.
  void AllowOnce(content::WebContents* contents, const GURL& url);

  // Called when a WebContents goes away, so neither map keeps a dangling
  // key.
  void Forget(content::WebContents* contents);

 private:
  const raw_ptr<WegletBridge> bridge_;

  // Keyed by WebContents, and only ever used as a key -- never
  // dereferenced -- so a stale entry cannot be a use-after-free. Forget()
  // removes them anyway, because a map that only grows is a leak.
  std::map<content::WebContents*, Notice> pending_;
  std::map<content::WebContents*, GURL> allowed_once_;
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_SECURITY_GUARD_H_
