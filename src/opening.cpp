// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "opening.hpp"
#include "game_state.hpp"
#include <chrono>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

bool play_opening(Display& display, Screen& screen, const Raster& opening, std::array<std::uint32_t, PaletteCycleTuning::entries>& palette,
                  const std::vector<std::uint8_t>& raw) {
    using Clock = std::chrono::steady_clock;
    screen.copy(opening, 0, 0, 320, 200, 0, 0);
    auto start = Clock::now();
    bool skip = false;
    for (;;) {
#ifdef __EMSCRIPTEN__
        emscripten_sleep(1);
#endif
        const auto action = display.poll();
        if (action == Action::close) return false;
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
        // DOS clock ticks at approximately 18.2 Hz; each original wait spans two ticks.
        if (action != Action::none && elapsed >= GameTiming::opening_skip_frames * GameTiming::tick_ms && !skip) { skip = true; start = Clock::now() - std::chrono::milliseconds(GameTiming::opening_skip_start_frames * GameTiming::tick_ms); }
        const int phase = int(elapsed / GameTiming::tick_ms);
        if (phase >= GameTiming::opening_frames || (skip && elapsed >= GameTiming::opening_frames * GameTiming::tick_ms)) break;
        const int level = phase < GameTiming::opening_fade_in_frames ? (phase + 1) * GameTiming::opening_level_step : phase < GameTiming::opening_fade_out_start ? GameTiming::opening_fade_in_frames * GameTiming::opening_level_step : (GameTiming::opening_frames - phase) * GameTiming::opening_level_step;
        palette[15] = rgb(unsigned(level), unsigned(level), unsigned(level));
        screen.present(display, palette);
    }
    for (std::size_t i = 1; i < PaletteCycleTuning::entries; ++i) palette[i] = rgb(raw[(i - 1) * 3] & PaletteCycleTuning::levels, raw[(i - 1) * 3 + 1] & PaletteCycleTuning::levels, raw[(i - 1) * 3 + 2] & PaletteCycleTuning::levels);
    return true;
}
