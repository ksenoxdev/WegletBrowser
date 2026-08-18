#!/usr/bin/env python3
# Copyright 2026 Weglet - Licensed under Apache 2.0
#
# weglet/ui/generators_test.py
#
# The generators are the mechanism that keeps four languages saying the
# same thing, and nothing tested them. Their only self-check was a
# duplicate-name test in generate_contract.py.
#
# That matters more than it looks: a mistake in contract.json or tokens.json
# does not fail here, it produces generated C++ that fails to compile with
# an error pointing at a file nobody wrote -- or, worse, generated code that
# compiles and is wrong.
#
# Run with: python3 weglet/ui/generators_test.py

import argparse
import json
import os
import subprocess
import sys
import tempfile
import unittest

UI_DIR = os.path.dirname(os.path.abspath(__file__))
CONTRACT = os.path.join(UI_DIR, "generate_contract.py")
TOKENS = os.path.join(UI_DIR, "generate_tokens.py")


def run(script: str, *args: str) -> subprocess.CompletedProcess:
    return subprocess.run(
        [sys.executable, script, *args], capture_output=True, text=True
    )


class GeneratorTestCase(unittest.TestCase):
    def setUp(self) -> None:
        self._dir = tempfile.TemporaryDirectory()
        self.out = self._dir.name
        self.addCleanup(self._dir.cleanup)

    def path(self, name: str) -> str:
        return os.path.join(self.out, name)

    def write_json(self, name: str, data: dict) -> str:
        path = self.path(name)
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(data, handle)
        return path


