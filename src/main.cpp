#include <SDL2/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <string>
#include <deque>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>
#include <climits>

#include "cartridge.h"
#include "cpu.h"
#include "file_dialog.h"
#include "memory_bus.h"
#include "save_state.h"
#include "settings.h"
#include "apu.h"

// Game Boy DMG native resolution
constexpr int GB_WIDTH  = 160;
constexpr int GB_HEIGHT = 144;
constexpr int SCALE     = 4;

// Window dimensions
constexpr int WIN_WIDTH  = GB_WIDTH  * SCALE;  // 640
constexpr int WIN_HEIGHT = GB_HEIGHT * SCALE;  // 576

// ── Resolve executable directory ─────────────────────────────────────
// Returns the directory containing the executable (with trailing /).
// Used to locate the bootrom relative to the binary, not the CWD.
static std::string getExeDir() {
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) return "";
    buf[len] = '\0';
    std::string path(buf);
    size_t slash = path.find_last_of('/');
    if (slash != std::string::npos)
        return path.substr(0, slash + 1);
    return "";
}

// Try to load bootrom: first relative to executable, then relative to CWD
static bool tryLoadBootrom(MemoryBus& bus) {
    // Try exe-relative path (e.g. build/../bootroms/dmg_boot.bin)
    std::string exeDir = getExeDir();
    if (!exeDir.empty()) {
        if (bus.loadBootrom(exeDir + "../bootroms/dmg_boot.bin")) return true;
        if (bus.loadBootrom(exeDir + "bootroms/dmg_boot.bin")) return true;
    }
    // Fallback: CWD-relative
    if (bus.loadBootrom("bootroms/dmg_boot.bin")) return true;
    return false;
}

// ── Mooneye pass/fail check ──────────────────────────────────────────
// A passing test writes Fibonacci 3/5/8/13/21/34 to B/C/D/E/H/L.
// A failing test writes 0x42 to all of B/C/D/E/H/L.
enum class MooneyeResult { PASS, FAIL, TIMEOUT };

MooneyeResult checkMooneye(const CPU& cpu) {
    if (cpu.reg.b == 3  && cpu.reg.c == 5  &&
        cpu.reg.d == 8  && cpu.reg.e == 13 &&
        cpu.reg.h == 21 && cpu.reg.l == 34) {
        return MooneyeResult::PASS;
    }
    return MooneyeResult::FAIL;
}

// ── Headless test runner ─────────────────────────────────────────────
int runTest(const std::string& romPath) {
    auto cartridge = Cartridge::loadFromFile(romPath);
    if (!cartridge) {
        std::fprintf(stderr, "Failed to load ROM: %s\n", romPath.c_str());
        return 2;
    }

    MemoryBus bus;
    bus.init();
    bus.loadCartridge(&(*cartridge));

    // Load bootrom (tries exe-relative, then CWD-relative)
    tryLoadBootrom(bus);

    CPU cpu(bus);

    // Run up to ~120 million cycles (≈30 seconds of Game Boy time)
    // Needs to be large enough to cover bootrom (~1.2M cycles) + heavy tests
    constexpr uint64_t MAX_CYCLES = 120'000'000;
    int breakpointCount = 0;

    while (cpu.totalCycles() < MAX_CYCLES) {
        if (!cpu.tick()) {
            if (cpu.hitUnimplemented()) {
                std::fprintf(stderr, "UNIMPLEMENTED opcode — aborting test\n");
                return 2;
            }
            break;
        }

        if (cpu.hitMooneyeBreakpoint()) {
            cpu.clearMooneyeBreakpoint();
            // Skip breakpoints during boot ROM — the DMG boot ROM
            // contains LD B,B (0x40) at offset 0x5E
            if (bus.bootromActive()) continue;
            breakpointCount++;

            // The test executes LD B, B twice:
            // 1st: registers are set with result
            // 2nd: after serial output, followed by infinite JR loop
            // We check on the first breakpoint.
            if (breakpointCount == 1) {
                MooneyeResult result = checkMooneye(cpu);
                if (result == MooneyeResult::PASS) {
                    std::printf("PASS: %s\n", romPath.c_str());
                    return 0;
                } else {
                    std::printf("FAIL: %s (B=%02X C=%02X D=%02X E=%02X H=%02X L=%02X)\n",
                               romPath.c_str(),
                               cpu.reg.b, cpu.reg.c, cpu.reg.d,
                               cpu.reg.e, cpu.reg.h, cpu.reg.l);
                    return 1;
                }
            }
        }
    }

    std::printf("TIMEOUT: %s\n", romPath.c_str());
    return 2;
}

