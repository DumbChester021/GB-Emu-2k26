#pragma once

#include <cstdint>

// ══════════════════════════════════════════════════════════════════════
// Hardware-accurate Game Boy DMG Timer
//
// Registers:
//   FF04  DIV   — upper 8 bits of internal 16-bit system counter
//   FF05  TIMA  — timer counter (incremented by falling-edge detection)
//   FF06  TMA   — timer modulo (loaded into TIMA on overflow, after 4-cycle delay)
//   FF07  TAC   — timer control (bit2=enable, bits1-0=clock select)
//
// Key behaviors modeled:
//   • TIMA clocks on the falling edge of (selected_bit AND enable)
//   • Writing to DIV resets the 16-bit counter → may trigger TIMA increment
//   • Changing TAC may trigger TIMA increment via falling edge
//   • TIMA overflow has a 4 T-cycle delay before TMA reload + interrupt
//   • Writing TIMA during the delay window cancels the reload
//   • Writing TMA during the delay window also updates the pending reload value
// ══════════════════════════════════════════════════════════════════════

class Timer {
public:
    // Provide a pointer to the IF register (FF0F) in IO memory so the
    // timer can set bit 2 (Timer interrupt) directly.
    void connectIF(uint8_t* ifReg) { ifReg_ = ifReg; }

    // Advance the timer by exactly 1 T-cycle.
    void tick();

    // Memory-mapped register access (addr must be 0xFF04–0xFF07).
    uint8_t read(uint16_t addr) const;
    void    write(uint16_t addr, uint8_t val);

private:
    // ── Internal state ───────────────────────────────────────────────
    uint16_t sysCounter_ = 0;   // 16-bit internal counter (DIV = upper 8 bits)

    uint8_t tima_ = 0;          // FF05 — Timer counter
    uint8_t tma_  = 0;          // FF06 — Timer modulo
    uint8_t tac_  = 0;          // FF07 — Timer control

    // ── Falling-edge detection ───────────────────────────────────────
    // We track the AND of the selected counter bit and the enable flag.
    // TIMA increments when this goes from 1 → 0.
    bool prevAndResult_ = false;

    // ── TIMA overflow delay (4 T-cycles) ─────────────────────────────
    // When TIMA overflows:
    //   - overflowCountdown_ is set to 4
    //   - After 4 cycles: TIMA = TMA, IF bit 2 is set
    //   - Writing TIMA during this window cancels the reload
    int  overflowCountdown_ = 0;
    bool overflowPending_   = false;
    bool reloadedThisCycle_ = false;  // True on the cycle TIMA was reloaded from TMA

    // ── IF register reference ────────────────────────────────────────
    uint8_t* ifReg_ = nullptr;

    // ── Helpers ──────────────────────────────────────────────────────

    // Which bit of sysCounter_ does the current TAC clock-select use?
    int selectedBitPos() const;

    // Get the current AND result: selected_bit & timer_enable
    bool currentAndResult() const;

    // Called when we detect a falling edge → increment TIMA
    void incrementTIMA();

    // Check and handle falling edge (call after any state change)
    void checkFallingEdge();
};
