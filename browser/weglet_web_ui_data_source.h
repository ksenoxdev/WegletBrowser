// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// Serves chrome://weglet/ from the resources compiled into the binary.

#ifndef WEGLET_BROWSER_WEGLET_WEB_UI_DATA_SOURCE_H_
#define WEGLET_BROWSER_WEGLET_WEB_UI_DATA_SOURCE_H_

namespace content {
class BrowserContext;
}

namespace weglet {

// Registers chrome://weglet/ and serves it from the pages built into the
// binary. Every response comes out of generated_resources.h, so a page of
// ours cannot be intercepted, cached stale, or replaced on disk.
//
// Called once per profile.
void AddWegletWebUIDataSource(content::BrowserContext* browser_context);

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_WEB_UI_DATA_SOURCE_H_
