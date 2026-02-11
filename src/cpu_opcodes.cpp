#include "cpu.h"
#include "memory_bus.h"

// ══════════════════════════════════════════════════════════════════════
// Misc / Control    (T-cycle costs from gbdev.io JSON)
// ══════════════════════════════════════════════════════════════════════

void CPU::op_nop()  { }  // 0x00 — 4T (fetch only)

void CPU::op_stop() { // 0x10
    stopped_ = true;
    fetchByte(); // consume the extra byte
}

void CPU::op_halt() { // 0x76
    // HALT bug: if IME=0 and there's a pending interrupt, PC doesn't increment next
    uint8_t pending = bus_.read(0xFF0F) & bus_.read(0xFFFF) & 0x1F;
    if (!ime_ && pending) {
        haltBug_ = true;
    } else {
        halted_ = true;
    }
}

void CPU::op_di() { ime_ = false; imeScheduled_ = false; } // 0xF3
void CPU::op_ei() { imeScheduled_ = true; }                // 0xFB

void CPU::op_ccf() { // 0x3F
    setFlag(Flag::N, false);
    setFlag(Flag::H, false);
    setFlag(Flag::C, !getFlag(Flag::C));
}

void CPU::op_scf() { // 0x37
    setFlag(Flag::N, false);
    setFlag(Flag::H, false);
    setFlag(Flag::C, true);
}

void CPU::op_cpl() { // 0x2F
    reg.a = ~reg.a;
    setFlag(Flag::N, true);
    setFlag(Flag::H, true);
}

void CPU::op_daa() { // 0x27
    int a = reg.a;
    if (!getFlag(Flag::N)) {
        if (getFlag(Flag::H) || (a & 0x0F) > 9) a += 0x06;
        if (getFlag(Flag::C) || a > 0x9F)        a += 0x60;
    } else {
        if (getFlag(Flag::H)) a = (a - 6) & 0xFF;
        if (getFlag(Flag::C)) a -= 0x60;
    }
    reg.a = a & 0xFF;
    setFlag(Flag::Z, reg.a == 0);
    setFlag(Flag::H, false);
    if (a >= 0x100) setFlag(Flag::C, true);
}

// ══════════════════════════════════════════════════════════════════════
// 8-bit Loads: LD r, r    (0x40–0x7F block, except 0x76=HALT)
// Register-to-register = 4T (fetch only)
// LD r,(HL) = 8T (fetch + read)
// LD (HL),r = 8T (fetch + write)
// ══════════════════════════════════════════════════════════════════════

// LD B, x
void CPU::op_ld_b_b() { if (!bus_.bootromActive()) mooneyeBreakpoint_ = true; }
void CPU::op_ld_b_c() { reg.b = reg.c; }
void CPU::op_ld_b_d() { reg.b = reg.d; }
void CPU::op_ld_b_e() { reg.b = reg.e; }
void CPU::op_ld_b_h() { reg.b = reg.h; }
void CPU::op_ld_b_l() { reg.b = reg.l; }
void CPU::op_ld_b_hl(){ reg.b = readByte(reg.hl); }
void CPU::op_ld_b_a() { reg.b = reg.a; }

// LD C, x
void CPU::op_ld_c_b() { reg.c = reg.b; }
void CPU::op_ld_c_c() { }
void CPU::op_ld_c_d() { reg.c = reg.d; }
void CPU::op_ld_c_e() { reg.c = reg.e; }
void CPU::op_ld_c_h() { reg.c = reg.h; }
void CPU::op_ld_c_l() { reg.c = reg.l; }
void CPU::op_ld_c_hl(){ reg.c = readByte(reg.hl); }
void CPU::op_ld_c_a() { reg.c = reg.a; }

// LD D, x
void CPU::op_ld_d_b() { reg.d = reg.b; }
void CPU::op_ld_d_c() { reg.d = reg.c; }
void CPU::op_ld_d_d() { }
void CPU::op_ld_d_e() { reg.d = reg.e; }
void CPU::op_ld_d_h() { reg.d = reg.h; }
void CPU::op_ld_d_l() { reg.d = reg.l; }
void CPU::op_ld_d_hl(){ reg.d = readByte(reg.hl); }
void CPU::op_ld_d_a() { reg.d = reg.a; }

