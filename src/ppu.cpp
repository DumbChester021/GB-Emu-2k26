#include "ppu.h"
#include <cstring>
#include <algorithm>


// ══════════════════════════════════════════════════════════════════════
// Main tick() — advance PPU by exactly 1 T-cycle (dot)
//
// Hardware-accurate behavior:
//   • Line 0 after LCD enable: mode 0 for 78 dots, then mode 3
//     (skips OAM search entirely, no sprites, line is 448 dots not 456)
//   • Normal visible lines: mode 2 (80 dots) → mode 3 (variable) → mode 0
//   • VBlank: lines 144–153, mode 1
//   • STAT IRQ uses rising-edge detection on a single shared line
//   • LY increments at the END of a scanline (dot 456)
//   • LYC comparison runs every dot when LCD is on
//   • LCD off: retains coincidence flag, stops comparison clock
// ══════════════════════════════════════════════════════════════════════


void PPU::tick() {
    if (!(lcdc_ & 0x80)) {
        // LCD is off — retain coincidence flag, set mode=0, LY=0
        ly_ = 0;
        dotCounter_ = 0;
        mode_ = MODE_HBLANK;
        // Keep STAT coincidence flag (bit 2) as-is — do NOT clear it
        // Only clear the mode bits
        stat_ = (stat_ & 0xFC);
        // Don't touch statIrqLine_ here — it's frozen at the value
        // set when LCD was turned off (in writeReg). This prevents
        // spurious rising edges when LCD is re-enabled.
        lcdWasOff_ = true;
        return;
    }

    // LCD just turned on — single dead tick for initialization
    if (lcdWasOff_) {
        lcdWasOff_ = false;
        ly_ = 0;
        dotCounter_ = 0;
        pixelX_ = 0;
        bgFifo_.clear();
        objFifo_.clear();
        fetcherState_ = FetcherState::GetTileT1;
        fetcherPositionX_ = -16;
        fetcherWindowTileX_ = 0;
        lineSpriteCount_ = 0;
        oamSearchIndex_ = 0;
        objFetchActive_ = false;
        wyTriggered_ = false;
        windowY_ = -1;

        // Line 0 after LCD enable starts in mode 0 briefly
        mode_ = MODE_HBLANK;
        stat_ = (stat_ & 0xFC) | MODE_HBLANK;

        // Do LYC comparison now that LCD is on
        checkLYC();
        updateStatIRQ();
        return;
    }


    // Normal PPU operation — advance one dot
    dotCounter_++;

    switch (mode_) {
        case MODE_OAM:    tickOAMSearch();     break;
        case MODE_XFER:   tickPixelTransfer(); break;
        case MODE_HBLANK: tickHBlank();        break;
        case MODE_VBLANK: tickVBlank();        break;
    }
}

// ══════════════════════════════════════════════════════════════════════
// Mode 2: OAM Search (80 dots)
// ══════════════════════════════════════════════════════════════════════

void PPU::tickOAMSearch() {
    // The DMG examines one OAM entry every two dots. Only Y and X are
    // retained here; tile number and attributes are read later by Mode 3.
    if (dotCounter_ >= 6 && (dotCounter_ & 1) == 0 && oamSearchIndex_ < 40) {
        scanOAMEntry(oamSearchIndex_++);
    }

    if (dotCounter_ >= OAM_DOTS + 4) {
        sortLineSprites();
        // Mode 3 takes five dots from exposing its mode bits to entering the
        // fetch loop. The transition dot is already consumed here.
        beginPixelTransfer(4);
    }
}

// ══════════════════════════════════════════════════════════════════════
// Mode 3: Pixel Transfer (variable length, ~172 dots minimum)
// ══════════════════════════════════════════════════════════════════════

void PPU::tickPixelTransfer() {
    // Mode 3's initial bus-blocking interval precedes the actual fetch loop.
    if (mode3StartupDots_ > 0) {
        mode3StartupDots_--;
        return;
    }

    if (handleWindow()) {
        if (wxJustChangedDots_ > 0) wxJustChangedDots_--;
        return;
    }

    if (objFetchActive_) {
        tickObjectFetch();
    } else if (beginObjectFetchIfNeeded()) {
        tickObjectFetch();
    } else {
        renderPixelIfPossible();
        advanceFetcher();

        if (pixelX_ >= SCREEN_WIDTH) {
            setMode(MODE_HBLANK);
            fetcherFetchingWindow_ = false;
        }
    }

    if (wxJustChangedDots_ > 0) {
        wxJustChangedDots_--;
    }
}

// ══════════════════════════════════════════════════════════════════════
// Mode 0: HBlank (fills remaining dots to 456 per line)
// ══════════════════════════════════════════════════════════════════════

