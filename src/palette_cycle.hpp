// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace PaletteCycleTuning {
inline constexpr int entries = 256;
inline constexpr int levels = 63;
inline constexpr int output_levels = 255;
inline constexpr int raw_bytes = 324;
inline constexpr int flash_index = 250;
inline constexpr int flash_source = raw_bytes - 3;
inline constexpr int transition_step = 2;
inline constexpr int transition_limit = 40;
inline constexpr int cycle_first = 1;
inline constexpr int cycle_last = 6;
inline constexpr int cycle_offset = 180;
inline constexpr int weight_base = 13;
inline constexpr int weight_scale = 12;
inline constexpr int position_min = 1;
inline constexpr int position_max = 5;
inline constexpr int initial_red = 60;
}

namespace PaletteCyclePhase {
inline constexpr int red_to_blue = 1;
inline constexpr int blue_to_green = 2;
inline constexpr int green_to_red = 3;
}

class PaletteCycle {
public:
    void step(std::span<const std::uint8_t> original, std::span<std::uint32_t, PaletteCycleTuning::entries> colors, bool flash);
private:
    int phase_ = PaletteCyclePhase::red_to_blue, red_ = PaletteCycleTuning::initial_red, green_ = 0, blue_ = 0, position_ = 0;
    bool blinking_ = false;
};
