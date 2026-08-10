# GB-Emu-2k26 — DMG Accuracy Gap Analysis

> **Current Score: 89/94 selected DMG-ABC tests passing** ⚠️
> **Target: hardware-accurate DMG-CPU B pipeline; temporary regressions are tracked**
> **DMG-ACID2: PASSING** ✅
> **Blargg: CPU/timing/APU/HALT and OAM bug suites pass** ✅
> **Mealybug Tearoom: 5/24 exact** ⚠️
> **Last Verified: 2026-08-11 (rebuilt and executed from current code)**

---

## Current Test Results (2026-08-11)

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
| PPU | 7 | 12 | ⚠️ Hardware rewrite in progress |
| Boot Regs (DMG-ABC) | 1 | 1 | ✅ Perfect |
| Boot DIV (DMG-ABC) | 1 | 1 | ✅ Perfect |
| Boot HWIO (DMG-ABC) | 1 | 1 | ✅ Perfect |
| Serial | 1 | 1 | ✅ Perfect |
| **Blargg dmg_sound** | **12** | **12** | ✅ **Perfect** |
| Blargg cpu_instrs | 11 | 11 | ✅ Perfect |
| Blargg instr_timing | 1 | 1 | ✅ Perfect |
| Blargg mem_timing | 3 | 3 | ✅ Perfect |
| Blargg mem_timing-2 | 3 | 3 | ✅ Perfect |
| Blargg halt_bug | 1 | 1 | ✅ Perfect |
| Blargg oam_bug | 8 | 8 | ✅ Perfect |
| Blargg oam_bug-2 | 8 | 8 | ✅ Perfect |
| Mealybug Tearoom | 5 | 24 | ⚠️ 19 exact image comparisons fail |

> [!NOTE]
> `cpu_instrs`, `instr_timing`, and the original `mem_timing` print passing
> results over serial; the current headless runner nevertheless returns timeout
> because it only recognizes the external-RAM completion protocol. `halt_bug`
> was verified from its rendered "Passed" result. cgb_sound is CGB-only.