void PPU::tickHBlank() {
    // Special case: first line after LCD enable
    // Line 0 starts in mode 0 for 78 dots then transitions to mode 3
    // (skips OAM search on the very first line, no sprites evaluated)
    if (firstLineAfterEnable_) {
        if (dotCounter_ >= 78) {
            firstLineAfterEnable_ = false;
            firstLineShorter_ = true;  // First line is 448 dots, not 456
            lineSpriteCount_ = 0;  // No sprites on first line
            if ((lcdc_ & 0x20) && wy_ == ly_) wyTriggered_ = true;
            beginPixelTransfer(11);
            return;
        }
        return;  // Stay in mode 0 until dot 78
    }

    // Pre-OAM transition: normal lines stay in mode 0 for 4 dots before mode 2
    if (dotCounter_ <= 4 && mode_ == MODE_HBLANK) {
        // On nonzero DMG lines, the interrupt source leads STAT's visible
        // mode bits by one dot. Line 0 is the documented exception.
        if (dotCounter_ == 3 && ly_ != 0) {
            oamStatEarly_ = true;
            updateStatIRQ();
        }
        if (dotCounter_ == 4) {
            setMode(MODE_OAM);
            oamStatEarly_ = false;
            lineSpriteCount_ = 0;
            oamSearchIndex_ = 0;
            if ((lcdc_ & 0x20) && wy_ == ly_) wyTriggered_ = true;
            // LYC coincidence becomes valid at this point
            // (SameBoy: ly_for_comparison set to actual LY here)
            checkLYC();
            updateStatIRQ();
        }
        return;
    }

    int lineLength = firstLineShorter_ ? FIRST_LINE_DOTS : DOTS_PER_LINE;
    if (dotCounter_ >= lineLength) {
        firstLineShorter_ = false;
        dotCounter_ = 0;

        ly_++;

        if (ly_ >= VISIBLE_LINES) {
            // Enter VBlank
            setMode(MODE_VBLANK);
            vblankLine_ = 0;  // Start VBlank line counter
            frameReady_ = true;

            // VBlank interrupt (IF bit 0)
            if (ifReg_) {
                *ifReg_ |= 0x01;
            }


            // DMG quirk: Mode 2 OAM STAT source briefly pulses at VBlank
            // entry. This allows mode 2 interrupt to fire at VBlank start,
            // but the pulse is transient — it doesn't hold the STAT IRQ
            // line high for the entire VBlank period.
            vblankOamPulse_ = true;
        } else {
            // Don't set mode to OAM yet — 4-dot pre-OAM transition
            // Mode stays HBLANK; tickHBlank handles transition at dot 4
        }

        if (ly_ >= VISIBLE_LINES) {
            checkLYC();
        } else {
            // Non-VBlank lines: LYC coincidence is invalid for first ~4 dots
            // (SameBoy: ly_for_comparison = -1 for DMG at line start)
            stat_ &= ~0x04;  // Clear coincidence flag
        }
        updateStatIRQ();
        vblankOamPulse_ = false;  // Clear after one-shot evaluation
    }
}

// ══════════════════════════════════════════════════════════════════════
// Mode 1: VBlank (lines 144–153)
// ══════════════════════════════════════════════════════════════════════

void PPU::tickVBlank() {
    if (dotCounter_ >= DOTS_PER_LINE) {
        dotCounter_ = 0;
        vblankLine_++;

        if (vblankLine_ >= 10) {
            // Frame complete — 10 VBlank lines (144–153) elapsed
            ly_ = 0;
            vblankLine_ = 0;
            windowY_ = -1;
            wyTriggered_ = false;
            setMode(MODE_HBLANK);
            oamStatEarly_ = false;
            lineSpriteCount_ = 0;
            oamSearchIndex_ = 0;
        } else {
            // Normal VBlank line — increment visible LY
            ly_ = VISIBLE_LINES + vblankLine_;
        }

        checkLYC();
        updateStatIRQ();
        return;
    }

    // LY=153 early reset: after ~4 dots, LY resets to 0 while still in VBlank.
    // On real DMG hardware, LY reads as 0 for the remainder of scanline 153.
    // This is important for games that poll LY==0 during VBlank for frame sync.
    if (vblankLine_ == 9 && dotCounter_ == 4) {
        ly_ = 0;
        checkLYC();
        updateStatIRQ();
    }
}

// ══════════════════════════════════════════════════════════════════════
// Mode 2 sprite selection — one OAM entry every two dots
// ══════════════════════════════════════════════════════════════════════

