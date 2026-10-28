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
#include "game_data.hpp"
#include "game_archive.hpp"
#include "raster.hpp"
#include "battle_view.hpp"
#include "menu_view.hpp"
#include "menu_state.hpp"
#include "menu_logic.hpp"
#include "battle_debug.hpp"
#include "opening.hpp"
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
        const GameData game_data = parse_game_data(archive.read("LitFight/SYS/DATA.DAT"));
        const auto& appearance = game_data.appearance;
        const auto& hair = game_data.hair;
        const auto& boundaries = game_data.boundaries;
        const auto& ranges = game_data.ranges;
        const auto& item_types = game_data.item_types;
        std::array<std::uint32_t, PaletteCycleTuning::entries> palette{};
        palette[0] = rgb(0, 0, 0);
        // The original writes the palette file starting at hardware index 1.
        for (std::size_t i = 1; i < PaletteCycleTuning::entries; ++i) palette[i] = rgb(raw[(i - 1) * 3] & PaletteCycleTuning::levels, raw[(i - 1) * 3 + 1] & PaletteCycleTuning::levels, raw[(i - 1) * 3 + 2] & PaletteCycleTuning::levels);
        Display display;
        Screen screen;
        if (!play_opening(display, screen, opening, palette, raw)) return 0;
        MenuState menu;
        auto& choice = menu.choice; auto& difficulty = menu.difficulty; auto& page = menu.page; auto& mode = menu.mode;
        auto& group = menu.group; auto& players = menu.players; auto& contest = menu.contest; auto& slots = menu.slots;
        auto& next_tick = menu.next_tick; auto& menu_activity = menu.menu_activity; auto& palette_tick = menu.palette_tick;
        auto& bracket_ready = menu.bracket_ready; auto& selection_ready = menu.selection_ready;
        PaletteCycle menu_cycle, tournament_cycle;
        auto& previous_page = menu.previous_page; auto& demonstration = menu.demonstration; auto& paused = menu.paused;
        BattleDebug debug;
        auto& timed = debug.timed; auto& draw_scene = debug.draw_scene;
        auto& tournament = menu.tournament; auto& entrants_wanted = menu.entrants_wanted; auto& entrant_number = menu.entrant_number; auto& candidate = menu.candidate; auto& assigning = menu.assigning;
        auto& assigned_controls = menu.assigned_controls; auto& bracket_entrants = menu.bracket_entrants;
        const auto text_at = [&](std::string_view text, int x, int y, int tint = 0) {
            for (const unsigned char c : text) {
                screen.copy(fonts, (c % 16) * 6, (c / 16) * 8, 5, 7, x, y, true, tint);
                x += 5;
            }
        };
        GameRandom random(current_seed());
        const auto draw = [&](int limit) { return random.draw(limit); };
        FieldTables field_tables;
        for (std::size_t i = 0; i < appearance.size(); ++i) field_tables.specials[i] = appearance[i].specials;
        field_tables.ranges = ranges;
        field_tables.item_types = item_types;
        for (std::size_t i = 0; i < boundaries.size(); ++i) field_tables.bounds[i] = {boundaries[i].top, boundaries[i].bottom};
        Field field(field_tables, random);
        const BattleArt art{stage_left, stage_right, figures, extra_figures, item_images, battle_heads, status, fonts, game_data, raw};
        field.held = [&](int player, Action direction) { return display.held(player, direction); };
        auto& fighters = field.fighters;
        auto& controllers = field.controllers;
        auto& projectiles = field.projectiles;
        auto& ground_items = field.ground_items;
        auto& blasts = field.blasts;
        auto& hit_effects = field.hit_effects;
        auto& round = field.round;
        const auto flush_plays = [&] {
            for (const auto& play : field.plays) display.sound(battle_sounds[play.sample], play.gain);
            field.plays.clear();
        };
        const auto play_menu_sound = [&](MenuSound sound) {
            if (sound == MenuSound::change) display.sound(change_sound);
            if (sound == MenuSound::confirm) display.sound(confirm_sound);
            if (sound == MenuSound::toggle) display.sound(toggle_sound);
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
                draw_battle(screen, palette, field, art, draw_scene);
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
                if (apply_battle_debug(debug, field, input, draw)) display.sound(battle_sounds[BattleSound::explosion]);
                field.pressed_by = display.player();
                field.difficulty = difficulty;
                field.press(input);
                flush_plays();
                if (Clock::now() < next_tick) continue;
                next_tick = Clock::now() + std::chrono::milliseconds(timed ? GameTiming::tick_ms : 0);
                const bool running = field.step();
                flush_plays();
                if (!running) finish_battle(menu, field, draw);
                continue;
            }
            if (page == GameScreen::match_overview) {
                draw_match_overview(screen, words, faces, fighters, controllers, mode, players, group);
                screen.present(display, palette);
                const auto input = display.poll();
                if (input == Action::close) break;
                handle_match_overview(menu, input, field, random);
                continue;
            }
            if (page == GameScreen::mode_selection) {
                draw_mode_selection(screen, modes, mode, players, group, contest);
                screen.present(display, palette);
                const auto input = display.poll();
                if (input == Action::close || input == Action::pause) break;
                play_menu_sound(handle_mode_selection(menu, input));
                continue;
            }
            if (page == GameScreen::tournament_count) {
                screen.copy(count_panels[contest - 1], 0, 0, 213, 150, 53, 25, true);
                const int x = (entrants_wanted - 1) * 16 + 77;
                screen.recolor(x, 100, x + (entrants_wanted < 10 ? 15 : 23), 114, 108, 106);
                screen.present(display, palette);
                const auto input = display.poll();
                if (input == Action::close || input == Action::pause) break;
                play_menu_sound(handle_tournament_count(menu, input));
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
                play_menu_sound(handle_tournament_selection(menu, input, draw));
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
                handle_tournament_bracket(menu, draw);
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
                handle_tournament_winner(menu, input);
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
                switch (handle_tournament_controls(menu, input, display.player(), field, random, boundaries[0].bottom)) {
                case MenuSound::confirm: display.sound(confirm_sound); break;
                case MenuSound::toggle: display.sound(toggle_sound); break;
                default: break;
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
                play_menu_sound(handle_character_selection(menu, input, display.player()));
                start_when_ready(menu, field, draw, boundaries[0].top, boundaries[0].bottom);
                continue;
            }
            draw_main_menu(screen, title, words, menu.choice, menu.difficulty);
            screen.present(display, palette);
            const auto action = display.poll();
            if (action == Action::close || action == Action::pause) break;
            const auto outcome = handle_main_menu(menu, action, field, draw, boundaries[0].top, boundaries[0].bottom);
            play_menu_sound(outcome.sound);
            if (outcome.quit) break;
        }
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