// LD E, x
void CPU::op_ld_e_b() { reg.e = reg.b; }
void CPU::op_ld_e_c() { reg.e = reg.c; }
void CPU::op_ld_e_d() { reg.e = reg.d; }
void CPU::op_ld_e_e() { }
void CPU::op_ld_e_h() { reg.e = reg.h; }
void CPU::op_ld_e_l() { reg.e = reg.l; }
void CPU::op_ld_e_hl(){ reg.e = readByte(reg.hl); }
void CPU::op_ld_e_a() { reg.e = reg.a; }

// LD H, x
void CPU::op_ld_h_b() { reg.h = reg.b; }
void CPU::op_ld_h_c() { reg.h = reg.c; }
void CPU::op_ld_h_d() { reg.h = reg.d; }
void CPU::op_ld_h_e() { reg.h = reg.e; }
void CPU::op_ld_h_h() { }
void CPU::op_ld_h_l() { reg.h = reg.l; }
void CPU::op_ld_h_hl(){ reg.h = readByte(reg.hl); }
void CPU::op_ld_h_a() { reg.h = reg.a; }

// LD L, x
void CPU::op_ld_l_b() { reg.l = reg.b; }
void CPU::op_ld_l_c() { reg.l = reg.c; }
void CPU::op_ld_l_d() { reg.l = reg.d; }
void CPU::op_ld_l_e() { reg.l = reg.e; }
void CPU::op_ld_l_h() { reg.l = reg.h; }
void CPU::op_ld_l_l() { }
void CPU::op_ld_l_hl(){ reg.l = readByte(reg.hl); }
void CPU::op_ld_l_a() { reg.l = reg.a; }

// LD (HL), r
void CPU::op_ld_hl_b(){ writeByte(reg.hl, reg.b); }
void CPU::op_ld_hl_c(){ writeByte(reg.hl, reg.c); }
void CPU::op_ld_hl_d(){ writeByte(reg.hl, reg.d); }
void CPU::op_ld_hl_e(){ writeByte(reg.hl, reg.e); }
void CPU::op_ld_hl_h(){ writeByte(reg.hl, reg.h); }
void CPU::op_ld_hl_l(){ writeByte(reg.hl, reg.l); }
void CPU::op_ld_hl_a(){ writeByte(reg.hl, reg.a); }

// LD A, x
void CPU::op_ld_a_b() { reg.a = reg.b; }
void CPU::op_ld_a_c() { reg.a = reg.c; }
void CPU::op_ld_a_d() { reg.a = reg.d; }
void CPU::op_ld_a_e() { reg.a = reg.e; }
void CPU::op_ld_a_h() { reg.a = reg.h; }
void CPU::op_ld_a_l() { reg.a = reg.l; }
void CPU::op_ld_a_hl(){ reg.a = readByte(reg.hl); }
void CPU::op_ld_a_a() { }

// LD r, n8 — immediate loads (8T = fetch + read imm)
void CPU::op_ld_b_n8() { reg.b = fetchByte(); }
void CPU::op_ld_c_n8() { reg.c = fetchByte(); }
void CPU::op_ld_d_n8() { reg.d = fetchByte(); }
void CPU::op_ld_e_n8() { reg.e = fetchByte(); }
void CPU::op_ld_h_n8() { reg.h = fetchByte(); }
void CPU::op_ld_l_n8() { reg.l = fetchByte(); }
void CPU::op_ld_hl_n8(){ writeByte(reg.hl, fetchByte()); }  // 12T
void CPU::op_ld_a_n8() { reg.a = fetchByte(); }

// LD A, (rr) / LD (rr), A  (8T)
void CPU::op_ld_a_bc()  { reg.a = readByte(reg.bc); }
void CPU::op_ld_a_de()  { reg.a = readByte(reg.de); }
void CPU::op_ld_bc_a()  { writeByte(reg.bc, reg.a); }
void CPU::op_ld_de_a()  { writeByte(reg.de, reg.a); }

