// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/ui/src/newtab.ts

import { register, scheduleFrame, spawnRipple, Strip } from "./anim.js";
import {byId, el, runPage} from "./dom.js";
import { setIcon } from "./icons.js";
import { createContextMenu, createModal } from "./modal.js";
import { onNewTab, send, type NewTabState, type Shortcut } from "./protocol.js";
import { tokens } from "./tokens.js";

// The one place in the interface that genuinely grows under the pointer:
// scale up to 1.45, spreading to neighbours as a gaussian. Everything else
// uses a strip that only heats the control being pointed at.
const dockStrip = new Strip(1.45, true);

let tiles: HTMLElement[] = [];
let shortcuts: readonly Shortcut[] = [];
// Which shortcut the context menu was opened on. -1 for the add tile, which
// has no shortcut behind it.
let menuTarget = -1;

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
  // Ticks on the minute rather than every second: 59 of every 60 wake-ups
  // changed nothing, and this page is open in every new tab.
  const msToNextMinute = 60_000 - (Date.now() % 60_000);
  window.setTimeout(() => {
    renderClock();
    window.setInterval(renderClock, 60_000);
  }, msToNextMinute);
}

// ---------------------------------------------------------------------
// Dock
// ---------------------------------------------------------------------

function bounce(tile: HTMLElement): void {
  // Removed, forced to reflow, re-added: without the reflow the class change
  // is coalesced and the animation never restarts on a second click.
  tile.classList.remove("bounce");
  void tile.offsetWidth;
  tile.classList.add("bounce");
  tile.addEventListener("animationend", () => tile.classList.remove("bounce"), {
    once: true,
  });
}

function dockSlot(
  index: number,
  tip: string,
  tile: HTMLElement,
  onContextMenu: ((event: MouseEvent) => void) | null,
): HTMLElement {
  const slot = el("div", { className: "dock-slot", children: [tile] });
  // An attribute, not an element: one tooltip per slot as a pseudo-element
  // costs nothing, and the dock has no room for real nodes.
  slot.dataset.tip = tip;

  slot.addEventListener("pointerenter", () => {
    dockStrip.hover(index);
    scheduleFrame();
  });
  slot.addEventListener("pointerleave", () => {
    dockStrip.hover(null);
    scheduleFrame();
  });
  if (onContextMenu) {
    slot.addEventListener("contextmenu", (event) => {
      event.preventDefault();
      onContextMenu(event);
    });
  }
  return slot;
}

function shortcutTile(shortcut: Shortcut): HTMLElement {
  // The first letter, not a favicon. Fetching one tells a third party what
  // the user has pinned, and this browser makes no request the user did not
  // ask for -- see docs/security.md.
  const initial = [...shortcut.title][0]?.toUpperCase() ?? "?";

  const tile = el("div", { className: "dock-tile", text: initial });
  tile.setAttribute("role", "button");
  tile.tabIndex = 0;

  const activate = (event: MouseEvent | null): void => {
    bounce(tile);
    if (event) {
      spawnRipple(tile, event.clientX, event.clientY);
    }
    send("navigate", shortcut.url);
  };

  tile.addEventListener("click", activate);
  tile.addEventListener("keydown", (event: Event) => {
    const key = (event as KeyboardEvent).key;
    if (key === "Enter" || key === " ") {
      event.preventDefault();
      activate(null);
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
    byId("modal-title").textContent = "Add shortcut";
    byId<HTMLInputElement>("shortcut-name").value = "";
    byId<HTMLInputElement>("shortcut-url").value = "";
    byId("shortcut-confirm").textContent = "Add";
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
  shortcuts = state.shortcuts;
  byId("hint").textContent = state.hint;
  const dock = byId("dock");
  const slots: HTMLElement[] = [];
  tiles = [];

  state.shortcuts.forEach((shortcut, index) => {
    const tile = shortcutTile(shortcut);
    tiles.push(tile);
    slots.push(
      dockSlot(index, shortcut.title, tile, (event) => {
        menuTarget = index;
        menu.openAt(event.clientX, event.clientY);
      }),
    );
  });

  // The add tile is a slot like any other, so it heats and grows with the
  // rest instead of sitting still at the end of a moving row.
  const add = addTile();
  tiles.push(add);
  slots.push(dockSlot(state.shortcuts.length, "Add shortcut", add, null));

  dock.replaceChildren(...slots);
  scheduleFrame();
}

function renderTiles(now: number): void {
  tiles.forEach((tile, index) => {
    const presentation = dockStrip.presentation(index, now);
    if (tile.classList.contains("bounce")) {
      // The bounce keyframes own the transform while they run; writing one
      // here would fight them.
      tile.style.opacity = String(presentation.arrival);
      return;
    }
    // Lift proportional to growth: a tile that scales without rising looks
    // like it is being squashed into the dock.
    const lift = (presentation.scale - 1) * tokens.layoutDockLift;
    tile.style.transform = `scale(${presentation.scale}) translateY(${-lift}px)`;
    tile.style.opacity = String(presentation.arrival);
  });
}

// ---------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------

function wireSearch(): void {
  const form = byId("search-form");
  const bar = byId<HTMLInputElement>("search-bar");
  // No mark yet: it should be the configured engine's own logo, and those
  // are not in the repository. The field keeps its right padding so its
  // proportions do not change when one arrives.

  // autofocus fires a real focus event on load, exactly like a click. The
  // sweep should mark the user arriving, not the page opening, so the first
  // moment is ignored.
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
    // Forces a reflow so the animation restarts rather than being coalesced.
    void form.offsetWidth;
    form.classList.add("neon-play");
  });

  bar.addEventListener("blur", () => {
    form.classList.remove("focused", "neon-play", "neon-drift");
  });

  // One fast lap, then a slow endless drift: the arrival should be noticed
  // and the idle state should not.
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
      byId("modal-title").textContent = "Edit shortcut";
      byId<HTMLInputElement>("shortcut-name").value = shortcut.title;
      byId<HTMLInputElement>("shortcut-url").value = shortcut.url;
      byId("shortcut-confirm").textContent = "Save";
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

  register(dockStrip, () => tiles.length, renderTiles);
  onNewTab(renderDock);
}

runPage(main);
