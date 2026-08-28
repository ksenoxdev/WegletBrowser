// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// The dropdown opened by the toolbar's "⋮" button. Its own floating
// widget, not a tab -- see WegletMenuPopup -- so a fresh one opens (and
// this script runs fresh) on every toggle.

import { spawnRipple } from "./anim.js";
import { byId, runPage } from "./dom.js";
import { applyI18n } from "./i18n.js";
import { setIcon } from "./icons.js";
import { onMenu, send, type MenuState } from "./protocol.js";
import { applyAccent, applyAddressBarShape } from "./theme.js";

function hide(): void {
  send("hideMenu");
}

function go(address: string): void {
  send("navigate", address);
  hide();
}

function main(): void {
  setIcon(byId("menu-settings-icon"), "settings", 15);
  setIcon(byId("menu-history-icon"), "history", 15);
  setIcon(byId("menu-bookmarks-icon"), "bookmark", 15);

  const settings = byId<HTMLButtonElement>("menu-settings");
  const history = byId<HTMLButtonElement>("menu-history");
  const bookmarks = byId<HTMLButtonElement>("menu-bookmarks");

  settings.addEventListener("click", (event) => {
    spawnRipple(settings, event.clientX, event.clientY);
    go("weglet://settings");
  });
  history.addEventListener("click", (event) => {
    spawnRipple(history, event.clientX, event.clientY);
    go("weglet://history");
  });
  bookmarks.addEventListener("click", (event) => {
    spawnRipple(bookmarks, event.clientX, event.clientY);
    go("weglet://bookmarks");
  });

  // Losing focus or pressing Escape both mean "never mind" -- there is
  // no separate close button, matching a dropdown more than a dialog.
  window.addEventListener("blur", hide);
  window.addEventListener("keydown", (event: Event) => {
    if ((event as KeyboardEvent).key === "Escape") {
      hide();
    }
  });

  onMenu((state: MenuState) => {
    applyI18n(state.language);
    applyAccent(state.accentColor);
    applyAddressBarShape(state.addressBarShape);
  });
}

runPage(main);
