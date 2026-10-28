// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include "filesystem.hpp"
#include <filesystem>
#include <memory>

std::unique_ptr<FileSystem> load_game_archive(const std::filesystem::path& path);
std::unique_ptr<FileSystem> open_game_archive();
