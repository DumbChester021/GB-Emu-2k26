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
        windowLineCounter_ = 0;
        pixelX_ = 0;
        bgFifo_.clear();
        fetcherState_ = FetcherState::ReadTileID;
        fetcherClock_ = 0;
        fetcherTileX_ = 0;
        lineSpriteCount_ = 0;
        spriteFetchPending_ = false;

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
    // Sprite evaluation on first dot of OAM search
    if (dotCounter_ == 5) {
        evaluateSprites();
    }

    if (dotCounter_ >= OAM_DOTS + 4) {
        // Transition to Mode 3 (Pixel Transfer) at dot 84
        // (OAM_DOTS=80 + 4 pre-OAM dots)
        setMode(MODE_XFER);
        mode3StartDot_ = dotCounter_;
        mode3PenaltyDots_ = 5;  // Initial penalty before first pixel

        // ── SCX M-cycle alignment penalty ────────────────────────────
        // The FIFO pixel discard adds exactly (SCX%8) extra dots to Mode 3.
        // But Mode 0 entry must align to 4-dot (M-cycle) boundaries so the
        // STAT interrupt fires at the correct M-cycle. This adds padding
        // dots to round the SCX penalty up to the next M-cycle boundary.
        //   SCX%8 = 0:     0 extra dots (Mode 0 at dot 256)
        //   SCX%8 = 1-4: +3/+2/+1/+0 dots → total 4 (Mode 0 at dot 260)
        //   SCX%8 = 5-7: +3/+2/+1 dots   → total 8 (Mode 0 at dot 264)
        int scxFine = scx_ & 7;
        if (scxFine > 0) {
            mode3PenaltyDots_ += (4 - (scxFine % 4)) % 4;
        }

        // ── Sprite mode 3 penalties ─────────────────────────────────
        // On DMG, each sprite extends mode 3. The cost per sprite is:
        //   6 dots (fixed sprite fetch) + alignment to fetcher state
        // Sprites at the same X share the alignment cost.
        // Sprites at X >= 168 are fully off-screen right (no penalty).
        for (int i = 0; i < lineSpriteCount_; i++) {
            int sx = lineSprites_[i].x;
            if (sx >= 168) continue;

            // Check if a previous sprite already paid alignment for this X
            bool alignmentPaid = false;
            for (int j = 0; j < i; j++) {
                if (lineSprites_[j].x == sx) {
                    alignmentPaid = true;
                    break;
                }
            }

            if (!alignmentPaid) {
                mode3PenaltyDots_ += std::max(0, 5 - static_cast<int>((sx + scx_) % 8));
            }
            mode3PenaltyDots_ += 6;  // Fixed per-sprite cost
        }

        pixelX_ = 0;
        discardPixels_ = scx_ & 7;
        bgFifo_.clear();
        fetcherState_ = FetcherState::ReadTileID;
        fetcherClock_ = 0;
        fetcherTileX_ = 0;
        windowTriggered_ = false;
        spriteFetchPending_ = false;
        currentSpriteIdx_ = 0;
    }
}

// ══════════════════════════════════════════════════════════════════════
// Mode 3: Pixel Transfer (variable length, ~172 dots minimum)
// ══════════════════════════════════════════════════════════════════════

