// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_content_browser_client.h

#ifndef WEGLET_BROWSER_WEGLET_CONTENT_BROWSER_CLIENT_H_
#define WEGLET_BROWSER_WEGLET_CONTENT_BROWSER_CLIENT_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"

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

  // Hands the engine a factory for weglet:// so a navigation to one of our
  // pages is served from the binary rather than attempted over the network.
  // Returns an unbound remote for any other scheme, which is how the base
  // class says "not mine".
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
  CreateNonNetworkNavigationURLLoaderFactory(
      const std::string& scheme,
      content::FrameTreeNodeId frame_tree_node_id) override;

  // And everything the page then asks for: its stylesheet and its module
  // scripts. Registering only the navigation factory gives a page that
  // loads and then renders unstyled with no scripts.
  void RegisterNonNetworkSubresourceURLLoaderFactories(
      int render_process_id,
      int render_frame_id,
      const std::optional<url::Origin>& request_initiator_origin,
      NonNetworkURLLoaderFactoryMap* factories) override;

 private:
  // Owned by the content layer, which outlives this client.
  raw_ptr<WegletBrowserMainParts> browser_main_parts_ = nullptr;
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_CONTENT_BROWSER_CLIENT_H_