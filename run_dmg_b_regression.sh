#!/usr/bin/env bash
# Complete automated regression gate for the emulator's DMG-CPU B target.
# Improvements are accepted; a previously passing target-compatible test
# becoming a failure makes this script return nonzero.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
EMU="$SCRIPT_DIR/build/gbemu"
MTS="$SCRIPT_DIR/test_roms/mts-20240926-1737-443f6e1"
ACCEPTANCE="$MTS/acceptance"
EMU_ONLY="$MTS/emulator-only"
TMP_DIR="$(mktemp -d /tmp/gbemu-dmg-b-regression.XXXXXX)"
trap 'rm -rf "$TMP_DIR"' EXIT

failures=0

record_failure() {
    echo "  REGRESSION: $1"
    failures=$((failures + 1))
}

echo "═══════════════════════════════════════════════════"
echo "  GB-Emu-2k26 — DMG-CPU B regression gate"
echo "═══════════════════════════════════════════════════"

for tool in cmake convert compare; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing required tool: $tool"
        exit 2
    fi
done

cmake -S "$SCRIPT_DIR" -B "$SCRIPT_DIR/build" -DCMAKE_BUILD_TYPE=Release >/dev/null
if ! cmake --build "$SCRIPT_DIR/build" -j"$(nproc)"; then
    echo "Build failed"
    exit 2
fi

# Mooneye naming follows the upstream model tags. Untagged, -G/-GS,
# -dmgABC, and -dmgABCmgb tests apply to DMG-CPU B. The patterns below reject
# DMG-0, MGB, SGB/SGB2, and CGB/AGB-only variants in the bundled release.
is_dmg_b_acceptance_rom() {
    local name
    name="$(basename "$1" .gb)"
    case "$name" in
        *-dmg0|*-mgb|*-sgb|*-sgb2|*-S|*-C|*-A|*-cgb*|*-agb*|*-ags*)
            return 1
            ;;
        *)
            return 0
            ;;
    esac
}

declare -A known_mooneye_failures=(
    ["acceptance/ppu/intr_1_2_timing-GS.gb"]=1
    ["acceptance/ppu/intr_2_mode0_timing_sprites.gb"]=1
    ["acceptance/ppu/lcdon_timing-GS.gb"]=1
    ["acceptance/ppu/lcdon_write_timing-GS.gb"]=1
)

echo ""
echo "── Mooneye: DMG-B-compatible acceptance + MBC ─────"
mooneye_total=0
mooneye_pass=0
mooneye_known_fail=0

run_mooneye_rom() {
    local rom="$1"
    local rel="${rom#$MTS/}"
    local rc

    timeout 30 "$EMU" --test "$rom" >"$TMP_DIR/mooneye.log" 2>&1
    rc=$?
    mooneye_total=$((mooneye_total + 1))

    if [ "$rc" -eq 0 ]; then
        echo "  ✓ $rel"
        mooneye_pass=$((mooneye_pass + 1))
    elif [ "$rc" -eq 1 ] && [ -n "${known_mooneye_failures[$rel]+known}" ]; then
        echo "  · $rel  (known PPU gap)"
        mooneye_known_fail=$((mooneye_known_fail + 1))
    else
        echo "  ✗ $rel  (rc=$rc)"
        record_failure "unexpected Mooneye failure: $rel"
    fi
}

while IFS= read -r rom; do
    if is_dmg_b_acceptance_rom "$rom"; then
        run_mooneye_rom "$rom"
    fi
done < <(find "$ACCEPTANCE" -type f -name '*.gb' | sort)

for mbc in mbc1 mbc2 mbc5; do
    while IFS= read -r rom; do
        run_mooneye_rom "$rom"
    done < <(find "$EMU_ONLY/$mbc" -type f -name '*.gb' | sort)
done

if [ "$mooneye_total" -ne 94 ]; then
    record_failure "Mooneye DMG-B manifest changed: expected 94 ROMs, found $mooneye_total"
fi
printf '  Mooneye result: %d/%d pass, %d known gaps\n' \
       "$mooneye_pass" "$mooneye_total" "$mooneye_known_fail"

# Mooneye labels this ROM manual-only, but its upstream source includes an
# exact 160x144 reference image. Promote it to an automated framebuffer gate.
sprite_priority_ppm="$TMP_DIR/sprite-priority.ppm"
sprite_priority_grey="$TMP_DIR/sprite-priority-grey.png"
timeout 15 "$EMU" --blargg --dump-lcd "$sprite_priority_ppm" \
    "$MTS/manual-only/sprite_priority.gb" \
    >"$TMP_DIR/sprite-priority.log" 2>&1 || true
if [ ! -f "$sprite_priority_ppm" ]; then
    record_failure "Mooneye sprite_priority produced no framebuffer"
else
    convert "$sprite_priority_ppm" \
        -fill '#FFFFFF' -opaque '#E0F8D0' \
        -fill '#AAAAAA' -opaque '#88C070' \
        -fill '#555555' -opaque '#346856' \
        -fill '#000000' -opaque '#081820' \
        "$sprite_priority_grey"
    sprite_priority_diff=$(compare -metric AE "$sprite_priority_grey" \
        "$SCRIPT_DIR/test_roms/mooneye-test-suite-main/manual-only/sprite_priority-expected.png" \
        NULL: 2>&1 || true)
    if [ "$sprite_priority_diff" = "0" ]; then
        echo "  ✓ manual-only/sprite_priority.gb  [pixel-exact]"
    else
        record_failure "Mooneye sprite_priority differs by $sprite_priority_diff pixels"
    fi