// ── PPM framebuffer dump ─────────────────────────────────────────────
static bool dumpFramebufferPPM(const PPU& ppu, const std::string& path) {
    // Ensure parent directories exist
    size_t lastSlash = path.find_last_of('/');
    if (lastSlash != std::string::npos) {
        std::string dir = path.substr(0, lastSlash);
        mkdir(dir.c_str(), 0755);
    }

    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        std::fprintf(stderr, "Failed to open %s for writing\n", path.c_str());
        return false;
    }

    constexpr int W = 160, H = 144;
    std::fprintf(f, "P6\n%d %d\n255\n", W, H);

    const uint32_t* fb = ppu.framebuffer();
    for (int i = 0; i < W * H; i++) {
        uint32_t argb = fb[i];
        uint8_t rgb[3] = {
            static_cast<uint8_t>((argb >> 16) & 0xFF),  // R
            static_cast<uint8_t>((argb >>  8) & 0xFF),  // G
            static_cast<uint8_t>((argb      ) & 0xFF),  // B
        };
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
    return true;
}

// ── Headless Blargg test runner ─────────────────────────────────────
// Polls external RAM at $A000 for Blargg's memory-mapped test output.
// Protocol: $A001-$A003 = signature DE B0 61 (valid when present)
//           $A000       = status (0x80 = running, 0 = pass, else fail code)
//           $A004+      = null-terminated text output
int runBlarggTest(const std::string& romPath, const std::string& dumpPath) {
    auto cartridge = Cartridge::loadFromFile(romPath);
    if (!cartridge) {
        std::fprintf(stderr, "Failed to load ROM: %s\n", romPath.c_str());
        return 2;
    }

    MemoryBus bus;
    bus.init();
    bus.loadCartridge(&(*cartridge));

    // Load bootrom (tries exe-relative, then CWD-relative)
    tryLoadBootrom(bus);

    CPU cpu(bus);

    // Run up to ~250 million cycles (≈60 seconds of Game Boy time)
    // dmg_sound multi-ROM takes a while
    constexpr uint64_t MAX_CYCLES = 250'000'000;
    constexpr uint64_t POLL_INTERVAL = 1'000'000; // Check every ~1M cycles
    uint64_t nextPoll = POLL_INTERVAL;

    // Track serial output for single ROMs
    std::string serialOutput;

    while (cpu.totalCycles() < MAX_CYCLES) {
        if (!cpu.tick()) {
            if (cpu.hitUnimplemented()) {
                std::fprintf(stderr, "UNIMPLEMENTED opcode — aborting test\n");
                return 2;
            }
            break;
        }

        // Capture serial output
        if (bus.hasSerialOutput()) {
            char c = bus.consumeSerial();
            serialOutput += c;
        }

        // Poll $A000 periodically
        if (cpu.totalCycles() >= nextPoll) {
            nextPoll = cpu.totalCycles() + POLL_INTERVAL;

            // Check signature at $A001-$A003
            uint8_t sig1 = bus.readCartridge(0xA001);
            uint8_t sig2 = bus.readCartridge(0xA002);
            uint8_t sig3 = bus.readCartridge(0xA003);

            if (sig1 == 0xDE && sig2 == 0xB0 && sig3 == 0x61) {
                uint8_t status = bus.readCartridge(0xA000);
                if (status != 0x80) {
                    // Test complete — read text output
                    std::string text;
                    for (int i = 0; i < 2048; i++) {
                        char c = static_cast<char>(bus.readCartridge(0xA004 + i));
                        if (c == '\0') break;
                        text += c;
                    }

                    // Print results
                    std::printf("=== Blargg Test: %s ===\n", romPath.c_str());
                    std::printf("%s", text.c_str());
                    if (!text.empty() && text.back() != '\n') std::printf("\n");
                    std::printf("Result code: %d (%s)\n",
                               status, status == 0 ? "PASSED" : "FAILED");

                    // Dump LCD if requested
                    if (!dumpPath.empty()) {
                        // Run a few more frames to ensure the LCD has the final output
                        for (uint64_t extra = 0; extra < 300'000; extra++) {
                            cpu.tick();
                        }
                        if (dumpFramebufferPPM(bus.ppu(), dumpPath)) {
                            std::printf("LCD dumped to: %s\n", dumpPath.c_str());
                        }
                    }

                    return (status == 0) ? 0 : 1;
                }
            }
        }
    }

    // Timeout — dump what we have
    std::printf("TIMEOUT: %s\n", romPath.c_str());
    if (!serialOutput.empty()) {
        std::printf("Serial output collected:\n%s\n", serialOutput.c_str());
    }
    if (!dumpPath.empty()) {
        if (dumpFramebufferPPM(bus.ppu(), dumpPath)) {
            std::printf("LCD dumped to: %s\n", dumpPath.c_str());
        }
    }
    return 2;
}

