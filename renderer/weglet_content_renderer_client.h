// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// ContentRendererClient. Mostly empty: this runs in the process that
// parses untrusted HTML. The one thing it does is attach a
// WegletRenderFrameObserver to every frame, so window.chrome exists the
// way it does in real Chrome -- see weglet_render_frame_observer.h.

#ifndef WEGLET_RENDERER_WEGLET_CONTENT_RENDERER_CLIENT_H_
#define WEGLET_RENDERER_WEGLET_CONTENT_RENDERER_CLIENT_H_

#include <memory>
#include <string>

#include "content/public/renderer/content_renderer_client.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/local_interface_provider.h"

class SpellCheck;

namespace weglet {

class WegletContentRendererClient
    : public content::ContentRendererClient,
      public service_manager::LocalInterfaceProvider {
 public:
  WegletContentRendererClient();
  WegletContentRendererClient(const WegletContentRendererClient&) = delete;
  WegletContentRendererClient& operator=(const WegletContentRendererClient&) =
      delete;
  ~WegletContentRendererClient() override;

  // content::ContentRendererClient:
  void RenderThreadStarted() override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;

  // service_manager::LocalInterfaceProvider:
  //
  // SpellCheck's HunspellEngine calls this to ask the browser for a
  // dictionary it doesn't already have. Weglet's own language always
  // arrives pre-initialized instead (see
  // WegletSpellCheckHost::InitializeDictionaries), so this never actually
  // fires; dropping the pipe is the correct no-op.
  void GetInterface(const std::string& name,
                    mojo::ScopedMessagePipeHandle request_handle) override;

 private:
  std::unique_ptr<SpellCheck> spellcheck_;
};

}  // namespace weglet

#endif  // WEGLET_RENDERER_WEGLET_CONTENT_RENDERER_CLIENT_H_
