// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// Serves chrome://weglet/ from the resources compiled into the binary.

#include "weglet/browser/weglet_web_ui_data_source.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "weglet/common/weglet_host.h"
#include "weglet/ui/generated_contract.h"
#include "weglet/ui/generated_resources.h"

namespace weglet {
namespace {

// The path a request arrives with, normalised to the key the embedded set
// uses. An empty path is the page itself. A path with a separator is
// rejected: the embedded set is flat.
std::string ResourceKeyFor(const std::string& path) {
  if (path.empty()) {
    return contract::kNewtabPath;
  }
  if (path.find('/') != std::string::npos ||
      path.find('\\') != std::string::npos) {
    return std::string();
  }
  return path;
}

bool ShouldHandleRequest(const std::string& path) {
  const std::string key = ResourceKeyFor(path);
  return !key.empty() && ui::Find(key) != nullptr;
}

void HandleRequest(const std::string& path,
                   content::WebUIDataSource::GotDataCallback callback) {
  const ui::Resource* resource = ui::Find(ResourceKeyFor(path));
  if (!resource) {
    // ShouldHandleRequest already said yes; an empty response still beats
    // a null dereference.
    std::move(callback).Run(nullptr);
    return;
  }
  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(
          std::string(resource->contents)));
}

}  // namespace

void AddWegletWebUIDataSource(content::BrowserContext* browser_context) {
  // std::string(kHost): CreateAndAdd takes a const std::string& and kHost
  // is a string_view, which does not convert implicitly.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, std::string(kHost));

  // A filter rather than resource ids: the files are embedded by
  // build_ui.py, not by grit, so there are no ids to register.
  source->SetRequestFilter(base::BindRepeating(&ShouldHandleRequest),
                           base::BindRepeating(&HandleRequest));

  // Tightened past the WebUI default: our pages have no reason to reach
  // off the machine, so a future mistake fails loudly.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::DefaultSrc, "default-src 'none';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc, "script-src 'self';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc, "style-src 'self';");
  // https: only for img-src, and only images: a favicon is the one
  // thing a page may ask the network for, opt-in via Settings -- see
  // docs/security.md and WegletBridge::FaviconsEnabled.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc,
      "img-src 'self' data: https:;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FormAction, "form-action 'none';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, "frame-src 'none';");
  // Not covered by default-src on WebUI, so set separately: without
  // frame-ancestors a page elsewhere could iframe ours; without
  // font-src the embedded fonts have no source that names them.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameAncestors,
      "frame-ancestors 'none';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FontSrc, "font-src 'self';");
}

}  // namespace weglet