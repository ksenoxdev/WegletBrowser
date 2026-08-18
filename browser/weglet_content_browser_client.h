// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_content_browser_client.h

#ifndef WEGLET_BROWSER_WEGLET_CONTENT_BROWSER_CLIENT_H_
#define WEGLET_BROWSER_WEGLET_CONTENT_BROWSER_CLIENT_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/content_browser_client.h"

namespace weglet {

class WegletBrowserMainParts;

// Where the browser process answers the content layer's questions about
// policy. Phase 4's request blocker hooks in here, through
// CreateURLLoaderThrottles -- an official extension point, so no patch to
// the Chromium tree is needed for it.
class WegletContentBrowserClient : public content::ContentBrowserClient {
 public:
  WegletContentBrowserClient();
  WegletContentBrowserClient(const WegletContentBrowserClient&) = delete;
  WegletContentBrowserClient& operator=(const WegletContentBrowserClient&) =
      delete;
  ~WegletContentBrowserClient() override;

  WegletBrowserMainParts* browser_main_parts() { return browser_main_parts_; }

  // content::ContentBrowserClient:
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test) override;
  std::string GetUserAgent() override;
  blink::UserAgentMetadata GetUserAgentMetadata() override;
  std::string GetProduct() override;

  // Where the block list and the risk heuristics actually take effect --
  // content's own hook, so no patch to the Chromium tree. This is the one
  // point that sees a redirect, which is exactly the case a shortened
  // link resolving to a lookalike arrives as.
  void CreateThrottlesForNavigation(
      content::NavigationThrottleRegistry& registry) override;

  // Weglet's pages are chrome://weglet/, served by a WebUIDataSource -- see
  // weglet_web_ui_data_source.h -- so there is no scheme of our own to
  // register a loader factory for.

 private:
  // Owned by the content layer, which outlives this client.
  raw_ptr<WegletBrowserMainParts> browser_main_parts_ = nullptr;
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_CONTENT_BROWSER_CLIENT_H_
