// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/ui/src/toolbar.ts

import {
  attachTooltip,
  mixChannel,
  mixRgb,
  register,
  scheduleFrame,
  spawnRipple,
  Strip,
} from "./anim.js";
import {byId, el, runPage} from "./dom.js";
import { setIcon, type IconName } from "./icons.js";
import { onState, pageUrl, pages, send, type BrowserState, type TabState } from "./protocol.js";
import { channels, tokens } from "./tokens.js";

// Channel values come from the generator, not from here: they are the same
// colours as tokens.css, and a triplet written out by hand is the same
// number in two places. The hover tint in particular is a color-mix that
// nobody can recompute by eye -- it was wrong here before the generator
// produced it.
const {
  textDim: TEXT_DIM,
  text: TEXT,
  surfaceRaised: SURFACE_RAISED,
  border: BORDER,
  borderStrong: BORDER_STRONG,
  surfaceHover: SURFACE_HOVER,
  danger: DANGER,
  background: BACKGROUND,
} = channels;

function textAt(heat: number): string {
  const r = mixChannel(TEXT_DIM[0], TEXT[0], heat);
  const g = mixChannel(TEXT_DIM[1], TEXT[1], heat);
  const b = mixChannel(TEXT_DIM[2], TEXT[2], heat);
  return `rgb(${r},${g},${b})`;
}

function rgba(colour: readonly [number, number, number], alpha: number): string {
  return `rgba(${colour[0]},${colour[1]},${colour[2]},${alpha})`;
}

// ---------------------------------------------------------------------
// Toolbar buttons
// ---------------------------------------------------------------------

interface Button {
  readonly el: HTMLElement;
  // Disabled buttons take no heat at all: a control that lights up and then
  // does nothing is worse than one that stays dim.
  disabled: boolean;
}

const buttons: Button[] = [];
const buttonStrip = new Strip(1.08, false);

function navButton(
  id: string,
  iconName: IconName,
  tooltipText: string,
  onActivate: (event: MouseEvent) => void,
): Button {
  const node = byId(id);
  setIcon(node, iconName);
  attachTooltip(node, tooltipText);

  const button: Button = { el: node, disabled: false };
  const index = buttons.length;
  buttons.push(button);

  node.addEventListener("pointerenter", () => {
    if (!button.disabled) {
      buttonStrip.hover(index);
      scheduleFrame();
    }
  });
  node.addEventListener("pointerleave", () => {
    buttonStrip.hover(null);
    scheduleFrame();
  });
  node.addEventListener("click", (event) => {
    if (button.disabled) {
      return;
    }
    onActivate(event);
    spawnRipple(node, event.clientX, event.clientY);
  });
  // A div standing in for a button has to answer the keyboard like one.
  node.addEventListener("keydown", (event: Event) => {
    const key = (event as KeyboardEvent).key;
    if (!button.disabled && (key === "Enter" || key === " ")) {
      event.preventDefault();
      node.click();
    }
  });
  return button;
}

function setDisabled(button: Button, disabled: boolean): void {
  button.disabled = disabled;
  button.el.classList.toggle("disabled", disabled);
  if (disabled) {
    // Cleared, or the button keeps whatever inline colours it had when it
    // went dim and the .disabled class loses to them.
    button.el.style.backgroundColor = "";
    button.el.style.borderColor = "";
    button.el.style.color = "";
    button.el.style.transform = "";
  }
}

function renderButtons(now: number): void {
  buttons.forEach((button, index) => {
    if (button.disabled) {
      return;
    }
    const presentation = buttonStrip.presentation(index, now);
    button.el.style.backgroundColor = rgba(SURFACE_HOVER, presentation.heat);
    button.el.style.borderColor = rgba(BORDER_STRONG, presentation.heat);
    button.el.style.color = textAt(presentation.heat);
    button.el.style.transform = `scale(${presentation.scale})`;
  });
}

// ---------------------------------------------------------------------
// Window controls
// ---------------------------------------------------------------------

const windowButtons: { el: HTMLElement; destructive: boolean }[] = [];
const windowStrip = new Strip(1, false);

