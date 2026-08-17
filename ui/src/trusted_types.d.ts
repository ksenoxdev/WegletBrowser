// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// weglet/ui/src/trusted_types.d.ts
//
// TypeScript's own lib.dom.d.ts references these three names but does not
// declare them -- they live in a separate @types package that Chromium
// wires in through its own config. Declaring them here keeps the build off
// that dependency.
//
// Opaque on purpose. Weglet never creates a TrustedHTML: the only way a
// string becomes DOM in this codebase is textContent (see dom.ts), so
// nothing needs to construct one and nothing should be able to.

declare interface TrustedHTML {
  readonly __brand: "TrustedHTML";
}

declare interface TrustedScript {
  readonly __brand: "TrustedScript";
}

declare interface TrustedScriptURL {
  readonly __brand: "TrustedScriptURL";
}
