// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/app/weglet_main.cc
//
// Process entry point. Every Weglet process -- browser, renderer, GPU,
// utility -- starts here; content::ContentMain reads the command line to
// work out which one it is and dispatches accordingly.

#include "weglet/app/weglet_main_delegate.h"

#include "content/public/app/content_main.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/win_util.h"
#include "content/public/app/sandbox_helper_win.h"
#include "sandbox/win/src/sandbox_types.h"

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  // Populated by InitializeSandboxInfo and handed to every child
  // process; without it the renderer starts unsandboxed.
  sandbox::SandboxInterfaceInfo sandbox_info = {nullptr};
  content::InitializeSandboxInfo(&sandbox_info);

  weglet::WegletMainDelegate delegate;
  content::ContentMainParams params(&delegate);
  params.instance = instance;
  params.sandbox_info = &sandbox_info;

  // Windows kills the process on some heap corruption instead of
  // letting it limp on with a corrupted allocator.
  base::win::EnableHighDPISupport();

  return content::ContentMain(std::move(params));
}
#else
int main(int argc, const char** argv) {
  weglet::WegletMainDelegate delegate;
  content::ContentMainParams params(&delegate);
  params.argc = argc;
  params.argv = argv;
  return content::ContentMain(std::move(params));
}
#endif