// ── QOL: Global state shared with audio callback thread ──────────────
static APU* g_apuPtr = nullptr;
static float g_volume = 1.0f;
static bool  g_muted  = false;
static SDL_Window* g_windowPtr = nullptr;  // for focus check in audio cb

// ── QOL: OSD bitmap font (8×8, 38 glyphs: 0-9, A-Z, :, %, space) ────
static const uint8_t FONT[39][8] = {
    {0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00}, // 0
    {0x10,0x30,0x10,0x10,0x10,0x10,0x38,0x00}, // 1
    {0x3C,0x42,0x02,0x0C,0x30,0x40,0x7E,0x00}, // 2
    {0x3C,0x42,0x02,0x1C,0x02,0x42,0x3C,0x00}, // 3
    {0x0C,0x14,0x24,0x44,0x7E,0x04,0x04,0x00}, // 4
    {0x7E,0x40,0x7C,0x02,0x02,0x42,0x3C,0x00}, // 5
    {0x3C,0x40,0x7C,0x42,0x42,0x42,0x3C,0x00}, // 6
    {0x7E,0x02,0x04,0x08,0x10,0x10,0x10,0x00}, // 7
    {0x3C,0x42,0x42,0x3C,0x42,0x42,0x3C,0x00}, // 8
    {0x3C,0x42,0x42,0x3E,0x02,0x02,0x3C,0x00}, // 9
    {0x3C,0x42,0x42,0x7E,0x42,0x42,0x42,0x00}, // A
    {0x7C,0x42,0x42,0x7C,0x42,0x42,0x7C,0x00}, // B
    {0x3C,0x42,0x40,0x40,0x40,0x42,0x3C,0x00}, // C
    {0x7C,0x42,0x42,0x42,0x42,0x42,0x7C,0x00}, // D
    {0x7E,0x40,0x40,0x7C,0x40,0x40,0x7E,0x00}, // E
    {0x7E,0x40,0x40,0x7C,0x40,0x40,0x40,0x00}, // F
    {0x3C,0x42,0x40,0x4E,0x42,0x42,0x3C,0x00}, // G
    {0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x00}, // H
    {0x38,0x10,0x10,0x10,0x10,0x10,0x38,0x00}, // I
    {0x0E,0x04,0x04,0x04,0x44,0x44,0x38,0x00}, // J
    {0x42,0x44,0x48,0x70,0x48,0x44,0x42,0x00}, // K
    {0x40,0x40,0x40,0x40,0x40,0x40,0x7E,0x00}, // L
    {0x42,0x66,0x5A,0x42,0x42,0x42,0x42,0x00}, // M
    {0x42,0x62,0x52,0x4A,0x46,0x42,0x42,0x00}, // N
    {0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00}, // O
    {0x7C,0x42,0x42,0x7C,0x40,0x40,0x40,0x00}, // P
    {0x3C,0x42,0x42,0x42,0x4A,0x44,0x3A,0x00}, // Q
    {0x7C,0x42,0x42,0x7C,0x48,0x44,0x42,0x00}, // R
    {0x3C,0x40,0x40,0x3C,0x02,0x02,0x3C,0x00}, // S
    {0x7C,0x10,0x10,0x10,0x10,0x10,0x10,0x00}, // T
    {0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x00}, // U
    {0x42,0x42,0x42,0x42,0x42,0x24,0x18,0x00}, // V
    {0x42,0x42,0x42,0x42,0x5A,0x66,0x42,0x00}, // W
    {0x42,0x24,0x18,0x18,0x24,0x42,0x42,0x00}, // X
    {0x44,0x44,0x28,0x10,0x10,0x10,0x10,0x00}, // Y
    {0x7E,0x04,0x08,0x10,0x20,0x40,0x7E,0x00}, // Z
    {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00}, // :
    {0x62,0x64,0x08,0x10,0x26,0x46,0x00,0x00}, // %
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // space
};

