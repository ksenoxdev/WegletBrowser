// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_tab_observer.cc

#include "weglet/browser/weglet_tab_observer.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "url/gurl.h"
#include "weglet/browser/weglet_window.h"

namespace weglet {

WegletTabObserver::WegletTabObserver(WegletWindow* window,
                                    uint64_t tab_id,
                                    content::WebContents* contents)
    : content::WebContentsObserver(contents), window_(window), tab_id_(tab_id) {}

WegletTabObserver::~WegletTabObserver() = default;

void WegletTabObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Subframes and uncommitted navigations are not where the tab is. An
  // error page is: the tab really is showing that URL, and hiding it would
  // leave the address bar claiming the previous page.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  // A same-document navigation is history.pushState or a fragment change.
  // The window turns this into a replace rather than a new history entry --
  // otherwise Back inside a single-page app walks entries the engine
  // already handled itself.
  window_->OnTabNavigated(tab_id_, navigation_handle->GetURL(),
                          navigation_handle->IsSameDocument());
}

void WegletTabObserver::TitleWasSet(content::NavigationEntry* entry) {
  const std::u16string title = entry ? entry->GetTitle() : std::u16string();
  window_->OnTabTitleChanged(tab_id_, base::UTF16ToUTF8(title));
}

void WegletTabObserver::DidStartLoading() {
  window_->OnTabLoadingChanged(tab_id_, true);
}

void WegletTabObserver::DidStopLoading() {
  window_->OnTabLoadingChanged(tab_id_, false);
}

}  // namespace weglet
