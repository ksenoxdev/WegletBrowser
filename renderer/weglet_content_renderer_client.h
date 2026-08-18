// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/renderer/weglet_content_renderer_client.h

#ifndef WEGLET_RENDERER_WEGLET_CONTENT_RENDERER_CLIENT_H_
#define WEGLET_RENDERER_WEGLET_CONTENT_RENDERER_CLIENT_H_

#include "content/public/renderer/content_renderer_client.h"

namespace weglet {

// Deliberately empty. Anything added here runs inside the renderer,
// which is the process that parses untrusted HTML -- the default answer
// to "should this live in the renderer?" is no.
class WegletContentRendererClient : public content::ContentRendererClient {
 public:
  WegletContentRendererClient();
  WegletContentRendererClient(const WegletContentRendererClient&) = delete;
  WegletContentRendererClient& operator=(const WegletContentRendererClient&) =
      delete;
  ~WegletContentRendererClient() override;
};

}  // namespace weglet

#endif  // WEGLET_RENDERER_WEGLET_CONTENT_RENDERER_CLIENT_H_