void PPU::scanOAMEntry(int index) {
    if (lineSpriteCount_ >= 10 || index < 0 || index >= 40) return;

    int spriteHeight = (lcdc_ & 0x04) ? 16 : 8;
    uint8_t spriteY = oam_[index * 4];
    int screenY = static_cast<int>(spriteY) - 16;

    if (static_cast<int>(ly_) >= screenY &&
        static_cast<int>(ly_) < screenY + spriteHeight) {
        Sprite& s = lineSprites_[lineSpriteCount_++];
        s.y = spriteY;
        s.x = oam_[index * 4 + 1];
        s.tile = 0;
        s.flags = 0;
        s.oamIndex = static_cast<uint8_t>(index);
    }
}

void PPU::sortLineSprites() {
    for (int i = 1; i < lineSpriteCount_; i++) {
        Sprite key = lineSprites_[i];
        int j = i - 1;
        while (j >= 0 && (lineSprites_[j].x > key.x ||
               (lineSprites_[j].x == key.x && lineSprites_[j].oamIndex > key.oamIndex))) {
            lineSprites_[j + 1] = lineSprites_[j];
            j--;
        }
        lineSprites_[j + 1] = key;
    }
}

void PPU::beginPixelTransfer(int startupDots) {
    setMode(MODE_XFER);
    mode3StartDot_ = dotCounter_;
    mode3StartupDots_ = startupDots;

    pixelX_ = 0;
    fetcherPositionX_ = -16;
    bgFifo_.clear();
    objFifo_.clear();
    for (int i = 0; i < 8; ++i) bgFifo_.push(FIFOPixel{});

    fetcherState_ = FetcherState::GetTileT1;
    fetcherWindowTileX_ = 0;
    fetcherFetchingWindow_ = false;
    windowTriggered_ = false;
    windowBeingFetched_ = false;
    insertBgPixel_ = false;
    disableWindowPixelInsertionGlitch_ = false;
    lineHasFractionalScrolling_ = false;

    currentSpriteIdx_ = 0;
    objFetchActive_ = false;
    objFetchPhase_ = 0;
}

bool PPU::handleWindow() {
    bool activate = false;

    if (!windowTriggered_ && wyTriggered_ && (lcdc_ & 0x20)) {
        if (wx_ == 0) {
            activate = fetcherPositionX_ == -7 ||
                       (fetcherPositionX_ == -16 && (scx_ & 7)) ||
                       (fetcherPositionX_ >= -15 && fetcherPositionX_ <= -8);
        } else if (wx_ < 166) {
            if (static_cast<int>(wx_) == fetcherPositionX_ + 7) {
                activate = true;
            } else if (static_cast<int>(wx_) == fetcherPositionX_ + 6 &&
                       wxJustChangedDots_ == 0) {
                activate = true;
                if (pixelX_ > 0) pixelX_--;
            }
        }
    }

    if (activate) {
        windowY_++;
        fetcherWindowTileX_ = 0;
        bgFifo_.clear();
        windowTriggered_ = true;
        fetcherFetchingWindow_ = true;
        windowBeingFetched_ = true;
        fetcherState_ = FetcherState::GetTileT1;
        return wx_ == 0 && (scx_ & 7);
    }

    if (windowTriggered_ && !windowBeingFetched_ &&
        static_cast<int>(wx_) == fetcherPositionX_ + 7 &&
        fetcherState_ == FetcherState::GetTileT1 && bgFifo_.size() == 8) {
        insertBgPixel_ = true;
    }
    return false;
}

bool PPU::beginObjectFetchIfNeeded() {
    int matchX = std::max(0, fetcherPositionX_ + 8);
    while (currentSpriteIdx_ < lineSpriteCount_ &&
           lineSprites_[currentSpriteIdx_].x < matchX) {
        currentSpriteIdx_++;
    }

    if (!(lcdc_ & 0x02) || currentSpriteIdx_ >= lineSpriteCount_ ||
        lineSprites_[currentSpriteIdx_].x != matchX) {
        return false;
    }

    objFetchActive_ = true;
    objFetchPhase_ = -1;
    return true;
}

