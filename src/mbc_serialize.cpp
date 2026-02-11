#include "mbc.h"
#include "save_state.h"

// ═══════════════════════════════════════════════════════════════════════
// MBC1 serialization
// ═══════════════════════════════════════════════════════════════════════

void MBC1Controller::serialize(SaveState& ss) const {
    ss.writeBool(ramEnabled_);
    ss.write<int32_t>(bank1_);
    ss.write<int32_t>(bank2_);
    ss.write<int32_t>(mode_);
}

void MBC1Controller::deserialize(SaveState& ss) {
    ramEnabled_ = ss.readBool();
    bank1_ = ss.read<int32_t>();
    bank2_ = ss.read<int32_t>();
    mode_  = ss.read<int32_t>();
}

// ═══════════════════════════════════════════════════════════════════════
// MBC2 serialization
// ═══════════════════════════════════════════════════════════════════════

void MBC2Controller::serialize(SaveState& ss) const {
    ss.writeBool(ramEnabled_);
    ss.write<int32_t>(romBank_);
}

void MBC2Controller::deserialize(SaveState& ss) {
    ramEnabled_ = ss.readBool();
    romBank_ = ss.read<int32_t>();
}

// ═══════════════════════════════════════════════════════════════════════
// MBC3 serialization
// ═══════════════════════════════════════════════════════════════════════

void MBC3Controller::serialize(SaveState& ss) const {
    ss.writeBool(ramRTCEnabled_);
    ss.write<int32_t>(romBank_);
    ss.write<int32_t>(ramBank_);
}

void MBC3Controller::deserialize(SaveState& ss) {
    ramRTCEnabled_ = ss.readBool();
    romBank_ = ss.read<int32_t>();
    ramBank_ = ss.read<int32_t>();
}

// ═══════════════════════════════════════════════════════════════════════
// MBC5 serialization
// ═══════════════════════════════════════════════════════════════════════

void MBC5Controller::serialize(SaveState& ss) const {
    ss.writeBool(ramEnabled_);
    ss.write<int32_t>(romBank_);
    ss.write<int32_t>(ramBank_);
}

void MBC5Controller::deserialize(SaveState& ss) {
    ramEnabled_ = ss.readBool();
    romBank_ = ss.read<int32_t>();
    ramBank_ = ss.read<int32_t>();
}
