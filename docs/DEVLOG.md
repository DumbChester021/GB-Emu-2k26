# GB-Emu-2k26 — Development Log & Architecture Reference

> **Last Updated:** 2026-02-22  
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

A **cycle-accurate Game Boy (DMG) emulator** written in C++17 with SDL2 for display and audio. The emulator targets M-cycle (4 T-cycle) accuracy for the CPU, per-dot (1 T-cycle) accuracy for the PPU, and T-cycle accuracy for the APU.

### Goals
- Pass all relevant Mooneye test suite ROMs for DMG hardware
- Accurate CPU instruction timing (M-cycle level)
- Accurate PPU timing (per-dot / T-cycle level with FIFO pixel pipeline)
- Hardware-accurate APU with all 4 audio channels and SDL2 audio output
- Play commercial Game Boy games correctly (with sound)

### Source Files (~5,800 LOC total)
| File | LOC | Purpose |
|------|-----|---------|
| `apu.cpp` | 697 | APU core: 4 channels, frame sequencer, register I/O, mixing |
| `ppu.cpp` | 659 | PPU state machine, pixel FIFO, tile fetcher, STAT IRQ |
| `cartridge.cpp` | 536 | ROM loading, header parsing |
| `cpu_opcodes.cpp` | 498 | Main opcode handlers (256 opcodes) |
| `mbc.h` | 451 | MBC1/MBC2/MBC3/MBC5 memory bank controllers |
| `cpu.cpp` | 407 | CPU core: tick loop, interrupts, bus access |
| `cpu_tables.cpp` | 388 | Opcode metadata tables |
| `memory_bus.cpp` | 395 | Bus routing, IO registers, DMA, subsystem integration |
| `cpu.h` | 327 | CPU class declaration, registers |
| `main.cpp` | 355 | SDL2 window, audio, render loop, input, frame pacing |
| `ppu.h` | 205 | PPU class declaration, FIFO, sprites, state |
| `apu_serialize.cpp` | 130 | APU save state serialization |
| `timer.cpp` | 138 | DIV/TIMA timer with falling-edge detection |
| `cartridge.h` | 126 | Cartridge class declaration |
| `apu.h` | 96 | APU class declaration, channel structs, ring buffer |
| `cpu_cb_opcodes.cpp` | 87 | CB-prefixed opcode handlers |
| `memory_bus.h` | 84 | MemoryBus class declaration |
| `timer.h` | 74 | Timer class declaration |
| `settings.cpp/h` | 120 | Persistent emulator settings (INI key=value) |
| `file_dialog.cpp/h` | 58 | Native file dialog with initial directory support |

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
- APU registers (FF10-FF26, FF30-FF3F) delegated to `APU::readReg()`/`writeReg()`
- Timer registers (FF04-FF07) delegated to `Timer`
- `tick()` advances PPU by 1 dot, timer by 1 T-cycle, and APU by 1 T-cycle
- OAM DMA: copies 160 bytes with SameBoy-style bus conflict emulation
  - Per-bus conflict detection: VRAM bus vs MAIN bus
  - Conflicting reads return last DMA-transferred byte (`dmaLastByte_`)
  - 2 M-cycle startup delay before bus conflict takes effect
  - OAM is blocked (returns 0xFF) only after startup delay
  - DMA from echo RAM region (≥0xE000) maps through `src & ~0x2000`
- **Boot ROM support**: Loads optional DMG boot ROM from `dmg_boot.bin`
  - PPU and Timer reset to power-on state via `resetForBootrom()`
  - Boot ROM overlay disabled when game writes to FF50

### Timer (`timer.cpp`, `timer.h`)
- Falling-edge detection on internal counter bit for TIMA increment
- DIV write resets internal 16-bit counter
- TIMA overflow: 1 M-cycle delay before TMA reload and IF flag

### APU (`apu.cpp`, `apu.h`, `apu_serialize.cpp`)
- **T-cycle accurate**: `tick()` called once per T-cycle from `MemoryBus::tick()`
- **4 channels:**
  - **CH1 (Pulse with Sweep)**: Square wave with frequency sweep, volume envelope, and length counter
  - **CH2 (Pulse)**: Square wave with volume envelope and length counter (no sweep)
  - **CH3 (Wave)**: Plays samples from 16-byte wave RAM (32 4-bit samples), with volume shift
  - **CH4 (Noise)**: LFSR-based noise generator with volume envelope, 15-bit or 7-bit mode
