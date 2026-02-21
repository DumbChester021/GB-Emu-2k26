#include "memory_bus.h"
#include "cartridge.h"
#include "save_state.h"

// ══════════════════════════════════════════════════════════════════════
// MemoryBus save state — delegates to subsystems, saves own RAM/IO
// ══════════════════════════════════════════════════════════════════════

void MemoryBus::serialize(SaveState& ss) const {
    // Bootrom state
    ss.writeBool(bootromActive_);

    // Subsystems (order must match deserialize)
    timer_.serialize(ss);
    ppu_.serialize(ss);
    joypad_.serialize(ss);
    apu_.serialize(ss);

    // Work RAM (8 KB)
    ss.writeBytes(wram_.data(), wram_.size());

    // High RAM (127 bytes)
    ss.writeBytes(hram_.data(), hram_.size());

    // IO registers (FF00–FF7F)
    ss.writeBytes(io_.data(), io_.size());

    // IE register
    ss.write<uint8_t>(ie_);

    // Serial
    ss.writeBool(serialReady_);
    ss.writeBool(serialMasterClock_);
    ss.write<uint8_t>(serialCount_);

    // OAM DMA state
    ss.writeBool(dmaActive_);
    ss.write<uint16_t>(dmaSrc_);
    ss.write<int32_t>(dmaByte_);
    ss.write<int32_t>(dmaClock_);
    ss.write<int32_t>(dmaDelay_);
    ss.writeBool(dmaRestarting_);
    ss.write<uint8_t>(dmaLastByte_);
}

void MemoryBus::deserialize(SaveState& ss) {
    // Bootrom state
    bootromActive_ = ss.readBool();

    // Subsystems (order must match serialize)
    timer_.deserialize(ss);
    ppu_.deserialize(ss);
    joypad_.deserialize(ss);
    apu_.deserialize(ss);

    // Work RAM (8 KB)
    ss.readBytes(wram_.data(), wram_.size());

    // High RAM (127 bytes)
    ss.readBytes(hram_.data(), hram_.size());

    // IO registers (FF00–FF7F)
    ss.readBytes(io_.data(), io_.size());

    // IE register
    ie_ = ss.read<uint8_t>();

    // Serial
    serialReady_ = ss.readBool();
    serialMasterClock_ = ss.readBool();
    serialCount_ = ss.read<uint8_t>();

    // OAM DMA state
    dmaActive_ = ss.readBool();
    dmaSrc_ = ss.read<uint16_t>();
    dmaByte_ = ss.read<int32_t>();
    dmaClock_ = ss.read<int32_t>();
    dmaDelay_ = ss.read<int32_t>();
    dmaRestarting_ = ss.readBool();
    dmaLastByte_ = ss.read<uint8_t>();
}
