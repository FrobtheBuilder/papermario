#!/usr/bin/env python3
"""
Converts PNG assets to N64 raw binary format and generates C source files
with the data as arrays, replacing the zero-initialized stubs.

Parses build.ninja for format info (ci4, rgba16, ia8, etc.) and source code
for INCLUDE_IMG/INCLUDE_PAL symbol names.

Usage:
    python tools/pc/gen_asset_data.py [--output-dir src/pc/gen]
"""

import os
import re
import struct
import sys
import argparse
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow is required. Install with: pip install Pillow", file=sys.stderr)
    sys.exit(1)


# =============================================================================
# N64 format converters
# =============================================================================

def convert_rgba16(img):
    """Convert RGBA image to N64 RGBA16 (5:5:5:1) big-endian bytes."""
    img = img.convert("RGBA")
    pixels = list(img.getdata())
    out = bytearray()
    for r, g, b, a in pixels:
        r5 = (r >> 3) & 0x1F
        g5 = (g >> 3) & 0x1F
        b5 = (b >> 3) & 0x1F
        a1 = 1 if a >= 128 else 0
        val = (r5 << 11) | (g5 << 6) | (b5 << 1) | a1
        out += struct.pack(">H", val)
    return bytes(out)


def convert_rgba32(img):
    """Convert RGBA image to N64 RGBA32 (8:8:8:8) bytes."""
    img = img.convert("RGBA")
    pixels = list(img.getdata())
    out = bytearray()
    for r, g, b, a in pixels:
        out += bytes([r, g, b, a])
    return bytes(out)


def convert_ia16(img):
    """Convert to IA16 (I8:A8) bytes."""
    img = img.convert("LA")
    pixels = list(img.getdata())
    out = bytearray()
    for intensity, alpha in pixels:
        out += bytes([intensity, alpha])
    return bytes(out)


def convert_ia8(img):
    """Convert to IA8 (I4:A4) - one byte per pixel."""
    img = img.convert("LA")
    pixels = list(img.getdata())
    out = bytearray()
    for intensity, alpha in pixels:
        i4 = (intensity >> 4) & 0xF
        a4 = (alpha >> 4) & 0xF
        out.append((i4 << 4) | a4)
    return bytes(out)


def convert_ia4(img):
    """Convert to IA4 (I3:A1) - 2 pixels per byte."""
    img = img.convert("LA")
    pixels = list(img.getdata())
    out = bytearray()
    for idx in range(0, len(pixels), 2):
        i0, a0 = pixels[idx]
        i3_0 = (i0 >> 5) & 0x7
        a1_0 = 1 if a0 >= 128 else 0
        nib0 = (i3_0 << 1) | a1_0

        if idx + 1 < len(pixels):
            i1, a1 = pixels[idx + 1]
            i3_1 = (i1 >> 5) & 0x7
            a1_1 = 1 if a1 >= 128 else 0
            nib1 = (i3_1 << 1) | a1_1
        else:
            nib1 = 0

        out.append((nib0 << 4) | nib1)
    return bytes(out)


def convert_i8(img):
    """Convert to I8 (8-bit intensity)."""
    img = img.convert("L")
    return bytes(img.getdata())


def convert_i4(img):
    """Convert to I4 (4-bit intensity) - 2 pixels per byte."""
    img = img.convert("L")
    pixels = list(img.getdata())
    out = bytearray()
    for idx in range(0, len(pixels), 2):
        p0 = (pixels[idx] >> 4) & 0xF
        p1 = (pixels[idx + 1] >> 4) & 0xF if idx + 1 < len(pixels) else 0
        out.append((p0 << 4) | p1)
    return bytes(out)


def convert_i1(img):
    """Convert to I1 (1-bit intensity) - 8 pixels per byte."""
    img = img.convert("L")
    pixels = list(img.getdata())
    out = bytearray()
    for idx in range(0, len(pixels), 8):
        byte = 0
        for bit in range(8):
            if idx + bit < len(pixels):
                if pixels[idx + bit] >= 128:
                    byte |= (1 << (7 - bit))
        out.append(byte)
    return bytes(out)


