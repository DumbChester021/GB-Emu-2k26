#include "ppu.h"
#include "save_state.h"

// ══════════════════════════════════════════════════════════════════════
// PPU save state — captures the complete pixel pipeline state
// ══════════════════════════════════════════════════════════════════════

void PPU::serialize(SaveState& ss) const {
    // LCD registers
    ss.write<uint8_t>(lcdc_);
    ss.write<uint8_t>(stat_);
    ss.write<uint8_t>(scy_);
    ss.write<uint8_t>(scx_);
    ss.write<uint8_t>(ly_);
    ss.write<uint8_t>(lyc_);
    ss.write<uint8_t>(dma_);
    ss.write<uint8_t>(bgp_);
    ss.write<uint8_t>(obp0_);
    ss.write<uint8_t>(obp1_);
    ss.write<uint8_t>(wy_);
    ss.write<uint8_t>(wx_);

    // Internal state
    ss.write<uint8_t>(mode_);
    ss.write<int32_t>(dotCounter_);
    ss.writeBool(lcdWasOff_);
    ss.writeBool(firstLineAfterEnable_);
    ss.write<int32_t>(vblankLine_);

    // STAT IRQ
    ss.writeBool(statIrqLine_);
    ss.writeBool(vblankOamPulse_);
    ss.write<int32_t>(mode3StartDot_);
    ss.write<int32_t>(mode3PenaltyDots_);

    // Framebuffer + frame ready flag
    ss.writeBool(frameReady_);
    ss.writeBytes(framebuffer_.data(), framebuffer_.size() * sizeof(uint32_t));

    // VRAM & OAM
    ss.writeBytes(vram_.data(), vram_.size());
    ss.writeBytes(oam_.data(), oam_.size());

    // Sprite evaluation
    ss.write<int32_t>(lineSpriteCount_);
    for (int i = 0; i < lineSpriteCount_; i++) {
        ss.write<uint8_t>(lineSprites_[i].y);
        ss.write<uint8_t>(lineSprites_[i].x);
        ss.write<uint8_t>(lineSprites_[i].tile);
        ss.write<uint8_t>(lineSprites_[i].flags);
        ss.write<uint8_t>(lineSprites_[i].oamIndex);
    }

    // BG FIFO
    ss.write<int32_t>(bgFifo_.head);
    ss.write<int32_t>(bgFifo_.count);
    for (int i = 0; i < bgFifo_.count; i++) {
        const auto& p = bgFifo_.pixels[(bgFifo_.head + i) % FIFO_SIZE];
        ss.write<uint8_t>(p.color);
        ss.write<uint8_t>(p.palette);
        ss.writeBool(p.bgPriority);
        ss.writeBool(p.isSprite);
    }

    // Fetcher state
    ss.write<uint8_t>(static_cast<uint8_t>(fetcherState_));
    ss.write<int32_t>(fetcherClock_);
    ss.write<int32_t>(fetcherTileX_);
    ss.write<uint8_t>(fetcherTileId_);
    ss.write<uint8_t>(fetcherTileDataLow_);
    ss.write<uint8_t>(fetcherTileDataHigh_);
    ss.writeBool(fetcherFetchingWindow_);

    // Pixel transfer
    ss.write<int32_t>(pixelX_);
    ss.write<int32_t>(discardPixels_);
    ss.writeBool(windowTriggered_);
    ss.write<int32_t>(windowLineCounter_);

    // Sprite fetch
    ss.writeBool(spriteFetchPending_);
    ss.write<int32_t>(spriteFetchDot_);
    ss.write<int32_t>(currentSpriteIdx_);
}

void PPU::deserialize(SaveState& ss) {
    // LCD registers
    lcdc_ = ss.read<uint8_t>();
    stat_ = ss.read<uint8_t>();
    scy_ = ss.read<uint8_t>();
    scx_ = ss.read<uint8_t>();
    ly_ = ss.read<uint8_t>();
    lyc_ = ss.read<uint8_t>();
    dma_ = ss.read<uint8_t>();
    bgp_ = ss.read<uint8_t>();
    obp0_ = ss.read<uint8_t>();
    obp1_ = ss.read<uint8_t>();
    wy_ = ss.read<uint8_t>();
    wx_ = ss.read<uint8_t>();

    // Internal state
    mode_ = ss.read<uint8_t>();
    dotCounter_ = ss.read<int32_t>();
    lcdWasOff_ = ss.readBool();
    firstLineAfterEnable_ = ss.readBool();
    vblankLine_ = ss.read<int32_t>();

    // STAT IRQ
    statIrqLine_ = ss.readBool();
    vblankOamPulse_ = ss.readBool();
    mode3StartDot_ = ss.read<int32_t>();
    mode3PenaltyDots_ = ss.read<int32_t>();

    // Framebuffer + frame ready flag
    frameReady_ = ss.readBool();
    ss.readBytes(framebuffer_.data(), framebuffer_.size() * sizeof(uint32_t));

    // VRAM & OAM
    ss.readBytes(vram_.data(), vram_.size());
    ss.readBytes(oam_.data(), oam_.size());

    // Sprite evaluation
    lineSpriteCount_ = ss.read<int32_t>();
    for (int i = 0; i < lineSpriteCount_; i++) {
        lineSprites_[i].y = ss.read<uint8_t>();
        lineSprites_[i].x = ss.read<uint8_t>();
        lineSprites_[i].tile = ss.read<uint8_t>();
        lineSprites_[i].flags = ss.read<uint8_t>();
        lineSprites_[i].oamIndex = ss.read<uint8_t>();
    }

    // BG FIFO
    bgFifo_.clear();
    int fifoHead = ss.read<int32_t>();
    int fifoCount = ss.read<int32_t>();
    bgFifo_.head = 0; // We re-push from index 0
    bgFifo_.count = 0;
    for (int i = 0; i < fifoCount; i++) {
        FIFOPixel p;
        p.color = ss.read<uint8_t>();
        p.palette = ss.read<uint8_t>();
        p.bgPriority = ss.readBool();
        p.isSprite = ss.readBool();
        bgFifo_.push(p);
    }
    (void)fifoHead; // Head is normalized to 0 on restore

    // Fetcher state
    fetcherState_ = static_cast<FetcherState>(ss.read<uint8_t>());
    fetcherClock_ = ss.read<int32_t>();
    fetcherTileX_ = ss.read<int32_t>();
    fetcherTileId_ = ss.read<uint8_t>();
    fetcherTileDataLow_ = ss.read<uint8_t>();
    fetcherTileDataHigh_ = ss.read<uint8_t>();
    fetcherFetchingWindow_ = ss.readBool();

    // Pixel transfer
    pixelX_ = ss.read<int32_t>();
    discardPixels_ = ss.read<int32_t>();
    windowTriggered_ = ss.readBool();
    windowLineCounter_ = ss.read<int32_t>();

    // Sprite fetch
    spriteFetchPending_ = ss.readBool();
    spriteFetchDot_ = ss.read<int32_t>();
    currentSpriteIdx_ = ss.read<int32_t>();
}
