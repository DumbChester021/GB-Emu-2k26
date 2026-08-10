#pragma once

#include <array>
#include <cstdint>
#include <string>

class SaveState;
#include "timer.h"
#include "ppu.h"
#include "joypad.h"
#include "apu.h"

class Cartridge;

class MemoryBus {
public:
    void loadCartridge(Cartridge* cart);
    void init();  // Call after construction to wire up internal references

    // Load DMG bootrom (256 bytes). Returns true on success.
    bool loadBootrom(const std::string& path);
    bool bootromActive() const { return bootromActive_; }

    uint8_t  read(uint16_t addr);
    void     write(uint16_t addr, uint8_t val);

    // The SM83 16-bit increment/decrement unit exposes its operand on the
    // address bus, which can corrupt OAM on monochrome hardware.
    void triggerOAMBug(uint16_t addr) { ppu_.triggerOAMWriteCorruption(addr); }

    // Call once per T-cycle to advance internal timers (LY, serial, etc.)
    void tick();

    // Serial output capture (for test ROMs like Blargg's cpu_instrs)
    bool hasSerialOutput() const { return serialReady_; }
    char consumeSerial();

    // OAM DMA query
    bool isDmaActive() const { return dmaActive_; }

    // Save state serialization
    void serialize(SaveState& ss) const;
    void deserialize(SaveState& ss);

    // PPU accessor
    PPU& ppu() { return ppu_; }
    const PPU& ppu() const { return ppu_; }

    // Joypad accessor
    Joypad& joypad() { return joypad_; }
    const Joypad& joypad() const { return joypad_; }

    // APU accessor
    APU& apu() { return apu_; }
    const APU& apu() const { return apu_; }

    // Direct cartridge read (bypasses bus conflicts) — for test runner polling
    uint8_t readCartridge(uint16_t addr) const;

private:
    Cartridge* cart_ = nullptr;

    // ── Bootrom overlay ─────────────────────────────────────────────
    std::array<uint8_t, 256> bootrom_{};
    bool bootromLoaded_ = false;
    bool bootromActive_ = false;

    // ── Timer subsystem (FF04–FF07) ─────────────────────────────────
    Timer timer_;

    // ── PPU subsystem ───────────────────────────────────────────────
    PPU ppu_;

    // ── Joypad subsystem ────────────────────────────────────────────
    Joypad joypad_;

    // ── APU subsystem ───────────────────────────────────────────────
    APU apu_;

    // Work RAM: C000–DFFF (8 KB)
    std::array<uint8_t, 0x2000> wram_{};

    // High RAM: FF80–FFFE (127 bytes)
    std::array<uint8_t, 0x7F> hram_{};

    // IO registers (minimal stubs)
    std::array<uint8_t, 0x80> io_{};     // FF00–FF7F

    // VRAM & OAM are now owned by PPU (ppu_)

    // Interrupt enable register: FFFF
    uint8_t ie_ = 0;

    // ── Serial transfer (DIV-aligned clock) ──────────────────────────
    bool serialReady_ = false;
    bool serialMasterClock_ = false; // Toggles on each falling edge of serial_mask bit
    bool prevSerialBit_ = false;     // Previous state of the serial mask bit for edge detection
    uint8_t serialCount_ = 0;        // Bits shifted so far (0-8)
    static constexpr uint16_t SERIAL_MASK = 0x0080; // Bit 7 of sysCounter (DMG: 8192 Hz)

    // ── OAM DMA ─────────────────────────────────────────────────────
    bool     dmaActive_  = false;
    uint16_t dmaSrc_     = 0;     // Source address (val << 8)
    int      dmaByte_    = 0;     // Current byte index (0–159)
    int      dmaClock_   = 0;     // T-cycle counter within current byte
    int      dmaDelay_   = 0;     // Startup delay (8 T-cycles = 2 M-cycles)
    bool     dmaRestarting_ = false; // Previous DMA was running when restarted
    uint8_t  dmaLastByte_= 0xFF;  // Last byte transferred by DMA (returned on bus conflict)
};
