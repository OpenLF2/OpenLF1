// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "display.hpp"
#ifdef OPENLF1_HAS_WINDOW_ICON
#include "window_icon.hpp"
#endif
#include <SDL3/SDL.h>
#include <stdexcept>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <array>
#ifdef __DJGPP__
#include <sys/nearptr.h>
#endif
namespace {
constexpr int gamepad_dead_zone = 16000;
constexpr std::size_t max_audio_voices = 32;
template<class T, void (*Destroy)(T*)> using Handle = std::unique_ptr<T, decltype(Destroy)>;
void check(bool ok) { if (!ok) throw std::runtime_error(SDL_GetError()); }
}
struct Display::Impl {
    int source_player = 0;
    std::array<std::array<int, 2>, GameLimits::human_players> axes{};
    Handle<SDL_Window, SDL_DestroyWindow> window{nullptr, SDL_DestroyWindow};
    Handle<SDL_Renderer, SDL_DestroyRenderer> renderer{nullptr, SDL_DestroyRenderer};
    Handle<SDL_Texture, SDL_DestroyTexture> texture{nullptr, SDL_DestroyTexture};
    std::vector<Handle<SDL_AudioStream, SDL_DestroyAudioStream>> voices;
    bool audio_ready = false;
    using Pad = Handle<SDL_Gamepad, SDL_CloseGamepad>;
    std::array<Pad, GameLimits::human_players> pads{{Pad{nullptr, SDL_CloseGamepad}, Pad{nullptr, SDL_CloseGamepad}, Pad{nullptr, SDL_CloseGamepad}}};
    int find(SDL_JoystickID id) const {
        for (int i = 0; i < GameLimits::human_players; ++i) if (pads[i] && SDL_GetGamepadID(pads[i].get()) == id) return i;
        return -1;
    }
    void connect(SDL_JoystickID id) {
        if (find(id) >= 0) return;
        for (auto& pad : pads) if (!pad) { pad.reset(SDL_OpenGamepad(id)); return; }
    }
    void disconnect(SDL_JoystickID id) {
        const int i = find(id);
        if (i >= 0) { pads[i].reset(); axes[i] = {}; }
    }
    void connect_available() {
        int count = 0;
        std::unique_ptr<SDL_JoystickID[], decltype(&SDL_free)> devices(SDL_GetGamepads(&count), SDL_free);
        for (int i = 0; devices && i < count; ++i) connect(devices[i]);
    }
    void toggle_fullscreen() {
        const SDL_WindowFlags flags = SDL_GetWindowFlags(window.get());
        SDL_SetWindowFullscreen(window.get(), (flags & SDL_WINDOW_FULLSCREEN) == 0);
    }
    Impl() {
#ifdef __DJGPP__
        if (!__djgpp_nearptr_enable()) throw std::runtime_error("Cannot enable DOS framebuffer access");
#endif
        check(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD));
        audio_ready = SDL_InitSubSystem(SDL_INIT_AUDIO);
#ifdef __DJGPP__
        window.reset(SDL_CreateWindow("OpenLF1", 320, 200, 0));
#else
        window.reset(SDL_CreateWindow("OpenLF1", 960, 600, SDL_WINDOW_RESIZABLE));
#endif
        check(bool(window));
#ifdef OPENLF1_HAS_WINDOW_ICON
        namespace icon = openlf1::icon;
        Handle<SDL_Surface, SDL_DestroySurface> surface(SDL_CreateSurface(icon::window_size, icon::window_size,
            SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface);
        if (surface) {
            auto* pixels = static_cast<unsigned char*>(surface->pixels);
            for (int row = 0; row < icon::window_size; ++row)
                SDL_memcpy(pixels + row * surface->pitch, icon::window_rgba.data() + row * icon::window_size * 4,
                    icon::window_size * 4);
            SDL_SetWindowIcon(window.get(), surface.get());
        }
#endif
        renderer.reset(SDL_CreateRenderer(window.get(), nullptr));
        check(bool(renderer));
        check(SDL_SetRenderLogicalPresentation(renderer.get(), 320, 200, SDL_LOGICAL_PRESENTATION_LETTERBOX));
        texture.reset(SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 320, 200));
        check(bool(texture));
        check(SDL_SetTextureScaleMode(texture.get(), SDL_SCALEMODE_NEAREST));
        connect_available();
    }
    ~Impl() { voices.clear(); for (auto& pad : pads) pad.reset(); texture.reset(); renderer.reset(); window.reset(); SDL_Quit(); }
};
Display::Display() : impl_(std::make_unique<Impl>()) {}
Display::~Display() = default;
std::filesystem::path Display::executable_directory() {
    const char* path = SDL_GetBasePath();
    return path ? std::filesystem::path(reinterpret_cast<const char8_t*>(path)) : std::filesystem::path{};
}
void Display::show(std::span<const std::uint32_t> pixels) {
    if (pixels.size() != 320 * 200) throw std::runtime_error("Invalid frame size");
    check(SDL_UpdateTexture(impl_->texture.get(), nullptr, pixels.data(), 320 * 4));
    check(SDL_SetRenderDrawColor(impl_->renderer.get(), 0, 0, 0, 255));
    check(SDL_RenderClear(impl_->renderer.get()));
    check(SDL_RenderTexture(impl_->renderer.get(), impl_->texture.get(), nullptr, nullptr));
    check(SDL_RenderPresent(impl_->renderer.get()));
}
Action Display::poll() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) return Action::close;
        if (event.type == SDL_EVENT_GAMEPAD_ADDED) impl_->connect(event.gdevice.which);
        if (event.type == SDL_EVENT_GAMEPAD_REMOVED) { impl_->disconnect(event.gdevice.which); impl_->connect_available(); }
        if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION &&
            (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX || event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY)) {
            const int player = impl_->find(event.gaxis.which);
            if (player < 0) continue;
            int& previous = impl_->axes[player][event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX ? 0 : 1];
            const int next = event.gaxis.value < -gamepad_dead_zone ? -1 : event.gaxis.value > gamepad_dead_zone ? 1 : 0;
            if (next != previous) {
                previous = next;
                impl_->source_player = player;
                if (next && event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) return next < 0 ? Action::left : Action::right;
                if (next) return next < 0 ? Action::up : Action::down;
            }
        }
        if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            const int player = impl_->find(event.gbutton.which);
            if (player < 0) continue;
            impl_->source_player = player;
            if (event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_LEFT) return Action::left;
            if (event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) return Action::right;
            if (event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_UP) return Action::up;
            if (event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_DOWN) return Action::down;
            if (event.gbutton.button == SDL_GAMEPAD_BUTTON_EAST) return Action::jump;
            if (event.gbutton.button == SDL_GAMEPAD_BUTTON_START) return Action::pause;
            if (event.gbutton.button == SDL_GAMEPAD_BUTTON_BACK) return Action::end_round;
            if (event.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH) return Action::accept;
            return Action::key;
        }
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
            if ((event.key.scancode == SDL_SCANCODE_RETURN || event.key.scancode == SDL_SCANCODE_KP_ENTER) &&
                (event.key.mod & SDL_KMOD_ALT)) {
                impl_->toggle_fullscreen();
                continue;
            }
            switch (event.key.scancode) {
            case SDL_SCANCODE_I: case SDL_SCANCODE_J: case SDL_SCANCODE_L: case SDL_SCANCODE_COMMA: case SDL_SCANCODE_K: case SDL_SCANCODE_SPACE: impl_->source_player = 1; break;
            case SDL_SCANCODE_KP_8: case SDL_SCANCODE_KP_4: case SDL_SCANCODE_KP_6: case SDL_SCANCODE_KP_2: case SDL_SCANCODE_KP_5: case SDL_SCANCODE_KP_0: impl_->source_player = 2; break;
            default: impl_->source_player = 0; break;
            }
            const bool shifted = (event.key.mod & SDL_KMOD_SHIFT) != 0;
            if (shifted) {
                switch (event.key.scancode) {
                case SDL_SCANCODE_1: return Action::cheat_1;
                case SDL_SCANCODE_2: return Action::cheat_2;
                case SDL_SCANCODE_3: return Action::cheat_3;
                case SDL_SCANCODE_4: return Action::cheat_4;
                case SDL_SCANCODE_5: return Action::cheat_5;
                case SDL_SCANCODE_6: return Action::cheat_6;
                case SDL_SCANCODE_7: return Action::cheat_7;
                case SDL_SCANCODE_8: return Action::cheat_8;
                case SDL_SCANCODE_9: return Action::cheat_previous_stage;
                case SDL_SCANCODE_0: return Action::cheat_next_stage;
                case SDL_SCANCODE_MINUS: return Action::cheat_spawn_items;
                case SDL_SCANCODE_BACKSLASH: return Action::cheat_heal3;
                default: break;
                }
            }
            switch (event.key.scancode) {
            case SDL_SCANCODE_ESCAPE: return Action::pause;
            case SDL_SCANCODE_F10: return Action::end_round;
            case SDL_SCANCODE_F1: return Action::unlock;
            case SDL_SCANCODE_F2: return Action::cheat_mode2;
            case SDL_SCANCODE_EQUALS: return Action::cheat_clear_items;
            case SDL_SCANCODE_LEFTBRACKET: return Action::cheat_heal1;
            case SDL_SCANCODE_RIGHTBRACKET: return Action::cheat_heal2;
            case SDL_SCANCODE_T: return Action::cheat_timer;
            case SDL_SCANCODE_A: case SDL_SCANCODE_J: case SDL_SCANCODE_KP_4: return Action::left;
            case SDL_SCANCODE_D: case SDL_SCANCODE_L: case SDL_SCANCODE_KP_6: return Action::right;
            case SDL_SCANCODE_W: case SDL_SCANCODE_I: case SDL_SCANCODE_KP_8: return Action::up;
            case SDL_SCANCODE_X: case SDL_SCANCODE_COMMA: case SDL_SCANCODE_KP_2: return Action::down;
            case SDL_SCANCODE_TAB: case SDL_SCANCODE_SPACE: case SDL_SCANCODE_KP_0: return Action::jump;
            case SDL_SCANCODE_RETURN: case SDL_SCANCODE_S: case SDL_SCANCODE_K: case SDL_SCANCODE_KP_5: return Action::accept;
            default: return Action::key;
            }
        }
    }
    SDL_Delay(5);
    return Action::none;
}

