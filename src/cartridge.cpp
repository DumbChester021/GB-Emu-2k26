#include "cartridge.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>

// ═════════════════════════════════════════════════════════════════════
//  Lookup tables
// ═════════════════════════════════════════════════════════════════════

const char* Cartridge::cartridgeTypeName(uint8_t code) {
    switch (code) {
        case 0x00: return "ROM ONLY";
        case 0x01: return "MBC1";
        case 0x02: return "MBC1+RAM";
        case 0x03: return "MBC1+RAM+BATTERY";
        case 0x05: return "MBC2";
        case 0x06: return "MBC2+BATTERY";
        case 0x08: return "ROM+RAM";
        case 0x09: return "ROM+RAM+BATTERY";
        case 0x0B: return "MMM01";
        case 0x0C: return "MMM01+RAM";
        case 0x0D: return "MMM01+RAM+BATTERY";
        case 0x0F: return "MBC3+TIMER+BATTERY";
        case 0x10: return "MBC3+TIMER+RAM+BATTERY";
        case 0x11: return "MBC3";
        case 0x12: return "MBC3+RAM";
        case 0x13: return "MBC3+RAM+BATTERY";
        case 0x19: return "MBC5";
        case 0x1A: return "MBC5+RAM";
        case 0x1B: return "MBC5+RAM+BATTERY";
        case 0x1C: return "MBC5+RUMBLE";
        case 0x1D: return "MBC5+RUMBLE+RAM";
        case 0x1E: return "MBC5+RUMBLE+RAM+BATTERY";
        case 0x20: return "MBC6";
        case 0x22: return "MBC7+SENSOR+RUMBLE+RAM+BATTERY";
        case 0xFC: return "POCKET CAMERA";
        case 0xFD: return "BANDAI TAMA5";
        case 0xFE: return "HuC3";
        case 0xFF: return "HuC1+RAM+BATTERY";
        default:   return "UNKNOWN";
    }
}

const char* Cartridge::romSizeString(uint8_t code) {
    switch (code) {
        case 0x00: return "32 KiB  (2 banks, no banking)";
        case 0x01: return "64 KiB  (4 banks)";
        case 0x02: return "128 KiB (8 banks)";
        case 0x03: return "256 KiB (16 banks)";
        case 0x04: return "512 KiB (32 banks)";
        case 0x05: return "1 MiB   (64 banks)";
        case 0x06: return "2 MiB   (128 banks)";
        case 0x07: return "4 MiB   (256 banks)";
        case 0x08: return "8 MiB   (512 banks)";
        case 0x52: return "1.1 MiB (72 banks)";
        case 0x53: return "1.2 MiB (80 banks)";
        case 0x54: return "1.5 MiB (96 banks)";
        default:   return "UNKNOWN";
    }
}

const char* Cartridge::ramSizeString(uint8_t code) {
    switch (code) {
        case 0x00: return "No RAM";
        case 0x01: return "Unused";
        case 0x02: return "8 KiB   (1 bank)";
        case 0x03: return "32 KiB  (4 banks of 8 KiB)";
        case 0x04: return "128 KiB (16 banks of 8 KiB)";
        case 0x05: return "64 KiB  (8 banks of 8 KiB)";
        default:   return "UNKNOWN";
    }
}

const char* Cartridge::cgbFlagString(uint8_t flag) {
    switch (flag) {
        case 0x80: return "CGB Enhanced (backwards compatible)";
        case 0xC0: return "CGB Only";
        default:   return "Non-CGB (DMG)";
    }
}

const char* Cartridge::destinationString(uint8_t code) {
    switch (code) {
        case 0x00: return "Japan (and possibly overseas)";
        case 0x01: return "Overseas only";
        default:   return "Unknown";
    }
}

