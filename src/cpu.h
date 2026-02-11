#pragma once

#include <cstdint>
#include <cstdio>

class MemoryBus;

// ── Flag bit positions in F register ─────────────────────────────────
namespace Flag {
    constexpr uint8_t Z = 0x80;  // Zero
    constexpr uint8_t N = 0x40;  // Subtract
    constexpr uint8_t H = 0x20;  // Half-carry
    constexpr uint8_t C = 0x10;  // Carry
}

class CPU {
public:
    explicit CPU(MemoryBus& bus);

    // Advance the CPU by exactly 1 T-cycle. Returns true if still running.
    bool tick();

    // Check if CPU hit an unimplemented opcode or is stopped
    bool isHalted() const { return halted_; }
    bool isStopped() const { return stopped_; }
    bool hitUnimplemented() const { return unimplemented_; }
    bool hitMooneyeBreakpoint() const { return mooneyeBreakpoint_; }
    void clearMooneyeBreakpoint() { mooneyeBreakpoint_ = false; }

    // Total T-cycles elapsed
    uint64_t totalCycles() const { return totalCycles_; }

    // ── Registers ────────────────────────────────────────────────────
    // Little-endian: on x86/ARM, low byte is at lower address
    // AF: A=high, F=low  |  BC: B=high, C=low  |  etc.
    struct Registers {
        union { struct { uint8_t f; uint8_t a; }; uint16_t af; };
        union { struct { uint8_t c; uint8_t b; }; uint16_t bc; };
        union { struct { uint8_t e; uint8_t d; }; uint16_t de; };
        union { struct { uint8_t l; uint8_t h; }; uint16_t hl; };
        uint16_t sp;
        uint16_t pc;

        Registers() : af(0), bc(0), de(0), hl(0), sp(0), pc(0) {}
    };

    Registers reg;

private:
    MemoryBus& bus_;

    // ── State machine ────────────────────────────────────────────────
    uint64_t totalCycles_ = 0;  // Total T-cycles elapsed

    // Current opcode being executed
    uint8_t currentOpcode_ = 0;
    bool cbPrefix_ = false;

    // Status flags
    bool halted_ = false;
    bool stopped_ = false;
    bool unimplemented_ = false;
    bool mooneyeBreakpoint_ = false;

    // ── Interrupt state ──────────────────────────────────────────────
    bool ime_ = false;           // Interrupt Master Enable
    bool imeScheduled_ = false;  // EI enables IME after next instruction
    bool haltBug_ = false;       // HALT bug: PC not incremented on next fetch

    // ── Timing helpers ───────────────────────────────────────────────
    void tick4();                // tick bus 4 times, add 4 to totalCycles_
    void internalCycle();        // 4T internal delay (no memory access)

    // ── Memory helpers (each ticks the bus for 4 T-cycles) ──────────
    uint8_t  readByte(uint16_t addr);
    void     writeByte(uint16_t addr, uint8_t val);
    uint8_t  fetchByte();        // read at PC, then PC++
    uint16_t fetchWord();        // read 16-bit little-endian at PC

    // ── Flag helpers ─────────────────────────────────────────────────
    void setFlag(uint8_t flag, bool val);
    bool getFlag(uint8_t flag) const;

    // Always mask lower 4 bits of F
    void setF(uint8_t val) { reg.f = val & 0xF0; }

    // ── Stack helpers ────────────────────────────────────────────────
    void pushWord(uint16_t val);
    uint16_t popWord();

    // ── Instruction dispatch ─────────────────────────────────────────
    using OpHandler = void (CPU::*)();
    OpHandler unprefixed_[256];
    OpHandler cbprefixed_[256];

    void initTables();
    void executeOpcode();
    void executeCBOpcode();

    // ── Interrupt handling ───────────────────────────────────────────
    void handleInterrupts();

    // ── Opcode implementations ───────────────────────────────────────

    // -- Misc / Control --
    void op_nop();
    void op_stop();
    void op_halt();
    void op_di();
    void op_ei();
    void op_ccf();
    void op_scf();
    void op_cpl();
    void op_daa();

