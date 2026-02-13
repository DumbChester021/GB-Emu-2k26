#include <SDL2/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "cartridge.h"
#include "cpu.h"
#include "file_dialog.h"
#include "memory_bus.h"
#include "save_state.h"
#include "apu.h"

// Game Boy DMG native resolution
constexpr int GB_WIDTH  = 160;
constexpr int GB_HEIGHT = 144;
constexpr int SCALE     = 4;

// Window dimensions
constexpr int WIN_WIDTH  = GB_WIDTH  * SCALE;  // 640
constexpr int WIN_HEIGHT = GB_HEIGHT * SCALE;  // 576

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

    // Try to load bootrom
    bus.loadBootrom("bootroms/dmg_boot.bin");

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

// ── SDL Audio callback ───────────────────────────────────────────────
// Runs on a separate SDL audio thread. Reads from the APU's lock-free
// ring buffer. Fills silence if the emulator hasn't produced enough.
static APU* g_apuPtr = nullptr;

static void audioCallback(void* /*userdata*/, Uint8* stream, int len) {
    auto* out = reinterpret_cast<float*>(stream);
    int totalFloats = len / static_cast<int>(sizeof(float));
    int totalSamples = totalFloats / 2;  // stereo

    if (g_apuPtr) {
        // Read directly from ring buffer
        APU::StereoSample buf[2048];
        int toRead = totalSamples < 2048 ? totalSamples : 2048;
        int got = g_apuPtr->readSamples(buf, toRead);

        for (int i = 0; i < got; i++) {
            out[i * 2]     = buf[i].left;
            out[i * 2 + 1] = buf[i].right;
        }
        // Fill remainder with silence
        for (int i = got * 2; i < totalFloats; i++) {
            out[i] = 0.0f;
        }
    } else {
        std::memset(stream, 0, static_cast<size_t>(len));
    }
}

// ── Normal emulation mode ────────────────────────────────────────────
int runEmulator(const std::string& romPath) {
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

    // Try to load bootrom
    bus.loadBootrom("bootroms/dmg_boot.bin");

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
        windowTitle += " — " + cartridge->title;
    }

    SDL_Window* window = SDL_CreateWindow(
        windowTitle.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_WIDTH, WIN_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

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
        }

        // --- Render ---
        // Only present when PPU has completed a frame (VBlank)
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
            SDL_RenderPresent(renderer);

            // --- Dirty-flag battery save (flush after idle) ---
            cartridge->tickBatterySave();
        }
    }

    // --- Battery save on exit (only if dirty) ---
    if (cartridge->isSramDirty()) {
        cartridge->writeBatterySave();
    }

    // ─── Cleanup ───
    g_apuPtr = nullptr;
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
    std::string romPath;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--test") == 0) {
            testMode = true;
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

    // ── Normal mode ──────────────────────────────────────────────────
    if (romPath.empty()) {
        std::printf("No ROM path provided. Opening file dialog...\n");
        romPath = openFileDialog();
        if (romPath.empty()) {
            std::printf("No file selected. Exiting.\n");
            return EXIT_SUCCESS;
        }
    }

    std::printf("Loading ROM: %s\n", romPath.c_str());
    return runEmulator(romPath);
}
