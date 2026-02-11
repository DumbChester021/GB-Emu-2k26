#!/bin/bash

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
echo "  Mooneye CPU Acceptance Tests (DMG)"
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

    timeout 10 ./"$EXECUTABLE" --test "$rom" > /dev/null 2>&1
    local RC=$?

    if [ $RC -eq 0 ]; then
        echo "  ✓ [$dir] $name"
        PASS=$((PASS + 1))
    elif [ $RC -eq 1 ]; then
        echo "  ✗ [$dir] $name"
        FAIL=$((FAIL + 1))
    else
        echo "  ? [$dir] $name (timeout/error rc=$RC)"
        ERROR=$((ERROR + 1))
    fi
}

# ── CPU Instruction Tests ──────────────────────────────────────────────
echo "── Instructions ──────────────────────────────────"
for rom in "$TEST_BASE"/instr/*.gb; do
    [ -f "$rom" ] && run_test "$rom"
done
echo ""

# ── Bits Tests (reg_f, mem_oam, unused_hwio) ───────────────────────────
echo "── Bits ──────────────────────────────────────────"
for rom in "$TEST_BASE"/bits/*.gb; do
    [ -f "$rom" ] && run_test "$rom"
done
echo ""

# ── Interrupt Tests ────────────────────────────────────────────────────
echo "── Interrupts ────────────────────────────────────"
for rom in "$TEST_BASE"/interrupts/*.gb; do
    [ -f "$rom" ] && run_test "$rom"
done
if [ -f "$TEST_BASE/if_ie_registers.gb" ]; then
    run_test "$TEST_BASE/if_ie_registers.gb"
fi
if [ -f "$TEST_BASE/intr_timing.gb" ]; then
    run_test "$TEST_BASE/intr_timing.gb"
fi
echo ""

# ── EI / DI Timing ────────────────────────────────────────────────────
echo "── EI / DI Timing ─────────────────────────────────"
for t in ei_sequence ei_timing di_timing-GS; do
    [ -f "$TEST_BASE/$t.gb" ] && run_test "$TEST_BASE/$t.gb"
done
echo ""

# ── HALT Tests ─────────────────────────────────────────────────────────
echo "── HALT ──────────────────────────────────────────"
for t in halt_ime0_ei halt_ime0_nointr_timing halt_ime1_timing halt_ime1_timing2-GS; do
    [ -f "$TEST_BASE/$t.gb" ] && run_test "$TEST_BASE/$t.gb"
done
echo ""

# ── CALL / JP / RET / POP / PUSH / RST Timing ────────────────────────
echo "── Call/JP/Ret/Pop/Push/RST Timing ─────────────"
for t in call_timing call_timing2 call_cc_timing call_cc_timing2 \
         jp_timing jp_cc_timing \
         ret_timing ret_cc_timing reti_timing reti_intr_timing \
         rst_timing pop_timing push_timing; do
    [ -f "$TEST_BASE/$t.gb" ] && run_test "$TEST_BASE/$t.gb"
done
echo ""

# ── ADD SP / LD HL,SP+e Timing ────────────────────────────────────────
echo "── ADD SP / LD HL,SP+e Timing ──────────────────"
for t in add_sp_e_timing ld_hl_sp_e_timing; do
    [ -f "$TEST_BASE/$t.gb" ] && run_test "$TEST_BASE/$t.gb"
done
echo ""

# ── DIV Timing ─────────────────────────────────────────────────────────
echo "── DIV Timing ────────────────────────────────────"
if [ -f "$TEST_BASE/div_timing.gb" ]; then
    run_test "$TEST_BASE/div_timing.gb"
fi
echo ""

# ── Timer Tests ────────────────────────────────────────────────────────
echo "── Timer ─────────────────────────────────────────"
for rom in "$TEST_BASE"/timer/*.gb; do
    [ -f "$rom" ] && run_test "$rom"
done
echo ""

# ── OAM DMA ──────────────────────────────────────────────────────────
echo "── OAM DMA ─────────────────────────────────────"
for rom in "$TEST_BASE"/oam_dma/*.gb; do
    [ -f "$rom" ] && run_test "$rom"
done
for t in oam_dma_restart oam_dma_start oam_dma_timing; do
    [ -f "$TEST_BASE/$t.gb" ] && run_test "$TEST_BASE/$t.gb"
done
echo ""

# ── Boot Registers (DMG) ──────────────────────────────────────────────
echo "── Boot Registers (DMG) ────────────────────────"
for t in boot_regs-dmgABC; do
    [ -f "$TEST_BASE/$t.gb" ] && run_test "$TEST_BASE/$t.gb"
done
echo ""

# ── Summary ────────────────────────────────────────────────────────────
TOTAL=$((PASS + FAIL + ERROR))
echo "═══════════════════════════════════════════════════"
printf "  Results: %d/%d passed" "$PASS" "$TOTAL"
if [ $FAIL -gt 0 ]; then
    printf ", %d failed" "$FAIL"
fi
if [ $ERROR -gt 0 ]; then
    printf ", %d timeout/error" "$ERROR"
fi
echo ""
echo "═══════════════════════════════════════════════════"

if [ $FAIL -eq 0 ] && [ $ERROR -eq 0 ]; then
    exit 0
else
    exit 1
fi
