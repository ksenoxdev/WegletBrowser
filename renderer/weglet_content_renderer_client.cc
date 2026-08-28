// Copyright 2026 Weglet - Licensed under Apache 2.0

#include "weglet/renderer/weglet_content_renderer_client.h"

#include "components/spellcheck/renderer/spellcheck.h"
#include "components/spellcheck/renderer/spellcheck_provider.h"
#include "weglet/renderer/weglet_render_frame_observer.h"

namespace weglet {

WegletContentRendererClient::WegletContentRendererClient() = default;
WegletContentRendererClient::~WegletContentRendererClient() = default;

void WegletContentRendererClient::RenderThreadStarted() {
  spellcheck_ = std::make_unique<SpellCheck>(this);
}

void WegletContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  // Self-owning: deletes itself from OnDestruct, same as the frame it
  // watches deletes itself.
  new WegletRenderFrameObserver(render_frame);
  // Self-owning the same way, via its own RenderFrameObserver base.
  new SpellCheckProvider(render_frame, spellcheck_.get());
}

void WegletContentRendererClient::GetInterface(
    const std::string& name,
    mojo::ScopedMessagePipeHandle request_handle) {}

}  // namespace weglet
