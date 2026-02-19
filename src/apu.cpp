#include "apu.h"
#include <algorithm>
#include <cmath>

// ══════════════════════════════════════════════════════════════════════
// Read masks for APU registers — unused bits read as 1
// ══════════════════════════════════════════════════════════════════════

uint8_t APU::readMask(uint16_t addr) {
    switch (addr) {
        case 0xFF10: return 0x80;  // NR10
        case 0xFF11: return 0x3F;  // NR11 (duty readable, length not)
        case 0xFF12: return 0x00;  // NR12
        case 0xFF13: return 0xFF;  // NR13 (write-only)
        case 0xFF14: return 0xBF;  // NR14 (bit 6 readable)
        case 0xFF15: return 0xFF;  // Unused
        case 0xFF16: return 0x3F;  // NR21
        case 0xFF17: return 0x00;  // NR22
        case 0xFF18: return 0xFF;  // NR23 (write-only)
        case 0xFF19: return 0xBF;  // NR24
        case 0xFF1A: return 0x7F;  // NR30
        case 0xFF1B: return 0xFF;  // NR31 (write-only)
        case 0xFF1C: return 0x9F;  // NR32
        case 0xFF1D: return 0xFF;  // NR33 (write-only)
        case 0xFF1E: return 0xBF;  // NR34
        case 0xFF1F: return 0xFF;  // Unused
        case 0xFF20: return 0xFF;  // NR41 (write-only)
        case 0xFF21: return 0x00;  // NR42
        case 0xFF22: return 0x00;  // NR43
        case 0xFF23: return 0xBF;  // NR44
        case 0xFF24: return 0x00;  // NR50
        case 0xFF25: return 0x00;  // NR51
        case 0xFF26: return 0x70;  // NR52 (bits 4-6 unused)
        default:     return 0xFF;
    }
}

// ══════════════════════════════════════════════════════════════════════
// PulseChannel implementation
// ══════════════════════════════════════════════════════════════════════

constexpr uint8_t APU::PulseChannel::DUTY_TABLE[4];

int APU::PulseChannel::output() const {
    if (!enabled || !dacEnabled) return 0;
    int bit = (DUTY_TABLE[dutyLength >> 6] >> (7 - dutyPosition)) & 1;
    return bit ? currentVolume : 0;
}

void APU::PulseChannel::tickFrequency() {
    if (--frequencyTimer <= 0) {
        int freq = freqLo | ((freqHi & 0x07) << 8);
        frequencyTimer = (2048 - freq) * 4;
        dutyPosition = (dutyPosition + 1) & 7;
    }
}

void APU::PulseChannel::tickLengthCounter() {
    if ((freqHi & 0x40) && lengthCounter > 0) {
        if (--lengthCounter == 0) {
            enabled = false;
        }
    }
}

void APU::PulseChannel::tickVolumeEnvelope() {
    if (!envelopeRunning) return;
    int period = envelope & 0x07;
    if (period == 0) return;

    if (--envelopeTimer <= 0) {
        envelopeTimer = period;
        if (envelope & 0x08) {
            // Increase
            if (currentVolume < 15) currentVolume++;
            else envelopeRunning = false;
        } else {
            // Decrease
            if (currentVolume > 0) currentVolume--;
            else envelopeRunning = false;
        }
    }
}

int APU::PulseChannel::calcSweepFreq() const {
    int shift = sweep & 0x07;
    int delta = sweepShadowFreq >> shift;
    if (sweep & 0x08) {
        return sweepShadowFreq - delta;
    }
    return sweepShadowFreq + delta;
}

