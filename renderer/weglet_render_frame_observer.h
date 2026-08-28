// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// Installs window.chrome.loadTimes()/csi() on every script context, the
// same way ChromeRenderFrameObserver does for real Chrome. See
// weglet_loadtimes_bindings.h for why.

#ifndef WEGLET_RENDERER_WEGLET_RENDER_FRAME_OBSERVER_H_
#define WEGLET_RENDERER_WEGLET_RENDER_FRAME_OBSERVER_H_

#include "content/public/renderer/render_frame_observer.h"

namespace weglet {

class WegletRenderFrameObserver : public content::RenderFrameObserver {
 public:
  explicit WegletRenderFrameObserver(content::RenderFrame* render_frame);
  WegletRenderFrameObserver(const WegletRenderFrameObserver&) = delete;
  WegletRenderFrameObserver& operator=(const WegletRenderFrameObserver&) = delete;

  // content::RenderFrameObserver:
  void DidCreateScriptContext(v8::Local<v8::Context> context,
                              int32_t world_id) override;
  void OnDestruct() override;
};

}  // namespace weglet

#endif  // WEGLET_RENDERER_WEGLET_RENDER_FRAME_OBSERVER_H_
