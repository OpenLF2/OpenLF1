// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "game_archive.hpp"
#include "archive.hpp"
#include "display.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <stdexcept>
#include <vector>

std::unique_ptr<FileSystem> load_game_archive(const std::filesystem::path& path) {
    auto archive = std::make_unique<Archive>(path);
    for (const auto* file : {"BEGIN.EXE", "CONTEST.EXE", "FIGHT.EXE", "START.EXE"})
        if (archive->read(file).empty()) throw std::runtime_error("Empty game executable");
    return archive;
}

std::unique_ptr<FileSystem> open_game_archive() {
    std::vector<std::filesystem::path> directories{std::filesystem::current_path()};
    const char* appimage = std::getenv("APPIMAGE");
    const auto binary_directory = appimage && *appimage ? std::filesystem::absolute(std::filesystem::path(reinterpret_cast<const char8_t*>(appimage))).parent_path() : Display::executable_directory();
    if (!binary_directory.empty() && binary_directory != directories.front()) directories.push_back(binary_directory);
#ifdef __APPLE__
    for (auto directory = binary_directory; !directory.empty() && directory != directory.root_path(); directory = directory.parent_path()) {
        if (directory.extension() == ".app") {
            const auto parent = directory.parent_path();
            if (!parent.empty() && std::find(directories.begin(), directories.end(), parent) == directories.end()) directories.push_back(parent);
            break;
        }
    }
#endif
    for (const auto& directory : directories) {
        std::vector<std::filesystem::path> candidates;
        std::error_code error;
        for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error)) {
            if (!it->is_regular_file(error)) { error.clear(); continue; }
            auto extension = it->path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            if (extension == ".zip") candidates.push_back(it->path());
        }
        std::sort(candidates.begin(), candidates.end());
        for (const auto& candidate : candidates) {
            try {
                return load_game_archive(candidate);
            } catch (const std::exception&) { }
        }
    }
    throw std::runtime_error("No Little Fighter ZIP found. Place the original game ZIP next to OpenLF1 or in the current directory.");
}