const char* Cartridge::newLicenseeName(const std::string& code) {
    if (code == "00") return "None";
    if (code == "01") return "Nintendo R&D1";
    if (code == "08") return "Capcom";
    if (code == "13") return "EA (Electronic Arts)";
    if (code == "18") return "Hudson Soft";
    if (code == "19") return "B-AI";
    if (code == "20") return "KSS";
    if (code == "22") return "Planning Office WADA";
    if (code == "24") return "PCM Complete";
    if (code == "25") return "San-X";
    if (code == "28") return "Kemco";
    if (code == "29") return "SETA Corporation";
    if (code == "30") return "Viacom";
    if (code == "31") return "Nintendo";
    if (code == "32") return "Bandai";
    if (code == "33") return "Ocean/Acclaim";
    if (code == "34") return "Konami";
    if (code == "35") return "HectorSoft";
    if (code == "37") return "Taito";
    if (code == "38") return "Hudson Soft";
    if (code == "39") return "Banpresto";
    if (code == "41") return "Ubi Soft";
    if (code == "42") return "Atlus";
    if (code == "44") return "Malibu Interactive";
    if (code == "46") return "Angel";
    if (code == "47") return "Bullet-Proof Software";
    if (code == "49") return "Irem";
    if (code == "50") return "Absolute";
    if (code == "51") return "Acclaim Entertainment";
    if (code == "52") return "Activision";
    if (code == "53") return "Sammy USA Corporation";
    if (code == "54") return "Konami";
    if (code == "55") return "Hi Tech Expressions";
    if (code == "56") return "LJN";
    if (code == "57") return "Matchbox";
    if (code == "58") return "Mattel";
    if (code == "59") return "Milton Bradley";
    if (code == "60") return "Titus Interactive";
    if (code == "61") return "Virgin Games";
    if (code == "64") return "Lucasfilm Games";
    if (code == "67") return "Ocean Software";
    if (code == "69") return "EA (Electronic Arts)";
    if (code == "70") return "Infogrames";
    if (code == "71") return "Interplay Entertainment";
    if (code == "72") return "Broderbund";
    if (code == "73") return "Sculptured Software";
    if (code == "75") return "The Sales Curve Limited";
    if (code == "78") return "THQ";
    if (code == "79") return "Accolade";
    if (code == "80") return "Misawa Entertainment";
    if (code == "83") return "LOZC G.";
    if (code == "86") return "Tokuma Shoten";
    if (code == "87") return "Tsukuda Original";
    if (code == "91") return "Chunsoft";
    if (code == "92") return "Video System";
    if (code == "93") return "Ocean/Acclaim";
    if (code == "95") return "Varie";
    if (code == "96") return "Yonezawa/S'Pal";
    if (code == "97") return "Kaneko";
    if (code == "99") return "Pack-In-Video";
    if (code == "9H") return "Bottom Up";
    if (code == "A4") return "Konami (Yu-Gi-Oh!)";
    if (code == "BL") return "MTO";
    if (code == "DK") return "Kodansha";
    return "Unknown";
}

