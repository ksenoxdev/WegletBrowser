// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/ui/src/bookmarks.ts
//
// A flat, newest-first list of saved pages, backed by the profile's own
// bookmarks store.

import { byId, el, runPage } from "./dom.js";
import { faviconUrl } from "./favicon.js";
import { applyI18n, t, type Language } from "./i18n.js";
import { setIcon } from "./icons.js";
import { onBookmarks, send, type BookmarkEntry } from "./protocol.js";
import { applyAccent, applyAddressBarShape } from "./theme.js";

let currentLanguage: Language = "en";

function row(entry: BookmarkEntry, index: number, faviconsEnabled: boolean): HTMLElement {
  // First letter by default; see docs/security.md.
  const initial = [...entry.title][0]?.toUpperCase() ?? "?";
  const icon = el("div", { className: "row-icon", text: initial });

  const src = faviconsEnabled ? faviconUrl(entry.url) : null;
  if (src) {
    const mark = el("img", { className: "row-icon-mark" });
    mark.src = src;
    mark.alt = "";
    mark.referrerPolicy = "no-referrer";
    mark.addEventListener("error", () => mark.remove(), { once: true });
    icon.appendChild(mark);
  }

  const remove = el("button", {
    className: "row-remove",
    title: t("bookmarks.remove", currentLanguage),
  });
  setIcon(remove, "x", 13);
  remove.addEventListener("click", (event) => {
    event.stopPropagation();
    // By index, which is how the profile stores the list. The browser
    // bounds-checks it, so a stale index from a race with another window
    // is a dropped message, not a wrong removal.
    send("removeBookmark", index);
  });

  const line = el("div", {
    className: "row",
    children: [
      icon,
      el("div", {
        className: "row-text",
        children: [
          el("div", { className: "row-title", text: entry.title }),
          el("div", { className: "row-sub", text: entry.url }),
        ],
      }),
      remove,
    ],
    // el()'s onClick, not a bare listener, so the row is keyboard-reachable.
    onClick: () => {
      send("navigate", entry.url);
    },
  });
  return line;
}

function render(entries: readonly BookmarkEntry[], faviconsEnabled: boolean): void {
  const list = byId("bookmarks-list");
  for (const child of [...list.children]) {
    child.remove();
  }
  if (entries.length === 0) {
    list.appendChild(el("div", { className: "empty", text: t("bookmarks.empty", currentLanguage) }));
    return;
  }
  entries.forEach((entry, index) => list.appendChild(row(entry, index, faviconsEnabled)));
}

function main(): void {
  byId("done").addEventListener("click", () => send("goBack"));
  onBookmarks((state) => {
    currentLanguage = state.language;
    applyI18n(state.language);
    applyAccent(state.accentColor);
    applyAddressBarShape(state.addressBarShape);
    render(state.entries, state.faviconsEnabled);
  });
}

runPage(main);
