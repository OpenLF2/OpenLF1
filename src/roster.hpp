// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include "combat.hpp"
#include "game_state.hpp"
#include <array>
#include <functional>

struct Participant {
    bool joined = false;
    int character = 1;
    int team = 1;
};
struct MatchSetup {
    std::array<Combatant, GameLimits::fighter_slots> fighters;
    std::array<int, GameLimits::fighter_slots> controls;
};
class Roster {
public:
    using Draw = std::function<int(int)>;
    static MatchSetup create(int mode, int option, std::array<Participant, GameLimits::human_players> people, int upper, int lower, const Draw& draw, bool rare_character = false);
};
