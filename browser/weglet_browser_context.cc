// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/browser/weglet_browser_context.cc

#include "weglet/browser/weglet_browser_context.h"

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "build/build_config.h"

namespace weglet {

WegletBrowserContext::WegletBrowserContext(bool off_the_record)
    : off_the_record_(off_the_record), path_(DefaultProfilePath()) {
  if (!off_the_record_ && !base::CreateDirectory(path_)) {
    // Not fatal: the content layer falls back to memory-backed storage,
    // so the browser still runs -- it just forgets everything on exit.
    LOG(ERROR) << "could not create the profile directory at " << path_
               << "; this session will not be saved";
  }
}

WegletBrowserContext::~WegletBrowserContext() {
  // Tears down keyed services in the right order; skipping it trips a
  // DCHECK in the base class destructor.
  NotifyWillBeDestroyed();
  ShutdownStoragePartitions();
}

base::FilePath WegletBrowserContext::DefaultProfilePath() {
  base::FilePath dir;
#if BUILDFLAG(IS_WIN)
  CHECK(base::PathService::Get(base::DIR_LOCAL_APP_DATA, &dir));
#else
  CHECK(base::PathService::Get(base::DIR_HOME, &dir));
  dir = dir.AppendASCII(".local").AppendASCII("share");
#endif
  return dir.AppendASCII("Weglet").AppendASCII("Default");
}

base::FilePath WegletBrowserContext::GetPath() const {
  return path_;
}

bool WegletBrowserContext::IsOffTheRecord() {
  return off_the_record_;
}

std::unique_ptr<content::ZoomLevelDelegate>
WegletBrowserContext::CreateZoomLevelDelegate(const base::FilePath&) {
  // Per-host zoom is not persisted yet, so the default in-memory
  // behaviour is the honest one.
  return nullptr;
}

content::DownloadManagerDelegate*
WegletBrowserContext::GetDownloadManagerDelegate() {
  return nullptr;
}

content::BrowserPluginGuestManager* WegletBrowserContext::GetGuestManager() {
  return nullptr;
}

storage::SpecialStoragePolicy*
WegletBrowserContext::GetSpecialStoragePolicy() {
  return nullptr;
}

content::PlatformNotificationService*
WegletBrowserContext::GetPlatformNotificationService() {
  // No web notifications. A browser that promises no unrequested
  // network traffic has no business holding a push channel open.
  return nullptr;
}

content::PushMessagingService*
WegletBrowserContext::GetPushMessagingService() {
  return nullptr;
}

content::StorageNotificationService*
WegletBrowserContext::GetStorageNotificationService() {
  return nullptr;
}

content::SSLHostStateDelegate*
WegletBrowserContext::GetSSLHostStateDelegate() {
  // Returning nullptr means certificate errors are never remembered, so
  // "proceed anyway" does not stick across restarts. That is the safer
  // default and it stays until there is a UI to manage exceptions.
  return nullptr;
}

content::PermissionControllerDelegate*
WegletBrowserContext::GetPermissionControllerDelegate() {
  // Without a delegate the content layer denies every permission
  // request. Camera, microphone and geolocation are off until Weglet
  // has UI to ask about them properly.
  return nullptr;
}

content::ClientHintsControllerDelegate*
WegletBrowserContext::GetClientHintsControllerDelegate() {
  return nullptr;
}

content::BackgroundFetchDelegate*
WegletBrowserContext::GetBackgroundFetchDelegate() {
  return nullptr;
}

content::BackgroundSyncController*
WegletBrowserContext::GetBackgroundSyncController() {
  return nullptr;
}

content::BrowsingDataRemoverDelegate*
WegletBrowserContext::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

content::ReduceAcceptLanguageControllerDelegate*
WegletBrowserContext::GetReduceAcceptLanguageControllerDelegate() {
  return nullptr;
}

}  // namespace weglet