def get_palette_and_indices(img, max_colors):
    """Extract or generate palette and indices from image.
    Returns (palette_rgba5551_bytes, index_data).
    """
    w, h = img.size

    if img.mode == "P":
        # Already paletted - use existing palette
        pal_data = img.getpalette()  # flat list [R,G,B, R,G,B, ...]
        if pal_data is None:
            img = img.convert("RGBA")
        else:
            # Check if image has transparency
            if img.info.get("transparency") is not None:
                img_rgba = img.convert("RGBA")
            else:
                img_rgba = img.convert("RGBA")

            # Get actual palette entries used
            indices = list(img.getdata())

            # Build palette as RGBA from the P palette
            pal_rgba = []
            num_entries = min(len(pal_data) // 3, max_colors)
            rgba_pixels = list(img_rgba.getdata())

            # Build a full palette from the RGBA conversion
            # First, find unique colors and map indices
            palette_map = {}
            final_palette = []
            final_indices = []

            for pixel_idx, ci in enumerate(indices):
                if ci >= max_colors:
                    ci = 0
                rgba = rgba_pixels[pixel_idx] if pixel_idx < len(rgba_pixels) else (0, 0, 0, 0)
                if ci not in palette_map:
                    palette_map[ci] = rgba
                final_indices.append(ci)

            # Build palette array (up to max_colors entries)
            for i in range(max_colors):
                if i in palette_map:
                    final_palette.append(palette_map[i])
                elif i < num_entries:
                    r = pal_data[i * 3]
                    g = pal_data[i * 3 + 1]
                    b = pal_data[i * 3 + 2]
                    final_palette.append((r, g, b, 255))
                else:
                    final_palette.append((0, 0, 0, 0))

            # Convert palette to RGBA5551 big-endian
            pal_bytes = bytearray()
            for r, g, b, a in final_palette:
                r5 = (r >> 3) & 0x1F
                g5 = (g >> 3) & 0x1F
                b5 = (b >> 3) & 0x1F
                a1 = 1 if a >= 128 else 0
                val = (r5 << 11) | (g5 << 6) | (b5 << 1) | a1
                pal_bytes += struct.pack(">H", val)

            return bytes(pal_bytes), final_indices

    # Not paletted - need to quantize
    img = img.convert("RGBA")
    pixels = list(img.getdata())

    # Use PIL's built-in quantization
    try:
        quant = img.quantize(colors=max_colors, method=Image.Quantize.MEDIANCUT)
    except Exception:
        quant = img.quantize(colors=max_colors)

    pal_data = quant.getpalette()
    indices = list(quant.getdata())

    # Build palette
    pal_bytes = bytearray()
    num_entries = min(len(pal_data) // 3, max_colors)
    for i in range(max_colors):
        if i < num_entries:
            r = pal_data[i * 3]
            g = pal_data[i * 3 + 1]
            b = pal_data[i * 3 + 2]
            # Try to preserve alpha from original
            # Find a pixel that maps to this palette entry
            a = 255
            for pi, ci in enumerate(indices):
                if ci == i and pi < len(pixels):
                    a = pixels[pi][3]
                    break
            r5 = (r >> 3) & 0x1F
            g5 = (g >> 3) & 0x1F
            b5 = (b >> 3) & 0x1F
            a1 = 1 if a >= 128 else 0
        else:
            r5 = g5 = b5 = a1 = 0
        val = (r5 << 11) | (g5 << 6) | (b5 << 1) | a1
        pal_bytes += struct.pack(">H", val)

    return bytes(pal_bytes), indices


def convert_ci4(img):
    """Convert to CI4 (4-bit color indexed) - 2 pixels per byte."""
    pal_bytes, indices = get_palette_and_indices(img, 16)
    out = bytearray()
    for idx in range(0, len(indices), 2):
        hi = indices[idx] & 0xF
        lo = (indices[idx + 1] & 0xF) if idx + 1 < len(indices) else 0
        out.append((hi << 4) | lo)
    return bytes(out), pal_bytes


def convert_ci8(img):
    """Convert to CI8 (8-bit color indexed)."""
    pal_bytes, indices = get_palette_and_indices(img, 256)
    out = bytearray(idx & 0xFF for idx in indices)
    return bytes(out), pal_bytes


def convert_palette(img):
    """Extract just the palette from a CI image as RGBA5551."""
    if img.mode == "P":
        pal_data = img.getpalette()
        if pal_data:
            num_entries = len(pal_data) // 3
            # Check for transparency
            img_rgba = img.convert("RGBA")
            rgba_pixels = list(img_rgba.getdata())
            ci_pixels = list(img.getdata())

            # Build alpha per palette entry
            alpha_map = {}
            for pi, ci in enumerate(ci_pixels):
                if ci not in alpha_map and pi < len(rgba_pixels):
                    alpha_map[ci] = rgba_pixels[pi][3]

            pal_bytes = bytearray()
            for i in range(num_entries):
                r = pal_data[i * 3]
                g = pal_data[i * 3 + 1]
                b = pal_data[i * 3 + 2]
                a = alpha_map.get(i, 255)
                r5 = (r >> 3) & 0x1F
                g5 = (g >> 3) & 0x1F
                b5 = (b >> 3) & 0x1F
                a1 = 1 if a >= 128 else 0
                val = (r5 << 11) | (g5 << 6) | (b5 << 1) | a1
                pal_bytes += struct.pack(">H", val)
            return bytes(pal_bytes)

    # Fallback: quantize to 256 colors
    pal_bytes, _ = get_palette_and_indices(img, 256)
    return pal_bytes


# =============================================================================
# build.ninja parser
# =============================================================================

def parse_build_ninja(ninja_path):
    """Parse build.ninja to find all pigment/img rules with their img_type.
    Returns dict: output_bin_path -> (input_png_path, img_type)
    """
    rules = {}
    with open(ninja_path, "r") as f:
        lines = f.readlines()

    i = 0
    while i < len(lines):
        line = lines[i].rstrip("\n")
        # Match: build <output>: pigment <input>
        # or:    build <output>: img <input>
        m = re.match(r"^build\s+(.+?):\s+(?:pigment|img)\s+(.+)$", line)
        if m:
            output_path = m.group(1).strip()
            input_path = m.group(2).strip()
            if "||" in input_path:
                input_path = input_path.split("||")[0].strip()

            # Read variable lines
            img_type = None
            j = i + 1
            while j < len(lines):
                var_line = lines[j].rstrip("\n")
                if not var_line.startswith("  "):
                    break
                var_m = re.match(r"^\s+img_type\s*=\s*(.+)$", var_line)
                if var_m:
                    img_type = var_m.group(1).strip()
                j += 1

            if img_type:
                rules[output_path] = (input_path, img_type)

        i += 1
    return rules


def parse_source_includes(project_root):
    """Parse all source files for INCLUDE_IMG/INCLUDE_PAL/INCLUDE_RAW calls.
    Returns list of (macro_type, file_path, symbol_name, source_file).
    """
    includes = []
    pattern = re.compile(
        r'INCLUDE_(IMG|PAL|RAW)\s*\(\s*"([^"]+)"\s*,\s*(\w+)\s*\)'
    )

    src_dir = os.path.join(project_root, "src")
    for root, dirs, files in os.walk(src_dir):
        for fname in files:
            if not fname.endswith((".c", ".h")):
                continue
            filepath = os.path.join(root, fname)
            try:
                with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
                    for line in f:
                        m = pattern.search(line)
                        if m:
                            macro_type = m.group(1)  # IMG, PAL, or RAW
                            file_path = m.group(2)   # e.g., "battle/action_cmd/thing.png"
                            symbol = m.group(3)       # e.g., battle_action_cmd_thing_png
                            includes.append((macro_type, file_path, symbol, filepath))
            except IOError:
                pass

    return includes


def resolve_asset(include_entry, ninja_rules, project_root):
    """Resolve an INCLUDE_IMG/PAL entry to its PNG path and format type.
    Returns (png_path, img_type) or None.
    """
    macro_type, file_path, symbol, source_file = include_entry

    # The INCLUDE_IMG file path is relative (e.g., "battle/action_cmd/thing.png")
    # The build.ninja output is "ver/us/build/<file_path>.bin"
    # The input PNG is "assets/us/<file_path>"

    if macro_type == "PAL":
        # Palette file: look for .pal.bin rule
        bin_path = f"ver/us/build/{file_path}.bin"
        # The input file for palette is the corresponding .png (not .pal)
        # e.g., "battle/action_cmd/thing.pal" -> PNG is "battle/action_cmd/thing.png"
        png_rel = file_path.replace(".pal", ".png")
    elif macro_type == "IMG":
        bin_path = f"ver/us/build/{file_path}.bin"
        png_rel = file_path
    elif macro_type == "RAW":
        # RAW includes binary data directly - skip for now
        return None
    else:
        return None

    # Look up in ninja rules
    if bin_path in ninja_rules:
        input_png, img_type = ninja_rules[bin_path]
        png_path = os.path.join(project_root, input_png)
        if os.path.isfile(png_path):
            return (png_path, img_type, symbol)
    else:
        # Try to find the PNG directly
        png_path = os.path.join(project_root, "assets", "us", png_rel)
        if os.path.isfile(png_path):
            # Guess format from symbol name and file extension
            img_type = guess_format(file_path, macro_type)
            if img_type:
                return (png_path, img_type, symbol)

    return None


def guess_format(file_path, macro_type):
    """Guess N64 format from file path patterns."""
    if macro_type == "PAL":
        return "palette"

    # Common patterns in Paper Mario
    path_lower = file_path.lower()
    if "rgba32" in path_lower:
        return "rgba32"
    if "rgba16" in path_lower:
        return "rgba16"
    if "ci8" in path_lower:
        return "ci8"
    if "ci4" in path_lower:
        return "ci4"
    if "ia16" in path_lower:
        return "ia16"
    if "ia8" in path_lower:
        return "ia8"
    if "ia4" in path_lower:
        return "ia4"
    if "i8" in path_lower:
        return "i8"
    if "i4" in path_lower:
        return "i4"

    # Default based on common usage
    return "ci4"  # Most UI elements in Paper Mario are CI4


def convert_image(png_path, img_type):
    """Convert a PNG file to N64 raw binary format.
    Returns (image_data, palette_data_or_None).
    """
    try:
        img = Image.open(png_path)
    except Exception as e:
        print(f"  WARNING: Cannot open {png_path}: {e}", file=sys.stderr)
        return None, None

    try:
        if img_type == "rgba16":
            return convert_rgba16(img), None
        elif img_type == "rgba32":
            return convert_rgba32(img), None
        elif img_type == "ia16":
            return convert_ia16(img), None
        elif img_type == "ia8":
            return convert_ia8(img), None
        elif img_type == "ia4":
            return convert_ia4(img), None
        elif img_type == "i8":
            return convert_i8(img), None
        elif img_type == "i4":
            return convert_i4(img), None
        elif img_type == "i1":
            return convert_i1(img), None
        elif img_type == "ci4":
            data, pal = convert_ci4(img)
            return data, pal
        elif img_type == "ci8":
            data, pal = convert_ci8(img)
            return data, pal
        elif img_type == "palette":
            return convert_palette(img), None
        elif img_type == "bg":
            # Background images - typically ia8 or rgba16
            return convert_ia8(img), None
        elif img_type == "party":
            # Party images - typically rgba32
            return convert_rgba32(img), None
        else:
            print(f"  WARNING: Unknown img_type '{img_type}', using rgba16", file=sys.stderr)
            return convert_rgba16(img), None
    except Exception as e:
        print(f"  WARNING: Conversion failed for {png_path} ({img_type}): {e}", file=sys.stderr)
        return None, None


def bytes_to_c_array(data, symbol_name, is_palette=False):
    """Generate C array definition from raw bytes."""
    if is_palette:
        dtype = "unsigned short"
        # Convert byte pairs to u16 values
        vals = []
        for i in range(0, len(data), 2):
            val = (data[i] << 8) | data[i + 1] if i + 1 < len(data) else data[i] << 8
            vals.append(f"0x{val:04X}")
        body = ", ".join(vals)
        return f"{dtype} {symbol_name}[] = {{{body}}};\n"
    else:
        dtype = "unsigned char"
        # Output as hex bytes, 16 per line
        lines = []
        for i in range(0, len(data), 16):
            chunk = data[i:i+16]
            hex_vals = ", ".join(f"0x{b:02X}" for b in chunk)
            lines.append(f"    {hex_vals},")
        body = "\n".join(lines)
        return f"{dtype} {symbol_name}[] = {{\n{body}\n}};\n"


def main():
    parser = argparse.ArgumentParser(description="Generate N64 asset data for PC port")
    parser.add_argument("--output-dir", default="src/pc/gen",
                        help="Output directory for generated C files")
    parser.add_argument("--max-assets", type=int, default=0,
                        help="Max number of assets to process (0 = all)")
    args = parser.parse_args()

    # Determine project root
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.normpath(os.path.join(script_dir, "..", ".."))

    ninja_path = os.path.join(project_root, "build.ninja")
    if not os.path.isfile(ninja_path):
        print("ERROR: build.ninja not found. Run ./configure.py first.", file=sys.stderr)
        sys.exit(1)

    print("=== Paper Mario PC Port - Asset Data Generator ===")
    print(f"Project root: {project_root}")
    print()

    # Step 1: Parse build.ninja
    print("Parsing build.ninja for image format rules...")
    ninja_rules = parse_build_ninja(ninja_path)
    print(f"  Found {len(ninja_rules)} image conversion rules")

    # Step 2: Parse source code for INCLUDE_IMG/PAL/RAW
    print("Scanning source code for asset includes...")
    source_includes = parse_source_includes(project_root)
    print(f"  Found {len(source_includes)} INCLUDE_IMG/PAL/RAW references")

    # Step 3: Resolve each include to a PNG + format
    print("Resolving asset paths and formats...")
    resolved = []
    for entry in source_includes:
        result = resolve_asset(entry, ninja_rules, project_root)
        if result:
            resolved.append(result)

    print(f"  Resolved {len(resolved)} assets with PNG files")

    if args.max_assets > 0:
        resolved = resolved[:args.max_assets]
        print(f"  (limited to {args.max_assets} assets)")

    # Step 4: Convert and generate C files
    output_dir = os.path.join(project_root, args.output_dir)
    os.makedirs(output_dir, exist_ok=True)

    # Group by batches to avoid huge files
    BATCH_SIZE = 200
    num_batches = (len(resolved) + BATCH_SIZE - 1) // BATCH_SIZE

    converted = 0
    failed = 0
    symbols_done = set()  # Avoid duplicate symbols

    for batch_idx in range(num_batches):
        batch_start = batch_idx * BATCH_SIZE
        batch_end = min(batch_start + BATCH_SIZE, len(resolved))
        batch = resolved[batch_start:batch_end]

        output_file = os.path.join(output_dir, f"asset_data_{batch_idx:03d}.c")

        with open(output_file, "w") as f:
            f.write(f"// Auto-generated asset data (batch {batch_idx})\n")
            f.write("// Generated by tools/pc/gen_asset_data.py\n\n")

            for png_path, img_type, symbol in batch:
                if symbol in symbols_done:
                    continue
                symbols_done.add(symbol)

                data, pal = convert_image(png_path, img_type)
                if data is None or len(data) == 0:
                    failed += 1
                    continue

                # Determine if this is a palette symbol
                is_pal_symbol = symbol.endswith("_pal")

                if img_type == "palette" or is_pal_symbol:
                    # Palette data
                    if img_type == "palette":
                        f.write(bytes_to_c_array(data, symbol, is_palette=True))
                    else:
                        f.write(bytes_to_c_array(data, symbol, is_palette=False))
                elif img_type in ("ci4", "ci8"):
                    # CI format: write image data
                    f.write(bytes_to_c_array(data, symbol))
                else:
                    # Non-CI format: write image data
                    f.write(bytes_to_c_array(data, symbol))

                converted += 1

                if converted % 100 == 0:
                    print(f"  Converted {converted} assets...")

        print(f"  Wrote batch {batch_idx}: {output_file}")

    print()
    print(f"=== Summary ===")
    print(f"  Total resolved: {len(resolved)}")
    print(f"  Converted: {converted}")
    print(f"  Failed: {failed}")
    print(f"  Output directory: {output_dir}")
    print()

    # Generate a CMake include file listing the generated sources
    cmake_file = os.path.join(output_dir, "CMakeLists_assets.cmake")
    with open(cmake_file, "w") as f:
        f.write("# Auto-generated by gen_asset_data.py\n")
        f.write("set(GENERATED_ASSET_SOURCES\n")
        for batch_idx in range(num_batches):
            src_path = f"src/pc/gen/asset_data_{batch_idx:03d}.c"
            f.write(f"    {src_path}\n")
        f.write(")\n")

    print(f"  CMake include: {cmake_file}")
    print()
    print("Add to CMakeLists.txt:")
    print('  include(src/pc/gen/CMakeLists_assets.cmake)')
    print('  target_sources(papermario PRIVATE ${GENERATED_ASSET_SOURCES})')


if __name__ == "__main__":
    main()
