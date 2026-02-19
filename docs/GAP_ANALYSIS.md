# GB-Emu-2k26 — Gap Analysis for Mooneye Compliance

> **Current Score: 91/94 DMG-ABC tests passing**
> **Target: All Mooneye DMG tests (94/94)**
> **Last Updated: 2026-02-19**

---

## Current Test Results (2026-02-19)

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
| PPU | 11 | 12 | 🟡 1 failing |
| Boot Regs (DMG-ABC) | 1 | 1 | ✅ Perfect |
| Boot DIV (DMG-ABC) | 1 | 1 | ✅ Perfect |
| Boot HWIO (DMG-ABC) | 0 | 1 | 🔴 Failing |
| Serial | 0 | 1 | 🔴 Failing |
| **Blargg dmg_sound** | **12** | **12** | ✅ **Perfect** |
| Blargg cpu_instrs | all | all | ✅ Perfect |
| Blargg instr_timing | 1 | 1 | ✅ Perfect |
| Blargg mem_timing | all | all | ✅ Perfect |

> [!NOTE]
> DMG-0 and SGB/MGB-only variant tests are excluded from this count. We target DMG-B (same as SameBoy). See [SameBoy Cross-Reference](#sameboy-cross-reference) section below.

---

## Failing Tests — 3 Remaining

### 🔴 Priority 1: `hblank_ly_scx_timing-GS` (PPU)

**Status:** FAILING — was previously fixed, has regressed
**SameBoy DMG-B:** ✅ PASS — **this is a real bug, NOT a variant issue**
**Suffix `-GS`:** Applies to DMG+MGB+SGB+SGB2

**What it tests:**
HBlank timing and LY register updates as a function of SCX scroll values. Mode 3 length depends on `SCX % 8`, which shifts when HBlank begins. The test measures LY changes at specific cycle offsets after synchronizing to a known PPU phase.

**Root cause analysis:**
The test was previously passing after Fix 6 (LCD enable timing + 4-dot pre-OAM delay). It has since regressed, likely due to a subtle timing change in the PPU state machine affecting the CPU-PPU phase alignment for HBlank transitions.

**Fix approach:**
1. Add cycle traces at HBlank→new scanline transition with various SCX values
2. Compare PPU dot counter at LY increment against SameBoy's behavior
3. The mode 3 duration formula `172 + (SCX % 8) + sprite_penalties` may need re-verification
4. Check if the 4-dot pre-OAM delay is still correctly applied

**Difficulty:** Medium-Hard — subtle per-dot PPU timing issue
**Game Impact:** Medium — affects scroll-heavy games

---

### 🔴 Priority 2: `boot_hwio-dmgABCmgb` (Boot HWIO)

**Status:** FAILING
**SameBoy DMG-B:** ✅ PASS — **real bug, NOT variant-related**
**Suffix `-dmgABCmgb`:** Applies to DMG-A/B/C + MGB (NOT DMG-0)

**What it tests:**
Checks ALL hardware I/O registers (`0xFF00`–`0xFF7F`) for their expected values immediately after the boot ROM finishes executing.

**Root cause analysis:**
One or more I/O registers are not initialized to the correct post-boot state. Likely related to the `boot_sclk_align` failure — the serial register (`SC` at `0xFF02`) or other I/O registers may have wrong initial values.

**Fix approach:**
1. Run the test with verbose tracing to identify which specific register(s) have wrong values
2. Cross-reference expected post-boot values against [Pan Docs](https://gbdev.io/pandocs/Power_Up_Sequence.html) and SameBoy's `GB_reset_internal_state()` in `gb.c`
3. Fix register initialization in `MemoryBus` constructor or boot ROM execution path

**Difficulty:** Easy-Medium — once the wrong register is identified, the fix is straightforward
**Game Impact:** Very Low — games don't depend on exact I/O state after boot

---

### 🔴 Priority 3: `boot_sclk_align-dmgABCmgb` (Serial)

**Status:** FAILING
**SameBoy DMG-B:** ✅ PASS — **real bug, NOT variant-related**
**Suffix `-dmgABCmgb`:** Applies to DMG-A/B/C + MGB (NOT DMG-0)

**What it tests:**
The serial transfer clock alignment state after the boot ROM finishes. Verifies that the internal serial shift clock phase is correct relative to DIV after boot.

**Root cause analysis:**
The serial transfer control register (`SC` at `0xFF02`) or the internal serial shift register clock phase is not correctly initialized. This may be fixed automatically by fixing `boot_hwio` if the wrong register is SC.

**Fix approach:**
1. Check if fixing `boot_hwio` fixes this test too (likely related)
2. If not, implement proper serial shift register with clock divider phase tracking
3. Ensure SC register initial value matches DMG-B post-boot state

**Difficulty:** Medium — may require implementing serial clock phase tracking
**Game Impact:** Very Low — no games depend on serial clock alignment

---

## Completed Fixes

| # | Feature | Tests Fixed | Status |
|---|---------|-------------|--------|
| 1 | **OAM DMA bus conflicts** | `sources-GS`, `reg_read`, `oam_dma_restart`, `oam_dma_timing`, `oam_dma_start` | ✅ All 6 pass |
| 2 | **Sprite mode 3 penalties** | `intr_2_mode0_timing_sprites` | ✅ Pass |
| 3 | **LCD enable timing** | `lcdon_timing-GS`, `lcdon_write_timing-GS` | ✅ Both pass |
| 4 | **STAT LYC on/off** | `stat_lyc_onoff` | ✅ Pass |
| 5 | **TIMA write during reload** | `tima_write_reloading` | ✅ Pass |
| 6 | **IE push edge case** | `ie_push` | ✅ Pass |
| 7 | **Boot DIV** | `boot_div-dmgABCmgb` | ✅ Now passing |

---

## SameBoy Cross-Reference

### Variant Confirmation

SameBoy **only implements `GB_MODEL_DMG_B`** (source: [`Core/model.h`](https://github.com/LIJI32/SameBoy/blob/master/Core/model.h)). DMG-0, DMG-A, DMG-C are commented out. SameBoy passes **all 94 Mooneye DMG acceptance tests** we run.

**All 3 of our failures are genuine bugs, not DMG variant differences.**

### DMG Revision Differences (for reference)

| Feature | DMG-0 | DMG-A | DMG-B | DMG-C |
|---------|-------|-------|-------|-------|
| Boot logo ® | ❌ | ✅ | ✅ | ✅ |
| Wave RAM retrigger | Different | "Wave glitch" | ✅ Normal | Same as B |
| CPU/PPU/Timer | Same | Same | Same | Same |

Only boot ROM and APU wave behavior differ between DMG revisions. Everything else is identical.

### Mooneye Naming Convention

| Suffix | Models | Description |
|--------|--------|-------------|
| (none) | All | Universal test |
| `-GS` | DMG+MGB+SGB+SGB2 | G=DMG+MGB, S=SGB family |
| `-dmg0` | DMG-0 | Earliest revision only |
| `-dmgABC` | DMG-A/B/C | Post-0 DMG |
| `-dmgABCmgb` | DMG-A/B/C+MGB | Our target |
| `-C` | CGB+AGB+AGS | Game Boy Color family |

---

## What to Tackle Next

| # | Test | Difficulty | Game Impact | Recommended? |
|---|------|------------|-------------|--------------|
| 1 | `hblank_ly_scx_timing-GS` | Medium-Hard | Medium | ✅ Yes — PPU accuracy |
| 2 | `boot_hwio-dmgABCmgb` | Easy-Medium | Very Low | 🟡 Optional |
| 3 | `boot_sclk_align-dmgABCmgb` | Medium | Very Low | 🟡 Optional |

> [!TIP]
> **Start with `hblank_ly_scx_timing-GS`** — it's the highest-impact remaining failure and tests core PPU behavior that affects real games. The boot/serial tests are edge cases with negligible game impact.

> [!NOTE]
> **APU is fully implemented**: Hardware-accurate DMG APU with all 4 channels, frame sequencer, stereo mixing, and SDL2 audio output. **Blargg `dmg_sound` 12/12 passing** — all tests match SameBoy output.