fi

echo ""
echo "── Mealybug: DMG-CPU B references ────────────────"
mealybug_log="$TMP_DIR/mealybug.log"
bash "$SCRIPT_DIR/run_mealybug.sh" | tee "$mealybug_log"
mealybug_rc=${PIPESTATUS[0]}
if [ "$mealybug_rc" -ne 0 ]; then
    record_failure "Mealybug runner error (rc=$mealybug_rc)"
fi

required_mealybug_passes=(
    m2_win_en_toggle
    m3_scx_low_3_bits
    m3_wx_4_change
    m3_wx_4_change_sprites
    m3_wx_5_change
)
for test_name in "${required_mealybug_passes[@]}"; do
    if ! grep -Fq "✓ $test_name " "$mealybug_log"; then
        record_failure "previously exact Mealybug image changed: $test_name"
    fi
done

run_blargg_protocol() {
    local label="$1"
    local rom="$2"
    local rc
    timeout 90 "$EMU" --blargg "$rom" >"$TMP_DIR/blargg.log" 2>&1
    rc=$?
    if [ "$rc" -eq 0 ]; then
        echo "  ✓ $label"
    else
        echo "  ✗ $label (rc=$rc)"
        tail -n 12 "$TMP_DIR/blargg.log"
        record_failure "Blargg protocol test failed: $label"
    fi
}

run_blargg_serial() {
    local label="$1"
    local rom="$2"
    timeout 90 "$EMU" --blargg "$rom" >"$TMP_DIR/blargg.log" 2>&1 || true
    if grep -Fq "Passed" "$TMP_DIR/blargg.log" &&
       ! grep -Fq "Failed" "$TMP_DIR/blargg.log"; then
        echo "  ✓ $label  [serial result]"
    else
        echo "  ✗ $label  [serial result missing/failing]"
        tail -n 12 "$TMP_DIR/blargg.log"
        record_failure "Blargg serial test failed: $label"
    fi
}

echo ""
echo "── Blargg: DMG-compatible automated suites ───────"
run_blargg_serial "cpu_instrs 11/11" \
    "$SCRIPT_DIR/test_roms/blargg/cpu_instrs/cpu_instrs.gb"
run_blargg_serial "instr_timing" \
    "$SCRIPT_DIR/test_roms/blargg/instr_timing/instr_timing.gb"
run_blargg_serial "mem_timing 3/3" \
    "$SCRIPT_DIR/test_roms/blargg/mem_timing/mem_timing.gb"

for rom in "$SCRIPT_DIR"/test_roms/blargg/mem_timing-2/rom_singles/*.gb; do
    run_blargg_protocol "mem_timing-2/$(basename "$rom" .gb)" "$rom"
done
for rom in "$SCRIPT_DIR"/test_roms/blargg/dmg_sound-2/rom_singles/*.gb; do
    run_blargg_protocol "dmg_sound/$(basename "$rom" .gb)" "$rom"
done
run_blargg_protocol "oam_bug combined 8/8" \
    "$SCRIPT_DIR/test_roms/blargg/oam_bug/oam_bug.gb"
run_blargg_protocol "oam_bug-2 combined 8/8" \
    "$SCRIPT_DIR/test_roms/blargg/oam_bug-2/oam_bug.gb"

# halt_bug reports only on the LCD and has no machine-readable completion
# protocol. Execute it for coverage; the four strict Mooneye HALT tests above
# remain the automated pass/fail gate.
timeout 30 "$EMU" --blargg \
    "$SCRIPT_DIR/test_roms/blargg/halt_bug/halt_bug.gb" \
    >"$TMP_DIR/halt_bug.log" 2>&1 || true
echo "  · halt_bug executed  [visual-only ROM; Mooneye HALT is the strict gate]"

echo ""
echo "── DMG-ACID2: official DMG reference ─────────────"
acid_ppm="$TMP_DIR/dmg-acid2.ppm"
acid_grey="$TMP_DIR/dmg-acid2-grey.png"
timeout 60 "$EMU" --blargg --dump-lcd "$acid_ppm" \
    "$SCRIPT_DIR/test_roms/dmg-acid2.gb" >"$TMP_DIR/acid.log" 2>&1 || true

if [ ! -f "$acid_ppm" ]; then
    record_failure "DMG-ACID2 produced no framebuffer"
else
    convert "$acid_ppm" \
        -fill '#FFFFFF' -opaque '#E0F8D0' \
        -fill '#AAAAAA' -opaque '#88C070' \
        -fill '#555555' -opaque '#346856' \
        -fill '#000000' -opaque '#081820' \
        "$acid_grey"
    acid_diff=$(compare -metric AE "$acid_grey" \
        "$SCRIPT_DIR/test_roms/dmg-acid2-master/img/reference-dmg.png" \
        NULL: 2>&1 || true)
    if [ "$acid_diff" = "0" ]; then
        echo "  ✓ DMG-ACID2 pixel-exact"
    else
        record_failure "DMG-ACID2 differs by $acid_diff pixels"
    fi
fi

echo ""
echo "═══════════════════════════════════════════════════"
if [ "$failures" -eq 0 ]; then
    echo "  DMG-CPU B regression gate: PASS"
    echo "═══════════════════════════════════════════════════"
    exit 0
fi

echo "  DMG-CPU B regression gate: FAIL ($failures regressions)"
echo "═══════════════════════════════════════════════════"
exit 1