const char* Cartridge::oldLicenseeName(uint8_t code) {
    switch (code) {
        case 0x00: return "None";
        case 0x01: return "Nintendo";
        case 0x08: return "Capcom";
        case 0x09: return "HOT-B";
        case 0x0A: return "Jaleco";
        case 0x0B: return "Coconuts Japan";
        case 0x0C: return "Elite Systems";
        case 0x13: return "EA (Electronic Arts)";
        case 0x18: return "Hudson Soft";
        case 0x19: return "ITC Entertainment";
        case 0x1A: return "Yanoman";
        case 0x1D: return "Japan Clary";
        case 0x1F: return "Virgin Games";
        case 0x24: return "PCM Complete";
        case 0x25: return "San-X";
        case 0x28: return "Kemco";
        case 0x29: return "SETA Corporation";
        case 0x30: return "Infogrames";
        case 0x31: return "Nintendo";
        case 0x32: return "Bandai";
        case 0x33: return "-> See New Licensee Code";
        case 0x34: return "Konami";
        case 0x35: return "HectorSoft";
        case 0x38: return "Capcom";
        case 0x39: return "Banpresto";
        case 0x3C: return "Entertainment Interactive";
        case 0x3E: return "Gremlin";
        case 0x41: return "Ubi Soft";
        case 0x42: return "Atlus";
        case 0x44: return "Malibu Interactive";
        case 0x46: return "Angel";
        case 0x47: return "Spectrum HoloByte";
        case 0x49: return "Irem";
        case 0x4A: return "Virgin Games";
        case 0x4D: return "Malibu Interactive";
        case 0x4F: return "U.S. Gold";
        case 0x50: return "Absolute";
        case 0x51: return "Acclaim Entertainment";
        case 0x52: return "Activision";
        case 0x53: return "Sammy USA Corporation";
        case 0x54: return "GameTek";
        case 0x55: return "Park Place";
        case 0x56: return "LJN";
        case 0x57: return "Matchbox";
        case 0x59: return "Milton Bradley";
        case 0x5A: return "Mindscape";
        case 0x5B: return "Romstar";
        case 0x5C: return "Naxat Soft";
        case 0x5D: return "Tradewest";
        case 0x60: return "Titus Interactive";
        case 0x61: return "Virgin Games";
        case 0x67: return "Ocean Software";
        case 0x69: return "EA (Electronic Arts)";
        case 0x6E: return "Elite Systems";
        case 0x6F: return "Electro Brain";
        case 0x70: return "Infogrames";
        case 0x71: return "Interplay Entertainment";
        case 0x72: return "Broderbund";
        case 0x73: return "Sculptured Software";
        case 0x75: return "The Sales Curve Limited";
        case 0x78: return "THQ";
        case 0x79: return "Accolade";
        case 0x7A: return "Triffix Entertainment";
        case 0x7C: return "MicroProse";
        case 0x7F: return "Kemco";
        case 0x80: return "Misawa Entertainment";
        case 0x83: return "LOZC G.";
        case 0x86: return "Tokuma Shoten";
        case 0x8B: return "Bullet-Proof Software";
        case 0x8C: return "Vic Tokai Corp.";
        case 0x8E: return "Ape Inc.";
        case 0x8F: return "I'Max";
        case 0x91: return "Chunsoft";
        case 0x92: return "Video System";
        case 0x93: return "Tsubaraya Productions";
        case 0x95: return "Varie";
        case 0x96: return "Yonezawa/S'Pal";
        case 0x97: return "Kemco";
        case 0x99: return "Arc";
        case 0x9A: return "Nihon Bussan";
        case 0x9B: return "Tecmo";
        case 0x9C: return "Imagineer";
        case 0x9D: return "Banpresto";
        case 0x9F: return "Nova";
        case 0xA1: return "Hori Electric";
        case 0xA2: return "Bandai";
        case 0xA4: return "Konami";
        case 0xA6: return "Kawada";
        case 0xA7: return "Takara";
        case 0xA9: return "Technos Japan";
        case 0xAA: return "Broderbund";
        case 0xAC: return "Toei Animation";
        case 0xAD: return "Toho";
        case 0xAF: return "Namco";
        case 0xB0: return "Acclaim Entertainment";
        case 0xB1: return "ASCII/Nexsoft";
        case 0xB2: return "Bandai";
        case 0xB4: return "Square Enix";
        case 0xB6: return "HAL Laboratory";
        case 0xB7: return "SNK";
        case 0xB9: return "Pony Canyon";
        case 0xBA: return "Culture Brain";
        case 0xBB: return "Sunsoft";
        case 0xBD: return "Sony Imagesoft";
        case 0xBF: return "Sammy Corporation";
        case 0xC0: return "Taito";
        case 0xC2: return "Kemco";
        case 0xC3: return "Square";
        case 0xC4: return "Tokuma Shoten";
        case 0xC5: return "Data East";
        case 0xC6: return "Tonkin House";
        case 0xC8: return "Koei";
        case 0xC9: return "UFL";
        case 0xCA: return "Ultra Games";
        case 0xCB: return "VAP, Inc.";
        case 0xCC: return "Use Corporation";
        case 0xCD: return "Meldac";
        case 0xCE: return "Pony Canyon";
        case 0xCF: return "Angel";
        case 0xD0: return "Taito";
        case 0xD1: return "SOFEL";
        case 0xD2: return "Quest";
        case 0xD3: return "Sigma Enterprises";
        case 0xD4: return "ASK Kodansha";
        case 0xD6: return "Naxat Soft";
        case 0xD7: return "Copya System";
        case 0xD9: return "Banpresto";
        case 0xDA: return "Tomy";
        case 0xDB: return "LJN";
        case 0xDD: return "Nippon Computer Systems";
        case 0xDE: return "Human Ent.";
        case 0xDF: return "Altron";
        case 0xE0: return "Jaleco";
        case 0xE1: return "Towa Chiki";
        case 0xE2: return "Yutaka";
        case 0xE3: return "Varie";
        case 0xE5: return "Epoch";
        case 0xE7: return "Athena";
        case 0xE8: return "Asmik Ace Entertainment";
        case 0xE9: return "Natsume";
        case 0xEA: return "King Records";
        case 0xEB: return "Atlus";
        case 0xEC: return "Epic/Sony Records";
        case 0xEE: return "IGS";
        case 0xF0: return "A Wave";
        case 0xF3: return "Extreme Entertainment";
        case 0xFF: return "LJN";
        default:   return "Unknown";
    }
}

