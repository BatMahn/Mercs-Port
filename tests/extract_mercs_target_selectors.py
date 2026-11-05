#!/usr/bin/env python3
"""Extract the real frontend target gates for a headless consumer regression.

No gate is reimplemented here. A generated include holds the three exact C
function bodies and the hash of the tools/glview.c input that supplied them.
"""
import argparse
import hashlib
import re
from pathlib import Path


def extract(text, name):
    match = re.search(r"^static int " + re.escape(name) + r"\([^\n]*\)\s*\{", text, re.M)
    if match is None:
        raise ValueError(f"Required frontend function not found: {name}")
    pos = text.index("{", match.start()) + 1
    depth, state = 1, "code"
    while pos < len(text) and depth:
        char = text[pos]
        pair = text[pos:pos+2]
        if state == "line":
            if char == "\n":
                state = "code"
        elif state == "block":
            if pair == "*/":
                state = "code"
                pos += 1
        elif state in ("\"", "'"):
            if char == "\\":
                pos += 1
            elif char == state:
                state = "code"
        elif pair == "//":
            state = "line"
            pos += 1
        elif pair == "/*":
            state = "block"
            pos += 1
        elif char in ("\"", "'"):
            state = char
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
        pos += 1
    if depth:
        raise ValueError(f"Unterminated frontend function: {name}")
    return text[match.start():pos] + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    raw = args.source.read_bytes()
    text = raw.decode("utf-8")
    body = "\n".join(extract(text, name) for name in
                     ("thing_targetable", "unit_side", "unit_targetable"))
    result = ("/* Generated verbatim from tools/glview.c.\n"
              f" * Source SHA256 {hashlib.sha256(raw).hexdigest()}. */\n"
              "#ifndef MERCS_AIRCRAFT_SELECTOR_FIXTURE_H\n"
              "#define MERCS_AIRCRAFT_SELECTOR_FIXTURE_H\n" + body + "#endif\n")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(result, encoding="utf-8")


if __name__ == "__main__":
    main()