// LD A, (HL+/-) / LD (HL+/-), A  (8T)
void CPU::op_ld_a_hli() { reg.a = readByte(reg.hl); reg.hl++; }
void CPU::op_ld_a_hld() { reg.a = readByte(reg.hl); reg.hl--; }
void CPU::op_ld_hli_a() { writeByte(reg.hl, reg.a); reg.hl++; }
void CPU::op_ld_hld_a() { writeByte(reg.hl, reg.a); reg.hl--; }

// LD A, (a16) / LD (a16), A  (16T)
void CPU::op_ld_a_a16() { uint16_t addr = fetchWord(); reg.a = readByte(addr); }
void CPU::op_ld_a16_a() { uint16_t addr = fetchWord(); writeByte(addr, reg.a); }

// LDH A, (C) / LDH (C), A  (8T)
void CPU::op_ldh_a_c()  { reg.a = readByte(0xFF00 + reg.c); }
void CPU::op_ldh_c_a()  { writeByte(0xFF00 + reg.c, reg.a); }

// LDH A, (a8) / LDH (a8), A  (12T)
void CPU::op_ldh_a_a8() { uint8_t off = fetchByte(); reg.a = readByte(0xFF00 + off); }
void CPU::op_ldh_a8_a() { uint8_t off = fetchByte(); writeByte(0xFF00 + off, reg.a); }

// ══════════════════════════════════════════════════════════════════════
// 16-bit Loads
// ══════════════════════════════════════════════════════════════════════

void CPU::op_ld_bc_n16() { reg.bc = fetchWord(); }   // 12T
void CPU::op_ld_de_n16() { reg.de = fetchWord(); }
void CPU::op_ld_hl_n16() { reg.hl = fetchWord(); }
void CPU::op_ld_sp_n16() { reg.sp = fetchWord(); }

void CPU::op_ld_a16_sp() { // 0x08 — 20T
    uint16_t addr = fetchWord();
    writeByte(addr,     reg.sp & 0xFF);
    writeByte(addr + 1, (reg.sp >> 8) & 0xFF);
}

void CPU::op_ld_sp_hl() { reg.sp = reg.hl; internalCycle(); } // 0xF9 — 8T

void CPU::op_ld_hl_sp_e8() { // 0xF8 — 12T
    int8_t offset = static_cast<int8_t>(fetchByte());
    uint32_t result = reg.sp + offset;
    setFlag(Flag::Z, false);
    setFlag(Flag::N, false);
    setFlag(Flag::H, ((reg.sp ^ offset ^ result) & 0x10) != 0);
    setFlag(Flag::C, ((reg.sp ^ offset ^ result) & 0x100) != 0);
    reg.hl = result & 0xFFFF;
    internalCycle();
}

// ══════════════════════════════════════════════════════════════════════
// Push / Pop
// ══════════════════════════════════════════════════════════════════════

void CPU::op_push_af() { internalCycle(); pushWord(reg.af); }  // 16T
void CPU::op_push_bc() { internalCycle(); pushWord(reg.bc); }
void CPU::op_push_de() { internalCycle(); pushWord(reg.de); }
void CPU::op_push_hl() { internalCycle(); pushWord(reg.hl); }

void CPU::op_pop_af() { reg.af = popWord(); reg.f &= 0xF0; }  // 12T
void CPU::op_pop_bc() { reg.bc = popWord(); }
void CPU::op_pop_de() { reg.de = popWord(); }
void CPU::op_pop_hl() { reg.hl = popWord(); }

// ══════════════════════════════════════════════════════════════════════
// 8-bit ALU — ADD, ADC, SUB, SBC, AND, XOR, OR, CP
// Register = 4T (fetch only), (HL) = 8T, immediate = 8T
// ══════════════════════════════════════════════════════════════════════

void CPU::op_add_a_b()  { alu_add(reg.b); }
void CPU::op_add_a_c()  { alu_add(reg.c); }
void CPU::op_add_a_d()  { alu_add(reg.d); }
void CPU::op_add_a_e()  { alu_add(reg.e); }
void CPU::op_add_a_h()  { alu_add(reg.h); }
void CPU::op_add_a_l()  { alu_add(reg.l); }
void CPU::op_add_a_hl() { alu_add(readByte(reg.hl)); }
void CPU::op_add_a_a()  { alu_add(reg.a); }
void CPU::op_add_a_n8() { alu_add(fetchByte()); }

