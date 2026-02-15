#!/usr/bin/env bash
set -euo pipefail

WORK_DIR="${WORK_DIR:-/work}"
CLONE_DIR="${CLONE_DIR:-$WORK_DIR/papermario}"
REPO_URL="${REPO_URL:-https://github.com/pmret/papermario.git}"
REPO_REF="${REPO_REF:-main}"
CLONE_DEPTH="${CLONE_DEPTH:-1}"

ROM_PATH="${ROM_PATH:-/roms/baserom.z64}"
VERSION="${VERSION:-us}"
BUILD_ROM="${BUILD_ROM:-0}"
SPLAT_URL="${SPLAT_URL:-https://github.com/ethteck/splat.git}"
SPLAT_REF="${SPLAT_REF:-0.31.0}"
CONFIGURE_ARGS="${CONFIGURE_ARGS:-}"

SYNC_TO_HOST="${SYNC_TO_HOST:-1}"
HOST_DECOMP_DIR="${HOST_DECOMP_DIR:-/host-decomp}"

case "$VERSION" in
  us|jp|pal|ique) ;;
  *)
    echo "error: VERSION must be one of: us, jp, pal, ique"
    exit 2
    ;;
esac

if [[ ! -f "$ROM_PATH" ]]; then
  echo "error: rom file not found: $ROM_PATH"
  exit 2
fi

mkdir -p "$WORK_DIR"
rm -rf "$CLONE_DIR"

echo "cloning papermario repo: $REPO_URL ($REPO_REF)"
git clone --depth "$CLONE_DEPTH" --branch "$REPO_REF" "$REPO_URL" "$CLONE_DIR"

cd "$CLONE_DIR"

if [[ ! -d "tools/splat" ]]; then
  echo "tools/splat missing, cloning $SPLAT_URL ($SPLAT_REF)"
  git clone --depth 1 --branch "$SPLAT_REF" "$SPLAT_URL" tools/splat
fi

if [[ ! -x "tools/build/cc/gcc/gcc" ]]; then
  echo "compiler bundles missing, running install_compilers.sh"
  bash ./install_compilers.sh
fi

echo "installing python dependencies for configure"
python3 -m pip install -U -r tools/configure/requirements.txt --break-system-packages

mkdir -p "ver/$VERSION"
cp "$ROM_PATH" "ver/$VERSION/baserom.z64"

echo "running configure for version '$VERSION'"
if [[ -n "$CONFIGURE_ARGS" ]]; then
  # Intentionally split on whitespace for CLI-like behavior.
  # shellcheck disable=SC2206
  extra_args=($CONFIGURE_ARGS)
  bash ./configure "${extra_args[@]}" "$VERSION"
else
  bash ./configure "$VERSION"
fi

if [[ "$BUILD_ROM" == "1" ]]; then
  echo "running ninja"
  ninja
else
  echo "configure complete; asset extraction/build files generated."
fi

if [[ "$SYNC_TO_HOST" == "1" ]]; then
  if [[ ! -d "$HOST_DECOMP_DIR" ]]; then
    echo "error: SYNC_TO_HOST=1 but host output directory is missing: $HOST_DECOMP_DIR"
    exit 2
  fi

  echo "syncing generated outputs to host decomp dir: $HOST_DECOMP_DIR"
  mkdir -p "$HOST_DECOMP_DIR/assets/$VERSION"
  mkdir -p "$HOST_DECOMP_DIR/ver/$VERSION/build"
  mkdir -p "$HOST_DECOMP_DIR/ver/$VERSION"

  rsync -a --delete "assets/$VERSION/" "$HOST_DECOMP_DIR/assets/$VERSION/"
  rsync -a --delete "ver/$VERSION/build/" "$HOST_DECOMP_DIR/ver/$VERSION/build/"
  cp "ver/$VERSION/baserom.z64" "$HOST_DECOMP_DIR/ver/$VERSION/baserom.z64"
  cp "build.ninja" "$HOST_DECOMP_DIR/build.ninja"
fi

echo "done."
