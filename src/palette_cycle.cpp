// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "palette_cycle.hpp"
#include <stdexcept>
namespace {
std::uint32_t convert(int r, int g, int b) {
    return 0xff000000u | (unsigned(r * PaletteCycleTuning::output_levels / PaletteCycleTuning::levels) << 16) | (unsigned(g * PaletteCycleTuning::output_levels / PaletteCycleTuning::levels) << 8) | unsigned(b * PaletteCycleTuning::output_levels / PaletteCycleTuning::levels);
}
}
void PaletteCycle::step(std::span<const std::uint8_t> original, std::span<std::uint32_t, PaletteCycleTuning::entries> colors, bool flash) {
    if (original.size() < PaletteCycleTuning::raw_bytes) throw std::invalid_argument("Palette is too short");
    if (flash) {
        blinking_ = !blinking_;
        colors[PaletteCycleTuning::flash_index] = blinking_ ? convert(original[PaletteCycleTuning::flash_source] & PaletteCycleTuning::levels, original[PaletteCycleTuning::flash_source + 1] & PaletteCycleTuning::levels, original[PaletteCycleTuning::flash_source + 2] & PaletteCycleTuning::levels) : convert(PaletteCycleTuning::levels, PaletteCycleTuning::levels, PaletteCycleTuning::levels);
    }
    const int phase = phase_;
    if (phase == PaletteCyclePhase::red_to_blue) { red_ -= PaletteCycleTuning::transition_step; blue_ += PaletteCycleTuning::transition_step; if (blue_ == PaletteCycleTuning::transition_limit) phase_ = PaletteCyclePhase::blue_to_green; }
    if (phase == PaletteCyclePhase::blue_to_green) { blue_ -= PaletteCycleTuning::transition_step; green_ += PaletteCycleTuning::transition_step; if (green_ == PaletteCycleTuning::transition_limit) phase_ = PaletteCyclePhase::green_to_red; }
    if (phase == PaletteCyclePhase::green_to_red) { green_ -= PaletteCycleTuning::transition_step; red_ += PaletteCycleTuning::transition_step; if (red_ == PaletteCycleTuning::transition_limit) phase_ = PaletteCyclePhase::red_to_blue; }
    if (--position_ < PaletteCycleTuning::position_min) position_ = PaletteCycleTuning::position_max;
    int index = position_;
    for (int i = PaletteCycleTuning::cycle_first; i <= PaletteCycleTuning::cycle_last; ++i) {
        if (++index > PaletteCycleTuning::cycle_last) index = PaletteCycleTuning::cycle_first;
        const int weight = PaletteCycleTuning::weight_base - i;
        colors[index + PaletteCycleTuning::cycle_offset] = convert(weight * red_ / PaletteCycleTuning::weight_scale, weight * green_ / PaletteCycleTuning::weight_scale, weight * blue_ / PaletteCycleTuning::weight_scale);
    }
}