void PPU::tickObjectFetch() {
    if (!objFetchActive_ || currentSpriteIdx_ >= lineSpriteCount_) {
        objFetchActive_ = false;
        return;
    }

    if (!(lcdc_ & 0x02)) {
        objFetchActive_ = false;
        return;
    }

    if (objFetchPhase_ < 0) {
        if (static_cast<int>(fetcherState_) <
                static_cast<int>(FetcherState::GetTileDataHighT2) ||
            bgFifo_.empty()) {
            advanceFetcher();
            return;
        }
        objFetchPhase_ = 0;
    }

    const Sprite& sprite = lineSprites_[currentSpriteIdx_];
    switch (objFetchPhase_) {
        case 0:
            advanceFetcher();
            objFetchPhase_ = 1;
            break;
        case 1: {
            advanceFetcher();
            const int base = sprite.oamIndex * 4;
            objTile_ = oam_[base + 2];
            objFlags_ = oam_[base + 3];
            objFetchPhase_ = 2;
            break;
        }
        case 2:
            objFetchPhase_ = 3;
            break;
        case 3:
            objDataAddr_ = objectLineAddress(sprite.y, objTile_, objFlags_);
            objDataLow_ = vram_[objDataAddr_];
            objFetchPhase_ = 4;
            break;
        case 4:
            objFetchPhase_ = 5;
            break;
        case 5:
            objDataAddr_ = objectLineAddress(sprite.y, objTile_, objFlags_);
            objDataHigh_ = vram_[objDataAddr_ + 1];
            overlayObjectRow();
            currentSpriteIdx_++;
            objFetchActive_ = false;
            break;
    }
}

uint16_t PPU::objectLineAddress(uint8_t y, uint8_t tile, uint8_t flags) const {
    bool tall = (lcdc_ & 0x04) != 0;
    uint8_t tileY = static_cast<uint8_t>(ly_ - y) & (tall ? 0x0F : 0x07);
    if (flags & 0x40) tileY ^= tall ? 0x0F : 0x07;
    uint8_t tileIndex = tall ? (tile & 0xFE) : tile;
    return static_cast<uint16_t>(tileIndex * 16 + tileY * 2);
}

void PPU::overlayObjectRow() {
    while (objFifo_.size() < 8) objFifo_.push(FIFOPixel{});

    for (int i = 0; i < 8; ++i) {
        int bit = (objFlags_ & 0x20) ? i : (7 - i);
        uint8_t color = static_cast<uint8_t>(((objDataHigh_ >> bit) & 1) << 1 |
                                             ((objDataLow_ >> bit) & 1));
        if (color == 0) continue;

        FIFOPixel& target = objFifo_.at(i);
        if (target.isSprite) continue;
        target.color = color;
        target.palette = (objFlags_ & 0x10) ? 2 : 1;
        target.bgPriority = (objFlags_ & 0x80) != 0;
        target.isSprite = true;
    }
}

// ══════════════════════════════════════════════════════════════════════
// Seven-phase BG/window fetcher. Address and data phases are separate
// because DMG register writes can affect an in-flight tile fetch.
// ══════════════════════════════════════════════════════════════════════

uint8_t PPU::fetcherY() const {
    return fetcherFetchingWindow_ ? static_cast<uint8_t>(windowY_)
                                  : static_cast<uint8_t>(ly_ + scy_);
}

void PPU::advanceFetcher() {

    switch (fetcherState_) {
        case FetcherState::GetTileT1: {
            if (!(lcdc_ & 0x20)) {
                windowTriggered_ = false;
                fetcherFetchingWindow_ = false;
            }

            uint16_t map = 0x9800;
            if (fetcherFetchingWindow_) {
                if (lcdc_ & 0x40) map = 0x9C00;
            } else if (lcdc_ & 0x08) {
                map = 0x9C00;
            }

            uint8_t y = fetcherY();
            int tileX;
            if (fetcherFetchingWindow_) {
                tileX = fetcherWindowTileX_;
            } else if (fetcherPositionX_ >= -16 && fetcherPositionX_ < -8) {
                tileX = scx_ >> 3;
            } else {
                tileX = (scx_ + fetcherPositionX_ + 8) / 8;
            }
            fetcherMapAddr_ = static_cast<uint16_t>(map +
                ((y >> 3) * 32) + (tileX & 0x1F));
            fetcherState_ = FetcherState::GetTileT2;
            break;
        }

        case FetcherState::GetTileT2:
            fetcherTileId_ = vram_[fetcherMapAddr_ - 0x8000];
            fetcherState_ = FetcherState::GetTileDataLowT1;
            break;

        case FetcherState::GetTileDataLowT1: {
            uint16_t base;
            if (lcdc_ & 0x10) {
                base = static_cast<uint16_t>(fetcherTileId_ * 16);
            } else {
                base = static_cast<uint16_t>(0x1000 +
                    static_cast<int8_t>(fetcherTileId_) * 16);
            }
            fetcherDataAddr_ = static_cast<uint16_t>(base +
                ((fetcherY() & 7) * 2));
            fetcherState_ = FetcherState::GetTileDataLowT2;
            break;
        }

        case FetcherState::GetTileDataLowT2:
            fetcherTileDataLow_ = vram_[fetcherDataAddr_];
            fetcherState_ = FetcherState::GetTileDataHighT1;
            break;

        case FetcherState::GetTileDataHighT1: {
            uint16_t base;
            if (lcdc_ & 0x10) {
                base = static_cast<uint16_t>(fetcherTileId_ * 16);
            } else {
                base = static_cast<uint16_t>(0x1000 +
                    static_cast<int8_t>(fetcherTileId_) * 16);
            }
            fetcherDataAddr_ = static_cast<uint16_t>(base +
                ((fetcherY() & 7) * 2) + 1);
            fetcherState_ = FetcherState::GetTileDataHighT2;
            break;
        }

        case FetcherState::GetTileDataHighT2:
            fetcherTileDataHigh_ = vram_[fetcherDataAddr_];
            if (fetcherFetchingWindow_) {
                fetcherWindowTileX_ = (fetcherWindowTileX_ + 1) & 31;
            }
            fetcherState_ = FetcherState::Push;
            [[fallthrough]];

        case FetcherState::Push: {
            if (!bgFifo_.empty()) return;

            if (wyTriggered_ && !(lcdc_ & 0x20) &&
                !disableWindowPixelInsertionGlitch_) {
                int logicalPosition = fetcherPositionX_ + 7;
                if (logicalPosition > 167) logicalPosition = 0;
                if (static_cast<int>(wx_) == logicalPosition) {
                    bgFifo_.push(FIFOPixel{});
                    return;
                }
            }

            for (int bit = 7; bit >= 0; bit--) {
                uint8_t colorLo = (fetcherTileDataLow_ >> bit) & 1;
                uint8_t colorHi = (fetcherTileDataHigh_ >> bit) & 1;
                uint8_t colorIdx = (colorHi << 1) | colorLo;

                FIFOPixel p{};
                p.color = colorIdx;
                p.palette = 0;
                p.bgPriority = false;
                p.isSprite = false;
                bgFifo_.push(p);
            }

            fetcherState_ = FetcherState::GetTileT1;
            break;
        }
    }
}

