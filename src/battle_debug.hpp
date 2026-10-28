// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include "display.hpp"
#include "field.hpp"
#include "game_state.hpp"
#include <functional>

// State of the developer keys that can be unlocked during a battle.
struct BattleDebug {
    DebugMode mode = DebugMode::off;
    bool timed = true;
    bool draw_scene = true;
};

// Applies one developer key. Returns true when the explosion sound should be played.
bool apply_battle_debug(BattleDebug& debug, Field& field, Action input, const std::function<int(int)>& draw);
