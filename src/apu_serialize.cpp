#include "apu.h"
#include "save_state.h"

// ══════════════════════════════════════════════════════════════════════
// APU save state serialization
// ══════════════════════════════════════════════════════════════════════

void APU::serialize(SaveState& ss) const {
    // Master state
    ss.writeBool(powered_);
    ss.write<uint8_t>(nr50_);
    ss.write<uint8_t>(nr51_);
    ss.write<uint8_t>(nr52_);
    ss.write<int32_t>(frameSequencerStep_);
    ss.write<int32_t>(frameSequencerClock_);

    // Channel 1
    ss.write<uint8_t>(ch1_.sweep);
    ss.write<uint8_t>(ch1_.dutyLength);
    ss.write<uint8_t>(ch1_.envelope);
    ss.write<uint8_t>(ch1_.freqLo);
    ss.write<uint8_t>(ch1_.freqHi);
    ss.writeBool(ch1_.enabled);
    ss.writeBool(ch1_.dacEnabled);
    ss.write<int32_t>(ch1_.lengthCounter);
    ss.write<int32_t>(ch1_.frequencyTimer);
    ss.write<int32_t>(ch1_.dutyPosition);
    ss.write<int32_t>(ch1_.currentVolume);
    ss.write<int32_t>(ch1_.envelopeTimer);
    ss.writeBool(ch1_.envelopeRunning);
    ss.writeBool(ch1_.sweepEnabled);
    ss.write<int32_t>(ch1_.sweepTimer);
    ss.write<int32_t>(ch1_.sweepShadowFreq);
    ss.writeBool(ch1_.sweepNegateUsed);

    // Channel 2
    ss.write<uint8_t>(ch2_.dutyLength);
    ss.write<uint8_t>(ch2_.envelope);
    ss.write<uint8_t>(ch2_.freqLo);
    ss.write<uint8_t>(ch2_.freqHi);
    ss.writeBool(ch2_.enabled);
    ss.writeBool(ch2_.dacEnabled);
    ss.write<int32_t>(ch2_.lengthCounter);
    ss.write<int32_t>(ch2_.frequencyTimer);
    ss.write<int32_t>(ch2_.dutyPosition);
    ss.write<int32_t>(ch2_.currentVolume);
    ss.write<int32_t>(ch2_.envelopeTimer);
    ss.writeBool(ch2_.envelopeRunning);

    // Channel 3
    ss.write<uint8_t>(ch3_.dacPower);
    ss.write<uint8_t>(ch3_.length);
    ss.write<uint8_t>(ch3_.volumeCode);
    ss.write<uint8_t>(ch3_.freqLo);
    ss.write<uint8_t>(ch3_.freqHi);
    ss.writeBytes(ch3_.waveRAM.data(), ch3_.waveRAM.size());
    ss.writeBool(ch3_.enabled);
    ss.writeBool(ch3_.dacEnabled);
    ss.write<int32_t>(ch3_.lengthCounter);
    ss.write<int32_t>(ch3_.frequencyTimer);
    ss.write<int32_t>(ch3_.wavePosition);
    ss.write<uint8_t>(ch3_.currentSample);

    // Channel 4
    ss.write<uint8_t>(ch4_.length);
    ss.write<uint8_t>(ch4_.envelope);
    ss.write<uint8_t>(ch4_.polynomial);
    ss.write<uint8_t>(ch4_.control);
    ss.writeBool(ch4_.enabled);
    ss.writeBool(ch4_.dacEnabled);
    ss.write<int32_t>(ch4_.lengthCounter);
    ss.write<int32_t>(ch4_.frequencyTimer);
    ss.write<uint16_t>(ch4_.lfsr);
    ss.write<int32_t>(ch4_.currentVolume);
    ss.write<int32_t>(ch4_.envelopeTimer);
    ss.writeBool(ch4_.envelopeRunning);
}

