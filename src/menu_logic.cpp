// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "menu_logic.hpp"
#include "roster.hpp"
#include <algorithm>

MenuSound handle_mode_selection(MenuState& menu, Action input) {
    const bool moved = input == Action::up || input == Action::down || input == Action::left || input == Action::right;
    if (input == Action::left || input == Action::right) {
        if (menu.mode == GameMode::free_for_all) menu.players = 2 + (menu.players - 2 + (input == Action::right ? 1 : 6)) % 7;
        if (menu.mode == GameMode::tournament) menu.contest = 3 - menu.contest;
    }
    if (input == Action::up) {
        if (menu.mode == GameMode::free_for_all) menu.mode = GameMode::tournament;
        else if (menu.mode == GameMode::teams && --menu.group == 0) menu.mode = GameMode::free_for_all;
        else if (menu.mode == GameMode::tournament) { menu.mode = GameMode::teams; menu.group = 5; }
    }
    if (input == Action::down) {
        if (menu.mode == GameMode::free_for_all) { menu.mode = GameMode::teams; menu.group = 1; }
        else if (menu.mode == GameMode::teams && ++menu.group == 6) menu.mode = GameMode::tournament;
        else if (menu.mode == GameMode::tournament) menu.mode = GameMode::free_for_all;
    }
    if (input == Action::accept) {
        menu.rare_character = false;
        if (menu.mode == GameMode::tournament) { menu.entrants_wanted = 1; menu.entrant_number = menu.candidate = 0; menu.entrants = {}; menu.bracket_entrants = {}; menu.tournament.reset(); menu.page = GameScreen::tournament_count; }
        else menu.page = GameScreen::character_selection;
    }
    if (moved) return MenuSound::change;
    return input == Action::accept ? MenuSound::confirm : MenuSound::none;
}

MenuSound handle_tournament_count(MenuState& menu, Action input) {
    const int limit = menu.contest == TournamentFormat::pairs ? TournamentLimits::pair_entrants : TournamentLimits::triple_entrants;
    if (input == Action::left) { menu.entrants_wanted = 1 + (menu.entrants_wanted + limit - 2) % limit; return MenuSound::change; }
    if (input == Action::right) { menu.entrants_wanted = 1 + menu.entrants_wanted % limit; return MenuSound::change; }
    if (input == Action::accept) { menu.page = GameScreen::tournament_selection; return MenuSound::confirm; }
    return MenuSound::none;
}

MenuSound handle_tournament_selection(MenuState& menu, Action input, const std::function<int(int)>& draw) {
    const auto used = [&](int value) {
        for (int i = 0; i < menu.entrant_number; ++i) if (menu.entrants[i].character == value + 1) return true;
        return false;
    };
    if (input == Action::unlock) menu.rare_character = true;
    const int choices = menu.rare_character ? GameLimits::characters : GameLimits::selectable_characters;
    if (input == Action::left || input == Action::right) {
        do { menu.candidate = (menu.candidate + (input == Action::left ? choices - 1 : 1)) % choices; } while (used(menu.candidate));
        return MenuSound::change;
    }
    if (input != Action::accept) return MenuSound::none;
    menu.entrants[menu.entrant_number] = {menu.candidate + 1, menu.entrant_number + 1};
    const int total = menu.contest == TournamentFormat::pairs ? TournamentLimits::pair_entrants : TournamentLimits::triple_entrants;
    Tournament::place(std::span(menu.bracket_entrants).first(total), menu.entrants[menu.entrant_number], draw);
    ++menu.entrant_number;
    if (menu.entrant_number == menu.entrants_wanted) {
        Tournament::fill(std::span(menu.bracket_entrants).first(total), menu.rare_character ? GameLimits::characters : GameLimits::selectable_characters, draw);
        menu.tournament = std::make_unique<Tournament>(menu.contest, std::span(menu.bracket_entrants).first(total));
        menu.page = GameScreen::tournament_bracket;
    } else {
        do { menu.candidate = (menu.candidate + 1) % choices; } while (used(menu.candidate));
    }
    return MenuSound::confirm;
}

void handle_tournament_winner(MenuState& menu, Action input) {
    if (input == Action::accept) { menu.page = GameScreen::main_menu; menu.tournament.reset(); }
}

