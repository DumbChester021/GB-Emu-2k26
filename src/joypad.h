#pragma once

#include <cstdint>

class SaveState;

// ══════════════════════════════════════════════════════════════════════
//  Joypad — hardware-accurate Game Boy joypad (FF00 / P1/JOYP)
//
//  The DMG joypad matrix is a 2×4 multiplexer:
//
//        Bit 3    Bit 2    Bit 1    Bit 0
//  P14 (bit 4 low = direction): Down   Up     Left   Right
//  P15 (bit 5 low = action):    Start  Select B      A
//
//  All button lines are active-low: 0 = pressed, 1 = released.
//  Bits 7–6 are unused and always read as 1.
// ══════════════════════════════════════════════════════════════════════

class Joypad {
public:
    // Individual button identifiers
    enum class Button : uint8_t {
        Right  = 0,
        Left   = 1,
        Up     = 2,
        Down   = 3,
        A      = 4,
        B      = 5,
        Select = 6,
        Start  = 7,
    };

    void connectIF(uint8_t* ifReg) { if_ = ifReg; }

    // Save state serialization
    void serialize(SaveState& ss) const;
    void deserialize(SaveState& ss);

    // ── Frontend interface ─────────────────────────────────────────
    // Called by the SDL layer to press or release a button.
    void setButton(Button btn, bool pressed) {
        uint8_t bit;
        if (static_cast<uint8_t>(btn) < 4) {
            // Direction button (Right/Left/Up/Down → bits 0-3)
            bit = 1u << static_cast<uint8_t>(btn);
            if (pressed)
                dirButtons_ &= ~bit;   // active-low: clear = pressed
            else
                dirButtons_ |= bit;    // set = released
        } else {
            // Action button (A/B/Select/Start → bits 0-3)
            bit = 1u << (static_cast<uint8_t>(btn) - 4);
            if (pressed)
                actionButtons_ &= ~bit;
            else
                actionButtons_ |= bit;
        }
    }

    // ── P1 register read (FF00) ────────────────────────────────────
    // Returns the current P1 value based on column selection.
    uint8_t readP1() const {
        uint8_t result = select_ | 0xC0; // Bits 7-6 always 1

        // Mix in the button lines for selected column(s)
        uint8_t lines = 0x0F; // Default: all released (high)
        if (!(select_ & 0x10)) lines &= dirButtons_;    // P14 selected
        if (!(select_ & 0x20)) lines &= actionButtons_; // P15 selected

        return result | lines;
    }

    // ── P1 register write (FF00) ───────────────────────────────────
    // Only bits 5–4 (column select) are writable.
    // Writing may make previously-invisible pressed buttons visible,
    // which should trigger a joypad interrupt (IF bit 4).
    void writeP1(uint8_t val) {
        uint8_t oldP1 = readP1();
        select_ = val & 0x30; // Keep only bits 5-4
        uint8_t newP1 = readP1();

        // Joypad interrupt: triggered on a HIGH→LOW transition on any
        // of the lower 4 bits (a button becoming "visible"/"pressed").
        uint8_t fallingEdge = (oldP1 & ~newP1) & 0x0F;
        if (fallingEdge && if_) {
            *if_ |= 0x10; // IF bit 4 = Joypad interrupt
        }
    }

private:
    uint8_t* if_ = nullptr;

    // Column select bits (bits 5-4 of P1).
    // On power-up, both columns are deselected (bits set = high).
    uint8_t select_ = 0x30;

    // Button state — active-low nibbles (0x0F = all released)
    uint8_t dirButtons_    = 0x0F; // Right, Left, Up, Down
    uint8_t actionButtons_ = 0x0F; // A, B, Select, Start
};
