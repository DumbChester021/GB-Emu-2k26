#include "cpu.h"
#include "memory_bus.h"
#include <cstdio>
#include <cstring>

// ══════════════════════════════════════════════════════════════════════
// Construction & table init
// ══════════════════════════════════════════════════════════════════════

CPU::CPU(MemoryBus& bus) : bus_(bus) {
    std::memset(unprefixed_, 0, sizeof(unprefixed_));
    std::memset(cbprefixed_, 0, sizeof(cbprefixed_));
    initTables();

    if (bus_.bootromActive()) {
        // Bootrom will set up registers — start from zero
        reg.af = 0x0000;
        reg.bc = 0x0000;
        reg.de = 0x0000;
        reg.hl = 0x0000;
        reg.sp = 0x0000;
        reg.pc = 0x0000;
    } else {
        // Post-boot register state (DMG-ABC)
        reg.af = 0x01B0;
        reg.bc = 0x0013;
        reg.de = 0x00D8;
        reg.hl = 0x014D;
        reg.sp = 0xFFFE;
        reg.pc = 0x0100; // Skip boot ROM
    }
}

// ══════════════════════════════════════════════════════════════════════
// Timing helpers
// ══════════════════════════════════════════════════════════════════════

void CPU::tick4() {
    for (int i = 0; i < 4; i++) {
        bus_.tick();
        totalCycles_++;
    }
}

void CPU::internalCycle() {
    tick4();
}

// ══════════════════════════════════════════════════════════════════════
// Tick — advance CPU by one instruction (returns true if still running)
// ══════════════════════════════════════════════════════════════════════

bool CPU::tick() {
    if (unimplemented_) return false;

    // Check interrupts at the start of each instruction cycle
    handleInterrupts();
    if (halted_) {
        tick4();
        return true;
    }

    // Handle pending EI: delayed by one instruction.
    // Applied AFTER interrupt check but BEFORE fetch, so the instruction
    // after EI runs fully before any interrupt can fire.
    if (imeScheduled_) {
        ime_ = true;
        imeScheduled_ = false;
    }

    // Fetch & execute
    currentOpcode_ = fetchByte();  // 4 T-cycles consumed here

    // HALT bug: don't increment PC for the next fetch
    if (haltBug_) {
        reg.pc--;
        haltBug_ = false;
    }

    executeOpcode();

    return !unimplemented_;
}

// ══════════════════════════════════════════════════════════════════════
// Opcode dispatch
// ══════════════════════════════════════════════════════════════════════

void CPU::executeOpcode() {
    if (currentOpcode_ == 0xCB) {
        currentOpcode_ = fetchByte();  // 4 T-cycles for CB operand fetch
        cbPrefix_ = true;
        executeCBOpcode();
        cbPrefix_ = false;
        return;
    }

    OpHandler handler = unprefixed_[currentOpcode_];
    if (!handler) {
        std::fprintf(stderr,
            "\n╔══════════════════════════════════════════╗\n"
            "║  UNIMPLEMENTED OPCODE: 0x%02X             ║\n"
            "║  PC = 0x%04X                             ║\n"
            "║  Total cycles: %llu                       ║\n"
            "╚══════════════════════════════════════════╝\n",
            currentOpcode_, reg.pc - 1, (unsigned long long)totalCycles_);
        unimplemented_ = true;
        return;
    }
    (this->*handler)();
}

void CPU::executeCBOpcode() {
    OpHandler handler = cbprefixed_[currentOpcode_];
    if (!handler) {
        std::fprintf(stderr,
            "\n╔══════════════════════════════════════════╗\n"
            "║  UNIMPLEMENTED CB OPCODE: 0xCB 0x%02X     ║\n"
            "║  PC = 0x%04X                             ║\n"
            "║  Total cycles: %llu                       ║\n"
            "╚══════════════════════════════════════════╝\n",
            currentOpcode_, reg.pc - 2, (unsigned long long)totalCycles_);
        unimplemented_ = true;
        return;
    }
    (this->*handler)();
}

