// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// lib.dom.d.ts references these three names but does not declare them;
// they live in a separate @types package. Declaring them here keeps the
// build off that dependency.
//
// Opaque on purpose: nothing in this codebase constructs a TrustedHTML,
// because the only way a string becomes DOM is textContent.

declare interface TrustedHTML {
  readonly __brand: "TrustedHTML";
}

declare interface TrustedScript {
  readonly __brand: "TrustedScript";
}

declare interface TrustedScriptURL {
  readonly __brand: "TrustedScriptURL";
}
