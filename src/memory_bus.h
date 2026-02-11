#pragma once

#include <array>
#include <cstdint>
#include <string>

class SaveState;
#include "timer.h"
#include "ppu.h"
#include "joypad.h"

class Cartridge;

class MemoryBus {
public:
    void loadCartridge(Cartridge* cart);
    void init();  // Call after construction to wire up internal references

    // Load DMG bootrom (256 bytes). Returns true on success.
    bool loadBootrom(const std::string& path);
    bool bootromActive() const { return bootromActive_; }

    uint8_t  read(uint16_t addr) const;
    void     write(uint16_t addr, uint8_t val);

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

    // Work RAM: C000–DFFF (8 KB)
    std::array<uint8_t, 0x2000> wram_{};

    // High RAM: FF80–FFFE (127 bytes)
    std::array<uint8_t, 0x7F> hram_{};

    // IO registers (minimal stubs)
    std::array<uint8_t, 0x80> io_{};     // FF00–FF7F

    // VRAM & OAM are now owned by PPU (ppu_)

    // Interrupt enable register: FFFF
    uint8_t ie_ = 0;

    // Serial transfer
    bool serialReady_ = false;


    // ── Serial transfer timer ────────────────────────────────────────
    int serialTimer_ = 0;         // T-cycles remaining for transfer
    static constexpr int SERIAL_CYCLES = 512; // 8 bits × 64 cycles each (internal clock)

    // ── OAM DMA ─────────────────────────────────────────────────────
    bool     dmaActive_  = false;
    uint16_t dmaSrc_     = 0;     // Source address (val << 8)
    int      dmaByte_    = 0;     // Current byte index (0–159)
    int      dmaClock_   = 0;     // T-cycle counter within current byte
    int      dmaDelay_   = 0;     // Startup delay (8 T-cycles = 2 M-cycles)
};