// ══════════════════════════════════════════════════════════════════════
// Memory helpers — each ticks the bus for 4 T-cycles
// ══════════════════════════════════════════════════════════════════════

uint8_t CPU::readByte(uint16_t addr) {
    uint8_t val = bus_.read(addr);
    tick4();
    return val;
}

void CPU::writeByte(uint16_t addr, uint8_t val) {
    // LCDC is wired directly into the DMG LCD pipeline. During the conflict
    // dot, most bits retain their old state; a newly asserted BG-enable bit is
    // already visible. OBJ/window have additional fetcher-side behavior.
    if (addr == 0xFF40) {
        bus_.write(addr, bus_.ppu().beginDMGLCDCWrite(val));
        bus_.tick();
        totalCycles_++;
        bus_.write(addr, val);
        for (int i = 0; i < 3; ++i) {
            bus_.tick();
            totalCycles_++;
        }
        return;
    }

    // DMG palette registers drive the LCD bus directly. For the first dot of
    // a write, old and new bits are both visible; the requested value wins on
    // the following dot. The M-cycle remains exactly four T-cycles long.
    if (addr >= 0xFF47 && addr <= 0xFF49) {
        uint8_t old = bus_.read(addr);
        bus_.write(addr, static_cast<uint8_t>(old | val));
        bus_.tick();
        totalCycles_++;
        bus_.write(addr, val);
        for (int i = 0; i < 3; ++i) {
            bus_.tick();
            totalCycles_++;
        }
        return;
    }
    bus_.write(addr, val);
    tick4();
}

uint8_t CPU::fetchByte() {
    uint8_t val = readByte(reg.pc);
    reg.pc++;
    return val;
}

uint16_t CPU::fetchWord() {
    uint8_t lo = fetchByte();
    uint8_t hi = fetchByte();
    return (uint16_t(hi) << 8) | lo;
}

// ══════════════════════════════════════════════════════════════════════
// Flag helpers
// ══════════════════════════════════════════════════════════════════════

void CPU::setFlag(uint8_t flag, bool val) {
    if (val) reg.f |= flag;
    else     reg.f &= ~flag;
    reg.f &= 0xF0; // lower nibble always 0
}

bool CPU::getFlag(uint8_t flag) const {
    return (reg.f & flag) != 0;
}

// ══════════════════════════════════════════════════════════════════════
// Stack helpers
// ══════════════════════════════════════════════════════════════════════

void CPU::pushWord(uint16_t val) {
    reg.sp--;
    writeByte(reg.sp, (val >> 8) & 0xFF);
    reg.sp--;
    writeByte(reg.sp, val & 0xFF);
}

uint16_t CPU::popWord() {
    uint8_t lo = readByte(reg.sp); reg.sp++;
    uint8_t hi = readByte(reg.sp); reg.sp++;
    return (uint16_t(hi) << 8) | lo;
}

// ══════════════════════════════════════════════════════════════════════
// ALU helpers
// ══════════════════════════════════════════════════════════════════════

void CPU::alu_add(uint8_t val) {
    uint16_t result = reg.a + val;
    setFlag(Flag::Z, (result & 0xFF) == 0);
    setFlag(Flag::N, false);
    setFlag(Flag::H, ((reg.a & 0x0F) + (val & 0x0F)) > 0x0F);
    setFlag(Flag::C, result > 0xFF);
    reg.a = result & 0xFF;
}

void CPU::alu_adc(uint8_t val) {
    uint8_t carry = getFlag(Flag::C) ? 1 : 0;
    uint16_t result = reg.a + val + carry;
    setFlag(Flag::Z, (result & 0xFF) == 0);
    setFlag(Flag::N, false);
    setFlag(Flag::H, ((reg.a & 0x0F) + (val & 0x0F) + carry) > 0x0F);
    setFlag(Flag::C, result > 0xFF);
    reg.a = result & 0xFF;
}

