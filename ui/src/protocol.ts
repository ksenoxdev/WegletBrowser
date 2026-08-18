// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/ui/src/protocol.ts
//
// Every message that crosses between a page and the browser, typed once.
//
// This file is the contract. The C++ side mirrors it by hand -- see
// weglet/browser/weglet_message_handler.cc -- so a change here is a change
// there in the same commit. On this side at least, a typo in a message name
// is a compile error instead of a silently ignored string.
//
// The channel itself is content's WebUI bindings. They exist only in a frame
// the WebUI factory claimed, which is why ordinary web content cannot reach
// any of this: there is no allow-list here to get wrong.

// Generated from weglet/ui/contract.json -- the message names and the page
// paths live there, so the browser's registration and this type cannot go
// out of step. See generate_contract.py.
export type { Outgoing, TabId } from "./contract.js";
export { pages, pageUrl, pushes } from "./contract.js";

import type { Outgoing, TabId } from "./contract.js";
import { pushes, type PushName } from "./contract.js";

export interface TabState {
  readonly id: TabId;
  readonly url: string;
  // The page title, or its host until a title arrives, or a name for one of
  // Weglet's own pages. Decided by the model, not here.
  readonly label: string;
  readonly canGoBack: boolean;
  readonly canGoForward: boolean;
  readonly loading: boolean;
}

export interface BrowserState {
  readonly tabs: readonly TabState[];
  readonly activeId: TabId;
  // Set for exactly one push, in answer to Ctrl+L. Focus lives in the
  // DOM, so the browser can bring the toolbar forward but not put the
  // caret in the address bar; this is how it asks.
  readonly focusOmnibox: boolean;
}

export interface SearchEngineChoice {
  readonly id: string;
  readonly label: string;
}

export interface SettingsState {
  // Sent by the browser rather than listed here: the engines are a Rust enum,
  // and a list on this side would be a second copy of it.
  readonly engines: readonly SearchEngineChoice[];
  readonly searchEngine: string;
  readonly customSearchUrl: string;
  readonly restoreSession: boolean;
  readonly blockedHosts: readonly string[];
  readonly accentColor: string;
  readonly addressBarShape: "pill" | "rounded" | "square";
}

export interface RiskNotice {
  readonly level: "warning" | "block";
  readonly title: string;
  readonly reason: string;
  readonly host: string;
  readonly target: string;
}

export interface Shortcut {
  readonly title: string;
  readonly url: string;
}

export interface NewTabState {
  readonly shortcuts: readonly Shortcut[];
  // A line under the search field: which engine will answer, and anything
  // else worth saying once. Composed by the browser, because the engine is a
  // profile setting and the page does not read the profile.
  readonly hint: string;
}

// What a page may ask the browser to do. The browser registers exactly these
// names; anything else is dropped with a log line.
interface ChromeSend {
  send(message: string, args?: unknown[]): void;
}

// Installed by the WebUI bindings before any page script runs. Absent when a
// page is opened outside Weglet, so this is a no-op rather than a crash.
function bridge(): ChromeSend | undefined {
  return (globalThis as { chrome?: ChromeSend }).chrome;
}

export function send(...message: Outgoing): void {
  const [name, ...args] = message;
  bridge()?.send(name, args);
}

// The browser pushes state by calling the function the contract names for
// this page. Registering the handler installs that global -- there is no addEventListener here, because
// WebUI calls a named function rather than posting a message.
export function onState(handler: (state: BrowserState) => void): void {
  install(pushes.state, (value: unknown) => {
    const parsed = parseState(value);
    if (parsed) {
      handler(parsed);
    }
  });
  // The browser only pushes on a change, so a page that loaded late has to
  // ask for the current state itself.
  send("requestState");
}

// WebUI calls a named function rather than posting a message, so receiving
// means installing one on a shared global. Merged rather than replaced: a
// page may listen for more than one kind of push.
type Receiver = (value: unknown) => void;

function install(name: PushName, receiver: Receiver): void {
  const host = globalThis as { weglet?: Record<string, Receiver> };
  host.weglet = { ...host.weglet, [name]: receiver };
}

// The browser pushes the new tab page's own state by its own function
// name. Separate from the toolbar's because the two pages want
// different things and neither should have to ignore the other's.
export function onNewTab(handler: (state: NewTabState) => void): void {
  install(pushes.newTab, (value: unknown) => {
    const parsed = parseNewTab(value);
    if (parsed) {
      handler(parsed);
    }
  });
  send("requestState");
}

function parseNewTab(value: unknown): NewTabState | null {
  if (typeof value !== "object" || value === null) {
    return null;
  }
  const candidate = value as { shortcuts?: unknown; hint?: unknown };
  if (!Array.isArray(candidate.shortcuts) || typeof candidate.hint !== "string") {
    console.warn("weglet: malformed new tab state", value);
    return null;
  }
  const shortcuts: Shortcut[] = [];
  for (const entry of candidate.shortcuts) {
    if (typeof entry !== "object" || entry === null) {
      return null;
    }
    const shortcut = entry as Record<string, unknown>;
    if (typeof shortcut.title !== "string" || typeof shortcut.url !== "string") {
      console.warn("weglet: malformed shortcut", entry);
      return null;
    }
    shortcuts.push({ title: shortcut.title, url: shortcut.url });
  }
  return { shortcuts, hint: candidate.hint };
}

