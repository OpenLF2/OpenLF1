// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "battle_view.hpp"
#include <algorithm>
#include <string>

namespace {
struct HeadOffsets {
    std::array<int, FighterAction::normal_pose_count + 1> x{}, y{};
    HeadOffsets() {
        auto& head_x = x; auto& head_y = y;
        for (int i = 0; i <= FighterAction::normal_pose_count; ++i) {
            if (i == 25) head_x[i] = 5;
            if (i == 7 || i == 11 || i == 24 || i == 38 || i == 39) head_x[i] = 4;
            if (i == 44 || i == 45 || i == 46 || i == 50 || i == 51 || i > 57) head_x[i] = 3;
            if (i == 21) head_y[i] = 3;
            if ((i >= 12 && i <= 20) || i == 22 || i == 23 || (i >= 26 && i <= 31) ||
                (i >= 33 && i <= 43) || (i >= 52 && i <= 56) || i == 47 || i == 48) head_y[i] = 50;
            if (i == 50 || i == 51 || i == 38) head_y[i] = 60;
            if (i == 39) head_y[i] = 61;
        }
    }
};
}

void draw_battle(Screen& screen, std::array<std::uint32_t, PaletteCycleTuning::entries>& palette, Field& field, const BattleArt& art, bool draw_scene) {
    static const HeadOffsets offsets;
    const auto& head_x = offsets.x;
    const auto& head_y = offsets.y;
    auto& fighters = field.fighters;
    auto& projectiles = field.projectiles;
    auto& ground_items = field.ground_items;
    auto& blasts = field.blasts;
    auto& hit_effects = field.hit_effects;
    const int stage = field.stage;
    const int camera = field.camera;
    const unsigned tick = field.tick;
    const auto& stage_left = art.stage_left;
    const auto& stage_right = art.stage_right;
    const auto& figures = art.figures;
    const auto& extra_figures = art.extra_figures;
    const auto& item_images = art.item_images;
    const auto& battle_heads = art.battle_heads;
    const auto& status = art.status;
    const auto& fonts = art.fonts;
    const auto& appearance = art.data.appearance;
    const auto& hair = art.data.hair;
    const auto& boundaries = art.data.boundaries;
    const auto& item_types = art.data.item_types;
    const auto& raw = art.palette_file;
    const auto text_at = [&](std::string_view text, int x, int y, int tint = 0) {
        for (const unsigned char c : text) {
            screen.copy(fonts, (c % 16) * 6, (c / 16) * 8, 5, 7, x, y, true, tint);
            x += 5;
        }
    };
    screen.copy(stage_left[stage], 0, 0, 320, 100, camera - 160, 56);
    screen.copy(stage_right[stage], 0, 0, 320, 100, camera + 160, 56);
    screen.copy(stage_left[stage], 0, 99, 320, 45, camera - 160, 155);
    screen.copy(stage_right[stage], 0, 99, 320, 45, camera + 160, 155);
    enum class VisibleKind { fighter, item, projectile, blast, hit_effect };
    struct Visible { int y; VisibleKind kind; std::size_t index; };
    std::vector<Visible> depth;
    for (std::size_t i = 0; i < fighters.size(); ++i) if (fighters[i].active) depth.push_back({fighters[i].y, VisibleKind::fighter, i});
    for (std::size_t i = 0; i < ground_items.size(); ++i) if (ground_items[i].active) depth.push_back({ground_items[i].y, VisibleKind::item, i});
    for (std::size_t i = 0; i < projectiles.size(); ++i) if (projectiles[i].active) depth.push_back({projectiles[i].y, VisibleKind::projectile, i});
    for (std::size_t i = 0; i < blasts.size(); ++i) if (blasts[i].active) depth.push_back({blasts[i].y, VisibleKind::blast, i});
    for (std::size_t i = 0; i < hit_effects.size(); ++i) depth.push_back({hit_effects[i].y, VisibleKind::hit_effect, i});
    std::stable_sort(depth.begin(), depth.end(), [](const auto& a, const auto& b) { return a.y < b.y; });
    if (draw_scene) for (const auto& visible : depth) {
        if (visible.kind == VisibleKind::item) {
            const auto& item = ground_items[visible.index];
            if (item.frame != 4) {
                const int image = item_types[item.type].images[item.frame - 1] - 1 + (item.type == ItemType::drink && item.variant == 1 ? 59 : 0);
                if (image >= 0) {
                    if (item.x > boundaries[stage + 1].left + 10 && item.x < boundaries[stage + 1].right - 10)
                        screen.copy(figures, 1, 127, 25, 3, item.x - 12, item.y - 1, true);
                    screen.copy(item_images, (image % 15) * 18 + 1, (image / 15) * 14 + 1, 17, 13, item.x - 8, item.y + item.z - 12, true, 0, item.facing < 0);
                }
            }
            continue;
        }
        if (visible.kind == VisibleKind::projectile) {
            const auto& p = projectiles[visible.index];
            const int image = item_types[p.type].images[p.frame - 1] - 1;
            if (image >= 0) {
                if (p.x > boundaries[stage + 1].left + 10 && p.x < boundaries[stage + 1].right - 10)
                    screen.copy(figures, 1, 127, 25, 3, p.x - 12, p.y - 1, true);
                screen.copy(item_images, (image % 15) * 18 + 1, (image / 15) * 14 + 1, 17, 13, p.x - 8, p.y + p.z - 12, true, 0, p.facing < 0);
            }
            continue;
        }
        if (visible.kind == VisibleKind::blast) {
            const auto& blast = blasts[visible.index];
            const int frame = blast.frame - 1 - blast.offset;
            if (frame >= 0 && frame < 3) screen.copy(item_images, frame * 36 + 127, 81, 35, 29, blast.x - 17, blast.y + blast.z - 27, true);
            continue;
        }
        if (visible.kind == VisibleKind::hit_effect) {
            const auto& effect = hit_effects[visible.index];
            const int image = item_types[ItemType::hit_effect].images[effect.frame - 1] - 1;
            if (image >= 0) {
                if (effect.x > boundaries[stage + 1].left + 10 && effect.x < boundaries[stage + 1].right - 10)
                    screen.copy(figures, 1, 127, 25, 3, effect.x - 12, effect.y - 1, true);
                screen.copy(item_images, (image % 15) * 18 + 1, (image / 15) * 14 + 1, 17, 13,
                            effect.x - 8, effect.y + effect.z - 12, true, 0, effect.facing < 0);
            }
            continue;
        }
        const auto i = visible.index;
        auto& f = fighters[i];
        if (!f.active) continue;
        if (f.death_pose > 0) {
            const int pose = f.death_pose - 1;
            screen.copy(figures, (pose % 12) * 26 + 1, (pose / 12) * 26 + 1,
                25, 25, f.x - 12, f.y - 24, true, 0, f.facing < 0);
            f.death_pose = 0;
            continue;
        }
        const auto& look = appearance[f.archetype - 1];
        for (int c = 0; c < 10; ++c) {
            const int index = c == 9 ? hair[look.head - 1] : look.color[c];
            palette[i * 10 + 151 + c] = rgb(raw[(index - 1) * 3] & PaletteCycleTuning::levels, raw[(index - 1) * 3 + 1] & PaletteCycleTuning::levels, raw[(index - 1) * 3 + 2] & PaletteCycleTuning::levels);
        }
        if (f.state / 10 != FighterStateGroup::angel)
            screen.copy(figures, 1, 127, 25, 3, f.x - 12 - f.facing * 2, f.y - 1, true);
        else if (f.z > -50 && (!(tick & 1) || f.z > -25))
            screen.copy(figures, 1, 79, 25, 25, f.x - 12, f.y - 24, true, 0, f.facing < 0);
        if (f.invisible_ticks > 10 || f.invisible_ticks % 2 != 0) continue;
        if (f.state / 10 == FighterStateGroup::angel && f.archetype == FighterType::regenerating_fighter) continue;
        const int marker_tint = f.energy >= look.specials[2].cost && (tick & 1) ? 0 : f.team_color - 15;
        text_at(f.marker ? std::to_string(f.marker) : "c", f.x - (f.marker == 10 ? 7 : 2) - f.facing * 2,
                f.y + f.z - 30, marker_tint);
        if (f.effect_ticks > 0 && f.effect_ticks % 2 != 0) continue;
        const bool extra = f.action >= FighterAction::angel_alt;
        const int pose = std::clamp(f.action - (extra ? FighterAction::normal_pose_count : 0), 1, FighterAction::normal_pose_count) - 1;
        const int head = look.head - 1;
        const auto head_at = [&](int y) {
            screen.copy(battle_heads, (head % 11) * 18 + 1, (head / 11) * 13 + 1, 17, 12,
                f.x - f.facing * 4 + head_x[f.action] * f.facing - 8, y, true, 0, f.facing < 0);
        };
        if (!extra && head_y[f.action] >= 60) head_at(f.y + head_y[f.action] + f.z - 84);
        const bool fixed_color = (pose >= 35 && pose <= 41 && pose != 37 && pose != 38 && pose != 39) || (pose >= 51 && pose <= 55);
        const int tint = !extra && f.action < FighterAction::death_pose && !fixed_color ? int(i) * 10 : 0;
        screen.copy(extra ? extra_figures : figures, (pose % 12) * 26 + 1, (pose / 12) * 26 + 1,
            25, 25, f.x - 12, f.y + f.z - 24, true, tint, f.facing < 0);
        if (!extra && f.health < 45 && f.archetype != FighterType::regenerating_fighter &&
            ((head_x[f.action] == 0 && head_y[f.action] < 40) || f.action == FighterAction::rest || f.action == FighterAction::rest_alt)) {
            const int off_x = f.action == FighterAction::rest || f.action == FighterAction::rest_alt ? -5 : 0;
            const int off_y = f.action == FighterAction::rest || f.action == FighterAction::rest_alt ? FighterAction::rest - f.action : 0;
            screen.fill(f.x - f.facing * (off_x + 4), f.y - off_y + f.z - 12, 1, 2, 34);
        }
        if (!extra && head_y[f.action] < 50) head_at(f.y + head_y[f.action] + f.z - 24);
    }
    screen.copy(status, 0, 0, 320, 56, 0, 0);
    for (std::size_t i = 0; i < fighters.size(); ++i) {
        const auto& f = fighters[i];
        if (!f.active && !f.eliminated) continue;
        const auto& look = appearance[f.archetype - 1];
        const int x = int(i % 4) * 80, y = int(i / 4) * 27;
        if (f.eliminated)
            screen.copy(figures, f.archetype == FighterType::regenerating_fighter ? 209 : 261, 105, 25, 25, x + 3, y + 3, true);
        else
            screen.copy(figures, 1, 1, 25, 25, x + 6, y + 3, true, int(i) * 10);
        const int head = look.head - 1;
        screen.copy(battle_heads, (head % 11) * 18 + 1, (head / 11) * 13 + 1, 17, 12, x + 6, y + 3, true);
        if (f.eliminated) continue;
        screen.fill(x + 26, y + 10, std::clamp(f.health * 50 / f.max_health, 0, 50), 5, 31);
        const bool flash = tick & 1;
        const auto& moves = look.specials;
        const int energy_color = f.energy >= moves[2].cost && flash ? 15 :
            ((f.energy >= moves[1].cost && flash) || f.energy >= moves[2].cost) ? 106 :
            ((f.energy >= moves[0].cost && flash) || f.energy >= moves[1].cost) ? 104 : 1;
        screen.fill(x + 26, y + 20, std::clamp(f.energy * 50 / f.max_energy, 0, 50), 5, energy_color);
    }
}
