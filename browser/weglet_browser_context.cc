// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// The browsing profile and the subsystems it does not enable.

#include "weglet/browser/weglet_browser_context.h"

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "components/embedder_support/user_agent_utils.h"
#include "url/gurl.h"

namespace weglet {

namespace {

// WegletBrowserContext does not offer a way to disable JavaScript.
bool IsJavaScriptAlwaysAllowed(const GURL&) {
  return true;
}

}  // namespace

WegletBrowserContext::WegletBrowserContext(bool off_the_record)
    : off_the_record_(off_the_record),
      path_(DefaultProfilePath()),
      client_hints_delegate_(/*network_quality_tracker=*/nullptr,
                             base::BindRepeating(&IsJavaScriptAlwaysAllowed),
                             embedder_support::GetUserAgentMetadata()) {
  if (!off_the_record_ && !base::CreateDirectory(path_)) {
    // Not fatal: content falls back to memory-backed storage.
    LOG(ERROR) << "could not create the profile directory at " << path_
               << "; this session will not be saved";
  }

#if BUILDFLAG(IS_WIN)
  // base::PreventExecuteMapping hard-crashes the browser if a writable
  // file handle passed to a renderer isn't under a path it recognises.
  // `path_` has no PathService key of its own for it to recognise, so
  // register one (5000: clear of base's and content's own ranges).
  static constexpr int kDirUserData = 5000;
  if (base::PathService::Override(kDirUserData, path_)) {
    base::SetExtraNoExecuteAllowedPath(kDirUserData);
  }
#endif  // BUILDFLAG(IS_WIN)
}

WegletBrowserContext::~WegletBrowserContext() {
  // Tears down keyed services in order; skipping it trips a DCHECK in the
  // base class destructor.
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
  // Per-host zoom is not persisted, so in-memory is the honest answer.
  return nullptr;
}

content::DownloadManagerDelegate*
WegletBrowserContext::GetDownloadManagerDelegate() {
  return &download_manager_delegate_;
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
  // No web notifications: a push channel is unrequested network traffic.
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
  // Certificate errors are never remembered, so "proceed anyway" does not
  // stick across restarts.
  return nullptr;
}

content::PermissionControllerDelegate*
WegletBrowserContext::GetPermissionControllerDelegate() {
  // Grants only clipboard write (see WegletPermissionDelegate). Camera,
  // microphone and geolocation stay off until there is UI to ask.
  return &permission_delegate_;
}

content::ClientHintsControllerDelegate*
WegletBrowserContext::GetClientHintsControllerDelegate() {
  return &client_hints_delegate_;
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
