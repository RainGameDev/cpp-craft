#!/usr/bin/env bash
# Linux/macOS one-step: configure + build + run.
# Usage:
#   ./run.sh                 # build (Release) and run
#   ./run.sh --build-only    # build only, don't launch the window
#   ./run.sh --clean         # wipe build/ first, then build+run
#   BUILD_TYPE=Debug BUILD_DIR=build ./run.sh
set -euo pipefail
cd "$(dirname "$0")"

BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_DIR="${BUILD_DIR:-build}"
BUILD_ONLY=0

for arg in "$@"; do
  case "$arg" in
    --build-only) BUILD_ONLY=1 ;;
    --clean) rm -rf "$BUILD_DIR" ;;
    -h|--help)
      echo "Usage: ./run.sh [--clean] [--build-only]"
      exit 0 ;;
  esac
done

JOBS="$(nproc 2>/dev/null || echo 4)"

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR" -j "$JOBS"

if [ "$BUILD_ONLY" -eq 1 ]; then
  echo "Build done: $BUILD_DIR/minecraft"
  exit 0
fi

exec "$BUILD_DIR/minecraft"
