#!/usr/bin/env python3
"""Post-process move_data.inc.c to replace (s32) "string" with SCRIPT_CAST("string").

The decomp build tools generate move_data.inc.c with (s32) casts on string
literals. The PC port needs these to be SCRIPT_CAST() instead, which is a
macro that handles the cast in a portable way.

Usage:
    python tools/pc/fix_move_data.py ver/us/build/include/move_data.inc.c
"""
import re
import sys


def main():
    if len(sys.argv) < 2:
        print("usage: fix_move_data.py <move_data.inc.c>", file=sys.stderr)
        sys.exit(1)

    filepath = sys.argv[1]

    with open(filepath, "r", encoding="utf-8") as f:
        content = f.read()

    # Replace (s32) "string_literal" with SCRIPT_CAST("string_literal")
    content = re.sub(r'\(s32\)\s*("(?:[^"\\]|\\.)*")', r'SCRIPT_CAST(\1)', content)

    with open(filepath, "w", encoding="utf-8") as f:
        f.write(content)

    print(f"  Post-processed {filepath}")


if __name__ == "__main__":
    main()