class ContractTest(GeneratorTestCase):
    def minimal(self, **overrides) -> dict:
        contract = {
            "pages": {"newtab": "newtab.html", "toolbar": "toolbar.html"},
            "messages": [
                {"name": "navigate", "args": ["string"], "implemented": True},
                {"name": "closeTab", "args": ["TabId"], "implemented": True},
                {"name": "later", "args": [], "implemented": False},
            ],
            "pushes": {"state": {"page": "toolbar", "function": "setState"}},
            "internal_addresses": {
                "blank-tab": {"address": "about:blank", "page": "newtab"}
            },
        }
        contract.update(overrides)
        return contract

    def generate(self, contract: dict, *outputs: str) -> subprocess.CompletedProcess:
        source = self.write_json("contract.json", contract)
        args = ["--contract", source]
        for output in outputs or ("ts", "header", "rust"):
            args += [f"--{output}", self.path(f"out.{output}")]
        return run(CONTRACT, *args)

    def read(self, kind: str) -> str:
        with open(self.path(f"out.{kind}"), encoding="utf-8") as handle:
            return handle.read()

    # --- the happy path -------------------------------------------------

    def test_generates_every_output_from_one_source(self):
        self.assertEqual(self.generate(self.minimal()).returncode, 0)
        self.assertIn('["navigate", string]', self.read("ts"))
        self.assertIn('"navigate"', self.read("header"))
        self.assertIn('BLANK_TAB: &str = "about:blank"', self.read("rust"))

    # Asking for one output must not write the others. build_rust.py used
    # to pass throwaway paths inside the source tree because of this.
    def test_each_output_is_independent(self):
        result = self.generate(self.minimal(), "rust")
        self.assertEqual(result.returncode, 0)
        self.assertTrue(os.path.exists(self.path("out.rust")))
        self.assertFalse(os.path.exists(self.path("out.ts")))
        self.assertFalse(os.path.exists(self.path("out.header")))

    def test_asking_for_nothing_is_an_error(self):
        source = self.write_json("contract.json", self.minimal())
        result = run(CONTRACT, "--contract", source)
        self.assertNotEqual(result.returncode, 0)

    # The arity and the argument types reach C++, which is the whole point
    # of the MessageSpec table: before, only TypeScript used them.
    def test_argument_types_reach_the_header(self):
        self.generate(self.minimal())
        header = self.read("header")
        self.assertIn('MessageSpec{"navigate", 1, {ArgKind::kString', header)
        self.assertIn('MessageSpec{"closeTab", 1, {ArgKind::kTabId', header)
        self.assertIn('MessageSpec{"later", 0,', header)
        # And whether it is implemented, so the handler need not keep a
        # second list.
        self.assertIn("}, false},", header)

    def test_pushes_reach_both_sides_with_the_same_spelling(self):
        self.generate(self.minimal())
        self.assertIn('Push{PageKind::kToolbar, "setState"}', self.read("header"))
        self.assertIn('state: "setState"', self.read("ts"))

    def test_page_kinds_are_generated_from_the_page_list(self):
        self.generate(self.minimal())
        header = self.read("header")
        self.assertIn("enum class PageKind", header)
        self.assertIn("  kNewtab,", header)
        self.assertIn("  kToolbar,", header)
        self.assertIn("KindForPath", header)

    # --- what it has to refuse ------------------------------------------

    def test_duplicate_message_name_is_refused(self):
        contract = self.minimal()
        contract["messages"].append(
            {"name": "navigate", "args": [], "implemented": True}
        )
        result = self.generate(contract)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("duplicate message name", result.stderr)

    # An unknown type would become an ArgKind that does not exist, and the
    # generated header would fail to compile with a KeyError's worth of
    # context.
    def test_unknown_argument_type_is_refused(self):
        contract = self.minimal()
        contract["messages"][0]["args"] = ["Widget"]
        result = self.generate(contract)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unknown type", result.stderr)

    def test_push_at_an_unknown_page_is_refused(self):
        contract = self.minimal()
        contract["pushes"]["state"]["page"] = "nonesuch"
        result = self.generate(contract)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not in the pages block", result.stderr)

    # Two pushes at one page: the browser picks the function by page, so
    # the second could never be sent.
    def test_two_pushes_at_one_page_are_refused(self):
        contract = self.minimal()
        contract["pushes"]["other"] = {"page": "toolbar", "function": "setOther"}
        result = self.generate(contract)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("same page", result.stderr)

    def test_duplicate_push_function_is_refused(self):
        contract = self.minimal()
        contract["pushes"]["other"] = {"page": "newtab", "function": "setState"}
        result = self.generate(contract)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("duplicate push function", result.stderr)

    # The exact failure that started all of this: an address resolving to a
    # page that does not exist. It used to produce a header referring to an
    # undeclared identifier.
    def test_address_resolving_to_an_unknown_page_is_refused(self):
        contract = self.minimal()
        contract["internal_addresses"]["blank-tab"]["page"] = "nonesuch"
        result = self.generate(contract)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not in the pages block", result.stderr)

    # Checked whichever output was asked for, so a mistake is caught by the
    # first build that reads the file rather than by whichever one happens
    # to write the affected output.
    def test_validation_runs_even_for_an_unrelated_output(self):
        contract = self.minimal()
        contract["pushes"]["state"]["page"] = "nonesuch"
        result = self.generate(contract, "rust")
        self.assertNotEqual(result.returncode, 0)

    # An unchanged output keeps its timestamp, or ninja rebuilds
    # everything downstream on every run.
    def test_an_unchanged_output_is_not_rewritten(self):
        contract = self.minimal()
        self.generate(contract, "rust")
        before = os.stat(self.path("out.rust")).st_mtime_ns
        os.utime(self.path("out.rust"), ns=(before - 10**9, before - 10**9))
        stamped = os.stat(self.path("out.rust")).st_mtime_ns
        self.generate(contract, "rust")
        self.assertEqual(os.stat(self.path("out.rust")).st_mtime_ns, stamped)

    def test_the_real_contract_generates(self):
        result = run(
            CONTRACT,
            "--contract",
            os.path.join(UI_DIR, "contract.json"),
            "--ts",
            self.path("out.ts"),
            "--header",
            self.path("out.header"),
            "--rust",
            self.path("out.rust"),
        )
        self.assertEqual(result.returncode, 0, result.stderr)


