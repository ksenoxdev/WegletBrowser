#!/usr/bin/env python3
# Copyright 2026 Weglet - Licensed under Apache 2.0
#
# weglet/ui/run_ui_tests.py
#
# Compiles and runs weglet/ui/test/*.ts under Chromium's vendored node --
# the same node and the same TypeScript that build_ui.py compiles the
# pages with. No npm, no node_modules, no test framework: the tests assert
# and exit non-zero, which is all a build step needs.
#
# Usage:
#   python3 weglet/ui/run_ui_tests.py --chromium-root ../..

import argparse
import os
import shutil
import subprocess
import sys

# weglet/build_support.py -- find_node_toolchain and write_depfile, which
# every build script here needs and which used to exist twice.
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import build_support  # noqa: E402
import tempfile

UI_DIR = os.path.dirname(os.path.abspath(__file__))



def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--chromium-root", default=os.path.join(UI_DIR, "..", ".."))
    parser.add_argument("--stamp")
    args = parser.parse_args()

    node, tsc = build_support.find_node_toolchain(os.path.abspath(args.chromium_root))

    with tempfile.TemporaryDirectory() as out:
        # tokens.ts and contract.ts are generated next to the sources by
        # build_ui.py. Generated here too, so the tests run without a full
        # build having happened first.
        for script, source_flag, source, flag, name in [
            ("generate_tokens.py", "--tokens", "tokens.json", "--ts", "tokens.ts"),
            ("generate_contract.py", "--contract", "contract.json", "--ts", "contract.ts"),
        ]:
            result = subprocess.run(
                [
                    sys.executable,
                    os.path.join(UI_DIR, script),
                    source_flag,
                    os.path.join(UI_DIR, source),
                    flag,
                    os.path.join(UI_DIR, "src", name),
                ]
            )
            if result.returncode != 0:
                return result.returncode

        # Its own tsconfig: the tests are not part of what ships, and the
        # shipped tsconfig's `include` deliberately covers only src/.
        config = os.path.join(out, "tsconfig.json")
        with open(config, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(
                '{\n'
                '  "extends": "%s",\n'
                '  "compilerOptions": {\n'
                '    "outDir": "%s",\n'
                '    "rootDir": "%s",\n'
                '    "types": []\n'
                '  },\n'
                '  "include": ["%s/test/**/*.ts", "%s/src/**/*.ts"]\n'
                '}\n'
                % (
                    os.path.join(UI_DIR, "tsconfig.json").replace("\\", "/"),
                    out.replace("\\", "/"),
                    UI_DIR.replace("\\", "/"),
                    UI_DIR.replace("\\", "/"),
                    UI_DIR.replace("\\", "/"),
                )
            )

        # ES modules under node need the directory marked as such. Written
        # before tsc runs: the module settings come from the shipped
        # tsconfig, which already emits ES modules, and node reads this to
        # decide how to load them.
        with open(
            os.path.join(out, "package.json"), "w", encoding="utf-8", newline="\n"
        ) as handle:
            handle.write('{"type": "module"}\n')

        result = subprocess.run([node, tsc, "--project", config])
        if result.returncode != 0:
            return result.returncode

        failed = 0
        test_dir = os.path.join(out, "test")
        for name in sorted(os.listdir(test_dir)) if os.path.isdir(test_dir) else []:
            if not name.endswith(".js"):
                continue
            print(f"--- {name}")
            if subprocess.run([node, os.path.join(test_dir, name)]).returncode != 0:
                failed += 1
        if failed:
            sys.stderr.write(f"{failed} UI test file(s) failed\n")
            return 1

    if args.stamp:
        os.makedirs(os.path.dirname(args.stamp) or ".", exist_ok=True)
        with open(args.stamp, "w", encoding="utf-8", newline="\n") as handle:
            handle.write("ui tests passed\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