void CPU::op_adc_a_b()  { alu_adc(reg.b); }
void CPU::op_adc_a_c()  { alu_adc(reg.c); }
void CPU::op_adc_a_d()  { alu_adc(reg.d); }
void CPU::op_adc_a_e()  { alu_adc(reg.e); }
void CPU::op_adc_a_h()  { alu_adc(reg.h); }
void CPU::op_adc_a_l()  { alu_adc(reg.l); }
void CPU::op_adc_a_hl() { alu_adc(readByte(reg.hl)); }
void CPU::op_adc_a_a()  { alu_adc(reg.a); }
void CPU::op_adc_a_n8() { alu_adc(fetchByte()); }

void CPU::op_sub_b()    { alu_sub(reg.b); }
void CPU::op_sub_c()    { alu_sub(reg.c); }
void CPU::op_sub_d()    { alu_sub(reg.d); }
void CPU::op_sub_e()    { alu_sub(reg.e); }
void CPU::op_sub_h()    { alu_sub(reg.h); }
void CPU::op_sub_l()    { alu_sub(reg.l); }
void CPU::op_sub_hl()   { alu_sub(readByte(reg.hl)); }
void CPU::op_sub_a()    { alu_sub(reg.a); }
void CPU::op_sub_n8()   { alu_sub(fetchByte()); }

void CPU::op_sbc_a_b()  { alu_sbc(reg.b); }
void CPU::op_sbc_a_c()  { alu_sbc(reg.c); }
void CPU::op_sbc_a_d()  { alu_sbc(reg.d); }
void CPU::op_sbc_a_e()  { alu_sbc(reg.e); }
void CPU::op_sbc_a_h()  { alu_sbc(reg.h); }
void CPU::op_sbc_a_l()  { alu_sbc(reg.l); }
void CPU::op_sbc_a_hl() { alu_sbc(readByte(reg.hl)); }
void CPU::op_sbc_a_a()  { alu_sbc(reg.a); }
void CPU::op_sbc_a_n8() { alu_sbc(fetchByte()); }

void CPU::op_and_b()    { alu_and(reg.b); }
void CPU::op_and_c()    { alu_and(reg.c); }
void CPU::op_and_d()    { alu_and(reg.d); }
void CPU::op_and_e()    { alu_and(reg.e); }
void CPU::op_and_h()    { alu_and(reg.h); }
void CPU::op_and_l()    { alu_and(reg.l); }
void CPU::op_and_hl()   { alu_and(readByte(reg.hl)); }
void CPU::op_and_a()    { alu_and(reg.a); }
void CPU::op_and_n8()   { alu_and(fetchByte()); }

void CPU::op_xor_b()    { alu_xor(reg.b); }
void CPU::op_xor_c()    { alu_xor(reg.c); }
void CPU::op_xor_d()    { alu_xor(reg.d); }
void CPU::op_xor_e()    { alu_xor(reg.e); }
void CPU::op_xor_h()    { alu_xor(reg.h); }
void CPU::op_xor_l()    { alu_xor(reg.l); }
void CPU::op_xor_hl()   { alu_xor(readByte(reg.hl)); }
void CPU::op_xor_a()    { alu_xor(reg.a); }
void CPU::op_xor_n8()   { alu_xor(fetchByte()); }

void CPU::op_or_b()     { alu_or(reg.b); }
void CPU::op_or_c()     { alu_or(reg.c); }
void CPU::op_or_d()     { alu_or(reg.d); }
void CPU::op_or_e()     { alu_or(reg.e); }
void CPU::op_or_h()     { alu_or(reg.h); }
void CPU::op_or_l()     { alu_or(reg.l); }
void CPU::op_or_hl()    { alu_or(readByte(reg.hl)); }
void CPU::op_or_a()     { alu_or(reg.a); }
void CPU::op_or_n8()    { alu_or(fetchByte()); }