class TokensTest(GeneratorTestCase):
    def minimal(self, **overrides) -> dict:
        tokens = {
            "color": {"accent-solid": "#A855F7", "ring": "#C084FC", "field": "#121118"},
            "space": {"md": 8},
            "radius": {"pill": 999, "md": 13},
            "layout": {"tab-height": 32},
            "motion": {"hover-tau": 55},
            "font": {"size-md": 14, "weight-bold": 700},
            "accentPresets": ["#A855F7", "#3B82F6"],
        }
        tokens.update(overrides)
        return tokens

    def generate(self, tokens: dict, *outputs: str) -> subprocess.CompletedProcess:
        source = self.write_json("tokens.json", tokens)
        args = ["--tokens", source]
        for output in outputs or ("css", "ts", "header", "rust"):
            args += [f"--{output}", self.path(f"out.{output}")]
        return run(TOKENS, *args)

    def read(self, kind: str) -> str:
        with open(self.path(f"out.{kind}"), encoding="utf-8") as handle:
            return handle.read()

    def test_units_are_added_per_group(self):
        self.assertEqual(self.generate(self.minimal()).returncode, 0)
        css = self.read("css")
        self.assertIn("--space-md: 8px;", css)
        self.assertIn("--layout-tab-height: 32px;", css)
        self.assertIn("--motion-hover-tau: 55ms;", css)
        self.assertIn("--font-size-md: 14px;", css)
        # Not a length: a weight with px on it is silently ignored.
        self.assertIn("--font-weight-bold: 700;", css)
        # A pill is "as round as it gets", not 999 pixels of anything.
        self.assertIn("--radius-pill: 9999px;", css)

    # The C++ header exists for one failure: the toolbar's height is set by
    # the markup that fills it and by the view that sizes it, and when they
    # disagree the bottom row is clipped.
    def test_only_numeric_groups_reach_cpp(self):
        self.generate(self.minimal())
        header = self.read("header")
        self.assertIn("kLayoutTabHeight = 32;", header)
        self.assertNotIn("accent", header.lower())

    def test_fonts_are_woff2(self):
        self.generate(self.minimal())
        css = self.read("css")
        self.assertIn('format("woff2")', css)
        # .ttf has no entry in content's own MIME table and would be served
        # as text/html.
        self.assertNotIn(".ttf", css)

    def test_the_default_accent_reaches_rust(self):
        self.generate(self.minimal())
        self.assertIn('DEFAULT_ACCENT: &str = "#A855F7"', self.read("rust"))

    # Three consumers, one value. The check that used to enforce this read
    # settings.rs with a regular expression from Python.
    def test_accent_disagreeing_with_the_first_preset_is_refused(self):
        tokens = self.minimal()
        tokens["accentPresets"] = ["#123456", "#3B82F6"]
        result = self.generate(tokens)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("accent-solid", result.stderr)

    def test_each_output_is_independent(self):
        result = self.generate(self.minimal(), "rust")
        self.assertEqual(result.returncode, 0)
        self.assertTrue(os.path.exists(self.path("out.rust")))
        self.assertFalse(os.path.exists(self.path("out.css")))

    def test_asking_for_nothing_is_an_error(self):
        source = self.write_json("tokens.json", self.minimal())
        self.assertNotEqual(run(TOKENS, "--tokens", source).returncode, 0)

    # anim.ts interpolates these per frame and needs numbers, not strings.
    # The hover tint in particular is a color-mix nobody can recompute by
    # eye, and it was wrong when it was written out by hand.
    def test_channel_values_and_the_derived_mix_are_generated(self):
        self.generate(self.minimal())
        ts = self.read("ts")
        self.assertIn("export const channels", ts)
        self.assertIn("ring: [192, 132, 252]", ts)
        self.assertIn("surfaceHover:", ts)

    def test_the_real_tokens_generate(self):
        result = run(
            TOKENS,
            "--tokens",
            os.path.join(UI_DIR, "tokens.json"),
            "--css",
            self.path("out.css"),
            "--ts",
            self.path("out.ts"),
            "--header",
            self.path("out.header"),
            "--rust",
            self.path("out.rust"),
        )
        self.assertEqual(result.returncode, 0, result.stderr)


def main() -> int:
    """Runs the tests, and writes a stamp when GN asked for one.

    A GN action has to produce the outputs it declares, so `--stamp` is how
    this becomes a build step rather than something someone remembers to
    run. Without it, `python3 generators_test.py` behaves like any other
    unittest file.
    """
    parser = argparse.ArgumentParser()
    parser.add_argument("--stamp")
    args, rest = parser.parse_known_args()

    result = unittest.main(argv=[sys.argv[0], *rest], exit=False).result
    if not result.wasSuccessful():
        return 1

    if args.stamp:
        os.makedirs(os.path.dirname(args.stamp) or ".", exist_ok=True)
        with open(args.stamp, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(f"{result.testsRun} generator tests passed\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
