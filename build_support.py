#!/usr/bin/env python3
# Copyright 2026 Weglet - Licensed under Apache 2.0
#
# weglet/build_support.py
#
# The two things every build script here needs, in one place.
#
# Both existed twice: find_toolchain in build_ui.py and run_ui_tests.py,
# write_depfile in build_ui.py and build_rust.py. The copies had already
# drifted -- build_rust.py's opened the depfile without newline="\n", so
# on Windows it wrote CRLF into a format ninja parses by hand, and the two
# copies of the drive-letter explanation said the same thing in different
# words.
#
# Imported by scripts that live one directory down, so they add weglet/ to
# sys.path first:
#
#     sys.path.insert(0, os.path.dirname(os.path.dirname(
#         os.path.abspath(__file__))))
#     import build_support

import os


# Where gclient runhooks puts node, per platform. Not a lookup on PATH:
# the build has to use the same node and the same TypeScript as everyone
# else building this tree, or "works on my machine" becomes a version of
# tsc nobody else has.
_NODE_BINARIES = (
    ("win", "node.exe"),
    ("linux", "node-linux-x64", "bin", "node"),
    ("linux", "node-linux-arm64", "bin", "node"),
    ("mac", "node-darwin-x64", "bin", "node"),
    ("mac", "node-darwin-arm64", "bin", "node"),
)


def find_node_toolchain(chromium_root: str) -> tuple[str, str]:
    """Returns (node binary, tsc entry point) from Chromium's third_party.

    Raises SystemExit with the command to run when either is missing --
    the answer is always `gclient runhooks`, and a script that only says
    "not found" makes the reader go looking for that.
    """
    node_dir = os.path.join(chromium_root, "third_party", "node")
    candidates = [os.path.join(node_dir, *parts) for parts in _NODE_BINARIES]
    node = next((path for path in candidates if os.path.exists(path)), None)
    tsc = os.path.join(node_dir, "node_modules", "typescript", "lib", "tsc.js")

    if node is None or not os.path.exists(tsc):
        raise SystemExit(
            "Chromium's vendored node/TypeScript was not found under\n"
            f"  {node_dir}\n"
            "Run `gclient runhooks` -- it downloads both.\n"
        )
    return node, tsc


def write_depfile(depfile: str, out: str, root: str, matches) -> None:
    """Writes a make-style depfile listing every input under `root`.

    `matches(name)` decides whether a file counts. Directories named
    target, node_modules, __pycache__ and .git are skipped: the first two
    are build output, and a depfile listing its own products is a build
    that never settles.

    Paths come out relative to the build directory, and that is not
    cosmetic. An absolute Windows path carries a drive-letter colon, and
    the depfile format uses the colon as its one separator -- ninja
    rejects the whole file over it. cwd is the build directory: GN runs
    these scripts from there.

    newline="\\n" for the same kind of reason: ninja parses this format
    itself, and CRLF is not part of it.
    """
    inputs = []
    for directory, subdirectories, names in os.walk(root):
        subdirectories[:] = [
            name
            for name in subdirectories
            if name not in ("target", "node_modules", "__pycache__", ".git")
        ]
        inputs += [
            os.path.join(directory, name) for name in names if matches(name)
        ]

    deps = " ".join(
        os.path.relpath(path).replace("\\", "/") for path in sorted(inputs)
    )
    os.makedirs(os.path.dirname(depfile) or ".", exist_ok=True)
    with open(depfile, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(f"{out.replace(os.sep, '/')}: {deps}\n")