static int fontIndex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return 10 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 10 + (c - 'a');
    if (c == ':') return 36;
    if (c == '%') return 37;
    if (c == ' ') return 38;
    return -1;
}

static void drawOsdChar(SDL_Renderer* r, int x, int y, char c, uint8_t cr, uint8_t cg, uint8_t cb) {
    int idx = fontIndex(c);
    if (idx < 0) return;
    // Black outline
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    for (int row = 0; row < 8; row++) {
        uint8_t line = FONT[idx][row];
        for (int col = 0; col < 8; col++) {
            if (line & (1 << (7 - col))) {
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++)
                        if (dx || dy) {
                            SDL_Rect rc = {x+col+dx, y+row+dy, 1, 1};
                            SDL_RenderFillRect(r, &rc);
                        }
            }
        }
    }
    // Foreground
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    for (int row = 0; row < 8; row++) {
        uint8_t line = FONT[idx][row];
        for (int col = 0; col < 8; col++) {
            if (line & (1 << (7 - col))) {
                SDL_Rect rc = {x+col, y+row, 1, 1};
                SDL_RenderFillRect(r, &rc);
            }
        }
    }
}

static void drawOsdString(SDL_Renderer* r, int x, int y, const std::string& s,
                          uint8_t cr = 255, uint8_t cg = 255, uint8_t cb = 255) {
    for (char c : s) { drawOsdChar(r, x, y, c, cr, cg, cb); x += 8; }
}

// ── OSD notification queue ───────────────────────────────────────────
struct Notification { std::string text; int framesLeft; };
static std::deque<Notification> g_notifications;

static void showNotification(const std::string& text) {
    g_notifications.push_back({text, 120}); // ~2 seconds at 60fps
    while (g_notifications.size() > 5) g_notifications.pop_front();
}

// ── SDL Audio callback ───────────────────────────────────────────────
// Runs on a separate SDL audio thread. Reads from the APU's lock-free
// ring buffer. Applies volume/mute. Auto-mutes when window loses focus.
static void audioCallback(void* /*userdata*/, Uint8* stream, int len) {
    auto* out = reinterpret_cast<float*>(stream);
    int totalFloats = len / static_cast<int>(sizeof(float));
    int totalSamples = totalFloats / 2;  // stereo

    // Check if we should mute (explicit mute or window unfocused)
    bool shouldMute = g_muted;
    if (!shouldMute && g_windowPtr) {
        Uint32 flags = SDL_GetWindowFlags(g_windowPtr);
        if (!(flags & SDL_WINDOW_INPUT_FOCUS)) shouldMute = true;
    }

    if (shouldMute || !g_apuPtr) {
        std::memset(stream, 0, static_cast<size_t>(len));
        return;
    }

    // Read from ring buffer
    APU::StereoSample buf[2048];
    int toRead = totalSamples < 2048 ? totalSamples : 2048;
    int got = g_apuPtr->readSamples(buf, toRead);

    float vol = g_volume;
    for (int i = 0; i < got; i++) {
        out[i * 2]     = buf[i].left  * vol;
        out[i * 2 + 1] = buf[i].right * vol;
    }
    // Fill remainder with silence
    for (int i = got * 2; i < totalFloats; i++) {
        out[i] = 0.0f;
    }
}

