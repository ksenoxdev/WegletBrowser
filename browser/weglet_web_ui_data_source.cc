// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_web_ui_data_source.cc

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
// uses. An empty path is the page itself.
//
// Query and fragment are already stripped by the time a data source sees
// the path, but a path with a separator in it is still rejected: the
// embedded set is flat, so a request for "a/b" is not a request for
// anything we have.
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
    // ShouldHandleRequest already said yes, so this is a race with nothing
    // -- but an empty response is still better than a null dereference.
    std::move(callback).Run(nullptr);
    return;
  }
  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(
          std::string(resource->contents)));
}

}  // namespace

void AddWegletWebUIDataSource(content::BrowserContext* browser_context) {
  // std::string(kHost), not kHost: CreateAndAdd takes a const std::string&
  // and kHost is a string_view, which does not convert implicitly. The
  // view is what every other caller wants -- comparing against url.host()
  // and building a URL -- so the conversion belongs here rather than in
  // the constant.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, std::string(kHost));

  // Served from a filter rather than resource ids: the files are embedded
  // by our own build step (weglet/ui/build_ui.py), not by grit, so there
  // are no ids to register.
  source->SetRequestFilter(base::BindRepeating(&ShouldHandleRequest),
                           base::BindRepeating(&HandleRequest));

  // Tightened past the WebUI default. Weglet's pages have no reason to
  // reach off the machine, and saying so here means a future mistake fails
  // loudly instead of quietly phoning home.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::DefaultSrc, "default-src 'none';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc, "script-src 'self';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc, "style-src 'self';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc, "img-src 'self' data:;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FormAction, "form-action 'none';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, "frame-src 'none';");
  // Not covered by default-src on WebUI: the platform overrides these two
  // separately from the rest. Without frame-ancestors a compromised page
  // elsewhere could iframe a Weglet page and see it render; without
  // font-src the five .ttf files the pages load have no CSP source that
  // names them, and only work at all because WebUI's own defaults happen
  // to allow chrome://weglet/ to load its own fonts.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameAncestors,
      "frame-ancestors 'none';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FontSrc, "font-src 'self';");
}

}  // namespace weglet