function windowButton(id: string, iconName: IconName, destructive: boolean): void {
  const node = byId(id);
  setIcon(node, iconName, 14);

  const index = windowButtons.length;
  windowButtons.push({ el: node, destructive });

  node.addEventListener("pointerenter", () => {
    windowStrip.hover(index);
    scheduleFrame();
  });
  node.addEventListener("pointerleave", () => {
    windowStrip.hover(null);
    scheduleFrame();
  });
}

function renderWindowButtons(now: number): void {
  windowButtons.forEach((button, index) => {
    const presentation = windowStrip.presentation(index, now);
    // Close tints towards a muted red rather than the usual purple: it is
    // the one control in the window whose result cannot be undone.
    const lit = button.destructive ? mixRgb(DANGER, BACKGROUND, 0.45) : SURFACE_HOVER;
    button.el.style.backgroundColor = rgba(lit, presentation.heat);
    button.el.style.color = textAt(presentation.heat);
  });
}

// ---------------------------------------------------------------------
// Tabs
// ---------------------------------------------------------------------

const tabStrip = new Strip(1.02, false);
let tabElements: HTMLElement[] = [];
let activeIndex = -1;

// Only settings has a page of its own right now; history and bookmarks both
// resolve to the new tab page until they get one -- see contract.json's
// internal_addresses and WegletWindow::ResolveForEngine, which are the
// actual source of truth for what a tab's URL will be. A mark keyed on
// "history.html" would never match, because that page does not exist.
const SETTINGS_URL = pageUrl(pages.settings);

function faviconFor(tab: TabState): HTMLElement {
  // .loading drives the spinner in toolbar.css. The model has tracked
  // this all along; it just had no way across the boundary until the tab
  // state carried it.
  const slot = el("span", {
    className: tab.loading ? "tab-favicon loading" : "tab-favicon",
  });
  // Weglet's own pages get their own mark. Everything else gets nothing yet:
  // a favicon has to come from somewhere, and asking a third party for one
  // tells them what the user has open -- see docs/security.md.
  if (tab.url === SETTINGS_URL) {
    const mark = el("span", { className: "tab-favicon-mark" });
    setIcon(mark, "settings", 14);
    slot.appendChild(mark);
  }
  return slot;
}

function tabPill(tab: TabState, index: number, active: boolean): HTMLElement {
  const close = el("span", { className: "tab-close", text: "\u00d7" });
  close.setAttribute("aria-label", `Close ${tab.label}`);
  // pointerdown as well as click: pressing close must not also start
  // whatever the pill does with a press.
  close.addEventListener("pointerdown", (event) => event.stopPropagation());
  close.addEventListener("click", (event) => {
    event.stopPropagation();
    send("closeTab", tab.id);
  });

  const pill = el("div", {
    className: active ? "tab-pill active" : "tab-pill",
    children: [
      faviconFor(tab),
      el("span", { className: "tab-title", text: tab.label }),
      close,
    ],
  });
  pill.setAttribute("role", "tab");
  pill.setAttribute("aria-selected", String(active));
  pill.tabIndex = 0;
  attachTooltip(pill, tab.url || tab.label);

  pill.addEventListener("click", () => send("activateTab", tab.id));
  pill.addEventListener("keydown", (event: Event) => {
    const key = (event as KeyboardEvent).key;
    if (key === "Enter" || key === " ") {
      event.preventDefault();
      send("activateTab", tab.id);
    }
  });
  pill.addEventListener("pointerenter", () => {
    tabStrip.hover(index);
    scheduleFrame();
  });
  pill.addEventListener("pointerleave", () => {
    tabStrip.hover(null);
    scheduleFrame();
  });
  return pill;
}

function renderTabs(now: number): void {
  tabElements.forEach((pill, index) => {
    const presentation = tabStrip.presentation(index, now);
    // A tab grows into place from just over half height rather than popping
    // in at full size.
    pill.style.height = `${tokens.layoutTabHeight * (0.55 + 0.45 * presentation.arrival)}px`;
    pill.style.transform = `scale(${presentation.scale})`;

    if (index === activeIndex) {
      // The active pill's own CSS carries its look; inline values would
      // outrank it and a stale hover tint would survive selection.
      pill.style.backgroundColor = "";
      pill.style.borderColor = "";
      pill.style.color = "";
      return;
    }
    // Damped to 45%: a hovered inactive tab should hint, not compete with
    // the selected one.
    pill.style.backgroundColor = rgba(SURFACE_RAISED, presentation.heat * 0.45);
    pill.style.borderColor = rgba(BORDER, presentation.heat);
    pill.style.color = textAt(presentation.heat);
  });
}

