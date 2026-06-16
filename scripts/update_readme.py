#!/usr/bin/env python3
"""Splice a results Markdown file into README.md between the RESULTS markers.

Usage: python3 scripts/update_readme.py results.md
"""
import pathlib
import sys

START = "<!-- RESULTS:START -->"
END = "<!-- RESULTS:END -->"

results = pathlib.Path(sys.argv[1]).read_text()
readme_path = pathlib.Path("README.md")
text = readme_path.read_text()

if START not in text or END not in text:
    sys.exit("README.md is missing the RESULTS markers")

head = text[: text.index(START) + len(START)]
tail = text[text.index(END):]
readme_path.write_text(f"{head}\n{results}\n{tail}")
print("README.md results updated")
