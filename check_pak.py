#!/usr/bin/env python3
# Copyright 2026 Weglet - Licensed under Apache 2.0
#
# Compares weglet_pak against content/shell's own repack target.
#
# weglet/BUILD.gn carries a hand-written list of Chromium's resource packs,
# trimmed from content/shell's. A pack added there is usually one the
# content layer now expects every embedder to have, and the way you find
# out you missed it is a CHECK at startup with nothing on screen. Most of
# the difference is deliberate, so the differences are declared below and a
# new one fails the build with the pack's name.

import argparse
import os
import re
import sys

# Packs content/shell has that Weglet deliberately does not, and why. A
# pack in content/shell that is in neither this table nor weglet/BUILD.gn
# is a build failure.
def load_exclusions(path: str) -> dict:
    """Packs Weglet deliberately does not have, and why.

    A table, not a dict in this file: adding an exclusion should not mean
    editing the script that checks them.
    """
    exclusions = {}
    with open(path, encoding="utf-8") as handle:
        for line in handle:
            line = line.split("#")[0].strip()
            if not line:
                continue
            name, _, reason = line.partition(" ")
            exclusions[name] = reason.strip()
    return exclusions



def repack_sources(text: str, target: str) -> set:
    """The .pak files listed in a repack() target."""
    match = re.search(
        r'repack\(\s*"' + re.escape(target) + r'"\s*\)\s*\{(.*?)\n\}',
        text,
        re.S,
    )
    if not match:
        raise SystemExit(f'no repack("{target}") target found\n')
    return set(
        re.findall(r'"\$\{?root_gen_dir\}?/([^"]+\.pak)"', match.group(1))
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--weglet-build", required=True, help="weglet/BUILD.gn")
    parser.add_argument(
        "--shell-build", required=True, help="content/shell/BUILD.gn"
    )
    parser.add_argument(
        "--exclusions", required=True, help="weglet/pak_exclusions.txt"
    )
    # Written only so GN has an output to depend on.
    parser.add_argument("--stamp")
    args = parser.parse_args()

    with open(args.weglet_build, encoding="utf-8") as handle:
        ours = repack_sources(handle.read(), "weglet_pak")
    with open(args.shell_build, encoding="utf-8") as handle:
        theirs = repack_sources(handle.read(), "pak")

    missing = sorted(theirs - ours - set(load_exclusions(args.exclusions)))
    if missing:
        lines = [
            "weglet/BUILD.gn's weglet_pak is missing resource packs that",
            "content/shell has:",
            "",
        ]
        lines += [f"    {name}" for name in missing]
        lines += [
            "",
            "A pack added to content/shell is usually one the content layer",
            "now expects every embedder to have, and a missing one shows up",
            "as a CHECK at startup rather than as a build error.",
            "",
            "Either add it to the weglet_pak target, or -- if Weglet really",
            "does not want it -- add it to weglet/pak_exclusions.txt with",
            "the reason.",
            "",
        ]
        sys.stderr.write("\n".join(lines))
        return 1

    # Not fatal: our list is allowed to be a superset.
    extra = sorted(ours - theirs)
    if extra:
        sys.stderr.write(
            "note: weglet_pak lists packs content/shell does not: "
            + ", ".join(extra)
            + "\n"
        )

    if args.stamp:
        os.makedirs(os.path.dirname(args.stamp) or ".", exist_ok=True)
        with open(args.stamp, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(
                f"compared {len(ours)} weglet packs against "
                f"{len(theirs)} in content/shell\n"
            )
    return 0


if __name__ == "__main__":
    sys.exit(main())
