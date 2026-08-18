// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/ui/src/security_notice.ts

import { spawnRipple } from "./anim.js";
import {byId, runPage} from "./dom.js";
import { setIcon } from "./icons.js";
import { onRisk, send, type RiskNotice } from "./protocol.js";

function render(notice: RiskNotice): void {
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
  // greyed-out button invites the user to go looking for the way round it.
  byId("proceed-btn").hidden = isBlock;

  // Focus lands on Go back, so Enter does the safe thing. Proceeding has to
  // be a deliberate click or a deliberate Tab.
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
  // dialog go away must not be the one that lets the navigation through.
  window.addEventListener("keydown", (event: Event) => {
    if ((event as KeyboardEvent).key === "Escape") {
      send("securityDismiss");
    }
  });

  onRisk(render);
}

runPage(main);
