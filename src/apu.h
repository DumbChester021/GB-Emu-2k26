#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>

class SaveState;

// ══════════════════════════════════════════════════════════════════════
// Hardware-Accurate Game Boy DMG APU
//
// 4 channels:
//   CH1 — Pulse with sweep  (NR10-NR14, FF10-FF14)
//   CH2 — Pulse              (NR21-NR24, FF16-FF19)
//   CH3 — Wave               (NR30-NR34, FF1A-FF1E, wave RAM FF30-FF3F)
//   CH4 — Noise              (NR41-NR44, FF20-FF23)
//
// Control:
//   NR50 (FF24) — Master volume / VIN routing
//   NR51 (FF25) — Channel panning (L/R enable per channel)
//   NR52 (FF26) — Master power / channel enable status
//
// Clocked at 4.194304 MHz (1 T-cycle). Frame sequencer runs at 512 Hz
// (every 8192 T-cycles), driven by DIV bit 4 falling edge.
//
// Output: stereo float samples at 44100 Hz via lock-free ring buffer.
// ══════════════════════════════════════════════════════════════════════

class APU {
public:
    // ── Wiring ──────────────────────────────────────────────────────
    void connectIF(uint8_t* ifReg) { ifReg_ = ifReg; }

    // ── Tick — call once per T-cycle ─────────────────────────────────
    void tick();

    // ── Register access ─────────────────────────────────────────────
    uint8_t readReg(uint16_t addr) const;
    void    writeReg(uint16_t addr, uint8_t val);

    // ── Save state ──────────────────────────────────────────────────
    void serialize(SaveState& ss) const;
    void deserialize(SaveState& ss);

    // ── Audio output ring buffer ────────────────────────────────────
    // Lock-free SPSC ring buffer. SDL callback is consumer, tick() is producer.
    static constexpr int SAMPLE_RATE = 44100;
    static constexpr int BUFFER_SIZE = 8192;  // Must be power of 2

    struct StereoSample {
        float left;
        float right;
    };

    // Returns number of samples available for reading
    int samplesAvailable() const {
        int w = writePos_.load(std::memory_order_acquire);
        int r = readPos_.load(std::memory_order_relaxed);
        return (w - r + BUFFER_SIZE) & (BUFFER_SIZE - 1);
    }

    // Read samples into output buffer. Returns number actually read.
    int readSamples(StereoSample* out, int count) {
        int avail = samplesAvailable();
        if (count > avail) count = avail;
        int r = readPos_.load(std::memory_order_relaxed);
        for (int i = 0; i < count; i++) {
            out[i] = sampleBuffer_[(r + i) & (BUFFER_SIZE - 1)];
        }
        readPos_.store((r + count) & (BUFFER_SIZE - 1), std::memory_order_release);
        return count;
    }

private:
    // ── IF register pointer (for future use if APU generates IRQs) ──
    uint8_t* ifReg_ = nullptr;

    // ── Master power ────────────────────────────────────────────────
    bool powered_ = false;

    // ── NR50/NR51/NR52 registers ────────────────────────────────────
    uint8_t nr50_ = 0x77;   // Master volume
    uint8_t nr51_ = 0xF3;   // Channel panning
    uint8_t nr52_ = 0xF1;   // Power + channel status (lower 4 bits read-only)

    // ══════════════════════════════════════════════════════════════════
    // Channel 1 — Pulse with Sweep
    // ══════════════════════════════════════════════════════════════════
    struct PulseChannel {
        // Registers
        uint8_t sweep = 0;       // NR10: sweep period, negate, shift
        uint8_t dutyLength = 0;  // NR11/NR21: duty cycle + length load
        uint8_t envelope = 0;    // NR12/NR22: volume, direction, period
        uint8_t freqLo = 0;     // NR13/NR23: frequency low 8 bits
        uint8_t freqHi = 0;     // NR14/NR24: trigger, length enable, freq high 3 bits

        // Internal state
        bool enabled = false;
        bool dacEnabled = false;
        int lengthCounter = 0;   // 0-64
        int frequencyTimer = 0;  // Counts down, reload from (2048 - freq) * 4
        int dutyPosition = 0;    // 0-7 position in duty cycle

        // Volume envelope
        int currentVolume = 0;   // 0-15
        int envelopeTimer = 0;
        bool envelopeRunning = false;

        // Sweep (CH1 only)
        bool sweepEnabled = false;
        int sweepTimer = 0;
        int sweepShadowFreq = 0;
        bool sweepNegateUsed = false;