// The settings page's own push.
export function onSettings(handler: (state: SettingsState) => void): void {
  install(pushes.settings, (value: unknown) => {
    const parsed = parseSettings(value);
    if (parsed) {
      handler(parsed);
    }
  });
  send("requestState");
}

function parseSettings(value: unknown): SettingsState | null {
  if (typeof value !== "object" || value === null) {
    return null;
  }
  const s = value as Record<string, unknown>;
  // The shape drives a CSS attribute, so an unrecognised one is dropped
  // rather than written through -- an attribute nobody styles would leave the
  // address bar with no shape at all.
  if (
    s.addressBarShape !== "pill" &&
    s.addressBarShape !== "rounded" &&
    s.addressBarShape !== "square"
  ) {
    console.warn("weglet: malformed address bar shape", value);
    return null;
  }
  if (!Array.isArray(s.engines)) {
    console.warn("weglet: settings without an engine list", value);
    return null;
  }
  const engines: SearchEngineChoice[] = [];
  for (const entry of s.engines) {
    if (
      typeof entry !== "object" ||
      entry === null ||
      typeof (entry as SearchEngineChoice).id !== "string" ||
      typeof (entry as SearchEngineChoice).label !== "string"
    ) {
      console.warn("weglet: malformed engine", entry);
      return null;
    }
    engines.push({
      id: (entry as SearchEngineChoice).id,
      label: (entry as SearchEngineChoice).label,
    });
  }
  if (
    typeof s.searchEngine !== "string" ||
    typeof s.customSearchUrl !== "string" ||
    typeof s.restoreSession !== "boolean" ||
    typeof s.accentColor !== "string" ||
    !Array.isArray(s.blockedHosts)
  ) {
    console.warn("weglet: malformed settings", value);
    return null;
  }
  return {
    engines,
    searchEngine: s.searchEngine,
    customSearchUrl: s.customSearchUrl,
    restoreSession: s.restoreSession,
    blockedHosts: s.blockedHosts.filter(
      (host): host is string => typeof host === "string",
    ),
    accentColor: s.accentColor,
    addressBarShape: s.addressBarShape,
  };
}

// The security notice's own push. Its own function rather than a field on
// the browser state, because the notice is a separate page shown over the
// one being blocked and has no use for a tab list.
export function onRisk(handler: (notice: RiskNotice) => void): void {
  install(pushes.risk, (value: unknown) => {
    const parsed = parseRisk(value);
    if (parsed) {
      handler(parsed);
    }
  });
  send("requestState");
}

function parseRisk(value: unknown): RiskNotice | null {
  if (typeof value !== "object" || value === null) {
    return null;
  }
  const notice = value as Record<string, unknown>;
  // The level decides whether a bypass is offered at all, so an unrecognised
  // one is dropped rather than guessed -- guessing "warning" would invent a
  // way through a block.
  if (notice.level !== "warning" && notice.level !== "block") {
    console.warn("weglet: malformed risk level", value);
    return null;
  }
  if (
    typeof notice.title !== "string" ||
    typeof notice.reason !== "string" ||
    typeof notice.host !== "string" ||
    typeof notice.target !== "string"
  ) {
    console.warn("weglet: malformed risk notice", value);
    return null;
  }
  return {
    level: notice.level,
    title: notice.title,
    reason: notice.reason,
    host: notice.host,
    target: notice.target,
  };
}

// Validates rather than casts. The browser is trusted, but a bug on either
// side should surface as a dropped update with a console line, not as an
// exception halfway through rendering the tab strip.
function parseState(value: unknown): BrowserState | null {
  if (typeof value !== "object" || value === null) {
    return null;
  }
  const candidate = value as {
    tabs?: unknown;
    activeId?: unknown;
    focusOmnibox?: unknown;
  };
  if (
    !Array.isArray(candidate.tabs) ||
    typeof candidate.activeId !== "string" ||
    typeof candidate.focusOmnibox !== "boolean"
  ) {
    console.warn("weglet: malformed state", value);
    return null;
  }
  const tabs: TabState[] = [];
  for (const entry of candidate.tabs) {
    const tab = parseTab(entry);
    if (!tab) {
      return null;
    }
    tabs.push(tab);
  }
  return {
    tabs,
    activeId: candidate.activeId,
    focusOmnibox: candidate.focusOmnibox,
  };
}

function parseTab(value: unknown): TabState | null {
  if (typeof value !== "object" || value === null) {
    return null;
  }
  const tab = value as Record<string, unknown>;
  if (
    typeof tab.id !== "string" ||
    typeof tab.url !== "string" ||
    typeof tab.label !== "string" ||
    typeof tab.canGoBack !== "boolean" ||
    typeof tab.canGoForward !== "boolean" ||
    typeof tab.loading !== "boolean"
  ) {
    console.warn("weglet: malformed tab", value);
    return null;
  }
  return {
    id: tab.id,
    url: tab.url,
    label: tab.label,
    canGoBack: tab.canGoBack,
    canGoForward: tab.canGoForward,
    loading: tab.loading,
  };
}