void APU::PulseChannel::tickSweep(bool& channelEnable) {
    if (--sweepTimer <= 0) {
        int period = (sweep >> 4) & 0x07;
        sweepTimer = period > 0 ? period : 8;

        if (sweepEnabled && period > 0) {
            int newFreq = calcSweepFreq();
            if (sweep & 0x08) sweepNegateUsed = true;
            if (newFreq > 2047) {
                channelEnable = false;
                enabled = false;
            } else if ((sweep & 0x07) > 0) {
                sweepShadowFreq = newFreq;
                freqLo = newFreq & 0xFF;
                freqHi = (freqHi & 0xF8) | ((newFreq >> 8) & 0x07);

                // Overflow check again with new frequency
                int newFreq2 = calcSweepFreq();
                if (sweep & 0x08) sweepNegateUsed = true;
                if (newFreq2 > 2047) {
                    channelEnable = false;
                    enabled = false;
                }
            }
        }
    }
}

void APU::PulseChannel::trigger(bool isCh1) {
    enabled = true;

    if (lengthCounter == 0) lengthCounter = 64;

    int freq = freqLo | ((freqHi & 0x07) << 8);
    frequencyTimer = (2048 - freq) * 4;

    // Reload volume envelope
    currentVolume = envelope >> 4;
    envelopeTimer = envelope & 0x07;
    if (envelopeTimer == 0) envelopeTimer = 8;
    envelopeRunning = true;

    // DAC check
    dacEnabled = (envelope & 0xF8) != 0;
    if (!dacEnabled) enabled = false;

    // Sweep (CH1 only)
    if (isCh1) {
        sweepShadowFreq = freq;
        int period = (sweep >> 4) & 0x07;
        int shift = sweep & 0x07;
        sweepTimer = period > 0 ? period : 8;
        sweepEnabled = (period > 0 || shift > 0);
        sweepNegateUsed = false;

        // If shift > 0, do an overflow check immediately
        if (shift > 0) {
            if (sweep & 0x08) sweepNegateUsed = true;
            int newFreq = calcSweepFreq();
            if (newFreq > 2047) {
                enabled = false;
            }
        }
    }
}

// ══════════════════════════════════════════════════════════════════════
// WaveChannel implementation
// ══════════════════════════════════════════════════════════════════════

int APU::WaveChannel::output() const {
    if (!enabled || !dacEnabled) return 0;

    int sample = currentSample;
    int shift = (volumeCode >> 5) & 0x03;
    // Shift codes: 0=mute, 1=100%, 2=50%, 3=25%
    static constexpr int SHIFT_TABLE[4] = {4, 0, 1, 2};
    sample >>= SHIFT_TABLE[shift];
    return sample;
}

void APU::WaveChannel::tickFrequency() {
    if (waveJustAccessed > 0) waveJustAccessed--;
    if (--frequencyTimer <= 0) {
        int freq = freqLo | ((freqHi & 0x07) << 8);
        frequencyTimer = (2048 - freq) * 2;
        wavePosition = (wavePosition + 1) & 31;

        // Read the current sample from wave RAM
        uint8_t byte = waveRAM[wavePosition / 2];
        if (wavePosition & 1)
            currentSample = byte & 0x0F;
        else
            currentSample = byte >> 4;

        // DMG: mark that wave RAM was just accessed (read window open for 2 T-cycles)
        waveJustAccessed = 2;
    }
}

void APU::WaveChannel::tickLengthCounter() {
    if ((freqHi & 0x40) && lengthCounter > 0) {
        if (--lengthCounter == 0) {
            enabled = false;
        }
    }
}

void APU::WaveChannel::trigger() {
    // DMG: wave RAM corruption on retrigger while channel is active
    // SameBoy checks sample_countdown == 0 (about to clock next cycle).
    // In our model (APU ticks before CPU writes), frequencyTimer == 2
    // is the equivalent: next tick decrements to 1, then to 0 → wave clocks.
    if (enabled && dacEnabled && frequencyTimer == 2) {
        // Offset uses next position (matching SameBoy's (index+1) >> 1)
        unsigned offset = ((wavePosition + 1) >> 1) & 0xF;
        if (offset < 4) {
            waveRAM[0] = waveRAM[offset];
        } else {
            int alignedStart = offset & ~3;
            waveRAM[0] = waveRAM[alignedStart];
            waveRAM[1] = waveRAM[alignedStart + 1];
            waveRAM[2] = waveRAM[alignedStart + 2];
            waveRAM[3] = waveRAM[alignedStart + 3];
        }
    }

    enabled = true;
    dacEnabled = (dacPower & 0x80) != 0;
    if (!dacEnabled) enabled = false;

    if (lengthCounter == 0) lengthCounter = 256;

    int freq = freqLo | ((freqHi & 0x07) << 8);
    frequencyTimer = (2048 - freq) * 2 + 6;  // +6 T-cycle startup delay (DMG)
    wavePosition = 0;
    waveJustAccessed = 4;
}

