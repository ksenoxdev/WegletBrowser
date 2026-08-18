// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/ui/src/settings.ts

import { spawnRipple } from "./anim.js";
import {byId, el, runPage} from "./dom.js";
import {
  onSettings,
  send,
  type SearchEngineChoice,
  type SettingsState,
} from "./protocol.js";
import { accentPresets } from "./tokens.js";

// The default is the first preset, so "reset" and the first swatch cannot
// disagree -- they were separate literals before, and the presets themselves
// were a second copy of what tokens.json already held.
const DEFAULT_ACCENT = accentPresets[0];

// The panels, in the order the switcher shows them. One list rather than a
// query of the DOM, so the switcher's index arithmetic and the panels cannot
// end up in different orders.
const PANELS = ["search", "general", "security", "appearance"] as const;
type PanelName = (typeof PANELS)[number];


const SHAPES = ["pill", "rounded", "square"] as const;
type Shape = (typeof SHAPES)[number];

// ---------------------------------------------------------------------
// Switcher
// ---------------------------------------------------------------------

function showPanel(name: PanelName): void {
  const index = PANELS.indexOf(name);
  if (index < 0) {
    return;
  }

  // Custom properties rather than a computed pixel offset: the pill's width
  // is a fraction of the switcher, so a resized window moves it correctly
  // without anything having to listen for the resize.
  byId("switcher").style.setProperty("--count", String(PANELS.length));
  byId("switcher-pill").style.setProperty("--index", String(index));

  for (const panel of PANELS) {
    const tab = byId(`tab-${panel}`);
    const active = panel === name;
    tab.classList.toggle("active", active);
    tab.setAttribute("aria-selected", String(active));
  }
  for (const node of document.querySelectorAll<HTMLElement>("[role='tabpanel']")) {
    node.hidden = node.dataset.panel !== name;
  }
}

function wireSwitcher(): void {
  PANELS.forEach((panel, index) => {
    const tab = byId(`tab-${panel}`);
    tab.addEventListener("click", () => showPanel(panel));
    tab.addEventListener("keydown", (event: Event) => {
      const key = (event as KeyboardEvent).key;
      if (key === "Enter" || key === " ") {
        event.preventDefault();
        showPanel(panel);
        return;
      }
      // Arrow keys move between tabs, which is what a tablist is expected to
      // do -- and the only way to reach a panel without a pointer once focus
      // is inside the strip.
      const delta = key === "ArrowRight" ? 1 : key === "ArrowLeft" ? -1 : 0;
      if (delta === 0) {
        return;
      }
      event.preventDefault();
      const next = PANELS[(index + delta + PANELS.length) % PANELS.length];
      if (next) {
        showPanel(next);
        byId(`tab-${next}`).focus();
      }
    });
  });
  showPanel("search");
}

// ---------------------------------------------------------------------
// Controls
// ---------------------------------------------------------------------

function renderEngines(
  engines: readonly SearchEngineChoice[],
  current: string,
): void {
  const row = byId("engine-choice");
  row.replaceChildren(
    ...engines.map((engine) => {
      const pill = el("button", {
        className: engine.id === current ? "engine-pill active" : "engine-pill",
        text: engine.label,
        onClick: () => send("setSearchEngine", engine.id),
      });
      pill.setAttribute("aria-pressed", String(engine.id === current));
      return pill;
    }),
  );
}

function toggle(id: string, on: boolean, onChange: (next: boolean) => void): void {
  const node = byId(id);
  node.classList.toggle("on", on);
  node.setAttribute("aria-checked", String(on));
  // Replaced rather than added: render runs on every push, and adding would
  // stack a listener per update until one click sent a dozen messages.
  const clone = node.cloneNode(true) as HTMLElement;
  node.replaceWith(clone);
  clone.addEventListener("click", () => onChange(!on));
  clone.addEventListener("keydown", (event: Event) => {
    const key = (event as KeyboardEvent).key;
    if (key === "Enter" || key === " ") {
      event.preventDefault();
      onChange(!on);
    }
  });
}

function renderBlockedHosts(hosts: readonly string[]): void {
  const list = byId("blocked-hosts-list");
  if (hosts.length === 0) {
    list.replaceChildren(
      el("div", { className: "blocked-hosts-empty", text: "No sites blocked." }),
    );
    return;
  }
  list.replaceChildren(
    ...hosts.map((host, index) => {
      const remove = el("button", {
        className: "blocked-host-remove",
        text: "Remove",
        // By index, which is how the profile stores the list. The browser
        // bounds-checks it, so a stale index is a dropped message rather than
        // the wrong host being unblocked.
        onClick: () => send("unblockHost", index),
      });
      remove.setAttribute("aria-label", `Unblock ${host}`);
      return el("div", {
        className: "blocked-host-row",
        children: [el("span", { text: host }), remove],
      });
    }),
  );
}

function isValidHex(value: string): boolean {
  return /^#[0-9a-fA-F]{6}$/.test(value);
}

