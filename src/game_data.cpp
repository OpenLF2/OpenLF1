// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "game_data.hpp"
#include <sstream>
#include <stdexcept>
#include <string>

GameData parse_game_data(const std::vector<std::uint8_t>& data) {
    GameData result;
    auto& appearance = result.appearance; auto& hair = result.hair; auto& boundaries = result.boundaries; auto& ranges = result.ranges; auto& item_types = result.item_types;
    std::istringstream source(std::string(data.begin(), data.end()));
    for (auto& item : appearance) {
        std::string label, name, form;
        int ignored;
        source >> label >> name >> label >> item.head >> label >> ignored >> ignored;
        for (int j = 0; j < 3; ++j) source >> label >> item.color[j * 2] >> item.color[j * 2 + 1];
        item.color[6] = 15; item.color[7] = 31; item.color[8] = 10;
        source >> label;
        for (auto& move : item.specials) {
            source >> form;
            if (form.size() != 1) throw std::runtime_error("Invalid special move");
            move.form = form[0];
            for (auto& frame : move.frames) source >> frame;
            source >> move.cost;
        }
        if (!source || item.head < 1 || item.head > GameLimits::heads) throw std::runtime_error("Invalid character data");
    }
    std::string end_marker;
    source >> end_marker;
    for (auto& color : hair) source >> color;
    if (!source || end_marker != "E") throw std::runtime_error("Invalid character data");
    std::string header;
    source >> header >> header;
    for (auto& row : boundaries)
        source >> header >> row.left >> row.right >> row.top >> row.bottom >> row.edge_left >> row.edge_right;
    if (!source) throw std::runtime_error("Invalid stage data");
    for (int i = 1;; ++i) {
        source >> header;
        if (!source) throw std::runtime_error("Invalid special data");
        if (header == "E") break;
        if (i >= int(ranges.size())) throw std::runtime_error("Too many special moves");
        source >> ranges[i].center >> ranges[i].width >> ranges[i].depth >> ranges[i].strength;
    }
    source >> header;
    for (;;) {
        int index;
        source >> header;
        if (!source) throw std::runtime_error("Invalid item data");
        if (header == "E") break;
        source >> index;
        if (index < 0 || index >= int(item_types.size())) throw std::runtime_error("Invalid item index");
        auto& item = item_types[index];
        source >> item.category >> item.motion >> item.effect >> item.damage >> item.knockdown;
        for (int& image : item.images) source >> image;
        source >> item.bounce;
    }
    return result;
}
