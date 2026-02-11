#!/bin/bash
set -e

# build_and_run.sh

BUILD_DIR="build"
EXECUTABLE="gbemu"

echo "Cleaning previous build..."
rm -rf "$BUILD_DIR"
mkdir "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Configuring with CMake..."
cmake ..

echo "Building with $(nproc) cores..."
make -j$(nproc)

if [ -f "$EXECUTABLE" ]; then
    echo "Running $EXECUTABLE..."
    ./"$EXECUTABLE"
else
    echo "Error: Executable $EXECUTABLE not found!"
    exit 1
fi
