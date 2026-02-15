#!/bin/bash
# Paper Mario PC Port - Build & Run Script
# Usage:
#   ./build.sh          Build only
#   ./build.sh run      Build and run
#   ./build.sh run-only Run without building
#   ./build.sh clean    Clean build artifacts

export PATH="/c/msys64/ucrt64/bin:$PATH"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
CMAKE="/c/msys64/ucrt64/bin/cmake.exe"

case "${1:-build}" in
    build)
        echo "=== Building ==="
        cd "$BUILD_DIR" && $CMAKE --build . 2>&1
        echo "=== Exit: $? ==="
        ;;
    run)
        echo "=== Building ==="
        cd "$BUILD_DIR" && $CMAKE --build . 2>&1
        if [ $? -ne 0 ]; then
            echo "Build failed!"
            exit 1
        fi
        echo "=== Running ==="
        cd "$SCRIPT_DIR" && ./build/papermario.exe 2>&1
        ;;
    run-only)
        echo "=== Running ==="
        cd "$SCRIPT_DIR" && ./build/papermario.exe 2>&1
        ;;
    clean)
        echo "=== Cleaning ==="
        cd "$BUILD_DIR" && $CMAKE --build . --target clean 2>&1
        ;;
    *)
        echo "Usage: $0 [build|run|run-only|clean]"
        exit 1
        ;;
esac
