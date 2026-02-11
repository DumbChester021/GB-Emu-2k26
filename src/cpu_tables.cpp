#include "cpu.h"

// ══════════════════════════════════════════════════════════════════════
// Dispatch table initialization — maps every opcode to its handler.
// Slots left as nullptr will trigger "UNIMPLEMENTED OPCODE" at runtime.
// ══════════════════════════════════════════════════════════════════════

void CPU::initTables() {

    // ── Unprefixed opcodes ───────────────────────────────────────────

    // 0x0x
    unprefixed_[0x00] = &CPU::op_nop;
    unprefixed_[0x01] = &CPU::op_ld_bc_n16;
    unprefixed_[0x02] = &CPU::op_ld_bc_a;
    unprefixed_[0x03] = &CPU::op_inc_bc;
    unprefixed_[0x04] = &CPU::op_inc_b;
    unprefixed_[0x05] = &CPU::op_dec_b;
    unprefixed_[0x06] = &CPU::op_ld_b_n8;
    unprefixed_[0x07] = &CPU::op_rlca;
    unprefixed_[0x08] = &CPU::op_ld_a16_sp;
    unprefixed_[0x09] = &CPU::op_add_hl_bc;
    unprefixed_[0x0A] = &CPU::op_ld_a_bc;
    unprefixed_[0x0B] = &CPU::op_dec_bc;
    unprefixed_[0x0C] = &CPU::op_inc_c;
    unprefixed_[0x0D] = &CPU::op_dec_c;
    unprefixed_[0x0E] = &CPU::op_ld_c_n8;
    unprefixed_[0x0F] = &CPU::op_rrca;

    // 0x1x
    unprefixed_[0x10] = &CPU::op_stop;
    unprefixed_[0x11] = &CPU::op_ld_de_n16;
    unprefixed_[0x12] = &CPU::op_ld_de_a;
    unprefixed_[0x13] = &CPU::op_inc_de;
    unprefixed_[0x14] = &CPU::op_inc_d;
    unprefixed_[0x15] = &CPU::op_dec_d;
    unprefixed_[0x16] = &CPU::op_ld_d_n8;
    unprefixed_[0x17] = &CPU::op_rla;
    unprefixed_[0x18] = &CPU::op_jr_e8;
    unprefixed_[0x19] = &CPU::op_add_hl_de;
    unprefixed_[0x1A] = &CPU::op_ld_a_de;
    unprefixed_[0x1B] = &CPU::op_dec_de;
    unprefixed_[0x1C] = &CPU::op_inc_e;
    unprefixed_[0x1D] = &CPU::op_dec_e;
    unprefixed_[0x1E] = &CPU::op_ld_e_n8;
    unprefixed_[0x1F] = &CPU::op_rra;

    // 0x2x
    unprefixed_[0x20] = &CPU::op_jr_nz_e8;
    unprefixed_[0x21] = &CPU::op_ld_hl_n16;
    unprefixed_[0x22] = &CPU::op_ld_hli_a;
    unprefixed_[0x23] = &CPU::op_inc_hl;
    unprefixed_[0x24] = &CPU::op_inc_h;
    unprefixed_[0x25] = &CPU::op_dec_h;
    unprefixed_[0x26] = &CPU::op_ld_h_n8;
    unprefixed_[0x27] = &CPU::op_daa;
    unprefixed_[0x28] = &CPU::op_jr_z_e8;
    unprefixed_[0x29] = &CPU::op_add_hl_hl;
    unprefixed_[0x2A] = &CPU::op_ld_a_hli;
    unprefixed_[0x2B] = &CPU::op_dec_hl;
    unprefixed_[0x2C] = &CPU::op_inc_l;
    unprefixed_[0x2D] = &CPU::op_dec_l;
    unprefixed_[0x2E] = &CPU::op_ld_l_n8;
    unprefixed_[0x2F] = &CPU::op_cpl;

    // 0x3x
    unprefixed_[0x30] = &CPU::op_jr_nc_e8;
    unprefixed_[0x31] = &CPU::op_ld_sp_n16;
    unprefixed_[0x32] = &CPU::op_ld_hld_a;
    unprefixed_[0x33] = &CPU::op_inc_sp;
    unprefixed_[0x34] = &CPU::op_inc_hl_ind;
    unprefixed_[0x35] = &CPU::op_dec_hl_ind;
    unprefixed_[0x36] = &CPU::op_ld_hl_n8;
    unprefixed_[0x37] = &CPU::op_scf;
    unprefixed_[0x38] = &CPU::op_jr_c_e8;
    unprefixed_[0x39] = &CPU::op_add_hl_sp;
    unprefixed_[0x3A] = &CPU::op_ld_a_hld;
    unprefixed_[0x3B] = &CPU::op_dec_sp;
    unprefixed_[0x3C] = &CPU::op_inc_a;
    unprefixed_[0x3D] = &CPU::op_dec_a;
    unprefixed_[0x3E] = &CPU::op_ld_a_n8;
    unprefixed_[0x3F] = &CPU::op_ccf;

    // 0x4x — LD B/C, r
    unprefixed_[0x40] = &CPU::op_ld_b_b;  unprefixed_[0x41] = &CPU::op_ld_b_c;
    unprefixed_[0x42] = &CPU::op_ld_b_d;  unprefixed_[0x43] = &CPU::op_ld_b_e;
    unprefixed_[0x44] = &CPU::op_ld_b_h;  unprefixed_[0x45] = &CPU::op_ld_b_l;
    unprefixed_[0x46] = &CPU::op_ld_b_hl; unprefixed_[0x47] = &CPU::op_ld_b_a;
    unprefixed_[0x48] = &CPU::op_ld_c_b;  unprefixed_[0x49] = &CPU::op_ld_c_c;
    unprefixed_[0x4A] = &CPU::op_ld_c_d;  unprefixed_[0x4B] = &CPU::op_ld_c_e;
    unprefixed_[0x4C] = &CPU::op_ld_c_h;  unprefixed_[0x4D] = &CPU::op_ld_c_l;
    unprefixed_[0x4E] = &CPU::op_ld_c_hl; unprefixed_[0x4F] = &CPU::op_ld_c_a;

    // 0x5x — LD D/E, r
    unprefixed_[0x50] = &CPU::op_ld_d_b;  unprefixed_[0x51] = &CPU::op_ld_d_c;
    unprefixed_[0x52] = &CPU::op_ld_d_d;  unprefixed_[0x53] = &CPU::op_ld_d_e;
    unprefixed_[0x54] = &CPU::op_ld_d_h;  unprefixed_[0x55] = &CPU::op_ld_d_l;
    unprefixed_[0x56] = &CPU::op_ld_d_hl; unprefixed_[0x57] = &CPU::op_ld_d_a;
    unprefixed_[0x58] = &CPU::op_ld_e_b;  unprefixed_[0x59] = &CPU::op_ld_e_c;
    unprefixed_[0x5A] = &CPU::op_ld_e_d;  unprefixed_[0x5B] = &CPU::op_ld_e_e;
    unprefixed_[0x5C] = &CPU::op_ld_e_h;  unprefixed_[0x5D] = &CPU::op_ld_e_l;
    unprefixed_[0x5E] = &CPU::op_ld_e_hl; unprefixed_[0x5F] = &CPU::op_ld_e_a;

    // 0x6x — LD H/L, r
    unprefixed_[0x60] = &CPU::op_ld_h_b;  unprefixed_[0x61] = &CPU::op_ld_h_c;
    unprefixed_[0x62] = &CPU::op_ld_h_d;  unprefixed_[0x63] = &CPU::op_ld_h_e;
    unprefixed_[0x64] = &CPU::op_ld_h_h;  unprefixed_[0x65] = &CPU::op_ld_h_l;
    unprefixed_[0x66] = &CPU::op_ld_h_hl; unprefixed_[0x67] = &CPU::op_ld_h_a;
    unprefixed_[0x68] = &CPU::op_ld_l_b;  unprefixed_[0x69] = &CPU::op_ld_l_c;
    unprefixed_[0x6A] = &CPU::op_ld_l_d;  unprefixed_[0x6B] = &CPU::op_ld_l_e;
    unprefixed_[0x6C] = &CPU::op_ld_l_h;  unprefixed_[0x6D] = &CPU::op_ld_l_l;
    unprefixed_[0x6E] = &CPU::op_ld_l_hl; unprefixed_[0x6F] = &CPU::op_ld_l_a;

    // 0x7x — LD (HL)/A, r  (0x76 = HALT)
    unprefixed_[0x70] = &CPU::op_ld_hl_b; unprefixed_[0x71] = &CPU::op_ld_hl_c;
    unprefixed_[0x72] = &CPU::op_ld_hl_d; unprefixed_[0x73] = &CPU::op_ld_hl_e;
    unprefixed_[0x74] = &CPU::op_ld_hl_h; unprefixed_[0x75] = &CPU::op_ld_hl_l;
    unprefixed_[0x76] = &CPU::op_halt;
    unprefixed_[0x77] = &CPU::op_ld_hl_a;
    unprefixed_[0x78] = &CPU::op_ld_a_b;  unprefixed_[0x79] = &CPU::op_ld_a_c;
    unprefixed_[0x7A] = &CPU::op_ld_a_d;  unprefixed_[0x7B] = &CPU::op_ld_a_e;
    unprefixed_[0x7C] = &CPU::op_ld_a_h;  unprefixed_[0x7D] = &CPU::op_ld_a_l;
    unprefixed_[0x7E] = &CPU::op_ld_a_hl; unprefixed_[0x7F] = &CPU::op_ld_a_a;

    // 0x8x — ADD A / ADC A
    unprefixed_[0x80] = &CPU::op_add_a_b; unprefixed_[0x81] = &CPU::op_add_a_c;
    unprefixed_[0x82] = &CPU::op_add_a_d; unprefixed_[0x83] = &CPU::op_add_a_e;
    unprefixed_[0x84] = &CPU::op_add_a_h; unprefixed_[0x85] = &CPU::op_add_a_l;
    unprefixed_[0x86] = &CPU::op_add_a_hl;unprefixed_[0x87] = &CPU::op_add_a_a;
    unprefixed_[0x88] = &CPU::op_adc_a_b; unprefixed_[0x89] = &CPU::op_adc_a_c;
    unprefixed_[0x8A] = &CPU::op_adc_a_d; unprefixed_[0x8B] = &CPU::op_adc_a_e;
    unprefixed_[0x8C] = &CPU::op_adc_a_h; unprefixed_[0x8D] = &CPU::op_adc_a_l;
    unprefixed_[0x8E] = &CPU::op_adc_a_hl;unprefixed_[0x8F] = &CPU::op_adc_a_a;

    // 0x9x — SUB / SBC A
    unprefixed_[0x90] = &CPU::op_sub_b;   unprefixed_[0x91] = &CPU::op_sub_c;
    unprefixed_[0x92] = &CPU::op_sub_d;   unprefixed_[0x93] = &CPU::op_sub_e;
    unprefixed_[0x94] = &CPU::op_sub_h;   unprefixed_[0x95] = &CPU::op_sub_l;
    unprefixed_[0x96] = &CPU::op_sub_hl;  unprefixed_[0x97] = &CPU::op_sub_a;
    unprefixed_[0x98] = &CPU::op_sbc_a_b; unprefixed_[0x99] = &CPU::op_sbc_a_c;
    unprefixed_[0x9A] = &CPU::op_sbc_a_d; unprefixed_[0x9B] = &CPU::op_sbc_a_e;
    unprefixed_[0x9C] = &CPU::op_sbc_a_h; unprefixed_[0x9D] = &CPU::op_sbc_a_l;
    unprefixed_[0x9E] = &CPU::op_sbc_a_hl;unprefixed_[0x9F] = &CPU::op_sbc_a_a;

    // 0xAx — AND / XOR
    unprefixed_[0xA0] = &CPU::op_and_b;   unprefixed_[0xA1] = &CPU::op_and_c;
    unprefixed_[0xA2] = &CPU::op_and_d;   unprefixed_[0xA3] = &CPU::op_and_e;
    unprefixed_[0xA4] = &CPU::op_and_h;   unprefixed_[0xA5] = &CPU::op_and_l;
    unprefixed_[0xA6] = &CPU::op_and_hl;  unprefixed_[0xA7] = &CPU::op_and_a;
    unprefixed_[0xA8] = &CPU::op_xor_b;   unprefixed_[0xA9] = &CPU::op_xor_c;
    unprefixed_[0xAA] = &CPU::op_xor_d;   unprefixed_[0xAB] = &CPU::op_xor_e;
    unprefixed_[0xAC] = &CPU::op_xor_h;   unprefixed_[0xAD] = &CPU::op_xor_l;
    unprefixed_[0xAE] = &CPU::op_xor_hl;  unprefixed_[0xAF] = &CPU::op_xor_a;

    // 0xBx — OR / CP
    unprefixed_[0xB0] = &CPU::op_or_b;    unprefixed_[0xB1] = &CPU::op_or_c;
    unprefixed_[0xB2] = &CPU::op_or_d;    unprefixed_[0xB3] = &CPU::op_or_e;
    unprefixed_[0xB4] = &CPU::op_or_h;    unprefixed_[0xB5] = &CPU::op_or_l;
    unprefixed_[0xB6] = &CPU::op_or_hl;   unprefixed_[0xB7] = &CPU::op_or_a;
    unprefixed_[0xB8] = &CPU::op_cp_b;    unprefixed_[0xB9] = &CPU::op_cp_c;
    unprefixed_[0xBA] = &CPU::op_cp_d;    unprefixed_[0xBB] = &CPU::op_cp_e;
    unprefixed_[0xBC] = &CPU::op_cp_h;    unprefixed_[0xBD] = &CPU::op_cp_l;
    unprefixed_[0xBE] = &CPU::op_cp_hl;   unprefixed_[0xBF] = &CPU::op_cp_a;

    // 0xCx
    unprefixed_[0xC0] = &CPU::op_ret_nz;
    unprefixed_[0xC1] = &CPU::op_pop_bc;
    unprefixed_[0xC2] = &CPU::op_jp_nz_n16;
    unprefixed_[0xC3] = &CPU::op_jp_n16;
    unprefixed_[0xC4] = &CPU::op_call_nz_n16;
    unprefixed_[0xC5] = &CPU::op_push_bc;
    unprefixed_[0xC6] = &CPU::op_add_a_n8;
    unprefixed_[0xC7] = &CPU::op_rst_00;
    unprefixed_[0xC8] = &CPU::op_ret_z;
    unprefixed_[0xC9] = &CPU::op_ret;
    unprefixed_[0xCA] = &CPU::op_jp_z_n16;
    unprefixed_[0xCB] = &CPU::op_prefix_cb;  // handled specially in executeOpcode
    unprefixed_[0xCC] = &CPU::op_call_z_n16;
    unprefixed_[0xCD] = &CPU::op_call_n16;
    unprefixed_[0xCE] = &CPU::op_adc_a_n8;
    unprefixed_[0xCF] = &CPU::op_rst_08;

    // 0xDx  (0xD3, 0xDB, 0xDD are illegal — left nullptr)
    unprefixed_[0xD0] = &CPU::op_ret_nc;
    unprefixed_[0xD1] = &CPU::op_pop_de;
    unprefixed_[0xD2] = &CPU::op_jp_nc_n16;
    // 0xD3 = illegal
    unprefixed_[0xD4] = &CPU::op_call_nc_n16;
    unprefixed_[0xD5] = &CPU::op_push_de;
    unprefixed_[0xD6] = &CPU::op_sub_n8;
    unprefixed_[0xD7] = &CPU::op_rst_10;
    unprefixed_[0xD8] = &CPU::op_ret_c;
    unprefixed_[0xD9] = &CPU::op_reti;
    unprefixed_[0xDA] = &CPU::op_jp_c_n16;
    // 0xDB = illegal
    unprefixed_[0xDC] = &CPU::op_call_c_n16;
    // 0xDD = illegal
    unprefixed_[0xDE] = &CPU::op_sbc_a_n8;
    unprefixed_[0xDF] = &CPU::op_rst_18;

    // 0xEx  (0xE3, 0xE4, 0xEB, 0xEC, 0xED are illegal)
    unprefixed_[0xE0] = &CPU::op_ldh_a8_a;
    unprefixed_[0xE1] = &CPU::op_pop_hl;
    unprefixed_[0xE2] = &CPU::op_ldh_c_a;
    // 0xE3 = illegal
    // 0xE4 = illegal
    unprefixed_[0xE5] = &CPU::op_push_hl;
    unprefixed_[0xE6] = &CPU::op_and_n8;
    unprefixed_[0xE7] = &CPU::op_rst_20;
    unprefixed_[0xE8] = &CPU::op_add_sp_e8;
    unprefixed_[0xE9] = &CPU::op_jp_hl;
    unprefixed_[0xEA] = &CPU::op_ld_a16_a;
    // 0xEB = illegal
    // 0xEC = illegal
    // 0xED = illegal
    unprefixed_[0xEE] = &CPU::op_xor_n8;
    unprefixed_[0xEF] = &CPU::op_rst_28;

    // 0xFx  (0xF4, 0xFC, 0xFD are illegal)
    unprefixed_[0xF0] = &CPU::op_ldh_a_a8;
    unprefixed_[0xF1] = &CPU::op_pop_af;
    unprefixed_[0xF2] = &CPU::op_ldh_a_c;
    unprefixed_[0xF3] = &CPU::op_di;
    // 0xF4 = illegal
    unprefixed_[0xF5] = &CPU::op_push_af;
    unprefixed_[0xF6] = &CPU::op_or_n8;
    unprefixed_[0xF7] = &CPU::op_rst_30;
    unprefixed_[0xF8] = &CPU::op_ld_hl_sp_e8;
    unprefixed_[0xF9] = &CPU::op_ld_sp_hl;
    unprefixed_[0xFA] = &CPU::op_ld_a_a16;
    unprefixed_[0xFB] = &CPU::op_ei;
    // 0xFC = illegal
    // 0xFD = illegal
    unprefixed_[0xFE] = &CPU::op_cp_n8;
    unprefixed_[0xFF] = &CPU::op_rst_38;

    // ── CB-prefixed opcodes ──────────────────────────────────────────

    // 0x00–0x07: RLC
    cbprefixed_[0x00] = &CPU::cb_op_rlc_b;  cbprefixed_[0x01] = &CPU::cb_op_rlc_c;
    cbprefixed_[0x02] = &CPU::cb_op_rlc_d;  cbprefixed_[0x03] = &CPU::cb_op_rlc_e;
    cbprefixed_[0x04] = &CPU::cb_op_rlc_h;  cbprefixed_[0x05] = &CPU::cb_op_rlc_l;
    cbprefixed_[0x06] = &CPU::cb_op_rlc_hl; cbprefixed_[0x07] = &CPU::cb_op_rlc_a;

    // 0x08–0x0F: RRC
    cbprefixed_[0x08] = &CPU::cb_op_rrc_b;  cbprefixed_[0x09] = &CPU::cb_op_rrc_c;
    cbprefixed_[0x0A] = &CPU::cb_op_rrc_d;  cbprefixed_[0x0B] = &CPU::cb_op_rrc_e;
    cbprefixed_[0x0C] = &CPU::cb_op_rrc_h;  cbprefixed_[0x0D] = &CPU::cb_op_rrc_l;
    cbprefixed_[0x0E] = &CPU::cb_op_rrc_hl; cbprefixed_[0x0F] = &CPU::cb_op_rrc_a;

    // 0x10–0x17: RL
    cbprefixed_[0x10] = &CPU::cb_op_rl_b;   cbprefixed_[0x11] = &CPU::cb_op_rl_c;
    cbprefixed_[0x12] = &CPU::cb_op_rl_d;   cbprefixed_[0x13] = &CPU::cb_op_rl_e;
    cbprefixed_[0x14] = &CPU::cb_op_rl_h;   cbprefixed_[0x15] = &CPU::cb_op_rl_l;
    cbprefixed_[0x16] = &CPU::cb_op_rl_hl;  cbprefixed_[0x17] = &CPU::cb_op_rl_a;

    // 0x18–0x1F: RR
    cbprefixed_[0x18] = &CPU::cb_op_rr_b;   cbprefixed_[0x19] = &CPU::cb_op_rr_c;
    cbprefixed_[0x1A] = &CPU::cb_op_rr_d;   cbprefixed_[0x1B] = &CPU::cb_op_rr_e;
    cbprefixed_[0x1C] = &CPU::cb_op_rr_h;   cbprefixed_[0x1D] = &CPU::cb_op_rr_l;
    cbprefixed_[0x1E] = &CPU::cb_op_rr_hl;  cbprefixed_[0x1F] = &CPU::cb_op_rr_a;

    // 0x20–0x27: SLA
    cbprefixed_[0x20] = &CPU::cb_op_sla_b;  cbprefixed_[0x21] = &CPU::cb_op_sla_c;
    cbprefixed_[0x22] = &CPU::cb_op_sla_d;  cbprefixed_[0x23] = &CPU::cb_op_sla_e;
    cbprefixed_[0x24] = &CPU::cb_op_sla_h;  cbprefixed_[0x25] = &CPU::cb_op_sla_l;
    cbprefixed_[0x26] = &CPU::cb_op_sla_hl; cbprefixed_[0x27] = &CPU::cb_op_sla_a;

    // 0x28–0x2F: SRA
    cbprefixed_[0x28] = &CPU::cb_op_sra_b;  cbprefixed_[0x29] = &CPU::cb_op_sra_c;
    cbprefixed_[0x2A] = &CPU::cb_op_sra_d;  cbprefixed_[0x2B] = &CPU::cb_op_sra_e;
    cbprefixed_[0x2C] = &CPU::cb_op_sra_h;  cbprefixed_[0x2D] = &CPU::cb_op_sra_l;
    cbprefixed_[0x2E] = &CPU::cb_op_sra_hl; cbprefixed_[0x2F] = &CPU::cb_op_sra_a;

    // 0x30–0x37: SWAP
    cbprefixed_[0x30] = &CPU::cb_op_swap_b; cbprefixed_[0x31] = &CPU::cb_op_swap_c;
    cbprefixed_[0x32] = &CPU::cb_op_swap_d; cbprefixed_[0x33] = &CPU::cb_op_swap_e;
    cbprefixed_[0x34] = &CPU::cb_op_swap_h; cbprefixed_[0x35] = &CPU::cb_op_swap_l;
    cbprefixed_[0x36] = &CPU::cb_op_swap_hl;cbprefixed_[0x37] = &CPU::cb_op_swap_a;

    // 0x38–0x3F: SRL
    cbprefixed_[0x38] = &CPU::cb_op_srl_b;  cbprefixed_[0x39] = &CPU::cb_op_srl_c;
    cbprefixed_[0x3A] = &CPU::cb_op_srl_d;  cbprefixed_[0x3B] = &CPU::cb_op_srl_e;
    cbprefixed_[0x3C] = &CPU::cb_op_srl_h;  cbprefixed_[0x3D] = &CPU::cb_op_srl_l;
    cbprefixed_[0x3E] = &CPU::cb_op_srl_hl; cbprefixed_[0x3F] = &CPU::cb_op_srl_a;

    // 0x40–0x7F: BIT 0-7
    cbprefixed_[0x40]=&CPU::cb_op_bit_0_b; cbprefixed_[0x41]=&CPU::cb_op_bit_0_c;
    cbprefixed_[0x42]=&CPU::cb_op_bit_0_d; cbprefixed_[0x43]=&CPU::cb_op_bit_0_e;
    cbprefixed_[0x44]=&CPU::cb_op_bit_0_h; cbprefixed_[0x45]=&CPU::cb_op_bit_0_l;
    cbprefixed_[0x46]=&CPU::cb_op_bit_0_hl;cbprefixed_[0x47]=&CPU::cb_op_bit_0_a;
    cbprefixed_[0x48]=&CPU::cb_op_bit_1_b; cbprefixed_[0x49]=&CPU::cb_op_bit_1_c;
    cbprefixed_[0x4A]=&CPU::cb_op_bit_1_d; cbprefixed_[0x4B]=&CPU::cb_op_bit_1_e;
    cbprefixed_[0x4C]=&CPU::cb_op_bit_1_h; cbprefixed_[0x4D]=&CPU::cb_op_bit_1_l;
    cbprefixed_[0x4E]=&CPU::cb_op_bit_1_hl;cbprefixed_[0x4F]=&CPU::cb_op_bit_1_a;
    cbprefixed_[0x50]=&CPU::cb_op_bit_2_b; cbprefixed_[0x51]=&CPU::cb_op_bit_2_c;
    cbprefixed_[0x52]=&CPU::cb_op_bit_2_d; cbprefixed_[0x53]=&CPU::cb_op_bit_2_e;
    cbprefixed_[0x54]=&CPU::cb_op_bit_2_h; cbprefixed_[0x55]=&CPU::cb_op_bit_2_l;
    cbprefixed_[0x56]=&CPU::cb_op_bit_2_hl;cbprefixed_[0x57]=&CPU::cb_op_bit_2_a;
    cbprefixed_[0x58]=&CPU::cb_op_bit_3_b; cbprefixed_[0x59]=&CPU::cb_op_bit_3_c;
    cbprefixed_[0x5A]=&CPU::cb_op_bit_3_d; cbprefixed_[0x5B]=&CPU::cb_op_bit_3_e;
    cbprefixed_[0x5C]=&CPU::cb_op_bit_3_h; cbprefixed_[0x5D]=&CPU::cb_op_bit_3_l;
    cbprefixed_[0x5E]=&CPU::cb_op_bit_3_hl;cbprefixed_[0x5F]=&CPU::cb_op_bit_3_a;
    cbprefixed_[0x60]=&CPU::cb_op_bit_4_b; cbprefixed_[0x61]=&CPU::cb_op_bit_4_c;
    cbprefixed_[0x62]=&CPU::cb_op_bit_4_d; cbprefixed_[0x63]=&CPU::cb_op_bit_4_e;
    cbprefixed_[0x64]=&CPU::cb_op_bit_4_h; cbprefixed_[0x65]=&CPU::cb_op_bit_4_l;
    cbprefixed_[0x66]=&CPU::cb_op_bit_4_hl;cbprefixed_[0x67]=&CPU::cb_op_bit_4_a;
    cbprefixed_[0x68]=&CPU::cb_op_bit_5_b; cbprefixed_[0x69]=&CPU::cb_op_bit_5_c;
    cbprefixed_[0x6A]=&CPU::cb_op_bit_5_d; cbprefixed_[0x6B]=&CPU::cb_op_bit_5_e;
    cbprefixed_[0x6C]=&CPU::cb_op_bit_5_h; cbprefixed_[0x6D]=&CPU::cb_op_bit_5_l;
    cbprefixed_[0x6E]=&CPU::cb_op_bit_5_hl;cbprefixed_[0x6F]=&CPU::cb_op_bit_5_a;
    cbprefixed_[0x70]=&CPU::cb_op_bit_6_b; cbprefixed_[0x71]=&CPU::cb_op_bit_6_c;
    cbprefixed_[0x72]=&CPU::cb_op_bit_6_d; cbprefixed_[0x73]=&CPU::cb_op_bit_6_e;
    cbprefixed_[0x74]=&CPU::cb_op_bit_6_h; cbprefixed_[0x75]=&CPU::cb_op_bit_6_l;
    cbprefixed_[0x76]=&CPU::cb_op_bit_6_hl;cbprefixed_[0x77]=&CPU::cb_op_bit_6_a;
    cbprefixed_[0x78]=&CPU::cb_op_bit_7_b; cbprefixed_[0x79]=&CPU::cb_op_bit_7_c;
    cbprefixed_[0x7A]=&CPU::cb_op_bit_7_d; cbprefixed_[0x7B]=&CPU::cb_op_bit_7_e;
    cbprefixed_[0x7C]=&CPU::cb_op_bit_7_h; cbprefixed_[0x7D]=&CPU::cb_op_bit_7_l;
    cbprefixed_[0x7E]=&CPU::cb_op_bit_7_hl;cbprefixed_[0x7F]=&CPU::cb_op_bit_7_a;

    // 0x80–0xBF: RES 0-7
    cbprefixed_[0x80]=&CPU::cb_op_res_0_b; cbprefixed_[0x81]=&CPU::cb_op_res_0_c;
    cbprefixed_[0x82]=&CPU::cb_op_res_0_d; cbprefixed_[0x83]=&CPU::cb_op_res_0_e;
    cbprefixed_[0x84]=&CPU::cb_op_res_0_h; cbprefixed_[0x85]=&CPU::cb_op_res_0_l;
    cbprefixed_[0x86]=&CPU::cb_op_res_0_hl;cbprefixed_[0x87]=&CPU::cb_op_res_0_a;
    cbprefixed_[0x88]=&CPU::cb_op_res_1_b; cbprefixed_[0x89]=&CPU::cb_op_res_1_c;
    cbprefixed_[0x8A]=&CPU::cb_op_res_1_d; cbprefixed_[0x8B]=&CPU::cb_op_res_1_e;
    cbprefixed_[0x8C]=&CPU::cb_op_res_1_h; cbprefixed_[0x8D]=&CPU::cb_op_res_1_l;
    cbprefixed_[0x8E]=&CPU::cb_op_res_1_hl;cbprefixed_[0x8F]=&CPU::cb_op_res_1_a;
    cbprefixed_[0x90]=&CPU::cb_op_res_2_b; cbprefixed_[0x91]=&CPU::cb_op_res_2_c;
    cbprefixed_[0x92]=&CPU::cb_op_res_2_d; cbprefixed_[0x93]=&CPU::cb_op_res_2_e;
    cbprefixed_[0x94]=&CPU::cb_op_res_2_h; cbprefixed_[0x95]=&CPU::cb_op_res_2_l;
    cbprefixed_[0x96]=&CPU::cb_op_res_2_hl;cbprefixed_[0x97]=&CPU::cb_op_res_2_a;
    cbprefixed_[0x98]=&CPU::cb_op_res_3_b; cbprefixed_[0x99]=&CPU::cb_op_res_3_c;
    cbprefixed_[0x9A]=&CPU::cb_op_res_3_d; cbprefixed_[0x9B]=&CPU::cb_op_res_3_e;
    cbprefixed_[0x9C]=&CPU::cb_op_res_3_h; cbprefixed_[0x9D]=&CPU::cb_op_res_3_l;
    cbprefixed_[0x9E]=&CPU::cb_op_res_3_hl;cbprefixed_[0x9F]=&CPU::cb_op_res_3_a;
    cbprefixed_[0xA0]=&CPU::cb_op_res_4_b; cbprefixed_[0xA1]=&CPU::cb_op_res_4_c;
    cbprefixed_[0xA2]=&CPU::cb_op_res_4_d; cbprefixed_[0xA3]=&CPU::cb_op_res_4_e;
    cbprefixed_[0xA4]=&CPU::cb_op_res_4_h; cbprefixed_[0xA5]=&CPU::cb_op_res_4_l;
    cbprefixed_[0xA6]=&CPU::cb_op_res_4_hl;cbprefixed_[0xA7]=&CPU::cb_op_res_4_a;
    cbprefixed_[0xA8]=&CPU::cb_op_res_5_b; cbprefixed_[0xA9]=&CPU::cb_op_res_5_c;
    cbprefixed_[0xAA]=&CPU::cb_op_res_5_d; cbprefixed_[0xAB]=&CPU::cb_op_res_5_e;
    cbprefixed_[0xAC]=&CPU::cb_op_res_5_h; cbprefixed_[0xAD]=&CPU::cb_op_res_5_l;
    cbprefixed_[0xAE]=&CPU::cb_op_res_5_hl;cbprefixed_[0xAF]=&CPU::cb_op_res_5_a;
    cbprefixed_[0xB0]=&CPU::cb_op_res_6_b; cbprefixed_[0xB1]=&CPU::cb_op_res_6_c;
    cbprefixed_[0xB2]=&CPU::cb_op_res_6_d; cbprefixed_[0xB3]=&CPU::cb_op_res_6_e;
    cbprefixed_[0xB4]=&CPU::cb_op_res_6_h; cbprefixed_[0xB5]=&CPU::cb_op_res_6_l;
    cbprefixed_[0xB6]=&CPU::cb_op_res_6_hl;cbprefixed_[0xB7]=&CPU::cb_op_res_6_a;
    cbprefixed_[0xB8]=&CPU::cb_op_res_7_b; cbprefixed_[0xB9]=&CPU::cb_op_res_7_c;
    cbprefixed_[0xBA]=&CPU::cb_op_res_7_d; cbprefixed_[0xBB]=&CPU::cb_op_res_7_e;
    cbprefixed_[0xBC]=&CPU::cb_op_res_7_h; cbprefixed_[0xBD]=&CPU::cb_op_res_7_l;
    cbprefixed_[0xBE]=&CPU::cb_op_res_7_hl;cbprefixed_[0xBF]=&CPU::cb_op_res_7_a;

    // 0xC0–0xFF: SET 0-7
    cbprefixed_[0xC0]=&CPU::cb_op_set_0_b; cbprefixed_[0xC1]=&CPU::cb_op_set_0_c;
    cbprefixed_[0xC2]=&CPU::cb_op_set_0_d; cbprefixed_[0xC3]=&CPU::cb_op_set_0_e;
    cbprefixed_[0xC4]=&CPU::cb_op_set_0_h; cbprefixed_[0xC5]=&CPU::cb_op_set_0_l;
    cbprefixed_[0xC6]=&CPU::cb_op_set_0_hl;cbprefixed_[0xC7]=&CPU::cb_op_set_0_a;
    cbprefixed_[0xC8]=&CPU::cb_op_set_1_b; cbprefixed_[0xC9]=&CPU::cb_op_set_1_c;
    cbprefixed_[0xCA]=&CPU::cb_op_set_1_d; cbprefixed_[0xCB]=&CPU::cb_op_set_1_e;
    cbprefixed_[0xCC]=&CPU::cb_op_set_1_h; cbprefixed_[0xCD]=&CPU::cb_op_set_1_l;
    cbprefixed_[0xCE]=&CPU::cb_op_set_1_hl;cbprefixed_[0xCF]=&CPU::cb_op_set_1_a;
    cbprefixed_[0xD0]=&CPU::cb_op_set_2_b; cbprefixed_[0xD1]=&CPU::cb_op_set_2_c;
    cbprefixed_[0xD2]=&CPU::cb_op_set_2_d; cbprefixed_[0xD3]=&CPU::cb_op_set_2_e;
    cbprefixed_[0xD4]=&CPU::cb_op_set_2_h; cbprefixed_[0xD5]=&CPU::cb_op_set_2_l;
    cbprefixed_[0xD6]=&CPU::cb_op_set_2_hl;cbprefixed_[0xD7]=&CPU::cb_op_set_2_a;
    cbprefixed_[0xD8]=&CPU::cb_op_set_3_b; cbprefixed_[0xD9]=&CPU::cb_op_set_3_c;
    cbprefixed_[0xDA]=&CPU::cb_op_set_3_d; cbprefixed_[0xDB]=&CPU::cb_op_set_3_e;
    cbprefixed_[0xDC]=&CPU::cb_op_set_3_h; cbprefixed_[0xDD]=&CPU::cb_op_set_3_l;
    cbprefixed_[0xDE]=&CPU::cb_op_set_3_hl;cbprefixed_[0xDF]=&CPU::cb_op_set_3_a;
    cbprefixed_[0xE0]=&CPU::cb_op_set_4_b; cbprefixed_[0xE1]=&CPU::cb_op_set_4_c;
    cbprefixed_[0xE2]=&CPU::cb_op_set_4_d; cbprefixed_[0xE3]=&CPU::cb_op_set_4_e;
    cbprefixed_[0xE4]=&CPU::cb_op_set_4_h; cbprefixed_[0xE5]=&CPU::cb_op_set_4_l;
    cbprefixed_[0xE6]=&CPU::cb_op_set_4_hl;cbprefixed_[0xE7]=&CPU::cb_op_set_4_a;
    cbprefixed_[0xE8]=&CPU::cb_op_set_5_b; cbprefixed_[0xE9]=&CPU::cb_op_set_5_c;
    cbprefixed_[0xEA]=&CPU::cb_op_set_5_d; cbprefixed_[0xEB]=&CPU::cb_op_set_5_e;
    cbprefixed_[0xEC]=&CPU::cb_op_set_5_h; cbprefixed_[0xED]=&CPU::cb_op_set_5_l;
    cbprefixed_[0xEE]=&CPU::cb_op_set_5_hl;cbprefixed_[0xEF]=&CPU::cb_op_set_5_a;
    cbprefixed_[0xF0]=&CPU::cb_op_set_6_b; cbprefixed_[0xF1]=&CPU::cb_op_set_6_c;
    cbprefixed_[0xF2]=&CPU::cb_op_set_6_d; cbprefixed_[0xF3]=&CPU::cb_op_set_6_e;
    cbprefixed_[0xF4]=&CPU::cb_op_set_6_h; cbprefixed_[0xF5]=&CPU::cb_op_set_6_l;
    cbprefixed_[0xF6]=&CPU::cb_op_set_6_hl;cbprefixed_[0xF7]=&CPU::cb_op_set_6_a;
    cbprefixed_[0xF8]=&CPU::cb_op_set_7_b; cbprefixed_[0xF9]=&CPU::cb_op_set_7_c;
    cbprefixed_[0xFA]=&CPU::cb_op_set_7_d; cbprefixed_[0xFB]=&CPU::cb_op_set_7_e;
    cbprefixed_[0xFC]=&CPU::cb_op_set_7_h; cbprefixed_[0xFD]=&CPU::cb_op_set_7_l;
    cbprefixed_[0xFE]=&CPU::cb_op_set_7_hl;cbprefixed_[0xFF]=&CPU::cb_op_set_7_a;
}