> [!NOTE]
> DMG-0 and SGB/MGB-only variant tests are excluded from this count. We target DMG-B (same as SameBoy). See [SameBoy Cross-Reference](#sameboy-cross-reference) section below.

---

## Failing Tests

All selected non-PPU Mooneye tests and the current Blargg DMG suites pass. The
per-dot PPU replacement intentionally removed synthetic penalty rounding and
currently exposes these remaining gaps:

- Mooneye PPU: `hblank_ly_scx_timing-GS`, `intr_1_2_timing-GS`,
  `intr_2_mode0_timing_sprites`, `lcdon_timing-GS`, and
  `lcdon_write_timing-GS` fail.
- Mealybug Tearoom: 19/24 fail. The exact passes are `m2_win_en_toggle`,
  `m3_scx_low_3_bits`, `m3_wx_4_change`, `m3_wx_4_change_sprites`, and
  `m3_wx_5_change`.
- MBC3 RTC is unimplemented and has not passed `rtc3test`.

---

## Completed Fixes

| # | Feature | Tests Fixed | Status |
|---|---------|-------------|--------|
| 1 | **OAM DMA bus conflicts** | `sources-GS`, `reg_read`, `oam_dma_restart`, `oam_dma_timing`, `oam_dma_start` | ✅ All 6 pass |
| 2 | **Real OBJ FIFO/fetch stalls** | Mealybug sprite behavior | ⚠️ Implemented; exact transition timing remains |
| 3 | **LCD enable timing** | `lcdon_timing-GS`, `lcdon_write_timing-GS` | ⚠️ Revalidation needed after pipeline rewrite |
| 4 | **STAT LYC on/off** | `stat_lyc_onoff` | ✅ Pass |
| 5 | **TIMA write during reload** | `tima_write_reloading` | ✅ Pass |
| 6 | **IE push edge case** | `ie_push` | ✅ Pass |
| 7 | **Boot DIV** | `boot_div-dmgABCmgb` | ✅ Pass |
| 8 | **Boot HWIO** | `boot_hwio-dmgABCmgb` | ✅ Pass |
| 9 | **Boot SCLK Align** | `boot_sclk_align-dmgABCmgb` | ✅ Pass |
| 10 | **SCX fine-scroll pipeline** | Mealybug SCX effects | ⚠️ Synthetic M-cycle rounding removed |
| 11 | **DMG-B OAM corruption** | Blargg `oam_bug`, `oam_bug-2` | ✅ Both 8/8 |
| 12 | **SM83 bus-phase scheduler** | EI/DI, HALT, timer, interrupt entry, DMG I/O conflicts | ✅ Core timing categories pass |

---

## SameBoy Cross-Reference

The bundled SameBoy source is the behavioral reference for the replacement
pipeline. This project does not currently match SameBoy on the full selected
Mooneye subset: the measured score is 89/94.

### Variant Confirmation

SameBoy **only implements `GB_MODEL_DMG_B`** (source: [`Core/model.h`](https://github.com/LIJI32/SameBoy/blob/master/Core/model.h)). DMG-0, DMG-A, DMG-C are commented out. SameBoy passes **all 94 Mooneye DMG acceptance tests** we run.

**Current project score: 89/94; SameBoy remains the oracle, not a claimed parity result.**

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

> [!TIP]
> **Next accuracy target: finish edge ordering around the new mode-3 pipeline.**
> The real OBJ FIFO/fetcher and CPU bus-phase scheduler are now present. Align
> PPU consumption edges, HBlank/Mode-2 transitions, first-line access windows,
> and the WX=6 comparator,
> then drive both Mooneye and Mealybug back upward without restoring synthetic
> penalty formulas.

> [!NOTE]
> **APU is implemented and Blargg `dmg_sound` is verified at 12/12.** Broader
> SameSuite and hardware-output validation is still needed before claiming full
> APU hardware accuracy.

---

## Game Compatibility Fixes (2026-02-22)

| Fix | Description | Games Affected |
|-----|-------------|----------------|
| WX < 7 clipping | Clips `(7 - WX)` pixels when window starts at left edge | Games using WX=0–6 |
| MBC3 bank 0 (known bug) | Current code maps a written bank 0 to bank 0; hardware remaps it to bank 1 | MBC3 titles |
| HALT bug | Already implemented — PC double-read with IME=0 | Pokémon Yellow, edge cases |
| Window discard fix | SCX fine-scroll discard no longer bleeds into Window layer | Zelda: Link's Awakening HUD jitter |
| Window line counter fix | Counter only increments when window actually renders (not just LY >= WY) | DMG-ACID2 chin, games using WX mid-frame |
| DMA VRAM bypass | OAM DMA reads VRAM directly, bypassing PPU mode 3 blocking | Games running DMA during mode 3 |
| LY=153 early reset | LY resets to 0 after ~4 dots on scanline 153 (DMG quirk) | Games polling LY==0 during VBlank |
| Unused OAM 0x00 | 0xFEA0–0xFEFF reads return 0x00 (DMG behavior, was 0xFF) | Daiku no Gen-san, Tokyo Disneyland |
| WX≥167 guard | Window cannot trigger at pixel ≥160 (prevents single-pixel artifact) | Games disabling window mid-frame |
| STAT write glitch | Writing STAT fires spurious interrupt when IRQ line is low (DMG bug) | Road Rash, Zerd no Densetsu |
| VBlank frame presentation | Present framebuffer at VBlank, not after full T-cycle batch (prevents scroll tearing) | Metroid 2, all scrolling games |

---

## DMG-ACID2 (2026-02-22)

**Status: PASSING** ✅

![DMG-ACID2 Passing](dmg_acid2_pass.png)

The [DMG-ACID2](https://github.com/mattcurrie/dmg-acid2) test validates correct PPU rendering of background, window, objects, palettes, tile data addressing, and OBJ priority. All visual elements render correctly:
- "Hello World" text (10-sprite limit, BG exclamation mark shows through)
- Hair hidden by LCDC bit 0 disable
- Eyes (BG left, Window right, OBJ-to-BG priority)
- Nose (sprite flipping)
- Mouth (8×16 sprites, tile index bit 0 ignored)
- Chin (window resumes after WX set offscreen)
- Footer text ($9C00 tile map, $8800 tile data)

### Not Yet Implemented
- **MBC3 RTC**: Real-Time Clock registers stubbed (Pokémon GSC time features)
- **Mode-3 edge timing**: 19/24 Mealybug tests fail exact comparison
- **PPU consumption edges**: several register changes reach the fetch/output
  pipeline on the wrong dot despite their explicit CPU bus phases
