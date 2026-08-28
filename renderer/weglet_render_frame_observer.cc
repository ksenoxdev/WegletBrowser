// Copyright 2026 Weglet - Licensed under Apache 2.0

#include "weglet/renderer/weglet_render_frame_observer.h"

#include "weglet/renderer/weglet_loadtimes_bindings.h"

namespace weglet {

WegletRenderFrameObserver::WegletRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {}

void WegletRenderFrameObserver::DidCreateScriptContext(
    v8::Local<v8::Context> context,
    int32_t world_id) {
  WegletLoadTimesBindings::Install(context);
}

void WegletRenderFrameObserver::OnDestruct() {
  delete this;
}

}  // namespace weglet
