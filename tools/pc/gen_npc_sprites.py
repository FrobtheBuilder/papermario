#!/usr/bin/env python3
"""Generate NPC sprite headers from the decomp XML data.

This reads assets/us/sprite/npc.xml for sprite ordering, and each NPC's
SpriteSheet.xml for raster/palette/animation data, then generates one
header per NPC sprite in ver/us/build/include/sprite/npc/.

This replicates the output of tools/build/sprite/header.py without needing
to load any actual PNG image data.

Output: ver/us/build/include/sprite/npc/<SpriteName>.h (one per NPC)

Each header contains:
  - #define SPR_<Name> 0x<id>
  - SPR_IMG_<Name>_<img> defines for each raster
  - SPR_PAL_<Name>[_<pal>] defines for each palette
  - ANIM_<Name>_[<pal>_]<anim> defines for each palette x animation

Usage:
    python tools/pc/gen_npc_sprites.py
"""
import os
import sys
import xml.etree.ElementTree as ET


def generate_npc_header(sprite_name, sprite_id, sprite_xml):
    """Generate the header file content for a single NPC sprite."""
    lines = []

    guard = f"_NPC_SPRITE_{sprite_name.upper()}_H_"
    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}")
    lines.append("")
    lines.append('#include "types.h"')
    lines.append("")

    # Sprite ID define
    lines.append(f"#define SPR_{sprite_name} 0x{sprite_id:02X}")
    lines.append("")

    # Raster (image) defines
    image_names = []
    for idx, raster in enumerate(sprite_xml.findall("./RasterList/Raster")):
        img_name = raster.attrib["src"].split(".png")[0]
        image_names.append(img_name)
        lines.append(f"#define SPR_IMG_{sprite_name}_{img_name} 0x{idx:X}")
    lines.append("")

    # Palette defines
    palette_names = []
    for idx, palette in enumerate(sprite_xml.findall("./PaletteList/Palette")):
        pal_name = palette.get("name", palette.attrib["src"].split(".png")[0])
        palette_names.append(pal_name)
        if pal_name == "Default":
            lines.append(f"#define SPR_PAL_{sprite_name} 0x{idx:X}")
        else:
            lines.append(f"#define SPR_PAL_{sprite_name}_{pal_name} 0x{idx:X}")
    lines.append("")

    # Animation defines
    animation_names = []
    for anim in sprite_xml.findall("./AnimationList/Animation"):
        animation_names.append(anim.attrib["name"])

    for pal_idx, pal_name in enumerate(palette_names):
        for anim_idx, anim_name in enumerate(animation_names):
            if pal_name == "Default":
                define_name = f"ANIM_{sprite_name}_{anim_name}"
            else:
                define_name = f"ANIM_{sprite_name}_{pal_name}_{anim_name}"
            lines.append(f"#define {define_name} 0x{sprite_id:02X}{pal_idx:02X}{anim_idx:02X}")
        lines.append("")

    lines.append("#endif")
    lines.append("")

    return "\n".join(lines)


def main():
    project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    os.chdir(project_root)

    npc_xml_path = os.path.join("assets", "us", "sprite", "npc.xml")
    npc_dir = os.path.join("assets", "us", "sprite", "npc")
    output_dir = os.path.join("ver", "us", "build", "include", "sprite", "npc")

    os.makedirs(output_dir, exist_ok=True)

    # Read sprite ordering from npc.xml
    ordering_tree = ET.parse(npc_xml_path)
    root = ordering_tree.getroot()

    sprite_order = []
    for sprite_tag in root.find("Sprites"):
        sprite_order.append(sprite_tag.attrib["name"])

    print(f"  Processing {len(sprite_order)} NPC sprites...")

    generated = 0
    skipped = 0

    for idx, sprite_name in enumerate(sprite_order):
        sprite_id = idx + 1  # 1-indexed

        sprite_sheet_path = os.path.join(npc_dir, sprite_name, "SpriteSheet.xml")
        if not os.path.isfile(sprite_sheet_path):
            print(f"  WARNING: SpriteSheet.xml not found for {sprite_name}, skipping")
            skipped += 1
            continue

        try:
            tree = ET.parse(sprite_sheet_path)
            sprite_xml = tree.getroot()
        except ET.ParseError as e:
            print(f"  WARNING: Failed to parse {sprite_sheet_path}: {e}", file=sys.stderr)
            skipped += 1
            continue

        header_content = generate_npc_header(sprite_name, sprite_id, sprite_xml)

        output_path = os.path.join(output_dir, f"{sprite_name}.h")
        with open(output_path, "w", newline="\n") as f:
            f.write(header_content)

        generated += 1

    print(f"  Generated {generated} NPC sprite headers ({skipped} skipped)")


if __name__ == "__main__":
    main()
