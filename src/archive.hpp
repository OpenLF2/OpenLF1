// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include "filesystem.hpp"
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

class Archive final : public FileSystem {
public:
    explicit Archive(const std::filesystem::path& path);
    [[nodiscard]] std::vector<std::uint8_t> read(std::string_view path) const override;
private:
    std::vector<std::uint8_t> bytes_;
};
