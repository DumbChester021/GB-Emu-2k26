#include "timer.h"

// ══════════════════════════════════════════════════════════════════════
// Bit position in sysCounter_ for each TAC clock-select value
// ══════════════════════════════════════════════════════════════════════
//   TAC bits 1-0 │ Bit │ Frequency  │ Period (T-cycles)
//   ─────────────┼─────┼────────────┼───────────────────
//        00      │  9  │   4096 Hz  │ 1024
//        01      │  3  │ 262144 Hz  │   16
//        10      │  5  │  65536 Hz  │   64
//        11      │  7  │  16384 Hz  │  256

int Timer::selectedBitPos() const {
    static constexpr int bitTable[4] = { 9, 3, 5, 7 };
    return bitTable[tac_ & 0x03];
}

bool Timer::currentAndResult() const {
    bool enable = (tac_ & 0x04) != 0;
    bool selectedBit = (sysCounter_ >> selectedBitPos()) & 1;
    return enable && selectedBit;
}

// ══════════════════════════════════════════════════════════════════════
// TIMA increment (called on falling edge)
// ══════════════════════════════════════════════════════════════════════

void Timer::incrementTIMA() {
    tima_++;
    if (tima_ == 0) {
        // Overflow! Start the 4-cycle delay
        overflowPending_ = true;
        overflowCountdown_ = 4;
    }
}

void Timer::checkFallingEdge() {
    bool current = currentAndResult();
    if (prevAndResult_ && !current) {
        incrementTIMA();
    }
    prevAndResult_ = current;
}

// ══════════════════════════════════════════════════════════════════════
// tick() — advance 1 T-cycle
// ══════════════════════════════════════════════════════════════════════

void Timer::tick() {
    // Clear the "reloaded this cycle" flag from the previous cycle
    reloadedThisCycle_ = false;

    // ── Handle TIMA overflow delay ───────────────────────────────────
    if (overflowPending_) {
        overflowCountdown_--;
        if (overflowCountdown_ <= 0) {
            // Reload TIMA from TMA and fire Timer interrupt
            tima_ = tma_;
            if (ifReg_) {
                *ifReg_ |= 0x04; // IF bit 2 = Timer
            }
            overflowPending_ = false;
            reloadedThisCycle_ = true;
        }
    }

    // ── Increment the 16-bit system counter ──────────────────────────
    sysCounter_++;

    // ── Check for falling edge → TIMA increment ─────────────────────
    checkFallingEdge();
}

// ══════════════════════════════════════════════════════════════════════
// Register reads
// ══════════════════════════════════════════════════════════════════════

uint8_t Timer::read(uint16_t addr) const {
    switch (addr) {
        case 0xFF04: return static_cast<uint8_t>(sysCounter_ >> 8); // DIV
        case 0xFF05: return tima_;
        case 0xFF06: return tma_;
        case 0xFF07: return tac_ | 0xF8; // Upper 5 bits read as 1
        default:     return 0xFF;
    }
}

// ══════════════════════════════════════════════════════════════════════
// Register writes
// ══════════════════════════════════════════════════════════════════════

void Timer::write(uint16_t addr, uint8_t val) {
    switch (addr) {
        case 0xFF04: {
            // Writing ANY value to DIV resets the entire 16-bit counter.
            // This can cause a falling edge if the selected bit was 1.
            sysCounter_ = 0;
            checkFallingEdge();
            break;
        }

        case 0xFF05: {
            // Writing to TIMA during the overflow delay window cancels
            // the pending TMA reload.
            if (overflowPending_) {
                overflowPending_ = false;
            }
            tima_ = val;
            break;
        }

        case 0xFF06: {
            tma_ = val;
            // If TMA is written on the exact cycle where TIMA was just
            // reloaded from TMA, the new value also goes into TIMA.
            if (reloadedThisCycle_) {
                tima_ = val;
            }
            break;
        }

        case 0xFF07: {
            // Changing TAC can trigger a falling edge.
            // We need to check with the OLD tac_ value vs the NEW one.
            uint8_t oldTac = tac_;
            tac_ = val & 0x07; // Only lower 3 bits are writable

            // Detect falling edge from the config change:
            // Old AND result was computed with old tac_, new AND result
            // uses the new tac_. If old=1 and new=0, TIMA increments.
            // prevAndResult_ still holds the old value, and
            // currentAndResult() now uses the new tac_.
            checkFallingEdge();
            (void)oldTac;
            break;
        }
    }
}