- **Frame Sequencer**: 512 Hz (8192 T-cycles), 8 steps clocking length counters (steps 0,2,4,6), sweep (steps 2,6), and volume envelopes (step 7)
- **Register I/O**: Hardware-accurate read masks (unused bits read as 1), write-only registers, DMG-specific edge cases:
  - Extra length clocking when length enable is newly set on even frame sequencer steps
  - Sweep negate-to-positive mode change disables channel
  - Wave RAM access conflicts when CH3 is active
  - Length counters writable even when APU is powered off (DMG behavior)
- **Audio Pipeline**: T-cycle accurate → downsample to 44100 Hz → NR50/NR51 stereo mix → high-pass filter (DC offset removal) → lock-free SPSC ring buffer → SDL audio callback
- **Master Power Control**: NR52 bit 7 enables/disables APU; powering off zeros all registers (wave RAM preserved on DMG)

### Settings (`settings.cpp`, `settings.h`)
- Lightweight INI-style key=value persistence at `~/.config/gbemu/settings.ini`
- Currently stores `last_rom_dir` — the directory of the last ROM opened via file picker
- Creates config directory automatically on first save
- Reusable for future emulator config (palette, volume, etc.)

### Main Loop (`main.cpp`)
- **File Picker**: Opens in last ROM directory (persisted in settings), falls back to executable directory on first launch
- **Frame Pacing**: Hardware-accurate ~59.7275 Hz (4194304 / 70224) using SDL performance counters
  - Coarse SDL_Delay for bulk wait, spin-wait for sub-millisecond precision
  - Sub-tick error accumulator prevents long-term drift
- **FPS Display**: Updates window title with current FPS every ~1 second

---

## Test Results Summary

### Mooneye Test Suite (mts-20240926) — As of 2026-02-21

**Grand Total: 94/94 DMG-ABC tests passing**

