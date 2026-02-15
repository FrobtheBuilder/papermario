#!/usr/bin/env python3
"""
Parses build.ninja to find all img_header build rules, then generates
corresponding .png.h header files with width/height defines read from the
actual PNG files.

Run from the project root directory:
    python tools/pc/gen_png_headers.py
"""

import os
import re
import struct
import sys


def read_png_dimensions(png_path):
    """Read width and height from a PNG file's IHDR chunk.

    The PNG format stores dimensions in the IHDR chunk which always starts
    at byte offset 16 (after the 8-byte signature and 8-byte chunk header).
    Width is at bytes 16-19 and height at bytes 20-23, both as big-endian
    unsigned 32-bit integers.
    """
    with open(png_path, "rb") as f:
        # PNG signature (8 bytes) + IHDR chunk length (4 bytes) + chunk type (4 bytes) = 16 bytes
        header = f.read(24)
        if len(header) < 24:
            raise ValueError(f"File too small to be a valid PNG: {png_path}")
        # Verify PNG signature
        if header[:8] != b"\x89PNG\r\n\x1a\n":
            raise ValueError(f"Not a valid PNG file: {png_path}")
        width = struct.unpack(">I", header[16:20])[0]
        height = struct.unpack(">I", header[20:24])[0]
    return width, height


def parse_build_ninja(ninja_path):
    """Parse build.ninja and extract all img_header build rules.

    Returns a list of (output_path, input_path, c_name) tuples.
    """
    rules = []

    with open(ninja_path, "r") as f:
        lines = f.readlines()

    i = 0
    while i < len(lines):
        line = lines[i].rstrip("\n")

        # Match: build <output>: img_header <input>
        m = re.match(r"^build\s+(.+?):\s+img_header\s+(.+)$", line)
        if m:
            output_path = m.group(1).strip()
            input_path = m.group(2).strip()

            # Remove order-only dependencies (anything after ||)
            if "||" in input_path:
                input_path = input_path.split("||")[0].strip()

            # Read the variable lines that follow the build statement
            c_name = None
            j = i + 1
            while j < len(lines):
                var_line = lines[j].rstrip("\n")
                # Variable lines are indented with spaces
                if not var_line.startswith("  "):
                    break
                var_m = re.match(r"^\s+c_name\s*=\s*(.+)$", var_line)
                if var_m:
                    c_name = var_m.group(1).strip()
                j += 1

            if c_name is not None:
                rules.append((output_path, input_path, c_name))

        i += 1

    return rules


def generate_header(c_name, width, height):
    """Generate the header file content for the given c_name and dimensions."""
    guard = f"_{c_name.upper()}_"
    return (
        f"// Generated file, do not edit.\n"
        f"#ifndef {guard}\n"
        f"#define {guard}\n"
        f"\n"
        f"#define {c_name}_width {width}\n"
        f"#define {c_name}_height {height}\n"
        f"\n"
        f"#endif\n"
    )


def main():
    # Determine project root (script is at tools/pc/gen_png_headers.py)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.normpath(os.path.join(script_dir, "..", ".."))

    ninja_path = os.path.join(project_root, "build.ninja")
    if not os.path.isfile(ninja_path):
        print(f"ERROR: build.ninja not found at {ninja_path}", file=sys.stderr)
        print("Please run this script from the project root or ensure build.ninja exists.", file=sys.stderr)
        sys.exit(1)

    print(f"Parsing {ninja_path} ...")
    rules = parse_build_ninja(ninja_path)
    print(f"Found {len(rules)} img_header build rules.")

    generated = 0
    skipped = 0

    for output_path, input_path, c_name in rules:
        # Resolve paths relative to project root
        abs_input = os.path.join(project_root, input_path)
        abs_output = os.path.join(project_root, output_path)

        if not os.path.isfile(abs_input):
            print(f"  WARNING: PNG not found, skipping: {input_path}")
            skipped += 1
            continue

        try:
            width, height = read_png_dimensions(abs_input)
        except (ValueError, IOError) as e:
            print(f"  WARNING: Could not read PNG {input_path}: {e}")
            skipped += 1
            continue

        # Create output directory if needed
        out_dir = os.path.dirname(abs_output)
        if out_dir:
            os.makedirs(out_dir, exist_ok=True)

        header_content = generate_header(c_name, width, height)

        with open(abs_output, "w") as f:
            f.write(header_content)

        generated += 1

    print()
    print(f"Summary:")
    print(f"  Total img_header rules: {len(rules)}")
    print(f"  Headers generated:      {generated}")
    print(f"  Skipped (missing PNG):  {skipped}")


if __name__ == "__main__":
    main()
