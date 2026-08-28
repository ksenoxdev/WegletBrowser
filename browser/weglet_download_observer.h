// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// Forwards content::DownloadManager's real download lifecycle into the
// profile's own download record. See weglet-profile/src/downloads.rs for
// what happens to it on the other side of the FFI boundary.

#ifndef WEGLET_BROWSER_WEGLET_DOWNLOAD_OBSERVER_H_
#define WEGLET_BROWSER_WEGLET_DOWNLOAD_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/download_manager.h"

namespace weglet {

class WegletBridge;
class WegletStateService;

// Owned by WegletBrowserMainParts, not attached to the BrowserContext as
// user data like WegletSecurityGuard/WegletStateService are: registering
// as a DownloadManager::Observer has to happen exactly once, right after
// the manager exists, which main_parts already knows how to time.
class WegletDownloadObserver : public content::DownloadManager::Observer,
                               public download::DownloadItem::Observer {
 public:
  // `manager`, `bridge` and `state` must outlive this observer.
  WegletDownloadObserver(content::DownloadManager* manager,
                         WegletBridge* bridge,
                         WegletStateService* state);
  WegletDownloadObserver(const WegletDownloadObserver&) = delete;
  WegletDownloadObserver& operator=(const WegletDownloadObserver&) = delete;
  ~WegletDownloadObserver() override;

  // content::DownloadManager::Observer:
  void OnDownloadCreated(content::DownloadManager* manager,
                        download::DownloadItem* item) override;
  void ManagerGoingDown(content::DownloadManager* manager) override;

  // download::DownloadItem::Observer:
  void OnDownloadUpdated(download::DownloadItem* item) override;
  void OnDownloadDestroyed(download::DownloadItem* item) override;

 private:
  raw_ptr<content::DownloadManager> manager_;
  raw_ptr<WegletBridge> bridge_;
  raw_ptr<WegletStateService> state_;
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_DOWNLOAD_OBSERVER_H_