#### PPU Tests: **12/12 PASSING** ✅
| Test | Status | Notes |
|------|--------|-------|
| `hblank_ly_scx_timing-GS` | ✅ PASS | Fixed by SCX M-cycle alignment penalty (C++ modulo bug) |
| `intr_1_2_timing-GS` | ✅ PASS | Fixed by VBlank OAM pulse |
| `intr_2_0_timing` | ✅ PASS | Fixed by mode 3 penalty (172 dots) |
| `intr_2_mode0_timing` | ✅ PASS | Fixed by read-before-tick CPU ordering |
| `intr_2_mode0_timing_sprites` | ✅ PASS | Fixed by sprite mode 3 penalties |
| `intr_2_mode3_timing` | ✅ PASS | Fixed by read-before-tick CPU ordering |
| `intr_2_oam_ok_timing` | ✅ PASS | Fixed by read-before-tick CPU ordering |
| `lcdon_timing-GS` | ✅ PASS | Fixed by LCD enable timing (Fix #6) |
| `lcdon_write_timing-GS` | ✅ PASS | Fixed by LCD enable timing (Fix #6) |
| `stat_irq_blocking` | ✅ PASS | |
| `stat_lyc_onoff` | ✅ PASS | Fixed by STAT IRQ line LCD toggle fix |
| `vblank_stat_intr-GS` | ✅ PASS | |

#### Timer Tests: **13/13 PASSING** ✅
All 13 timer tests pass.

#### MBC1 Tests: **13/13 PASSING** ✅
All MBC1 emulator-only tests pass.

#### Interrupt Tests: **3/3 PASSING** ✅
All 3 interrupt tests pass (including `ie_push`).

#### Blargg Tests: **ALL PASSING** ✅
- `dmg_sound` 12/12 — all tests match SameBoy output
- `cpu_instrs` — all passing
- `instr_timing` — passing
- `mem_timing` / `mem_timing-2` — passing

#### Boot/Serial Tests: **4/4 DMG-ABC PASSING** ✅
- ✅ `boot_regs-dmgABC`, ✅ `boot_div-dmgABCmgb`
- ✅ `boot_hwio-dmgABCmgb`, ✅ `boot_sclk_align-dmgABCmgb`

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

**Impact:** This fixed 3 PPU tests. The initial implementation temporarily regressed `hblank_ly_scx_timing-GS`, which was later resolved by the SCX M-cycle alignment fix (Fix 10).

**Files:** `cpu.cpp` (lines ~153-161)

---

### Fix 4: OAM DMA Bus Conflicts (`sources-GS`, `reg_read`, `oam_dma_restart`)

**Problem:** During OAM DMA, the CPU continued reading/writing all memory normally. Tests expected bus conflicts where CPU accesses to the same bus as DMA return corrupted data.

**Root Cause:** DMG has two external buses — MAIN (ROM, WRAM, External RAM) and VRAM. During DMA, the DMA controller occupies one bus. CPU reads from the same bus get the last byte DMA transferred, not the actual memory value. Addresses ≥0xFE00 (OAM, IO, HRAM, IE) are on the internal bus and never blocked.

**Fix (SameBoy-style):**
```cpp
// In read(): per-bus conflict detection
if (dmaActive_ && dmaDelay_ == 0 && addr < 0xFE00) {
    bool dmaOnVRAM = (dmaSrc_ >= 0x8000 && dmaSrc_ < 0xA000);
    bool cpuOnVRAM = (addr >= 0x8000 && addr < 0xA000);
    if (dmaOnVRAM == cpuOnVRAM) return dmaLastByte_;
}
// OAM blocked only after startup delay:
if (addr < 0xFEA0 && dmaActive_ && dmaDelay_ == 0) return 0xFF;
```

**Key insight:** Initial attempts using `addr < 0xFF80` (block everything below HRAM) and returning 0xFF caused regressions. The correct approach is:
1. Per-bus conflict — only block when DMA and CPU are on the **same** bus
2. Return `dmaLastByte_` (last DMA-transferred byte), not 0xFF
3. OAM blocking must respect the 2 M-cycle startup delay
4. DMA from echo RAM (≥0xE000) maps through `srcAddr & ~0x2000` to WRAM

**Impact:** Fixed `sources-GS` (75→76/85). Zero regressions.

**Files:** `memory_bus.cpp` (read/write guards + tick DMA), `memory_bus.h` (`dmaLastByte_`)

---

### Fix 5: OAM DMA Start Timing (`oam_dma_start`)

**Problem:** The `oam_dma_start` test checks exact cycle-by-cycle behavior during DMA startup. When DMA is restarted while a previous one is running, OAM should stay blocked through the new DMA's startup delay. The old code only checked `dmaDelay_ == 0`, which let OAM become accessible during the restart's startup delay.

**Root Cause:** When DMA restarts, `dmaDelay_` is reset to 8. During the new delay period, `dmaDelay_ != 0` means OAM blocking was disabled — but the previous DMA was already blocking OAM, so it should stay blocked.

**Fix:** Added `dmaRestarting_` flag:
```cpp
// In DMA trigger (FF46 write):
dmaRestarting_ = dmaActive_; // Previous DMA was running
dmaActive_ = true;
dmaByte_ = 0;
dmaClock_ = 0;
dmaDelay_ = 8;

// In OAM read:
if (dmaActive_ && (dmaDelay_ == 0 || dmaRestarting_)) return 0xFF;

// In DMA tick, after first byte transfer:
dmaRestarting_ = false;
```

**Impact:** Fixed `oam_dma_start` (76→77/85 acceptance). Zero regressions.

**Files:** `memory_bus.cpp`, `memory_bus.h` (`dmaRestarting_`), `memory_bus_serialize.cpp`

---

### Fix 6: LCD Enable Timing (`lcdon_timing-GS`, `lcdon_write_timing-GS`)

**Problem:** After LCD is enabled (LCDC bit 7 set), the first scanline has unique timing behavior. Tests check LY, STAT mode, OAM/VRAM access patterns at specific dot offsets after enable. All 6 sub-tests (LY, STAT, OAM read, VRAM read, OAM write, VRAM write) were failing.

**Root Cause:** Multiple timing behaviors on the first scanline and on normal scanlines were inaccurate:
1. First line after enable was too long and had wrong mode sequence
2. Normal lines lacked a 4-dot pre-OAM transition (STAT shows mode 0 before switching to mode 2)
3. Mode 3 started 4 dots too early on normal lines
4. LYC coincidence updated too early at line boundaries
5. OAM/VRAM access pre-blocking was missing (hardware blocks access before STAT mode bits change)
6. OAM write blocking had different timing from read blocking

**Fix (6 coordinated sub-fixes):**

| Timing Behavior | Before | After |
|---|---|---|
| First line after LCD enable | 4-dot mode 0 → mode 3, 456 dots | 78-dot mode 0 → mode 3, 448 dots, no sprite eval |
| Normal line pre-OAM | Mode 2 at dot 0 | Mode 0 for dots 1-3, mode 2 at dot 4 |
| Mode 3 start (normal lines) | Dot 80 | Dot 84 (accounts for 4-dot pre-OAM) |
| LYC coincidence at line boundary | Immediate | Cleared at boundary, set at dot 4 |
| OAM read pre-blocking | None | Returns $FF at dots 3-4 before mode 2 |
| VRAM read pre-blocking | None | Returns $FF at dot 83+ (1 dot before mode 3) |
| OAM write blocking | Full mode 2 + mode 3 | Mode 2 up to dot 82 only + mode 3 |

**Key insight:** On real DMG hardware, memory access blocking is separate from STAT mode bits. OAM reads are blocked 1 dot before STAT shows mode 2. VRAM reads are blocked 1 dot before mode 3. But OAM *writes* are unblocked 1 dot before mode 3 (while STAT still shows mode 2). These asymmetric read/write blocking windows are modeled by SameBoy's separate `oam_read_blocked`, `oam_write_blocked`, `vram_read_blocked` flags.

**Impact:** Fixed both `lcdon_timing-GS` and `lcdon_write_timing-GS`. Also corrected CPU-PPU phase alignment for `hblank_ly_scx_timing-GS`. PPU score: 8/12 → **11/12**.

**Files:** `ppu.cpp` (`tickHBlank`, `tickOAMSearch`, `readVRAM`, `readOAM`, `writeOAM`, `writeVRAM`), `ppu.h` (`FIRST_LINE_DOTS`, `firstLineShorter_`)

---

### Fix 7: STAT IRQ Line LCD Toggle (`stat_lyc_onoff`)

**Problem:** When the LCD was toggled off and back on, a spurious STAT interrupt could fire even when the LYC coincidence flag didn't actually change state. The test checks all four combinations: coincidence true→false, true→true, false→false, false→true during LCD toggle. Only the last case (false→true) should fire an interrupt.

**Root Cause:** In `tick()`, when the LCD was off, `statIrqLine_` was unconditionally set to `false` on every tick. When the LCD was re-enabled and the retained coincidence flag caused `updateStatIRQ()` to compute `line = true`, it always detected a rising edge (false→true) — even when the coincidence flag had been set the entire time (true→true case).

**Fix:** Two changes:
1. Removed `statIrqLine_ = false` from the LCD-off tick path — the line state is now frozen
2. In `writeReg()` when LCD turns off, set `statIrqLine_` directly based on the retained coincidence flag: `(stat_ & 0x40) && (stat_ & 0x04)`. Mode sources are inactive when the PPU is stopped, so only LYC coincidence contributes.

```cpp
// In writeReg (FF40), LCD turning off:
statIrqLine_ = ((stat_ & 0x40) && (stat_ & 0x04));

// In tick(), LCD off path:
// statIrqLine_ = false;  ← REMOVED
```

**Impact:** Fixed `stat_lyc_onoff`. PPU score: 11/12 → **12/12 (perfect)**. Zero regressions.

**Files:** `ppu.cpp` (`tick()` LCD-off path, `writeReg()` LCDC handler)

---

### Fix 8: TIMA Write During TMA Reload (`tima_write_reloading`)

**Problem:** Writing to TIMA (FF05) during the exact T-cycle when TMA is reloaded into TIMA was always cancelling the reload. The test checks 4 different write timings around the reload cycle:
- 1 M-cycle before reload: write cancels reload, but timer increment still fires (0x7F→0x80)
- During overflow delay: write cancels reload (value 0x7F kept)
- On reload cycle: write should be **ignored**, TMA (0xFE) wins
- After reload: normal write (value 0x7F kept)

**Root Cause:** `Timer::write()` for `case 0xFF05` unconditionally cancelled `overflowPending_` whenever a write occurred during the overflow delay. It didn't distinguish between "cycle A" (overflow pending, before reload — write should cancel) and "cycle B" (reload cycle — write should be ignored).

**Fix:** Added `reloadedThisCycle_` guard at the top of the TIMA write handler:
```cpp
case 0xFF05: {
    // Cycle B: TMA was just loaded → CPU write is ignored
    if (reloadedThisCycle_) break;
    // Cycle A: Cancel the pending reload
    if (overflowPending_) overflowPending_ = false;
    tima_ = val;
    break;
}
```

The `reloadedThisCycle_` flag was already set in `tick()` on the exact cycle of reload. No new state variables needed.

**Impact:** Fixed `tima_write_reloading`. Timer score: 12/13 → **13/13 (perfect)**. Zero regressions.

**Files:** `timer.cpp` (`Timer::write()`, case 0xFF05)

---

### Fix 9: IE Push During Interrupt Dispatch (`ie_push`)

**Problem:** During interrupt dispatch, the CPU pushes PC onto the stack in two steps (high byte, then low byte). If SP happens to point at 0x0000, the high-byte push writes to 0xFFFF — the IE register. The test expects the interrupt vector to be determined by the **new** IE value after the push, not the original value read at the start of dispatch.

**Root Cause:** `handleInterrupts()` read IE once at the start and determined the interrupt vector immediately. The two-byte push was done via `pushWord()` as a single operation. Any change to IE during the push was invisible to the vector selection logic.

**Fix:** Split the interrupt dispatch to perform the two push writes individually, with an IE re-read between them:
```cpp
// Push high byte of PC
reg.sp--;
writeByte(reg.sp, (reg.pc >> 8) & 0xFF);

// Re-read IE — the push may have written to 0xFFFF
ieReg = bus_.read(0xFFFF);
pending = ifReg & ieReg & 0x1F;

// Push low byte of PC
reg.sp--;
writeByte(reg.sp, reg.pc & 0xFF);

if (pending == 0) {
    // Dispatch cancelled — PC goes to 0x0000, IF untouched
    reg.pc = 0x0000;
} else {
    // Service highest-priority from refreshed pending set
    // Clear IF bit and jump to vector
}
```

The test has 4 rounds:
1. SP=0x0000, high-byte push clears IE → dispatch cancelled, PC→0x0000, IF untouched
2. After cancellation, IME is 0 → no spurious interrupt fires
3. SP=0x0001, low-byte push writes IE → too late to cancel, interrupt dispatches normally
4. SP=0x0000, two interrupts pending, high-byte push clears one → other interrupt dispatches

**Impact:** Fixed `ie_push`. Interrupts: 2/3 → **3/3 (perfect)**. Zero regressions.

**Files:** `cpu.cpp` (`handleInterrupts()`)

---

## Known Issues & Remaining Work

### PPU — ✅ All Passing

- ✅ All 12 PPU tests pass, including `hblank_ly_scx_timing-GS` (fixed by SCX M-cycle alignment penalty — C++ negative modulo bug in `(4 - scxFine) % 4` was producing wrong values for SCX%8 ≥ 5).

### APU — ✅ Implemented

Full DMG APU with all 4 channels, frame sequencer, register I/O, stereo mixing, and SDL2 audio output. **Hardware-accurate** with DMG edge cases (sweep negate quirk, extra length clocking, wave RAM conflicts, DAC enable logic). **Blargg `dmg_sound` 12/12 passing** — all tests match SameBoy output.

### Boot/Serial — ✅ All Passing

All boot and serial tests now pass: `boot_regs-dmgABC`, `boot_div-dmgABCmgb`, `boot_hwio-dmgABCmgb`, `boot_sclk_align-dmgABCmgb`.

### Completed Fixes

- ~~**`tima_write_reloading`**~~: ✅ Fixed (Fix 8)
- ~~**`ie_push`**~~: ✅ Fixed (Fix 9)
- ~~**`boot_div-dmgABCmgb`**~~: ✅ Now passing
- ~~**`boot_hwio-dmgABCmgb`**~~: ✅ Now passing
- ~~**`boot_sclk_align-dmgABCmgb`**~~: ✅ Now passing

### Not Yet Implemented
- **Sub-M-cycle bus accuracy**: CPU reads at T3 of M-cycle, not T0 or T4
- **CGB (Game Boy Color)**: See SameBoy Variant Research below for CGB model roadmap

---

## Key Technical Details

### PPU Dot Counter
- `dotCounter_` is pre-incremented in `tick()` before calling mode handlers
- Normal lines: 4-dot pre-OAM delay (dots 1-3 stay in HBLANK, mode 2 at dot 4)
- OAM search: mode 3 transition at `dotCounter_ >= OAM_DOTS + 4` (dot 84)
- First line after LCD enable: 448 dots, 78-dot mode 0 → mode 3, no OAM
- HBlank fills until `dotCounter_ >= DOTS_PER_LINE` (456, or 448 for first line)
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
Where `sprite_penalties` per unique X group = `max(0, 5 - (X + SCX) % 8)` alignment (shared) + `6 × count` per sprite. Sprites at X ≥ 168 are free. ✅ IMPLEMENTED.

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

### Run All Tests (Comprehensive)
```bash
bash run_mooneye_all.sh
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

---

## SameBoy DMG Variant Research (2026-02-19)

> Cross-reference analysis comparing our emulator against SameBoy to confirm whether failures are real bugs or variant-specific behavior.

### SameBoy Model Architecture

SameBoy defines its models in [`Core/model.h`](https://github.com/LIJI32/SameBoy/blob/master/Core/model.h). For DMG, **only `GB_MODEL_DMG_B` (0x002) is implemented**. DMG-0, DMG-A, and DMG-C are commented out:

```c
typedef enum {
    // GB_MODEL_DMG_0 = 0x000,  // NOT IMPLEMENTED
    // GB_MODEL_DMG_A = 0x001,  // NOT IMPLEMENTED
    GB_MODEL_DMG_B = 0x002,     // ✅ Only active DMG model
    // GB_MODEL_DMG_C = 0x003,  // NOT IMPLEMENTED
} GB_model_t;
```

**Conclusion:** There is no "SameBoy does DMG differently" concern. It's the same DMG-B target as our emulator.

### DMG Revision Differences

| Feature | DMG-0 | DMG-A | DMG-B | DMG-C |
|---------|-------|-------|-------|-------|
| Boot logo ® symbol | ❌ Missing | ✅ Has | ✅ Has | ✅ Has |
| Wave RAM retrigger corruption | Different pattern | Has "wave glitch" | ✅ No wave glitch | Same as B |
| OAM bug behavior | Same | Same | Same | Same |
| Timer behavior | Same | Same | Same | Same |
| PPU timing | Same | Same | Same | Same |

Key differences between DMG variants are **only** in the boot ROM and APU wave channel. CPU timing, PPU timing, timer, and memory bus behavior are identical across all DMG revisions.

### Failing Test Cross-Reference vs SameBoy

| Test | SameBoy DMG-B | Variant Issue? | Root Cause |
|------|---------------|----------------|------------|
| `hblank_ly_scx_timing-GS` | ✅ PASS | **No** | ✅ Fixed — SCX M-cycle alignment penalty |
| `boot_sclk_align-dmgABCmgb` | ✅ PASS | **No** | ✅ Fixed — Serial clock init |
| `boot_hwio-dmgABCmgb` | ✅ PASS | **No** | ✅ Fixed — I/O register post-boot state |

**All 3 previously failing tests are now fixed.** Full 94/94 Mooneye compliance achieved.

### Mooneye Test Naming Convention

| Suffix | Target Models | Notes |
|--------|---------------|-------|
| (none) | Universal | Should pass on all models |
| `-GS` | DMG+MGB+SGB+SGB2 | "G" = DMG+MGB, "S" = SGB+SGB2 |
| `-dmg0` | DMG-0 only | Earliest DMG revision |
| `-dmgABC` | DMG-A/B/C only | Post-0 DMG revisions |
| `-dmgABCmgb` | DMG-A/B/C + MGB | Our target group |
| `-S` | SGB+SGB2 only | Super Game Boy specific |
| `-C` | CGB+AGB+AGS | Game Boy Color family |
| `-A` | AGB+AGS only | Game Boy Advance only |

### CGB Preparation Notes

When ready for CGB support:
- SameBoy supports **6 CGB revisions**: CGB-0, CGB-A, CGB-B, CGB-C, CGB-D, CGB-E
- CGB-E is the most common real-world unit and best documented
- Key CGB features: double speed mode, HDMA/GDMA, VRAM banking, palette RAM, CGB compatibility mode for DMG games
- Mooneye has CGB-specific tests in `misc/` directory
- Universal acceptance tests (no suffix) should still pass on CGB
- SameBoy's model enum uses family masks: `GB_MODEL_DMG_FAMILY = 0x000`, `GB_MODEL_CGB_FAMILY = 0x200`