int Display::player() const { return impl_->source_player; }
bool Display::held(int player, Action direction) const {
    constexpr SDL_Scancode keys[GameLimits::human_players][4] = {
        {SDL_SCANCODE_A, SDL_SCANCODE_D, SDL_SCANCODE_W, SDL_SCANCODE_X},
        {SDL_SCANCODE_J, SDL_SCANCODE_L, SDL_SCANCODE_I, SDL_SCANCODE_COMMA},
        {SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_6, SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_2}
    };
    int index = direction == Action::left ? 0 : direction == Action::right ? 1 : direction == Action::up ? 2 : direction == Action::down ? 3 : -1;
    if (player < 0 || player >= GameLimits::human_players || index < 0) return false;
    const bool* keyboard = SDL_GetKeyboardState(nullptr);
    bool result = keyboard[keys[player][index]];
    const auto& pad = impl_->pads[player];
    if (pad) {
        constexpr SDL_GamepadButton buttons[] = {SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN};
        result = result || SDL_GetGamepadButton(pad.get(), buttons[index]);
        const int axis = SDL_GetGamepadAxis(pad.get(), index < 2 ? SDL_GAMEPAD_AXIS_LEFTX : SDL_GAMEPAD_AXIS_LEFTY);
        result = result || (index % 2 == 0 ? axis < -gamepad_dead_zone : axis > gamepad_dead_zone);
    }
    return result;
}


