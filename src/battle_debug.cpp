// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "battle_debug.hpp"
#include <algorithm>

bool apply_battle_debug(BattleDebug& debug, Field& field, Action input, const std::function<int(int)>& draw) {
    auto& fighters = field.fighters;
    auto& controllers = field.controllers;
    auto& projectiles = field.projectiles;
    auto& ground_items = field.ground_items;
    auto& hit_effects = field.hit_effects;
    bool explosion = false;
    if (input == Action::unlock) debug.mode = DebugMode::battle_controls;
    if (input == Action::cheat_mode2) debug.mode = DebugMode::character_selection;
    if (debug.mode == DebugMode::battle_controls) {
        if (input == Action::cheat_previous_stage) field.stage = (field.stage + GameLimits::stages - 1) % GameLimits::stages;
        if (input == Action::cheat_next_stage) field.stage = (field.stage + 1) % GameLimits::stages;
        if (input == Action::cheat_timer) debug.timed = !debug.timed;
        if (input == Action::cheat_6) { debug.timed = false; debug.draw_scene = false; }
        if (input == Action::cheat_7 && !fighters.empty()) fighters[0].active = false;
        if (input == Action::cheat_8 && fighters.size() > 3) fighters[3].active = false;
        const int player = input == Action::cheat_heal1 ? 0 : input == Action::cheat_heal2 ? 1 : input == Action::cheat_heal3 ? 2 : -1;
        if (player >= 0) for (std::size_t i = 0; i < fighters.size(); ++i) if (controllers[i] == player) {
            fighters[i].health = fighters[i].max_health; fighters[i].energy = fighters[i].max_energy;
        }
        if (input == Action::cheat_clear_items) {
            std::vector<Projectile> effects;
            for (auto& item : ground_items) if (item.active && item.type != ItemType::clear_effect) {
                Projectile effect; effect.slot = item.slot; effect.owner = 0; effect.type = ItemType::clear_effect; effect.x = item.x; effect.y = item.y; effect.z = item.z; effect.exploding = true;
                effects.push_back(effect); item.active = false; item.owner = ItemOwnership::unowned;
            }
            for (const auto& projectile : projectiles) if (projectile.active && projectile.type != ItemType::clear_effect) {
                Projectile effect; effect.slot = projectile.slot; effect.owner = projectile.owner; effect.type = ItemType::clear_effect; effect.x = projectile.x; effect.y = projectile.y; effect.z = projectile.z; effect.exploding = true;
                effects.push_back(effect);
            }
            for (auto& fighter : fighters) fighter.carried = ItemSlot::none;
            projectiles = std::move(effects);
            hit_effects.clear();
            explosion = !projectiles.empty();
        }
        if (input == Action::cheat_spawn_items) {
            for (int type = 1; type <= 10; ++type) {
                auto item = GroundItem::create(draw);
                item.type = type == ItemType::head ? ItemType::drink : type;
                item.variant = type == ItemType::head ? 1 : 0;
                item.durability = item.type == ItemType::stone || item.type == ItemType::box || item.type == ItemType::claw ? 2 : 6;
                item.slot = field.free_slot(8);
                if (item.slot < 30) ground_items.push_back(item);
            }
        }
    }
    if (debug.mode == DebugMode::character_selection && input >= Action::cheat_1 && input <= Action::cheat_8) {
        const auto slot = std::size_t(int(input) - int(Action::cheat_1));
        if (slot < fighters.size()) {
            int candidate = fighters[slot].archetype;
            do { candidate = candidate % GameLimits::characters + 1; }
            while (std::any_of(fighters.begin(), fighters.end(), [&](const auto& other) { return &other != &fighters[slot] && other.active && other.archetype == candidate; }));
            fighters[slot].archetype = candidate;
        }
    }
    return explosion;
}
