// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// Ported from content/shell/browser/shell_devtools_manager_delegate.{h,cc},
// trimmed to what weglet's own DevTools needs: serving the bundled
// front-end over a local HTTP port. The live protocol connection between
// that front-end and the page it inspects is a separate, in-process path
// -- see weglet_devtools_bindings.h -- so nothing here handles CDP
// commands directly.

#ifndef WEGLET_BROWSER_WEGLET_DEVTOOLS_MANAGER_DELEGATE_H_
#define WEGLET_BROWSER_WEGLET_DEVTOOLS_MANAGER_DELEGATE_H_

#include "content/public/browser/devtools_manager_delegate.h"

namespace content {
class BrowserContext;
}

namespace weglet {

class WegletDevToolsManagerDelegate : public content::DevToolsManagerDelegate {
 public:
  // Starts the local HTTP server the DevTools front-end tab loads from.
  static void StartHttpHandler(content::BrowserContext* browser_context);
  static void StopHttpHandler();
  // The port StartHttpHandler's ephemeral socket bound to. 0 before that.
  static int GetHttpHandlerPort();

  WegletDevToolsManagerDelegate();
  WegletDevToolsManagerDelegate(const WegletDevToolsManagerDelegate&) = delete;
  WegletDevToolsManagerDelegate& operator=(const WegletDevToolsManagerDelegate&) = delete;
  ~WegletDevToolsManagerDelegate() override;

  // content::DevToolsManagerDelegate:
  bool HasBundledFrontendResources() override;
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_DEVTOOLS_MANAGER_DELEGATE_H_