// ══════════════════════════════════════════════════════════════════════
// NoiseChannel implementation
// ══════════════════════════════════════════════════════════════════════

int APU::NoiseChannel::output() const {
    if (!enabled || !dacEnabled) return 0;
    // LFSR bit 0 inverted: 0 = output high, 1 = output low
    return (~lfsr & 1) ? currentVolume : 0;
}

void APU::NoiseChannel::tickFrequency() {
    if (--frequencyTimer <= 0) {
        int divisor = polynomial & 0x07;
        int shift = polynomial >> 4;
        static constexpr int DIVISOR_TABLE[8] = {8, 16, 32, 48, 64, 80, 96, 112};
        frequencyTimer = DIVISOR_TABLE[divisor] << shift;

        // Clock the LFSR
        int xorBit = (lfsr & 1) ^ ((lfsr >> 1) & 1);
        lfsr = (lfsr >> 1) | (xorBit << 14);

        // 7-bit mode (width bit set)
        if (polynomial & 0x08) {
            lfsr &= ~(1 << 6);
            lfsr |= (xorBit << 6);
        }
    }
}

void APU::NoiseChannel::tickLengthCounter() {
    if ((control & 0x40) && lengthCounter > 0) {
        if (--lengthCounter == 0) {
            enabled = false;
        }
    }
}

void APU::NoiseChannel::tickVolumeEnvelope() {
    if (!envelopeRunning) return;
    int period = envelope & 0x07;
    if (period == 0) return;

    if (--envelopeTimer <= 0) {
        envelopeTimer = period;
        if (envelope & 0x08) {
            if (currentVolume < 15) currentVolume++;
            else envelopeRunning = false;
        } else {
            if (currentVolume > 0) currentVolume--;
            else envelopeRunning = false;
        }
    }
}

void APU::NoiseChannel::trigger() {
    enabled = true;

    if (lengthCounter == 0) lengthCounter = 64;

    int divisor = polynomial & 0x07;
    int shift = polynomial >> 4;
    static constexpr int DIVISOR_TABLE[8] = {8, 16, 32, 48, 64, 80, 96, 112};
    frequencyTimer = DIVISOR_TABLE[divisor] << shift;

    // Reset LFSR to all 1s
    lfsr = 0x7FFF;

    // Reload volume envelope
    currentVolume = envelope >> 4;
    envelopeTimer = envelope & 0x07;
    if (envelopeTimer == 0) envelopeTimer = 8;
    envelopeRunning = true;

    dacEnabled = (envelope & 0xF8) != 0;
    if (!dacEnabled) enabled = false;
}

// ══════════════════════════════════════════════════════════════════════
// APU::tick() — called once per T-cycle
// ══════════════════════════════════════════════════════════════════════