        // Returns current output (0 or currentVolume based on duty)
        int output() const;
        void tickFrequency();
        void tickLengthCounter();
        void tickVolumeEnvelope();
        void tickSweep(bool& channelEnable);
        void trigger(bool isCh1);

    private:
        int calcSweepFreq() const;

        // Duty cycle waveforms
        static constexpr uint8_t DUTY_TABLE[4] = {
            0b00000001,  // 12.5%
            0b00000011,  // 25%
            0b00001111,  // 50%
            0b11111100,  // 75%
        };
    };

    // ══════════════════════════════════════════════════════════════════
    // Channel 3 — Wave
    // ══════════════════════════════════════════════════════════════════
    struct WaveChannel {
        // Registers
        uint8_t dacPower = 0;    // NR30: DAC enable (bit 7)
        uint8_t length = 0;      // NR31: length load
        uint8_t volumeCode = 0;  // NR32: volume shift (bits 5-6)
        uint8_t freqLo = 0;     // NR33: frequency low 8 bits
        uint8_t freqHi = 0;     // NR34: trigger, length enable, freq high 3 bits

        // Wave RAM (16 bytes = 32 nibbles)
        std::array<uint8_t, 16> waveRAM = {};

        // Internal state
        bool enabled = false;
        bool dacEnabled = false;
        int lengthCounter = 0;   // 0-256
        int frequencyTimer = 0;  // Counts down, reload from (2048 - freq) * 2
        int wavePosition = 0;    // 0-31 position in wave table

        // The byte currently being read by the wave channel
        uint8_t currentSample = 0;

        // DMG: wave RAM access window (reads/writes only work within
        // a few clocks of the frequency timer clocking)
        int waveJustAccessed = 0;

        int output() const;
        void tickFrequency();
        void tickLengthCounter();
        void trigger();
    };

    // ══════════════════════════════════════════════════════════════════
    // Channel 4 — Noise
    // ══════════════════════════════════════════════════════════════════
    struct NoiseChannel {
        // Registers
        uint8_t length = 0;      // NR41: length load
        uint8_t envelope = 0;    // NR42: volume, direction, period
        uint8_t polynomial = 0;  // NR43: clock shift, width mode, divisor
        uint8_t control = 0;     // NR44: trigger, length enable

        // Internal state
        bool enabled = false;
        bool dacEnabled = false;
        int lengthCounter = 0;   // 0-64
        int frequencyTimer = 0;
        uint16_t lfsr = 0x7FFF;  // 15-bit Linear Feedback Shift Register

        // Volume envelope
        int currentVolume = 0;
        int envelopeTimer = 0;
        bool envelopeRunning = false;

        int output() const;
        void tickFrequency();
        void tickLengthCounter();
        void tickVolumeEnvelope();
        void trigger();
    };

    // ── Channel instances ───────────────────────────────────────────
    PulseChannel ch1_;   // Channel 1 (with sweep)
    PulseChannel ch2_;   // Channel 2 (no sweep)
    WaveChannel  ch3_;   // Channel 3
    NoiseChannel ch4_;   // Channel 4

    // ── Frame sequencer ─────────────────────────────────────────────
    int frameSequencerStep_ = 0;    // 0-7
    int frameSequencerClock_ = 0;   // T-cycle counter (resets at 8192)

    // ── Downsample counter ──────────────────────────────────────────
    // Accumulate T-cycles, output one stereo sample every ~95.1 T-cycles
    static constexpr int CPU_CLOCK = 4194304;
    double sampleCounter_ = 0.0;
    static constexpr double CYCLES_PER_SAMPLE =
        static_cast<double>(CPU_CLOCK) / static_cast<double>(SAMPLE_RATE);

    // ── High-pass filter state (DC offset removal) ──────────────────
    float hpfCapacitorL_ = 0.0f;
    float hpfCapacitorR_ = 0.0f;
    static constexpr float HPF_CHARGE_FACTOR = 0.999958f; // ~4 Hz cutoff at 44100 Hz

    // ── Output ring buffer ──────────────────────────────────────────
    std::array<StereoSample, BUFFER_SIZE> sampleBuffer_ = {};
    std::atomic<int> writePos_{0};
    std::atomic<int> readPos_{0};

    void pushSample(float left, float right);

    // ── Helpers ──────────────────────────────────────────────────────
    void stepFrameSequencer();
    void powerOff();
    void powerOn();

    // Read masks for APU registers (unused bits read as 1)
    static uint8_t readMask(uint16_t addr);
};
