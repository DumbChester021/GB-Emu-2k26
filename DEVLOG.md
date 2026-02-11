# GB-Emu-2k26 — Development Log & Architecture Reference

> **Last Updated:** 2026-02-11  
> **Purpose:** Capture all development context, architecture decisions, hard-won bug fixes, and remaining work so future sessions can continue efficiently without re-discovering past findings.

---

## Table of Contents
1. [Project Overview](#project-overview)
2. [Architecture](#architecture)
3. [Test Results Summary](#test-results-summary)
4. [Critical Fixes — Hard Problems Solved](#critical-fixes--hard-problems-solved)
5. [Known Issues & Remaining Work](#known-issues--remaining-work)
6. [Key Technical Details](#key-technical-details)
7. [Debugging Playbook](#debugging-playbook)
8. [Build & Test Instructions](#build--test-instructions)

---

## Project Overview

A **cycle-accurate Game Boy (DMG) emulator** written in C++17 with SDL2 for display. The emulator targets M-cycle (4 T-cycle) accuracy for the CPU and per-dot (1 T-cycle) accuracy for the PPU.

### Goals
- Pass all relevant Mooneye test suite ROMs for DMG hardware
- Accurate CPU instruction timing (M-cycle level)
- Accurate PPU timing (per-dot / T-cycle level with FIFO pixel pipeline)
- Play commercial Game Boy games correctly

### Source Files (4,680 LOC total)
| File | LOC | Purpose |
|------|-----|---------|
| `ppu.cpp` | 659 | PPU state machine, pixel FIFO, tile fetcher, STAT IRQ |
| `cartridge.cpp` | 536 | ROM loading, header parsing |
| `cpu_opcodes.cpp` | 498 | Main opcode handlers (256 opcodes) |
| `mbc.h` | 451 | MBC1/MBC2/MBC3/MBC5 memory bank controllers |
| `cpu.cpp` | 407 | CPU core: tick loop, interrupts, bus access |
| `cpu_tables.cpp` | 388 | Opcode metadata tables |
| `memory_bus.cpp` | 385 | Bus routing, IO registers, DMA, PPU/timer integration |
| `cpu.h` | 327 | CPU class declaration, registers |
| `main.cpp` | 271 | SDL2 window, render loop, input |
| `ppu.h` | 205 | PPU class declaration, FIFO, sprites, state |
| `timer.cpp` | 138 | DIV/TIMA timer with falling-edge detection |
| `cartridge.h` | 126 | Cartridge class declaration |
| `cpu_cb_opcodes.cpp` | 87 | CB-prefixed opcode handlers |
| `memory_bus.h` | 79 | MemoryBus class declaration |
| `timer.h` | 74 | Timer class declaration |
| `file_dialog.cpp/h` | 49 | Native file dialog for ROM selection |

---

## Architecture

### CPU (`cpu.cpp`, `cpu.h`)
- **M-cycle accurate**: Each instruction consumes the correct number of M-cycles
- `readByte(addr)` → `bus_.read(addr)` THEN `tick4()` (read-before-tick ordering)
- `writeByte(addr, val)` → `bus_.write(addr, val)` THEN `tick4()`
- **IMPORTANT**: The read/write-before-tick ordering was deliberately chosen. The CPU latches bus data at the beginning of the M-cycle, before PPU state changes within that same M-cycle take effect. This is critical for passing polling-based PPU timing tests like `intr_2_mode3_timing`.
- `tick4()` calls `bus_.tick()` 4 times, advancing the PPU by 4 dots
- `handleInterrupts()` runs at the start of each instruction, before fetch
- EI has a 1-instruction delay (applied after interrupt check, before fetch)
- HALT wakes on ANY pending interrupt (IF & IE), even with IME=0

### PPU (`ppu.cpp`, `ppu.h`)
- **Per-dot (T-cycle) state machine** with 4 modes:
  - **Mode 2 (OAM Search)**: 80 dots, evaluates sprites for current line
  - **Mode 3 (Pixel Transfer)**: ~172 dots (minimum, no sprites/scroll), FIFO-based
  - **Mode 0 (HBlank)**: fills remainder to 456 dots per line
  - **Mode 1 (VBlank)**: 10 lines (LY 144-153), 4560 dots total
- **FIFO Pixel Pipeline**:
  - Tile fetcher with 4 states: `ReadTileID` → `ReadTileDataLow` → `ReadTileDataHigh` → `PushToFIFO`
  - Each state takes 2 T-cycles (fetcher clock counts 0→1, resets)
  - 5-dot initial penalty (`mode3PenaltyDots_`) at mode 3 start to match DMG 172-dot mode 3 duration
  - Background and window tiles with SCX fine-scroll discard
  - Sprite mixing via `mixSpritePixel()`
- **STAT Interrupt Logic** (`updateStatIRQ()`):
  - Rising-edge detection: IRQ only fires on `false→true` transition of combined STAT line
  - Sources OR'd: mode 0 enable (bit 3), mode 1 enable (bit 4), mode 2 enable (bit 5), LYC=LY (bit 6)
  - VBlank OAM pulse: `vblankOamPulse_` flag creates a one-shot mode 2 source at VBlank entry
- **LCD Enable/Disable**: LCDC bit 7 controls LCD. Disable resets to mode 0, LY=0, dot=0

### Memory Bus (`memory_bus.cpp`, `memory_bus.h`)
- Routes reads/writes to: ROM/MBC, VRAM, WRAM, OAM, IO registers, HRAM
- PPU registers (FF40-FF4B) delegated to `PPU::readReg()`/`writeReg()`
- Timer registers (FF04-FF07) delegated to `Timer`
- `tick()` advances PPU by 1 dot and timer by 1 T-cycle
- OAM DMA: basic implementation (copies 160 bytes, no bus conflict emulation)

### Timer (`timer.cpp`, `timer.h`)
- Falling-edge detection on internal counter bit for TIMA increment
- DIV write resets internal 16-bit counter
- TIMA overflow: 1 M-cycle delay before TMA reload and IF flag

---

## Test Results Summary

### Mooneye Test Suite (mts-20240926) — As of 2026-02-11

#### PPU Tests: **8/12 PASSING** ✅
| Test | Status | Notes |
|------|--------|-------|
| `hblank_ly_scx_timing-GS` | ❌ FAIL | Regressed from read-before-tick change; see Known Issues |
| `intr_1_2_timing-GS` | ✅ PASS | Fixed by VBlank OAM pulse |
| `intr_2_0_timing` | ✅ PASS | Fixed by mode 3 penalty (172 dots) |
| `intr_2_mode0_timing` | ✅ PASS | Fixed by read-before-tick CPU ordering |
| `intr_2_mode0_timing_sprites` | ❌ FAIL | Needs sprite-extended mode 3 timing |
| `intr_2_mode3_timing` | ✅ PASS | Fixed by read-before-tick CPU ordering |
| `intr_2_oam_ok_timing` | ✅ PASS | Fixed by read-before-tick CPU ordering |
| `lcdon_timing-GS` | ❌ FAIL | LCD enable timing not yet accurate |
| `lcdon_write_timing-GS` | ❌ FAIL | LCD enable write timing |
| `stat_irq_blocking` | ✅ PASS | |
| `stat_lyc_onoff` | ❌ FAIL | LYC comparison with LCD toggle |
| `vblank_stat_intr-GS` | ✅ PASS | |

#### Timer Tests: **12/13 PASSING** ✅
| Test | Status |
|------|--------|
| `div_write` through `tma_write_reloading` | All ✅ PASS |
| `tima_write_reloading` | ❌ FAIL |

#### MBC1 Tests: **13/13 PASSING** ✅
All MBC1 emulator-only tests pass.

#### Acceptance Root: **29/41 PASSING**
- 12 failures are all `boot_div`, `boot_hwio`, `boot_regs` (SGB/DMG0/MGB variants) + `oam_dma_start`

#### CPU Tests: 0/1 (timeout)
- `cpu_instrs` times out — depends on OAM DMA timing

---

## Critical Fixes — Hard Problems Solved

### Fix 1: VBlank OAM Pulse (`intr_1_2_timing-GS`)

**Problem:** The mode 2 STAT interrupt didn't fire at the correct time relative to VBlank. The test measured the gap between VBlank interrupt and the next mode 2 (OAM) interrupt. The mode 2 interrupt was arriving ~464 T-cycles (one full scanline) late, at LY=1 instead of LY=0.

**Root Cause:** The original `updateStatIRQ()` checked `(stat_ & 0x20) && (mode_ == MODE_OAM || mode_ == MODE_VBLANK)` for the mode 2 source. This held the STAT IRQ line HIGH throughout the entire VBlank period (mode 1). When the PPU transitioned from VBlank to OAM at LY=0, the line was ALREADY high → **no rising edge** → no interrupt. The interrupt only fired at LY=1 (after HBlank at LY=0 briefly dropped the line).

**Fix:** Added `vblankOamPulse_` flag:
```cpp
// In tickHBlank(), at VBlank entry:
vblankOamPulse_ = true;    // Set before updateStatIRQ
checkLYC();
updateStatIRQ();
vblankOamPulse_ = false;   // Clear immediately — one-shot

// In updateStatIRQ():
if ((stat_ & 0x20) && (mode_ == MODE_OAM || vblankOamPulse_)) line = true;
//                                          ^^^^^^^^^^^^^^^^ was: mode_ == MODE_VBLANK
```
This creates a transient pulse at VBlank entry, allowing a proper rising edge for the mode 2 interrupt without holding the line high throughout VBlank.

**Files:** `ppu.cpp` (lines ~185-215, ~495), `ppu.h` (line ~88)

---

### Fix 2: Mode 3 Duration — 167 → 172 dots (`intr_2_0_timing`, `hblank_ly_scx_timing-GS`)

**Problem:** Mode 3 (pixel transfer) was completing in 167 dots instead of the correct 172 dots. This made HBlank start 5 dots too early, shifting all mode 0 STAT interrupt timings.

**Root Cause:** The FIFO tile fetcher started immediately when mode 3 began. On real DMG hardware, there is a 12-dot initial delay before the first pixel is pushed. Our fetcher naturally produced 7 dots of delay (one tile fetch cycle = 8 dots, minus 1 because push and fetch overlap). The remaining 5 dots were missing.

**Fix:** Added `mode3PenaltyDots_` counter initialized to 5 at mode 3 entry:
```cpp
// In tickOAMSearch(), at mode 3 transition:
mode3PenaltyDots_ = 5;  // 5-dot initial penalty

// In tickPixelTransfer():
if (mode3PenaltyDots_ > 0) {
    mode3PenaltyDots_--;
    return;  // Skip fetcher/push during penalty
}
```
Result: mode 3 duration = 5 (penalty) + 167 (fetcher pipeline) = **172 dots** ✓

**Files:** `ppu.cpp` (lines ~82, ~103-108), `ppu.h` (line ~90)

---

### Fix 3: CPU Bus Read/Write Ordering (`intr_2_mode3_timing`, `intr_2_mode0_timing`, `intr_2_oam_ok_timing`)

**Problem:** Polling tests that read STAT in a tight loop (`ld a,(hl); and $03; cp $03; jr nz,-`) got the wrong iteration count. The mode transition was visible 1 M-cycle too early.

**Root Cause:** `readByte()` was doing `tick4()` FIRST, then `bus_.read()`. This meant the PPU advanced 4 dots before the CPU read the bus, making mode transitions that occurred during those 4 dots immediately visible. On real DMG, the CPU latches the bus value at the beginning of the M-cycle, before PPU state changes take effect.

**Fix:** Swapped the order:
```cpp
// BEFORE (wrong — PPU advances, then CPU reads new state):
uint8_t CPU::readByte(uint16_t addr) {
    tick4();
    return bus_.read(addr);
}

// AFTER (correct — CPU reads current state, then PPU advances):
uint8_t CPU::readByte(uint16_t addr) {
    uint8_t val = bus_.read(addr);
    tick4();
    return val;
}
// Same change applied to writeByte()
```

**Impact:** This fixed 3 PPU tests but **regressed** `hblank_ly_scx_timing-GS`. The regression suggests that test depends on a slightly different CPU-PPU phase alignment. This is a known trade-off that needs further investigation (see Known Issues).

**Files:** `cpu.cpp` (lines ~153-161)

---

## Known Issues & Remaining Work

### PPU Failures (4 remaining)

#### 1. `hblank_ly_scx_timing-GS` — REGRESSED
- **What:** Tests that mode 0 (HBlank) duration varies correctly with SCX value
- **Why:** Regressed when readByte/writeByte ordering was changed (Fix #3). The test previously passed with tick-before-read. The read-before-tick ordering shifted CPU-PPU phase alignment by 1 M-cycle, which this test is sensitive to.
- **Investigation needed:** May need a more nuanced solution — perhaps read-before-tick for IO register reads only, or adjusting the dotCounter_ phase by 1. The root issue is that tick-before-read passes this test but fails `intr_2_*` tests, and read-before-tick passes `intr_2_*` but fails this one.
- **Possible fix direction:** The real DMG CPU reads the bus at T3 of the M-cycle (not T0 or T4). Sub-M-cycle accuracy (ticking 1 dot at a time inside readByte) might resolve both. Alternatively, the dotCounter_ increment in `tick()` could be adjusted from pre-increment to post-increment.

#### 2. `intr_2_mode0_timing_sprites` — Sprite penalty timing
- **What:** Mode 3 duration should increase with sprite count and position
- **Why:** Our sprite rendering mixes pixels but doesn't add mode 3 dot penalties. On real DMG, each sprite adds 6-11 extra dots to mode 3 depending on its X position.
- **Fix:** Add per-sprite penalty dots in `tickPixelTransfer()` when sprite fetching occurs.

#### 3. `lcdon_timing-GS` / `lcdon_write_timing-GS` — LCD enable timing
- **What:** Tests the exact frame timing after setting LCDC bit 7 (LCD enable)
- **Why:** Our LCD enable handling (`firstLineAfterEnable_`) starts line 0 in mode 0 for 4 dots then jumps to mode 3, skipping OAM. The exact timing of the first frame after LCD enable may be off.
- **Fix:** Compare with DMG hardware docs for exact LCD enable behavior. May need adjustment to the initial dot counter and mode sequence.

#### 4. `stat_lyc_onoff` — LYC with LCD toggle
- **What:** Tests LYC coincidence behavior when LCD is turned off and on
- **Why:** When LCD is disabled, LY resets to 0. The test checks that LYC coincidence flag and interrupt behave correctly around LCD toggle.
- **Fix:** Ensure that `checkLYC()` and `updateStatIRQ()` are called correctly when LCDC bit 7 changes.

### Other Failures

- **`tima_write_reloading`**: Timer edge case — writing to TIMA during the reload cycle
- **`oam_dma_start`**: OAM DMA doesn't model bus conflicts
- **Boot register tests**: We emulate DMG-ABC, not SGB/DMG0/MGB variants
- **CPU test**: Timeout — depends on OAM DMA accuracy

### Not Yet Implemented
- **OAM DMA bus conflicts**: During DMA, CPU should only access HRAM
- **VRAM/OAM access locking**: PPU should block CPU VRAM access during mode 3, OAM during modes 2-3
- **Sprite mode 3 penalties**: 6-11 extra dots per sprite based on X position
- **Sub-M-cycle bus accuracy**: CPU reads at T3 of M-cycle, not T0 or T4

---

## Key Technical Details

### PPU Dot Counter
- `dotCounter_` is pre-incremented in `tick()` before calling mode handlers
- First tick of OAM has `dotCounter_=1`, transition happens at `dotCounter_>=80`
- HBlank fills until `dotCounter_>=456` (DOTS_PER_LINE)
- dotCounter is reset to 0 at scanline boundaries

### STAT IRQ Rising Edge
The STAT interrupt uses **composite rising-edge detection**:
```
combined_line = (mode0_en && mode==0) ||
                (mode1_en && mode==1) ||
                (mode2_en && (mode==2 || vblankOamPulse)) ||
                (lyc_en && ly==lyc)
                
if (combined_line && !prev_line) → fire STAT interrupt (IF bit 1)
prev_line = combined_line
```
This means writing to STAT register can cause or suppress interrupts, and multiple sources share one line (so enabling mode 0 while in mode 0 fires immediately only if line was previously low).

### Mode 3 Duration Formula
```
mode3_dots = 172 + (SCX % 8) + sprite_penalties
```
Where `sprite_penalties` = sum of per-sprite costs (6-11 dots each, based on X position modulo 8). Currently we only implement the base 172 dots.

### Interrupt Dispatch Timing
```
handleInterrupts() reads IF                → 0T
  internalCycle() (2x)                    → 8T  
  pushWord(PC)                            → 8T (2 writes × 4T)
  internalCycle() (set PC to vector)      → 4T
Total dispatch cost:                        20T (5 M-cycles)
```

### CPU Read/Write Bus Ordering
```
readByte(addr):  val = bus_.read(addr) → tick4() → return val
writeByte(addr): bus_.write(addr, val) → tick4()
```
The bus access happens BEFORE the 4 PPU dots advance. This is the "read-before-tick" model. The CPU sees PPU state from the start of the M-cycle. This is critical for passing STAT polling tests.

---

## Debugging Playbook

### Adding PC Traces
When debugging timing, add conditional traces in `CPU::tick()`:
```cpp
static int traceCount = 0;
static bool traceActive = false;
bool wasHalted = halted_;
handleInterrupts();
// Activate on HALT wakeup when only STAT is enabled
if (wasHalted && !halted_ && bus_.read(0xFFFF) == 0x02) {
    traceActive = true;
    traceCount = 0;
}
if (traceActive && traceCount < 30) {
    fprintf(stderr, "[TRACE] PC=0x%04X op=0x%02X B=0x%02X LY=%d STAT=0x%02X IF=0x%02X IME=%d\n",
        reg.pc, bus_.read(reg.pc), reg.b, bus_.ppu().currentLY(),
        bus_.read(0xFF41), bus_.read(0xFF0F), ime_);
    traceCount++;
}
```

### Measuring Mode 3 Duration
Add to `tickOAMSearch()` and `tickPixelTransfer()`:
```cpp
// At OAM→XFER transition: mode3StartDot_ = dotCounter_;
// At XFER→HBlank transition: int duration = dotCounter_ - mode3StartDot_;
// Expected: 172 (no sprites/scroll), 172 + (SCX%8) with scroll
```

### Test ROM Source
Mooneye test source is in `test_roms/mooneye-test-suite-main/acceptance/ppu/`. The `.s` assembly files show exactly what timing the test expects. Key pattern:
```asm
setup_and_wait_mode2:     ; Synchronize to specific LY and mode
  wait_ly $42             ; Wait for LY=66
  wait_mode $00           ; Wait for HBlank  
  wait_mode $03           ; Wait for next pixel transfer
  ; Now at a known PPU phase
  ld a, %00100000         ; Enable mode 2 STAT
  ldh (<STAT), a
  clear_interrupts
  ei
  halt                    ; Wait for mode 2 interrupt
```

---

## Build & Test Instructions

### Build
```bash
cd /mnt/data/Github/c++/GB-Emu-2k26
mkdir -p build && cd build
cmake .. && make -j$(nproc)
```

### Run Individual Test
```bash
./build/gbemu --test test_roms/mts-20240926-1737-443f6e1/acceptance/ppu/intr_2_0_timing.gb
```

### Run All PPU Tests
```bash
for rom in test_roms/mts-20240926-1737-443f6e1/acceptance/ppu/*.gb; do
    timeout 10 ./build/gbemu --test "$rom" 2>/dev/null
done
```

### Run Full Acceptance Suite
```bash
for rom in test_roms/mts-20240926-1737-443f6e1/acceptance/*.gb; do
    timeout 10 ./build/gbemu --test "$rom" 2>/dev/null
done
```

### Run Timer / MBC Tests
```bash
for rom in test_roms/mts-20240926-1737-443f6e1/acceptance/timer/*.gb; do
    timeout 10 ./build/gbemu --test "$rom" 2>/dev/null
done
for rom in test_roms/mts-20240926-1737-443f6e1/emulator-only/mbc1/*.gb; do
    timeout 10 ./build/gbemu --test "$rom" 2>/dev/null
done
```

### Run ROM Visually
```bash
./build/gbemu path/to/rom.gb
```
