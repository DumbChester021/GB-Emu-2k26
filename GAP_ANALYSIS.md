# GB-Emu-2k26 — Gap Analysis for Mooneye Compliance

> **Current Score: 88/97 tests passing** (80/85 acceptance + 8/12 PPU)
> **Target: All Mooneye DMG tests**

---

## Current Test Results (2026-02-13)

| Category | Pass | Total | Status |
|----------|------|-------|--------|
| CPU Instructions | 1 | 1 | ✅ Perfect |
| Bits | 3 | 3 | ✅ Perfect |
| EI/DI Timing | 4 | 4 | ✅ Perfect |
| HALT | 4 | 4 | ✅ Perfect |
| Call/JP/Ret/Pop/Push/RST | 13 | 13 | ✅ Perfect |
| ADD SP / LD HL,SP+e | 2 | 2 | ✅ Perfect |
| DIV Timing | 1 | 1 | ✅ Perfect |
| Timer | 12 | 13 | 🟡 1 failing |
| MBC1 | 13 | 13 | ✅ Perfect |
| MBC2 | 7 | 7 | ✅ Perfect |
| MBC5 | 8 | 8 | ✅ Perfect |
| Interrupts | 2 | 3 | 🟡 1 failing |
| OAM DMA | 6 | 6 | ✅ Perfect |
| PPU | 11 | 12 | 🟡 1 failing |
| Boot Regs (DMG) | 1 | 2 | 🟡 DMG0 variant |
| Boot DIV (DMG) | 0 | 2 | 🔴 Both failing |
| Boot HWIO (DMG) | 0 | 2 | 🔴 Both failing |
| Serial | 0 | 1 | 🔴 Failing |

---

## Failing Tests — Prioritized by Hardware Accuracy Impact

### ✅ Priority 1: OAM DMA Bus Conflicts — COMPLETED
**Fixed tests:** `sources-GS`, `reg_read`, `oam_dma_restart`, `oam_dma_timing`, `oam_dma_start` (all passing)

**What was done:**
Implemented SameBoy-style per-bus conflict detection. On DMG, the DMA controller owns one of two external buses (MAIN or VRAM). CPU reads from the conflicting bus return `dmaLastByte_` (the last byte DMA transferred). OAM is blocked only after the 2 M-cycle startup delay. DMA from echo RAM region (≥0xE000) now maps through `srcAddr & ~0x2000`.

Fixed `oam_dma_start` by adding `dmaRestarting_` flag: when DMA is restarted mid-transfer, OAM stays blocked through the new DMA's startup delay (because the previous DMA was already blocking). For fresh DMA, OAM is accessible during startup since `dmaDelay_ > 0`.

**Difficulty:** Complete

---

### ✅ Priority 2: Sprite Mode 3 Penalties — COMPLETED
**Fixed test:** `intr_2_mode0_timing_sprites` (now passing)

**What was done:**
Implemented per-sprite mode 3 penalty computation at OAM→XFER transition. Each sprite on a scanline adds 6 dots (fixed sprite fetch cost) plus an alignment cost of `max(0, 5 - (X + SCX) % 8)` dots. Sprites at the same X position share the alignment cost. Sprites at X ≥ 168 (off-screen right) receive zero penalty.

The penalty formula was verified against all 105 test cases in the Mooneye test ROM with 100% accuracy.

**Difficulty:** Complete

---

### ✅ Priority 3: LCD Enable Timing — COMPLETED
**Fixed tests:** `lcdon_timing-GS`, `lcdon_write_timing-GS` (both passing)

**What was done:**
Implemented accurate first-line-after-LCD-enable timing: 78-dot mode 0 → mode 3, 448-dot line, no sprite evaluation. Added 4-dot pre-OAM transition delay on normal lines (STAT stays mode 0 for dots 1-3, mode 2 at dot 4). Shifted mode 3 start from dot 80 to dot 84. Delayed LYC coincidence check to dot 4 at line boundaries.

Implemented asymmetric OAM/VRAM access pre-blocking matching SameBoy's separate blocked flags:
- OAM reads: blocked 1 dot before mode 2 (dots 3-4 during pre-OAM transition)
- VRAM reads: blocked 1 dot before mode 3 (dot 83+, while STAT still shows mode 2)
- OAM writes: blocked during mode 2 up to dot 82 only (unblocked at dot 83 before mode 3)
- VRAM writes: blocked only during actual mode 3 (no pre-blocking)

**Impact:** Also fixed the `hblank_ly_scx_timing-GS` regression — the pre-OAM delay corrected the CPU-PPU phase alignment.

**Difficulty:** Complete

---

### ✅ Priority 4: `hblank_ly_scx_timing-GS` — COMPLETED (Fixed alongside Priority 3)
**Fixed test:** `hblank_ly_scx_timing-GS` (now passing)

**What was done:**
The 4-dot pre-OAM transition delay from Priority 3 fixed the CPU-PPU phase alignment that was causing this test to fail. No additional changes needed.

**Difficulty:** Complete

---

### 🟡 Priority 5: `stat_lyc_onoff` — LYC with LCD Toggle (MEDIUM IMPACT)
**Failing test:** `stat_lyc_onoff`