void CPU::alu_sub(uint8_t val) {
    int result = reg.a - val;
    setFlag(Flag::Z, (result & 0xFF) == 0);
    setFlag(Flag::N, true);
    setFlag(Flag::H, (int)(reg.a & 0x0F) - (int)(val & 0x0F) < 0);
    setFlag(Flag::C, result < 0);
    reg.a = result & 0xFF;
}

void CPU::alu_sbc(uint8_t val) {
    uint8_t carry = getFlag(Flag::C) ? 1 : 0;
    int result = reg.a - val - carry;
    setFlag(Flag::Z, (result & 0xFF) == 0);
    setFlag(Flag::N, true);
    setFlag(Flag::H, (int)(reg.a & 0x0F) - (int)(val & 0x0F) - carry < 0);
    setFlag(Flag::C, result < 0);
    reg.a = result & 0xFF;
}

void CPU::alu_and(uint8_t val) {
    reg.a &= val;
    setFlag(Flag::Z, reg.a == 0);
    setFlag(Flag::N, false);
    setFlag(Flag::H, true);
    setFlag(Flag::C, false);
}

void CPU::alu_xor(uint8_t val) {
    reg.a ^= val;
    setFlag(Flag::Z, reg.a == 0);
    setFlag(Flag::N, false);
    setFlag(Flag::H, false);
    setFlag(Flag::C, false);
}

void CPU::alu_or(uint8_t val) {
    reg.a |= val;
    setFlag(Flag::Z, reg.a == 0);
    setFlag(Flag::N, false);
    setFlag(Flag::H, false);
    setFlag(Flag::C, false);
}

void CPU::alu_cp(uint8_t val) {
    int result = reg.a - val;
    setFlag(Flag::Z, (result & 0xFF) == 0);
    setFlag(Flag::N, true);
    setFlag(Flag::H, (int)(reg.a & 0x0F) - (int)(val & 0x0F) < 0);
    setFlag(Flag::C, result < 0);
}

void CPU::alu_inc(uint8_t& r) {
    setFlag(Flag::H, (r & 0x0F) == 0x0F);
    r++;
    setFlag(Flag::Z, r == 0);
    setFlag(Flag::N, false);
}

void CPU::alu_dec(uint8_t& r) {
    setFlag(Flag::H, (r & 0x0F) == 0x00);
    r--;
    setFlag(Flag::Z, r == 0);
    setFlag(Flag::N, true);
}

// ══════════════════════════════════════════════════════════════════════
// CB helper: get/set register by index
// ══════════════════════════════════════════════════════════════════════

uint8_t CPU::getCBReg(int idx) {
    switch (idx) {
        case 0: return reg.b; case 1: return reg.c;
        case 2: return reg.d; case 3: return reg.e;
        case 4: return reg.h; case 5: return reg.l;
        case 6: return readByte(reg.hl);
        case 7: return reg.a;
    }
    return 0;
}

void CPU::setCBReg(int idx, uint8_t val) {
    switch (idx) {
        case 0: reg.b = val; return; case 1: reg.c = val; return;
        case 2: reg.d = val; return; case 3: reg.e = val; return;
        case 4: reg.h = val; return; case 5: reg.l = val; return;
        case 6: writeByte(reg.hl, val); return;
        case 7: reg.a = val; return;
    }
}

// ══════════════════════════════════════════════════════════════════════
// CB ALU operations
// ══════════════════════════════════════════════════════════════════════

uint8_t CPU::cb_rlc(uint8_t val) {
    uint8_t carry = (val >> 7) & 1;
    val = (val << 1) | carry;
    setFlag(Flag::Z, val == 0); setFlag(Flag::N, false);
    setFlag(Flag::H, false); setFlag(Flag::C, carry);
    return val;
}

uint8_t CPU::cb_rrc(uint8_t val) {
    uint8_t carry = val & 1;
    val = (val >> 1) | (carry << 7);
    setFlag(Flag::Z, val == 0); setFlag(Flag::N, false);
    setFlag(Flag::H, false); setFlag(Flag::C, carry);
    return val;
}

