// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/ui/src/newtab.ts

import { byId, el, replaceChildren } from "./dom.js";
import { receive, send, type Shortcut } from "./protocol.js";
import { tokens } from "./tokens.js";

function shortcutTile(shortcut: Shortcut, index: number): HTMLElement {
  // The first letter, not a favicon. Fetching one means asking a third
  // party what the user has open, and this browser does not do that --
  // see docs/security.md. A real icon comes from the page itself once
  // there is a cache for it.
  const initial = [...shortcut.title][0]?.toUpperCase() ?? "?";

  return el("button", {
    className: "tile",
    title: shortcut.url,
    onClick: () => send({ kind: "shortcut-clicked", index }),
    children: [
      el("span", { className: "tile-badge", text: initial }),
      el("span", { className: "tile-title", text: shortcut.title }),
    ],
  });
}

function render(shortcuts: readonly Shortcut[]): void {
  const dock = byId("dock");

  if (shortcuts.length === 0) {
    replaceChildren(dock, [
      el("p", {
        className: "empty",
        text: "Pinned sites appear here. Press Ctrl+D on a page to pin it.",
      }),
    ]);
    return;
  }

  // Capped at the same number the browser enforces, so a hand-edited
  // settings file cannot produce a dock that overflows its own grid.
  const visible = shortcuts.slice(0, tokens.layoutDockMaxTiles);
  replaceChildren(dock, visible.map(shortcutTile));
}

function main(): void {
  render([]);

  receive((message) => {
    if (message.kind === "newtab") {
      render(message.shortcuts);
    }
  });
}

main();