// ══════════════════════════════════════════════════════════════════════
// FIFO output and DMG BG/OBJ priority mixing
// ══════════════════════════════════════════════════════════════════════

void PPU::renderPixelIfPossible() {
    if (currentSpriteIdx_ < lineSpriteCount_ && (lcdc_ & 0x02) &&
        lineSprites_[currentSpriteIdx_].x == 0) {
        return;
    }
    if (bgFifo_.empty()) return;

    FIFOPixel bg{};
    if (insertBgPixel_) {
        insertBgPixel_ = false;
    } else {
        bg = bgFifo_.pop();
    }

    FIFOPixel obj{};
    if (!objFifo_.empty()) obj = objFifo_.pop();

    if (fetcherPositionX_ >= -16 && fetcherPositionX_ < -8) {
        if ((fetcherPositionX_ & 7) == (scx_ & 7)) {
            fetcherPositionX_ = -8;
        } else if (windowBeingFetched_ && (fetcherPositionX_ & 7) == 6 &&
                   (scx_ & 7) == 7) {
            fetcherPositionX_ = -8;
        } else if (fetcherPositionX_ == -9) {
            fetcherPositionX_ = -16;
            return;
        } else {
            lineHasFractionalScrolling_ = true;
        }
    }

    windowBeingFetched_ = false;

    if (fetcherPositionX_ < 0) {
        fetcherPositionX_++;
        return;
    }

    if (pixelX_ < SCREEN_WIDTH && ly_ < SCREEN_HEIGHT) {
        bool bgEnabled = (lcdc_ & 0x01) != 0;
        uint8_t bgColor = bgEnabled ? bg.color : 0;
        bool drawObj = obj.isSprite && obj.color != 0 && (lcdc_ & 0x02);
        if (drawObj && bgColor != 0 && obj.bgPriority && bgEnabled) {
            drawObj = false;
        }

        if (drawObj) {
            framebuffer_[ly_ * SCREEN_WIDTH + pixelX_] =
                applyPalette(obj.color, obj.palette);
        } else {
            framebuffer_[ly_ * SCREEN_WIDTH + pixelX_] =
                applyPalette(bgColor, 0);
        }
    }
    pixelX_++;
    fetcherPositionX_++;
}

// ══════════════════════════════════════════════════════════════════════
// Mode transition
// ══════════════════════════════════════════════════════════════════════

void PPU::setMode(uint8_t newMode) {
    mode_ = newMode;
    stat_ = (stat_ & 0xFC) | newMode;
    updateStatIRQ();
}

// ══════════════════════════════════════════════════════════════════════
// LYC coincidence check
// ══════════════════════════════════════════════════════════════════════

void PPU::checkLYC() {
    if (ly_ == lyc_) {
        stat_ |= 0x04;
    } else {
        stat_ &= ~0x04;
    }
}