MenuSound handle_character_selection(MenuState& menu, Action input, int who) {
    MenuSound sound = MenuSound::none;
    if (input == Action::unlock) menu.rare_character = true;
    const int choices = menu.rare_character ? GameLimits::characters : GameLimits::selectable_characters;
    auto& slot = menu.slots[who];
    auto occupied = [&](int fighter) {
        for (int i = 0; i < 3; ++i) if (i != who && menu.slots[i].phase != SelectionPhase::inactive && menu.slots[i].fighter == fighter) return true;
        return false;
    };
    if (slot.phase == SelectionPhase::character && (input == Action::left || input == Action::right)) {
        do { slot.fighter = (slot.fighter + (input == Action::left ? choices - 1 : 1)) % choices; } while (occupied(slot.fighter));
        sound = MenuSound::change;
    }
    if (slot.phase == SelectionPhase::team && (input == Action::up || input == Action::down)) {
        const int count = menu.group < 4 ? 2 : menu.group - 1;
        do {
            slot.team = 1 + (slot.team - 1 + (input == Action::up ? count - 1 : 1)) % count;
            int members = 0;
            for (const auto& other : menu.slots) if (other.phase == SelectionPhase::ready && other.team == slot.team) ++members;
            if (!(menu.group == 1 || menu.group == 4 || menu.group == 5) || members < 2) break;
        } while (true);
        sound = MenuSound::change;
    }
    if (input == Action::accept) {
        if (slot.phase == SelectionPhase::inactive) {
            int joined = 0;
            for (const auto& other : menu.slots) if (other.phase != SelectionPhase::inactive) ++joined;
            if (joined < (menu.mode == GameMode::free_for_all ? menu.players : 8)) {
                while (occupied(slot.fighter)) slot.fighter = (slot.fighter + 1) % choices;
                slot.phase = SelectionPhase::character;
                sound = MenuSound::toggle;
            }
        } else if (slot.phase == SelectionPhase::character) {
            slot.phase = menu.mode == GameMode::teams ? SelectionPhase::team : SelectionPhase::ready;
            if (slot.phase == SelectionPhase::team && (menu.group == 1 || menu.group == 4 || menu.group == 5)) {
                const int count = menu.group < 4 ? 2 : menu.group - 1;
                for (int attempts = 0; attempts < count; ++attempts) {
                    int members = 0;
                    for (const auto& other : menu.slots) if (other.phase == SelectionPhase::ready && other.team == slot.team) ++members;
                    if (members < 2) break;
                    slot.team = slot.team % count + 1;
                }
            }
            sound = MenuSound::confirm;
        } else if (slot.phase == SelectionPhase::team) {
            int members = 0;
            for (const auto& other : menu.slots) if (other.phase == SelectionPhase::ready && other.team == slot.team) ++members;
            if (!(menu.group == 1 || menu.group == 4 || menu.group == 5) || members < 2) {
                slot.phase = SelectionPhase::ready;
                sound = MenuSound::confirm;
            }
        }
    }
    return sound;
}

void handle_tournament_bracket(MenuState& menu, const std::function<int(int)>& draw) {
    if (MenuState::Clock::now() < menu.bracket_ready) return;
    auto& tournament = *menu.tournament;
    const auto fixture = tournament.fixture();
    bool human = false;
    for (int i = 0; i < fixture.count; ++i) human |= tournament.entrant(fixture.contestants[i]).player_number != 0;
    if (!human) {
        std::array<Entrant, 3> competitors{};
        for (int i = 0; i < fixture.count; ++i) competitors[i] = tournament.entrant(fixture.contestants[i]);
        tournament.advance(Tournament::automatic_winner(std::span(competitors).first(fixture.count), draw));
        if (tournament.finished()) menu.page = GameScreen::tournament_winner;
        else menu.bracket_ready = MenuState::Clock::now() + std::chrono::milliseconds(GameTiming::bracket_delay_ms);
    } else {
        menu.assigned_controls = {-1, -1, -1};
        menu.assigning = 0;
        while (menu.assigning < fixture.count && !tournament.entrant(fixture.contestants[menu.assigning]).player_number) ++menu.assigning;
        menu.page = GameScreen::tournament_controls;
    }
}

MenuSound handle_tournament_controls(MenuState& menu, Action input, int pressed_by, Field& field, GameRandom& random, int floor) {
    if (input != Action::accept) return MenuSound::none;
    auto& assigned = menu.assigned_controls;
    if (std::find(assigned.begin(), assigned.end(), pressed_by) != assigned.end()) return MenuSound::toggle;
    const auto fixture = menu.tournament->fixture();
    assigned[menu.assigning] = pressed_by;
    ++menu.assigning;
    while (menu.assigning < fixture.count && !menu.tournament->entrant(fixture.contestants[menu.assigning]).player_number) ++menu.assigning;
    if (menu.assigning == fixture.count) {
        random.seed(current_seed());
        field.stage = random.draw(4); field.camera = 0; field.tick = 0; field.round = {};
        field.projectiles.clear(); field.ground_items.clear(); field.blasts.clear(); field.hit_effects.clear();
        std::array<Entrant, 3> contenders{};
        for (int i = 0; i < fixture.count; ++i) contenders[i] = menu.tournament->entrant(fixture.contestants[i]);
        const auto match = Roster::create_contest(std::span(contenders).first(fixture.count), assigned, floor);
        field.fighters.assign(match.fighters.begin(), match.fighters.end());
        field.controllers.assign(match.controls.begin(), match.controls.end());
        Round::assign_colors(field.fighters);
        equip_initial_bows(field.fighters, field.controllers, field.ground_items);
        menu.next_tick = MenuState::Clock::now();
        menu.page = GameScreen::battle;
    }
    return MenuSound::confirm;
}

