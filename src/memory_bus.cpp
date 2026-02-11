#include "memory_bus.h"
#include "cartridge.h"
#include <cstdio>
#include <fstream>

void MemoryBus::loadCartridge(Cartridge* cart) {
    cart_ = cart;
}

void MemoryBus::init() {
    // Wire up the timer's IF register pointer to our IO array's FF0F slot
    timer_.connectIF(&io_[0x0F]);
    // Wire up the PPU's IF register pointer
    ppu_.connectIF(&io_[0x0F]);
    // Wire up the joypad's IF register pointer
    joypad_.connectIF(&io_[0x0F]);
}

// ══════════════════════════════════════════════════════════════════════
// Bootrom loading
// ══════════════════════════════════════════════════════════════════════

bool MemoryBus::loadBootrom(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    file.read(reinterpret_cast<char*>(bootrom_.data()), 256);
    if (file.gcount() != 256) return false;

    bootromLoaded_ = true;
    bootromActive_ = true;
    return true;
}

// ══════════════════════════════════════════════════════════════════════
// Unused I/O register mask — returns the OR mask for each IO address.
// Bits that are unused/unreadable read as 1.
// ══════════════════════════════════════════════════════════════════════

static uint8_t ioReadMask(uint8_t reg) {
    switch (reg) {
        // Joypad — bits 7-6 unused (always 1), bits 3-0 = input lines
        // (active-low: 1 = not pressed; no input support yet so all high)
        case 0x00: return 0xCF;

        // Serial
        case 0x01: return 0x00; // FF01: SB — fully readable
        case 0x02: return 0x7E; // FF02: SC — bits 1-6 unused on DMG

        // Unused
        case 0x03: return 0xFF;

        // Timer
        case 0x04: return 0x00; // DIV
        case 0x05: return 0x00; // TIMA
        case 0x06: return 0x00; // TMA
        case 0x07: return 0xF8; // TAC: upper 5 bits unused

        // Unused FF08-FF0E
        case 0x08: case 0x09: case 0x0A: case 0x0B:
        case 0x0C: case 0x0D: case 0x0E:
            return 0xFF;

        // IF
        case 0x0F: return 0xE0; // Upper 3 bits always read as 1

        // Audio (NR10-NR52) — return 0xFF for now (no APU)
        case 0x10: return 0x80;
        case 0x11: return 0x3F;
        case 0x12: return 0x00;
        case 0x13: return 0xFF;
        case 0x14: return 0xBF;
        case 0x15: return 0xFF;
        case 0x16: return 0x3F;
        case 0x17: return 0x00;
        case 0x18: return 0xFF;
        case 0x19: return 0xBF;
        case 0x1A: return 0x7F;
        case 0x1B: return 0xFF;
        case 0x1C: return 0x9F;
        case 0x1D: return 0xFF;
        case 0x1E: return 0xBF;
        case 0x1F: return 0xFF;
        case 0x20: return 0xFF;
        case 0x21: return 0x00;
        case 0x22: return 0x00;
        case 0x23: return 0xBF;
        case 0x24: return 0x00;
        case 0x25: return 0x00;
        case 0x26: return 0x70;

        // Unused FF27-FF2F
        case 0x27: case 0x28: case 0x29: case 0x2A: case 0x2B:
        case 0x2C: case 0x2D: case 0x2E: case 0x2F:
            return 0xFF;

        // Wave RAM FF30-FF3F — fully readable
        case 0x30: case 0x31: case 0x32: case 0x33:
        case 0x34: case 0x35: case 0x36: case 0x37:
        case 0x38: case 0x39: case 0x3A: case 0x3B:
        case 0x3C: case 0x3D: case 0x3E: case 0x3F:
            return 0x00;

        // LCD
        case 0x40: return 0x00; // LCDC
        case 0x41: return 0x80; // STAT: bit 7 unused
        case 0x42: return 0x00; // SCY
        case 0x43: return 0x00; // SCX
        case 0x44: return 0x00; // LY
        case 0x45: return 0x00; // LYC
        case 0x46: return 0x00; // DMA
        case 0x47: return 0x00; // BGP
        case 0x48: return 0x00; // OBP0
        case 0x49: return 0x00; // OBP1
        case 0x4A: return 0x00; // WY
        case 0x4B: return 0x00; // WX

        // Unused FF4C-FF4F (on DMG)
        case 0x4C: case 0x4D: case 0x4E: case 0x4F:
            return 0xFF;

        // FF50: boot ROM disable — reads as 0xFF on DMG
        case 0x50: return 0xFF;

        // Unused FF51-FF7F (CGB-only or unused on DMG)
        default:
            if (reg >= 0x51 && reg <= 0x7F) return 0xFF;
            return 0x00;
    }
}