uint8_t CPU::cb_rl(uint8_t val) {
    uint8_t oldCarry = getFlag(Flag::C) ? 1 : 0;
    uint8_t newCarry = (val >> 7) & 1;
    val = (val << 1) | oldCarry;
    setFlag(Flag::Z, val == 0); setFlag(Flag::N, false);
    setFlag(Flag::H, false); setFlag(Flag::C, newCarry);
    return val;
}

uint8_t CPU::cb_rr(uint8_t val) {
    uint8_t oldCarry = getFlag(Flag::C) ? 0x80 : 0;
    uint8_t newCarry = val & 1;
    val = (val >> 1) | oldCarry;
    setFlag(Flag::Z, val == 0); setFlag(Flag::N, false);
    setFlag(Flag::H, false); setFlag(Flag::C, newCarry);
    return val;
}

uint8_t CPU::cb_sla(uint8_t val) {
    uint8_t carry = (val >> 7) & 1;
    val <<= 1;
    setFlag(Flag::Z, val == 0); setFlag(Flag::N, false);
    setFlag(Flag::H, false); setFlag(Flag::C, carry);
    return val;
}

uint8_t CPU::cb_sra(uint8_t val) {
    uint8_t carry = val & 1;
    val = (val & 0x80) | (val >> 1); // preserve bit 7
    setFlag(Flag::Z, val == 0); setFlag(Flag::N, false);
    setFlag(Flag::H, false); setFlag(Flag::C, carry);
    return val;
}

uint8_t CPU::cb_swap(uint8_t val) {
    val = ((val & 0x0F) << 4) | ((val & 0xF0) >> 4);
    setFlag(Flag::Z, val == 0); setFlag(Flag::N, false);
    setFlag(Flag::H, false); setFlag(Flag::C, false);
    return val;
}

uint8_t CPU::cb_srl(uint8_t val) {
    uint8_t carry = val & 1;
    val >>= 1;
    setFlag(Flag::Z, val == 0); setFlag(Flag::N, false);
    setFlag(Flag::H, false); setFlag(Flag::C, carry);
    return val;
}

void CPU::cb_bit(int bit, uint8_t val) {
    setFlag(Flag::Z, (val & (1 << bit)) == 0);
    setFlag(Flag::N, false);
    setFlag(Flag::H, true);
    // C unchanged
}

// ══════════════════════════════════════════════════════════════════════
// Interrupt handling
// ══════════════════════════════════════════════════════════════════════

void CPU::handleInterrupts() {
    uint8_t ifReg = bus_.read(0xFF0F);
    uint8_t ieReg = bus_.read(0xFFFF);
    uint8_t pending = ifReg & ieReg & 0x1F;




    if (pending != 0) {
        // Any pending interrupt wakes from HALT, even with IME=0
        halted_ = false;
    }

    if (!ime_ || pending == 0) return;

    ime_ = false;

    // Two internal wait cycles
    internalCycle();  // 4T
    internalCycle();  // 4T

    // Push high byte of PC (SP decrements first, then writes)
    reg.sp--;
    writeByte(reg.sp, (reg.pc >> 8) & 0xFF);  // 4T

    // Re-read IE after the high-byte push — the push itself may have
    // written to 0xFFFF (if SP was 0x0000, the write targets IE).
    ieReg = bus_.read(0xFFFF);
    pending = ifReg & ieReg & 0x1F;

    // Push low byte of PC
    reg.sp--;
    writeByte(reg.sp, reg.pc & 0xFF);  // 4T

    if (pending == 0) {
        // Dispatch cancelled — no matching interrupt after IE change.
        // PC goes to 0x0000, IF is NOT modified.
        reg.pc = 0x0000;
    } else {
        // Service highest-priority interrupt from the refreshed pending set
        for (int i = 0; i < 5; i++) {
            if (pending & (1 << i)) {
                bus_.write(0xFF0F, bus_.read(0xFF0F) & ~(1 << i));
                reg.pc = 0x0040 + (i * 8);
                break;
            }
        }
    }

    internalCycle();  // 4T — set PC to vector
}