void APU::tick() {
    // Frame sequencer clock always advances (tied to DIV which keeps running)
    if (++frameSequencerClock_ >= 8192) {
        frameSequencerClock_ = 0;
        if (powered_) {
            stepFrameSequencer();
        }
    }

    if (!powered_) {
        // Still need to advance downsample counter to push silence
        sampleCounter_ += 1.0;
        if (sampleCounter_ >= CYCLES_PER_SAMPLE) {
            sampleCounter_ -= CYCLES_PER_SAMPLE;
            pushSample(0.0f, 0.0f);
        }
        return;
    }

    // ── Tick channel frequency timers ────────────────────────────────
    ch1_.tickFrequency();
    ch2_.tickFrequency();
    ch3_.tickFrequency();
    ch4_.tickFrequency();

    // ── Downsample to output ────────────────────────────────────────
    sampleCounter_ += 1.0;
    if (sampleCounter_ >= CYCLES_PER_SAMPLE) {
        sampleCounter_ -= CYCLES_PER_SAMPLE;

        // Mix channels
        float leftMix = 0.0f, rightMix = 0.0f;

        float ch1Out = static_cast<float>(ch1_.output());
        float ch2Out = static_cast<float>(ch2_.output());
        float ch3Out = static_cast<float>(ch3_.output());
        float ch4Out = static_cast<float>(ch4_.output());

        // NR51 panning
        if (nr51_ & 0x01) rightMix += ch1Out;
        if (nr51_ & 0x02) rightMix += ch2Out;
        if (nr51_ & 0x04) rightMix += ch3Out;
        if (nr51_ & 0x08) rightMix += ch4Out;
        if (nr51_ & 0x10) leftMix  += ch1Out;
        if (nr51_ & 0x20) leftMix  += ch2Out;
        if (nr51_ & 0x40) leftMix  += ch3Out;
        if (nr51_ & 0x80) leftMix  += ch4Out;

        // NR50 master volume (0-7 each side)
        int rightVol = (nr50_ & 0x07) + 1;
        int leftVol  = ((nr50_ >> 4) & 0x07) + 1;

        leftMix  *= static_cast<float>(leftVol);
        rightMix *= static_cast<float>(rightVol);

        // Normalize: max per channel = 15, 4 channels, volume 8 → max 480
        // Scale to [-1, 1] range
        constexpr float SCALE = 1.0f / 480.0f;
        leftMix  *= SCALE;
        rightMix *= SCALE;

        // High-pass filter (DC offset removal, SameBoy-style)
        float outL = leftMix - hpfCapacitorL_;
        hpfCapacitorL_ = leftMix - outL * HPF_CHARGE_FACTOR;

        float outR = rightMix - hpfCapacitorR_;
        hpfCapacitorR_ = rightMix - outR * HPF_CHARGE_FACTOR;

        pushSample(outL, outR);
    }
}

// ══════════════════════════════════════════════════════════════════════
// Frame sequencer steps
// ══════════════════════════════════════════════════════════════════════

void APU::stepFrameSequencer() {
    switch (frameSequencerStep_) {
        case 0: // Length
            ch1_.tickLengthCounter();
            ch2_.tickLengthCounter();
            ch3_.tickLengthCounter();
            ch4_.tickLengthCounter();
            break;
        case 1: // Nothing
            break;
        case 2: // Length + Sweep
            ch1_.tickLengthCounter();
            ch2_.tickLengthCounter();
            ch3_.tickLengthCounter();
            ch4_.tickLengthCounter();
            ch1_.tickSweep(ch1_.enabled);
            break;
        case 3: // Nothing
            break;
        case 4: // Length
            ch1_.tickLengthCounter();
            ch2_.tickLengthCounter();
            ch3_.tickLengthCounter();
            ch4_.tickLengthCounter();
            break;
        case 5: // Nothing
            break;
        case 6: // Length + Sweep
            ch1_.tickLengthCounter();
            ch2_.tickLengthCounter();
            ch3_.tickLengthCounter();
            ch4_.tickLengthCounter();
            ch1_.tickSweep(ch1_.enabled);
            break;
        case 7: // Volume envelope
            ch1_.tickVolumeEnvelope();
            ch2_.tickVolumeEnvelope();
            ch4_.tickVolumeEnvelope();
            break;
    }
    frameSequencerStep_ = (frameSequencerStep_ + 1) & 7;
}

// ══════════════════════════════════════════════════════════════════════
// Power control
// ══════════════════════════════════════════════════════════════════════