void Display::sound(std::span<const std::uint8_t> wave, float gain) {
    if (!impl_->audio_ready) return;
    SDL_AudioSpec format{};
    Uint8* samples = nullptr;
    Uint32 length = 0;
    std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)> input(SDL_IOFromConstMem(wave.data(), wave.size()), SDL_CloseIO);
    if (!input) return;
    const bool loaded = SDL_LoadWAV_IO(input.get(), false, &format, &samples, &length);
    std::unique_ptr<Uint8, decltype(&SDL_free)> data(samples, SDL_free);
    if (!loaded) return;
    std::erase_if(impl_->voices, [](const auto& voice) { return SDL_GetAudioStreamQueued(voice.get()) <= 0; });
    if (impl_->voices.size() >= max_audio_voices) impl_->voices.erase(impl_->voices.begin());
    Handle<SDL_AudioStream, SDL_DestroyAudioStream> voice{SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &format, nullptr, nullptr), SDL_DestroyAudioStream};
    if (!voice) return;
    SDL_SetAudioStreamGain(voice.get(), std::clamp(gain, 0.0f, 1.0f));
    if (!SDL_PutAudioStreamData(voice.get(), data.get(), int(length))) return;
    if (!SDL_ResumeAudioStreamDevice(voice.get())) return;
    impl_->voices.push_back(std::move(voice));
}
