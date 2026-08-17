// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/app/weglet_main_delegate.h

#ifndef WEGLET_APP_WEGLET_MAIN_DELEGATE_H_
#define WEGLET_APP_WEGLET_MAIN_DELEGATE_H_

#include <memory>
#include <optional>

#include "content/public/app/content_main_delegate.h"

namespace weglet {

class WegletContentBrowserClient;
class WegletContentClient;
class WegletContentRendererClient;

// The one object the content layer calls back into before it knows
// anything about Weglet. It runs in every process type, so anything set
// up here has to be safe in a sandboxed renderer too.
class WegletMainDelegate : public content::ContentMainDelegate {
 public:
  WegletMainDelegate();
  WegletMainDelegate(const WegletMainDelegate&) = delete;
  WegletMainDelegate& operator=(const WegletMainDelegate&) = delete;
  ~WegletMainDelegate() override;

  // content::ContentMainDelegate:
  std::optional<int> BasicStartupComplete() override;
  content::ContentClient* CreateContentClient() override;
  void PreSandboxStartup() override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;

 private:
  // Loads weglet.pak. Called once per process, before the sandbox
  // closes -- a sandboxed renderer cannot open the file itself.
  static void InitializeResourceBundle();

  // Created in every process type: the renderer needs it to reach the
  // resource pack just as much as the browser does.
  std::unique_ptr<WegletContentClient> content_client_;
  std::unique_ptr<WegletContentBrowserClient> browser_client_;
  std::unique_ptr<WegletContentRendererClient> renderer_client_;
};

}  // namespace weglet

#endif  // WEGLET_APP_WEGLET_MAIN_DELEGATE_H_
