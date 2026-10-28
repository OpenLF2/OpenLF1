// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include <cstdint>
#include <string_view>
#include <vector>

class FileSystem {
public:
    virtual ~FileSystem() = default;
    [[nodiscard]] virtual std::vector<std::uint8_t> read(std::string_view path) const = 0;
};
