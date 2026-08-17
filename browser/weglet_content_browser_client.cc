// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_content_browser_client.cc

#include "weglet/browser/weglet_content_browser_client.h"

#include <utility>

#include "components/embedder_support/user_agent_utils.h"
#include "weglet/browser/weglet_browser_main_parts.h"

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

}  // namespace weglet