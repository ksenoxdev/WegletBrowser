// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// The new tab page: clock, search field, dock.

import { spawnRipple } from "./anim.js";
import {byId, el, runPage} from "./dom.js";
import { faviconUrl } from "./favicon.js";
import { applyI18n, t, type Language } from "./i18n.js";
import { setIcon } from "./icons.js";
import { createContextMenu, createModal } from "./modal.js";
import { onNewTab, send, type NewTabState, type Shortcut } from "./protocol.js";
import { applyAccent, applyAddressBarShape } from "./theme.js";

let tiles: HTMLElement[] = [];
let shortcuts: readonly Shortcut[] = [];
// Which shortcut the context menu was opened on. -1 for the add tile.
let menuTarget = -1;
// Read by the modal's open/edit handlers, which run outside render().
let currentLanguage: Language = "en";

// ---------------------------------------------------------------------
// Clock
// ---------------------------------------------------------------------

function renderClock(): void {
  const now = new Date();
  const hours = String(now.getHours()).padStart(2, "0");
  const minutes = String(now.getMinutes()).padStart(2, "0");
  byId("clock").textContent = `${hours}:${minutes}`;
}

function startClock(): void {
  renderClock();
  // Ticks on the minute: 59 of every 60 wake-ups changed nothing, and this
  // page is open in every new tab.
  const msToNextMinute = 60_000 - (Date.now() % 60_000);
  window.setTimeout(() => {
    renderClock();
    window.setInterval(renderClock, 60_000);
  }, msToNextMinute);
}

// ---------------------------------------------------------------------
// Dock
// ---------------------------------------------------------------------

function dockSlot(
  tip: string,
  tile: HTMLElement,
  onContextMenu: ((event: MouseEvent) => void) | null,
): HTMLElement {
  const slot = el("div", { className: "dock-slot", children: [tile] });
  // An attribute, not an element: the dock has no room for real nodes.
  slot.dataset.tip = tip;

  if (onContextMenu) {
    slot.addEventListener("contextmenu", (event) => {
      event.preventDefault();
      onContextMenu(event);
    });
  }
  return slot;
}

function shortcutTile(shortcut: Shortcut, faviconsEnabled: boolean): HTMLElement {
  // First letter by default -- fetching a favicon tells a third party
  // what's pinned. See docs/security.md.
  const initial = [...shortcut.title][0]?.toUpperCase() ?? "?";

  const tile = el("div", { className: "dock-tile", text: initial });
  tile.setAttribute("role", "button");
  tile.tabIndex = 0;

  const src = faviconsEnabled ? faviconUrl(shortcut.url) : null;
  if (src) {
    const img = el("img", { className: "dock-tile-mark" });
    img.src = src;
    img.alt = "";
    img.referrerPolicy = "no-referrer";
    // Falls back to the letter already sitting in the tile: removing
    // the image just uncovers it.
    img.addEventListener("error", () => img.remove(), { once: true });
    tile.appendChild(img);
  }

  const activate = (): void => {
    send("navigate", shortcut.url);
  };

  tile.addEventListener("click", activate);
  tile.addEventListener("keydown", (event: Event) => {
    const key = (event as KeyboardEvent).key;
    if (key === "Enter" || key === " ") {
      event.preventDefault();
      activate();
    }
  });
  return tile;
}

function addTile(): HTMLElement {
  const tile = el("div", { className: "dock-tile add" });
  setIcon(tile, "plus", 22);
  tile.setAttribute("role", "button");
  tile.setAttribute("aria-label", "Add shortcut");
  tile.tabIndex = 0;

  const open = (): void => {
    menuTarget = -1;
    byId("modal-title").textContent = t("newtab.addShortcutTitle", currentLanguage);
    byId<HTMLInputElement>("shortcut-name").value = "";
    byId<HTMLInputElement>("shortcut-url").value = "";
    byId("shortcut-confirm").textContent = t("newtab.add", currentLanguage);
    modal.open();
  };

  tile.addEventListener("click", open);
  tile.addEventListener("keydown", (event: Event) => {
    const key = (event as KeyboardEvent).key;
    if (key === "Enter" || key === " ") {
      event.preventDefault();
      open();
    }
  });
  return tile;
}

