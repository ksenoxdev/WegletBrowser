// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// Ctrl+F's floating bar. Its own floating widget, not a tab -- see
// WegletFindBar -- but unlike the menu/context-menu popups it is never
// reloaded while open, so the query the user typed survives a re-open.

import { byId, runPage } from "./dom.js";
import { applyI18n } from "./i18n.js";
import { setIcon } from "./icons.js";
import { onFindBar, send, type FindBarState } from "./protocol.js";
import { applyAccent, applyAddressBarShape } from "./theme.js";

function main(): void {
  const input = byId<HTMLInputElement>("find-input");
  const count = byId("find-count");
  const previous = byId<HTMLButtonElement>("find-previous");
  const next = byId<HTMLButtonElement>("find-next");
  const close = byId<HTMLButtonElement>("find-close");

  setIcon(previous, "arrow-left", 15);
  setIcon(next, "arrow-right", 15);
  setIcon(close, "x", 14);

  input.addEventListener("input", () => send("findInPage", input.value));
  input.addEventListener("keydown", (event: Event) => {
    const key = (event as KeyboardEvent).key;
    if (key === "Enter") {
      event.preventDefault();
      send((event as KeyboardEvent).shiftKey ? "findPrevious" : "findNext");
    } else if (key === "Escape") {
      send("closeFindBar");
    }
  });
  previous.addEventListener("click", () => send("findPrevious"));
  next.addEventListener("click", () => send("findNext"));
  close.addEventListener("click", () => send("closeFindBar"));

  // The window regaining focus is what a re-open (a second Ctrl+F) looks
  // like from here, since the popup is never reloaded -- see WegletFindBar.
  window.addEventListener("focus", () => {
    input.focus();
    input.select();
  });

  onFindBar((state: FindBarState) => {
    applyI18n(state.language);
    applyAccent(state.accentColor);
    applyAddressBarShape(state.addressBarShape);
    count.textContent = state.query ? `${state.activeOrdinal}/${state.matchCount}` : "";
  });
}

runPage(main);
