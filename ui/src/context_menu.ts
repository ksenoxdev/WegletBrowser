// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// The dropdown a right click opens. Its own floating widget, not a tab --
// see WegletContextMenu -- so a fresh one opens (and this script runs
// fresh) on every click, with whatever items that click's target allows.

import { byId, el, runPage } from "./dom.js";
import { applyI18n, t } from "./i18n.js";
import { onContextMenu, send, type ContextMenuItem, type ContextMenuState } from "./protocol.js";
import { applyAccent, applyAddressBarShape } from "./theme.js";

function hide(): void {
  send("hideContextMenu");
}

function renderItem(item: ContextMenuItem, language: ContextMenuState["language"]): HTMLElement {
  const button = el("button", {
    className: "menu-item",
    text: item.label ?? t(`contextMenu.${item.id}`, language),
  });
  button.disabled = !item.enabled;
  button.addEventListener("click", () => {
    send("contextMenuAction", item.id);
  });
  return button;
}

function main(): void {
  const items = byId("items");

  window.addEventListener("blur", hide);
  window.addEventListener("keydown", (event: Event) => {
    if ((event as KeyboardEvent).key === "Escape") {
      hide();
    }
  });

  onContextMenu((state: ContextMenuState) => {
    applyI18n(state.language);
    applyAccent(state.accentColor);
    applyAddressBarShape(state.addressBarShape);
    items.replaceChildren(...state.items.map((item) => renderItem(item, state.language)));
  });
}

runPage(main);