function renderAccent(accent: string): void {
  const row = byId("color-preset-row");
  row.replaceChildren(
    ...accentPresets.map((preset) => {
      const swatch = el("button", {
        className:
          preset.toLowerCase() === accent.toLowerCase()
            ? "color-preset active"
            : "color-preset",
        onClick: () => send("setAccentColor", preset),
      });
      swatch.style.background = preset;
      swatch.setAttribute("aria-label", `Accent ${preset}`);
      return swatch;
    }),
  );

  byId("hex-swatch").style.background = accent;
  const hex = byId<HTMLInputElement>("hex-input");
  // Not while the user is mid-word: overwriting the field they are typing in
  // is the same mistake as repainting the address bar.
  if (document.activeElement !== hex) {
    hex.value = accent;
  }
}

function renderShape(shape: Shape): void {
  for (const node of document.querySelectorAll<HTMLElement>("[data-shape]")) {
    const active = node.dataset.shape === shape;
    node.classList.toggle("active", active);
    node.setAttribute("aria-pressed", String(active));
  }
  // Applied to this page too, so the preview is the real thing rather than a
  // description of it.
  document.documentElement.dataset.addressShape = shape;
}

// ---------------------------------------------------------------------

function render(state: SettingsState): void {
  renderEngines(state.engines, state.searchEngine);
  byId("custom-engine-row").hidden = state.searchEngine !== "custom";
  const custom = byId<HTMLInputElement>("custom-engine-input");
  if (document.activeElement !== custom) {
    custom.value = state.customSearchUrl;
  }

  toggle("restore-session-toggle", state.restoreSession, (next) =>
    send("setRestoreSession", next),
  );
  // The phishing toggle and its refresh button have no profile field behind
  // them yet, so the panel shows them without state. Left visible rather
  // than removed: the panel's shape should not change when they start
  // working.

  renderBlockedHosts(state.blockedHosts);
  renderAccent(state.accentColor);
  renderShape(state.addressBarShape);
}

function confirmingButton(id: string, action: () => void): void {
  const node = byId<HTMLButtonElement>(id);
  const original = node.textContent ?? "";
  let armed = false;
  let timer = 0;

  node.addEventListener("click", (event) => {
    spawnRipple(node, event.clientX, event.clientY);
    if (!armed) {
      // Two clicks, not a dialog: the action is destructive but not
      // catastrophic, and a modal for it would be in the way every time.
      armed = true;
      node.textContent = "Click again to confirm";
      timer = window.setTimeout(() => {
        armed = false;
        node.textContent = original;
      }, 3000);
      return;
    }
    window.clearTimeout(timer);
    armed = false;
    node.textContent = original;
    action();
  });
}

function main(): void {
  wireSwitcher();

  const done = byId<HTMLButtonElement>("done");
  done.addEventListener("click", (event) => {
    send("goBack");
    spawnRipple(done, event.clientX, event.clientY);
  });

  byId("custom-engine-input").addEventListener("change", () => {
    send("setCustomSearchUrl", byId<HTMLInputElement>("custom-engine-input").value.trim());
  });

  const blockInput = byId<HTMLInputElement>("block-host-input");
  const blockHost = (): void => {
    const host = blockInput.value.trim();
    if (host) {
      send("blockHost", host);
      blockInput.value = "";
    }
  };
  byId("block-host-btn").addEventListener("click", blockHost);
  blockInput.addEventListener("keydown", (event: Event) => {
    if ((event as KeyboardEvent).key === "Enter") {
      blockHost();
    }
  });

  const hex = byId<HTMLInputElement>("hex-input");
  hex.addEventListener("change", () => {
    const value = hex.value.trim();
    if (isValidHex(value)) {
      send("setAccentColor", value);
    } else {
      // Reverted rather than left as typed: an invalid value that stays in
      // the field looks like it was accepted.
      send("requestState");
    }
  });

  byId("reset-color-btn").addEventListener("click", (event) => {
    send("setAccentColor", DEFAULT_ACCENT);
    spawnRipple(byId("reset-color-btn"), event.clientX, event.clientY);
  });

  for (const node of document.querySelectorAll<HTMLElement>("[data-shape]")) {
    node.addEventListener("click", (event) => {
      const shape = node.dataset.shape;
      if (shape) {
        send("setAddressBarShape", shape);
      }
      spawnRipple(node, (event as MouseEvent).clientX, (event as MouseEvent).clientY);
    });
  }

  const refresh = byId<HTMLButtonElement>("refresh-threat-feed-btn");
  refresh.addEventListener("click", (event) => {
    send("refreshThreatFeed");
    refresh.disabled = true;
    refresh.textContent = "Refreshing...";
    spawnRipple(refresh, event.clientX, event.clientY);
  });

  confirmingButton("clear-data-btn", () => send("clearBrowsingData"));

  onSettings(render);
}

runPage(main);