**What's missing:**
When the LCD is disabled (LCDC bit 7 → 0), LY resets to 0. The test checks that:
- LYC coincidence flag in STAT is updated correctly when LCD is toggled
- The STAT interrupt fires (or doesn't) at the right time based on LYC=LY comparison during the toggle

Currently, `checkLYC()` and `updateStatIRQ()` may not be called with the right timing around LCD enable/disable transitions.

**Why it matters:**
- 1 PPU failure
- Related to Priority 3 (LCD enable/disable handling)
- Some games check LYC status after toggling LCD

**Difficulty:** Medium — likely a fix within the LCDC write handler in `writeReg()`

---

### 🟡 Priority 6: `tima_write_reloading` — Timer Edge Case (LOW-MEDIUM IMPACT)
**Failing test:** `tima_write_reloading`

**What's missing:**
Writing to TIMA (FF05) during the exact T-cycle when TMA is being reloaded into TIMA after overflow has a specific behavior:
- If the write happens **on the same cycle as the reload**, the written value should be **overwritten** by TMA
- If the write happens **one cycle before the reload**, the reload should be **cancelled**

Currently, writing to TIMA while `overflowPending_` cancels the reload entirely, but the timing of when the cancel vs. override takes effect may be off by 1 cycle.

**Why it matters:**
- 1 timer failure
- Very edge-case behavior that few games trigger
- All other 12/13 timer tests pass, so the timer model is very close

**Difficulty:** Easy-Medium — likely a 1-cycle adjustment in the overflow countdown logic in `Timer::tick()` / `Timer::write()`

---

### 🟡 Priority 7: `ie_push` — Interrupt Vector Edge Case (LOW IMPACT)
**Failing test:** `ie_push`

**What's missing:**
This tests the behavior when IE (Interrupt Enable register at 0xFFFF) is modified between the two push cycles of interrupt dispatch. During interrupt handling, the CPU pushes the high byte of PC, then the low byte. If the IE register changes between those two pushes (because the stack pointer happens to point at 0xFFFF), the interrupt vector selected should use the **new** IE value.

Currently, `handleInterrupts()` reads IE once at the start and doesn't re-read between push cycles.

**Why it matters:**
- 1 interrupt failure
- Extremely edge-case behavior — only triggers if SP is carefully positioned
- Very unlikely to affect any commercial game
- But it IS a real hardware behavior

**Difficulty:** Medium — requires splitting interrupt dispatch to re-check IE between the two push cycles

---

### ⚪ Priority 8: Boot Tests — DMG0/Boot DIV/Boot HWIO (LOW IMPACT)
**Failing tests:** `boot_regs-dmg0`, `boot_div-dmg0`, `boot_div-dmgABCmgb`, `boot_hwio-dmg0`, `boot_hwio-dmgABCmgb`

**What's missing:**
- `boot_regs-dmg0`: We emulate DMG-ABC, not DMG-0 (original revision). DMG-0 has different initial register values.
- `boot_div-*`: The DIV register should have a specific value after the bootrom finishes. Our bootrom execution may not produce the exact T-cycle count needed.
- `boot_hwio-*`: After bootrom, certain IO registers should have specific values.

**Why it matters:**
- 5 failures, but these are **bootrom-dependent** edge cases
- DMG-0 is a different hardware revision — irrelevant for DMG-ABC accuracy
- `boot_div` and `boot_hwio` depend on exact bootrom execution time, which requires matching the exact bootrom timing cycle-for-cycle
- Very unlikely to affect any commercial game (games don't depend on initial DIV value)

**Difficulty:** `boot_div`/`boot_hwio` are Medium (bootrom cycle counting), `boot_regs-dmg0` requires implementing a DMG-0 mode

---

### ⚪ Priority 9: Serial `boot_sclk_align-dmgABCmgb` (LOW IMPACT)
**Failing test:** `boot_sclk_align-dmgABCmgb`

**What's missing:**
Serial clock alignment after bootrom. The serial transfer shift clock should be synchronized to a specific phase relative to DIV after boot.

**Why it matters:**
- No games depend on serial clock alignment relative to DIV
- Serial is mainly used for link cable (Game Boy↔Game Boy communication)
- Proper serial implementation would require a full serial shift register with clock divider

**Difficulty:** Medium — but very low value for game compatibility

---

## Summary — What to Tackle and In What Order

| # | Feature | Tests Fixed | Difficulty | Game Impact |
|---|---------|-------------|------------|-------------|
| 1 | ~~**OAM DMA bus conflicts**~~ | ✅ all done | ~~Medium~~ | ✅ Completed — all 6 tests pass |
| 2 | ~~**Sprite mode 3 penalties**~~ | ✅ done | ~~Medium~~ | ✅ Completed — sprite test passes |
| 3 | ~~**LCD enable timing**~~ | ✅ +3 done | ~~Medium-Hard~~ | ✅ Completed — lcdon + hblank pass |
| 4 | **STAT LYC on/off** | +1 | Medium | Medium — LCD toggle games |
| 5 | **TIMA write during reload** | +1 | Easy-Medium | Low — rare edge case |
| 6 | **IE push edge case** | +1 | Medium | Very Low — almost never happens |
| 7 | **Boot register/DIV/HWIO** | +5 | Medium | Very Low — bootrom-only |
| 8 | **Serial clock alignment** | +1 | Medium | Very Low — link cable only |

> [!TIP]
> **Next best bang for the buck**: Item 4 (`stat_lyc_onoff`) is the last remaining PPU failure and likely a fix within the LCDC write handler. Item 5 is the easiest single fix.

> [!NOTE]
> **Not listed but also missing**: Audio/APU (no sound at all). This doesn't affect Mooneye tests but is essential for game experience.
