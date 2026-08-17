// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_content_browser_client.cc

#include "weglet/browser/weglet_content_browser_client.h"

#include <utility>

#include "components/embedder_support/user_agent_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "weglet/browser/weglet_browser_main_parts.h"
#include "weglet/browser/weglet_url_loader_factory.h"
#include "weglet/common/weglet_scheme.h"

namespace weglet {

WegletContentBrowserClient::WegletContentBrowserClient() = default;
WegletContentBrowserClient::~WegletContentBrowserClient() = default;

std::unique_ptr<content::BrowserMainParts>
WegletContentBrowserClient::CreateBrowserMainParts(bool is_integration_test) {
  auto parts = std::make_unique<WegletBrowserMainParts>();
  browser_main_parts_ = parts.get();
  return parts;
}

std::string WegletContentBrowserClient::GetProduct() {
  // Chrome's own product string, on purpose. A distinct one here would
  // make every Weglet user uniquely identifiable to every site they
  // visit, which is the opposite of the point.
  return embedder_support::GetProductAndVersion();
}

std::string WegletContentBrowserClient::GetUserAgent() {
  return embedder_support::GetUserAgent();
}

blink::UserAgentMetadata WegletContentBrowserClient::GetUserAgentMetadata() {
  return embedder_support::GetUserAgentMetadata();
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
WegletContentBrowserClient::CreateNonNetworkNavigationURLLoaderFactory(
    const std::string& scheme,
    content::FrameTreeNodeId frame_tree_node_id) {
  if (scheme == kWegletScheme) {
    return CreateWegletURLLoaderFactory();
  }
  // Unbound: the engine reads that as "the embedder does not handle this
  // scheme" and carries on with its own handling.
  return mojo::NullRemote();
}

void WegletContentBrowserClient::
    RegisterNonNetworkSubresourceURLLoaderFactories(
        int render_process_id,
        int render_frame_id,
        const std::optional<url::Origin>& request_initiator_origin,
        NonNetworkURLLoaderFactoryMap* factories) {
  factories->emplace(kWegletScheme, CreateWegletURLLoaderFactory());
}

}  // namespace weglet