#!/bin/bash

# ══════════════════════════════════════════════════════════════════════════
#  Comprehensive Mooneye Acceptance Test Runner (DMG only, no PPU/Sound)
# ══════════════════════════════════════════════════════════════════════════

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
echo "  Mooneye Acceptance Tests (DMG, no PPU/Sound)"
echo "═══════════════════════════════════════════════════"
echo ""

PASS=0
FAIL=0
ERROR=0
TOTAL_PASS=0
TOTAL_FAIL=0
TOTAL_ERROR=0

TEST_BASE="test_roms/mts-20240926-1737-443f6e1/acceptance"
EMU_BASE="test_roms/mts-20240926-1737-443f6e1/emulator-only"

run_test() {
    local rom="$1"
    local name=$(basename "$rom" .gb)
    local dir=$(basename $(dirname "$rom"))

    timeout 30 ./"$EXECUTABLE" --test "$rom" > /dev/null 2>&1
    local RC=$?

    if [ $RC -eq 0 ]; then
        echo "  ✓ $name"
        PASS=$((PASS + 1))
    elif [ $RC -eq 1 ]; then
        echo "  ✗ $name"
        FAIL=$((FAIL + 1))
    else
        echo "  ? $name (timeout/error rc=$RC)"
        ERROR=$((ERROR + 1))
    fi
}

section_summary() {
    local section="$1"
    local total=$((PASS + FAIL + ERROR))
    echo "  ── $section: $PASS/$total passed"
    TOTAL_PASS=$((TOTAL_PASS + PASS))
    TOTAL_FAIL=$((TOTAL_FAIL + FAIL))
    TOTAL_ERROR=$((TOTAL_ERROR + ERROR))
    PASS=0; FAIL=0; ERROR=0
}