    // -- 8-bit Load --
    void op_ld_b_b(); void op_ld_b_c(); void op_ld_b_d(); void op_ld_b_e();
    void op_ld_b_h(); void op_ld_b_l(); void op_ld_b_hl(); void op_ld_b_a();
    void op_ld_c_b(); void op_ld_c_c(); void op_ld_c_d(); void op_ld_c_e();
    void op_ld_c_h(); void op_ld_c_l(); void op_ld_c_hl(); void op_ld_c_a();
    void op_ld_d_b(); void op_ld_d_c(); void op_ld_d_d(); void op_ld_d_e();
    void op_ld_d_h(); void op_ld_d_l(); void op_ld_d_hl(); void op_ld_d_a();
    void op_ld_e_b(); void op_ld_e_c(); void op_ld_e_d(); void op_ld_e_e();
    void op_ld_e_h(); void op_ld_e_l(); void op_ld_e_hl(); void op_ld_e_a();
    void op_ld_h_b(); void op_ld_h_c(); void op_ld_h_d(); void op_ld_h_e();
    void op_ld_h_h(); void op_ld_h_l(); void op_ld_h_hl(); void op_ld_h_a();
    void op_ld_l_b(); void op_ld_l_c(); void op_ld_l_d(); void op_ld_l_e();
    void op_ld_l_h(); void op_ld_l_l(); void op_ld_l_hl(); void op_ld_l_a();
    void op_ld_hl_b(); void op_ld_hl_c(); void op_ld_hl_d(); void op_ld_hl_e();
    void op_ld_hl_h(); void op_ld_hl_l(); void op_ld_hl_a();
    void op_ld_a_b(); void op_ld_a_c(); void op_ld_a_d(); void op_ld_a_e();
    void op_ld_a_h(); void op_ld_a_l(); void op_ld_a_hl(); void op_ld_a_a();

    void op_ld_b_n8(); void op_ld_c_n8(); void op_ld_d_n8(); void op_ld_e_n8();
    void op_ld_h_n8(); void op_ld_l_n8(); void op_ld_hl_n8(); void op_ld_a_n8();

    void op_ld_a_bc(); void op_ld_a_de();
    void op_ld_bc_a(); void op_ld_de_a();
    void op_ld_a_hli(); void op_ld_a_hld();
    void op_ld_hli_a(); void op_ld_hld_a();

    void op_ld_a_a16(); void op_ld_a16_a();
    void op_ldh_a_c(); void op_ldh_c_a();
    void op_ldh_a_a8(); void op_ldh_a8_a();

    // -- 16-bit Load --
    void op_ld_bc_n16(); void op_ld_de_n16(); void op_ld_hl_n16(); void op_ld_sp_n16();
    void op_ld_a16_sp();
    void op_ld_sp_hl();
    void op_ld_hl_sp_e8();

    // -- Push / Pop --
    void op_push_af(); void op_push_bc(); void op_push_de(); void op_push_hl();
    void op_pop_af();  void op_pop_bc();  void op_pop_de();  void op_pop_hl();

    // -- 8-bit ALU --
    void op_add_a_b(); void op_add_a_c(); void op_add_a_d(); void op_add_a_e();
    void op_add_a_h(); void op_add_a_l(); void op_add_a_hl(); void op_add_a_a();
    void op_add_a_n8();

    void op_adc_a_b(); void op_adc_a_c(); void op_adc_a_d(); void op_adc_a_e();
    void op_adc_a_h(); void op_adc_a_l(); void op_adc_a_hl(); void op_adc_a_a();
    void op_adc_a_n8();

    void op_sub_b(); void op_sub_c(); void op_sub_d(); void op_sub_e();
    void op_sub_h(); void op_sub_l(); void op_sub_hl(); void op_sub_a();
    void op_sub_n8();

    void op_sbc_a_b(); void op_sbc_a_c(); void op_sbc_a_d(); void op_sbc_a_e();
    void op_sbc_a_h(); void op_sbc_a_l(); void op_sbc_a_hl(); void op_sbc_a_a();
    void op_sbc_a_n8();

    void op_and_b(); void op_and_c(); void op_and_d(); void op_and_e();
    void op_and_h(); void op_and_l(); void op_and_hl(); void op_and_a();
    void op_and_n8();

