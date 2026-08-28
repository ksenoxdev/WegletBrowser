// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// The real favicon URL, when the user has opted in -- see docs/security.md.
// Always the well-known /favicon.ico, not whatever <link rel="icon"> a page
// points at; every call site keeps its own letter/mark as a fallback since
// the request can fail.
export function faviconUrl(pageUrl: string): string | null {
  let parsed: URL;
  try {
    parsed = new URL(pageUrl);
  } catch {
    return null;
  }
  if (parsed.protocol !== "http:" && parsed.protocol !== "https:") {
    return null;
  }
  return `https://${parsed.hostname}/favicon.ico`;
}
