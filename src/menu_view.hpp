// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include "field.hpp"
#include "raster.hpp"
#include <vector>

// Shows who will fight whom before a battle starts.
void draw_match_overview(Screen& screen, const Raster& words, const Raster& faces, const std::vector<Combatant>& fighters,
                         const std::vector<int>& controllers, int mode, int players, int group);

// Shows the choices of the mode screen with the current selection highlighted.
void draw_mode_selection(Screen& screen, const Raster& modes, int mode, int players, int group, int contest);

// The title menu with the selected entry highlighted.
void draw_main_menu(Screen& screen, const Raster& title, const Raster& words, int choice, int difficulty);
