#include "gb_cpu.h"
#include "gb_memory.h"
#include "gb_ppu.h"
#include "gb_timer.h"
#include "gb_input.h"
#include "gb_apu.h"
#include "gb_cartridge.h"
#include "bios.h"

#include <SDL.h>
#include <cstdio>
#include <cstring>
#include <string>
#ifndef __EMSCRIPTEN__
#include <filesystem>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static CPU cpu;
static Memory mem;
static PPU ppu;
static Timer timer;
static Input input;
static APU apu;
static Cartridge cart;

static SDL_Window* window = nullptr;
static SDL_Renderer* renderer = nullptr;
static SDL_Texture* texture = nullptr;
static SDL_AudioDeviceID audio_dev = 0;
static bool running = true;

static const int TARGET_FPS = 60;
static const int FRAME_MS = 1000 / TARGET_FPS;
static u32 frame_start_ms = 0;

static std::string save_path;
static std::string save_key;
static int save_timer = 0;

static const int SAVE_INTERVAL = 300;

static void persist_save_js() {
#ifdef __EMSCRIPTEN__
    if (save_key.empty()) return;
    EM_ASM({
        try {
            var path = UTF8ToString($0);
            var key = UTF8ToString($1);
            var data = FS.readFile(path);
            var bin = '';
            for (var i = 0; i < data.length; i++) bin += String.fromCharCode(data[i]);
            localStorage.setItem(key, btoa(bin));
        } catch(e) {}
    }, save_path.c_str(), save_key.c_str());
#endif
}

static bool save_ram_to_file() {
    if (cart.ram_size() == 0) return false;
    cart.save_ram(save_path);
    persist_save_js();
    return true;
}

static bool load_ram_from_file() {
    if (cart.ram_size() == 0) return false;
    return cart.load_ram(save_path);
}

static Input::Button sdl_to_gb(SDL_Keycode key) {
    switch (key) {
        case SDLK_RIGHT:  return Input::RIGHT;
        case SDLK_LEFT:   return Input::LEFT;
        case SDLK_UP:     return Input::UP;
        case SDLK_DOWN:   return Input::DOWN;
        case SDLK_z:      return Input::A;
        case SDLK_x:      return Input::B;
        case SDLK_RETURN: return Input::START;
        case SDLK_RSHIFT: return Input::SELECT;
        case SDLK_LSHIFT: return Input::SELECT;
        default:          return (Input::Button)-1;
    }
}

static void handle_event(const SDL_Event& e) {
    if (e.type == SDL_QUIT) {
        running = false;
        return;
    }
    if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
        int w = e.window.data1;
        int h = e.window.data2;
        int fit_w = h * SCREEN_W / SCREEN_H;
        int fit_h = w * SCREEN_H / SCREEN_W;
        if (fit_w <= w) {
            int vw = fit_w;
            int vh = h;
            int vx = (w - vw) / 2;
            int vy = 0;
            SDL_Rect viewport = { vx, vy, vw, vh };
            SDL_RenderSetViewport(renderer, &viewport);
        } else {
            int vw = w;
            int vh = fit_h;
            int vx = 0;
            int vy = (h - vh) / 2;
            SDL_Rect viewport = { vx, vy, vw, vh };
            SDL_RenderSetViewport(renderer, &viewport);
        }
        return;
    }
    if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
        Input::Button b = sdl_to_gb(e.key.keysym.sym);
        if (b != (Input::Button)-1) {
            if (e.type == SDL_KEYDOWN) {
                input.key_down(b);
                mem.io_regs[0x0F] |= INT_JOYPAD;
            } else {
                input.key_up(b);
            }
        }
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
            running = false;
        }
    }
}

static void run_frame() {
    u64 target_cycles = 70224;
    u64 total = 0;

    while (total < target_cycles) {
        u8 cycles = cpu.tick(mem, ppu, timer, input, apu);
        ppu.step(cycles, mem, (u8*)&mem.io_regs[0x0F]);
        timer.step(cycles, mem);
        apu.step(cycles, mem);
        total += cycles;
    }

    if (texture) {
        SDL_UpdateTexture(texture, nullptr, ppu.pixels.data(), SCREEN_W * sizeof(u32));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

    if (audio_dev && apu.buffer_pos > 0) {
        SDL_QueueAudio(audio_dev, apu.audio_buffer.data(), apu.buffer_pos * sizeof(s16));
        apu.buffer_pos = 0;
    }

    save_timer++;
    if (cart.dirty && save_timer >= SAVE_INTERVAL) {
        save_timer = 0;
        cart.dirty = false;
        save_ram_to_file();
    }

    u32 now = SDL_GetTicks();
    u32 elapsed = now - frame_start_ms;
    if (elapsed < FRAME_MS) {
        SDL_Delay(FRAME_MS - elapsed);
    }
    frame_start_ms = SDL_GetTicks();
}

#ifdef __EMSCRIPTEN__
static void emscripten_loop() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) handle_event(e);
    run_frame();
}
#endif

static bool init_sdl() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    int win_flags = SDL_WINDOW_SHOWN;
#ifndef __EMSCRIPTEN__
    win_flags |= SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
#endif
    window = SDL_CreateWindow("gb-emu by TS ",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W * 3, SCREEN_H * 3,
        win_flags);

    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    int render_flags = SDL_RENDERER_ACCELERATED;
#ifndef __EMSCRIPTEN__
    render_flags |= SDL_RENDERER_PRESENTVSYNC;
#endif
    renderer = SDL_CreateRenderer(window, -1, render_flags);

    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }

    texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_W, SCREEN_H);

    if (!texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 2048;
    want.callback = nullptr;

    audio_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (audio_dev == 0) {
        fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
    } else {
        SDL_PauseAudioDevice(audio_dev, 0);
    }

    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "0");
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: gbemu <rom.gb>\n");
        return 1;
    }

    if (!cart.load(argv[1])) {
        fprintf(stderr, "Failed to load ROM: %s\n", argv[1]);
        return 1;
    }

    mem.load_boot_rom(dmg_boot_data, dmg_boot_size);

#ifdef __EMSCRIPTEN__
    save_path = argc > 2 ? argv[2] : "save.sav";
    save_key = argc > 3 ? argv[3] : "gb_save_data";
    if (cart.ram_size() > 0) load_ram_from_file();
#else
    std::string rom_path(argv[1]);
    save_path = rom_path + ".sav";
    if (std::filesystem::exists(save_path)) {
        load_ram_from_file();
    }
#endif

    if (!init_sdl()) {
        return 1;
    }

    cpu.reset();
    mem.reset();
    ppu.reset();
    timer.reset();
    apu.reset();
    mem.set_cartridge(&cart);
    mem.set_input(&input);
    mem.set_apu(&apu);

    fprintf(stdout, "Loaded: %s | Type: %02X | ROM Banks: %d | RAM Banks: %d\n",
        argv[1], cart.mbc_type, cart.num_rom_banks, cart.num_ram_banks);

    frame_start_ms = SDL_GetTicks();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(emscripten_loop, 0, 1);
#else
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) handle_event(e);
        run_frame();
    }

    save_ram_to_file();

    if (audio_dev) SDL_CloseAudioDevice(audio_dev);
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
#endif

    return 0;
}
