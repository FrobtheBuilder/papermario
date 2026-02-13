#!/usr/bin/env python3
"""Standalone script to generate sprite/player.h without building the full sprite binary.
Only needs the XML metadata and PNG files for raster size calculation."""

import os
import sys
import struct
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Dict, List, Tuple

# Add parent dirs to path so we can import shared helpers
TOOLS_DIR = Path(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, str(TOOLS_DIR))
sys.path.insert(0, str(TOOLS_DIR.parent))

from common import get_asset_path

try:
    import png
except ImportError:
    print("ERROR: pypng not installed. Run: pip install pypng", file=sys.stderr)
    sys.exit(1)


PLAYER_SPRITE_METADATA_XML_FILENAME = "player.xml"
HAS_BACK_XML = "hasBack"


def iter_in_groups(iterable, n):
    it = iter(iterable)
    while True:
        group = []
        try:
            for _ in range(n):
                group.append(next(it))
        except StopIteration:
            if group:
                yield tuple(group)
            return
        yield tuple(group)


def get_player_sprite_metadata(asset_stack: Tuple[Path, ...]):
    orderings_tree = ET.parse(get_asset_path(Path("sprite") / PLAYER_SPRITE_METADATA_XML_FILENAME, asset_stack))
    sprite_order = []
    for sprite_tag in orderings_tree.getroot()[1]:
        sprite_order.append(sprite_tag.attrib["name"])
    raster_order = []
    for raster_tag in orderings_tree.getroot()[2]:
        raster_order.append(raster_tag.attrib["name"])
    return sprite_order, raster_order


def get_raster_sizes(raster_order: List[str], asset_stack: Tuple[Path, ...]) -> Dict[str, int]:
    """Get width*height//2 for each raster (CI4 size)."""
    sizes = {}
    for raster_name in raster_order:
        png_path = get_asset_path(Path(f"sprite/player/rasters/{raster_name}.png"), asset_stack)
        if os.path.getsize(png_path) == 0x10:
            # Special raster
            sizes[raster_name] = 0x10
        else:
            with open(png_path, "rb") as f:
                reader = png.Reader(f)
                width, height, _, _ = reader.read()
                sizes[raster_name] = width * height // 2
    return sizes


def write_player_sprite_header(
    sprite_order: List[str],
    asset_stack: Tuple[Path, ...],
    raster_sizes: Dict[str, int],
    out_file: Path,
):
    ifdef_name = "_PLAYER_SPRITE_H_"

    sprite_id = 1
    player_sprites: Dict[str, int] = {}
    player_rasters: Dict[str, Dict[str, int]] = {}
    player_palettes: Dict[str, Dict[str, int]] = {}
    player_anims: Dict[str, Dict[str, int]] = {}
    max_sprite_sizes: Dict[str, int] = {}

    for sprite_name in sprite_order:
        sprite_xml = ET.parse(get_asset_path(Path(f"sprite/player/{sprite_name}.xml"), asset_stack)).getroot()
        has_back = sprite_xml.attrib[HAS_BACK_XML] == "true"

        anim_elems = sprite_xml.findall("./AnimationList/Animation")
        img_elems = sprite_xml.findall("./RasterList/Raster")
        pal_elems = sprite_xml.findall("./PaletteList/Palette")

        player_sprites[f"SPR_{sprite_name}"] = sprite_id
        player_rasters[sprite_name] = {}
        player_palettes[sprite_name] = {}
        player_anims[sprite_name] = {}

        for palette_xml in pal_elems:
            palette_id = int(palette_xml.attrib["id"], 0x10)
            palette_name = palette_xml.attrib["name"]
            player_palettes[sprite_name][f"SPR_PAL_{sprite_name}_{palette_name}"] = palette_id

            for anim_id, anim_xml in enumerate(anim_elems):
                anim_name = anim_xml.attrib["name"]
                if palette_id > 0:
                    anim_name = f"{palette_name}_{anim_name}"
                player_anims[sprite_name][f"ANIM_{sprite_name}_{anim_name}"] = (
                    (sprite_id << 16) | (palette_id << 8) | anim_id
                )

        max_size = 0
        for raster_xml in img_elems:
            raster_id = int(raster_xml.attrib["id"], 0x10)
            raster_name_attr = raster_xml.attrib["name"]
            player_rasters[sprite_name][f"SPR_IMG_{sprite_name}_{raster_name_attr}"] = raster_id

            raster_src = raster_xml.attrib["src"][:-4]  # strip .png
            if raster_src in raster_sizes:
                size = raster_sizes[raster_src]
                if max_size < size:
                    max_size = size
        max_sprite_sizes[sprite_name] = max_size

        sprite_id += 1

        if has_back:
            player_sprites[f"SPR_{sprite_name}_Back"] = sprite_id

            max_size = 0
            for raster_xml in img_elems:
                if "back" in raster_xml.attrib:
                    raster_src = raster_xml.attrib["back"][:-4]
                    if raster_src in raster_sizes:
                        size = raster_sizes[raster_src]
                        if max_size < size:
                            max_size = size
            max_sprite_sizes[f"{sprite_name}_Back"] = max_size

            sprite_id += 1

    out_file.parent.mkdir(exist_ok=True, parents=True)
    with open(out_file, "w") as f:
        f.write(f"#ifndef {ifdef_name}\n")
        f.write(f"#define {ifdef_name}\n\n")

        f.write("enum PlayerSprites {\n")
        for spr_name, spr_id in player_sprites.items():
            f.write(f"    {spr_name} = 0x{spr_id:X},\n")
        f.write("};\n\n")

        for spr_name in max_sprite_sizes:
            f.write(f"#define MAX_IMG_{spr_name} 0x{max_sprite_sizes[spr_name]:04X}\n")
        f.write("\n")

        for spr_name in sprite_order:
            f.write(f"// {spr_name}\n")

            for raster_name, raster_id in player_rasters[spr_name].items():
                f.write(f"#define {raster_name} 0x{raster_id:02X}\n")
            f.write("\n")

            for palette_name, palette_id in player_palettes[spr_name].items():
                f.write(f"#define {palette_name} 0x{palette_id:02X}\n")
            f.write("\n")

            for anim_name, anim_id in player_anims[spr_name].items():
                f.write(f"#define {anim_name} 0x{anim_id:X}\n")
            f.write("\n")

        f.write(f"#endif // {ifdef_name}\n")


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Generate sprite/player.h header only")
    parser.add_argument("out_header", type=Path, help="output header file path")
    parser.add_argument("asset_stack", help="comma-separated asset stack (e.g. 'us')")
    args = parser.parse_args()

    asset_stack = tuple(Path(d) for d in args.asset_stack.split(","))
    sprite_order, raster_order = get_player_sprite_metadata(asset_stack)
    raster_sizes = get_raster_sizes(raster_order, asset_stack)
    write_player_sprite_header(sprite_order, asset_stack, raster_sizes, args.out_header)
    print(f"Generated {args.out_header}")


if __name__ == "__main__":
    main()
