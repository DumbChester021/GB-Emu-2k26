#include "save_state.h"

#include <cstdio>
#include <cstring>

// ══════════════════════════════════════════════════════════════════════
// CRC-32 (IEEE 802.3) — table-driven, computed once at startup
// ══════════════════════════════════════════════════════════════════════

static uint32_t crc32Table[256];
static bool crc32TableBuilt = false;

static void buildCRC32Table() {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320u : 0);
        }
        crc32Table[i] = crc;
    }
    crc32TableBuilt = true;
}

uint32_t SaveState::crc32(const uint8_t* data, size_t len) {
    if (!crc32TableBuilt) buildCRC32Table();

    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc32Table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFFu;
}

// ══════════════════════════════════════════════════════════════════════
// File I/O
// ══════════════════════════════════════════════════════════════════════

bool SaveState::saveToFile(const std::string& path) const {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;

    // Header: magic(4) + version(4) + crc(4) + payload_len(4) = 16 bytes
    uint32_t magic = MAGIC;
    uint32_t version = VERSION;
    uint32_t payloadLen = static_cast<uint32_t>(data_.size());
    uint32_t crc = crc32(data_.data(), data_.size());

    std::fwrite(&magic, 4, 1, f);
    std::fwrite(&version, 4, 1, f);
    std::fwrite(&crc, 4, 1, f);
    std::fwrite(&payloadLen, 4, 1, f);
    std::fwrite(data_.data(), 1, data_.size(), f);
    std::fclose(f);
    return true;
}

bool SaveState::loadFromFile(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    // Read header
    uint32_t magic, version, fileCrc, payloadLen;
    if (std::fread(&magic, 4, 1, f) != 1 ||
        std::fread(&version, 4, 1, f) != 1 ||
        std::fread(&fileCrc, 4, 1, f) != 1 ||
        std::fread(&payloadLen, 4, 1, f) != 1) {
        std::fclose(f);
        return false;
    }

    if (magic != MAGIC) {
        std::fprintf(stderr, "Save state: invalid magic (expected GBSS)\n");
        std::fclose(f);
        return false;
    }

    if (version != VERSION) {
        std::fprintf(stderr, "Save state: version mismatch (file=%u, expected=%u)\n",
                     version, VERSION);
        std::fclose(f);
        return false;
    }

    // Read payload
    data_.resize(payloadLen);
    if (std::fread(data_.data(), 1, payloadLen, f) != payloadLen) {
        std::fclose(f);
        data_.clear();
        return false;
    }
    std::fclose(f);

    // Verify CRC
    uint32_t computedCrc = crc32(data_.data(), data_.size());
    if (computedCrc != fileCrc) {
        std::fprintf(stderr, "Save state: CRC mismatch (file corrupt)\n");
        data_.clear();
        return false;
    }

    pos_ = 0;
    error_ = false;
    return true;
}
