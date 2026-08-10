#pragma once

#include <array>
#include <cstdint>

class SaveState;

// ══════════════════════════════════════════════════════════════════════
// Hardware-Accurate Game Boy DMG PPU — Per-Dot State Machine
//
// Models the PPU at T-cycle (dot) granularity with:
//   • Pixel FIFO for background/window rendering
//   • OAM sprite evaluation with 10-per-line limit
//   • STAT interrupt blocking (single IRQ line, rising-edge only)
//   • VRAM/OAM access restrictions by mode
//   • Window internal line counter
//   • Proper LCD on/off behavior
// ══════════════════════════════════════════════════════════════════════

class PPU {
public:
    // Provide a pointer to the IF register (FF0F) in IO memory
    void connectIF(uint8_t* ifReg) { ifReg_ = ifReg; }

    // Reset PPU to power-on state for boot ROM execution.
    void resetForBootrom() {
        lcdc_ = 0x00;
        stat_ = 0x00;
        scy_  = 0x00;  scx_  = 0x00;
        ly_   = 0x00;  lyc_  = 0x00;
        dma_  = 0x00;
        bgp_  = 0x00;  obp0_ = 0x00;  obp1_ = 0x00;
        wy_   = 0x00;  wx_   = 0x00;
        mode_ = MODE_HBLANK;
        lcdWasOff_ = true;
        lcdEnableDelayDots_ = 2;
        dotCounter_ = 0;
        vblankLine_ = 0;
        statIrqLine_ = false;
        frameReady_ = false;
        firstLineAfterEnable_ = false;
        mode0StatDelay_ = false;
        firstLineShorter_ = false;
        windowTriggered_ = false;
        windowBeingFetched_ = false;
        wyTriggered_ = false;
        windowY_ = -1;
        insertBgPixel_ = false;
        disableWindowPixelInsertionGlitch_ = false;
        lineHasFractionalScrolling_ = false;
        wxJustChangedDots_ = 0;
        bgFifo_.clear();
        objFifo_.clear();
        fetcherState_ = FetcherState::GetTileT1;
        fetcherPositionX_ = -16;
        fetcherWindowTileX_ = 0;
        fetcherFetchingWindow_ = false;
        lineSpriteCount_ = 0;
        oamSearchIndex_ = 0;
        objFetchActive_ = false;
        objFetchPhase_ = 0;
    }

    // Advance the PPU by exactly 1 T-cycle
    void tick();

    // ── Register access (FF40–FF4B) ─────────────────────────────────
    uint8_t readReg(uint16_t addr) const;
    void    writeReg(uint16_t addr, uint8_t val);

    // Value driven to LCDC during the DMG's one-dot CPU/PPU bus conflict.
    // This also captures window-fetch state needed by the disable glitch.
    uint8_t beginDMGLCDCWrite(uint8_t val);

    // ── VRAM access (0x8000–0x9FFF) with mode-dependent blocking ────
    uint8_t readVRAM(uint16_t addr) const;
    void    writeVRAM(uint16_t addr, uint8_t val);

    // ── Direct VRAM read — bypasses mode blocking (used by OAM DMA) ──
    uint8_t directReadVRAM(uint16_t addr) const {
        return vram_[addr - 0x8000];
    }

    // ── OAM access (0xFE00–0xFE9F) with mode-dependent blocking ────
    uint8_t readOAM(uint16_t addr) const;
    void    writeOAM(uint16_t addr, uint8_t val);

    // DMG OAM corruption caused by CPU address-bus activity in mode 2.
    void triggerOAMWriteCorruption(uint16_t addr);
    void triggerOAMReadCorruption(uint16_t addr);

    // ── OAM DMA: bypasses mode blocking ─────────────────────────────
    void dmaWriteOAM(uint8_t index, uint8_t val);

    // ── Framebuffer output ──────────────────────────────────────────
    bool frameReady() const { return frameReady_; }
    void clearFrameReady() { frameReady_ = false; }
    const uint32_t* framebuffer() const { return framebuffer_.data(); }

    // ── State queries ───────────────────────────────────────────────
    uint8_t currentMode() const { return mode_; }
    uint8_t currentLY()   const { return ly_; }
    int     currentDot()  const { return dotCounter_; }
    bool    lcdEnabled()  const { return lcdc_ & 0x80; }