void APU::powerOff() {
    powered_ = false;

    // DMG: preserve length counters and wave RAM across power off
    int len1 = ch1_.lengthCounter;
    int len2 = ch2_.lengthCounter;
    int len3 = ch3_.lengthCounter;
    int len4 = ch4_.lengthCounter;
    auto savedWaveRAM = ch3_.waveRAM;

    // Zero all registers and state
    ch1_ = PulseChannel{};
    ch2_ = PulseChannel{};
    ch3_ = WaveChannel{};
    ch4_ = NoiseChannel{};

    // Restore preserved state
    ch1_.lengthCounter = len1;
    ch2_.lengthCounter = len2;
    ch3_.lengthCounter = len3;
    ch3_.waveRAM = savedWaveRAM;
    ch4_.lengthCounter = len4;

    nr50_ = 0;
    nr51_ = 0;
    frameSequencerStep_ = 0;
    // Don't reset frameSequencerClock_ — DIV keeps running
}

void APU::powerOn() {
    powered_ = true;
    frameSequencerStep_ = 0;
    // Don't reset frameSequencerClock_ — continues from DIV position
}

// ══════════════════════════════════════════════════════════════════════
// Ring buffer push
// ══════════════════════════════════════════════════════════════════════

void APU::pushSample(float left, float right) {
    int w = writePos_.load(std::memory_order_relaxed);
    int next = (w + 1) & (BUFFER_SIZE - 1);
    int r = readPos_.load(std::memory_order_acquire);

    if (next == r) {
        // Buffer full — drop sample (consumer too slow)
        return;
    }

    sampleBuffer_[w] = {left, right};
    writePos_.store(next, std::memory_order_release);
}

// ══════════════════════════════════════════════════════════════════════
// Register read
// ══════════════════════════════════════════════════════════════════════

uint8_t APU::readReg(uint16_t addr) const {
    // Wave RAM (FF30-FF3F)
    if (addr >= 0xFF30 && addr <= 0xFF3F) {
        if (ch3_.enabled) {
            // DMG: reads only return the current byte within the access window
            if (ch3_.waveJustAccessed) {
                return ch3_.waveRAM[ch3_.wavePosition / 2];
            }
            return 0xFF;  // Outside access window on DMG
        }
        return ch3_.waveRAM[addr - 0xFF30];
    }

    uint8_t mask = readMask(addr);
    uint8_t val = 0;

    switch (addr) {
        case 0xFF10: val = ch1_.sweep; break;
        case 0xFF11: val = ch1_.dutyLength; break;
        case 0xFF12: val = ch1_.envelope; break;
        case 0xFF13: val = ch1_.freqLo; break;
        case 0xFF14: val = ch1_.freqHi & 0x40; break;  // Only bit 6 readable

        case 0xFF16: val = ch2_.dutyLength; break;
        case 0xFF17: val = ch2_.envelope; break;
        case 0xFF18: val = ch2_.freqLo; break;
        case 0xFF19: val = ch2_.freqHi & 0x40; break;

        case 0xFF1A: val = ch3_.dacPower; break;
        case 0xFF1B: val = ch3_.length; break;
        case 0xFF1C: val = ch3_.volumeCode; break;
        case 0xFF1D: val = ch3_.freqLo; break;
        case 0xFF1E: val = ch3_.freqHi & 0x40; break;

        case 0xFF20: val = ch4_.length; break;
        case 0xFF21: val = ch4_.envelope; break;
        case 0xFF22: val = ch4_.polynomial; break;
        case 0xFF23: val = ch4_.control & 0x40; break;

        case 0xFF24: val = nr50_; break;
        case 0xFF25: val = nr51_; break;
        case 0xFF26: {
            val = (powered_ ? 0x80 : 0x00);
            if (ch1_.enabled) val |= 0x01;
            if (ch2_.enabled) val |= 0x02;
            if (ch3_.enabled) val |= 0x04;
            if (ch4_.enabled) val |= 0x08;
            break;
        }

        default:
            return 0xFF;
    }

    return val | mask;
}

// ══════════════════════════════════════════════════════════════════════
// Register write
// ══════════════════════════════════════════════════════════════════════