void CPU::op_cp_b()     { alu_cp(reg.b); }
void CPU::op_cp_c()     { alu_cp(reg.c); }
void CPU::op_cp_d()     { alu_cp(reg.d); }
void CPU::op_cp_e()     { alu_cp(reg.e); }
void CPU::op_cp_h()     { alu_cp(reg.h); }
void CPU::op_cp_l()     { alu_cp(reg.l); }
void CPU::op_cp_hl()    { alu_cp(readByte(reg.hl)); }
void CPU::op_cp_a()     { alu_cp(reg.a); }
void CPU::op_cp_n8()    { alu_cp(fetchByte()); }

// ══════════════════════════════════════════════════════════════════════
// 8-bit INC / DEC
// Register = 4T, (HL) = 12T (fetch + read + write)
// ══════════════════════════════════════════════════════════════════════

void CPU::op_inc_b() { alu_inc(reg.b); }
void CPU::op_inc_c() { alu_inc(reg.c); }
void CPU::op_inc_d() { alu_inc(reg.d); }
void CPU::op_inc_e() { alu_inc(reg.e); }
void CPU::op_inc_h() { alu_inc(reg.h); }
void CPU::op_inc_l() { alu_inc(reg.l); }
void CPU::op_inc_a() { alu_inc(reg.a); }
void CPU::op_inc_hl_ind() { // INC (HL) — 12T
    uint8_t val = readByte(reg.hl);
    alu_inc(val);
    writeByte(reg.hl, val);
}

void CPU::op_dec_b() { alu_dec(reg.b); }
void CPU::op_dec_c() { alu_dec(reg.c); }
void CPU::op_dec_d() { alu_dec(reg.d); }
void CPU::op_dec_e() { alu_dec(reg.e); }
void CPU::op_dec_h() { alu_dec(reg.h); }
void CPU::op_dec_l() { alu_dec(reg.l); }
void CPU::op_dec_a() { alu_dec(reg.a); }
void CPU::op_dec_hl_ind() { // DEC (HL) — 12T
    uint8_t val = readByte(reg.hl);
    alu_dec(val);
    writeByte(reg.hl, val);
}

// ══════════════════════════════════════════════════════════════════════
// 16-bit Arithmetic
// INC/DEC rr = 8T (fetch + internal), ADD HL,rr = 8T (fetch + internal)
// ══════════════════════════════════════════════════════════════════════

void CPU::op_inc_bc() { reg.bc++; internalCycle(); }
void CPU::op_inc_de() { reg.de++; internalCycle(); }
void CPU::op_inc_hl() { reg.hl++; internalCycle(); }
void CPU::op_inc_sp() { reg.sp++; internalCycle(); }

void CPU::op_dec_bc() { reg.bc--; internalCycle(); }
void CPU::op_dec_de() { reg.de--; internalCycle(); }
void CPU::op_dec_hl() { reg.hl--; internalCycle(); }
void CPU::op_dec_sp() { reg.sp--; internalCycle(); }

void CPU::op_add_hl_bc() {
    uint32_t r = reg.hl + reg.bc;
    setFlag(Flag::N, false);
    setFlag(Flag::H, ((reg.hl & 0x0FFF) + (reg.bc & 0x0FFF)) > 0x0FFF);
    setFlag(Flag::C, r > 0xFFFF);
    reg.hl = r & 0xFFFF;
    internalCycle();
}
void CPU::op_add_hl_de() {
    uint32_t r = reg.hl + reg.de;
    setFlag(Flag::N, false);
    setFlag(Flag::H, ((reg.hl & 0x0FFF) + (reg.de & 0x0FFF)) > 0x0FFF);
    setFlag(Flag::C, r > 0xFFFF);
    reg.hl = r & 0xFFFF;
    internalCycle();
}
void CPU::op_add_hl_hl() {
    uint32_t r = reg.hl + reg.hl;
    setFlag(Flag::N, false);
    setFlag(Flag::H, ((reg.hl & 0x0FFF) + (reg.hl & 0x0FFF)) > 0x0FFF);
    setFlag(Flag::C, r > 0xFFFF);
    reg.hl = r & 0xFFFF;
    internalCycle();
}
void CPU::op_add_hl_sp() {
    uint32_t r = reg.hl + reg.sp;
    setFlag(Flag::N, false);
    setFlag(Flag::H, ((reg.hl & 0x0FFF) + (reg.sp & 0x0FFF)) > 0x0FFF);
    setFlag(Flag::C, r > 0xFFFF);
    reg.hl = r & 0xFFFF;
    internalCycle();
}

