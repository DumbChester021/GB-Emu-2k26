#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "mbc.h"

class SaveState;

// ── Nintendo logo expected at $0104–$0133 ────────────────────────────
static constexpr std::array<uint8_t, 48> NINTENDO_LOGO = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
    0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
    0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
    0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
    0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
    0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
};

// ── Header address constants ─────────────────────────────────────────
namespace HeaderAddr {
    constexpr uint16_t ENTRY_POINT     = 0x0100;
    constexpr uint16_t LOGO            = 0x0104;
    constexpr uint16_t TITLE           = 0x0134;
    constexpr uint16_t MANUFACTURER    = 0x013F;
    constexpr uint16_t CGB_FLAG        = 0x0143;
    constexpr uint16_t NEW_LICENSEE    = 0x0144;
    constexpr uint16_t SGB_FLAG        = 0x0146;
    constexpr uint16_t CARTRIDGE_TYPE  = 0x0147;
    constexpr uint16_t ROM_SIZE        = 0x0148;
    constexpr uint16_t RAM_SIZE        = 0x0149;
    constexpr uint16_t DEST_CODE       = 0x014A;
    constexpr uint16_t OLD_LICENSEE    = 0x014B;
    constexpr uint16_t ROM_VERSION     = 0x014C;
    constexpr uint16_t HEADER_CHECKSUM = 0x014D;
    constexpr uint16_t GLOBAL_CHECKSUM = 0x014E;
    constexpr uint16_t HEADER_END      = 0x014F;
}

class Cartridge {
public:
    // ── Move semantics (rebind MBC storage after move) ──────────────
    Cartridge() = default;
    Cartridge(Cartridge&& other) noexcept { moveFrom(std::move(other)); }
    Cartridge& operator=(Cartridge&& other) noexcept {
        if (this != &other) moveFrom(std::move(other));
        return *this;
    }
    Cartridge(const Cartridge&) = delete;
    Cartridge& operator=(const Cartridge&) = delete;

    // ── Parsed header fields ─────────────────────────────────────────
    std::array<uint8_t, 4>  entryPoint{};
    std::array<uint8_t, 48> logo{};
    std::string             title;
    std::string             manufacturerCode;
    uint8_t                 cgbFlag        = 0;
    std::string             newLicenseeCode;
    uint8_t                 sgbFlag        = 0;
    uint8_t                 cartridgeType  = 0;
    uint8_t                 romSizeCode    = 0;
    uint8_t                 ramSizeCode    = 0;
    uint8_t                 destinationCode = 0;
    uint8_t                 oldLicenseeCode = 0;
    uint8_t                 romVersion     = 0;
    uint8_t                 headerChecksum = 0;
    uint16_t                globalChecksum = 0;

    // Raw ROM data
    std::vector<uint8_t>    rom;
    std::vector<uint8_t>    externalRam;
    std::unique_ptr<MBC>    mbc;
    std::string             filepath;

    // ── MBC read/write delegation ────────────────────────────────────
    uint8_t read(uint16_t addr) const;
    void    write(uint16_t addr, uint8_t val);

    // ── Static factory ───────────────────────────────────────────────
    static std::optional<Cartridge> loadFromFile(const std::string& path);

    // ── Validation ───────────────────────────────────────────────────
    bool validateLogo() const;
    bool validateHeaderChecksum() const;

    // ── Pretty-print ─────────────────────────────────────────────────
    void printHeader() const;

    // ── Lookup helpers ───────────────────────────────────────────────
    static const char* cartridgeTypeName(uint8_t code);
    static const char* romSizeString(uint8_t code);
    static const char* ramSizeString(uint8_t code);
    static const char* newLicenseeName(const std::string& code);
    static const char* oldLicenseeName(uint8_t code);
    static const char* cgbFlagString(uint8_t flag);
    static const char* destinationString(uint8_t code);

    // ── Save state serialization ─────────────────────────────────────────
    void serialize(SaveState& ss) const;
    void deserialize(SaveState& ss);

    // ── Battery save (SRAM persistence) ──────────────────────────────
    bool hasBattery() const;
    std::string savFilePath() const;
    void loadBatterySave();
    void writeBatterySave();

    // Dirty-flag based flush (SameBoy-style)
    // Call once per frame. Flushes to disk only when SRAM was written
    // to and has been idle for FLUSH_DELAY frames (~2 seconds).
    void tickBatterySave();
    bool isSramDirty() const { return sramDirty_; }

    // ── Save state file path helper ──────────────────────────────────
    std::string saveStatePath(int slot) const;

private:
    void parseHeader();

    // ── Battery save dirty tracking ─────────────────────────────────
    bool sramDirty_      = false;
    int  sramIdleFrames_ = 0;
    static constexpr int FLUSH_DELAY = 120; // ~2 seconds at 60fps

    void moveFrom(Cartridge&& other) noexcept {
        entryPoint = other.entryPoint;
        logo = other.logo;
        title = std::move(other.title);
        manufacturerCode = std::move(other.manufacturerCode);
        cgbFlag = other.cgbFlag;
        newLicenseeCode = std::move(other.newLicenseeCode);
        sgbFlag = other.sgbFlag;
        cartridgeType = other.cartridgeType;
        romSizeCode = other.romSizeCode;
        ramSizeCode = other.ramSizeCode;
        destinationCode = other.destinationCode;
        oldLicenseeCode = other.oldLicenseeCode;
        romVersion = other.romVersion;
        headerChecksum = other.headerChecksum;
        globalChecksum = other.globalChecksum;
        rom = std::move(other.rom);
        externalRam = std::move(other.externalRam);
        mbc = std::move(other.mbc);
        filepath = std::move(other.filepath);
        sramDirty_ = other.sramDirty_;
        sramIdleFrames_ = other.sramIdleFrames_;
        // Rebind MBC to point at OUR vectors (not the moved-from ones)
        if (mbc) mbc->rebindStorage(rom, externalRam);
    }
};
