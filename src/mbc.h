#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

class SaveState;

// ═══════════════════════════════════════════════════════════════════════
//  MBC base class — handles ROM (0x0000–0x7FFF) and external RAM
//  (0xA000–0xBFFF) reads/writes
// ═══════════════════════════════════════════════════════════════════════

class MBC {
public:
    virtual ~MBC() = default;

    virtual uint8_t read(uint16_t addr) const = 0;
    virtual void    write(uint16_t addr, uint8_t val) = 0;

    // Save state serialization
    virtual void serialize(SaveState& ss) const = 0;
    virtual void deserialize(SaveState& ss) = 0;

    // Call after the owning Cartridge is moved/copied to fix dangling refs
    void rebindStorage(std::vector<uint8_t>& rom, std::vector<uint8_t>& ram) {
        rom_ = &rom;
        ram_ = &ram;
    }

protected:
    std::vector<uint8_t>* rom_;
    std::vector<uint8_t>* ram_;
    int romBankCount_;
    int ramBankCount_;

    MBC(std::vector<uint8_t>& rom, std::vector<uint8_t>& ram,
        int romBanks, int ramBanks)
        : rom_(&rom), ram_(&ram),
          romBankCount_(romBanks), ramBankCount_(ramBanks) {}

    // Safe ROM read with masking
    uint8_t romRead(uint32_t absoluteAddr) const {
        if (rom_->empty()) return 0xFF;
        return (*rom_)[absoluteAddr % rom_->size()];
    }

    // Safe RAM read/write with masking
    uint8_t ramRead(uint32_t absoluteAddr) const {
        if (ram_->empty()) return 0xFF;
        return (*ram_)[absoluteAddr % ram_->size()];
    }