    // Save state serialization
    void serialize(SaveState& ss) const;
    void deserialize(SaveState& ss);

private:
    // ── PPU modes ───────────────────────────────────────────────────
    static constexpr uint8_t MODE_HBLANK = 0;
    static constexpr uint8_t MODE_VBLANK = 1;
    static constexpr uint8_t MODE_OAM    = 2;
    static constexpr uint8_t MODE_XFER   = 3;

    // ── Timing constants ────────────────────────────────────────────
    static constexpr int DOTS_PER_LINE = 456;
    static constexpr int FIRST_LINE_DOTS = 448;  // First line after LCD enable is 8 dots shorter
    static constexpr int VISIBLE_LINES = 144;
    static constexpr int TOTAL_LINES   = 154;
    static constexpr int OAM_DOTS      = 80;
    static constexpr int SCREEN_WIDTH  = 160;
    static constexpr int SCREEN_HEIGHT = 144;

    // ── LCD registers ───────────────────────────────────────────────
    uint8_t lcdc_ = 0x91; // FF40 — LCD Control (LCD on, BG on by default)
    uint8_t stat_ = 0x00; // FF41 — STAT (mode bits set by PPU)
    uint8_t scy_  = 0x00; // FF42 — Scroll Y
    uint8_t scx_  = 0x00; // FF43 — Scroll X
    uint8_t ly_   = 0x00; // FF44 — LY (current scanline)
    uint8_t lyc_  = 0x00; // FF45 — LY Compare
    uint8_t dma_  = 0xFF; // FF46 — DMA (write-only trigger, reads return last val)
    uint8_t bgp_  = 0xFC; // FF47 — BG Palette
    uint8_t obp0_ = 0x00; // FF48 — OBJ Palette 0
    uint8_t obp1_ = 0x00; // FF49 — OBJ Palette 1
    uint8_t wy_   = 0x00; // FF4A — Window Y
    uint8_t wx_   = 0x00; // FF4B — Window X

    // ── Internal state ──────────────────────────────────────────────
    uint8_t mode_ = MODE_OAM;  // Current PPU mode
    int dotCounter_ = 0;       // T-cycle counter within current scanline (0–455)
    int vblankLine_ = 0;       // VBlank line counter (0–9, for LY=153 early reset quirk)
    bool lcdWasOff_ = false;   // Tracks LCD just-enabled state
    int lcdEnableDelayDots_ = 0; // DMG PPU begins two dots after LCDC enable
    bool firstLineAfterEnable_ = false; // Line 0 after LCD enable has special timing
    bool mode0StatDelay_ = false;       // 1-dot delay for Mode 0 STAT IRQ (DMG behavior)
    bool firstLineShorter_ = false;     // First line after LCD enable is 448 dots (not 456)

    // ── STAT interrupt line (rising-edge detection) ─────────────────
    bool statIrqLine_ = false;  // Previous combined STAT IRQ signal
    bool vblankOamPulse_ = false; // One-shot pulse: mode 2 OAM source at VBlank entry
    bool oamStatEarly_ = false;
    int mode3StartDot_ = 0;       // Debug: measure mode 3 duration
    int mode3StartupDots_ = 0;    // VRAM/OAM blocking before the fetch loop

    // ── Framebuffer ─────────────────────────────────────────────────
    std::array<uint32_t, SCREEN_WIDTH * SCREEN_HEIGHT> framebuffer_{};
    bool frameReady_ = false;

    // ── VRAM & OAM (owned by PPU) ───────────────────────────────────
    std::array<uint8_t, 0x2000> vram_{};  // 8000–9FFF
    std::array<uint8_t, 0xA0>  oam_{};   // FE00–FE9F

    // ── IF register reference ───────────────────────────────────────
    uint8_t* ifReg_ = nullptr;

    // ── Sprite evaluation ───────────────────────────────────────────
    struct Sprite {
        uint8_t y;
        uint8_t x;
        uint8_t tile;
        uint8_t flags;
        uint8_t oamIndex;  // Original OAM position (for priority)
    };
    std::array<Sprite, 10> lineSprites_{};
    int lineSpriteCount_ = 0;
    int oamSearchIndex_ = 0;

