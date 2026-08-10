# Mealybug Tearoom Tests — DMG Compliance

> **Current Score: 4/24 DMG-blob tests passing** (hardware rewrite in progress)
> **Last Updated:** 2026-08-11

---

## Overview

The [Mealybug Tearoom Tests](https://github.com/mattcurrie/mealybug-tearoom-tests) verify PPU register write timing during **Mode 3** (pixel transfer). Each test writes PPU registers mid-scanline via STAT Mode 2 interrupts and checks that the visual effect appears at the exact correct pixel position.

These are extremely sensitive to the dot-to-pixel mapping — even a 1-dot timing error shifts every register-change boundary by 1 pixel.

---

## Current Hardware Model

The earlier bulk renderer and precomputed Mode-3 penalty formula have been
replaced. The current implementation now has:

- an eight-pixel BG FIFO and an independent eight-pixel OBJ FIFO;
- a seven-phase, one-dot-at-a-time tile fetcher;
- signed internal X beginning at -16 with eight junk pixels preloaded;
- progressive Mode-2 OAM search, one entry every two dots;
- real object-fetch alignment, six fetch dots, FIFO overlay, and DMG priority;
- dynamic SCX/SCY, tile-map, tile-select, object-size, and palette sampling;
- WY/WX latches, internal window Y, reactivation pixels, and WX write state;
- one-dot DMG palette conflicts and multi-phase LCDC conflicts.

This is a structural hardware model rather than a screenshot-specific offset
table. Temporary regressions are accepted while its edge ordering is completed.

## Measured Snapshot

Fresh `bash run_mealybug.sh` output on 2026-08-11 is **4/24**. During
development, compensating Mode-2/startup offsets reached 16/24, but those
offsets contradicted the measured one-dot Mode-2 interrupt lead and were removed.

The remaining work is concentrated in:

- CPU/PPU ordering within an SM83 M-cycle for SCX, SCY, palettes, LCDC, and WX;
- the same-dot WX=6 comparator/write race;
- exact first-line LCD-enable and HBlank transition timing;
- OBJ fetch termination/abort edges and grouped-sprite penalties.

The bundled SameBoy core is used as the behavioral oracle. Its conflict map is
still relevant: BGP/OBP use `PALETTE_DMG`, LCDC uses `DMG_LCDC`, SCX has a DMG
early-write path, SCY reads the new value, and WX raises `wx_just_changed` for
one T-cycle.

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
| `m3_wx_4_change` | ✅ Pass |
| `m3_wx_4_change_sprites` | ✅ Pass |
| `m3_wx_5_change` | ✅ Pass |

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

The fetcher now advances through `GetTileT1/T2`, `GetTileDataLowT1/T2`,
`GetTileDataHighT1/T2`, and `Push`. BG output and fetching run concurrently;
OBJ fetches pause that loop dynamically. There is no SCX/sprite penalty lookup
table. A four-dot post-transition bus interval remains and is being validated
alongside the five failing Mooneye PPU tests.

### Register Read Points in Fetcher

| Register | Read At | Stage |
|----------|---------|-------|
| LCDC bit 3 (BG map) | ReadTileID | Tile map base address |
| LCDC bit 4 (Tile select) | ReadTileDataLow/High | Tile data base address |
| LCDC bit 6 (Window map) | ReadTileID (window) | Window tile map base |
| SCX high 5 bits | ReadTileID | Tile X column |
| SCY | ReadTileID + ReadTileDataLow/High | Tile Y row |
| BGP/OBP0/OBP1 | `renderPixelIfPossible()` | Applied at pixel output |
| LCDC bit 0 (BG enable) | `renderPixelIfPossible()` | Blanks BG color |
| LCDC bit 1 (OBJ enable) | object fetch and pixel output | Controls stalls and sprite mixing |
| WX | `handleWindow()` | Window trigger and reactivation checks |
