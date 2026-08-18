#!/usr/bin/env python3
# Copyright 2026 Weglet - Licensed under Apache 2.0
#
# weglet/rust/build_rust.py
#
# Runs cargo for GN and reports the result the way GN expects.
#
# Cargo rather than GN's own Rust support because the dependency set comes
# from crates.io. Declaring each crate as a third_party GN target would be
# a hand-maintained copy of Cargo.lock, and every `cargo update` would be
# a patch to the Chromium tree.

import argparse
import os
import shutil
import subprocess
import sys

# weglet/build_support.py -- find_node_toolchain and write_depfile, which
# every build script here needs and which used to exist twice.
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import build_support  # noqa: E402


def generate(workspace: str) -> None:
    """Regenerates the two files weglet crates get from weglet/ui.

    Run before every cargo build, not gated by a depfile: both are small to
    produce, and the crates have to build correctly under a plain `cargo
    build` too, with no GN action having run first. Both are checked in for
    the same reason.

      * weglet-core/src/generated_addresses.rs -- Weglet's own addresses,
        from contract.json. The C++ side generates its copy from the same
        file, so the addresses a tab can hold and the addresses
        ResolveForEngine knows how to open cannot list a different set
        again.

      * weglet-profile/src/generated_defaults.rs -- the default accent,
        from tokens.json. The same colour the CSS compiles in and the
        settings page offers first. This used to be a literal in
        settings.rs that this script kept honest by reading the .rs file
        with a regular expression: Python parsing Rust to notice a
        disagreement it could not fix. Generating it means there is
        nothing left to disagree.

    Each generator is asked for exactly the output wanted. They used to
    write all of theirs at once, so this script passed throwaway paths
    inside the source tree and deleted them afterwards.
    """
    ui_dir = os.path.join(os.path.dirname(workspace), "ui")

    jobs = [
        (
            "generate_contract.py",
            "--contract",
            os.path.join(ui_dir, "contract.json"),
            os.path.join(workspace, "weglet-core", "src", "generated_addresses.rs"),
        ),
        (
            "generate_tokens.py",
            "--tokens",
            os.path.join(ui_dir, "tokens.json"),
            os.path.join(workspace, "weglet-profile", "src", "generated_defaults.rs"),
        ),
    ]

    for script, source_flag, source, out in jobs:
        result = subprocess.run(
            [
                sys.executable,
                os.path.join(ui_dir, script),
                source_flag,
                source,
                "--rust",
                out,
            ]
        )
        if result.returncode != 0:
            raise SystemExit(result.returncode)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", required=True, help="path to rust/Cargo.toml")
    parser.add_argument("--target", required=True, help="rust target triple")
    parser.add_argument("--out", required=True, help="where to put the static library")
    parser.add_argument("--profile", default="release", choices=["debug", "release"])
    parser.add_argument("--depfile", help="GN depfile to write")
    args = parser.parse_args()

    manifest = os.path.abspath(args.manifest)
    workspace = os.path.dirname(manifest)
    out = os.path.abspath(args.out)

    generate(workspace)

    cargo = shutil.which("cargo")
    if cargo is None:
        sys.stderr.write(
            "cargo not found on PATH.\n"
            "Weglet's browser logic is Rust; install the toolchain from\n"
            "https://rustup.rs and re-run gn gen.\n"
        )
        return 1

    # Its own target directory, inside the GN output tree. Sharing one with
    # a developer's `cargo build` would mean the two fighting over the same
    # lock and rebuilding each other's artifacts.
    target_dir = os.path.join(os.path.dirname(out), "rust")

    command = [
        cargo,
        "build",
        "--manifest-path",
        manifest,
        "--package",
        "weglet-ffi",
        "--target",
        args.target,
        "--target-dir",
        target_dir,
        # Reproducible: no network at build time, and a lock file that is
        # never silently updated by a build.
        "--locked",
        "--offline",
    ]
    if args.profile == "release":
        command.append("--release")

    result = subprocess.run(command, cwd=workspace)
    if result.returncode != 0:
        return result.returncode

    # Windows names it weglet_ffi.lib, everything else libweglet_ffi.a.
    built_dir = os.path.join(target_dir, args.target, args.profile)
    candidates = ["weglet_ffi.lib", "libweglet_ffi.a"]
    for name in candidates:
        built = os.path.join(built_dir, name)
        if os.path.exists(built):
            os.makedirs(os.path.dirname(out), exist_ok=True)
            shutil.copy2(built, out)
            break
    else:
        sys.stderr.write(
            f"cargo reported success but no static library in {built_dir}\n"
            f"looked for: {', '.join(candidates)}\n"
        )
        return 1

    # Tells ninja to re-run this when any source changes. Without it a
    # change to a .rs file does not rebuild, which is the kind of thing
    # that costs an afternoon.
    if args.depfile:
        # Every .rs plus the manifests: cargo decides what to rebuild, but
        # GN has to know when to ask it at all.
        build_support.write_depfile(
            args.depfile,
            args.out,
            workspace,
            lambda name: name.endswith(".rs")
            or name in ("Cargo.toml", "Cargo.lock"),
        )

    return 0



if __name__ == "__main__":
    sys.exit(main())
