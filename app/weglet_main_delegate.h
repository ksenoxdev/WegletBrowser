// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// ContentMainDelegate: process-wide setup and the client objects.

#ifndef WEGLET_APP_WEGLET_MAIN_DELEGATE_H_
#define WEGLET_APP_WEGLET_MAIN_DELEGATE_H_

#include <memory>
#include <optional>

#include "content/public/app/content_main_delegate.h"

namespace weglet {

class WegletContentBrowserClient;
class WegletContentClient;
class WegletContentRendererClient;

// content's entry point into Weglet. Runs in every process type.
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
  // Loads weglet.pak before the sandbox closes.
  static void InitializeResourceBundle();

  std::unique_ptr<WegletContentClient> content_client_;
  std::unique_ptr<WegletContentBrowserClient> browser_client_;
  std::unique_ptr<WegletContentRendererClient> renderer_client_;
};

}  // namespace weglet

#endif  // WEGLET_APP_WEGLET_MAIN_DELEGATE_H_