// ═════════════════════════════════════════════════════════════════════
//  Loading
// ═════════════════════════════════════════════════════════════════════

std::optional<Cartridge> Cartridge::loadFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::fprintf(stderr, "Error: Failed to open file: %s\n", path.c_str());
        return std::nullopt;
    }

    auto size = file.tellg();
    if (size < HeaderAddr::HEADER_END + 1) {
        std::fprintf(stderr, "Error: ROM too small (%lld bytes). Minimum is %d bytes.\n",
                     static_cast<long long>(size), HeaderAddr::HEADER_END + 1);
        return std::nullopt;
    }

    Cartridge cart;
    cart.filepath = path;
    cart.rom.resize(static_cast<size_t>(size));

    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(cart.rom.data()), size);
    file.close();

    cart.parseHeader();

    // ── Allocate external RAM based on header ─────────────────────────
    size_t ramBytes = mbcRamSizeBytes(cart.ramSizeCode);
    // MBC2 has built-in 512×4-bit RAM regardless of header
    if (cart.cartridgeType == 0x05 || cart.cartridgeType == 0x06) {
        ramBytes = 512;
    }
    cart.externalRam.resize(ramBytes, 0);

    // ── Create appropriate MBC controller ─────────────────────────────
    cart.mbc = createMBC(cart.cartridgeType, cart.rom, cart.externalRam,
                         cart.romSizeCode, cart.ramSizeCode);

    return cart;
}

// ═════════════════════════════════════════════════════════════════════
//  Header parsing
// ═════════════════════════════════════════════════════════════════════

void Cartridge::parseHeader() {
    // Entry point ($0100–$0103)
    std::copy_n(rom.begin() + HeaderAddr::ENTRY_POINT, 4, entryPoint.begin());

    // Nintendo logo ($0104–$0133)
    std::copy_n(rom.begin() + HeaderAddr::LOGO, 48, logo.begin());

    // Title ($0134–$0143) — up to 16 chars, NUL-padded
    {
        const char* titleStart = reinterpret_cast<const char*>(&rom[HeaderAddr::TITLE]);
        size_t maxLen = 16;
        // Find the actual end (before NUL padding)
        size_t len = 0;
        while (len < maxLen && titleStart[len] != '\0') ++len;
        title = std::string(titleStart, len);
    }

    // Manufacturer code ($013F–$0142)
    {
        const char* mfgStart = reinterpret_cast<const char*>(&rom[HeaderAddr::MANUFACTURER]);
        size_t len = 0;
        while (len < 4 && mfgStart[len] != '\0') ++len;
        manufacturerCode = std::string(mfgStart, len);
    }

    // CGB flag ($0143)
    cgbFlag = rom[HeaderAddr::CGB_FLAG];

    // New licensee code ($0144–$0145) — 2-char ASCII
    newLicenseeCode = std::string(1, static_cast<char>(rom[HeaderAddr::NEW_LICENSEE]));
    newLicenseeCode += static_cast<char>(rom[HeaderAddr::NEW_LICENSEE + 1]);

    // SGB flag ($0146)
    sgbFlag = rom[HeaderAddr::SGB_FLAG];

    // Cartridge type ($0147)
    cartridgeType = rom[HeaderAddr::CARTRIDGE_TYPE];

    // ROM size ($0148)
    romSizeCode = rom[HeaderAddr::ROM_SIZE];

    // RAM size ($0149)
    ramSizeCode = rom[HeaderAddr::RAM_SIZE];

    // Destination code ($014A)
    destinationCode = rom[HeaderAddr::DEST_CODE];

    // Old licensee code ($014B)
    oldLicenseeCode = rom[HeaderAddr::OLD_LICENSEE];

    // ROM version ($014C)
    romVersion = rom[HeaderAddr::ROM_VERSION];

    // Header checksum ($014D)
    headerChecksum = rom[HeaderAddr::HEADER_CHECKSUM];

    // Global checksum ($014E–$014F) — big-endian
    globalChecksum = (static_cast<uint16_t>(rom[HeaderAddr::GLOBAL_CHECKSUM]) << 8)
                   |  static_cast<uint16_t>(rom[HeaderAddr::GLOBAL_CHECKSUM + 1]);
}