void CPU::op_add_sp_e8() { // 0xE8 — 16T (fetch + read imm + 2 internal)
    int8_t offset = static_cast<int8_t>(fetchByte());
    uint32_t result = reg.sp + offset;
    setFlag(Flag::Z, false);
    setFlag(Flag::N, false);
    setFlag(Flag::H, ((reg.sp ^ offset ^ result) & 0x10) != 0);
    setFlag(Flag::C, ((reg.sp ^ offset ^ result) & 0x100) != 0);
    reg.sp = result & 0xFFFF;
    internalCycle();
    internalCycle();
}

// ══════════════════════════════════════════════════════════════════════
// Rotates (unprefixed — always clear Z flag)
// All 4T (fetch only)
// ══════════════════════════════════════════════════════════════════════

void CPU::op_rlca() {
    uint8_t carry = (reg.a >> 7) & 1;
    reg.a = (reg.a << 1) | carry;
    setFlag(Flag::Z, false); setFlag(Flag::N, false);
    setFlag(Flag::H, false); setFlag(Flag::C, carry);
}

void CPU::op_rrca() {
    uint8_t carry = reg.a & 1;
    reg.a = (reg.a >> 1) | (carry << 7);
    setFlag(Flag::Z, false); setFlag(Flag::N, false);
    setFlag(Flag::H, false); setFlag(Flag::C, carry);
}

void CPU::op_rla() {
    uint8_t oldC = getFlag(Flag::C) ? 1 : 0;
    uint8_t newC = (reg.a >> 7) & 1;
    reg.a = (reg.a << 1) | oldC;
    setFlag(Flag::Z, false); setFlag(Flag::N, false);
    setFlag(Flag::H, false); setFlag(Flag::C, newC);
}

void CPU::op_rra() {
    uint8_t oldC = getFlag(Flag::C) ? 0x80 : 0;
    uint8_t newC = reg.a & 1;
    reg.a = (reg.a >> 1) | oldC;
    setFlag(Flag::Z, false); setFlag(Flag::N, false);
    setFlag(Flag::H, false); setFlag(Flag::C, newC);
}

// ══════════════════════════════════════════════════════════════════════
// Jumps
// ══════════════════════════════════════════════════════════════════════

void CPU::op_jp_n16()    { reg.pc = fetchWord(); internalCycle(); }  // 16T
void CPU::op_jp_hl()     { reg.pc = reg.hl; }                       // 4T

void CPU::op_jp_nz_n16() { uint16_t a = fetchWord(); if (!getFlag(Flag::Z)) { reg.pc = a; internalCycle(); } }  // 16T taken / 12T not
void CPU::op_jp_z_n16()  { uint16_t a = fetchWord(); if ( getFlag(Flag::Z)) { reg.pc = a; internalCycle(); } }
void CPU::op_jp_nc_n16() { uint16_t a = fetchWord(); if (!getFlag(Flag::C)) { reg.pc = a; internalCycle(); } }
void CPU::op_jp_c_n16()  { uint16_t a = fetchWord(); if ( getFlag(Flag::C)) { reg.pc = a; internalCycle(); } }

void CPU::op_jr_e8()     { int8_t o = (int8_t)fetchByte(); reg.pc += o; internalCycle(); }  // 12T
void CPU::op_jr_nz_e8()  { int8_t o = (int8_t)fetchByte(); if (!getFlag(Flag::Z)) { reg.pc += o; internalCycle(); } }  // 12T taken / 8T not
void CPU::op_jr_z_e8()   { int8_t o = (int8_t)fetchByte(); if ( getFlag(Flag::Z)) { reg.pc += o; internalCycle(); } }
void CPU::op_jr_nc_e8()  { int8_t o = (int8_t)fetchByte(); if (!getFlag(Flag::C)) { reg.pc += o; internalCycle(); } }
void CPU::op_jr_c_e8()   { int8_t o = (int8_t)fetchByte(); if ( getFlag(Flag::C)) { reg.pc += o; internalCycle(); } }