// ---------------------------------------------------------------------

function isEditingAddress(): boolean {
  return document.activeElement === byId("address-bar");
}

function render(state: BrowserState): void {
  const strip = byId("tabstrip");
  const newTab = byId("new-tab");

  // Rebuilt rather than diffed. The strip is at most a hundred pills of
  // three nodes each, and the heat carried in the Strip survives a rebuild
  // by index -- so hovering a tab while it repaints does not flicker.
  for (const old of strip.querySelectorAll(".tab-pill")) {
    old.remove();
  }
  tabElements = [];
  activeIndex = state.tabs.findIndex((tab) => tab.id === state.activeId);

  state.tabs.forEach((tab, index) => {
    const pill = tabPill(tab, index, tab.id === state.activeId);
    strip.insertBefore(pill, newTab);
    tabElements.push(pill);
  });

  const active = activeIndex >= 0 ? state.tabs[activeIndex] : undefined;

  const address = byId<HTMLInputElement>("address-bar");
  if (!isEditingAddress()) {
    // Blank for the new tab page: showing its internal URL tells the user
    // nothing and invites them to edit it.
    const url = active?.url ?? "";
    address.value = url.startsWith("chrome://weglet/") ? "" : url;
  }

  // Ctrl+L. The keystroke reaches the browser process, which focuses this
  // page and sets this for exactly one push -- focus lives in the DOM, so
  // putting the caret here is the only part it cannot do itself.
  //
  // After the value above, so selecting selects what is actually shown.
  if (state.focusOmnibox) {
    address.focus();
    address.select();
  }

  setDisabled(back, !active?.canGoBack);
  setDisabled(forward, !active?.canGoForward);

  scheduleFrame();
}

const back = navButton("go-back", "arrow-left", "Back", () => send("goBack"));
const forward = navButton("go-forward", "arrow-right", "Forward", () =>
  send("goForward"),
);
navButton("reload", "refresh", "Reload", () => send("reload"));
navButton("bookmark-toggle", "bookmark", "Bookmark this page", () => {
  // Bookmarks live in the profile, and the page that manages them is still
  // to come. Wired up but inert rather than absent, so the toolbar's shape
  // does not change when it starts working.
});
navButton("copy-url", "link", "Copy address", () => {
  const url = byId<HTMLInputElement>("address-bar").value;
  if (url) {
    void navigator.clipboard.writeText(url);
  }
});
navButton("downloads-button", "download", "Downloads", () => {});
navButton("open-settings", "settings", "Settings", () => send("openSettings"));

function main(): void {
  const newTab = byId("new-tab");
  setIcon(newTab, "plus");
  attachTooltip(newTab, "New tab");
  newTab.addEventListener("click", (event) => {
    send("newTab");
    spawnRipple(newTab, event.clientX, event.clientY);
  });

  windowButton("win-minimize", "minus", false);
  windowButton("win-maximize", "square", false);
  windowButton("win-close", "x", true);

  const address = byId<HTMLInputElement>("address-bar");
  address.addEventListener("keydown", (event: Event) => {
    const key = (event as KeyboardEvent).key;
    if (key === "Enter") {
      const text = address.value.trim();
      if (text) {
        send("navigate", text);
        // Focus moves to the page. Leaving it here sends the next keystroke
        // to the wrong place and also stops render from ever updating the
        // field again.
        address.blur();
      }
    } else if (key === "Escape") {
      address.blur();
      send("requestState");
    }
  });

  register(buttonStrip, () => buttons.length, renderButtons);
  register(windowStrip, () => windowButtons.length, renderWindowButtons);
  register(tabStrip, () => tabElements.length, renderTabs);

  onState(render);
}

runPage(main);
