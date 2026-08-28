#!/usr/bin/env python3
# Copyright 2026 Weglet - Licensed under Apache 2.0
#
# Compares weglet/CHROMIUM_VERSION against chrome/VERSION in the tree
# being built.
#
# Which checkout was recorded nowhere -- weglet/DEPS is an include-rules
# file and has no business pinning revisions -- so building against an
# unexpected tree failed as a link error or a startup CHECK, if at all.
#
# A mismatch is a warning by default and an error with --strict: the point
# is to say which tree this is, not to stop someone trying a newer one.

import argparse
import os
import sys


def read_pairs(path: str) -> dict:
    values = {}
    with open(path, encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, _, value = line.partition("=")
            values[key.strip()] = value.strip()
    return values


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--expected", required=True, help="weglet/CHROMIUM_VERSION")
    parser.add_argument("--actual", required=True, help="chrome/VERSION")
    parser.add_argument("--stamp")
    parser.add_argument(
        "--strict",
        action="store_true",
        help="fail the build on a mismatch instead of warning",
    )
    args = parser.parse_args()

    expected = read_pairs(args.expected).get("VERSION")
    if not expected:
        raise SystemExit("weglet/CHROMIUM_VERSION has no VERSION= line\n")

    actual_parts = read_pairs(args.actual)
    try:
        actual = ".".join(
            actual_parts[part] for part in ("MAJOR", "MINOR", "BUILD", "PATCH")
        )
    except KeyError as missing:
        raise SystemExit(f"chrome/VERSION has no {missing} line\n")

    if expected != actual:
        message = (
            f"Weglet expects Chromium {expected}; this tree is {actual}.\n"
            "If the move is deliberate, update weglet/CHROMIUM_VERSION in "
            "the same commit as any code it required.\n"
        )
        if args.strict:
            sys.stderr.write("error: " + message)
            return 1
        sys.stderr.write("warning: " + message)

    if args.stamp:
        os.makedirs(os.path.dirname(args.stamp) or ".", exist_ok=True)
        with open(args.stamp, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(f"expected {expected}, tree {actual}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
