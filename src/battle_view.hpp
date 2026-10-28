// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include "field.hpp"
#include "game_data.hpp"
#include "raster.hpp"
#include <array>
#include <cstdint>
#include <vector>

// The pictures and tables needed to draw a battle.
struct BattleArt {
    const std::vector<Raster>& stage_left;
    const std::vector<Raster>& stage_right;
    const Raster& figures;
    const Raster& extra_figures;
    const Raster& item_images;
    const Raster& battle_heads;
    const Raster& status;
    const Raster& fonts;
    const GameData& data;
    const std::vector<std::uint8_t>& palette_file;
};

// Draws the stage, everything on it and the status panel. Fighters take their colors from the character tables.
void draw_battle(Screen& screen, std::array<std::uint32_t, PaletteCycleTuning::entries>& palette, Field& field, const BattleArt& art, bool draw_scene);
