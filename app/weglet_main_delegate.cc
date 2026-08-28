// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// ContentMainDelegate: logging, the resource pack, the client objects.

#include "weglet/app/weglet_main_delegate.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/logging/logging_settings.h"
#include "base/path_service.h"
#include "content/public/common/content_switches.h"
#include "ui/base/resource/resource_bundle.h"
#include "weglet/browser/weglet_content_browser_client.h"
#include "weglet/common/weglet_content_client.h"
#include "weglet/renderer/weglet_content_renderer_client.h"

namespace weglet {

WegletMainDelegate::WegletMainDelegate() = default;
WegletMainDelegate::~WegletMainDelegate() = default;

std::optional<int> WegletMainDelegate::BasicStartupComplete() {
  // LOG_DEFAULT implies LOG_TO_FILE on Windows, which CHECKs for a path
  // the defaults don't set -- so the defaults abort before the first window.
  logging::LoggingSettings settings;

  // A sandboxed renderer can't open a file by path.
  const bool is_browser_process = !base::CommandLine::ForCurrentProcess()
                                       ->HasSwitch(switches::kProcessType);

  // Must outlive InitLogging: log_file_path points into this string.
  base::FilePath log_path;
  if (is_browser_process) {
    base::FilePath dir;
    if (base::PathService::Get(base::DIR_TEMP, &dir)) {
      log_path = dir.AppendASCII("weglet.log");
      settings.log_file_path = log_path.value().c_str();
      settings.logging_dest = logging::LOG_TO_ALL;
    } else {
      settings.logging_dest = logging::LOG_TO_STDERR;
    }
  } else {
    settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  }
  logging::InitLogging(settings);

  return std::nullopt;
}

void WegletMainDelegate::PreSandboxStartup() {
  // The renderer's sandbox closes right after this returns.
  InitializeResourceBundle();
}

void WegletMainDelegate::InitializeResourceBundle() {
  base::FilePath pak_dir;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &pak_dir));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(
      pak_dir.AppendASCII("weglet.pak"));
}

content::ContentClient* WegletMainDelegate::CreateContentClient() {
  content_client_ = std::make_unique<WegletContentClient>();
  return content_client_.get();
}

content::ContentBrowserClient* WegletMainDelegate::CreateContentBrowserClient() {
  browser_client_ = std::make_unique<WegletContentBrowserClient>();
  return browser_client_.get();
}

content::ContentRendererClient*
WegletMainDelegate::CreateContentRendererClient() {
  renderer_client_ = std::make_unique<WegletContentRendererClient>();
  return renderer_client_.get();
}

}  // namespace weglet
