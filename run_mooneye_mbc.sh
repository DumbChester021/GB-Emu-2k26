#!/bin/bash
set -e

# ── Build ──────────────────────────────────────────────────────────────
BUILD_DIR="build"
EXECUTABLE="$BUILD_DIR/gbemu"

echo "═══════════════════════════════════════════════════"
echo "  Building GB Emu..."
echo "═══════════════════════════════════════════════════"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1
make -j$(nproc) 2>&1
cd ..

if [ ! -f "$EXECUTABLE" ]; then
    echo "Error: Build failed — $EXECUTABLE not found"
    exit 1
fi

echo ""
echo "═══════════════════════════════════════════════════"
echo "  Mooneye MBC Test Suite"
echo "═══════════════════════════════════════════════════"
echo ""

PASS=0
FAIL=0
ERROR=0

TEST_BASE="test_roms/mts-20240926-1737-443f6e1/emulator-only"

run_test() {
    local rom="$1"
    local name=$(basename "$rom" .gb)
    local dir=$(basename $(dirname "$rom"))

    OUTPUT=$(./"$EXECUTABLE" --test "$rom" 2>/dev/null)
    RC=$?

    if [ $RC -eq 0 ]; then
        echo "  ✓ [$dir] $name"
        PASS=$((PASS + 1))
    elif [ $RC -eq 1 ]; then
        echo "  ✗ [$dir] $name"
        FAIL=$((FAIL + 1))
    else
        echo "  ? [$dir] $name (timeout/error)"
        ERROR=$((ERROR + 1))
    fi
}

# ── MBC1 tests ─────────────────────────────────────────────────────────
echo "── MBC1 ──────────────────────────────────────────"
for rom in "$TEST_BASE"/mbc1/*.gb; do
    run_test "$rom"
done
echo ""

# ── MBC2 tests ─────────────────────────────────────────────────────────
echo "── MBC2 ──────────────────────────────────────────"
for rom in "$TEST_BASE"/mbc2/*.gb; do
    run_test "$rom"
done
echo ""

# ── MBC5 tests ─────────────────────────────────────────────────────────
echo "── MBC5 ──────────────────────────────────────────"
for rom in "$TEST_BASE"/mbc5/*.gb; do
    run_test "$rom"
done
echo ""

# ── Summary ────────────────────────────────────────────────────────────
TOTAL=$((PASS + FAIL + ERROR))
echo "═══════════════════════════════════════════════════"
echo "  Results: $PASS/$TOTAL passed, $FAIL failed, $ERROR errors"
echo "═══════════════════════════════════════════════════"

if [ $FAIL -eq 0 ] && [ $ERROR -eq 0 ]; then
    exit 0
else
    exit 1
fi