void PPU::tickPixelTransfer() {
    // Initial penalty dots: DMG needs 12 dots before first pixel push
    // (our fetcher provides 7 dots naturally, this adds 5 more)
    if (mode3PenaltyDots_ > 0) {
        mode3PenaltyDots_--;
        return;
    }

    // Check if window should activate
    if (!windowTriggered_ && (lcdc_ & 0x20)) {
        if (ly_ >= wy_) {
            int wxTrigger = wx_ - 7;
            if (wxTrigger < 0) wxTrigger = 0;
            if (wxTrigger < SCREEN_WIDTH && pixelX_ == wxTrigger) {
                windowTriggered_ = true;
                fetcherFetchingWindow_ = true;
                fetcherState_ = FetcherState::ReadTileID;
                fetcherClock_ = 0;
                fetcherTileX_ = 0;
                bgFifo_.clear();
                // WX < 7: clip first (7 - WX) pixels of the window tile
                discardPixels_ = (wx_ < 7) ? (7 - wx_) : 0;
            }
        }
    }

    // Run the tile fetcher
    fetcherTick();

    // Try to push a pixel to the screen
    if (bgFifo_.size() > 0) {
        // Check for sprite at current pixel X before pushing
        if (lcdc_ & 0x02) {
            for (int i = 0; i < lineSpriteCount_; i++) {
                int spriteScreenX = lineSprites_[i].x - 8;
                if (spriteScreenX <= pixelX_ && (spriteScreenX + 8) > pixelX_) {
                    mixSpritePixel(i);
                }
            }
        }
        pushPixel();
    }

    if (pixelX_ >= SCREEN_WIDTH) {
        setMode(MODE_HBLANK);
        fetcherFetchingWindow_ = false;
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
            setMode(MODE_XFER);
            mode3StartDot_ = dotCounter_;
            mode3PenaltyDots_ = 6;  // Initial penalty: 6 penalty + 6 fetch (PushToFIFO instant) = 12 dots
            pixelX_ = 0;
            discardPixels_ = scx_ & 7;
            bgFifo_.clear();
            fetcherState_ = FetcherState::ReadTileID;
            fetcherClock_ = 0;
            fetcherTileX_ = 0;
            windowTriggered_ = false;
            spriteFetchPending_ = false;
            currentSpriteIdx_ = 0;
            lineSpriteCount_ = 0;  // No sprites on first line
            return;
        }
        return;  // Stay in mode 0 until dot 78
    }

    // Pre-OAM transition: normal lines stay in mode 0 for 4 dots before mode 2
    if (dotCounter_ <= 4 && mode_ == MODE_HBLANK) {
        if (dotCounter_ == 4) {
            setMode(MODE_OAM);
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

        // Track window line counter — only increments when the window
        // actually rendered on this scanline (windowTriggered_).
        // Per Pan Docs / SameBoy: counter does NOT advance when WX is
        // offscreen, even if LY >= WY.
        if (windowTriggered_) {
            windowLineCounter_++;
        }

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
            windowLineCounter_ = 0;
            setMode(MODE_OAM);
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
// Sprite evaluation — scan OAM for sprites on the current scanline
// ══════════════════════════════════════════════════════════════════════

void PPU::evaluateSprites() {
    lineSpriteCount_ = 0;
    int spriteHeight = (lcdc_ & 0x04) ? 16 : 8;

    for (int i = 0; i < 40 && lineSpriteCount_ < 10; i++) {
        uint8_t spriteY = oam_[i * 4 + 0];
        uint8_t spriteX = oam_[i * 4 + 1];
        uint8_t tileIdx = oam_[i * 4 + 2];
        uint8_t flags   = oam_[i * 4 + 3];

        int screenY = spriteY - 16;

        if (ly_ >= screenY && ly_ < screenY + spriteHeight) {
            Sprite& s = lineSprites_[lineSpriteCount_];
            s.y = spriteY;
            s.x = spriteX;
            s.tile = tileIdx;
            s.flags = flags;
            s.oamIndex = i;
            lineSpriteCount_++;
        }
    }

    // DMG priority: sort by X position, then by OAM index
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

// ══════════════════════════════════════════════════════════════════════
// Tile fetcher — runs every 2 dots, feeding the BG/Window FIFO
// ══════════════════════════════════════════════════════════════════════

void PPU::fetcherTick() {
    fetcherClock_++;
    if (fetcherClock_ < 2) return;
    fetcherClock_ = 0;

    switch (fetcherState_) {
        case FetcherState::ReadTileID: {
            uint16_t tileMapBase;
            int tileX, tileY;

            if (fetcherFetchingWindow_) {
                tileMapBase = (lcdc_ & 0x40) ? 0x9C00 : 0x9800;
                tileX = fetcherTileX_;
                tileY = windowLineCounter_ / 8;
            } else {
                tileMapBase = (lcdc_ & 0x08) ? 0x9C00 : 0x9800;
                tileX = ((scx_ / 8) + fetcherTileX_) & 0x1F;
                tileY = ((ly_ + scy_) & 0xFF) / 8;
            }

            uint16_t tileMapAddr = tileMapBase + (tileY * 32) + tileX;
            fetcherTileId_ = vram_[tileMapAddr - 0x8000];
            fetcherState_ = FetcherState::ReadTileDataLow;
            break;
        }

        case FetcherState::ReadTileDataLow: {
            int line;
            if (fetcherFetchingWindow_) {
                line = windowLineCounter_ % 8;
            } else {
                line = (ly_ + scy_) % 8;
            }

            uint16_t tileDataBase;
            if (lcdc_ & 0x10) {
                tileDataBase = 0x8000 + (fetcherTileId_ * 16);
            } else {
                tileDataBase = 0x9000 + (static_cast<int8_t>(fetcherTileId_) * 16);
            }

            fetcherTileDataLow_ = vram_[(tileDataBase + line * 2) - 0x8000];
            fetcherState_ = FetcherState::ReadTileDataHigh;
            break;
        }

        case FetcherState::ReadTileDataHigh: {
            int line;
            if (fetcherFetchingWindow_) {
                line = windowLineCounter_ % 8;
            } else {
                line = (ly_ + scy_) % 8;
            }

            uint16_t tileDataBase;
            if (lcdc_ & 0x10) {
                tileDataBase = 0x8000 + (fetcherTileId_ * 16);
            } else {
                tileDataBase = 0x9000 + (static_cast<int8_t>(fetcherTileId_) * 16);
            }

            fetcherTileDataHigh_ = vram_[(tileDataBase + line * 2 + 1) - 0x8000];
            fetcherState_ = FetcherState::PushToFIFO;
            break;
        }

        case FetcherState::PushToFIFO: {
            if (bgFifo_.size() > 0) {
                // FIFO not empty — stall
                return;
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

            fetcherTileX_++;
            fetcherState_ = FetcherState::ReadTileID;
            break;
        }
    }
}

// ══════════════════════════════════════════════════════════════════════
// Push a pixel from the FIFO to the framebuffer
// ══════════════════════════════════════════════════════════════════════

void PPU::pushPixel() {
    if (bgFifo_.empty()) return;

    // Discard pixels for SCX fine scrolling
    if (discardPixels_ > 0) {
        bgFifo_.pop();
        discardPixels_--;
        return;
    }

    FIFOPixel pixel = bgFifo_.pop();

    if (pixelX_ < SCREEN_WIDTH && ly_ < SCREEN_HEIGHT) {
        // BG enable check (LCDC bit 0 on DMG)
        if (!(lcdc_ & 0x01)) {
            if (!pixel.isSprite) {
                pixel.color = 0;
            }
        }

        uint32_t color;
        if (pixel.isSprite) {
            color = applyPalette(pixel.color, pixel.palette);
        } else {
            color = applyPalette(pixel.color, 0);
        }

        framebuffer_[ly_ * SCREEN_WIDTH + pixelX_] = color;
        pixelX_++;
    }
}

// ══════════════════════════════════════════════════════════════════════
// Mix sprite pixel into the BG FIFO
// ══════════════════════════════════════════════════════════════════════

void PPU::mixSpritePixel(int spriteIdx) {
    const Sprite& sprite = lineSprites_[spriteIdx];

    int spriteHeight = (lcdc_ & 0x04) ? 16 : 8;
    int lineInSprite = ly_ - (sprite.y - 16);

    // Y-flip
    if (sprite.flags & 0x40) {
        lineInSprite = (spriteHeight - 1) - lineInSprite;
    }

    uint8_t tileIdx = sprite.tile;
    if (spriteHeight == 16) {
        tileIdx &= 0xFE;
    }

    uint16_t tileAddr = 0x8000 + (tileIdx * 16) + (lineInSprite * 2);
    uint8_t dataLo = vram_[tileAddr - 0x8000];
    uint8_t dataHi = vram_[tileAddr + 1 - 0x8000];

    int spriteScreenX = sprite.x - 8;
    bool bgOverObj = (sprite.flags & 0x80) != 0;
    uint8_t palNum = (sprite.flags & 0x10) ? 2 : 1;

    for (int bit = 7; bit >= 0; bit--) {
        int screenX = spriteScreenX + (7 - bit);

        int actualBit = bit;
        if (sprite.flags & 0x20) {
            actualBit = 7 - bit;
        }

        uint8_t colorLo = (dataLo >> actualBit) & 1;
        uint8_t colorHi = (dataHi >> actualBit) & 1;
        uint8_t colorIdx = (colorHi << 1) | colorLo;

        if (colorIdx == 0) continue;

        int fifoPos = screenX - pixelX_ + discardPixels_;
        if (fifoPos < 0 || fifoPos >= bgFifo_.size()) continue;

        FIFOPixel& existing = bgFifo_.at(fifoPos);
        if (existing.isSprite) continue;
        if (bgOverObj && existing.color != 0) continue;

        existing.color = colorIdx;
        existing.palette = palNum;
        existing.isSprite = true;
        existing.bgPriority = bgOverObj;
    }
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
    if ((stat_ & 0x20) && (mode_ == MODE_OAM || vblankOamPulse_)) line = true;

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

void PPU::writeReg(uint16_t addr, uint8_t val) {
    switch (addr) {
        case 0xFF40: {
            bool wasOn = lcdc_ & 0x80;
            lcdc_ = val;
            bool isOn = lcdc_ & 0x80;

            if (wasOn && !isOn) {
                // LCD turning off
                ly_ = 0;
                dotCounter_ = 0;
                mode_ = MODE_HBLANK;
                stat_ = (stat_ & 0xFC);
                lcdWasOff_ = true;
                // Coincidence flag is RETAINED (not cleared)
                // Freeze STAT IRQ line based on retained coincidence flag.
                statIrqLine_ = ((stat_ & 0x40) && (stat_ & 0x04));
            } else if (!wasOn && isOn) {
                lcdWasOff_ = true;
                firstLineAfterEnable_ = true;
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
        case 0xFF4A: wy_ = val; break;
        case 0xFF4B: wx_ = val; break;
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
