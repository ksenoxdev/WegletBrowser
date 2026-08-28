// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// The page shown when a navigation was stopped.

import { spawnRipple } from "./anim.js";
import {byId, runPage} from "./dom.js";
import { applyI18n } from "./i18n.js";
import { setIcon } from "./icons.js";
import { onRisk, send, type RiskNotice } from "./protocol.js";
import { applyAccent, applyAddressBarShape } from "./theme.js";

function render(notice: RiskNotice): void {
  applyI18n(notice.language);
  applyAccent(notice.accentColor);
  applyAddressBarShape(notice.addressBarShape);
  const isBlock = notice.level === "block";

  const icon = byId("icon");
  icon.className = isBlock ? "icon block" : "icon warning";
  setIcon(icon, "alert-triangle", 20);

  byId("title").textContent = notice.title;
  byId("reason").textContent = notice.reason;

  const detail = byId("detail-row");
  if (notice.host) {
    byId("detail-host").textContent = notice.host;
    detail.hidden = false;
  } else {
    detail.hidden = true;
  }

  // A hard block offers no way through. Hidden rather than disabled: a
  // greyed-out button invites going looking for the way round it.
  byId("proceed-btn").hidden = isBlock;

  // Focus lands on Go back, so Enter does the safe thing.
  byId<HTMLButtonElement>("dismiss-btn").focus();
}

function main(): void {
  const dismiss = byId<HTMLButtonElement>("dismiss-btn");
  const proceed = byId<HTMLButtonElement>("proceed-btn");

  dismiss.addEventListener("click", (event) => {
    send("securityDismiss");
    spawnRipple(dismiss, event.clientX, event.clientY);
  });
  proceed.addEventListener("click", (event) => {
    send("securityProceed");
    spawnRipple(proceed, event.clientX, event.clientY);
  });

  // Escape is dismiss, never proceed: the key someone presses to make a
  // dialog go away must not let the navigation through.
  window.addEventListener("keydown", (event: Event) => {
    if ((event as KeyboardEvent).key === "Escape") {
      send("securityDismiss");
    }
  });

  onRisk(render);
}

runPage(main);