function renderDock(state: NewTabState): void {
  currentLanguage = state.language;
  applyI18n(state.language);
  applyAccent(state.accentColor);
  applyAddressBarShape(state.addressBarShape);
  shortcuts = state.shortcuts;
  byId("hint").textContent = state.hint;
  const dock = byId("dock");
  const slots: HTMLElement[] = [];
  tiles = [];

  state.shortcuts.forEach((shortcut, index) => {
    const tile = shortcutTile(shortcut, state.faviconsEnabled);
    tiles.push(tile);
    slots.push(
      dockSlot(shortcut.title, tile, (event) => {
        menuTarget = index;
        menu.openAt(event.clientX, event.clientY);
      }),
    );
  });

  // The add tile is a slot like any other.
  const add = addTile();
  tiles.push(add);
  slots.push(dockSlot("Add shortcut", add, null));

  dock.replaceChildren(...slots);
}

// ---------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------

function wireSearch(): void {
  const form = byId("search-form");
  const bar = byId<HTMLInputElement>("search-bar");
  // No mark yet: it should be the engine's own logo. The field keeps its
  // right padding so its proportions do not change when one arrives.

  // autofocus fires a real focus event on load. The sweep marks the user
  // arriving, not the page opening, so the first moment is ignored.
  let ignoreAutofocus = true;
  window.setTimeout(() => {
    ignoreAutofocus = false;
  }, 200);

  bar.addEventListener("focus", () => {
    form.classList.add("focused");
    if (ignoreAutofocus) {
      return;
    }
    form.classList.remove("neon-play", "neon-drift");
    // Forces a reflow so the animation restarts rather than coalescing.
    void form.offsetWidth;
    form.classList.add("neon-play");
  });

  bar.addEventListener("blur", () => {
    form.classList.remove("focused", "neon-play", "neon-drift");
  });

  // One fast lap, then a slow drift: the arrival should be noticed and the
  // idle state should not.
  form.addEventListener("animationend", () => {
    if (form.classList.contains("neon-play")) {
      form.classList.remove("neon-play");
      form.classList.add("neon-drift");
    }
  });

  bar.addEventListener("keydown", (event: Event) => {
    if ((event as KeyboardEvent).key !== "Enter") {
      return;
    }
    const text = bar.value.trim();
    if (text) {
      send("navigate", text);
      bar.value = "";
    }
  });
}

// ---------------------------------------------------------------------

const modal = createModal({
  backdropId: "shortcut-backdrop",
  focusId: "shortcut-name",
  cancelId: "shortcut-cancel",
  onConfirm: () => {
    const title = byId<HTMLInputElement>("shortcut-name").value.trim();
    const url = byId<HTMLInputElement>("shortcut-url").value.trim();
    if (!title || !url) {
      return;
    }
    if (menuTarget >= 0) {
      send("editShortcut", menuTarget, title, url);
    } else {
      send("addShortcut", title, url);
    }
    modal.close();
  },
});

const menu = createContextMenu("shortcut-menu", [
  {
    id: "menu-edit",
    onActivate: () => {
      const shortcut = shortcuts[menuTarget];
      if (!shortcut) {
        return;
      }
      byId("modal-title").textContent = t("newtab.editShortcutTitle", currentLanguage);
      byId<HTMLInputElement>("shortcut-name").value = shortcut.title;
      byId<HTMLInputElement>("shortcut-url").value = shortcut.url;
      byId("shortcut-confirm").textContent = t("newtab.save", currentLanguage);
      modal.open();
    },
  },
  {
    id: "menu-delete",
    onActivate: () => {
      if (menuTarget >= 0) {
        send("removeShortcut", menuTarget);
      }
    },
  },
]);

function main(): void {
  startClock();
  wireSearch();

  byId("shortcut-cancel").addEventListener("click", () => modal.close());
  byId("shortcut-confirm").addEventListener("click", (event) => {
    spawnRipple(byId("shortcut-confirm"), event.clientX, event.clientY);
  });

  onNewTab(renderDock);
}

runPage(main);