// ── Normal emulation mode ────────────────────────────────────────────
int runEmulator(const std::string& romPath, Settings& settings) {
    auto cartridge = Cartridge::loadFromFile(romPath);
    if (!cartridge) {
        std::fprintf(stderr, "Failed to load ROM. Exiting.\n");
        return EXIT_FAILURE;
    }

    cartridge->printHeader();

    // ── Load battery save (SRAM persistence) ─────────────────────────
    cartridge->loadBatterySave();

    // ── Emulation core ───────────────────────────────────────────────
    MemoryBus bus;
    bus.init();
    bus.loadCartridge(&(*cartridge));

    // Load bootrom (tries exe-relative, then CWD-relative)
    tryLoadBootrom(bus);

    CPU cpu(bus);

    std::printf("\n── CPU initialised (PC=0x%04X, SP=0x%04X) ──\n\n",
               cpu.reg.pc, cpu.reg.sp);

    // ── SDL initialisation ───────────────────────────────────────────
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    // Build window title from ROM title
    std::string windowTitle = "GB Emu 2k26";
    if (!cartridge->title.empty()) {
        windowTitle += " - " + cartridge->title;
    }

    // ── QOL: Restore window state from settings ──────────────────────
    int winX = settings.getInt("WindowX", SDL_WINDOWPOS_CENTERED);
    int winY = settings.getInt("WindowY", SDL_WINDOWPOS_CENTERED);
    int winW = settings.getInt("WindowWidth", WIN_WIDTH);
    int winH = settings.getInt("WindowHeight", WIN_HEIGHT);

    SDL_Window* window = SDL_CreateWindow(
        windowTitle.c_str(),
        winX, winY, winW, winH,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }
    g_windowPtr = window;

    if (settings.getInt("Maximized", 0)) {
        SDL_MaximizeWindow(window);
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED
    );
    if (!renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    // Maintain GB aspect ratio regardless of window size
    SDL_RenderSetLogicalSize(renderer, GB_WIDTH, GB_HEIGHT);

    // Create a texture at native GB resolution — this is what the emulator
    // will draw into. The renderer will upscale it to the window size.
    SDL_Texture* framebuffer = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        GB_WIDTH, GB_HEIGHT
    );
    if (!framebuffer) {
        std::fprintf(stderr, "SDL_CreateTexture Error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    // Use nearest-neighbor scaling for that crisp pixel look
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    // ── SDL Audio setup ─────────────────────────────────────────────
    g_apuPtr = &bus.apu();
    SDL_AudioSpec audioSpec;
    SDL_zero(audioSpec);
    audioSpec.freq = APU::SAMPLE_RATE;
    audioSpec.format = AUDIO_F32SYS;
    audioSpec.channels = 2;
    audioSpec.samples = 1024;
    audioSpec.callback = audioCallback;
    audioSpec.userdata = nullptr;

    SDL_AudioDeviceID audioDevice = SDL_OpenAudioDevice(
        nullptr, 0, &audioSpec, nullptr, 0);
    if (audioDevice == 0) {
        std::fprintf(stderr, "SDL audio open failed: %s (continuing without sound)\n",
                     SDL_GetError());
    } else {
        SDL_PauseAudioDevice(audioDevice, 0);  // Start playback
    }

    // ── Frame pacing ─────────────────────────────────────────────────
    // Game Boy: 4,194,304 Hz CPU / 70,224 T-cycles per frame ≈ 59.7275 Hz
    constexpr double GB_FPS = 4194304.0 / 70224.0;  // ~59.7275 Hz
    const uint64_t perfFreq = SDL_GetPerformanceFrequency();
    const double targetFrameTime = static_cast<double>(perfFreq) / GB_FPS;
    uint64_t frameStart = SDL_GetPerformanceCounter();
    double frameTimeAccum = 0.0;  // sub-tick error accumulator

    // FPS counter
    uint64_t fpsLastTime = frameStart;
    int fpsFrameCount = 0;
    int fpsDisplay = 0;

    // ── QOL: Load persisted state ────────────────────────────────────
    g_volume = settings.getFloat("Volume", 1.0f);
    g_muted  = settings.getInt("Muted", 0) != 0;
    bool showFpsOsd = settings.getInt("ShowFPS", 0) != 0;

    bool running = true;
    SDL_Event event;

    while (running) {
        // --- Event handling ---
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            running = false;
                            break;
                        case SDLK_F5: {
                            // ── Save state (slot 1) ─────────────────
                            SaveState ss;
                            cpu.serialize(ss);
                            bus.serialize(ss);
                            cartridge->serialize(ss);
                            std::string path = cartridge->saveStatePath(1);
                            if (ss.saveToFile(path)) {
                                std::printf("State saved: %s (%zu bytes)\n",
                                            path.c_str(), ss.size());
                            } else {
                                std::fprintf(stderr, "Failed to save state!\n");
                            }
                            break;
                        }
                        case SDLK_F9: {
                            // ── Load state (slot 1) ─────────────────
                            std::string path = cartridge->saveStatePath(1);
                            SaveState ss;
                            if (ss.loadFromFile(path)) {
                                cpu.deserialize(ss);
                                bus.deserialize(ss);
                                cartridge->deserialize(ss);
                                if (ss.hasError()) {
                                    std::fprintf(stderr, "State load error: data truncated\n");
                                } else {
                                    std::printf("State loaded: %s\n", path.c_str());
                                }
                            } else {
                                std::fprintf(stderr, "No save state found: %s\n",
                                             path.c_str());
                            }
                            break;
                        }
                        // ── QOL key bindings ─────────────────
                        case SDLK_EQUALS:
                        case SDLK_PLUS: {
                            g_volume = std::min(1.0f, g_volume + 0.1f);
                            int pct = static_cast<int>(g_volume * 100 + 0.5f);
                            char buf[32]; std::snprintf(buf, sizeof(buf), "VOL:%d%%", pct);
                            showNotification(buf);
                            break;
                        }
                        case SDLK_MINUS: {
                            g_volume = std::max(0.0f, g_volume - 0.1f);
                            int pct = static_cast<int>(g_volume * 100 + 0.5f);
                            char buf[32]; std::snprintf(buf, sizeof(buf), "VOL:%d%%", pct);
                            showNotification(buf);
                            break;
                        }
                        case SDLK_m:
                            g_muted = !g_muted;
                            showNotification(g_muted ? "MUTED" : "UNMUTED");
                            break;
                        case SDLK_F3:
                            showFpsOsd = !showFpsOsd;
                            break;
                        case SDLK_F12: {
                            // ── Screenshot ────────────────────
                            const uint32_t* fb = bus.ppu().framebuffer();
                            mkdir("screenshots", 0755);
                            std::time_t t = std::time(nullptr);
                            char fname[64];
                            std::strftime(fname, sizeof(fname),
                                          "screenshots/%Y%m%d_%H%M%S.bmp",
                                          std::localtime(&t));
                            SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(
                                0, GB_WIDTH, GB_HEIGHT, 32, SDL_PIXELFORMAT_ARGB8888);
                            if (surf) {
                                std::memcpy(surf->pixels, fb,
                                            GB_WIDTH * GB_HEIGHT * sizeof(uint32_t));
                                SDL_SaveBMP(surf, fname);
                                SDL_FreeSurface(surf);
                                std::printf("Screenshot saved: %s\n", fname);
                                showNotification("SCREENSHOT SAVED");
                            }
                            break;
                        }
                        default:
                            break;
                    }
                    break;
            }
        }

        // --- Joypad input (polled per frame) ---
        const uint8_t* keys = SDL_GetKeyboardState(nullptr);
        auto& joy = bus.joypad();
        joy.setButton(Joypad::Button::Right,  keys[SDL_SCANCODE_RIGHT]);
        joy.setButton(Joypad::Button::Left,   keys[SDL_SCANCODE_LEFT]);
        joy.setButton(Joypad::Button::Up,     keys[SDL_SCANCODE_UP]);
        joy.setButton(Joypad::Button::Down,   keys[SDL_SCANCODE_DOWN]);
        joy.setButton(Joypad::Button::A,      keys[SDL_SCANCODE_Z]);
        joy.setButton(Joypad::Button::B,      keys[SDL_SCANCODE_X]);
        joy.setButton(Joypad::Button::Select, keys[SDL_SCANCODE_BACKSPACE]);
        joy.setButton(Joypad::Button::Start,  keys[SDL_SCANCODE_RETURN]);

        // --- Update / Emulation tick ---
        // Run one full frame worth of T-cycles (70224 = 154 scanlines × 456 dots)
        // Frame presentation happens inside the loop at VBlank to prevent
        // the PPU from overwriting early scanlines with the next frame's
        // scroll position before the framebuffer is copied to SDL.
        constexpr int TCYCLES_PER_FRAME = 70224;
        for (int t = 0; t < TCYCLES_PER_FRAME; ) {
            uint64_t before = cpu.totalCycles();
            if (!cpu.tick()) {
                if (cpu.hitUnimplemented()) {
                    std::fprintf(stderr, "CPU halted: unimplemented opcode. Stopping.\n");
                    running = false;
                }
                break;
            }
            t += (int)(cpu.totalCycles() - before);

            // --- Render ---
            // Present frame immediately at VBlank — before next-frame
            // scanlines overwrite the framebuffer (prevents scroll tearing)
            if (bus.ppu().frameReady()) {
                bus.ppu().clearFrameReady();

                uint32_t* pixels = nullptr;
                int pitch = 0;
                SDL_LockTexture(framebuffer, nullptr, reinterpret_cast<void**>(&pixels), &pitch);

                const uint32_t* fb = bus.ppu().framebuffer();
                for (int y = 0; y < GB_HEIGHT; y++) {
                    std::memcpy(
                        reinterpret_cast<uint8_t*>(pixels) + y * pitch,
                        fb + y * GB_WIDTH,
                        GB_WIDTH * sizeof(uint32_t)
                    );
                }

                SDL_UnlockTexture(framebuffer);

                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, framebuffer, nullptr, nullptr);

                // ── QOL: On-screen display (rendered at logical 160×144) ─
                if (showFpsOsd) {
                    char fpsBuf[16];
                    std::snprintf(fpsBuf, sizeof(fpsBuf), "FPS:%d", fpsDisplay);
                    drawOsdString(renderer, 2, GB_HEIGHT - 10, fpsBuf, 255, 255, 0);
                }
                // Draw notifications (stacked from top)
                int notifyY = 2;
                for (auto it = g_notifications.begin(); it != g_notifications.end(); ) {
                    drawOsdString(renderer, 2, notifyY, it->text);
                    notifyY += 10;
                    it->framesLeft--;
                    if (it->framesLeft <= 0) it = g_notifications.erase(it);
                    else ++it;
                }

                SDL_RenderPresent(renderer);

                // --- Dirty-flag battery save (flush after idle) ---
                cartridge->tickBatterySave();

                // --- FPS counter (every ~1 second) ---
                fpsFrameCount++;
                uint64_t now = SDL_GetPerformanceCounter();
                double elapsed = static_cast<double>(now - fpsLastTime) / perfFreq;
                if (elapsed >= 1.0) {
                    double fps = fpsFrameCount / elapsed;
                    fpsDisplay = static_cast<int>(fps + 0.5);
                    char titleBuf[128];
                    std::snprintf(titleBuf, sizeof(titleBuf), "%s  [%.2f FPS]",
                                  windowTitle.c_str(), fps);
                    SDL_SetWindowTitle(window, titleBuf);
                    fpsFrameCount = 0;
                    fpsLastTime = now;
                }
            }
        }

        // ── Frame pacing: sleep until the next frame boundary ────────
        {
            frameTimeAccum += targetFrameTime;
            uint64_t targetTicks = static_cast<uint64_t>(frameTimeAccum);
            frameTimeAccum -= targetTicks;  // keep fractional remainder

            uint64_t frameEnd = SDL_GetPerformanceCounter();
            int64_t remaining = static_cast<int64_t>(frameStart + targetTicks) - static_cast<int64_t>(frameEnd);

            if (remaining > 0) {
                // Coarse sleep (leave ~1.5ms for spin-wait)
                double remainingMs = (static_cast<double>(remaining) / perfFreq) * 1000.0;
                if (remainingMs > 2.0) {
                    SDL_Delay(static_cast<uint32_t>(remainingMs - 1.5));
                }
                // Spin-wait for precise timing
                while (SDL_GetPerformanceCounter() < frameStart + targetTicks) {
                    // busy-wait
                }
            }

            frameStart += targetTicks;
        }
    }

    // --- Battery save on exit (only if dirty) ---
    if (cartridge->isSramDirty()) {
        cartridge->writeBatterySave();
    }

    // ── QOL: Save window state + settings on exit ────────────────────
    {
        Uint32 flags = SDL_GetWindowFlags(window);
        bool maximized = flags & SDL_WINDOW_MAXIMIZED;
        settings.setInt("Maximized", maximized ? 1 : 0);
        if (!maximized) {
            int wx, wy, ww, wh;
            SDL_GetWindowPosition(window, &wx, &wy);
            SDL_GetWindowSize(window, &ww, &wh);

            // Compensate for window manager decorations (title bar).
            // SDL_GetWindowPosition returns the client area origin on
            // some Linux WMs, but SDL_CreateWindow positions the outer
            // frame — subtract the top border so Y doesn't drift down.
            int borderTop = 0, borderLeft = 0, borderBottom = 0, borderRight = 0;
            if (SDL_GetWindowBordersSize(window, &borderTop, &borderLeft,
                                         &borderBottom, &borderRight) == 0) {
                wy -= borderTop;
                wx -= borderLeft;
            }

            settings.setInt("WindowX", wx);
            settings.setInt("WindowY", wy);
            settings.setInt("WindowWidth", ww);
            settings.setInt("WindowHeight", wh);
        }
        settings.setFloat("Volume", g_volume);
        settings.setInt("Muted", g_muted ? 1 : 0);
        settings.setInt("ShowFPS", showFpsOsd ? 1 : 0);
        settings.save();
    }

    // ─── Cleanup ───
    g_apuPtr = nullptr;
    g_windowPtr = nullptr;
    if (audioDevice != 0) {
        SDL_PauseAudioDevice(audioDevice, 1);
        SDL_CloseAudioDevice(audioDevice);
    }
    SDL_DestroyTexture(framebuffer);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
    // ── Parse arguments ──────────────────────────────────────────────
    bool testMode = false;
    bool blarggMode = false;
    std::string romPath;
    std::string dumpLcdPath;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--test") == 0) {
            testMode = true;
        } else if (std::strcmp(argv[i], "--blargg") == 0) {
            blarggMode = true;
        } else if (std::strcmp(argv[i], "--dump-lcd") == 0 && i + 1 < argc) {
            dumpLcdPath = argv[++i];
        } else {
            romPath = argv[i];
        }
    }

    if (testMode) {
        if (romPath.empty()) {
            std::fprintf(stderr, "Usage: gbemu --test <rom_path>\n");
            return 2;
        }
        return runTest(romPath);
    }

    if (blarggMode) {
        if (romPath.empty()) {
            std::fprintf(stderr, "Usage: gbemu --blargg [--dump-lcd <path.ppm>] <rom_path>\n");
            return 2;
        }
        return runBlarggTest(romPath, dumpLcdPath);
    }

    // ── Normal mode ──────────────────────────────────────────────────
    Settings settings;
    settings.load();  // silently ignored if file doesn't exist yet

    if (romPath.empty()) {
        std::printf("No ROM path provided. Opening file dialog...\n");

        // Determine where the file picker should start:
        //   1. Last directory we opened a ROM from (persisted in settings)
        //   2. Fall back to the executable's directory (build folder)
        std::string initialDir = settings.get("last_rom_dir");
        if (initialDir.empty()) {
            initialDir = getExeDir();
            // Strip trailing slash for cleanliness (zenity handles either way)
            if (!initialDir.empty() && initialDir.back() == '/') {
                initialDir.pop_back();
            }
        }

        romPath = openFileDialog(initialDir);
        if (romPath.empty()) {
            std::printf("No file selected. Exiting.\n");
            return EXIT_SUCCESS;
        }

        // Persist the directory of the selected ROM for next time
        size_t lastSlash = romPath.find_last_of('/');
        if (lastSlash != std::string::npos) {
            settings.set("last_rom_dir", romPath.substr(0, lastSlash));
            settings.save();
        }
    }

    std::printf("Loading ROM: %s\n", romPath.c_str());
    return runEmulator(romPath, settings);
}
