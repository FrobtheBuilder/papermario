#!/bin/bash
# Paper Mario PC Port - Asset Generation Script
#
# This generates all required build headers from the decomp source files.
# The actual game assets (ROMs, textures, etc.) cannot be distributed and
# must be obtained separately. This script only generates the derived header
# files that the PC port build needs.
#
# Prerequisites:
#   - Python 3
#   - pypng package (pip install pypng)
#   - msgpack package (pip install msgpack)
#   - build.ninja must exist (run ./configure.py first)
#
# Must be run from the project root directory.

set -e
export PYTHONUTF8=1

# Determine project root
if [ -n "$1" ]; then
    PROJECT_ROOT="$1"
else
    PROJECT_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fi

cd "$PROJECT_ROOT"

# Verify we're in the right directory
if [ ! -f "CMakeLists.txt" ] || [ ! -d "src" ]; then
    echo "ERROR: Must be run from the Paper Mario project root directory."
    echo "Current directory: $(pwd)"
    exit 1
fi

BUILD_INCLUDE="ver/us/build/include"
ASSETS="assets/us"

echo "=== Paper Mario PC Port Asset Generation ==="
echo "Project root: $PROJECT_ROOT"
echo ""

# Detect python command
if command -v python3 &> /dev/null; then
    PYTHON=python3
elif command -v python &> /dev/null; then
    PYTHON=python
else
    echo "ERROR: Python 3 not found. Please install Python 3."
    exit 1
fi

echo "Using Python: $($PYTHON --version)"
echo ""

# Ensure output directories exist
mkdir -p "$BUILD_INCLUDE/battle"
mkdir -p "$BUILD_INCLUDE/effects"
mkdir -p "$BUILD_INCLUDE/sprite/npc"

# Step 1: Move data (move_data.inc.c, move_enum.h)
echo "[1/10] Generating move_data.inc.c and move_enum.h..."
$PYTHON tools/build/move_data.py \
    "$BUILD_INCLUDE/move_data.inc.c" \
    "$BUILD_INCLUDE/move_enum.h" \
    src/move_table.yaml

# Post-process move_data.inc.c: replace (s32) "string" with SCRIPT_CAST("string")
$PYTHON tools/pc/fix_move_data.py "$BUILD_INCLUDE/move_data.inc.c"

# Step 2: Item data (item_data.inc.c, item_enum.h)
echo "[2/10] Generating item_data.inc.c and item_enum.h..."
$PYTHON tools/build/item_data.py \
    "$BUILD_INCLUDE/item_data.inc.c" \
    "$BUILD_INCLUDE/item_enum.h" \
    src/item_table.yaml \
    src/item_entity_scripts.yaml \
    src/item_hud_scripts.yaml

# Step 3: Actor types (actor_types.inc.c, actor_types.h)
echo "[3/10] Generating actor_types.inc.c and actor_types.h..."
$PYTHON tools/build/actor_types.py \
    "$BUILD_INCLUDE/battle/actor_types.inc.c" \
    "$BUILD_INCLUDE/battle/actor_types.h" \
    src/battle/actors.yaml

# Step 4: Sprite shading profiles
echo "[4/10] Generating sprite_shading_profiles.h..."
mkdir -p "ver/us/build/assets/us"
$PYTHON tools/build/sprite/sprite_shading_profiles.py \
    "$ASSETS/sprite/sprite_shading_profiles.json" \
    "ver/us/build/assets/us/sprite_shading_profiles.bin" \
    "$BUILD_INCLUDE/sprite/sprite_shading_profiles.h"

# Step 5: Icon offsets (icon_offsets.h)
echo "[5/10] Generating icon_offsets.h..."
$PYTHON tools/build/icons.py \
    "ver/us/build/assets/us/icons.bin" \
    "$BUILD_INCLUDE/icon_offsets.h" \
    "us"

# Step 6: Effects headers
echo "[6/10] Generating effect headers..."
$PYTHON tools/build/effects.py \
    src/effects.yaml \
    "$ASSETS/effects"

# Copy effects headers to build include if needed
if [ -d "$ASSETS/effects" ]; then
    mkdir -p "$BUILD_INCLUDE/effects"
    for f in effect_defs.h effect_macros.h effect_table.c; do
        if [ -f "$ASSETS/effects/$f" ]; then
            cp "$ASSETS/effects/$f" "$BUILD_INCLUDE/effects/$f"
        fi
    done
fi

# Step 7: Message IDs (message_ids.h)
echo "[7/10] Generating message_ids.h..."
$PYTHON tools/pc/gen_message_ids.py

# Step 8: PNG headers
echo "[8/10] Generating PNG image headers..."
if [ -f "build.ninja" ]; then
    $PYTHON tools/pc/gen_png_headers.py
else
    echo "  WARNING: build.ninja not found, skipping PNG headers."
    echo "  Run ./configure.py first to generate build.ninja."
fi

# Step 9: Player sprite header
echo "[9/10] Generating sprite/player.h..."
$PYTHON tools/pc/gen_player_sprite.py

# Step 10: NPC sprite headers
echo "[10/10] Generating NPC sprite headers..."
$PYTHON tools/pc/gen_npc_sprites.py

echo ""
echo "=== Asset generation complete! ==="
echo ""
echo "Generated files are in: $BUILD_INCLUDE/"
echo ""
echo "You can now build the PC port with:"
echo "  mkdir -p build && cd build"
echo "  cmake .. -G 'MinGW Makefiles'"
echo "  cmake --build . -j\$(nproc)"