// ══════════════════════════════════════════════════════════════════════
// STAT interrupt — rising-edge detection on combined IRQ line
//
// On DMG, all STAT interrupt sources share a single internal OR'd line.
// The IF bit 1 is only set on a rising edge (0→1 transition).
// This prevents duplicate interrupts when transitioning between
// modes that both have their STAT interrupt enabled.
//
// Additional DMG quirk: Mode 2 (OAM) interrupt source is also
// active during the transition from line 143→144 (VBlank start),
// meaning a Mode 2 interrupt fires at vblank if enabled.
// ══════════════════════════════════════════════════════════════════════

void PPU::updateStatIRQ() {
    bool line = false;

    // Mode 0 (HBlank) interrupt source
    if ((stat_ & 0x08) && mode_ == MODE_HBLANK) line = true;

    // Mode 1 (VBlank) interrupt source
    if ((stat_ & 0x10) && mode_ == MODE_VBLANK) line = true;

    // Mode 2 (OAM) interrupt source
    // On DMG, this fires at OAM mode start AND briefly pulses at VBlank
    // entry. The pulse is transient — it does NOT hold the line high
    // throughout VBlank, allowing a proper rising edge at LY=0 OAM.
    if ((stat_ & 0x20) && (mode_ == MODE_OAM || vblankOamPulse_ || oamStatEarly_)) line = true;

    // LYC coincidence interrupt source
    if ((stat_ & 0x40) && (stat_ & 0x04)) line = true;

    // Rising edge detection
    if (!statIrqLine_ && line) {
        if (ifReg_) {
            *ifReg_ |= 0x02;
        }
    }



    statIrqLine_ = line;
}

// ══════════════════════════════════════════════════════════════════════
// Palette application
// ══════════════════════════════════════════════════════════════════════

uint32_t PPU::applyPalette(uint8_t colorIdx, uint8_t palette) const {
    uint8_t pal;
    switch (palette) {
        case 0: pal = bgp_;  break;
        case 1: pal = obp0_; break;
        case 2: pal = obp1_; break;
        default: pal = bgp_; break;
    }

    uint8_t shade = (pal >> (colorIdx * 2)) & 0x03;
    return dmgColors_[shade];
}

// ══════════════════════════════════════════════════════════════════════
// Register reads (FF40–FF4B)
// ══════════════════════════════════════════════════════════════════════

uint8_t PPU::readReg(uint16_t addr) const {
    switch (addr) {
        case 0xFF40: return lcdc_;
        case 0xFF41: return (stat_ & 0x7F) | 0x80;
        case 0xFF42: return scy_;
        case 0xFF43: return scx_;
        case 0xFF44: return ly_;
        case 0xFF45: return lyc_;
        case 0xFF46: return dma_;
        case 0xFF47: return bgp_;
        case 0xFF48: return obp0_;
        case 0xFF49: return obp1_;
        case 0xFF4A: return wy_;
        case 0xFF4B: return wx_;
        default:     return 0xFF;
    }
}

// ══════════════════════════════════════════════════════════════════════
// Register writes (FF40–FF4B)
// ══════════════════════════════════════════════════════════════════════

uint8_t PPU::beginDMGLCDCWrite(uint8_t val) {
    uint8_t old = lcdc_;

    // LCDC.1 is sampled independently by FIFO output and object fetching.
    // A disable at X=0 or during an active fetch reaches that path on the
    // conflict dot instead of waiting for the final register value.
    if (!(val & 0x02) && (fetcherPositionX_ == 0 || objFetchActive_)) {
        old &= static_cast<uint8_t>(~0x02);
    }

    if ((lcdc_ & 0x20) && !(val & 0x20) && windowBeingFetched_) {
        disableWindowPixelInsertionGlitch_ = true;
    }

    // All bits read old on this dot except an asserted BG-enable bit.
    return static_cast<uint8_t>(old | (val & 0x01));
}

