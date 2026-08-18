# Copyright 2026 Weglet - Licensed under Apache 2.0
#
# Only the public surface. Reaching into content/browser or blink from
# here would compile, but it would also tie Weglet to Chromium's
# internals and turn every upstream refactor into our problem.

include_rules = [
  "+base",
  "+build",
  "+components/embedder_support",
  "+content/public/app",
  "+content/public/browser",
  "+content/public/common",
  "+content/public/renderer",
  "+net",
  "+services/network/public/mojom",
  "+sandbox",
  "+ui/aura",
  "+ui/base",
  # Keyboard codes, for the accelerators the window registers. Public UI
  # headers like everything else here: nothing under ui/events/blink or
  # any other internal path.
  "+ui/events/keycodes",
  "+ui/display",
  "+ui/gfx",
  "+ui/views",
  "+ui/wm",
  "+url",
]