// ═════════════════════════════════════════════════════════════════════
//  Validation
// ═════════════════════════════════════════════════════════════════════

bool Cartridge::validateLogo() const {
    return std::equal(logo.begin(), logo.end(), NINTENDO_LOGO.begin());
}

bool Cartridge::validateHeaderChecksum() const {
    uint8_t checksum = 0;
    for (uint16_t addr = 0x0134; addr <= 0x014C; ++addr) {
        checksum = checksum - rom[addr] - 1;
    }
    return checksum == headerChecksum;
}

// ═════════════════════════════════════════════════════════════════════
//  Pretty-print
// ═════════════════════════════════════════════════════════════════════

void Cartridge::printHeader() const {
    std::printf("\n");
    std::printf("╔══════════════════════════════════════════════════╗\n");
    std::printf("║            GAME BOY CARTRIDGE HEADER             ║\n");
    std::printf("╠══════════════════════════════════════════════════╣\n");
    std::printf("║  File: %-40s  ║\n", filepath.c_str());
    std::printf("║  Size: %-40zu  ║\n", rom.size());
    std::printf("╠══════════════════════════════════════════════════╣\n");

    // Entry point
    std::printf("║  Entry Point:     $%02X $%02X $%02X $%02X               ║\n",
                entryPoint[0], entryPoint[1], entryPoint[2], entryPoint[3]);

    // Nintendo logo validation
    bool logoValid = validateLogo();
    std::printf("║  Nintendo Logo:   %-30s  ║\n",
                logoValid ? "✓ VALID" : "✗ INVALID");

    // Title
    std::printf("║  Title:           %-30s  ║\n", title.c_str());

    // Manufacturer code
    if (!manufacturerCode.empty()) {
        std::printf("║  Manufacturer:    %-30s  ║\n", manufacturerCode.c_str());
    }

    // CGB flag
    std::printf("║  CGB Flag:        $%02X %-26s  ║\n",
                cgbFlag, cgbFlagString(cgbFlag));

    // SGB flag
    std::printf("║  SGB Flag:        $%02X %-26s  ║\n",
                sgbFlag, sgbFlag == 0x03 ? "(SGB supported)" : "(No SGB)");

    // Licensee
    if (oldLicenseeCode == 0x33) {
        std::printf("║  Licensee (new):  \"%s\" %-25s  ║\n",
                    newLicenseeCode.c_str(), newLicenseeName(newLicenseeCode));
    } else {
        std::printf("║  Licensee (old):  $%02X %-26s  ║\n",
                    oldLicenseeCode, oldLicenseeName(oldLicenseeCode));
    }

    // Cartridge type
    std::printf("║  Cartridge Type:  $%02X %-26s  ║\n",
                cartridgeType, cartridgeTypeName(cartridgeType));

    // ROM size
    std::printf("║  ROM Size:        $%02X %-26s  ║\n",
                romSizeCode, romSizeString(romSizeCode));

    // RAM size
    std::printf("║  RAM Size:        $%02X %-26s  ║\n",
                ramSizeCode, ramSizeString(ramSizeCode));

    // Destination
    std::printf("║  Destination:     $%02X %-26s  ║\n",
                destinationCode, destinationString(destinationCode));

    // ROM version
    std::printf("║  ROM Version:     $%02X                            ║\n", romVersion);

    // Header checksum
    bool checksumValid = validateHeaderChecksum();
    std::printf("║  Header Checksum: $%02X %-26s  ║\n",
                headerChecksum, checksumValid ? "✓ VALID" : "✗ INVALID");

    // Global checksum
    std::printf("║  Global Checksum: $%04X                          ║\n", globalChecksum);

    std::printf("╚══════════════════════════════════════════════════╝\n");

    // Logo hex dump
    std::printf("\n  Nintendo Logo Hex Dump ($0104-$0133):\n  ");
    for (size_t i = 0; i < logo.size(); ++i) {
        std::printf("%02X ", logo[i]);
        if ((i + 1) % 16 == 0 && i + 1 < logo.size())
            std::printf("\n  ");
    }
    std::printf("\n\n");
}

// ═════════════════════════════════════════════════════════════════════
//  MBC delegation
// ═════════════════════════════════════════════════════════════════════

uint8_t Cartridge::read(uint16_t addr) const {
    if (mbc) return mbc->read(addr);
    // Fallback (should not happen)
    if (addr < rom.size()) return rom[addr];
    return 0xFF;
}

