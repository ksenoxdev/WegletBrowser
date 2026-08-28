// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// The popup opened by the toolbar's "keep" button. Its own floating
// widget, not a tab -- see WegletShortcutPopup -- so a fresh one opens
// (and this script runs fresh) on every click.

import { spawnRipple } from "./anim.js";
import { byId, runPage } from "./dom.js";
import { applyI18n } from "./i18n.js";
import { onShortcutPopup, send, type ShortcutPopupState } from "./protocol.js";
import { applyAccent, applyAddressBarShape } from "./theme.js";

function domainOf(url: string): string {
  try {
    return new URL(url).hostname;
  } catch {
    return url;
  }
}

// Kept per push rather than read back from the DOM, since the name
// field is meant to be edited before it is sent.
let targetUrl = "";

function render(state: ShortcutPopupState): void {
  applyI18n(state.language);
  applyAccent(state.accentColor);
  applyAddressBarShape(state.addressBarShape);
  targetUrl = state.url;
  const domain = domainOf(state.url);

  // First letter, not a favicon -- see docs/security.md.
  const source = state.suggestedName || domain;
  byId("favicon").textContent = [...source][0]?.toUpperCase() ?? "?";
  byId("domain").textContent = domain;

  const nameInput = byId<HTMLInputElement>("name-input");
  nameInput.value = state.suggestedName || domain;
  nameInput.focus();
  nameInput.select();
}

function hide(): void {
  send("hideSaveShortcutPopup");
}

function main(): void {
  const form = byId<HTMLFormElement>("form");
  const nameInput = byId<HTMLInputElement>("name-input");
  const cancel = byId<HTMLButtonElement>("cancel-btn");
  const save = byId<HTMLButtonElement>("save-btn");

  form.addEventListener("submit", (event) => {
    event.preventDefault();
    const name = nameInput.value.trim();
    if (!name) {
      nameInput.focus();
      return;
    }
    send("addShortcut", name, targetUrl);
    hide();
  });

  cancel.addEventListener("click", (event) => {
    spawnRipple(cancel, event.clientX, event.clientY);
    hide();
  });
  save.addEventListener("click", (event) => {
    spawnRipple(save, event.clientX, event.clientY);
  });

  // Losing focus or pressing Escape both mean "never mind" -- there is
  // no separate close button, matching a dropdown more than a dialog.
  window.addEventListener("blur", hide);
  window.addEventListener("keydown", (event: Event) => {
    if ((event as KeyboardEvent).key === "Escape") {
      hide();
    }
  });

  onShortcutPopup(render);
}

runPage(main);