    void op_xor_b(); void op_xor_c(); void op_xor_d(); void op_xor_e();
    void op_xor_h(); void op_xor_l(); void op_xor_hl(); void op_xor_a();
    void op_xor_n8();

    void op_or_b(); void op_or_c(); void op_or_d(); void op_or_e();
    void op_or_h(); void op_or_l(); void op_or_hl(); void op_or_a();
    void op_or_n8();

    void op_cp_b(); void op_cp_c(); void op_cp_d(); void op_cp_e();
    void op_cp_h(); void op_cp_l(); void op_cp_hl(); void op_cp_a();
    void op_cp_n8();

    // -- 8-bit INC/DEC --
    void op_inc_b(); void op_inc_c(); void op_inc_d(); void op_inc_e();
    void op_inc_h(); void op_inc_l(); void op_inc_hl_ind(); void op_inc_a();
    void op_dec_b(); void op_dec_c(); void op_dec_d(); void op_dec_e();
    void op_dec_h(); void op_dec_l(); void op_dec_hl_ind(); void op_dec_a();

    // -- 16-bit Arithmetic --
    void op_inc_bc(); void op_inc_de(); void op_inc_hl(); void op_inc_sp();
    void op_dec_bc(); void op_dec_de(); void op_dec_hl(); void op_dec_sp();
    void op_add_hl_bc(); void op_add_hl_de(); void op_add_hl_hl(); void op_add_hl_sp();
    void op_add_sp_e8();

    // -- Rotates / Shifts (unprefixed) --
    void op_rlca(); void op_rrca(); void op_rla(); void op_rra();

    // -- Jumps --
    void op_jp_n16(); void op_jp_hl();
    void op_jp_nz_n16(); void op_jp_z_n16(); void op_jp_nc_n16(); void op_jp_c_n16();
    void op_jr_e8();
    void op_jr_nz_e8(); void op_jr_z_e8(); void op_jr_nc_e8(); void op_jr_c_e8();

    // -- Calls --
    void op_call_n16();
    void op_call_nz_n16(); void op_call_z_n16(); void op_call_nc_n16(); void op_call_c_n16();

    // -- Returns --
    void op_ret(); void op_reti();
    void op_ret_nz(); void op_ret_z(); void op_ret_nc(); void op_ret_c();

    // -- RST --
    void op_rst_00(); void op_rst_08(); void op_rst_10(); void op_rst_18();
    void op_rst_20(); void op_rst_28(); void op_rst_30(); void op_rst_38();

    // -- CB prefix trigger --
    void op_prefix_cb();

    // ── CB-prefixed opcode implementations ───────────────────────────
    // The 8 target registers/memory for CB ops:
    //   B=0, C=1, D=2, E=3, H=4, L=5, (HL)=6, A=7

    // Helpers that operate on a value and return result
    uint8_t cb_rlc(uint8_t val);
    uint8_t cb_rrc(uint8_t val);
    uint8_t cb_rl(uint8_t val);
    uint8_t cb_rr(uint8_t val);
    uint8_t cb_sla(uint8_t val);
    uint8_t cb_sra(uint8_t val);
    uint8_t cb_swap(uint8_t val);
    uint8_t cb_srl(uint8_t val);
    void    cb_bit(int bit, uint8_t val);

    // Get/set register by index (0-7), 6=(HL)
    uint8_t getCBReg(int idx);
    void    setCBReg(int idx, uint8_t val);

