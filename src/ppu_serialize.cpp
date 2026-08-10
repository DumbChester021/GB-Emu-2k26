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
    ss.write<int32_t>(lcdEnableDelayDots_);
    ss.writeBool(firstLineAfterEnable_);
    ss.writeBool(firstLineShorter_);
    ss.write<int32_t>(mode0StatDelayDots_);
    ss.write<int32_t>(vblankLine_);

    // STAT IRQ
    ss.writeBool(statIrqLine_);
    ss.writeBool(vblankOamPulse_);
    ss.writeBool(oamStatEarly_);
    ss.write<int32_t>(mode3StartDot_);
    ss.write<int32_t>(mode3StartupDots_);

    // Framebuffer + frame ready flag
    ss.writeBool(frameReady_);
    ss.writeBytes(framebuffer_.data(), framebuffer_.size() * sizeof(uint32_t));

    // VRAM & OAM
    ss.writeBytes(vram_.data(), vram_.size());
    ss.writeBytes(oam_.data(), oam_.size());

    // Sprite evaluation
    ss.write<int32_t>(lineSpriteCount_);
    ss.write<int32_t>(oamSearchIndex_);
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

    // OBJ FIFO
    ss.write<int32_t>(objFifo_.count);
    for (int i = 0; i < objFifo_.count; i++) {
        const auto& p = objFifo_.pixels[(objFifo_.head + i) % FIFO_SIZE];
        ss.write<uint8_t>(p.color);
        ss.write<uint8_t>(p.palette);
        ss.writeBool(p.bgPriority);
        ss.writeBool(p.isSprite);
    }

    // Fetcher state
    ss.write<uint8_t>(static_cast<uint8_t>(fetcherState_));
    ss.write<uint16_t>(fetcherMapAddr_);
    ss.write<uint16_t>(fetcherDataAddr_);
    ss.write<int32_t>(fetcherWindowTileX_);
    ss.write<uint8_t>(fetcherTileId_);
    ss.write<uint8_t>(fetcherTileDataLow_);
    ss.write<uint8_t>(fetcherTileDataHigh_);
    ss.writeBool(fetcherFetchingWindow_);

    // Pixel transfer
    ss.write<int32_t>(pixelX_);
    ss.write<int32_t>(fetcherPositionX_);
    ss.writeBool(windowTriggered_);
    ss.writeBool(windowBeingFetched_);
    ss.writeBool(wyTriggered_);
    ss.write<int32_t>(windowY_);
    ss.writeBool(insertBgPixel_);
    ss.writeBool(disableWindowPixelInsertionGlitch_);
    ss.writeBool(lineHasFractionalScrolling_);
    ss.write<int32_t>(wxJustChangedDots_);

    // Sprite fetch
    ss.writeBool(objFetchActive_);
    ss.write<int32_t>(objFetchPhase_);
    ss.write<int32_t>(currentSpriteIdx_);
    ss.write<uint8_t>(objTile_);
    ss.write<uint8_t>(objFlags_);
    ss.write<uint8_t>(objDataLow_);
    ss.write<uint8_t>(objDataHigh_);
    ss.write<uint16_t>(objDataAddr_);
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
    lcdEnableDelayDots_ = ss.read<int32_t>();
    firstLineAfterEnable_ = ss.readBool();
    firstLineShorter_ = ss.readBool();
    mode0StatDelayDots_ = ss.read<int32_t>();
    vblankLine_ = ss.read<int32_t>();

    // STAT IRQ
    statIrqLine_ = ss.readBool();
    vblankOamPulse_ = ss.readBool();
    oamStatEarly_ = ss.readBool();
    mode3StartDot_ = ss.read<int32_t>();
    mode3StartupDots_ = ss.read<int32_t>();

    // Framebuffer + frame ready flag
    frameReady_ = ss.readBool();
    ss.readBytes(framebuffer_.data(), framebuffer_.size() * sizeof(uint32_t));

    // VRAM & OAM
    ss.readBytes(vram_.data(), vram_.size());
    ss.readBytes(oam_.data(), oam_.size());

    // Sprite evaluation
    lineSpriteCount_ = ss.read<int32_t>();
    oamSearchIndex_ = ss.read<int32_t>();
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

    // OBJ FIFO
    objFifo_.clear();
    int objFifoCount = ss.read<int32_t>();
    for (int i = 0; i < objFifoCount; i++) {
        FIFOPixel p;
        p.color = ss.read<uint8_t>();
        p.palette = ss.read<uint8_t>();
        p.bgPriority = ss.readBool();
        p.isSprite = ss.readBool();
        objFifo_.push(p);
    }

    // Fetcher state
    fetcherState_ = static_cast<FetcherState>(ss.read<uint8_t>());
    fetcherMapAddr_ = ss.read<uint16_t>();
    fetcherDataAddr_ = ss.read<uint16_t>();
    fetcherWindowTileX_ = ss.read<int32_t>();
    fetcherTileId_ = ss.read<uint8_t>();
    fetcherTileDataLow_ = ss.read<uint8_t>();
    fetcherTileDataHigh_ = ss.read<uint8_t>();
    fetcherFetchingWindow_ = ss.readBool();

    // Pixel transfer
    pixelX_ = ss.read<int32_t>();
    fetcherPositionX_ = ss.read<int32_t>();
    windowTriggered_ = ss.readBool();
    windowBeingFetched_ = ss.readBool();
    wyTriggered_ = ss.readBool();
    windowY_ = ss.read<int32_t>();
    insertBgPixel_ = ss.readBool();
    disableWindowPixelInsertionGlitch_ = ss.readBool();
    lineHasFractionalScrolling_ = ss.readBool();
    wxJustChangedDots_ = ss.read<int32_t>();

    // Sprite fetch
    objFetchActive_ = ss.readBool();
    objFetchPhase_ = ss.read<int32_t>();
    currentSpriteIdx_ = ss.read<int32_t>();
    objTile_ = ss.read<uint8_t>();
    objFlags_ = ss.read<uint8_t>();
    objDataLow_ = ss.read<uint8_t>();
    objDataHigh_ = ss.read<uint8_t>();
    objDataAddr_ = ss.read<uint16_t>();
}
