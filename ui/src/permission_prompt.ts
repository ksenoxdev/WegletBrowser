// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// The camera/mic/location/notifications popup. Its own floating widget,
// not a tab -- see WegletPermissionPrompt -- so a fresh one opens (and
// this script runs fresh) on every request.

import { byId, el, runPage } from "./dom.js";
import { applyI18n, t } from "./i18n.js";
import { onPermissionPrompt, send, type PermissionPromptState } from "./protocol.js";
import { applyAccent, applyAddressBarShape } from "./theme.js";

function main(): void {
  const origin = byId("origin");
  const types = byId("types");
  const block = byId<HTMLButtonElement>("block");
  const allow = byId<HTMLButtonElement>("allow");

  block.addEventListener("click", () => send("answerPermissionPrompt", false));
  allow.addEventListener("click", () => send("answerPermissionPrompt", true));
  window.addEventListener("keydown", (event: Event) => {
    if ((event as KeyboardEvent).key === "Escape") {
      send("answerPermissionPrompt", false);
    }
  });

  onPermissionPrompt((state: PermissionPromptState) => {
    applyI18n(state.language);
    applyAccent(state.accentColor);
    applyAddressBarShape(state.addressBarShape);
    origin.textContent = state.origin;
    types.replaceChildren(
      ...state.types.map((type) =>
        el("li", { text: t(`permissionPrompt.${type}`, state.language) }),
      ),
    );
  });
}

runPage(main);
