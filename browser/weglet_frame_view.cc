// Copyright 2026 Weglet - Licensed under Apache 2.0

#include "weglet/browser/weglet_frame_view.h"

#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/client_view.h"

namespace weglet {

namespace {

// No visible border to grab, so this has to reach past the window's
// edge and into the toolbar, or the hit target shrinks to nothing.
constexpr int kResizeBorder = 6;
constexpr int kResizeCornerSize = 16;

}  // namespace

WegletFrameView::WegletFrameView(views::Widget* widget) : widget_(widget) {}

WegletFrameView::~WegletFrameView() = default;

void WegletFrameView::SetDraggableRegions(
    const std::vector<blink::mojom::DraggableRegionPtr>& regions) {
  // Same algorithm AppWindow::RawDraggableRegionsToSkRegion uses: later
  // regions can carve non-draggable holes out of earlier ones, so this
  // replays them in order rather than unioning every draggable rect.
  draggable_region_.setEmpty();
  for (const auto& region : regions) {
    draggable_region_.op(
        SkIRect::MakeLTRB(region->bounds.x(), region->bounds.y(),
                          region->bounds.right(), region->bounds.bottom()),
        region->draggable ? SkRegion::kUnion_Op : SkRegion::kDifference_Op);
  }
}

gfx::Rect WegletFrameView::GetBoundsForClientView() const {
  // No caption strip of our own: the toolbar page draws the whole top
  // of the window, including the parts that answer as HTCAPTION below.
  return bounds();
}

gfx::Rect WegletFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return client_bounds;
}

int WegletFrameView::NonClientHitTest(const gfx::Point& point) {
  if (widget_->IsFullscreen()) {
    return HTCLIENT;
  }
  if (!bounds().Contains(point)) {
    return HTNOWHERE;
  }

  // Resize handles first: they win even over a draggable region right
  // at the edge, or a window with a full-bleed drag strip could not be
  // resized from that edge at all.
  const bool can_resize =
      widget_->widget_delegate() && widget_->widget_delegate()->CanResize();
  const int resize_border =
      (widget_->IsMaximized() || widget_->IsFullscreen()) ? 0 : kResizeBorder;
  const int frame_component =
      GetHTComponentForFrame(point, gfx::Insets(resize_border),
                             kResizeCornerSize, kResizeCornerSize, can_resize);
  if (frame_component != HTNOWHERE) {
    return frame_component;
  }

  if (draggable_region_.contains(point.x(), point.y())) {
    return HTCAPTION;
  }

  // Everything else is the toolbar or the page underneath it, both of
  // which want ordinary clicks -- the client view claims any point in
  // its own bounds as HTCLIENT.
  const int client_component = widget_->client_view()->NonClientHitTest(point);
  if (client_component != HTNOWHERE) {
    return client_component;
  }

  return HTCAPTION;
}

}  // namespace weglet
