// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_browser_context.h

#ifndef WEGLET_BROWSER_WEGLET_BROWSER_CONTEXT_H_
#define WEGLET_BROWSER_WEGLET_BROWSER_CONTEXT_H_

#include <memory>

#include "base/files/file_path.h"
#include "content/public/browser/browser_context.h"

namespace weglet {

// One browsing profile: cookies, cache, storage, the lot. Everything on
// disk hangs off the path this returns.
//
// Almost every getter below returns nullptr. That is not laziness -- each
// one is a subsystem the content layer treats as optional, and every one
// we turn on is another thing that can talk to the network on its own.
// They get added when there is a reason, one at a time.
class WegletBrowserContext : public content::BrowserContext {
 public:
  explicit WegletBrowserContext(bool off_the_record);
  WegletBrowserContext(const WegletBrowserContext&) = delete;
  WegletBrowserContext& operator=(const WegletBrowserContext&) = delete;
  ~WegletBrowserContext() override;

  // content::BrowserContext:
  base::FilePath GetPath() const override;
  bool IsOffTheRecord() override;
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PlatformNotificationService* GetPlatformNotificationService()
      override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::StorageNotificationService* GetStorageNotificationService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override;
  content::ClientHintsControllerDelegate* GetClientHintsControllerDelegate()
      override;
  content::BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;
  content::BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate()
      override;
  content::ReduceAcceptLanguageControllerDelegate*
  GetReduceAcceptLanguageControllerDelegate() override;

 private:
  // %LOCALAPPDATA%\Weglet on Windows, the XDG data dir elsewhere. Chosen
  // once in the constructor so every caller agrees on it.
  static base::FilePath DefaultProfilePath();

  const bool off_the_record_;
  base::FilePath path_;
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_BROWSER_CONTEXT_H_
