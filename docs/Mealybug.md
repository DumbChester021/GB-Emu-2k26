# Mealybug Tearoom Tests — DMG Compliance

> **Current Score: 1/24 DMG-blob tests passing** (work in progress)
> **Last Updated:** 2026-02-23

---

## Overview

The [Mealybug Tearoom Tests](https://github.com/mattcurrie/mealybug-tearoom-tests) verify PPU register write timing during **Mode 3** (pixel transfer). Each test writes PPU registers mid-scanline via STAT Mode 2 interrupts and checks that the visual effect appears at the exact correct pixel position.

These are extremely sensitive to the dot-to-pixel mapping — even a 1-dot timing error shifts every register-change boundary by 1 pixel.

---

## Root Cause Analysis

### The 6-Pixel Offset Problem

Pixel diff analysis (2026-02-23) reveals that register writes take effect **6 pixels too late** across all tests. Palette transitions expected at pixel X appear at pixel X+6:

| Row | Expected transition | Our transition | Shift |
|-----|---------------------|----------------|-------|
| Row 1 | x=1 | x=7 | +6 |
| Row 50 | x=2 | x=7 | +5 |
| Row 100 | x=1 | x=7 | +6 |
| Row 140 | x=1 | x=7 | +6 |

This is a **uniform offset**, not a palette-specific issue. The PPU's pixel rendering position is 6 dots behind where it should be relative to the CPU instruction pointer.

### SameBoy's Per-Register Write Conflict System

SameBoy implements a `conflict_t` enum with 14 different write timing modes in `sm83_cpu.c`. Each PPU register has its own conflict type in a DMG conflict map:

| Register | Conflict Type | Advance Before Write | Description |
|----------|---------------|------|-------------|
| BGP/OBP0/OBP1 | `PALETTE_DMG` | pending−2 | Write `old\|new` first, then real value 1T later |
| SCY | `READ_NEW` | pending−1 | Write takes effect 1T earlier than default |
| SCX | `SCX_DMG` | pending−2 | Write takes effect 2T earlier than default |
| LCDC | `DMG_LCDC` | pending−2 | Multi-phase write with OBJ/BG bit handling |
| LYC, WY | `READ_OLD` | pending (full) | Default timing — write at end of M-cycle |
| WX | `WX_DMG` | pending (full) | Sets `wx_just_changed` flag for 1T |

### What We've Ruled Out

Extensive testing (2026-02-22/23) confirmed the 6-pixel offset is **NOT** caused by:

| Approach Tested | BGP Diff | Mooneye | Why It Failed |
|-----------------|----------|---------|---------------|
| **Baseline (no change)** | 4368 | 94/94 ✓ | Reference point |
| Pipeline reorder (render→fetcher) | 3658 | 91/94 ✗ | Helped palette but broke `lcdon_timing`, `intr_2_mode0_timing_sprites` |
| End-of-tick palette snapshot (1-dot) | 5078 | 94/94 ✓ | 1-dot delay is too much — worsened diffs |
| OR intermediate + 3-dot delay | 5601 | 94/94 ✓ | OR corrupts palette, delay too much |
| Simple 2-dot palette delay | 5787 | 94/94 ✓ | Any delay in wrong direction |
| tick-then-write CPU (all writes) | 7223 | 63/94 ✗ | Catastrophic — 31 Mooneye regressions |
| Per-register write conflict delays | 5787 | 94/94 ✓ | Palette diffs worsened, write timing not root cause |
| Mode 3 penalty = 0 | 7944 | 87/94 ✗ | Too aggressive, 7 Mooneye regressions |

**Key insight**: every form of **write delay** makes palette tests worse. The write must arrive at T0 (earliest possible). The 6-pixel offset is from Mode 3 pixel rendering starting too late relative to CPU cycle count — a PPU startup alignment issue, not a CPU write timing issue.

### CPU Write-Before-Tick Ordering

Our CPU uses write-before-tick bus ordering:
```cpp
void CPU::writeByte(uint16_t addr, uint8_t val) {
    bus_.write(addr, val);  // Register updated immediately at T0
    tick4();                // PPU advances 4 dots AFTER write
}
```

This is the same write-before-tick ordering that passes all 94 Mooneye tests — changing it globally breaks interrupt timing, LCD-on timing, and DMA timing tests.

---

## Remaining Investigation Areas

### Mode 3 Startup Timing

Our first pixel appears at dot **96** from line start (Mode 3 at dot 84, +5 penalty, +7 fetcher warmup). SameBoy's first pixel appears at dot ~**90** (pre-filled junk FIFO + 7-step fetcher pipeline). The ~6 dot difference matches the observed offset.

SameBoy handles Mode 3 startup differently:
- Pre-fills BG FIFO with 8 junk pixels at Mode 3 start
- Starts `position_in_line` at −16 (uint8_t = 240)
- Discards junk pixels while fetcher warms up in parallel
- First real pixel at `position_in_line = 0` after ~8 cycles of junk + SCX discard

Our model uses penalty dots where NO rendering occurs, then starts the fetcher from scratch. This sequential approach adds 5 extra dots of dead time that SameBoy avoids by running junk discard and fetcher warmup in parallel.

### What Would Fix It

Implementing SameBoy's junk-FIFO approach would allow the fetcher to warm up during pixel discard, eliminating the 5-6 dot overhead. This is a significant PPU architectural change requiring:
1. Pre-fill BG FIFO with 8 junk pixels at Mode 3 start
2. Replace penalty dots with a signed `position_in_line` counter starting at −16
3. Run rendering + fetcher simultaneously from Mode 3 start (discard junk while fetching)
4. Only output to framebuffer when position ≥ 0

---

## Test Categories

### 24 DMG-blob Tests

| Category | Tests | Description |
|----------|-------|-------------|
| **Palette** | `m3_bgp_change`, `m3_bgp_change_sprites`, `m3_obp0_change` | Write BGP/OBP0 during Mode 3 |
| **LCDC bits** | `m3_lcdc_bg_en_change`, `m3_lcdc_bg_map_change`, `m3_lcdc_tile_sel_change`, `m3_lcdc_tile_sel_win_change`, `m3_lcdc_obj_en_change`, `m3_lcdc_obj_en_change_variant`, `m3_lcdc_obj_size_change`, `m3_lcdc_obj_size_change_scx`, `m3_lcdc_win_en_change_multiple`, `m3_lcdc_win_en_change_multiple_wx`, `m3_lcdc_win_map_change` | Toggle LCDC bits during Mode 3 |
| **Scroll** | `m3_scx_low_3_bits`, `m3_scx_high_5_bits`, `m3_scy_change` | Write SCX/SCY during Mode 3 |
| **Window** | `m3_window_timing`, `m3_window_timing_wx_0`, `m3_wx_4_change`, `m3_wx_4_change_sprites`, `m3_wx_5_change`, `m3_wx_6_change`, `m2_win_en_toggle` | WX changes and window activation during Mode 3 |

### Currently Passing

| Test | Status |
|------|--------|
| `m2_win_en_toggle` | ✅ Pass |

---

## Key Technical Notes

### Test Infrastructure

- Tests use `LD B,B` ($40) as a breakpoint — emulator dumps LCD at this instruction
- Expected images use greyscale: `$00`, `$55`, `$AA`, `$FF`
- Our DMG palette maps: `E0F8D0→FF`, `88C070→AA`, `346856→55`, `081820→00`
- Run via `bash run_mealybug.sh` (uses ImageMagick for palette remapping + pixel compare)

### `line_0_fix` Macro

Many tests use this macro to handle the 4-cycle timing difference on scanline 0:
```asm
line_0_fix: MACRO
    ldh a, [rLY]    ; 12T
    and a            ; 4T
    jr nz, .target   ; 8T (not taken) or 12T (taken, LY=0)
.target
    ENDM
```
Takes 24T normally, 28T when LY=0, compensating for line 0's different mode 3 start timing.

### Fetcher Pipeline Timing
```
Mode 3 start → 5 penalty dots → ReadTileID (2) → ReadTileDataLow (2) →
ReadTileDataHigh (2) → PushToFIFO (2) → first pixel pushed
Total: 5 + 8 = 13 dots before first pixel
Mode 3 duration: 13 + 159 remaining pixels = 172 dots ✓
```

### Register Read Points in Fetcher

| Register | Read At | Stage |
|----------|---------|-------|
| LCDC bit 3 (BG map) | ReadTileID | Tile map base address |
| LCDC bit 4 (Tile select) | ReadTileDataLow/High | Tile data base address |
| LCDC bit 6 (Window map) | ReadTileID (window) | Window tile map base |
| SCX high 5 bits | ReadTileID | Tile X column |
| SCY | ReadTileID + ReadTileDataLow/High | Tile Y row |
| BGP/OBP0/OBP1 | pushPixel() | Applied at pixel output |
| LCDC bit 0 (BG enable) | pushPixel() | Blanks BG color |
| LCDC bit 1 (OBJ enable) | tickPixelTransfer() | Controls sprite mixing |
| WX | tickPixelTransfer() | Window trigger check |