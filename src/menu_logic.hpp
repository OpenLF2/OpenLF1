// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include "display.hpp"
#include "menu_state.hpp"
#include "field.hpp"
#include "random.hpp"
#include <chrono>
#include <cstdint>
#include <functional>

enum class MenuSound { none, change, confirm, toggle };

// Applies one key press on the mode screen and tells which sound belongs to it.
MenuSound handle_mode_selection(MenuState& menu, Action input);

MenuSound handle_tournament_count(MenuState& menu, Action input);
MenuSound handle_tournament_selection(MenuState& menu, Action input, const std::function<int(int)>& draw);
void handle_tournament_winner(MenuState& menu, Action input);
MenuSound handle_character_selection(MenuState& menu, Action input, int who);

inline std::uint16_t current_seed() {
    return static_cast<std::uint16_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

// Moves the bracket on once its waiting time is over: computers settle their match, humans pick controls.
void handle_tournament_bracket(MenuState& menu, const std::function<int(int)>& draw);

// Assigns the pressed controller to the next human of the match and starts the battle after the last one.
MenuSound handle_tournament_controls(MenuState& menu, Action input, int pressed_by, Field& field, GameRandom& random, int floor);

struct MainMenuOutcome { MenuSound sound = MenuSound::none; bool quit = false; };

// Applies one key press on the title menu, or starts a demonstration after a long idle time.
MainMenuOutcome handle_main_menu(MenuState& menu, Action input, Field& field, const std::function<int(int)>& draw, int top, int bottom);

// Creates the fighters once everybody who joined is ready and the short delay is over.
void start_when_ready(MenuState& menu, Field& field, const std::function<int(int)>& draw, int top, int bottom);

// Leaves the overview for the battle after the first key press (immediately in a demonstration).
void handle_match_overview(MenuState& menu, Action input, Field& field, GameRandom& random);

// Moves on after a battle: to the next bracket page of a tournament, or back to the title menu.
void finish_battle(MenuState& menu, Field& field, const std::function<int(int)>& draw);
