#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════
# Mealybug Tearoom Tests — DMG runner
#
# Runs each test ROM, dumps the LCD as PPM at the LD B,B breakpoint,
# converts to greyscale PNG (mapping our DMG palette to 00/55/AA/FF),
# and compares pixel-by-pixel against DMG-CPU B captures where available, with
# DMG-blob references used only for tests that have no CPU-B capture.
# ═══════════════════════════════════════════════════════════════════════

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
EMU="$SCRIPT_DIR/build/gbemu"
ROMS_DIR="$SCRIPT_DIR/test_roms/mealybug-tearoom-tests/build"
EXPECTED_CPU_B_DIR="$SCRIPT_DIR/test_roms/mealybug-tearoom-tests/expected/DMG-CPU B"
EXPECTED_BLOB_DIR="$SCRIPT_DIR/test_roms/mealybug-tearoom-tests/expected/DMG-blob"
OUTPUT_DIR="$SCRIPT_DIR/test_roms/mealybug-tearoom-tests/output"

# Ensure emulator is built
if [ ! -f "$EMU" ]; then
    echo "Building emulator..."
    mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1 && make -j$(nproc) > /dev/null 2>&1
    cd "$SCRIPT_DIR"
fi

mkdir -p "$OUTPUT_DIR"

# Our DMG palette (ARGB): E0F8D0 / 88C070 / 346856 / 081820
# Mealybug expects greyscale: FF / AA / 55 / 00
# We remap via ImageMagick after converting PPM→PNG

pass=0
fail=0
total=0
errors=0
failed_tests=""

echo "═══════════════════════════════════════════════════"
echo "  Mealybug Tearoom Tests — DMG-CPU B target"
echo "  CPU-B capture preferred; DMG-blob fallback"
echo "═══════════════════════════════════════════════════"
echo ""

for rom in "$ROMS_DIR"/*.gb; do
    name=$(basename "$rom" .gb)
    expected="$EXPECTED_CPU_B_DIR/${name}.png"
    reference="DMG-CPU B"
    if [ ! -f "$expected" ]; then
        expected="$EXPECTED_BLOB_DIR/${name}.png"
        reference="DMG-blob"
    fi

    # Skip tests that have no applicable DMG reference screenshot.
    if [ ! -f "$expected" ]; then
        continue
    fi

    total=$((total + 1))

    ppm_out="$OUTPUT_DIR/${name}.ppm"
    png_out="$OUTPUT_DIR/${name}.png"
    grey_out="$OUTPUT_DIR/${name}_grey.png"

    # Never let a stale framebuffer turn a crashed/timed-out run into a pass.
    rm -f "$ppm_out" "$png_out" "$grey_out"

    # Run ROM with Blargg mode (dumps LCD at completion) with timeout
    timeout 30 "$EMU" --blargg --dump-lcd "$ppm_out" "$rom" > /dev/null 2>&1 || true

    if [ ! -f "$ppm_out" ]; then
        echo "  ✗ $name  (no LCD output)"
        fail=$((fail + 1))
        errors=$((errors + 1))
        failed_tests="$failed_tests  $name\n"
        continue
    fi

    # Convert PPM to PNG
    convert "$ppm_out" "$png_out" 2>/dev/null

    # Remap our green palette to the expected greyscale palette
    # E0F8D0 → FFFFFF, 88C070 → AAAAAA, 346856 → 555555, 081820 → 000000
    convert "$png_out" \
        -fill "#FFFFFF" -opaque "#E0F8D0" \
        -fill "#AAAAAA" -opaque "#88C070" \
        -fill "#555555" -opaque "#346856" \
        -fill "#000000" -opaque "#081820" \
        "$grey_out" 2>/dev/null

    # Compare pixel-by-pixel
    diff_pixels=$(compare -metric AE "$grey_out" "$expected" NULL: 2>&1 || true)

    if [[ ! "$diff_pixels" =~ ^[0-9]+$ ]]; then
        echo "  ✗ $name  (comparison error: $diff_pixels)  [$reference]"
        fail=$((fail + 1))
        errors=$((errors + 1))
        failed_tests="$failed_tests  $name (comparison error)\n"
    elif [ "$diff_pixels" = "0" ]; then
        echo "  ✓ $name  [$reference]"
        pass=$((pass + 1))
    else
        echo "  ✗ $name  ($diff_pixels pixels differ)  [$reference]"
        fail=$((fail + 1))
        failed_tests="$failed_tests  $name ($diff_pixels px)\n"
    fi
done

if [ "$total" -ne 24 ]; then
    echo "  ✗ runner manifest error: expected 24 tests, found $total"
    errors=$((errors + 1))
fi

echo ""
echo "═══════════════════════════════════════════════════"
echo "  Results: $pass/$total passed, $fail failed"
echo "═══════════════════════════════════════════════════"

if [ $fail -gt 0 ]; then
    echo ""
    echo "Failed tests:"
    echo -e "$failed_tests"
fi

# Pixel mismatches are accuracy results, but a missing capture means this
# runner did not actually test the ROM and must fail the regression harness.
if [ "$errors" -gt 0 ]; then
    exit 2
fi