// ══════════════════════════════════════════════════════════════════════
// read()
// ══════════════════════════════════════════════════════════════════════

uint8_t MemoryBus::read(uint16_t addr) const {
    // ── Bootrom overlay (0x0000–0x00FF) ──────────────────────────────
    if (addr < 0x0100 && bootromActive_) {
        return bootrom_[addr];
    }

    // ── ROM (0x0000–0x7FFF) ── delegated to MBC via Cartridge ──────────
    if (addr < 0x8000) {
        if (cart_) return cart_->read(addr);
        return 0xFF;
    }

    // ── VRAM (0x8000–0x9FFF) — delegated to PPU ──────────────────────
    if (addr < 0xA000) {
        return ppu_.readVRAM(addr);
    }

    // ── External RAM (0xA000–0xBFFF) ── delegated to MBC via Cartridge ──
    if (addr < 0xC000) {
        if (cart_) return cart_->read(addr);
        return 0xFF;
    }

    // ── Work RAM (0xC000–0xDFFF) ─────────────────────────────────────
    if (addr < 0xE000) {
        return wram_[addr - 0xC000];
    }

    // ── Echo RAM (0xE000–0xFDFF) — mirror of C000–DDFF ───────────────
    if (addr < 0xFE00) {
        return wram_[addr - 0xE000];
    }

    // ── OAM (0xFE00–0xFE9F) — delegated to PPU ──────────────────────
    if (addr < 0xFEA0) {
        // During OAM DMA, CPU reads from OAM return 0xFF
        if (dmaActive_) return 0xFF;
        return ppu_.readOAM(addr);
    }

    // ── Unusable (0xFEA0–0xFEFF) ─────────────────────────────────────
    if (addr < 0xFF00) {
        return 0xFF;
    }

    // ── IO registers (0xFF00–0xFF7F) ─────────────────────────────────
    if (addr < 0xFF80) {
        uint8_t reg = addr - 0xFF00;

        // Joypad register (FF00) is handled by the Joypad subsystem
        if (addr == 0xFF00) {
            return joypad_.readP1();
        }

        // Timer registers are handled by the Timer subsystem
        if (addr >= 0xFF04 && addr <= 0xFF07) {
            return timer_.read(addr);
        }

        // PPU registers are handled by the PPU subsystem
        if (addr >= 0xFF40 && addr <= 0xFF4B) {
            return ppu_.readReg(addr);
        }

        // Apply unused-bit mask: unused bits read as 1
        return io_[reg] | ioReadMask(reg);
    }

    // ── High RAM (0xFF80–0xFFFE) ─────────────────────────────────────
    if (addr < 0xFFFF) {
        return hram_[addr - 0xFF80];
    }

    // ── Interrupt Enable (0xFFFF) ────────────────────────────────────
    return ie_;
}

// ══════════════════════════════════════════════════════════════════════
// write()
// ══════════════════════════════════════════════════════════════════════

