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
echo "  Mooneye Timer Test Suite"
echo "═══════════════════════════════════════════════════"
echo ""

PASS=0
FAIL=0
ERROR=0

TEST_BASE="test_roms/mts-20240926-1737-443f6e1/acceptance"

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

# ── Timer tests ────────────────────────────────────────────────────────
echo "── Timer ─────────────────────────────────────────"
for rom in "$TEST_BASE"/timer/*.gb; do
    run_test "$rom"
done
echo ""

# ── DIV timing ─────────────────────────────────────────────────────────
echo "── DIV Timing ────────────────────────────────────"
if [ -f "$TEST_BASE/div_timing.gb" ]; then
    run_test "$TEST_BASE/div_timing.gb"
fi
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
