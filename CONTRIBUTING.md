# Contributing

## Before you write code

Read [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) first if you haven't --
it explains why logic is in Rust and C++ is an adapter, which decides
where a given change belongs.

## Setup

You need a full Chromium checkout with this repository at `src/weglet/`,
plus `rustc`/`cargo` on `PATH` and `gclient runhooks` run at least once
(it downloads the node/TypeScript toolchain the UI tests and build use).
See [`README.md`](README.md#building) for the build command.

Most of the test suite does not need a Chromium build at all -- see below.

## Files you generate, not edit

`ui/contract.json` and `ui/tokens.json` are the only place a message
name, a message's argument types, a page route, or a design token is
written. Everything else derived from them is generated and should never
be hand-edited:

- `ui/src/contract.ts`, `ui/src/tokens.ts`
- `gen/weglet/ui/generated_contract.h`, `generated_tokens.h` (build
  output, not checked in)
- `rust/weglet-core/src/generated_addresses.rs`,
  `rust/weglet-profile/src/generated_defaults.rs` (checked in, so the
  two crates build under a plain `cargo build` with no GN action having
  run -- but still written by `ui/generate_contract.py` /
  `generate_tokens.py`, never by hand)

If you need a new message, a new page, or a new token: edit the JSON,
then run the generator. `ui/generate_contract.py` and
`generate_tokens.py` both accept `--ts`, `--header`, `--rust` (tokens
also takes `--css`) independently -- pass only what you need regenerated.

```
python3 ui/generate_contract.py --contract ui/contract.json --ts ui/src/contract.ts
python3 ui/generate_tokens.py --tokens ui/tokens.json --css ui/pages/tokens.css --ts ui/src/tokens.ts
```

A change to `contract.json` or `tokens.json` should come with a run of
`generators_test.py` before you commit -- it validates the generator's
own output, not just that it ran.

## Running the tests

```
# Rust: the tab model, the omnibox parser, the profile, the security
# heuristics, and the FFI boundary itself. No Chromium build needed.
cd weglet/rust
cargo test --workspace
cargo clippy --workspace --all-targets

# The contract/token generators, standalone.
cd weglet/ui
python3 generators_test.py

# protocol.ts's own validation of what the browser pushes, under
# Chromium's vendored node.
python3 run_ui_tests.py

# tsc, strict mode.
tsc --project tsconfig.json --noEmit

# The generated contract from the C++ side. Needs a build.
autoninja -C out/Release weglet_unittests
out/Release/weglet_unittests
```

If you touch `weglet-ffi`, add a case to
`rust/weglet-ffi/tests/boundary.rs` rather than only a unit test inside
the crate: `boundary.rs` binds every FFI function by its declared C
signature and calls it the way C++ does, so a signature mismatch between
`weglet_ffi.h` and the Rust source fails to compile there instead of
becoming undefined behaviour in the browser process.

## Where a fix belongs

A rule of thumb, not a hard boundary: behavior of tabs, history, the
omnibox, settings, or the security heuristics goes in Rust
(`weglet-core`, `weglet-profile`, `weglet-security`). Whether a
message or push is well-formed, or how a page renders, goes in
TypeScript. Anything content's API requires an embedder to implement, or
the window/widget itself, goes in C++.

If you're not sure, `docs/ARCHITECTURE.md`'s "State flow" section walks
through who owns what for a typical message round-trip.

## Commit messages

Short, factual, present tense. State what changed, not what you did:
`fix: reject a settings-flush interval of zero`, not `Fixed a bug where
the interval could be zero`. No AI-generated bullet-list bodies; if the
summary line needs more context, write one or two plain sentences.

## License

By contributing, you agree your contribution is licensed under the
Apache License 2.0 (see `LICENSE`). Third-party assets (fonts, icon data,
Rust dependencies) keep their own licenses -- see
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md), and add an entry
there if you bring in a new one.
