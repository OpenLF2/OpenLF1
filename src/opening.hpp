// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include "display.hpp"
#include "palette_cycle.hpp"
#include "raster.hpp"
#include <array>
#include <cstdint>
#include <vector>

// Fades the title picture in and out. Returns false when the window was closed meanwhile.
bool play_opening(Display& display, Screen& screen, const Raster& opening, std::array<std::uint32_t, PaletteCycleTuning::entries>& palette,
                  const std::vector<std::uint8_t>& palette_file);
