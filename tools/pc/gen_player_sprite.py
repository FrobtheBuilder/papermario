#!/usr/bin/env python3
"""Generate sprite/player.h from the player sprite XML data.

This reads the decomp's player sprite XML files and generates the same
header that the full build process (tools/build/sprite/sprites.py) would
produce, but without needing to read actual PNG raster data or compress
anything. Only the header definitions are generated.

Output: ver/us/build/include/sprite/player.h

The header contains:
  - enum PlayerSprites (sprite sheet IDs, including _Back variants)
  - MAX_IMG_* defines (max raster size per sprite; we use 0x1000 as placeholder)
  - SPR_IMG_* defines (raster indices per sprite sheet)
  - SPR_PAL_* defines (palette indices per sprite sheet)
  - ANIM_* defines (animation IDs: sprite_id << 16 | palette_id << 8 | anim_id)

Usage:
    python tools/pc/gen_player_sprite.py
"""
import os
import sys
import xml.etree.ElementTree as ET


def main():
    project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    os.chdir(project_root)

    asset_dir = os.path.join("assets", "us", "sprite", "player")
    output_file = os.path.join("ver", "us", "build", "include", "sprite", "player.h")

    # Read the player sprite ordering from player.xml
    player_xml_path = os.path.join("assets", "us", "sprite", "player.xml")
    ordering_tree = ET.parse(player_xml_path)
    root = ordering_tree.getroot()

    sprite_order = []
    for sprite_tag in root.find("Sprites"):
        sprite_order.append(sprite_tag.attrib["name"])

    os.makedirs(os.path.dirname(output_file), exist_ok=True)

    # Parse each sprite sheet XML
    sprite_data = {}
    for sprite_name in sprite_order:
        xml_path = os.path.join(asset_dir, f"{sprite_name}.xml")
        tree = ET.parse(xml_path)
        sprite_xml = tree.getroot()

        has_back = sprite_xml.attrib.get("hasBack", "false") == "true"

        # Palettes
        palettes = []
        for pal in sprite_xml.findall("./PaletteList/Palette"):
            pal_id = int(pal.attrib["id"], 16)
            pal_name = pal.attrib.get("name", pal.attrib["src"].split(".png")[0])
            palettes.append((pal_id, pal_name))

        # Rasters
        rasters = []
        for raster in sprite_xml.findall("./RasterList/Raster"):
            raster_id = int(raster.attrib["id"], 16)
            raster_name = raster.attrib.get("name", raster.attrib["src"].split(".png")[0])
            rasters.append((raster_id, raster_name))

        # Animations
        animations = []
        for anim in sprite_xml.findall("./AnimationList/Animation"):
            animations.append(anim.attrib["name"])

        sprite_data[sprite_name] = {
            "has_back": has_back,
            "palettes": palettes,
            "rasters": rasters,
            "animations": animations,
        }

    # Build sprite ID assignments (matching sprites.py logic)
    sprite_id = 1
    player_sprites = {}  # name -> id
    for sprite_name in sprite_order:
        data = sprite_data[sprite_name]
        player_sprites[f"SPR_{sprite_name}"] = sprite_id
        sprite_id += 1
        if data["has_back"]:
            player_sprites[f"SPR_{sprite_name}_Back"] = sprite_id
            sprite_id += 1

    # Write the header
    with open(output_file, "w", newline="\n") as f:
        f.write("#ifndef _PLAYER_SPRITE_H_\n")
        f.write("#define _PLAYER_SPRITE_H_\n")
        f.write("\n")

        # PlayerSprites enum
        f.write("enum PlayerSprites {\n")
        for name, sid in player_sprites.items():
            f.write(f"    {name} = 0x{sid:X},\n")
        f.write("};\n")
        f.write("\n")

        # MAX_IMG defines
        for name in player_sprites:
            f.write(f"#define MAX_IMG_{name[4:]} 0x1000\n")
        f.write("\n")

        # Per-sprite definitions
        for sprite_name in sprite_order:
            data = sprite_data[sprite_name]
            sid = player_sprites[f"SPR_{sprite_name}"]

            f.write(f"// {sprite_name}\n")

            # SPR_IMG defines
            for raster_id, raster_name in data["rasters"]:
                f.write(f"#define SPR_IMG_{sprite_name}_{raster_name} 0x{raster_id:02X}\n")
            f.write("\n")

            # SPR_PAL defines
            for pal_id, pal_name in data["palettes"]:
                f.write(f"#define SPR_PAL_{sprite_name}_{pal_name} 0x{pal_id:02X}\n")
            f.write("\n")

            # ANIM defines: for each palette x animation
            for pal_id, pal_name in data["palettes"]:
                for anim_idx, anim_name in enumerate(data["animations"]):
                    if pal_id > 0:
                        full_name = f"ANIM_{sprite_name}_{pal_name}_{anim_name}"
                    else:
                        full_name = f"ANIM_{sprite_name}_{anim_name}"
                    value = (sid << 16) | (pal_id << 8) | anim_idx
                    f.write(f"#define {full_name} 0x{value:X}\n")
                f.write("\n")

        f.write("#endif // _PLAYER_SPRITE_H_\n")

    print(f"  Generated {output_file}")


if __name__ == "__main__":
    main()
