// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_web_ui_data_source.h

#ifndef WEGLET_BROWSER_WEGLET_WEB_UI_DATA_SOURCE_H_
#define WEGLET_BROWSER_WEGLET_WEB_UI_DATA_SOURCE_H_

namespace content {
class BrowserContext;
}

namespace weglet {

// Registers chrome://weglet/ and serves it from the pages built into the
// binary.
//
// Nothing here touches the network or the disk: every response comes out of
// weglet/ui/generated_resources.h. A page of ours therefore cannot be
// intercepted, cached stale, or replaced by something on the filesystem.
//
// Called once per profile.
void AddWegletWebUIDataSource(content::BrowserContext* browser_context);

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_WEB_UI_DATA_SOURCE_H_
