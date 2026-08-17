// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/ui/src/protocol.ts
//
// Every message that crosses between a page and the browser, typed once.
//
// This file is the contract. The C++ side mirrors it by hand, so a change
// here is a change there in the same commit -- but at least on this side
// a typo in a message name is a compile error rather than a silently
// ignored string, which is what the previous flat-string protocol gave.

// What the browser sends to a page.
export type ToPage =
  | { kind: "newtab"; shortcuts: Shortcut[] }
  | { kind: "settings"; settings: SettingsView }
  | { kind: "theme"; accent: string | null };

// What a page may send back. Deliberately small: a page gets what it
// needs to do its own job and nothing else. Anything privileged is
// unreachable from here by construction -- see the two channels in
// docs/security.md.
export type FromPage =
  | { kind: "navigate"; url: string }
  | { kind: "open-settings" }
  | { kind: "shortcut-clicked"; index: number };

export interface Shortcut {
  readonly title: string;
  readonly url: string;
}

export interface SettingsView {
  readonly searchEngine: "duckduckgo" | "google" | "bing";
  readonly restoreSession: boolean;
  readonly blockedHosts: readonly string[];
}

// The one place a page talks to the browser. Typed, so `send` cannot be
// handed a shape the browser does not understand.
export function send(message: FromPage): void {
  // Set up by the browser before any page script runs. Absent when a page
  // is opened outside Weglet -- during development, or if somebody saves
  // the HTML -- so this is a no-op rather than a crash.
  const bridge = (globalThis as { wegletSend?: (json: string) => void })
    .wegletSend;
  if (!bridge) {
    return;
  }
  bridge(JSON.stringify(message));
}

// Registers a handler for messages from the browser. Returns a function
// that unregisters it.
export function receive(handler: (message: ToPage) => void): () => void {
  const listener = (event: MessageEvent) => {
    const parsed = parse(event.data);
    if (parsed) {
      handler(parsed);
    }
  };
  globalThis.addEventListener("message", listener);
  return () => globalThis.removeEventListener("message", listener);
}

// Validates rather than casts. The browser is trusted, but a bug on
// either side should surface as a dropped message with a console line,
// not as an exception halfway through rendering.
function parse(data: unknown): ToPage | null {
  if (typeof data !== "object" || data === null) {
    return null;
  }
  const kind = (data as { kind?: unknown }).kind;
  if (kind !== "newtab" && kind !== "settings" && kind !== "theme") {
    console.warn("weglet: unknown message kind", kind);
    return null;
  }
  return data as ToPage;
}
