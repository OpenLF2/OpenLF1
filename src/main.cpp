// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "archive.hpp"
#include "display.hpp"
#include "combat.hpp"
#include "opponent.hpp"
#include "roster.hpp"
#include "round.hpp"
#include "projectile.hpp"
#include "field.hpp"
#include "blast.hpp"
#include "grapple.hpp"
#include "ground_item.hpp"
#include "tournament.hpp"
#include "palette_cycle.hpp"
#include "random.hpp"
#include <array>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <memory>
#include <numeric>
#include <string>
#include <stdexcept>
#include <vector>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
 
#ifdef __DJGPP__
// main and Screen::present together exceed DJGPP's default 512 KiB stack.
extern "C" {
unsigned int _stklen = 1024 * 1024;
}
#endif

namespace {
using Clock = std::chrono::steady_clock;
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
std::uint32_t rgb(unsigned r, unsigned g, unsigned b) {
    return 0xff000000u | ((r * PaletteCycleTuning::output_levels / PaletteCycleTuning::levels) << 16) | ((g * PaletteCycleTuning::output_levels / PaletteCycleTuning::levels) << 8) | (b * PaletteCycleTuning::output_levels / PaletteCycleTuning::levels);
}

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
}
int main(int argc, char** argv) {
    try {
        if (argc > 2) { std::cerr << "Usage: openlf1 [game.zip]\n"; return 1; }
        const std::unique_ptr<FileSystem> storage = argc == 2 ? load_game_archive(argv[1]) : open_game_archive();
        const FileSystem& archive = *storage;
        Raster opening(archive.read("LitFight/SYS/RIGHT.GRH"));
        Raster words(archive.read("LitFight/SYS/CWORD.GRH"));
        Raster title(archive.read("LitFight/SYS/MAIN.GRH"));
        Raster modes(archive.read("LitFight/SYS/MODE.GRH"));
        Raster faces(archive.read("LitFight/SYS/FACE.GRH"));
        Raster figures(archive.read("LitFight/SYS/ACT1.GRH"));
        Raster extra_figures(archive.read("LitFight/SYS/ACT2.GRH"));
        Raster item_images(archive.read("LitFight/SYS/WEAPON.GRH"));
        Raster heads(archive.read("LitFight/SYS/HEAD2.GRH"));
        Raster battle_heads(archive.read("LitFight/SYS/HEAD.GRH"));
        std::vector<Raster> stage_left, stage_right;
        for (char stage = '2'; stage <= '6'; ++stage) {
            stage_left.emplace_back(archive.read(std::string("LitFight/SYS/BACK") + stage + "1.GRH"));
            stage_right.emplace_back(archive.read(std::string("LitFight/SYS/BACK") + stage + "2.GRH"));
        }
        Raster status(archive.read("LitFight/SYS/MENU.GRH"));
        std::array<Raster, 3> prompts{Raster(archive.read("LitFight/SYS/KEY1.GRH")), Raster(archive.read("LitFight/SYS/KEY2.GRH")), Raster(archive.read("LitFight/SYS/KEY3.GRH"))};
        std::vector<Raster> portraits;
        for (char c = 'A'; c <= 'K'; ++c) portraits.emplace_back(archive.read(std::string("LitFight/SYS/SUPER") + c + ".GRH"));
        Raster done(archive.read("LitFight/SYS/ENDC.GRH"));
        Raster teams(archive.read("LitFight/SYS/CGROUP.GRH"));
        Raster control_panel(archive.read("LitFight/SYS/CCON.GRH"));
        Raster contest_labels(archive.read("LitFight/SYS/CONDATA.GRH"));
        Raster winner_panel(archive.read("LitFight/SYS/WINNER.GRH"));
        Raster fonts(archive.read("LitFight/SYS/NEWFONTS"));
        std::array<Raster, 2> count_panels{Raster(archive.read("LitFight/SYS/C1HM.GRH")), Raster(archive.read("LitFight/SYS/C2HM.GRH"))};
        std::array<Raster, 2> brackets{Raster(archive.read("LitFight/SYS/C1BOARD.GRH")), Raster(archive.read("LitFight/SYS/C2BOARD.GRH"))};
        const auto change_sound = archive.read("LitFight/SYS/B3.WAV");
        const auto confirm_sound = archive.read("LitFight/SYS/B2.WAV");
        const auto toggle_sound = archive.read("LitFight/SYS/B1.WAV");
        std::array<std::vector<std::uint8_t>, GameLimits::battle_sounds + 1> battle_sounds;
        for (int i = 1; i <= GameLimits::battle_sounds; ++i)
            battle_sounds[i] = archive.read(std::string("LitFight/SYS/S") + char('A' + i - 1) + ".WAV");
        const auto raw = archive.read("LitFight/SYS/PAL");
        if (raw.size() != PaletteCycleTuning::entries * 3) throw std::runtime_error("Invalid palette");
        struct Appearance { int head; std::array<int, 9> color; std::array<SpecialMove, GameLimits::special_moves> specials; };
        std::array<Appearance, GameLimits::characters> appearance{};
        std::array<int, GameLimits::heads> hair{};
        const auto data = archive.read("LitFight/SYS/DATA.DAT");
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
        std::array<std::array<SpecialMove, GameLimits::special_moves>, GameLimits::characters> special_profiles{};
        for (std::size_t i = 0; i < appearance.size(); ++i) special_profiles[i] = appearance[i].specials;
        std::string end_marker;
        source >> end_marker;
        for (auto& color : hair) source >> color;
        if (!source || end_marker != "E") throw std::runtime_error("Invalid character data");
        struct StageLimits { int left, right, top, bottom, edge_left, edge_right; };
        std::array<StageLimits, GameLimits::stage_table_entries> boundaries{};
        std::string header;
        source >> header >> header;
        for (auto& row : boundaries)
            source >> header >> row.left >> row.right >> row.top >> row.bottom >> row.edge_left >> row.edge_right;
        if (!source) throw std::runtime_error("Invalid stage data");
        std::array<MoveRange, GameLimits::move_ranges> ranges{};
        for (int i = 1;; ++i) {
            source >> header;
            if (!source) throw std::runtime_error("Invalid special data");
            if (header == "E") break;
            if (i >= int(ranges.size())) throw std::runtime_error("Too many special moves");
            source >> ranges[i].center >> ranges[i].width >> ranges[i].depth >> ranges[i].strength;
        }
        source >> header;
        std::array<ProjectileType, GameLimits::item_definitions> item_types{};
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
        std::array<std::uint32_t, PaletteCycleTuning::entries> palette{};
        palette[0] = rgb(0, 0, 0);
        // The original writes the palette file starting at hardware index 1.
        for (std::size_t i = 1; i < PaletteCycleTuning::entries; ++i) palette[i] = rgb(raw[(i - 1) * 3] & PaletteCycleTuning::levels, raw[(i - 1) * 3 + 1] & PaletteCycleTuning::levels, raw[(i - 1) * 3 + 2] & PaletteCycleTuning::levels);
        Display display;
        Screen screen;
        screen.copy(opening, 0, 0, 320, 200, 0, 0);
        auto start = Clock::now();
        bool skip = false;
        for (;;) {
#ifdef __EMSCRIPTEN__
            emscripten_sleep(1);
#endif
            const auto action = display.poll();
            if (action == Action::close) return 0;
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
            // DOS clock ticks at approximately 18.2 Hz; each original wait spans two ticks.
            if (action != Action::none && elapsed >= GameTiming::opening_skip_frames * GameTiming::tick_ms && !skip) { skip = true; start = Clock::now() - std::chrono::milliseconds(GameTiming::opening_skip_start_frames * GameTiming::tick_ms); }
            const int phase = int(elapsed / GameTiming::tick_ms);
            if (phase >= GameTiming::opening_frames || skip && elapsed >= GameTiming::opening_frames * GameTiming::tick_ms) break;
            const int level = phase < GameTiming::opening_fade_in_frames ? (phase + 1) * GameTiming::opening_level_step : phase < GameTiming::opening_fade_out_start ? GameTiming::opening_fade_in_frames * GameTiming::opening_level_step : (GameTiming::opening_frames - phase) * GameTiming::opening_level_step;
            palette[15] = rgb(unsigned(level), unsigned(level), unsigned(level));
            screen.present(display, palette);
        }
        for (std::size_t i = 1; i < PaletteCycleTuning::entries; ++i) palette[i] = rgb(raw[(i - 1) * 3] & PaletteCycleTuning::levels, raw[(i - 1) * 3 + 1] & PaletteCycleTuning::levels, raw[(i - 1) * 3 + 2] & PaletteCycleTuning::levels);
        int choice = 0, difficulty = 5, page = GameScreen::main_menu, mode = GameMode::free_for_all, group = 1, players = 2, contest = TournamentFormat::pairs;
        struct Slot { SelectionPhase phase = SelectionPhase::inactive; int fighter = 0; int team = 1; };
        std::array<Slot, GameLimits::human_players> slots{};
        auto next_tick = Clock::now();
        auto menu_activity = Clock::now();
        auto palette_tick = Clock::now();
        auto overview_ready = Clock::now();
        auto bracket_ready = Clock::now();
        auto selection_ready = Clock::time_point{};
        PaletteCycle menu_cycle, tournament_cycle;
        int previous_page = GameScreen::main_menu;
        bool demonstration = false, rare_character = false, paused = false, timed = true, draw_scene = true;
        DebugMode debug_mode = DebugMode::off;
        std::unique_ptr<Tournament> tournament;
        int entrants_wanted = 1, entrant_number = 0, candidate = 0, assigning = 0;
        std::array<int, 3> assigned_controls{-1, -1, -1};
        std::array<Entrant, TournamentLimits::pair_entrants> entrants{};
        std::array<Entrant, TournamentLimits::pair_entrants> bracket_entrants{};
        const auto text_at = [&](std::string_view text, int x, int y, int tint = 0) {
            for (const unsigned char c : text) {
                screen.copy(fonts, (c % 16) * 6, (c / 16) * 8, 5, 7, x, y, true, tint);
                x += 5;
            }
        };
        std::array<int, FighterAction::normal_pose_count + 1> head_x{}, head_y{};
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
        const auto current_seed = [] {
            return static_cast<std::uint16_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        };
        GameRandom random(current_seed());
        const auto draw = [&](int limit) { return random.draw(limit); };
        FieldTables field_tables;
        for (std::size_t i = 0; i < appearance.size(); ++i) field_tables.specials[i] = appearance[i].specials;
        field_tables.ranges = ranges;
        field_tables.item_types = item_types;
        for (std::size_t i = 0; i < boundaries.size(); ++i) field_tables.bounds[i] = {boundaries[i].top, boundaries[i].bottom};
        Field field(field_tables, random);
        field.held = [&](int player, Action direction) { return display.held(player, direction); };
        auto& fighters = field.fighters;
        auto& controllers = field.controllers;
        auto& projectiles = field.projectiles;
        auto& ground_items = field.ground_items;
        auto& blasts = field.blasts;
        auto& hit_effects = field.hit_effects;
        auto& round = field.round;
        auto& tick = field.tick;
        auto& stage = field.stage;
        auto& camera = field.camera;
        const auto flush_plays = [&] {
            for (const auto& play : field.plays) display.sound(battle_sounds[play.sample], play.gain);
            field.plays.clear();
        };
        const auto create_pending_item = [&](std::size_t fighter_index) {
            auto& fighter = fighters[fighter_index];
            if (!fighter.item_pending) return;
            while (ground_items.size() < 5) { GroundItem empty; empty.active = false; ground_items.push_back(empty); }
            const auto free_item = std::find_if(ground_items.begin(), ground_items.end(), [](const auto& item) { return !item.active; });
            if (free_item != ground_items.end()) {
                const int index = int(free_item - ground_items.begin());
                if (fighter.carried >= 0 && fighter.carried < int(ground_items.size())) {
                    auto& prior = ground_items[fighter.carried];
                    prior.owner = ItemOwnership::unowned; prior.last_owner = ItemOwnership::detached; prior.settled = false; prior.frame = 3; prior.vz = -7; prior.vx = draw(7) - 3;
                }
                *free_item = GroundItem{};
                free_item->type = *fighter.item_pending; free_item->owner = free_item->last_owner = int(fighter_index); free_item->durability = *fighter.item_pending == ItemType::summoned_sword ? 5 : 1;
                free_item->x = fighter.x; free_item->y = fighter.y + 1; free_item->z = fighter.animation_step - 11; free_item->facing = fighter.facing;
                fighter.carried = index;
            } else { fighter.state = FighterState::idle; fighter.action = FighterAction::idle; }
            fighter.item_pending.reset();
        };
        const auto show_hit = [&](const Combatant& target, const ImpactRegion& impact, int facing, int original_z, const std::array<int, 2>& offsets) {
            const int effect = impact.damage / ImpactCode::scale;
            if (effect == ImpactCode::normal_kind || effect == ImpactCode::explosion_kind || effect == ImpactCode::piercing_kind)
                hit_effects.push_back({target.x - facing * 7 + offsets[0], target.y + offsets[1], original_z - 12, facing});
        };
        for (;;) {
#ifdef __EMSCRIPTEN__
            emscripten_sleep(1);
#endif
            if (previous_page == GameScreen::battle && page != GameScreen::battle) {
                for (std::size_t i = 1; i < PaletteCycleTuning::entries; ++i) palette[i] = rgb(raw[(i - 1) * 3] & PaletteCycleTuning::levels, raw[(i - 1) * 3 + 1] & PaletteCycleTuning::levels, raw[(i - 1) * 3 + 2] & PaletteCycleTuning::levels);
                if (page == GameScreen::main_menu) menu_cycle = {};
                palette_tick = Clock::now();
            }
            if (page == GameScreen::tournament_bracket && previous_page != GameScreen::tournament_bracket) {
                random.seed(current_seed());
                bracket_ready = Clock::now() + std::chrono::milliseconds(GameTiming::bracket_delay_ms);
            }
            if (page == GameScreen::main_menu && (previous_page == GameScreen::battle || previous_page == GameScreen::tournament_winner)) random.seed(current_seed());
            if (page != GameScreen::character_selection) selection_ready = {};
            previous_page = page;
            if (page != GameScreen::battle && Clock::now() >= palette_tick) {
                const bool contest_scene = page == GameScreen::tournament_bracket || page == GameScreen::tournament_winner || page == GameScreen::tournament_controls;
                (contest_scene ? tournament_cycle : menu_cycle).step(raw, palette, !contest_scene);
                palette_tick = Clock::now() + std::chrono::milliseconds(GameTiming::tick_ms);
            }
            for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) screen.copy(words, 0, 0, 80, 50, x * 80, y * 50);
            if (page == GameScreen::battle) {
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
                screen.present(display, palette);
                const auto input = display.poll();
                if (input == Action::close) break;
                if (input == Action::end_round || (demonstration && input != Action::none)) {
                    slots = {}; fighters.clear(); controllers.clear(); projectiles.clear(); ground_items.clear(); blasts.clear(); hit_effects.clear(); tournament.reset(); choice = 0; page = GameScreen::main_menu; round = {};
                    if (demonstration) { difficulty = 5; mode = GameMode::free_for_all; group = 1; players = 2; }
                    demonstration = false; paused = false; menu_activity = Clock::now();
                    continue;
                }
                if (paused) {
                    if (input != Action::none) { paused = false; next_tick = Clock::now() + std::chrono::milliseconds(GameTiming::tick_ms); }
                    continue;
                }
                if (input == Action::pause) { paused = true; continue; }
                if (input == Action::unlock) debug_mode = DebugMode::battle_controls;
                if (input == Action::cheat_mode2) debug_mode = DebugMode::character_selection;
                if (debug_mode == DebugMode::battle_controls) {
                    if (input == Action::cheat_previous_stage) stage = (stage + GameLimits::stages - 1) % GameLimits::stages;
                    if (input == Action::cheat_next_stage) stage = (stage + 1) % GameLimits::stages;
                    if (input == Action::cheat_timer) timed = !timed;
                    if (input == Action::cheat_6) { timed = false; draw_scene = false; }
                    if (input == Action::cheat_7 && !fighters.empty()) fighters[0].active = false;
                    if (input == Action::cheat_8 && fighters.size() > 3) fighters[3].active = false;
                    const int player = input == Action::cheat_heal1 ? 0 : input == Action::cheat_heal2 ? 1 : input == Action::cheat_heal3 ? 2 : -1;
                    if (player >= 0) for (std::size_t i = 0; i < fighters.size(); ++i) if (controllers[i] == player) {
                        fighters[i].health = fighters[i].max_health; fighters[i].energy = fighters[i].max_energy;
                    }
                    if (input == Action::cheat_clear_items) {
                        std::vector<Projectile> effects;
                        for (auto& item : ground_items) if (item.active && item.type != ItemType::clear_effect) {
                            Projectile effect; effect.owner = 0; effect.type = ItemType::clear_effect; effect.x = item.x; effect.y = item.y; effect.z = item.z; effect.exploding = true;
                            effects.push_back(effect); item.active = false; item.owner = ItemOwnership::unowned;
                        }
                        for (const auto& projectile : projectiles) if (projectile.active && projectile.type != ItemType::clear_effect) {
                            Projectile effect; effect.owner = projectile.owner; effect.type = ItemType::clear_effect; effect.x = projectile.x; effect.y = projectile.y; effect.z = projectile.z; effect.exploding = true;
                            effects.push_back(effect);
                        }
                        for (auto& fighter : fighters) fighter.carried = ItemSlot::none;
                        projectiles = std::move(effects);
                        hit_effects.clear();
                        if (!projectiles.empty()) display.sound(battle_sounds[BattleSound::explosion]);
                    }
                    if (input == Action::cheat_spawn_items) {
                        for (int type = 1; type <= 10; ++type) {
                            auto item = GroundItem::create(draw);
                            item.type = type == ItemType::head ? ItemType::drink : type;
                            item.variant = type == ItemType::head ? 1 : 0;
                            item.durability = item.type == ItemType::stone || item.type == ItemType::box || item.type == ItemType::claw ? 2 : 6;
                            ground_items.push_back(item);
                        }
                    }
                }
                if (debug_mode == DebugMode::character_selection && input >= Action::cheat_1 && input <= Action::cheat_8) {
                    const auto slot = std::size_t(int(input) - int(Action::cheat_1));
                    if (slot < fighters.size()) {
                        int candidate = fighters[slot].archetype;
                        do { candidate = candidate % GameLimits::characters + 1; }
                        while (std::any_of(fighters.begin(), fighters.end(), [&](const auto& other) { return &other != &fighters[slot] && other.active && other.archetype == candidate; }));
                        fighters[slot].archetype = candidate;
                    }
                }
                field.pressed_by = display.player();
                field.difficulty = difficulty;
                field.press(input);
                flush_plays();
                if (Clock::now() < next_tick) continue;
                next_tick = Clock::now() + std::chrono::milliseconds(timed ? GameTiming::tick_ms : 0);
                const bool running = field.step();
                flush_plays();
                if (!running) {
                    if (tournament && !tournament->finished()) {
                        const auto fixture = tournament->fixture();
                        tournament->advance(Tournament::decide(fighters, fixture.count, draw));
                        page = tournament->finished() ? GameScreen::tournament_winner : GameScreen::tournament_bracket;
                    } else { slots = {}; choice = 0; page = GameScreen::main_menu; }
                    field.clear(); paused = false;
                    if (demonstration) { demonstration = false; difficulty = 5; mode = GameMode::free_for_all; group = 1; players = 2; }
                    menu_activity = Clock::now();
                }
                continue;
            }
            if (page == GameScreen::match_overview) {
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
                screen.present(display, palette);
                const auto input = display.poll();
                if (input == Action::close) break;
                if (Clock::now() >= overview_ready && (demonstration || input != Action::none)) {
                    random.seed(current_seed());
                    stage = draw(4);
                    next_tick = Clock::now(); page = GameScreen::battle;
                }
                continue;
            }
            if (page == GameScreen::mode_selection) {
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
                screen.present(display, palette);
                const auto input = display.poll();
                if (input == Action::close || input == Action::pause) break;
                if (input == Action::up || input == Action::down || input == Action::left || input == Action::right) display.sound(change_sound);
                if (input == Action::accept) display.sound(confirm_sound);
                if (input == Action::left || input == Action::right) {
                    if (mode == GameMode::free_for_all) players = 2 + (players - 2 + (input == Action::right ? 1 : 6)) % 7;
                    if (mode == GameMode::tournament) contest = 3 - contest;
                }
                if (input == Action::up) {
                    if (mode == GameMode::free_for_all) mode = GameMode::tournament;
                    else if (mode == GameMode::teams && --group == 0) mode = GameMode::free_for_all;
                    else if (mode == GameMode::tournament) { mode = GameMode::teams; group = 5; }
                }
                if (input == Action::down) {
                    if (mode == GameMode::free_for_all) { mode = GameMode::teams; group = 1; }
                    else if (mode == GameMode::teams && ++group == 6) mode = GameMode::tournament;
                    else if (mode == GameMode::tournament) mode = GameMode::free_for_all;
                }
                if (input == Action::accept) {
                    rare_character = false;
                    if (mode == GameMode::tournament) { entrants_wanted = 1; entrant_number = candidate = 0; entrants = {}; bracket_entrants = {}; tournament.reset(); page = GameScreen::tournament_count; }
                    else page = GameScreen::character_selection;
                }
                continue;
            }
            if (page == GameScreen::tournament_count) {
                screen.copy(count_panels[contest - 1], 0, 0, 213, 150, 53, 25, true);
                const int x = (entrants_wanted - 1) * 16 + 77;
                screen.recolor(x, 100, x + (entrants_wanted < 10 ? 15 : 23), 114, 108, 106);
                screen.present(display, palette);
                const auto input = display.poll();
                if (input == Action::close || input == Action::pause) break;
                const int limit = contest == TournamentFormat::pairs ? TournamentLimits::pair_entrants : TournamentLimits::triple_entrants;
                if (input == Action::left) { entrants_wanted = 1 + (entrants_wanted + limit - 2) % limit; display.sound(change_sound); }
                if (input == Action::right) { entrants_wanted = 1 + entrants_wanted % limit; display.sound(change_sound); }
                if (input == Action::accept) { page = GameScreen::tournament_selection; display.sound(confirm_sound); }
                continue;
            }
            if (page == GameScreen::tournament_selection) {
                screen.copy(brackets[contest - 1], 0, 0, 213, 150, 108, 33);
                screen.copy(words, 0, 106, 64, 14, 21, 0, true);
                if (entrant_number < 3) screen.copy(words, 0, 120 + entrant_number * 14, 64, 14, 21, 17, true);
                else if (entrant_number < 7) screen.copy(words, 171, (entrant_number - 3) * 15, 64, 13, 21, 18, true);
                else screen.copy(words, 241, (entrant_number - 7) * 15, 64, 13, 21, 18, true);
                screen.copy(portraits[candidate], 0, 0, 106, 152, 0, 32);
                constexpr int face_order[] = {1, 2, 3, 8, 10, 9, 4, 7, 5, 11, 6};
                const int face = face_order[candidate] - 1;
                screen.copy(faces, (face % 6) * 50, (face / 6) * 50, 50, 50, 53, 35);
                const auto& look = appearance[candidate];
                for (int c = 0; c < 10; ++c) {
                    const int index = c == 9 ? hair[look.head - 1] : look.color[c];
                    palette[151 + c] = rgb(raw[(index - 1) * 3] & PaletteCycleTuning::levels, raw[(index - 1) * 3 + 1] & PaletteCycleTuning::levels, raw[(index - 1) * 3 + 2] & PaletteCycleTuning::levels);
                }
                const auto step = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count() / GameTiming::tick_ms % 4;
                const int pose = step == 3 ? 1 : int(step);
                screen.copy(figures, pose * 26 + 1, 1, 25, 25, 9, 45, true);
                const int head = look.head - 1;
                screen.copy(heads, (head % 11) * 18 + 1, (head / 11) * 13 + 1, 17, 12, 9, 45, true);
                for (int n = 0; n < (contest == TournamentFormat::pairs ? TournamentLimits::pair_entrants : TournamentLimits::triple_entrants); ++n) if (bracket_entrants[n].character) {
                    const int head = appearance[bracket_entrants[n].character - 1].head - 1;
                    const int x = contest == TournamentFormat::pairs ? n * 20 + 113 : (n / 3) * 70 + (n % 3) * 20 + 113;
                    screen.copy(heads, (head % 11) * 18 + 1, (head / 11) * 13 + 1, 17, 12, x, 152, true);
                    text_at(std::to_string(bracket_entrants[n].player_number), x + 3, 168);
                }
                screen.present(display, palette);
                const auto input = display.poll();
                if (input == Action::close || input == Action::pause) break;
                const auto used = [&](int value) {
                    for (int i = 0; i < entrant_number; ++i) if (entrants[i].character == value + 1) return true;
                    return false;
                };
                if (input == Action::unlock) rare_character = true;
                const int choices = rare_character ? GameLimits::characters : GameLimits::selectable_characters;
                if (input == Action::left || input == Action::right) {
                    do { candidate = (candidate + (input == Action::left ? choices - 1 : 1)) % choices; } while (used(candidate));
                    display.sound(change_sound);
                }
                if (input == Action::accept) {
                    entrants[entrant_number] = {candidate + 1, entrant_number + 1};
                    const int total = contest == TournamentFormat::pairs ? TournamentLimits::pair_entrants : TournamentLimits::triple_entrants;
                    Tournament::place(std::span(bracket_entrants).first(total), entrants[entrant_number], draw);
                    ++entrant_number;
                    display.sound(confirm_sound);
                    if (entrant_number == entrants_wanted) {
                        Tournament::fill(std::span(bracket_entrants).first(total), rare_character ? GameLimits::characters : GameLimits::selectable_characters, draw);
                        tournament = std::make_unique<Tournament>(contest, std::span(bracket_entrants).first(total));
                        page = GameScreen::tournament_bracket;
                    } else {
                        do { candidate = (candidate + 1) % choices; } while (used(candidate));
                    }
                }
                continue;
            }
            if (page == GameScreen::tournament_bracket) {
                const int route_count = contest == TournamentFormat::pairs ? TournamentLimits::pair_nodes : TournamentLimits::triple_nodes;
                for (int i = 0; i < route_count; ++i) {
                    const auto route = tournament->route(i);
                    const int color = route == TournamentRoute::eliminated ? 327 : 321;
                    palette[i + 201] = route == TournamentRoute::advanced ? rgb(PaletteCycleTuning::levels, PaletteCycleTuning::levels, PaletteCycleTuning::levels) : rgb(raw[color] & PaletteCycleTuning::levels, raw[color + 1] & PaletteCycleTuning::levels, raw[color + 2] & PaletteCycleTuning::levels);
                }
                const auto active_fixture = tournament->fixture();
                const bool blink = (std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count() / 220) % 2 != 0;
                const int highlight = blink ? 35 : 63;
                for (int i = 0; i < active_fixture.count; ++i)
                    palette[active_fixture.contestants[i] + 201] = rgb(highlight, highlight, highlight);
                screen.copy(brackets[contest - 1], 0, 0, 213, 150, 53, 22);
                const int total = contest == TournamentFormat::pairs ? TournamentLimits::pair_entrants : TournamentLimits::triple_entrants;
                for (int i = 0; i < total; ++i) {
                    const auto& entry = tournament->entrant(i);
                    const int head = appearance[entry.character - 1].head - 1;
                    const int x = contest == TournamentFormat::pairs ? i * 20 + 58 : (i / 3) * 70 + (i % 3) * 20 + 58;
                    screen.copy(heads, (head % 11) * 18 + 1, (head / 11) * 13 + 1, 17, 12, x, 142, true);
                    text_at(entry.player_number ? std::to_string(entry.player_number) : "c", x + 3, 158);
                }
                screen.present(display, palette);
                const auto input = display.poll();
                if (input == Action::close || input == Action::pause) break;
                if (Clock::now() >= bracket_ready) {
                    const auto fixture = tournament->fixture();
                    bool human = false;
                    for (int i = 0; i < fixture.count; ++i) human |= tournament->entrant(fixture.contestants[i]).player_number != 0;
                    if (!human) {
                        std::array<Entrant, 3> competitors{};
                        for (int i = 0; i < fixture.count; ++i) competitors[i] = tournament->entrant(fixture.contestants[i]);
                        tournament->advance(Tournament::automatic_winner(std::span(competitors).first(fixture.count), draw));
                        if (tournament->finished()) page = GameScreen::tournament_winner;
                        else bracket_ready = Clock::now() + std::chrono::milliseconds(GameTiming::bracket_delay_ms);
                    } else {
                        assigned_controls = {-1, -1, -1};
                        assigning = 0;
                        while (assigning < fixture.count && !tournament->entrant(fixture.contestants[assigning]).player_number) ++assigning;
                        page = GameScreen::tournament_controls;
                    }
                }
                continue;
            }
            if (page == GameScreen::tournament_winner) {
                const auto& champion = tournament->entrant(tournament->champion());
                screen.copy(winner_panel, 0, 0, 213, 150, 53, 25);
                const int face_index[] = {1,2,3,8,10,9,4,7,5,11,6};
                const int face = face_index[champion.character - 1] - 1;
                screen.copy(faces, (face % 6) * 50, (face / 6) * 50, 50, 50, 134, 85);
                screen.copy(contest_labels, 190, (champion.character - 1) * 15, 96, 15, 134, 134, true);
                screen.copy(contest_labels, 0, champion.player_number ? 90 : 105, 64, 15, 126, 61, true);
                if (champion.player_number) {
                    if (champion.player_number >= 10) screen.copy(contest_labels, 8, 75, 8, 15, 179, 61, true);
                    screen.copy(contest_labels, (champion.player_number % 10) * 8, 75, 8, 15, champion.player_number >= 10 ? 186 : 179, 61, true);
                }
                screen.present(display, palette);
                const auto input = display.poll();
                if (input == Action::close || input == Action::pause) break;
                if (input == Action::accept) { page = GameScreen::main_menu; tournament.reset(); }
                continue;
            }
            if (page == GameScreen::tournament_controls) {
                const auto fixture = tournament->fixture();
                const auto& entry = tournament->entrant(fixture.contestants[assigning]);
                screen.copy(control_panel, 0, 0, 300, 180, 10, 10);
                for (int i = 0; i < 3; ++i) {
                    const bool chosen = std::find(assigned_controls.begin(), assigned_controls.end(), i) != assigned_controls.end();
                    palette[i + 151] = chosen ? rgb(30, 30, 30) : rgb(PaletteCycleTuning::levels, PaletteCycleTuning::levels, PaletteCycleTuning::levels);
                }
                constexpr int face_index[] = {1,2,3,8,10,9,4,7,5,11,6};
                const int face = face_index[entry.character - 1] - 1;
                screen.copy(faces, (face % 6) * 50, (face / 6) * 50, 50, 50, 38, 22);
                screen.copy(contest_labels, 190, (entry.character - 1) * 15, 96, 15, 39, 73, true);
                if (entry.player_number >= 10) screen.copy(contest_labels, 8, 75, 8, 15, 160, 55, true);
                screen.copy(contest_labels, (entry.player_number % 10) * 8, 75, 8, 15, entry.player_number >= 10 ? 167 : 160, 55, true);
                screen.present(display, palette);
                const auto input = display.poll();
                if (input == Action::close || input == Action::pause) break;
                if (input == Action::accept) {
                    const int selected = display.player();
                    if (std::find(assigned_controls.begin(), assigned_controls.end(), selected) == assigned_controls.end()) {
                        assigned_controls[assigning] = selected;
                        ++assigning;
                        while (assigning < fixture.count && !tournament->entrant(fixture.contestants[assigning]).player_number) ++assigning;
                        display.sound(confirm_sound);
                        if (assigning == fixture.count) {
                            random.seed(current_seed());
                            stage = draw(4); camera = 0; tick = 0; round = {}; projectiles.clear(); ground_items.clear(); blasts.clear(); hit_effects.clear();
                            fighters.assign(GameLimits::fighter_slots, Combatant{}); controllers.assign(GameLimits::fighter_slots, -1);
                            for (auto& fighter : fighters) { fighter.active = false; fighter.x = 64; fighter.y = boundaries[0].bottom; }
                            for (int i = 0; i < fixture.count; ++i) {
                                const auto& contender = tournament->entrant(fixture.contestants[i]);
                                const int slot = fixture.count == 2 && i == 1 ? 3 : i;
                                auto& fighter = fighters[slot];
                                fighter.active = true; fighter.archetype = contender.character; fighter.team = i + 1;
                                fighter.x = 104 + i * (fixture.count == 2 ? 176 : 88);
                                fighter.y = boundaries[0].bottom - 90;
                                fighter.facing = fighter.x < 160 ? 1 : -1;
                                fighter.health = fighter.max_health = FighterDefaults::battle_health; fighter.energy = FighterDefaults::battle_energy; fighter.max_energy = FighterDefaults::battle_max_energy;
                                fighter.effect_ticks = FighterDefaults::spawn_invulnerability; fighter.computer = contender.player_number == 0;
                                fighter.marker = contender.player_number; fighter.score = 1;
                                controllers[slot] = assigned_controls[i];
                            }
                            Round::assign_colors(fighters);
                            equip_initial_bows(fighters, controllers, ground_items);
                            next_tick = Clock::now(); page = GameScreen::battle;
                        }
                    } else display.sound(toggle_sound);
                }
                continue;
            }
            if (page == GameScreen::character_selection) {
                screen.copy(words, 0, 106, 64, 14, 128, 0, true);
                const int joined = int(std::count_if(slots.begin(), slots.end(), [](const auto& slot) { return slot.phase != SelectionPhase::inactive; }));
                for (int i = 0; i < 3; ++i) {
                    screen.copy(words, 0, 120 + i * 14, 64, 14, 21 + i * 106, 16, true);
                    const int x = i * 107;
                    const auto& slot = slots[i];
                    if (slot.phase == SelectionPhase::character) {
                        screen.copy(portraits[slot.fighter], 0, 0, 106, 152, x, 32);
                        constexpr int face_order[] = {1, 2, 3, 8, 10, 9, 4, 7, 5, 11, 6};
                        const int face = face_order[slot.fighter] - 1;
                        screen.copy(faces, (face % 6) * 50, (face / 6) * 50, 50, 50, x + 53, 35);
                        const auto& look = appearance[slot.fighter];
                        for (int c = 0; c < 10; ++c) {
                            const int index = c == 9 ? hair[look.head - 1] : look.color[c];
                            if (index < 1 || index > PaletteCycleTuning::entries) throw std::runtime_error("Invalid character palette");
                            palette[i * 10 + 151 + c] = rgb(raw[(index - 1) * 3] & PaletteCycleTuning::levels, raw[(index - 1) * 3 + 1] & PaletteCycleTuning::levels, raw[(index - 1) * 3 + 2] & PaletteCycleTuning::levels);
                        }
                        const auto tick = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count() / GameTiming::tick_ms;
                        // The original advances the shared animation step once per player slot.
                        const int frame = int((tick * 3 + i) % 4);
                        const int pose = frame == 3 ? 1 : frame;
                        screen.copy(figures, pose * 26 + 1, 1, 25, 25, x + 9, 45, true, i * 10);
                        constexpr int head_order[] = {1, 2, 3, 8, 4, 6, 5, 7, 9, 10, 11};
                        const int head = head_order[slot.fighter] - 1;
                        screen.copy(heads, (head % 11) * 18 + 1, (head / 11) * 13 + 1, 17, 12, x + 9, 45, true);
                    } else if (slot.phase == SelectionPhase::team) {
                        screen.copy(teams, 0, 0, 106, 152, x, 32);
                        for (int team = 1; team <= 4; ++team) {
                            int members = 0;
                            for (const auto& other : slots) if (other.phase == SelectionPhase::ready && other.team == team) ++members;
                            const int available = group < 4 ? 2 : group - 1;
                            if (team > available || ((group == 1 || group == 4 || group == 5) && members >= 2))
                                screen.recolor(x + 21, (team - 1) * 17 + 82, x + 84, (team - 1) * 17 + 96, 15, 12);
                        }
                        screen.recolor(x + 21, (slot.team - 1) * 17 + 82, x + 84, (slot.team - 1) * 17 + 96, 108, 106);
                    } else if (slot.phase == SelectionPhase::ready) {
                        screen.copy(done, 0, 0, 106, 152, x, 32);
                    } else if (mode != GameMode::free_for_all || joined < players) {
                        screen.copy(prompts[i], 0, 0, 106, 152, x, 32);
                    }
                }
                screen.present(display, palette);
                const auto polled = display.poll();
                if (polled == Action::close || polled == Action::pause) break;
                if (selection_ready != Clock::time_point{} && Clock::now() < selection_ready) continue;
                const auto input = selection_ready == Clock::time_point{} ? polled : Action::none;
                if (input == Action::unlock) rare_character = true;
                const int choices = rare_character ? GameLimits::characters : GameLimits::selectable_characters;
                const int who = display.player();
                auto& slot = slots[who];
                auto occupied = [&](int fighter) {
                    for (int i = 0; i < 3; ++i) if (i != who && slots[i].phase != SelectionPhase::inactive && slots[i].fighter == fighter) return true;
                    return false;
                };
                if (slot.phase == SelectionPhase::character && (input == Action::left || input == Action::right)) {
                    do { slot.fighter = (slot.fighter + (input == Action::left ? choices - 1 : 1)) % choices; } while (occupied(slot.fighter));
                    display.sound(change_sound);
                }
                if (slot.phase == SelectionPhase::team && (input == Action::up || input == Action::down)) {
                    const int count = group < 4 ? 2 : group - 1;
                    do {
                        slot.team = 1 + (slot.team - 1 + (input == Action::up ? count - 1 : 1)) % count;
                        int members = 0;
                        for (const auto& other : slots) if (other.phase == SelectionPhase::ready && other.team == slot.team) ++members;
                        if (!(group == 1 || group == 4 || group == 5) || members < 2) break;
                    } while (true);
                    display.sound(change_sound);
                }
                if (input == Action::accept) {
                    if (slot.phase == SelectionPhase::inactive) {
                        int joined = 0;
                        for (const auto& other : slots) if (other.phase != SelectionPhase::inactive) ++joined;
                        if (joined < (mode == GameMode::free_for_all ? players : 8)) {
                            while (occupied(slot.fighter)) slot.fighter = (slot.fighter + 1) % choices;
                            slot.phase = SelectionPhase::character;
                            display.sound(toggle_sound);
                        }
                    } else if (slot.phase == SelectionPhase::character) {
                        slot.phase = mode == GameMode::teams ? SelectionPhase::team : SelectionPhase::ready;
                        if (slot.phase == SelectionPhase::team && (group == 1 || group == 4 || group == 5)) {
                            const int count = group < 4 ? 2 : group - 1;
                            for (int attempts = 0; attempts < count; ++attempts) {
                                int members = 0;
                                for (const auto& other : slots) if (other.phase == SelectionPhase::ready && other.team == slot.team) ++members;
                                if (members < 2) break;
                                slot.team = slot.team % count + 1;
                            }
                        }
                        display.sound(confirm_sound);
                    } else if (slot.phase == SelectionPhase::team) {
                        int members = 0;
                        for (const auto& other : slots) if (other.phase == SelectionPhase::ready && other.team == slot.team) ++members;
                        if (!(group == 1 || group == 4 || group == 5) || members < 2) {
                            slot.phase = SelectionPhase::ready;
                            display.sound(confirm_sound);
                        }
                    }
                }
                bool started = false, ready = true;
                for (const auto& entry : slots) { started |= entry.phase == SelectionPhase::ready; ready &= entry.phase == SelectionPhase::inactive || entry.phase == SelectionPhase::ready; }
                if (started && ready) {
                    if (selection_ready == Clock::time_point{}) {
                        selection_ready = Clock::now() + std::chrono::milliseconds(GameTiming::selection_delay_ms);
                        continue;
                    }
                    std::array<Participant, GameLimits::human_players> people{};
                    for (int i = 0; i < GameLimits::human_players; ++i) people[i] = {slots[i].phase == SelectionPhase::ready, slots[i].fighter + 1, slots[i].team};
                    stage = 0;
                    camera = 0;
                    const auto match = Roster::create(mode == GameMode::teams ? GameMode::teams : GameMode::free_for_all, mode == GameMode::teams ? group : players, people, boundaries[0].top, boundaries[0].bottom, draw, rare_character);
                    fighters.assign(match.fighters.begin(), match.fighters.end());
                    Round::assign_colors(fighters);
                    controllers.assign(match.controls.begin(), match.controls.end());
                    tick = 0; round = {};
                    equip_initial_bows(fighters, controllers, ground_items);
                    overview_ready = Clock::now() + std::chrono::milliseconds(GameTiming::overview_delay_ms); page = GameScreen::match_overview;
                }
                continue;
            }
            screen.copy(title, 0, 0, 320, 200, 0, 0, true);
            screen.copy(words, 80, 56, 64, 14, 128, 112, true, choice == 0 ? 0 : 47);
            screen.copy(words, 80, difficulty == 0 ? 0 : 14, 88, 14, 116, 133, true, choice == 1 ? 0 : 47);
            screen.copy(words, 80, 42, 64, 14, 128, 154, true, choice == 2 ? 0 : 47);
            screen.present(display, palette);
            const auto action = display.poll();
            if (action == Action::close || action == Action::pause) break;
            if (action != Action::none) menu_activity = Clock::now();
            if (Clock::now() - menu_activity > std::chrono::milliseconds(GameTiming::demo_idle_ms)) {
                demonstration = true; difficulty = 0;
                const int demo_mode = 1 + draw(2);
                const int option = demo_mode == GameMode::free_for_all ? 2 + draw(7) : 1 + draw(5);
                mode = demo_mode;
                if (mode == GameMode::free_for_all) players = option; else group = option;
                stage = 0; camera = 0; tick = 0; round = {};
                const auto match = Roster::create(demo_mode, option, {}, boundaries[0].top, boundaries[0].bottom, draw);
                fighters.assign(match.fighters.begin(), match.fighters.end());
                Round::assign_colors(fighters);
                controllers.assign(match.controls.begin(), match.controls.end());
                projectiles.clear(); ground_items.clear(); blasts.clear(); hit_effects.clear();
                overview_ready = Clock::now() + std::chrono::milliseconds(GameTiming::overview_delay_ms); page = GameScreen::match_overview;
                continue;
            }
            if (action == Action::up || action == Action::down) display.sound(change_sound);
            if (action == Action::accept) display.sound(choice == 1 ? toggle_sound : confirm_sound);
            if (action == Action::up) choice = (choice + 2) % 3;
            if (action == Action::down) choice = (choice + 1) % 3;
            if (action == Action::accept) {
                if (choice == 2) break;
                if (choice == 1) difficulty = 5 - difficulty;
                if (choice == 0) page = GameScreen::mode_selection;
            }
        }
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