void PPU::writeReg(uint16_t addr, uint8_t val) {
    switch (addr) {
        case 0xFF40: {
            bool wasOn = lcdc_ & 0x80;
            uint8_t oldLcdc = lcdc_;
            lcdc_ = val;
            bool isOn = lcdc_ & 0x80;

            if ((oldLcdc & 0x02) && !(val & 0x02) && objFetchActive_) {
                objFetchActive_ = false;
            }
            if (isOn && (val & 0x20) && wy_ == ly_) {
                wyTriggered_ = true;
            }

            if (wasOn && !isOn) {
                // LCD turning off
                ly_ = 0;
                dotCounter_ = 0;
                mode_ = MODE_HBLANK;
                stat_ = (stat_ & 0xFC);
                lcdWasOff_ = true;
                wyTriggered_ = false;
                windowTriggered_ = false;
                windowY_ = -1;
                // Coincidence flag is RETAINED (not cleared)
                // Freeze STAT IRQ line based on retained coincidence flag.
                statIrqLine_ = ((stat_ & 0x40) && (stat_ & 0x04));
            } else if (!wasOn && isOn) {
                lcdWasOff_ = true;
                firstLineAfterEnable_ = true;
                wyTriggered_ = false;
                windowY_ = -1;
            }
            break;
        }
        case 0xFF41:
            // Bits 0-2 are read-only (mode + coincidence flag)
            stat_ = (stat_ & 0x07) | (val & 0x78);
            // DMG STAT write glitch: spurious interrupt if line was low
            if (lcdc_ & 0x80) {
                if (!statIrqLine_ && (val & 0x78)) {
                    if (ifReg_) *ifReg_ |= 0x02;
                }
                updateStatIRQ();
            }
            break;
        case 0xFF42: scy_ = val; break;
        case 0xFF43: scx_ = val; break;
        case 0xFF44: break; // LY is read-only
        case 0xFF45:
            lyc_ = val;
            if (lcdc_ & 0x80) {
                checkLYC();
                updateStatIRQ();
            }
            break;
        case 0xFF46: dma_ = val; break;
        case 0xFF47: bgp_ = val; break;
        case 0xFF48: obp0_ = val; break;
        case 0xFF49: obp1_ = val; break;
        case 0xFF4A:
            wy_ = val;
            if ((lcdc_ & 0xA0) == 0xA0 && wy_ == ly_) wyTriggered_ = true;
            break;
        case 0xFF4B:
            wx_ = val;
            if (mode_ == MODE_XFER) wxJustChangedDots_ = 1;
            break;
    }
}

// ══════════════════════════════════════════════════════════════════════
// VRAM access with mode-dependent blocking
// ══════════════════════════════════════════════════════════════════════

uint8_t PPU::readVRAM(uint16_t addr) const {
    if ((lcdc_ & 0x80) && mode_ == MODE_XFER) {
        return 0xFF;
    }
    if ((lcdc_ & 0x80) && mode_ == MODE_OAM && dotCounter_ >= (OAM_DOTS + 3)) {
        // VRAM pre-blocking: VRAM becomes inaccessible 1 dot before mode 3
        return 0xFF;
    }
    return vram_[addr - 0x8000];
}

void PPU::writeVRAM(uint16_t addr, uint8_t val) {
    if ((lcdc_ & 0x80) && mode_ == MODE_XFER) {
        return;
    }
    vram_[addr - 0x8000] = val;
}

// ══════════════════════════════════════════════════════════════════════
// OAM access with mode-dependent blocking
// ══════════════════════════════════════════════════════════════════════

uint8_t PPU::readOAM(uint16_t addr) const {
    if ((lcdc_ & 0x80) && (mode_ == MODE_OAM || mode_ == MODE_XFER)) {
        return 0xFF;
    }
    // Pre-OAM blocking: OAM reads return $FF 1 dot before mode 2 starts
    if ((lcdc_ & 0x80) && !firstLineAfterEnable_ && mode_ == MODE_HBLANK
        && dotCounter_ >= 3 && dotCounter_ <= 4 && ly_ < VISIBLE_LINES) {
        return 0xFF;
    }
    return oam_[addr - 0xFE00];
}

void PPU::writeOAM(uint16_t addr, uint8_t val) {
    if (lcdc_ & 0x80) {
        // During pixel transfer: writes always blocked
        if (mode_ == MODE_XFER) return;
        // During OAM search: writes blocked until the last dot before mode 3
        // (SameBoy: oam_write_blocked cleared at end of OAM search)
        if (mode_ == MODE_OAM && dotCounter_ < (OAM_DOTS + 3)) return;
    }
    oam_[addr - 0xFE00] = val;
}

// ══════════════════════════════════════════════════════════════════════
// OAM DMA write — bypasses mode blocking
// ══════════════════════════════════════════════════════════════════════

// The OAM scanner uses one 8-byte row per M-cycle. In this PPU's timing
// representation, mode-2 dots 4-79 correspond to corruptible rows 1-19.
int PPU::accessedOAMRow() const {
    if (!(lcdc_ & 0x80) || mode_ != MODE_OAM) return -1;
    if (dotCounter_ < 4 || dotCounter_ >= OAM_DOTS + 4) return -1;
    // This PPU enters mode 2 at dot 4, after the hardware's initial row-0
    // access. Thus its first mode-2 M-cycle corresponds to row 1.
    return (dotCounter_ / 4) * 8;
}

uint16_t PPU::readOAMWord(int offset) const {
    return static_cast<uint16_t>(oam_[offset]) |
           (static_cast<uint16_t>(oam_[offset + 1]) << 8);
}

