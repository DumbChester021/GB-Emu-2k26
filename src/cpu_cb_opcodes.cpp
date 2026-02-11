#include "cpu.h"
#include "memory_bus.h"

// ══════════════════════════════════════════════════════════════════════
// CB-prefixed: RLC, RRC, RL, RR, SLA, SRA, SWAP, SRL
// Register ops = 8T (4T CB prefix + 4T CB opcode fetch — no extra cycles)
// (HL) ops = 16T (4T CB + 4T fetch + 4T read + 4T write)
// ══════════════════════════════════════════════════════════════════════

#define CB_SHIFT_REG(name, fn, r) \
    void CPU::cb_op_##name##_##r() { reg.r = fn(reg.r); }
#define CB_SHIFT_HL(name, fn) \
    void CPU::cb_op_##name##_hl() { uint8_t v = readByte(reg.hl); v = fn(v); writeByte(reg.hl, v); }
#define CB_SHIFT_ALL(name, fn) \
    CB_SHIFT_REG(name, fn, b) CB_SHIFT_REG(name, fn, c) \
    CB_SHIFT_REG(name, fn, d) CB_SHIFT_REG(name, fn, e) \
    CB_SHIFT_REG(name, fn, h) CB_SHIFT_REG(name, fn, l) \
    CB_SHIFT_HL(name, fn) \
    CB_SHIFT_REG(name, fn, a)

CB_SHIFT_ALL(rlc,  cb_rlc)
CB_SHIFT_ALL(rrc,  cb_rrc)
CB_SHIFT_ALL(rl,   cb_rl)
CB_SHIFT_ALL(rr,   cb_rr)
CB_SHIFT_ALL(sla,  cb_sla)
CB_SHIFT_ALL(sra,  cb_sra)
CB_SHIFT_ALL(swap, cb_swap)
CB_SHIFT_ALL(srl,  cb_srl)

#undef CB_SHIFT_REG
#undef CB_SHIFT_HL
#undef CB_SHIFT_ALL

// ══════════════════════════════════════════════════════════════════════
// CB-prefixed: BIT b, r
// Register = 8T (4T CB + 4T fetch), (HL) = 12T (+ 4T read)
// ══════════════════════════════════════════════════════════════════════

#define CB_BIT_REG(bit, r) \
    void CPU::cb_op_bit_##bit##_##r() { cb_bit(bit, reg.r); }
#define CB_BIT_HL(bit) \
    void CPU::cb_op_bit_##bit##_hl() { cb_bit(bit, readByte(reg.hl)); }
#define CB_BIT_ALL(bit) \
    CB_BIT_REG(bit, b) CB_BIT_REG(bit, c) CB_BIT_REG(bit, d) CB_BIT_REG(bit, e) \
    CB_BIT_REG(bit, h) CB_BIT_REG(bit, l) CB_BIT_HL(bit) CB_BIT_REG(bit, a)

CB_BIT_ALL(0) CB_BIT_ALL(1) CB_BIT_ALL(2) CB_BIT_ALL(3)
CB_BIT_ALL(4) CB_BIT_ALL(5) CB_BIT_ALL(6) CB_BIT_ALL(7)

#undef CB_BIT_REG
#undef CB_BIT_HL
#undef CB_BIT_ALL

// ══════════════════════════════════════════════════════════════════════
// CB-prefixed: RES b, r / SET b, r
// Register = 8T (4T CB + 4T fetch), (HL) = 16T (+ 4T read + 4T write)
// ══════════════════════════════════════════════════════════════════════

#define CB_RES_REG(bit, r) \
    void CPU::cb_op_res_##bit##_##r() { reg.r &= ~(1 << bit); }
#define CB_RES_HL(bit) \
    void CPU::cb_op_res_##bit##_hl() { uint8_t v = readByte(reg.hl); v &= ~(1 << bit); writeByte(reg.hl, v); }
#define CB_RES_ALL(bit) \
    CB_RES_REG(bit, b) CB_RES_REG(bit, c) CB_RES_REG(bit, d) CB_RES_REG(bit, e) \
    CB_RES_REG(bit, h) CB_RES_REG(bit, l) CB_RES_HL(bit) CB_RES_REG(bit, a)

CB_RES_ALL(0) CB_RES_ALL(1) CB_RES_ALL(2) CB_RES_ALL(3)
CB_RES_ALL(4) CB_RES_ALL(5) CB_RES_ALL(6) CB_RES_ALL(7)

#undef CB_RES_REG
#undef CB_RES_HL
#undef CB_RES_ALL

#define CB_SET_REG(bit, r) \
    void CPU::cb_op_set_##bit##_##r() { reg.r |= (1 << bit); }
#define CB_SET_HL(bit) \
    void CPU::cb_op_set_##bit##_hl() { uint8_t v = readByte(reg.hl); v |= (1 << bit); writeByte(reg.hl, v); }
#define CB_SET_ALL(bit) \
    CB_SET_REG(bit, b) CB_SET_REG(bit, c) CB_SET_REG(bit, d) CB_SET_REG(bit, e) \
    CB_SET_REG(bit, h) CB_SET_REG(bit, l) CB_SET_HL(bit) CB_SET_REG(bit, a)

CB_SET_ALL(0) CB_SET_ALL(1) CB_SET_ALL(2) CB_SET_ALL(3)
CB_SET_ALL(4) CB_SET_ALL(5) CB_SET_ALL(6) CB_SET_ALL(7)

#undef CB_SET_REG
#undef CB_SET_HL
#undef CB_SET_ALL