    // CB handlers for each opcode 0x00-0xFF
    void cb_op_rlc_b();  void cb_op_rlc_c();  void cb_op_rlc_d();  void cb_op_rlc_e();
    void cb_op_rlc_h();  void cb_op_rlc_l();  void cb_op_rlc_hl(); void cb_op_rlc_a();
    void cb_op_rrc_b();  void cb_op_rrc_c();  void cb_op_rrc_d();  void cb_op_rrc_e();
    void cb_op_rrc_h();  void cb_op_rrc_l();  void cb_op_rrc_hl(); void cb_op_rrc_a();
    void cb_op_rl_b();   void cb_op_rl_c();   void cb_op_rl_d();   void cb_op_rl_e();
    void cb_op_rl_h();   void cb_op_rl_l();   void cb_op_rl_hl();  void cb_op_rl_a();
    void cb_op_rr_b();   void cb_op_rr_c();   void cb_op_rr_d();   void cb_op_rr_e();
    void cb_op_rr_h();   void cb_op_rr_l();   void cb_op_rr_hl();  void cb_op_rr_a();
    void cb_op_sla_b();  void cb_op_sla_c();  void cb_op_sla_d();  void cb_op_sla_e();
    void cb_op_sla_h();  void cb_op_sla_l();  void cb_op_sla_hl(); void cb_op_sla_a();
    void cb_op_sra_b();  void cb_op_sra_c();  void cb_op_sra_d();  void cb_op_sra_e();
    void cb_op_sra_h();  void cb_op_sra_l();  void cb_op_sra_hl(); void cb_op_sra_a();
    void cb_op_swap_b(); void cb_op_swap_c(); void cb_op_swap_d(); void cb_op_swap_e();
    void cb_op_swap_h(); void cb_op_swap_l(); void cb_op_swap_hl();void cb_op_swap_a();
    void cb_op_srl_b();  void cb_op_srl_c();  void cb_op_srl_d();  void cb_op_srl_e();
    void cb_op_srl_h();  void cb_op_srl_l();  void cb_op_srl_hl(); void cb_op_srl_a();

    // BIT b, r — 64 handlers (bits 0-7 × 8 registers)
    void cb_op_bit_0_b(); void cb_op_bit_0_c(); void cb_op_bit_0_d(); void cb_op_bit_0_e();
    void cb_op_bit_0_h(); void cb_op_bit_0_l(); void cb_op_bit_0_hl();void cb_op_bit_0_a();
    void cb_op_bit_1_b(); void cb_op_bit_1_c(); void cb_op_bit_1_d(); void cb_op_bit_1_e();
    void cb_op_bit_1_h(); void cb_op_bit_1_l(); void cb_op_bit_1_hl();void cb_op_bit_1_a();
    void cb_op_bit_2_b(); void cb_op_bit_2_c(); void cb_op_bit_2_d(); void cb_op_bit_2_e();
    void cb_op_bit_2_h(); void cb_op_bit_2_l(); void cb_op_bit_2_hl();void cb_op_bit_2_a();
    void cb_op_bit_3_b(); void cb_op_bit_3_c(); void cb_op_bit_3_d(); void cb_op_bit_3_e();
    void cb_op_bit_3_h(); void cb_op_bit_3_l(); void cb_op_bit_3_hl();void cb_op_bit_3_a();
    void cb_op_bit_4_b(); void cb_op_bit_4_c(); void cb_op_bit_4_d(); void cb_op_bit_4_e();
    void cb_op_bit_4_h(); void cb_op_bit_4_l(); void cb_op_bit_4_hl();void cb_op_bit_4_a();
    void cb_op_bit_5_b(); void cb_op_bit_5_c(); void cb_op_bit_5_d(); void cb_op_bit_5_e();
    void cb_op_bit_5_h(); void cb_op_bit_5_l(); void cb_op_bit_5_hl();void cb_op_bit_5_a();
    void cb_op_bit_6_b(); void cb_op_bit_6_c(); void cb_op_bit_6_d(); void cb_op_bit_6_e();
    void cb_op_bit_6_h(); void cb_op_bit_6_l(); void cb_op_bit_6_hl();void cb_op_bit_6_a();
    void cb_op_bit_7_b(); void cb_op_bit_7_c(); void cb_op_bit_7_d(); void cb_op_bit_7_e();
    void cb_op_bit_7_h(); void cb_op_bit_7_l(); void cb_op_bit_7_hl();void cb_op_bit_7_a();

