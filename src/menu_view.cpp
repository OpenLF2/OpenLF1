// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "menu_view.hpp"

void draw_match_overview(Screen& screen, const Raster& words, const Raster& faces, const std::vector<Combatant>& fighters,
                         const std::vector<int>& controllers, int mode, int players, int group) {
    const auto box = [&](int x1, int x2, int y1, int y2) {
        const int middle = (y2 + y1 + 70) / 2 - 70;
        screen.copy(words, 160, 130, 150, 70, x1, middle);
        screen.copy(words, 170, 130, 150, 70, x2 - 150, middle);
        screen.copy(words, 160, 120, 150, 70, x1, y1);
        screen.copy(words, 160, 130, 150, 70, x1, y2 - 70);
        screen.copy(words, 170, 120, 150, 70, x2 - 150, y1);
        screen.copy(words, 170, 130, 150, 70, x2 - 150, y2 - 70);
    };
    const auto label = [&](int sy, int x, int y) { screen.copy(words, 80, sy, 64, 14, x, y, true); };
    const auto face_at = [&](int index, int x, int y) {
        constexpr int faces_order[] = {1,2,3,8,10,9,4,7,5,11,6};
        const int face = faces_order[fighters[index].archetype - 1] - 1;
        screen.copy(faces, (face % 6) * 50, (face / 6) * 50, 50, 50, x, y);
    };
    if (mode == GameMode::free_for_all) {
        constexpr int first[] = {2,3,4,3,3,4,4};
        constexpr int second[] = {0,0,0,2,3,3,4};
        constexpr int upper[] = {75,75,75,36,36,36,36};
        constexpr int lower[] = {75,75,75,106,106,106,106};
        const int row = players - 2, spacing = (320 - first[row] * 50) / (first[row] + 1);
        box(spacing / 2, 320 - spacing / 2, upper[row] - 8, lower[row] + 71);
        label(84, 128, upper[row] - 30);
        const auto draw_row = [&](int base, int count, int y) {
            const int gap = (320 - count * 50) / (count + 1);
            for (int i = 0; i < count; ++i) {
                const int x = gap + (gap + 50) * i;
                face_at(base + i, x, y);
                if (controllers[base + i] >= 0) screen.copy(words, 0, 120 + controllers[base + i] * 14, 64, 14, x - 7, y + 51, true);
                else screen.copy(words, 0, 162, 24, 14, x + 12, y + 51, true);
            }
        };
        draw_row(0, first[row], upper[row]);
        if (second[row]) draw_row(first[row], second[row], lower[row]);
    } else {
        label(98, 128, 6);
        if (group <= 3) {
            box(50, 269, 28, 181);
            const int count = group == 1 ? 2 : group == 2 ? 3 : 4;
            const int stride = group == 1 ? 80 : group == 2 ? 67 : 50;
            const int left = group == 1 ? 95 : group == 2 ? 68 : 60;
            const int second_base = group == 1 ? 2 : 4;
            for (int i = 0; i < count; ++i) { face_at(i, left + stride * i, 51); face_at(i + second_base, left + stride * i, 121); }
            label(112, 128, 36); label(126, 128, 106);
        } else {
            box(40, 279, 28, 181);
            for (int i = 0; i < 2; ++i) {
                face_at(i, 50 + i * 50, 51); face_at(i + 2, 170 + i * 50, 51);
                face_at(i + 4, (group == 4 ? 110 : 50) + i * 50, 121);
                if (group == 5) face_at(i + 6, 170 + i * 50, 121);
            }
            label(112, 68, 36); label(126, 188, 36);
            label(140, group == 4 ? 128 : 68, 106);
            if (group == 5) label(154, 188, 106);
        }
    }
}

void draw_mode_selection(Screen& screen, const Raster& modes, int mode, int players, int group, int contest) {
    screen.copy(modes, 0, 0, 320, 200, 0, 0, true);
    auto dim = [&](int y1, int y2) {
        screen.recolor(36, y1, 103, y2, 98, 118);
        screen.recolor(105, y1, 280, y2, 34, 35);
        screen.recolor(36, y1, 280, y2, 15, 13);
    };
    if (mode != GameMode::free_for_all) dim(36, 67);
    if (mode != GameMode::teams) dim(69, 164);
    if (mode != GameMode::tournament) dim(166, 188);
    if (mode == GameMode::free_for_all) screen.recolor((players - 2) * 24 + 114, 54, (players - 2) * 24 + 128, 66, 34, 31);
    if (mode == GameMode::teams) screen.recolor(128, (group - 1) * 18 + 73, 257, (group - 1) * 18 + 90, 34, 31);
    if (mode == GameMode::tournament) screen.recolor((contest - 1) * 90 + 114, 168, (contest - 1) * 90 + 180, 185, 34, 31);
}

void draw_main_menu(Screen& screen, const Raster& title, const Raster& words, int choice, int difficulty) {
    screen.copy(title, 0, 0, 320, 200, 0, 0, true);
    screen.copy(words, 80, 56, 64, 14, 128, 112, true, choice == 0 ? 0 : 47);
    screen.copy(words, 80, difficulty == 0 ? 0 : 14, 88, 14, 116, 133, true, choice == 1 ? 0 : 47);
    screen.copy(words, 80, 42, 64, 14, 128, 154, true, choice == 2 ? 0 : 47);
}
