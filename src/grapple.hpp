// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include "combat.hpp"
#include <span>

class Grapple {
public:
    static bool rush(std::span<Combatant> fighters, int holder, int victim, const ImpactRegion& impact);
    static bool clinch(std::span<Combatant> fighters, int holder, int victim, bool left_held);
    static bool strike(std::span<Combatant> fighters, int holder);
    static bool throw_clinched(std::span<Combatant> fighters, int holder);
    static void update_clinch(std::span<Combatant> fighters, int holder);
    static bool lift(std::span<Combatant> fighters, int holder, int victim);
    static void update(std::span<Combatant> fighters, int holder, const std::function<int(int)>& draw);
    static void release_abandoned(std::span<Combatant> fighters);
    static bool throw_held(std::span<Combatant> fighters, int holder);
};
