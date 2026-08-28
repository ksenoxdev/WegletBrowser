// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// The dropdown opened by the toolbar's site-info button. Its own
// floating widget, not a tab -- see WegletSiteInfoPopup -- so a fresh
// one opens (and this script runs fresh) on every toggle, and on every
// permission change made from it.

import { byId, el, runPage } from "./dom.js";
import { applyI18n, t, type Language } from "./i18n.js";
import { setIcon } from "./icons.js";
import { onSiteInfo, send, type SiteInfoState, type SitePermission } from "./protocol.js";
import { applyAccent, applyAddressBarShape } from "./theme.js";

function permissionRow(permission: SitePermission, language: Language): HTMLElement {
  const granted = permission.status === "granted";
  const knob = el("div", { className: "toggle" });
  knob.classList.toggle("on", granted);
  knob.setAttribute("role", "switch");
  knob.tabIndex = 0;
  knob.setAttribute("aria-checked", String(granted));
  const flip = (): void => send("setPermissionDecision", permission.id, !granted);
  knob.addEventListener("click", flip);
  knob.addEventListener("keydown", (event: Event) => {
    const key = (event as KeyboardEvent).key;
    if (key === "Enter" || key === " ") {
      event.preventDefault();
      flip();
    }
  });

  return el("li", {
    className: "permission-row",
    children: [
      el("span", { className: "permission-label", text: t(`siteInfo.${permission.id}`, language) }),
      knob,
    ],
  });
}

function main(): void {
  setIcon(byId("shield-icon"), "shield-check", 22, true);

  const internalView = byId("internal-view");
  const externalView = byId("external-view");
  const origin = byId("origin");
  const permissions = byId("permissions");

  window.addEventListener("blur", () => send("hideSiteInfo"));
  window.addEventListener("keydown", (event: Event) => {
    if ((event as KeyboardEvent).key === "Escape") {
      send("hideSiteInfo");
    }
  });

  onSiteInfo((state: SiteInfoState) => {
    applyI18n(state.language);
    applyAccent(state.accentColor);
    applyAddressBarShape(state.addressBarShape);

    internalView.hidden = !state.isInternal;
    externalView.hidden = state.isInternal;
    if (state.isInternal) {
      return;
    }
    origin.textContent = state.origin;
    permissions.replaceChildren(
      ...state.permissions.map((permission) => permissionRow(permission, state.language)),
    );
  });
}

runPage(main);
