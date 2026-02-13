# GB-Emu-2k26 — Gap Analysis for Mooneye Compliance

> **Current Score: 85/97 tests passing** (77/85 acceptance + 8/12 PPU)
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
| PPU | 8 | 12 | 🔴 4 failing |
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

### 🔴 Priority 3: LCD Enable Timing (MEDIUM-HIGH IMPACT)
**Failing tests:** `lcdon_timing-GS`, `lcdon_write_timing-GS`

**What's missing:**
The exact behavior of the first frame after LCD is turned on (LCDC bit 7 set from 0→1) is not accurately modeled. On real hardware:
- Line 0 starts with a shortened Mode 2 (no OAM evaluation)
- The transition into Mode 3 happens at a specific dot offset
- Writes to certain registers during this first line behave differently

Currently, `firstLineAfterEnable_` starts in Mode 0 for 4 dots then jumps to Mode 3, which doesn't match hardware.

**Why it matters:**
- 2 PPU test failures
- Games that toggle the LCD (e.g., during screen transitions) rely on predictable timing after re-enable
- Less commonly tested in games than sprite penalties, but still important for accuracy

**Difficulty:** Medium-Hard — requires careful study of DMG hardware documentation for the exact LCD enable sequence

---

### 🟡 Priority 4: `hblank_ly_scx_timing-GS` Regression (MEDIUM IMPACT)
**Failing test:** `hblank_ly_scx_timing-GS`

**What's missing:**
This test previously passed but regressed when the CPU bus ordering was changed from "tick-before-read" to "read-before-tick" (Fix #3 in DEVLOG). The test checks that HBlank duration varies correctly with SCX value, and it's sensitive to the exact CPU-PPU phase alignment.

**Why it matters:**
- 1 PPU failure that used to pass
- The real DMG CPU reads the bus at **T3 of the M-cycle** (not T0), which is between read-before-tick and tick-before-read
- A proper fix likely requires **sub-M-cycle bus accuracy** — ticking 1 dot at a time inside `readByte()` with the read happening at dot 3

**Difficulty:** Hard — may require fundamental changes to the CPU-PPU synchronization model. This is the deepest timing issue.

**Possible approaches:**
1. **Sub-M-cycle ticking**: Change `readByte()` to tick 3 dots → read → tick 1 dot. Most accurate but invasive.
2. **Post-increment trick**: Adjust `dotCounter_` initialization or increment phase. Simpler but hacky.
3. **Separate IO read path**: Read-before-tick for IO registers (STAT, LY), tick-before-read for memory. May not fully solve it.

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
| 3 | **LCD enable timing** | +2 | Medium-Hard | Medium — screen transitions |
| 4 | **HBlank/SCX timing (sub-M-cycle)** | +1 | Hard | Medium — raster effects |
| 5 | **STAT LYC on/off** | +1 | Medium | Medium — LCD toggle games |
| 6 | **TIMA write during reload** | +1 | Easy-Medium | Low — rare edge case |
| 7 | **IE push edge case** | +1 | Medium | Very Low — almost never happens |
| 8 | **Boot register/DIV/HWIO** | +5 | Medium | Very Low — bootrom-only |
| 9 | **Serial clock alignment** | +1 | Medium | Very Low — link cable only |

> [!TIP]
> **Next best bang for the buck**: Item 2 (sprite penalties) gives foundational PPU accuracy affecting real games. Item 6 is the easiest single fix. Items 3 and 5 are related (LCD enable/disable handling) and could be tackled together.

> [!NOTE]
> **Not listed but also missing**: Audio/APU (no sound at all). This doesn't affect Mooneye tests but is essential for game experience.
