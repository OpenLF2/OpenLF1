// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include "game_state.hpp"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
enum class Action { none, close, pause, up, down, left, right, accept, jump, end_round, unlock, key,
    cheat_mode2, cheat_1, cheat_2, cheat_3, cheat_4, cheat_5, cheat_6, cheat_7, cheat_8,
    cheat_previous_stage, cheat_next_stage, cheat_spawn_items, cheat_clear_items,
    cheat_heal1, cheat_heal2, cheat_heal3, cheat_timer };
class Display {
public:
    Display();
    ~Display();
    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;
    static std::filesystem::path executable_directory();
    void show(std::span<const std::uint32_t> pixels);
    Action poll();
    int player() const;
    bool held(int player, Action direction) const;
    void sound(std::span<const std::uint8_t> wave, float gain = 0.6f);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
