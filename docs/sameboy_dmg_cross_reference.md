# SameBoy DMG Cross-Reference Analysis

## TL;DR

**SameBoy targets `DMG-CPU-B` exclusively** — it does not emulate DMG-0,
DMG-A, or DMG-C differently. This project should therefore describe its concrete
target as DMG-CPU B. The current per-dot rewrite passes **89/94 selected
Mooneye tests** and **4/24 Mealybug** exact images. Both bundled Blargg OAM
suites still pass 8/8. SameBoy is the oracle for closing the remaining PPU
edge-ordering gaps; parity is not currently claimed.

---

## SameBoy Model Definitions

From [model.h](https://github.com/LIJI32/SameBoy/blob/master/Core/model.h):

```c
typedef enum {
    // GB_MODEL_DMG_0 = 0x000,  // commented out
    // GB_MODEL_DMG_A = 0x001,  // commented out
    GB_MODEL_DMG_B = 0x002,     // ✅ ONLY active DMG model
    // GB_MODEL_DMG_C = 0x003,  // commented out
    GB_MODEL_SGB   = 0x004,
    GB_MODEL_MGB   = 0x100,
    GB_MODEL_SGB2  = 0x101,
    GB_MODEL_CGB_0 = 0x200,     // CGB revisions (for future ref)
    GB_MODEL_CGB_A = 0x201,
    GB_MODEL_CGB_B = 0x202,
    GB_MODEL_CGB_C = 0x203,
    GB_MODEL_CGB_D = 0x204,
    GB_MODEL_CGB_E = 0x205,
    GB_MODEL_AGB_A = 0x207,
} GB_model_t;
```

> [!IMPORTANT]
> SameBoy has **not** implemented DMG-0 or DMG-A specific behaviors. DMG-0 has notable differences (no ® on boot logo, different wave glitch behavior), but SameBoy only targets DMG-B. DMG-B and DMG-C are believed to be behaviorally identical for all known tests.

## DMG Revision Differences

| Feature | DMG-0 | DMG-A | DMG-B | DMG-C |
|---------|-------|-------|-------|-------|
| Boot logo ® symbol | ❌ Missing | ✅ Has | ✅ Has | ✅ Has |
| Wave RAM corruption on retrigger | Different pattern | Has "wave glitch" | ✅ No wave glitch | Same as B |
| OAM bug behavior | Same | Same | Same | Same |
| Timer behavior | Same | Same | Same | Same |
| PPU timing | Same | Same | Same | Same |

> [!NOTE]
> The key differences between DMG variants are only in the **boot ROM** and **APU wave channel behavior**. CPU timing, PPU timing, timer behavior, and memory bus behavior are the same across all DMG revisions.

## Selected Mooneye Cross-Reference — Rewrite Snapshot

### `hblank_ly_scx_timing-GS` (PPU) — ❌ CURRENTLY FAILING

The older renderer passed by rounding SCX penalties to M-cycle boundaries. That
formula was removed when the real FIFO pipeline landed; the natural fine-scroll
duration is now being aligned to the measured HBlank edge.

### ✓ `boot_sclk_align-dmgABCmgb` (Serial) — ✅ PASSING

### ✓ `boot_hwio-dmgABCmgb` (Boot HWIO) — ✅ PASSING

## Summary Table

| Test | Category | SameBoy | Our Status | Action |
|------|----------|---------|------------|--------|
| `hblank_ly_scx_timing-GS` | PPU | ✅ Pass | ❌ Fail | Reconcile natural FIFO duration |
| `boot_sclk_align-dmgABCmgb` | Serial | ✅ Pass | ✅ Pass | — |
| `boot_hwio-dmgABCmgb` | Boot | ✅ Pass | ✅ Pass | — |

## Mooneye Test Naming Reference

For future CGB work:

| Suffix | Models | Notes |
|--------|--------|-------|
| (none) | Universal | All models |
| `-GS` | DMG+MGB+SGB+SGB2 | G = DMG+MGB, S = SGB family |
| `-dmg0` | DMG-0 only | Earliest revision |
| `-dmgABC` | DMG-A/B/C | Post-0 DMG |
| `-dmgABCmgb` | DMG-A/B/C + MGB | Our target |
| `-C` | CGB+AGB+AGS | Game Boy Color family |
| `-S` | SGB+SGB2 | Super Game Boy family |

> [!TIP]
> **Current selected Mooneye score: 89/94.** Both bundled Blargg OAM corruption
> suites pass 8/8, while Mealybug is 4/24 exact. See `GAP_ANALYSIS.md` for the
> live failure list.
