#include "cpu.h"
#include "save_state.h"

// ══════════════════════════════════════════════════════════════════════
// CPU save state — registers + internal state machine
// ══════════════════════════════════════════════════════════════════════

void CPU::serialize(SaveState& ss) const {
    // Registers
    ss.write<uint16_t>(reg.af);
    ss.write<uint16_t>(reg.bc);
    ss.write<uint16_t>(reg.de);
    ss.write<uint16_t>(reg.hl);
    ss.write<uint16_t>(reg.sp);
    ss.write<uint16_t>(reg.pc);

    // Cycle counter
    ss.write<uint64_t>(totalCycles_);

    // Opcode state
    ss.write<uint8_t>(currentOpcode_);
    ss.writeBool(cbPrefix_);

    // CPU status
    ss.writeBool(halted_);
    ss.writeBool(stopped_);
    ss.writeBool(unimplemented_);

    // Interrupt state
    ss.writeBool(ime_);
    ss.writeBool(imeScheduled_);
    ss.writeBool(haltBug_);
}

void CPU::deserialize(SaveState& ss) {
    // Registers
    reg.af = ss.read<uint16_t>();
    reg.bc = ss.read<uint16_t>();
    reg.de = ss.read<uint16_t>();
    reg.hl = ss.read<uint16_t>();
    reg.sp = ss.read<uint16_t>();
    reg.pc = ss.read<uint16_t>();

    // Cycle counter
    totalCycles_ = ss.read<uint64_t>();

    // Opcode state
    currentOpcode_ = ss.read<uint8_t>();
    cbPrefix_ = ss.readBool();

    // CPU status
    halted_ = ss.readBool();
    stopped_ = ss.readBool();
    unimplemented_ = ss.readBool();

    // Interrupt state
    ime_ = ss.readBool();
    imeScheduled_ = ss.readBool();
    haltBug_ = ss.readBool();
}
