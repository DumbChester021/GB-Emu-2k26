#include "joypad.h"
#include "save_state.h"

void Joypad::serialize(SaveState& ss) const {
    ss.write<uint8_t>(select_);
    ss.write<uint8_t>(dirButtons_);
    ss.write<uint8_t>(actionButtons_);
}

void Joypad::deserialize(SaveState& ss) {
    select_ = ss.read<uint8_t>();
    dirButtons_ = ss.read<uint8_t>();
    actionButtons_ = ss.read<uint8_t>();
}
