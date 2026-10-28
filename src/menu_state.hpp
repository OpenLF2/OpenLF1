// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include "game_state.hpp"
#include "tournament.hpp"
#include <array>
#include <chrono>
#include <memory>

// What the player has chosen on the menu screens so far.
struct MenuState {
    struct Slot { SelectionPhase phase = SelectionPhase::inactive; int fighter = 0; int team = 1; };

    using Clock = std::chrono::steady_clock;
    Clock::time_point next_tick = Clock::now(), menu_activity = Clock::now(), palette_tick = Clock::now();
    Clock::time_point overview_ready = Clock::now(), bracket_ready = Clock::now(), selection_ready{};

    int page = GameScreen::main_menu;
    int previous_page = GameScreen::main_menu;
    int choice = 0;
    int difficulty = 5;
    int mode = GameMode::free_for_all;
    int group = 1;
    int players = 2;
    int contest = TournamentFormat::pairs;
    bool demonstration = false;
    bool rare_character = false;
    bool paused = false;
    std::array<Slot, GameLimits::human_players> slots{};
    std::unique_ptr<Tournament> tournament;
    int entrants_wanted = 1, entrant_number = 0, candidate = 0, assigning = 0;
    std::array<int, 3> assigned_controls{-1, -1, -1};
    std::array<Entrant, TournamentLimits::pair_entrants> entrants{};
    std::array<Entrant, TournamentLimits::pair_entrants> bracket_entrants{};
};
