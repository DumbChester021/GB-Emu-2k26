# 🎮 GB-Emu-2k26

A **cycle-accurate Game Boy (DMG) emulator** written in C++17 with SDL2.

<p align="center">
  <strong>94/94 Mooneye tests passing</strong> · <strong>Full audio</strong> · <strong>Save states</strong> · <strong>~5,800 LOC</strong>
</p>

---

## ✨ Features

| Feature | Status |
|---------|--------|
| **CPU** — All opcodes including CB-prefix, HALT, interrupts | ✅ M-cycle accurate |
| **PPU** — Pixel FIFO, sprite evaluation, all mode timings | ✅ T-cycle (per-dot) accurate |
| **APU** — All 4 channels: Pulse×2, Wave, Noise | ✅ T-cycle accurate with SDL2 audio |
| **Timer** — DIV/TIMA with falling-edge detection | ✅ T-cycle accurate |
| **MBC** — MBC1, MBC2, MBC3 (no RTC), MBC5 | ✅ Full bank switching |
| **OAM DMA** — Bus conflicts, restart, timing | ✅ SameBoy-style per-bus conflicts |
| **Save States** — Binary serialization with CRC32 | ✅ F5 save, F9 load |
| **Battery Saves** — Cartridge SRAM persistence | ✅ Auto save/load |
| **Mooneye Test Suite** — 94/94 DMG-ABC tests | ✅ See [test results](#test-results) |

---

## 🚀 Getting Started

### Prerequisites

- **C++17** compiler (GCC 7+, Clang 5+)
- **CMake** 3.10+
- **SDL2** development libraries

#### Ubuntu / Debian
```bash
sudo apt install build-essential cmake libsdl2-dev
```

#### Fedora
```bash
sudo dnf install gcc-c++ cmake SDL2-devel
```

#### Arch Linux
```bash
sudo pacman -S base-devel cmake sdl2
```

### Build

```bash
git clone https://github.com/DumbChester021/GB-Emu-2k26.git
cd GB-Emu-2k26
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Run

```bash
# With a file picker dialog (remembers last directory)
./build/gbemu

# Direct ROM path
./build/gbemu path/to/rom.gb

# Test mode (for Mooneye test ROMs)
./build/gbemu --test path/to/test.gb
```

---

## 🎮 Controls

| Key | Game Boy Button |
|-----|----------------|
| **↑ ↓ ← →** | D-Pad |
| **Z** | A |
| **X** | B |
| **Enter** | Start |
| **Backspace** | Select |
| **F5** | Save State |
| **F9** | Load State |
| **Escape** | Quit |

---

## 🏗️ Architecture

```
┌──────────────────────────────────────────────────────┐
│                      CPU (SM83)                       │
│            M-cycle accurate (4 T-cycles)             │
│    readByte → bus.read → tick4    (read-before-tick)  │
└──────────────┬───────────────────────────────────────┘
               │
               ▼
┌──────────────────────────────────────────────────────┐
│                    Memory Bus                         │
│                                                       │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐ │
│  │ Timer   │  │  PPU    │  │  APU    │  │ Joypad  │ │
│  │ FF04-07 │  │ FF40-4B │  │FF10-26  │  │  FF00   │ │
│  │         │  │         │  │FF30-3F  │  │         │ │
│  └─────────┘  └─────────┘  └─────────┘  └─────────┘ │
│                                                       │
│  ┌─────────────────────────────────────────────────┐  │
│  │   Cartridge / MBC   │   WRAM   │  OAM  │  HRAM │  │
│  └─────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────┘
```

### Subsystem Pattern

Every hardware component follows the same integration pattern:
1. **Class** with `tick()`, `readReg()`, `writeReg()`, `serialize()`, `deserialize()`
2. **Memory Bus** routes IO register reads/writes to the component
3. **`MemoryBus::tick()`** calls `component.tick()` once per T-cycle
4. **Interrupt wiring** via `connectIF(&io_[0x0F])` for shared IF register access

### Audio Pipeline

```
T-cycle tick → Channel frequency timers → Frame sequencer (512 Hz)
     ↓
  Mix 4 channels → NR50/NR51 stereo panning → Master volume
     ↓
  Downsample (4.19 MHz → 44100 Hz) → High-pass filter → Ring buffer
     ↓
  SDL audio callback (separate thread) → Speakers
```

---

## 📊 Test Results

**94/94 Mooneye DMG-ABC tests passing** (run via `bash run_mooneye_all.sh`)

| Category | Score | Status |
|----------|-------|--------|
| CPU Instructions | 1/1 | ✅ Perfect |
| Bits | 3/3 | ✅ Perfect |
| Interrupts | 3/3 | ✅ Perfect |
| EI/DI Timing | 4/4 | ✅ Perfect |
| HALT | 4/4 | ✅ Perfect |
| Call/JP/Ret/Pop/Push/RST | 13/13 | ✅ Perfect |
| ADD SP / LD HL,SP+e | 2/2 | ✅ Perfect |
| DIV Timing | 1/1 | ✅ Perfect |
| Timer | 13/13 | ✅ Perfect |
| OAM DMA | 6/6 | ✅ Perfect |
| PPU | 12/12 | ✅ Perfect |
| Boot Regs (DMG-ABC) | 1/1 | ✅ Perfect |
| Boot DIV (DMG-ABC) | 1/1 | ✅ Perfect |
| Boot HWIO (DMG-ABC) | 1/1 | ✅ Perfect |
| Serial | 1/1 | ✅ Perfect |
| MBC1 | 13/13 | ✅ Perfect |
| MBC2 | 7/7 | ✅ Perfect |
| MBC5 | 8/8 | ✅ Perfect |

> All 94 DMG-ABC tests pass — perfect Mooneye compliance.

---

## 📁 Project Structure

```
GB-Emu-2k26/
├── src/
│   ├── cpu.cpp / cpu.h          # SM83 CPU core
│   ├── cpu_opcodes.cpp          # Main opcode handlers
│   ├── cpu_cb_opcodes.cpp       # CB-prefix opcode handlers
│   ├── cpu_tables.cpp           # Opcode metadata tables
│   ├── ppu.cpp / ppu.h          # Pixel Processing Unit
│   ├── apu.cpp / apu.h          # Audio Processing Unit
│   ├── apu_serialize.cpp        # APU save state support
│   ├── timer.cpp / timer.h      # DIV/TIMA timer
│   ├── memory_bus.cpp / .h      # Bus routing and subsystem integration
│   ├── memory_bus_serialize.cpp # Bus save state support
│   ├── cartridge.cpp / .h       # ROM loading and header parsing
│   ├── mbc.h                    # Memory bank controllers
│   ├── joypad.cpp / .h          # Input handling
│   ├── save_state.h             # Binary serialization framework
│   ├── settings.cpp / .h        # Persistent emulator settings (~/.config/gbemu/)
│   ├── main.cpp                 # SDL2 init, render loop, audio, input
│   └── file_dialog.cpp / .h     # Native file picker (remembers last directory)
├── docs/
│   ├── DEVLOG.md                # Development log and architecture reference
│   └── GAP_ANALYSIS.md          # Mooneye test compliance analysis
├── bootroms/                    # DMG boot ROM
├── test_roms/                   # Mooneye test suite
├── CMakeLists.txt               # Build configuration
├── run_mooneye_all.sh           # Comprehensive test runner
└── build_and_run.sh             # Quick build + run script
```

---

## 📖 Documentation

- **[Development Log](docs/DEVLOG.md)** — Architecture reference, critical bug fixes with root cause analysis, debugging playbook
- **[Gap Analysis](docs/GAP_ANALYSIS.md)** — Mooneye test compliance breakdown with prioritized remaining work

---

## 🛠️ Development

### Run Tests
```bash
# Full Mooneye suite (builds automatically)
bash run_mooneye_all.sh

# Individual test
./build/gbemu --test test_roms/mts-20240926-1737-443f6e1/acceptance/ppu/intr_2_0_timing.gb
```

### Build Types
```bash
# Debug build (with symbols)
cmake .. -DCMAKE_BUILD_TYPE=Debug && make -j$(nproc)

# Release build (optimized)
cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)
```

---

## 📄 License

This project is for educational purposes — a learning exercise in hardware emulation and cycle-accurate system design.

---

## 🙏 Acknowledgments

- [Pan Docs](https://gbdev.io/pandocs/) — The comprehensive Game Boy technical reference
- [Mooneye Test Suite](https://github.com/Gekkio/mooneye-test-suite) — Hardware-accurate test ROMs
- [SameBoy](https://github.com/LIJI32/SameBoy) — Reference for OAM DMA bus conflict behavior
- [GBEDG](https://hacktix.github.io/GBEDG/) — Game Boy Emulator Development Guide