MainMenuOutcome handle_main_menu(MenuState& menu, Action input, Field& field, const std::function<int(int)>& draw, int top, int bottom) {
    using Clock = MenuState::Clock;
    MainMenuOutcome outcome;
    if (input != Action::none) menu.menu_activity = Clock::now();
    if (Clock::now() - menu.menu_activity > std::chrono::milliseconds(GameTiming::demo_idle_ms)) {
        menu.demonstration = true; menu.difficulty = 0;
        const int demo_mode = 1 + draw(2);
        const int option = demo_mode == GameMode::free_for_all ? 2 + draw(7) : 1 + draw(5);
        menu.mode = demo_mode;
        if (menu.mode == GameMode::free_for_all) menu.players = option; else menu.group = option;
        field.stage = 0; field.camera = 0; field.tick = 0; field.round = {};
        const auto match = Roster::create(demo_mode, option, {}, top, bottom, draw);
        field.fighters.assign(match.fighters.begin(), match.fighters.end());
        Round::assign_colors(field.fighters);
        field.controllers.assign(match.controls.begin(), match.controls.end());
        field.projectiles.clear(); field.ground_items.clear(); field.blasts.clear(); field.hit_effects.clear();
        menu.overview_ready = Clock::now() + std::chrono::milliseconds(GameTiming::overview_delay_ms);
        menu.page = GameScreen::match_overview;
        return outcome;
    }
    if (input == Action::up || input == Action::down) outcome.sound = MenuSound::change;
    if (input == Action::accept) outcome.sound = menu.choice == 1 ? MenuSound::toggle : MenuSound::confirm;
    if (input == Action::up) menu.choice = (menu.choice + 2) % 3;
    if (input == Action::down) menu.choice = (menu.choice + 1) % 3;
    if (input == Action::accept) {
        if (menu.choice == 2) outcome.quit = true;
        if (menu.choice == 1) menu.difficulty = 5 - menu.difficulty;
        if (menu.choice == 0) menu.page = GameScreen::mode_selection;
    }
    return outcome;
}

void start_when_ready(MenuState& menu, Field& field, const std::function<int(int)>& draw, int top, int bottom) {
    using Clock = MenuState::Clock;
    bool started = false, ready = true;
    for (const auto& entry : menu.slots) { started |= entry.phase == SelectionPhase::ready; ready &= entry.phase == SelectionPhase::inactive || entry.phase == SelectionPhase::ready; }
    if (!started || !ready) return;
    if (menu.selection_ready == Clock::time_point{}) {
        menu.selection_ready = Clock::now() + std::chrono::milliseconds(GameTiming::selection_delay_ms);
        return;
    }
    std::array<Participant, GameLimits::human_players> people{};
    for (int i = 0; i < GameLimits::human_players; ++i) people[i] = {menu.slots[i].phase == SelectionPhase::ready, menu.slots[i].fighter + 1, menu.slots[i].team};
    field.stage = 0;
    field.camera = 0;
    const bool teams = menu.mode == GameMode::teams;
    const auto match = Roster::create(teams ? GameMode::teams : GameMode::free_for_all, teams ? menu.group : menu.players, people, top, bottom, draw, menu.rare_character);
    field.fighters.assign(match.fighters.begin(), match.fighters.end());
    Round::assign_colors(field.fighters);
    field.controllers.assign(match.controls.begin(), match.controls.end());
    field.tick = 0; field.round = {};
    equip_initial_bows(field.fighters, field.controllers, field.ground_items);
    menu.overview_ready = Clock::now() + std::chrono::milliseconds(GameTiming::overview_delay_ms);
    menu.page = GameScreen::match_overview;
}

void handle_match_overview(MenuState& menu, Action input, Field& field, GameRandom& random) {
    if (MenuState::Clock::now() >= menu.overview_ready && (menu.demonstration || input != Action::none)) {
        random.seed(current_seed());
        field.stage = random.draw(4);
        menu.next_tick = MenuState::Clock::now();
        menu.page = GameScreen::battle;
    }
}

void finish_battle(MenuState& menu, Field& field, const std::function<int(int)>& draw) {
    if (menu.tournament && !menu.tournament->finished()) {
        const auto fixture = menu.tournament->fixture();
        menu.tournament->advance(Tournament::decide(field.fighters, fixture.count, draw));
        menu.page = menu.tournament->finished() ? GameScreen::tournament_winner : GameScreen::tournament_bracket;
    } else { menu.slots = {}; menu.choice = 0; menu.page = GameScreen::main_menu; }
    field.clear();
    menu.paused = false;
    if (menu.demonstration) { menu.demonstration = false; menu.difficulty = 5; menu.mode = GameMode::free_for_all; menu.group = 1; menu.players = 2; }
    menu.menu_activity = MenuState::Clock::now();
}