    // ── Background pixel FIFO ───────────────────────────────────────
    // Each FIFO entry holds a 2-bit color index + source info
    struct FIFOPixel {
        uint8_t color;    // 2-bit palette index (0–3)
        uint8_t palette;  // 0=BGP, 1=OBP0, 2=OBP1
        bool    bgPriority; // BG-over-OBJ flag (from sprite attributes)
        bool    isSprite;
    };

    // Hardware shift-register FIFOs are eight pixels wide.
    static constexpr int FIFO_SIZE = 8;
    struct PixelFIFO {
        FIFOPixel pixels[FIFO_SIZE]{};
        int head = 0;
        int count = 0;

        void clear() { head = 0; count = 0; }
        bool empty() const { return count == 0; }
        int size() const { return count; }

        void push(const FIFOPixel& p) {
            int idx = (head + count) % FIFO_SIZE;
            pixels[idx] = p;
            count++;
        }

        FIFOPixel pop() {
            FIFOPixel p = pixels[head];
            head = (head + 1) % FIFO_SIZE;
            count--;
            return p;
        }

        FIFOPixel& at(int i) {
            return pixels[(head + i) % FIFO_SIZE];
        }
    };

    PixelFIFO bgFifo_;
    PixelFIFO objFifo_;

    // ── Tile fetcher state machine ──────────────────────────────────
    enum class FetcherState {
        GetTileT1,
        GetTileT2,
        GetTileDataLowT1,
        GetTileDataLowT2,
        GetTileDataHighT1,
        GetTileDataHighT2,
        Push
    };

    FetcherState fetcherState_ = FetcherState::GetTileT1;
    uint16_t fetcherMapAddr_ = 0;
    uint16_t fetcherDataAddr_ = 0;
    int fetcherWindowTileX_ = 0;
    uint8_t fetcherTileId_ = 0;
    uint8_t fetcherTileDataLow_ = 0;
    uint8_t fetcherTileDataHigh_ = 0;
    bool fetcherFetchingWindow_ = false;

    // ── Pixel transfer state ────────────────────────────────────────
    int pixelX_ = 0;               // LCD X (0–159)
    int fetcherPositionX_ = -16;    // PPU's signed logical X
    bool windowTriggered_ = false;  // WX comparator has activated window
    bool windowBeingFetched_ = false;
    bool wyTriggered_ = false;      // WY comparator latch for this frame
    int windowY_ = -1;              // Internal window line, increments on activation
    bool insertBgPixel_ = false;     // DMG window-reactivation color-zero pixel
    bool disableWindowPixelInsertionGlitch_ = false;
    bool lineHasFractionalScrolling_ = false;
    int wxJustChangedDots_ = 0;

    // ── Sprite fetch state ──────────────────────────────────────────
    bool objFetchActive_ = false;
    int objFetchPhase_ = 0;
    int currentSpriteIdx_ = 0;
    uint8_t objTile_ = 0;
    uint8_t objFlags_ = 0;
    uint8_t objDataLow_ = 0;
    uint8_t objDataHigh_ = 0;
    uint16_t objDataAddr_ = 0;

    // ── DMG palette color lookup table ──────────────────────────────
    static constexpr uint32_t dmgColors_[4] = {
        0xFFE0F8D0, // Lightest (white-ish green)
        0xFF88C070, // Light
        0xFF346856, // Dark
        0xFF081820  // Darkest
    };

    // ── Internal methods ────────────────────────────────────────────
    void tickOAMSearch();
    void tickPixelTransfer();
    void tickHBlank();
    void tickVBlank();

    void scanOAMEntry(int index);
    void sortLineSprites();
    void beginPixelTransfer(int startupDots);
    bool handleWindow();
    bool beginObjectFetchIfNeeded();
    void tickObjectFetch();
    void advanceFetcher();
    void renderPixelIfPossible();
    void overlayObjectRow();
    uint16_t objectLineAddress(uint8_t y, uint8_t tile, uint8_t flags) const;
    uint8_t fetcherY() const;

    int accessedOAMRow() const;
    uint16_t readOAMWord(int offset) const;
    void writeOAMWord(int offset, uint16_t value);

    void setMode(uint8_t newMode);
    void checkLYC();
    void updateStatIRQ();

    uint32_t applyPalette(uint8_t colorIdx, uint8_t palette) const;
};