void APU::writeReg(uint16_t addr, uint8_t val) {
    // Wave RAM (FF30-FF3F) — always writable
    if (addr >= 0xFF30 && addr <= 0xFF3F) {
        if (ch3_.enabled) {
            // DMG: writes only affect the current byte within the access window
            if (ch3_.waveJustAccessed) {
                ch3_.waveRAM[ch3_.wavePosition / 2] = val;
            }
            // Outside access window: write is lost on DMG
        } else {
            ch3_.waveRAM[addr - 0xFF30] = val;
        }
        return;
    }

    // NR52 (FF26) — master power control
    if (addr == 0xFF26) {
        bool newPower = (val & 0x80) != 0;
        if (!newPower && powered_) {
            powerOff();
        } else if (newPower && !powered_) {
            powerOn();
        }
        return;
    }

    // If powered off, only NR52 and wave RAM are writable
    // Exception on DMG: length counters can still be written
    if (!powered_) {
        // On DMG, length counters CAN be written when powered off
        // but the register byte itself is NOT updated (stays zeroed)
        switch (addr) {
            case 0xFF11: ch1_.lengthCounter = 64 - (val & 0x3F); return;
            case 0xFF16: ch2_.lengthCounter = 64 - (val & 0x3F); return;
            case 0xFF1B: ch3_.lengthCounter = 256 - val; return;
            case 0xFF20: ch4_.lengthCounter = 64 - (val & 0x3F); return;
            default: return;
        }
    }

    switch (addr) {
        // ── Channel 1 ───────────────────────────────────────────────
        case 0xFF10: // NR10 — Sweep
            // If negate was used and we're switching from negate to positive,
            // disable the channel (obscure hardware behavior)
            if (ch1_.sweepNegateUsed && !(val & 0x08)) {
                ch1_.enabled = false;
            }
            ch1_.sweep = val;
            break;

        case 0xFF11: // NR11 — Duty + Length
            ch1_.dutyLength = val;
            ch1_.lengthCounter = 64 - (val & 0x3F);
            break;

        case 0xFF12: // NR12 — Volume Envelope
            ch1_.envelope = val;
            ch1_.dacEnabled = (val & 0xF8) != 0;
            if (!ch1_.dacEnabled) ch1_.enabled = false;
            break;

        case 0xFF13: // NR13 — Frequency Low
            ch1_.freqLo = val;
            break;

        case 0xFF14: { // NR14 — Trigger + Length Enable + Freq High
            bool wasLenEnabled = ch1_.freqHi & 0x40;
            bool newLenEnabled = val & 0x40;
            bool firstHalf = (frameSequencerStep_ & 1);
            ch1_.freqHi = val;
            // Extra clock only when NEWLY enabling length in first half
            if (!wasLenEnabled && newLenEnabled && firstHalf && ch1_.lengthCounter > 0) {
                ch1_.lengthCounter--;
                if (ch1_.lengthCounter == 0 && !(val & 0x80)) {
                    ch1_.enabled = false;
                }
            }
            if (val & 0x80) {
                int lenBefore = ch1_.lengthCounter;
                ch1_.trigger(true);
                // If trigger reloaded length from 0→max AND enabled in first half, extra clock
                if (lenBefore == 0 && newLenEnabled && firstHalf && ch1_.lengthCounter > 0) {
                    ch1_.lengthCounter--;
                    if (ch1_.lengthCounter == 0) ch1_.enabled = false;
                }
            }
            break;
        }

        // ── Channel 2 ───────────────────────────────────────────────
        case 0xFF16: // NR21 — Duty + Length
            ch2_.dutyLength = val;
            ch2_.lengthCounter = 64 - (val & 0x3F);
            break;

        case 0xFF17: // NR22 — Volume Envelope
            ch2_.envelope = val;
            ch2_.dacEnabled = (val & 0xF8) != 0;
            if (!ch2_.dacEnabled) ch2_.enabled = false;
            break;

        case 0xFF18: // NR23 — Frequency Low
            ch2_.freqLo = val;
            break;

        case 0xFF19: { // NR24 — Trigger + Length Enable + Freq High
            bool wasLenEnabled = ch2_.freqHi & 0x40;
            bool newLenEnabled = val & 0x40;
            bool firstHalf = (frameSequencerStep_ & 1);
            ch2_.freqHi = val;
            if (!wasLenEnabled && newLenEnabled && firstHalf && ch2_.lengthCounter > 0) {
                ch2_.lengthCounter--;
                if (ch2_.lengthCounter == 0 && !(val & 0x80)) {
                    ch2_.enabled = false;
                }
            }
            if (val & 0x80) {
                int lenBefore = ch2_.lengthCounter;
                ch2_.trigger(false);
                if (lenBefore == 0 && newLenEnabled && firstHalf && ch2_.lengthCounter > 0) {
                    ch2_.lengthCounter--;
                    if (ch2_.lengthCounter == 0) ch2_.enabled = false;
                }
            }
            break;
        }

        // ── Channel 3 ───────────────────────────────────────────────
        case 0xFF1A: // NR30 — DAC Power
            ch3_.dacPower = val;
            ch3_.dacEnabled = (val & 0x80) != 0;
            if (!ch3_.dacEnabled) ch3_.enabled = false;
            break;

        case 0xFF1B: // NR31 — Length
            ch3_.length = val;
            ch3_.lengthCounter = 256 - val;
            break;

        case 0xFF1C: // NR32 — Volume Code
            ch3_.volumeCode = val;
            break;

        case 0xFF1D: // NR33 — Frequency Low
            ch3_.freqLo = val;
            break;

        case 0xFF1E: { // NR34 — Trigger + Length Enable + Freq High
            bool wasLenEnabled = ch3_.freqHi & 0x40;
            bool newLenEnabled = val & 0x40;
            bool firstHalf = (frameSequencerStep_ & 1);
            ch3_.freqHi = val;
            if (!wasLenEnabled && newLenEnabled && firstHalf && ch3_.lengthCounter > 0) {
                ch3_.lengthCounter--;
                if (ch3_.lengthCounter == 0 && !(val & 0x80)) {
                    ch3_.enabled = false;
                }
            }
            if (val & 0x80) {
                int lenBefore = ch3_.lengthCounter;
                ch3_.trigger();
                if (lenBefore == 0 && newLenEnabled && firstHalf && ch3_.lengthCounter > 0) {
                    ch3_.lengthCounter--;
                    if (ch3_.lengthCounter == 0) ch3_.enabled = false;
                }
            }
            break;
        }

        // ── Channel 4 ───────────────────────────────────────────────
        case 0xFF20: // NR41 — Length
            ch4_.length = val;
            ch4_.lengthCounter = 64 - (val & 0x3F);
            break;

        case 0xFF21: // NR42 — Volume Envelope
            ch4_.envelope = val;
            ch4_.dacEnabled = (val & 0xF8) != 0;
            if (!ch4_.dacEnabled) ch4_.enabled = false;
            break;

        case 0xFF22: // NR43 — Polynomial Counter
            ch4_.polynomial = val;
            break;

        case 0xFF23: { // NR44 — Trigger + Length Enable
            bool wasLenEnabled = ch4_.control & 0x40;
            bool newLenEnabled = val & 0x40;
            bool firstHalf = (frameSequencerStep_ & 1);
            ch4_.control = val;
            if (!wasLenEnabled && newLenEnabled && firstHalf && ch4_.lengthCounter > 0) {
                ch4_.lengthCounter--;
                if (ch4_.lengthCounter == 0 && !(val & 0x80)) {
                    ch4_.enabled = false;
                }
            }
            if (val & 0x80) {
                int lenBefore = ch4_.lengthCounter;
                ch4_.trigger();
                if (lenBefore == 0 && newLenEnabled && firstHalf && ch4_.lengthCounter > 0) {
                    ch4_.lengthCounter--;
                    if (ch4_.lengthCounter == 0) ch4_.enabled = false;
                }
            }
            break;
        }

        // ── Master control ──────────────────────────────────────────
        case 0xFF24: // NR50 — Master volume
            nr50_ = val;
            break;

        case 0xFF25: // NR51 — Channel panning
            nr51_ = val;
            break;

        // NR52 handled above (before power-off guard)

        default:
            break;
    }
}
