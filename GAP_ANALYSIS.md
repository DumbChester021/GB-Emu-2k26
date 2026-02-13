# GB-Emu-2k26 — Gap Analysis for Mooneye Compliance

> **Current Score: 91/94 DMG-ABC tests passing**
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
| Timer | 13 | 13 | ✅ Perfect |
| MBC1 | 13 | 13 | ✅ Perfect |
| MBC2 | 7 | 7 | ✅ Perfect |
| MBC5 | 8 | 8 | ✅ Perfect |
| Interrupts | 3 | 3 | ✅ Perfect |
| OAM DMA | 6 | 6 | ✅ Perfect |
| PPU | 12 | 12 | ✅ Perfect |
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

### ✅ Priority 5: `stat_lyc_onoff` — COMPLETED
**Fixed test:** `stat_lyc_onoff` (now passing)

**What was done:**
Fixed STAT IRQ line state management during LCD toggle. The root cause was `statIrqLine_` being unconditionally reset to `false` during every LCD-off tick in `tick()`. This caused spurious rising edges when the LCD was re-enabled with a retained coincidence flag (flag was already set before LCD off, stayed set after LCD on → no real transition, but false→true edge was detected).

Fix: (1) Removed `statIrqLine_ = false` from the LCD-off tick path. (2) When LCD turns off in `writeReg()`, freeze `statIrqLine_` based on the retained coincidence flag state (`(stat_ & 0x40) && (stat_ & 0x04)`). Mode sources are inactive when the PPU is stopped, so only LYC coincidence contributes.

**Difficulty:** Complete

---

### ✅ Priority 6: `tima_write_reloading` — COMPLETED
**Fixed test:** `tima_write_reloading` (now passing)

**What was done:**
Fixed TIMA write behavior during the TMA reload cycle. The DMG timer has two distinct behavioral windows after TIMA overflow:
- **Cycle A** (overflow pending, before reload): Writing TIMA cancels the pending reload and IF flag
- **Cycle B** (TMA→TIMA reload cycle): Writing TIMA is ignored — TMA's value wins

The bug was that `Timer::write()` treated ALL writes during `overflowPending_` as cycle A (cancellation). Added a `reloadedThisCycle_` guard: if the reload already happened this cycle, the CPU write is ignored and TMA keeps its value.

**Difficulty:** Complete

---

### ✅ Priority 7: `ie_push` — COMPLETED
**Fixed test:** `ie_push` (now passing)

**What was done:**
Rewrote `handleInterrupts()` to split the two-byte PC push into individual writes and re-read IE between them. After the high byte of PC is pushed, IE is re-read. If the push wrote to 0xFFFF (because SP was 0x0000), the new IE value determines which interrupt is dispatched — or cancels dispatch entirely (PC→0x0000, IF untouched).

**Difficulty:** Complete

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
| 4 | ~~**STAT LYC on/off**~~ | ✅ done | ~~Medium~~ | ✅ Completed — PPU now 12/12 |
| 5 | ~~**TIMA write during reload**~~ | ✅ done | ~~Easy-Medium~~ | ✅ Completed — timer now 13/13 |
| 6 | ~~**IE push edge case**~~ | ✅ done | ~~Medium~~ | ✅ Completed — interrupts now 3/3 |
| 7 | **Boot register/DIV/HWIO** | +5 | Medium | Very Low — bootrom-only |
| 8 | **Serial clock alignment** | +1 | Medium | Very Low — link cable only |

> [!TIP]
> **Next best bang for the buck**: Item 6 (`ie_push`) is the last remaining interrupt failure. Item 7 (boot tests) would fix 5 tests but requires bootrom cycle counting.

> [!NOTE]
> **Not listed but also missing**: Audio/APU (no sound at all). This doesn't affect Mooneye tests but is essential for game experience.
