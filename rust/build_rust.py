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
        write_depfile(args.depfile, args.out, workspace)

    return 0


def write_depfile(depfile: str, out: str, workspace: str) -> None:
    """Writes a make-style depfile listing every Rust source.

    Paths are relative to the build directory, and that is not cosmetic:
    an absolute Windows path contains a drive-letter colon, and the
    depfile format uses the colon as its one separator. Ninja rejects the
    whole file over it.
    """
    sources = []
    for root, dirs, files in os.walk(workspace):
        # Skip the build output, or the depfile lists its own products and
        # the build never settles.
        dirs[:] = [d for d in dirs if d not in ("target", ".git")]
        for name in files:
            if name.endswith(".rs") or name in ("Cargo.toml", "Cargo.lock"):
                sources.append(os.path.join(root, name))

    # cwd is the build directory: GN runs the script from there and passes
    # --out relative to it.
    def relative(path: str) -> str:
        return os.path.relpath(path).replace("\\", "/")

    deps = " ".join(relative(path) for path in sorted(sources))
    with open(depfile, "w", encoding="utf-8") as handle:
        handle.write(f"{out.replace(chr(92), '/')}: {deps}\n")


if __name__ == "__main__":
    sys.exit(main())