# ═══════════════════════════════════════════════════════════════════════
#  1. CPU Instructions
# ═══════════════════════════════════════════════════════════════════════
echo "── CPU Instructions ─────────────────────────────"
for rom in "$TEST_BASE"/instr/*.gb; do
    [ -f "$rom" ] && run_test "$rom"
done
section_summary "Instructions"
echo ""

# ═══════════════════════════════════════════════════════════════════════
#  2. Bits (reg_f, mem_oam, unused_hwio)
# ═══════════════════════════════════════════════════════════════════════
echo "── Bits ──────────────────────────────────────────"
for rom in "$TEST_BASE"/bits/*.gb; do
    [ -f "$rom" ] && run_test "$rom"
done
section_summary "Bits"
echo ""

# ═══════════════════════════════════════════════════════════════════════
#  3. Interrupts
# ═══════════════════════════════════════════════════════════════════════
echo "── Interrupts ────────────────────────────────────"
for rom in "$TEST_BASE"/interrupts/*.gb; do
    [ -f "$rom" ] && run_test "$rom"
done
[ -f "$TEST_BASE/if_ie_registers.gb" ] && run_test "$TEST_BASE/if_ie_registers.gb"
[ -f "$TEST_BASE/intr_timing.gb" ] && run_test "$TEST_BASE/intr_timing.gb"
section_summary "Interrupts"
echo ""

# ═══════════════════════════════════════════════════════════════════════
#  4. EI / DI Timing
# ═══════════════════════════════════════════════════════════════════════
echo "── EI / DI Timing ────────────────────────────────"
for t in ei_sequence ei_timing di_timing-GS rapid_di_ei; do
    [ -f "$TEST_BASE/$t.gb" ] && run_test "$TEST_BASE/$t.gb"
done
section_summary "EI/DI Timing"
echo ""

# ═══════════════════════════════════════════════════════════════════════
#  5. HALT
# ═══════════════════════════════════════════════════════════════════════
echo "── HALT ──────────────────────────────────────────"
for t in halt_ime0_ei halt_ime0_nointr_timing halt_ime1_timing halt_ime1_timing2-GS; do
    [ -f "$TEST_BASE/$t.gb" ] && run_test "$TEST_BASE/$t.gb"
done
section_summary "HALT"
echo ""

# ═══════════════════════════════════════════════════════════════════════
#  6. CALL / JP / RET / POP / PUSH / RST Timing
# ═══════════════════════════════════════════════════════════════════════
echo "── Call/JP/Ret/Pop/Push/RST Timing ───────────────"
for t in call_timing call_timing2 call_cc_timing call_cc_timing2 \
         jp_timing jp_cc_timing \
         ret_timing ret_cc_timing reti_timing reti_intr_timing \
         rst_timing pop_timing push_timing; do
    [ -f "$TEST_BASE/$t.gb" ] && run_test "$TEST_BASE/$t.gb"
done
section_summary "Call/JP/Ret/Pop/Push/RST"
echo ""

# ═══════════════════════════════════════════════════════════════════════
#  7. ADD SP / LD HL,SP+e Timing
# ═══════════════════════════════════════════════════════════════════════
echo "── ADD SP / LD HL,SP+e Timing ────────────────────"
for t in add_sp_e_timing ld_hl_sp_e_timing; do
    [ -f "$TEST_BASE/$t.gb" ] && run_test "$TEST_BASE/$t.gb"
done
section_summary "ADD SP / LD HL,SP+e"
echo ""

# ═══════════════════════════════════════════════════════════════════════
#  8. DIV Timing
# ═══════════════════════════════════════════════════════════════════════
echo "── DIV Timing ────────────────────────────────────"
[ -f "$TEST_BASE/div_timing.gb" ] && run_test "$TEST_BASE/div_timing.gb"
section_summary "DIV Timing"
echo ""

# ═══════════════════════════════════════════════════════════════════════
#  9. Timer
# ═══════════════════════════════════════════════════════════════════════
echo "── Timer ─────────────────────────────────────────"
for rom in "$TEST_BASE"/timer/*.gb; do
    [ -f "$rom" ] && run_test "$rom"
done
section_summary "Timer"
echo ""

# ═══════════════════════════════════════════════════════════════════════
#  10. OAM DMA
# ═══════════════════════════════════════════════════════════════════════
echo "── OAM DMA ─────────────────────────────────────"
for rom in "$TEST_BASE"/oam_dma/*.gb; do
    [ -f "$rom" ] && run_test "$rom"
done
for t in oam_dma_restart oam_dma_start oam_dma_timing; do
    [ -f "$TEST_BASE/$t.gb" ] && run_test "$TEST_BASE/$t.gb"
done
section_summary "OAM DMA"
echo ""

# ═══════════════════════════════════════════════════════════════════════
#  11. Serial
# ═══════════════════════════════════════════════════════════════════════
echo "── Serial ──────────────────────────────────────"
for rom in "$TEST_BASE"/serial/*.gb; do
    [ -f "$rom" ] && run_test "$rom"
done
section_summary "Serial"
echo ""

# ═══════════════════════════════════════════════════════════════════════
#  12. Boot Registers (DMG variants only)
# ═══════════════════════════════════════════════════════════════════════
echo "── Boot Registers (DMG) ──────────────────────────"
for t in boot_regs-dmg0 boot_regs-dmgABC; do
    [ -f "$TEST_BASE/$t.gb" ] && run_test "$TEST_BASE/$t.gb"
done
section_summary "Boot Regs (DMG)"
echo ""

# ═══════════════════════════════════════════════════════════════════════
#  13. Boot DIV (DMG variants only)
# ═══════════════════════════════════════════════════════════════════════
echo "── Boot DIV (DMG) ────────────────────────────────"
for t in boot_div-dmg0 boot_div-dmgABCmgb; do
    [ -f "$TEST_BASE/$t.gb" ] && run_test "$TEST_BASE/$t.gb"
done
section_summary "Boot DIV (DMG)"
echo ""

# ═══════════════════════════════════════════════════════════════════════
#  14. Boot HWIO (DMG variants only)
# ═══════════════════════════════════════════════════════════════════════
echo "── Boot HWIO (DMG) ───────────────────────────────"
for t in boot_hwio-dmg0 boot_hwio-dmgABCmgb; do
    [ -f "$TEST_BASE/$t.gb" ] && run_test "$TEST_BASE/$t.gb"
done
section_summary "Boot HWIO (DMG)"
echo ""

# ═══════════════════════════════════════════════════════════════════════
#  15. MBC1 (emulator-only)
# ═══════════════════════════════════════════════════════════════════════
echo "── MBC1 ──────────────────────────────────────────"
for rom in "$EMU_BASE"/mbc1/*.gb; do
    [ -f "$rom" ] && run_test "$rom"
done
section_summary "MBC1"
echo ""

# ═══════════════════════════════════════════════════════════════════════
#  16. MBC2 (emulator-only)
# ═══════════════════════════════════════════════════════════════════════
echo "── MBC2 ──────────────────────────────────────────"
for rom in "$EMU_BASE"/mbc2/*.gb; do
    [ -f "$rom" ] && run_test "$rom"
done
section_summary "MBC2"
echo ""

# ═══════════════════════════════════════════════════════════════════════
#  17. MBC5 (emulator-only)
# ═══════════════════════════════════════════════════════════════════════
echo "── MBC5 ──────────────────────────────────────────"
for rom in "$EMU_BASE"/mbc5/*.gb; do
    [ -f "$rom" ] && run_test "$rom"
done
section_summary "MBC5"
echo ""

# ═══════════════════════════════════════════════════════════════════════
#  GRAND SUMMARY
# ═══════════════════════════════════════════════════════════════════════
GRAND_TOTAL=$((TOTAL_PASS + TOTAL_FAIL + TOTAL_ERROR))
echo "═══════════════════════════════════════════════════"
printf "  GRAND TOTAL: %d/%d passed" "$TOTAL_PASS" "$GRAND_TOTAL"
if [ $TOTAL_FAIL -gt 0 ]; then
    printf ", %d failed" "$TOTAL_FAIL"
fi
if [ $TOTAL_ERROR -gt 0 ]; then
    printf ", %d timeout/error" "$TOTAL_ERROR"
fi
echo ""
echo "═══════════════════════════════════════════════════"