void Cartridge::write(uint16_t addr, uint8_t val) {
    if (mbc) {
        mbc->write(addr, val);
        // Track SRAM writes for dirty-flag battery save
        if (addr >= 0xA000 && addr <= 0xBFFF) {
            sramDirty_ = true;
            sramIdleFrames_ = 0; // Reset idle counter on every write
        }
    }
}

// ═════════════════════════════════════════════════════════════════════
//  Save state serialization
// ═════════════════════════════════════════════════════════════════════

#include "save_state.h"

void Cartridge::serialize(SaveState& ss) const {
    // External RAM contents
    ss.write<uint32_t>(static_cast<uint32_t>(externalRam.size()));
    if (!externalRam.empty()) {
        ss.writeBytes(externalRam.data(), externalRam.size());
    }
    // MBC banking state
    if (mbc) mbc->serialize(ss);
}

void Cartridge::deserialize(SaveState& ss) {
    // External RAM contents
    uint32_t ramSize = ss.read<uint32_t>();
    if (ramSize == externalRam.size() && ramSize > 0) {
        ss.readBytes(externalRam.data(), ramSize);
    } else if (ramSize > 0) {
        // Size mismatch — skip the data
        for (uint32_t i = 0; i < ramSize; i++) ss.read<uint8_t>();
    }
    // MBC banking state
    if (mbc) mbc->deserialize(ss);
}

// ═════════════════════════════════════════════════════════════════════
//  Battery save (SRAM persistence)
// ═════════════════════════════════════════════════════════════════════

bool Cartridge::hasBattery() const {
    switch (cartridgeType) {
        case 0x03: // MBC1+RAM+BATTERY
        case 0x06: // MBC2+BATTERY
        case 0x09: // ROM+RAM+BATTERY
        case 0x0D: // MMM01+RAM+BATTERY
        case 0x0F: // MBC3+TIMER+BATTERY
        case 0x10: // MBC3+TIMER+RAM+BATTERY
        case 0x13: // MBC3+RAM+BATTERY
        case 0x1B: // MBC5+RAM+BATTERY
        case 0x1E: // MBC5+RUMBLE+RAM+BATTERY
        case 0xFF: // HuC1+RAM+BATTERY
            return true;
        default:
            return false;
    }
}

std::string Cartridge::savFilePath() const {
    if (filepath.empty()) return "";
    // Replace extension with .sav
    size_t dot = filepath.rfind('.');
    if (dot != std::string::npos) {
        return filepath.substr(0, dot) + ".sav";
    }
    return filepath + ".sav";
}

void Cartridge::loadBatterySave() {
    if (!hasBattery() || externalRam.empty()) return;

    std::string path = savFilePath();
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return; // No save file yet — that's fine

    // Get file size
    std::fseek(f, 0, SEEK_END);
    long fileSize = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    if (fileSize <= 0) {
        std::fclose(f);
        return;
    }

    // Read up to the external RAM size
    size_t toRead = std::min(static_cast<size_t>(fileSize), externalRam.size());
    std::fread(externalRam.data(), 1, toRead, f);
    std::fclose(f);

    std::printf("Battery save loaded: %s (%zu bytes)\n", path.c_str(), toRead);
}

void Cartridge::writeBatterySave() {
    if (!hasBattery() || externalRam.empty()) return;

    std::string path = savFilePath();
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        std::fprintf(stderr, "Failed to write battery save: %s\n", path.c_str());
        return;
    }

    std::fwrite(externalRam.data(), 1, externalRam.size(), f);
    std::fclose(f);

    sramDirty_ = false;
    sramIdleFrames_ = 0;

    std::printf("Battery save written: %s (%zu bytes)\n", path.c_str(), externalRam.size());
}

void Cartridge::tickBatterySave() {
    if (!sramDirty_ || !hasBattery()) return;

    sramIdleFrames_++;
    if (sramIdleFrames_ >= FLUSH_DELAY) {
        writeBatterySave();
    }
}

// ═════════════════════════════════════════════════════════════════════
//  Save state file path helper
// ═════════════════════════════════════════════════════════════════════

std::string Cartridge::saveStatePath(int slot) const {
    if (filepath.empty()) return "";
    size_t dot = filepath.rfind('.');
    std::string base = (dot != std::string::npos) ? filepath.substr(0, dot) : filepath;
    return base + ".ss" + std::to_string(slot);
}