// ══════════════════════════════════════════════════════════════════════
// Calls
// CALL nn = 24T taken: fetch(4) + read_lo(4) + read_hi(4) + internal(4) + push_hi(4) + push_lo(4)
// CALL cc, nn not-taken = 12T: fetch(4) + read_lo(4) + read_hi(4)
// ══════════════════════════════════════════════════════════════════════

void CPU::op_call_n16()    { uint16_t a = fetchWord(); internalCycle(); pushWord(reg.pc); reg.pc = a; }
void CPU::op_call_nz_n16() { uint16_t a = fetchWord(); if (!getFlag(Flag::Z)) { internalCycle(); pushWord(reg.pc); reg.pc = a; } }
void CPU::op_call_z_n16()  { uint16_t a = fetchWord(); if ( getFlag(Flag::Z)) { internalCycle(); pushWord(reg.pc); reg.pc = a; } }
void CPU::op_call_nc_n16() { uint16_t a = fetchWord(); if (!getFlag(Flag::C)) { internalCycle(); pushWord(reg.pc); reg.pc = a; } }
void CPU::op_call_c_n16()  { uint16_t a = fetchWord(); if ( getFlag(Flag::C)) { internalCycle(); pushWord(reg.pc); reg.pc = a; } }

// ══════════════════════════════════════════════════════════════════════
// Returns
// RET = 16T: fetch(4) + pop_lo(4) + pop_hi(4) + internal(4)
// RET cc taken = 20T: fetch(4) + internal(4) + pop_lo(4) + pop_hi(4) + internal(4)
// RET cc not-taken = 8T: fetch(4) + internal(4)
// ══════════════════════════════════════════════════════════════════════

void CPU::op_ret()     { reg.pc = popWord(); internalCycle(); }
void CPU::op_reti()    { reg.pc = popWord(); internalCycle(); ime_ = true; }
void CPU::op_ret_nz()  { internalCycle(); if (!getFlag(Flag::Z)) { reg.pc = popWord(); internalCycle(); } }
void CPU::op_ret_z()   { internalCycle(); if ( getFlag(Flag::Z)) { reg.pc = popWord(); internalCycle(); } }
void CPU::op_ret_nc()  { internalCycle(); if (!getFlag(Flag::C)) { reg.pc = popWord(); internalCycle(); } }
void CPU::op_ret_c()   { internalCycle(); if ( getFlag(Flag::C)) { reg.pc = popWord(); internalCycle(); } }

// ══════════════════════════════════════════════════════════════════════
// RST
// RST nn = 16T: fetch(4) + internal(4) + push_hi(4) + push_lo(4)
// ══════════════════════════════════════════════════════════════════════

void CPU::op_rst_00() { internalCycle(); pushWord(reg.pc); reg.pc = 0x00; }
void CPU::op_rst_08() { internalCycle(); pushWord(reg.pc); reg.pc = 0x08; }
void CPU::op_rst_10() { internalCycle(); pushWord(reg.pc); reg.pc = 0x10; }
void CPU::op_rst_18() { internalCycle(); pushWord(reg.pc); reg.pc = 0x18; }
void CPU::op_rst_20() { internalCycle(); pushWord(reg.pc); reg.pc = 0x20; }
void CPU::op_rst_28() { internalCycle(); pushWord(reg.pc); reg.pc = 0x28; }
void CPU::op_rst_30() { internalCycle(); pushWord(reg.pc); reg.pc = 0x30; }
void CPU::op_rst_38() { internalCycle(); pushWord(reg.pc); reg.pc = 0x38; }

// ══════════════════════════════════════════════════════════════════════
// CB prefix trigger
// ══════════════════════════════════════════════════════════════════════

void CPU::op_prefix_cb() {
    // Handled in executeOpcode() — this should never be called directly
}
