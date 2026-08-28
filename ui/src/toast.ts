// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// The small stack of dismissable notices under the address bar (link
// copied, bookmark added/removed, ...). Its own floating popup, not DOM
// inside the toolbar's own page -- see WegletToastPopup -- reloaded on
// every show like the context menu, so each notice starts its own
// dismiss timer (and entrance animation) fresh.

import { byId, el, runPage } from "./dom.js";
import { applyI18n, t } from "./i18n.js";
import { onToast, send, type ToastItem, type ToastState } from "./protocol.js";
import { applyAccent, applyAddressBarShape } from "./theme.js";

const AUTO_DISMISS_MS = 2700;
// Matches toast.css's transition duration -- long enough to see the
// notice leave before the popup reloads out from under it.
const EXIT_MS = 160;

// Plays the CSS exit transition (see .toast-item.shown in toast.css)
// before actually asking the browser to drop the notice.
function dismiss(id: number, row: HTMLElement): void {
  row.classList.remove("shown");
  window.setTimeout(() => send("dismissToast", id), EXIT_MS);
}

function renderItem(item: ToastItem, language: ToastState["language"]): HTMLElement {
  let row: HTMLElement;
  const close = el("div", {
    className: "toast-close",
    text: "×",
    title: t("toast.dismiss", language),
    onClick: () => dismiss(item.id, row),
  });

  row = el("div", {
    className: "toast-item",
    children: [
      el("span", { className: "toast-text", text: t(item.key, language) }),
      close,
    ],
  });

  // Starts off-screen (see toast.css); added on the next frame so the
  // transition from that starting state actually plays.
  requestAnimationFrame(() => row.classList.add("shown"));
  setTimeout(() => dismiss(item.id, row), AUTO_DISMISS_MS);

  return row;
}

function main(): void {
  const list = byId("toast-list");

  onToast((state: ToastState) => {
    applyI18n(state.language);
    applyAccent(state.accentColor);
    applyAddressBarShape(state.addressBarShape);
    list.replaceChildren(...state.items.map((item) => renderItem(item, state.language)));
  });
}

runPage(main);