void APU::deserialize(SaveState& ss) {
    // Master state
    powered_ = ss.readBool();
    nr50_ = ss.read<uint8_t>();
    nr51_ = ss.read<uint8_t>();
    nr52_ = ss.read<uint8_t>();
    frameSequencerStep_ = ss.read<int32_t>();
    frameSequencerClock_ = ss.read<int32_t>();

    // Channel 1
    ch1_.sweep = ss.read<uint8_t>();
    ch1_.dutyLength = ss.read<uint8_t>();
    ch1_.envelope = ss.read<uint8_t>();
    ch1_.freqLo = ss.read<uint8_t>();
    ch1_.freqHi = ss.read<uint8_t>();
    ch1_.enabled = ss.readBool();
    ch1_.dacEnabled = ss.readBool();
    ch1_.lengthCounter = ss.read<int32_t>();
    ch1_.frequencyTimer = ss.read<int32_t>();
    ch1_.dutyPosition = ss.read<int32_t>();
    ch1_.currentVolume = ss.read<int32_t>();
    ch1_.envelopeTimer = ss.read<int32_t>();
    ch1_.envelopeRunning = ss.readBool();
    ch1_.sweepEnabled = ss.readBool();
    ch1_.sweepTimer = ss.read<int32_t>();
    ch1_.sweepShadowFreq = ss.read<int32_t>();
    ch1_.sweepNegateUsed = ss.readBool();

    // Channel 2
    ch2_.dutyLength = ss.read<uint8_t>();
    ch2_.envelope = ss.read<uint8_t>();
    ch2_.freqLo = ss.read<uint8_t>();
    ch2_.freqHi = ss.read<uint8_t>();
    ch2_.enabled = ss.readBool();
    ch2_.dacEnabled = ss.readBool();
    ch2_.lengthCounter = ss.read<int32_t>();
    ch2_.frequencyTimer = ss.read<int32_t>();
    ch2_.dutyPosition = ss.read<int32_t>();
    ch2_.currentVolume = ss.read<int32_t>();
    ch2_.envelopeTimer = ss.read<int32_t>();
    ch2_.envelopeRunning = ss.readBool();

    // Channel 3
    ch3_.dacPower = ss.read<uint8_t>();
    ch3_.length = ss.read<uint8_t>();
    ch3_.volumeCode = ss.read<uint8_t>();
    ch3_.freqLo = ss.read<uint8_t>();
    ch3_.freqHi = ss.read<uint8_t>();
    ss.readBytes(ch3_.waveRAM.data(), ch3_.waveRAM.size());
    ch3_.enabled = ss.readBool();
    ch3_.dacEnabled = ss.readBool();
    ch3_.lengthCounter = ss.read<int32_t>();
    ch3_.frequencyTimer = ss.read<int32_t>();
    ch3_.wavePosition = ss.read<int32_t>();
    ch3_.currentSample = ss.read<uint8_t>();

    // Channel 4
    ch4_.length = ss.read<uint8_t>();
    ch4_.envelope = ss.read<uint8_t>();
    ch4_.polynomial = ss.read<uint8_t>();
    ch4_.control = ss.read<uint8_t>();
    ch4_.enabled = ss.readBool();
    ch4_.dacEnabled = ss.readBool();
    ch4_.lengthCounter = ss.read<int32_t>();
    ch4_.frequencyTimer = ss.read<int32_t>();
    ch4_.lfsr = ss.read<uint16_t>();
    ch4_.currentVolume = ss.read<int32_t>();
    ch4_.envelopeTimer = ss.read<int32_t>();
    ch4_.envelopeRunning = ss.readBool();

    // Reset ring buffer positions (audio state is ephemeral)
    writePos_.store(0, std::memory_order_relaxed);
    readPos_.store(0, std::memory_order_relaxed);
    sampleCounter_ = 0.0;
    hpfCapacitorL_ = 0.0f;
    hpfCapacitorR_ = 0.0f;
}
