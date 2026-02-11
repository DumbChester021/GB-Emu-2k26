#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// ══════════════════════════════════════════════════════════════════════
// SaveState — Efficient binary serialization buffer for save states
//
// Usage:
//   SaveState ss;
//   component.serialize(ss);
//   ss.saveToFile("game.ss1");
//
//   SaveState ss2;
//   ss2.loadFromFile("game.ss1");
//   component.deserialize(ss2);
// ══════════════════════════════════════════════════════════════════════

class SaveState {
public:
    // ── Magic / version ─────────────────────────────────────────────
    static constexpr uint32_t MAGIC   = 0x53534247; // "GBSS" in LE
    static constexpr uint32_t VERSION = 1;

    // ── Write primitives ────────────────────────────────────────────
    template<typename T>
    void write(T val) {
        static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
        const auto* p = reinterpret_cast<const uint8_t*>(&val);
        data_.insert(data_.end(), p, p + sizeof(T));
    }

    void writeBytes(const void* ptr, size_t len) {
        const auto* p = reinterpret_cast<const uint8_t*>(ptr);
        data_.insert(data_.end(), p, p + len);
    }

    void writeBool(bool val) { write<uint8_t>(val ? 1 : 0); }

    // ── Read primitives ─────────────────────────────────────────────
    template<typename T>
    T read() {
        static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
        if (pos_ + sizeof(T) > data_.size()) {
            error_ = true;
            return T{};
        }
        T val;
        std::memcpy(&val, data_.data() + pos_, sizeof(T));
        pos_ += sizeof(T);
        return val;
    }

    void readBytes(void* ptr, size_t len) {
        if (pos_ + len > data_.size()) {
            error_ = true;
            return;
        }
        std::memcpy(ptr, data_.data() + pos_, len);
        pos_ += len;
    }

    bool readBool() { return read<uint8_t>() != 0; }

    // ── Error state ─────────────────────────────────────────────────
    bool hasError() const { return error_; }

    // ── File I/O ────────────────────────────────────────────────────
    bool saveToFile(const std::string& path) const;
    bool loadFromFile(const std::string& path);

    // ── Reset read position ─────────────────────────────────────────
    void resetRead() { pos_ = 0; error_ = false; }

    // ── Buffer info ─────────────────────────────────────────────────
    size_t size() const { return data_.size(); }

private:
    std::vector<uint8_t> data_;
    size_t pos_ = 0;
    bool error_ = false;

    // ── CRC-32 (IEEE 802.3) ─────────────────────────────────────────
    static uint32_t crc32(const uint8_t* data, size_t len);
};