    void ramWrite(uint32_t absoluteAddr, uint8_t val) {
        if (!ram_->empty()) {
            (*ram_)[absoluteAddr % ram_->size()] = val;
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════
//  NoMBC — ROM ONLY (type 0x00, 0x08, 0x09)
// ═══════════════════════════════════════════════════════════════════════

class NoMBC : public MBC {
public:
    NoMBC(std::vector<uint8_t>& rom, std::vector<uint8_t>& ram,
          int romBanks, int ramBanks)
        : MBC(rom, ram, romBanks, ramBanks) {}

    uint8_t read(uint16_t addr) const override {
        if (addr < 0x8000) {
            return romRead(addr);
        }
        // 0xA000–0xBFFF: external RAM
        if (addr >= 0xA000 && addr < 0xC000) {
            return ramRead(addr - 0xA000);
        }
        return 0xFF;
    }

    void write(uint16_t addr, uint8_t val) override {
        // ROM writes are ignored for NoMBC
        if (addr >= 0xA000 && addr < 0xC000) {
            ramWrite(addr - 0xA000, val);
        }
    }

    void serialize(SaveState&) const override { /* no banking state */ }
    void deserialize(SaveState&) override { /* no banking state */ }
};

// ═══════════════════════════════════════════════════════════════════════
//  MBC1 — types 0x01–0x03
//
//  Registers:
//    RAMG  (0x0000–0x1FFF): RAM enable (lower nibble == 0x0A)
//    BANK1 (0x2000–0x3FFF): 5-bit ROM bank number (0→1 fixup)
//    BANK2 (0x4000–0x5FFF): 2-bit (RAM bank / upper ROM bits)
//    MODE  (0x6000–0x7FFF): banking mode (0=simple, 1=advanced)
//
//  Addressing:
//    Mode 0:
//      0x0000–0x3FFF → ROM bank 0
//      0x4000–0x7FFF → ROM bank (BANK2<<5 | BANK1), masked
//      0xA000–0xBFFF → RAM bank 0
//    Mode 1:
//      0x0000–0x3FFF → ROM bank (BANK2<<5), masked
//      0x4000–0x7FFF → ROM bank (BANK2<<5 | BANK1), masked
//      0xA000–0xBFFF → RAM bank BANK2
// ═══════════════════════════════════════════════════════════════════════

class MBC1Controller : public MBC {
public:
    MBC1Controller(std::vector<uint8_t>& rom, std::vector<uint8_t>& ram,
                   int romBanks, int ramBanks, bool multicart = false)
        : MBC(rom, ram, romBanks, ramBanks), multicart_(multicart) {}

    uint8_t read(uint16_t addr) const override {
        const int shift = multicart_ ? 4 : 5;
        // In MBC1M, only the lower 4 bits of BANK1 connect to ROM address
        const int bank1Eff = multicart_ ? (bank1_ & 0x0F) : bank1_;

        if (addr < 0x4000) {
            // Bank 0 area — affected by mode
            int bank = 0;
            if (mode_) {
                bank = (bank2_ << shift) & (romBankCount_ - 1);
            }
            return romRead(bank * 0x4000 + addr);
        }
        if (addr < 0x8000) {
            // Switchable bank area
            int bank = (bank2_ << shift) | bank1Eff;
            // Mask to actual ROM size
            bank &= (romBankCount_ - 1);
            return romRead(bank * 0x4000 + (addr - 0x4000));
        }
        if (addr >= 0xA000 && addr < 0xC000) {
            if (!ramEnabled_) return 0xFF;
            int ramBank = mode_ ? (bank2_ & (ramBankCount_ > 0 ? ramBankCount_ - 1 : 0)) : 0;
            return ramRead(ramBank * 0x2000 + (addr - 0xA000));
        }
        return 0xFF;
    }

    void write(uint16_t addr, uint8_t val) override {
        if (addr < 0x2000) {
            // RAM enable
            ramEnabled_ = ((val & 0x0F) == 0x0A);
        } else if (addr < 0x4000) {
            // 5-bit ROM bank number — 0 maps to 1
            // In MBC1M, the register is still 5 bits; only the lower 4 bits
            // connect to ROM address lines, but the fixup checks all 5 bits.
            bank1_ = val & 0x1F;
            if (bank1_ == 0) bank1_ = 1;
        } else if (addr < 0x6000) {
            // 2-bit register
            bank2_ = val & 0x03;
        } else if (addr < 0x8000) {
            // Mode select
            mode_ = val & 0x01;
        } else if (addr >= 0xA000 && addr < 0xC000) {
            if (!ramEnabled_) return;
            int ramBank = mode_ ? (bank2_ & (ramBankCount_ > 0 ? ramBankCount_ - 1 : 0)) : 0;
            ramWrite(ramBank * 0x2000 + (addr - 0xA000), val);
        }
    }

    void serialize(SaveState& ss) const override;
    void deserialize(SaveState& ss) override;

private:
    bool ramEnabled_ = false;
    int  bank1_ = 1;   // 5-bit ROM bank (never 0)
    int  bank2_ = 0;   // 2-bit register
    int  mode_  = 0;   // 0=simple, 1=advanced
    bool multicart_;
};

// ═══════════════════════════════════════════════════════════════════════
//  MBC2 — types 0x05–0x06
//
//  Built-in 512 × 4-bit RAM. Upper nibble reads as 0xF.
//  ROM bank register selected only when bit 8 of address is SET.
//  RAM enable when bit 8 of address is CLEAR.
// ═══════════════════════════════════════════════════════════════════════

class MBC2Controller : public MBC {
public:
    MBC2Controller(std::vector<uint8_t>& rom, std::vector<uint8_t>& ram,
                   int romBanks, int ramBanks)
        : MBC(rom, ram, romBanks, ramBanks) {
        // MBC2 has built-in 512 bytes of 4-bit RAM
        if (ram_->size() < 512) ram_->resize(512, 0);
    }

    uint8_t read(uint16_t addr) const override {
        if (addr < 0x4000) {
            return romRead(addr);
        }
        if (addr < 0x8000) {
            int bank = romBank_ & (romBankCount_ - 1);
            return romRead(bank * 0x4000 + (addr - 0x4000));
        }
        if (addr >= 0xA000 && addr < 0xC000) {
            if (!ramEnabled_) return 0xFF;
            // Only 512 bytes, mirrored. Upper nibble always 0xF.
            return (*ram_)[(addr - 0xA000) & 0x01FF] | 0xF0;
        }
        return 0xFF;
    }

    void write(uint16_t addr, uint8_t val) override {
        if (addr < 0x4000) {
            if (addr & 0x0100) {
                // Bit 8 set → ROM bank register (4-bit)
                romBank_ = val & 0x0F;
                if (romBank_ == 0) romBank_ = 1;
            } else {
                // Bit 8 clear → RAM enable
                ramEnabled_ = ((val & 0x0F) == 0x0A);
            }
        } else if (addr >= 0xA000 && addr < 0xC000) {
            if (!ramEnabled_) return;
            // Only lower nibble is writable
            (*ram_)[(addr - 0xA000) & 0x01FF] = val & 0x0F;
        }
    }

    void serialize(SaveState& ss) const override;
    void deserialize(SaveState& ss) override;

private:
    bool ramEnabled_ = false;
    int  romBank_ = 1;
};

// ═══════════════════════════════════════════════════════════════════════
//  MBC3 — types 0x0F–0x13
//
//  7-bit ROM bank, 2-bit RAM bank (or RTC register select 0x08–0x0C).
//  RTC registers are stubbed to 0.
// ═══════════════════════════════════════════════════════════════════════

class MBC3Controller : public MBC {
public:
    MBC3Controller(std::vector<uint8_t>& rom, std::vector<uint8_t>& ram,
                   int romBanks, int ramBanks, bool hasRTC = false)
        : MBC(rom, ram, romBanks, ramBanks), hasRTC_(hasRTC) {}

    uint8_t read(uint16_t addr) const override {
        if (addr < 0x4000) {
            return romRead(addr);
        }
        if (addr < 0x8000) {
            // MBC3 allows bank 0 in high bank (no 0→1 fixup unlike MBC1)
            int bank = romBank_ & (romBankCount_ - 1);
            return romRead(bank * 0x4000 + (addr - 0x4000));
        }
        if (addr >= 0xA000 && addr < 0xC000) {
            if (!ramRTCEnabled_) return 0xFF;
            if (ramBank_ <= 0x03) {
                // Normal RAM
                int bank = ramBank_;
                if (ramBankCount_ > 0) bank &= (ramBankCount_ - 1);
                return ramRead(bank * 0x2000 + (addr - 0xA000));
            }
            // RTC register (0x08–0x0C) — stub to 0
            return 0x00;
        }
        return 0xFF;
    }

    void write(uint16_t addr, uint8_t val) override {
        if (addr < 0x2000) {
            // RAM + RTC enable
            ramRTCEnabled_ = ((val & 0x0F) == 0x0A);
        } else if (addr < 0x4000) {
            // 7-bit ROM bank
            romBank_ = val & 0x7F;
            if (romBank_ == 0) romBank_ = 1;
        } else if (addr < 0x6000) {
            // RAM bank / RTC register select
            ramBank_ = val;
        } else if (addr < 0x8000) {
            // Latch clock data — stub (do nothing)
        } else if (addr >= 0xA000 && addr < 0xC000) {
            if (!ramRTCEnabled_) return;
            if (ramBank_ <= 0x03) {
                int bank = ramBank_;
                if (ramBankCount_ > 0) bank &= (ramBankCount_ - 1);
                ramWrite(bank * 0x2000 + (addr - 0xA000), val);
            }
            // RTC register writes — stub
        }
    }

    void serialize(SaveState& ss) const override;
    void deserialize(SaveState& ss) override;

private:
    bool ramRTCEnabled_ = false;
    int  romBank_ = 1;
    int  ramBank_ = 0;
    bool hasRTC_;
};

// ═══════════════════════════════════════════════════════════════════════
//  MBC5 — types 0x19–0x1E
//
//  9-bit ROM bank (0x2000 = low 8 bits, 0x3000 = bit 8).
//  4-bit RAM bank. Bank 0 IS allowed (no fixup).
// ═══════════════════════════════════════════════════════════════════════

class MBC5Controller : public MBC {
public:
    MBC5Controller(std::vector<uint8_t>& rom, std::vector<uint8_t>& ram,
                   int romBanks, int ramBanks)
        : MBC(rom, ram, romBanks, ramBanks) {}

    uint8_t read(uint16_t addr) const override {
        if (addr < 0x4000) {
            return romRead(addr);
        }
        if (addr < 0x8000) {
            int bank = romBank_ & (romBankCount_ - 1);
            return romRead(bank * 0x4000 + (addr - 0x4000));
        }
        if (addr >= 0xA000 && addr < 0xC000) {
            if (!ramEnabled_) return 0xFF;
            int bank = ramBank_;
            if (ramBankCount_ > 0) bank &= (ramBankCount_ - 1);
            return ramRead(bank * 0x2000 + (addr - 0xA000));
        }
        return 0xFF;
    }

    void write(uint16_t addr, uint8_t val) override {
        if (addr < 0x2000) {
            ramEnabled_ = ((val & 0x0F) == 0x0A);
        } else if (addr < 0x3000) {
            // Low 8 bits of ROM bank
            romBank_ = (romBank_ & 0x100) | val;
        } else if (addr < 0x4000) {
            // Bit 8 of ROM bank
            romBank_ = (romBank_ & 0xFF) | ((val & 0x01) << 8);
        } else if (addr < 0x6000) {
            // 4-bit RAM bank
            ramBank_ = val & 0x0F;
        } else if (addr >= 0xA000 && addr < 0xC000) {
            if (!ramEnabled_) return;
            int bank = ramBank_;
            if (ramBankCount_ > 0) bank &= (ramBankCount_ - 1);
            ramWrite(bank * 0x2000 + (addr - 0xA000), val);
        }
    }

    void serialize(SaveState& ss) const override;
    void deserialize(SaveState& ss) override;

private:
    bool ramEnabled_ = false;
    int  romBank_ = 1;
    int  ramBank_ = 0;
};

// ═══════════════════════════════════════════════════════════════════════
//  Factory
// ═══════════════════════════════════════════════════════════════════════

inline int mbcRomBankCount(uint8_t romSizeCode) {
    if (romSizeCode <= 0x08) return 2 << romSizeCode;
    switch (romSizeCode) {
        case 0x52: return 72;
        case 0x53: return 80;
        case 0x54: return 96;
        default:   return 2;
    }
}

inline size_t mbcRamSizeBytes(uint8_t ramSizeCode) {
    switch (ramSizeCode) {
        case 0x00: return 0;
        case 0x01: return 0;       // "Unused" per spec
        case 0x02: return 8192;    // 8 KiB
        case 0x03: return 32768;   // 32 KiB
        case 0x04: return 131072;  // 128 KiB
        case 0x05: return 65536;   // 64 KiB
        default:   return 0;
    }
}

inline int mbcRamBankCount(uint8_t ramSizeCode) {
    size_t bytes = mbcRamSizeBytes(ramSizeCode);
    if (bytes == 0) return 0;
    return static_cast<int>(bytes / 8192);
}

// ── MBC1 multicart detection ─────────────────────────────────────────
// MBC1M multicarts are 8 Mbit (1 MB) MBC1 ROMs that contain multiple
// games, each starting at a 256 KB boundary. We detect them by checking
// for valid Nintendo logos at offsets 0x40104, 0x80104, 0xC0104.
static constexpr std::array<uint8_t, 48> NINTENDO_LOGO_MBC = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
    0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
    0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
    0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
    0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
    0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
};

inline bool detectMBC1Multicart(const std::vector<uint8_t>& rom) {
    // Only 8 Mbit (1 MB = 64 banks) ROMs can be MBC1M multicarts
    if (rom.size() != 0x100000) return false;

    // Check for valid Nintendo logos at each 256 KB sub-ROM header
    int logoCount = 0;
    for (uint32_t base : {0x40000u, 0x80000u, 0xC0000u}) {
        uint32_t logoAddr = base + 0x104;
        if (logoAddr + 48 > rom.size()) continue;
        if (std::equal(NINTENDO_LOGO_MBC.begin(), NINTENDO_LOGO_MBC.end(),
                       rom.begin() + logoAddr)) {
            logoCount++;
        }
    }
    // If at least one sub-ROM has a valid logo, it's a multicart
    return logoCount >= 1;
}

inline std::unique_ptr<MBC> createMBC(
    uint8_t cartType,
    std::vector<uint8_t>& rom,
    std::vector<uint8_t>& ram,
    uint8_t romSizeCode,
    uint8_t ramSizeCode)
{
    int romBanks = mbcRomBankCount(romSizeCode);
    int ramBanks = mbcRamBankCount(ramSizeCode);

    switch (cartType) {
        // ROM ONLY
        case 0x00: case 0x08: case 0x09:
            return std::make_unique<NoMBC>(rom, ram, romBanks, ramBanks);

        // MBC1 (with multicart auto-detection)
        case 0x01: case 0x02: case 0x03: {
            bool multicart = detectMBC1Multicart(rom);
            return std::make_unique<MBC1Controller>(rom, ram, romBanks, ramBanks, multicart);
        }

        // MBC2
        case 0x05: case 0x06:
            return std::make_unique<MBC2Controller>(rom, ram, romBanks, ramBanks);

        // MBC3
        case 0x0F: case 0x10:
            return std::make_unique<MBC3Controller>(rom, ram, romBanks, ramBanks, true);
        case 0x11: case 0x12: case 0x13:
            return std::make_unique<MBC3Controller>(rom, ram, romBanks, ramBanks, false);

        // MBC5
        case 0x19: case 0x1A: case 0x1B:
        case 0x1C: case 0x1D: case 0x1E:
            return std::make_unique<MBC5Controller>(rom, ram, romBanks, ramBanks);

        default:
            std::fprintf(stderr, "Warning: Unsupported cartridge type 0x%02X, falling back to NoMBC\n", cartType);
            return std::make_unique<NoMBC>(rom, ram, romBanks, ramBanks);
    }
}