void PPU::writeOAMWord(int offset, uint16_t value) {
    oam_[offset] = static_cast<uint8_t>(value);
    oam_[offset + 1] = static_cast<uint8_t>(value >> 8);
}

static uint16_t oamWriteGlitch(uint16_t a, uint16_t b, uint16_t c) {
    return ((a ^ c) & (b ^ c)) ^ c;
}

static uint16_t oamReadGlitch(uint16_t a, uint16_t b, uint16_t c) {
    return b | (a & c);
}

static uint16_t oamSecondaryReadGlitch(uint16_t a, uint16_t b,
                                       uint16_t c, uint16_t d) {
    return (b & (a | c | d)) | (a & c & d);
}

static uint16_t oamTertiaryReadGlitch1(uint16_t a, uint16_t b,
                                      uint16_t c, uint16_t d, uint16_t e) {
    return c | (a & b & d & e);
}

static uint16_t oamTertiaryReadGlitch2(uint16_t a, uint16_t b,
                                      uint16_t c, uint16_t d, uint16_t e) {
    return (c & (a | b | d | e)) | (a & b & d & e);
}

static uint16_t oamTertiaryReadGlitch3(uint16_t a, uint16_t b,
                                      uint16_t c, uint16_t d, uint16_t e) {
    return (c & (a | b | d | e)) | (b & d & e);
}

static uint16_t oamQuaternaryReadGlitchDMG(uint16_t b, uint16_t c,
                                           uint16_t d, uint16_t e,
                                           uint16_t f, uint16_t g,
                                           uint16_t h) {
    return (e & (h | g | (~d & f) | c | b)) | (c & g & h);
}

void PPU::triggerOAMWriteCorruption(uint16_t addr) {
    if (addr < 0xFE00 || addr >= 0xFF00) return;

    const int row = accessedOAMRow();
    if (row < 8 || row > 0x98) return;

    writeOAMWord(row, oamWriteGlitch(readOAMWord(row),
                                     readOAMWord(row - 8),
                                     readOAMWord(row - 4)));
    for (int i = 2; i < 8; ++i) {
        oam_[row + i] = oam_[row - 8 + i];
    }
}

void PPU::triggerOAMReadCorruption(uint16_t addr) {
    if (addr < 0xFE00 || addr >= 0xFF00) return;

    const int row = accessedOAMRow();
    if (row < 8 || row > 0x98) return;

    if ((row & 0x18) == 0x10) {
        if (row < 0x98) {
            writeOAMWord(row - 8,
                oamSecondaryReadGlitch(readOAMWord(row - 16),
                                       readOAMWord(row - 8),
                                       readOAMWord(row),
                                       readOAMWord(row - 4)));
            for (int i = 0; i < 8; ++i) {
                oam_[row - 16 + i] = oam_[row - 8 + i];
            }
        }
    } else if ((row & 0x18) == 0) {
        if (row < 0x98) {
            uint16_t value;
            if (row == 0x40) {
                value = oamQuaternaryReadGlitchDMG(
                    readOAMWord(row), readOAMWord(row - 4),
                    readOAMWord(row - 6), readOAMWord(row - 8),
                    readOAMWord(row - 14), readOAMWord(row - 16),
                    readOAMWord(row - 32));
            } else {
                const uint16_t a = readOAMWord(row);
                const uint16_t b = readOAMWord(row - 4);
                const uint16_t c = readOAMWord(row - 8);
                const uint16_t d = readOAMWord(row - 16);
                const uint16_t e = readOAMWord(row - 32);
                if (row == 0x20) value = oamTertiaryReadGlitch2(a, b, c, d, e);
                else if (row == 0x60) value = oamTertiaryReadGlitch3(a, b, c, d, e);
                else value = oamTertiaryReadGlitch1(a, b, c, d, e);
            }
            writeOAMWord(row - 8, value);
            for (int i = 0; i < 8; ++i) {
                oam_[row - 16 + i] = oam_[row - 8 + i];
                oam_[row - 32 + i] = oam_[row - 8 + i];
            }
        }
    } else {
        const uint16_t value = oamReadGlitch(readOAMWord(row),
                                             readOAMWord(row - 8),
                                             readOAMWord(row - 4));
        writeOAMWord(row - 8, value);
        writeOAMWord(row, value);
    }

    for (int i = 0; i < 8; ++i) {
        oam_[row + i] = oam_[row - 8 + i];
    }
    if (row == 0x80) {
        for (int i = 0; i < 8; ++i) oam_[i] = oam_[row + i];
    }
}

void PPU::dmaWriteOAM(uint8_t index, uint8_t val) {
    if (index < 0xA0) {
        oam_[index] = val;
    }
}
