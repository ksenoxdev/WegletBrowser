// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// The whole non-client area for WegletWindow: no OS caption, no OS
// border. See WegletWindow::Init (remove_standard_frame) and
// WegletWindow::CreateFrameView.

#ifndef WEGLET_BROWSER_WEGLET_FRAME_VIEW_H_
#define WEGLET_BROWSER_WEGLET_FRAME_VIEW_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom-forward.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/window/frame_view.h"

namespace views {
class Widget;
}

namespace weglet {

// With no OS-drawn caption, this answers what the OS otherwise would:
// where the resize edge is (a fixed margin) and what counts as the
// title bar (the toolbar's -webkit-app-region: drag CSS, reported via
// DraggableRegionsChanged into SetDraggableRegions).
class WegletFrameView : public views::FrameView {
 public:
  explicit WegletFrameView(views::Widget* widget);
  WegletFrameView(const WegletFrameView&) = delete;
  WegletFrameView& operator=(const WegletFrameView&) = delete;
  ~WegletFrameView() override;

  // `regions` are in this view's own coordinates -- the toolbar page
  // always sits flush with the window's top-left corner (see
  // WegletWindow::Init), so what content reports needs no translation.
  void SetDraggableRegions(
      const std::vector<blink::mojom::DraggableRegionPtr>& regions);

  // views::FrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;

 private:
  const raw_ptr<views::Widget> widget_;
  SkRegion draggable_region_;
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_FRAME_VIEW_H_