    // RES b, r — 64 handlers
    void cb_op_res_0_b(); void cb_op_res_0_c(); void cb_op_res_0_d(); void cb_op_res_0_e();
    void cb_op_res_0_h(); void cb_op_res_0_l(); void cb_op_res_0_hl();void cb_op_res_0_a();
    void cb_op_res_1_b(); void cb_op_res_1_c(); void cb_op_res_1_d(); void cb_op_res_1_e();
    void cb_op_res_1_h(); void cb_op_res_1_l(); void cb_op_res_1_hl();void cb_op_res_1_a();
    void cb_op_res_2_b(); void cb_op_res_2_c(); void cb_op_res_2_d(); void cb_op_res_2_e();
    void cb_op_res_2_h(); void cb_op_res_2_l(); void cb_op_res_2_hl();void cb_op_res_2_a();
    void cb_op_res_3_b(); void cb_op_res_3_c(); void cb_op_res_3_d(); void cb_op_res_3_e();
    void cb_op_res_3_h(); void cb_op_res_3_l(); void cb_op_res_3_hl();void cb_op_res_3_a();
    void cb_op_res_4_b(); void cb_op_res_4_c(); void cb_op_res_4_d(); void cb_op_res_4_e();
    void cb_op_res_4_h(); void cb_op_res_4_l(); void cb_op_res_4_hl();void cb_op_res_4_a();
    void cb_op_res_5_b(); void cb_op_res_5_c(); void cb_op_res_5_d(); void cb_op_res_5_e();
    void cb_op_res_5_h(); void cb_op_res_5_l(); void cb_op_res_5_hl();void cb_op_res_5_a();
    void cb_op_res_6_b(); void cb_op_res_6_c(); void cb_op_res_6_d(); void cb_op_res_6_e();
    void cb_op_res_6_h(); void cb_op_res_6_l(); void cb_op_res_6_hl();void cb_op_res_6_a();
    void cb_op_res_7_b(); void cb_op_res_7_c(); void cb_op_res_7_d(); void cb_op_res_7_e();
    void cb_op_res_7_h(); void cb_op_res_7_l(); void cb_op_res_7_hl();void cb_op_res_7_a();

    // SET b, r — 64 handlers
    void cb_op_set_0_b(); void cb_op_set_0_c(); void cb_op_set_0_d(); void cb_op_set_0_e();
    void cb_op_set_0_h(); void cb_op_set_0_l(); void cb_op_set_0_hl();void cb_op_set_0_a();
    void cb_op_set_1_b(); void cb_op_set_1_c(); void cb_op_set_1_d(); void cb_op_set_1_e();
    void cb_op_set_1_h(); void cb_op_set_1_l(); void cb_op_set_1_hl();void cb_op_set_1_a();
    void cb_op_set_2_b(); void cb_op_set_2_c(); void cb_op_set_2_d(); void cb_op_set_2_e();
    void cb_op_set_2_h(); void cb_op_set_2_l(); void cb_op_set_2_hl();void cb_op_set_2_a();
    void cb_op_set_3_b(); void cb_op_set_3_c(); void cb_op_set_3_d(); void cb_op_set_3_e();
    void cb_op_set_3_h(); void cb_op_set_3_l(); void cb_op_set_3_hl();void cb_op_set_3_a();
    void cb_op_set_4_b(); void cb_op_set_4_c(); void cb_op_set_4_d(); void cb_op_set_4_e();
    void cb_op_set_4_h(); void cb_op_set_4_l(); void cb_op_set_4_hl();void cb_op_set_4_a();
    void cb_op_set_5_b(); void cb_op_set_5_c(); void cb_op_set_5_d(); void cb_op_set_5_e();
    void cb_op_set_5_h(); void cb_op_set_5_l(); void cb_op_set_5_hl();void cb_op_set_5_a();
    void cb_op_set_6_b(); void cb_op_set_6_c(); void cb_op_set_6_d(); void cb_op_set_6_e();
    void cb_op_set_6_h(); void cb_op_set_6_l(); void cb_op_set_6_hl();void cb_op_set_6_a();
    void cb_op_set_7_b(); void cb_op_set_7_c(); void cb_op_set_7_d(); void cb_op_set_7_e();
    void cb_op_set_7_h(); void cb_op_set_7_l(); void cb_op_set_7_hl();void cb_op_set_7_a();

    // ── ALU helpers ──────────────────────────────────────────────────
    void alu_add(uint8_t val);
    void alu_adc(uint8_t val);
    void alu_sub(uint8_t val);
    void alu_sbc(uint8_t val);
    void alu_and(uint8_t val);
    void alu_xor(uint8_t val);
    void alu_or(uint8_t val);
    void alu_cp(uint8_t val);
    void alu_inc(uint8_t& r);
    void alu_dec(uint8_t& r);
};
