// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_url_loader_factory.h

#ifndef WEGLET_BROWSER_WEGLET_URL_LOADER_FACTORY_H_
#define WEGLET_BROWSER_WEGLET_URL_LOADER_FACTORY_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace weglet {

// Serves the weglet:// scheme from the pages built into the binary.
//
// Nothing here touches the network or the disk: every response comes out
// of weglet/ui/generated_resources.h, which the build embeds. A page of
// ours therefore cannot be intercepted, cached stale, or replaced by
// something on the filesystem.
mojo::PendingRemote<network::mojom::URLLoaderFactory>
CreateWegletURLLoaderFactory();

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_URL_LOADER_FACTORY_H_
