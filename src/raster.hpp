// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include "display.hpp"
#include "palette_cycle.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

class Raster {
public:
    explicit Raster(const std::vector<std::uint8_t>& bytes) {
        if (bytes.size() < 300) throw std::runtime_error("Invalid image header");
        width_ = bytes[0] * 256 + bytes[1];
        height_ = bytes[2] * 256 + bytes[3];
        if (!width_ || !height_ || std::size_t(width_) * height_ != bytes.size() - 300)
            throw std::runtime_error("Invalid image dimensions");
        pixels_.assign(bytes.begin() + 300, bytes.end());
    }
    std::uint8_t at(int x, int y) const {
        if (x < 0 || y < 0 || x >= width_ || y >= height_) throw std::runtime_error("Image region out of bounds");
        return pixels_[std::size_t(y) * width_ + x];
    }
private:
    int width_{}, height_{};
    std::vector<std::uint8_t> pixels_;
};
class Screen {
public:
    void copy(const Raster& image, int sx, int sy, int width, int height, int dx, int dy, bool transparent = false, int offset = 0, bool flip = false) {
        for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
            auto c = image.at(sx + (flip ? width - 1 - x : x), sy + y);
            if ((!transparent || c) && dx + x >= 0 && dx + x < 320 && dy + y >= 0 && dy + y < 200)
                pixels_[std::size_t(dy + y) * 320 + dx + x] = std::uint8_t(c + offset);
        }
    }
    void fill(int x, int y, int width, int height, std::uint8_t color) {
        for (int row = 0; row < height; ++row) for (int col = 0; col < width; ++col)
            if (x + col >= 0 && x + col < 320 && y + row >= 0 && y + row < 200)
                pixels_[std::size_t(y + row) * 320 + x + col] = color;
    }
    void recolor(int x1, int y1, int x2, int y2, int old, int replacement) {
        for (int y = y1; y <= y2; ++y) for (int x = x1; x <= x2; ++x) {
            auto& pixel = pixels_[std::size_t(y) * 320 + x];
            if (pixel == old) pixel = std::uint8_t(replacement);
        }
    }
    void present(Display& display, const std::array<std::uint32_t, PaletteCycleTuning::entries>& palette) const {
        std::array<std::uint32_t, 320 * 200> frame{};
        for (std::size_t i = 0; i < frame.size(); ++i) frame[i] = palette[pixels_[i]];
        display.show(frame);
    }
private:
    std::array<std::uint8_t, 320 * 200> pixels_{};
};
inline std::uint32_t rgb(unsigned r, unsigned g, unsigned b) {
    return 0xff000000u | ((r * PaletteCycleTuning::output_levels / PaletteCycleTuning::levels) << 16) | ((g * PaletteCycleTuning::output_levels / PaletteCycleTuning::levels) << 8) | (b * PaletteCycleTuning::output_levels / PaletteCycleTuning::levels);
}