void MemoryBus::write(uint16_t addr, uint8_t val) {
    // ── ROM (0x0000–0x7FFF) ── delegated to MBC via Cartridge ──────────
    if (addr < 0x8000) {
        if (cart_) cart_->write(addr, val);
        return;
    }

    // ── VRAM (0x8000–0x9FFF) — delegated to PPU ──────────────────────
    if (addr < 0xA000) {
        ppu_.writeVRAM(addr, val);
        return;
    }

    // ── External RAM (0xA000–0xBFFF) ── delegated to MBC via Cartridge ──
    if (addr < 0xC000) {
        if (cart_) cart_->write(addr, val);
        return;
    }

    // ── Work RAM (0xC000–0xDFFF) ─────────────────────────────────────
    if (addr < 0xE000) {
        wram_[addr - 0xC000] = val;
        return;
    }

    // ── Echo RAM (0xE000–0xFDFF) ─────────────────────────────────────
    if (addr < 0xFE00) {
        wram_[addr - 0xE000] = val;
        return;
    }

    // ── OAM (0xFE00–0xFE9F) — delegated to PPU ──────────────────────
    if (addr < 0xFEA0) {
        // During OAM DMA, CPU writes to OAM are ignored
        if (dmaActive_) return;
        ppu_.writeOAM(addr, val);
        return;
    }

    // ── Unusable (0xFEA0–0xFEFF) ─────────────────────────────────────
    if (addr < 0xFF00) {
        return;
    }

    // ── IO registers (0xFF00–0xFF7F) ─────────────────────────────────
    if (addr < 0xFF80) {
        uint8_t reg = addr - 0xFF00;

        // Joypad register (FF00) is handled by the Joypad subsystem
        if (addr == 0xFF00) {
            joypad_.writeP1(val);
            return;
        }

        // Timer registers are handled by the Timer subsystem
        if (addr >= 0xFF04 && addr <= 0xFF07) {
            timer_.write(addr, val);
            return;
        }

        // PPU registers are handled by the PPU subsystem
        if (addr >= 0xFF40 && addr <= 0xFF4B) {
            ppu_.writeReg(addr, val);

            // OAM DMA trigger (FF46) — also handled here for bus control
            if (addr == 0xFF46) {
                dmaActive_ = true;
                dmaSrc_    = static_cast<uint16_t>(val) << 8;
                dmaByte_   = 0;
                dmaClock_  = 0;
                dmaDelay_  = 8; // 2 M-cycles startup delay
            }
            return;
        }

        // Serial transfer: capture output for test ROMs
        if (addr == 0xFF02 && val == 0x81) {
            char c = static_cast<char>(io_[0x01]); // SB = FF01
            std::printf("%c", c);
            std::fflush(stdout);
            serialReady_ = true;
        }

        // Bootrom disable (FF50)
        if (addr == 0xFF50 && bootromActive_) {
            if (val & 0x01) {
                bootromActive_ = false;
            }
        }

        // ── Hardware-accurate write masks for specific registers ──────
        switch (reg) {
            case 0x26: // NR52: only bit 7 (master enable) is writable
                io_[0x26] = (io_[0x26] & 0x7F) | (val & 0x80);
                return;
            default:
                io_[reg] = val;
                return;
        }
    }

    // ── High RAM (0xFF80–0xFFFE) ─────────────────────────────────────
    if (addr < 0xFFFF) {
        hram_[addr - 0xFF80] = val;
        return;
    }

    // ── Interrupt Enable (0xFFFF) ────────────────────────────────────
    ie_ = val;
}

// ══════════════════════════════════════════════════════════════════════
// tick() — advance 1 T-cycle
// ══════════════════════════════════════════════════════════════════════

void MemoryBus::tick() {
    // ── Timer subsystem ──────────────────────────────────────────────
    timer_.tick();

    // ── PPU subsystem ────────────────────────────────────────────────
    ppu_.tick();

    // ── OAM DMA ──────────────────────────────────────────────────────
    if (dmaActive_) {
        if (dmaDelay_ > 0) {
            // Startup delay
            dmaDelay_--;
        } else {
            // Copy one byte every 4 T-cycles (1 M-cycle)
            dmaClock_++;
            if (dmaClock_ >= 4) {
                dmaClock_ = 0;
                if (dmaByte_ < 160) {
                    // Read from source, write to OAM
                    // Use a direct internal read to avoid bus conflict on OAM
                    uint16_t srcAddr = dmaSrc_ + dmaByte_;
                    uint8_t val = 0xFF;

                    // Read from the appropriate memory region
                    if (srcAddr < 0x8000) {
                        if (cart_) val = cart_->read(srcAddr);
                    } else if (srcAddr < 0xA000) {
                        // DMA reads VRAM directly (bypasses PPU blocking)
                        val = ppu_.readVRAM(srcAddr);
                    } else if (srcAddr < 0xC000) {
                        if (cart_) val = cart_->read(srcAddr);
                    } else if (srcAddr < 0xE000) {
                        val = wram_[srcAddr - 0xC000];
                    } else if (srcAddr < 0xFE00) {
                        val = wram_[srcAddr - 0xE000]; // Echo RAM
                    }
                    // OAM DMA from OAM/IO/HRAM regions is undefined

                    // Write directly to PPU OAM, bypassing mode blocking
                    ppu_.dmaWriteOAM(dmaByte_, val);
                    dmaByte_++;

                    if (dmaByte_ >= 160) {
                        dmaActive_ = false;
                    }
                }
            }
        }
    }

    // ── Serial transfer timer ────────────────────────────────────────
    if (serialTimer_ > 0) {
        serialTimer_--;
        if (serialTimer_ == 0) {
            io_[0x02] &= ~0x80; // Clear transfer-in-progress bit
            io_[0x0F] |= 0x08;  // Request serial interrupt (IF bit 3)
        }
    }
}

char MemoryBus::consumeSerial() {
    serialReady_ = false;
    return static_cast<char>(io_[0x01]);
}
