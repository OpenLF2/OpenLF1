// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "tournament.hpp"
#include "game_state.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
constexpr int pairs[][3] = {{0,1,10}, {3,4,11}, {5,6,12}, {7,8,13}, {10,2,14}, {13,9,15}, {14,11,16}, {12,15,17}, {16,17,18}};
constexpr int triples[][4] = {{0,1,2,9}, {3,4,5,10}, {6,7,8,11}, {9,10,11,12}};
}
Tournament::Tournament(int format, std::span<const Entrant> participants) : format_(format) {
    if ((format != TournamentFormat::pairs && format != TournamentFormat::triples) || participants.size() != std::size_t(format == TournamentFormat::pairs ? TournamentLimits::pair_entrants : TournamentLimits::triple_entrants))
        throw std::invalid_argument("Invalid tournament participants");
    std::copy(participants.begin(), participants.end(), entries_.begin());
    for (const auto& participant : participants)
        if (participant.character < 1 || participant.character > GameLimits::characters || participant.player_number < 0 || participant.player_number > 10)
            throw std::invalid_argument("Invalid tournament entrant");
}
bool Tournament::finished() const { return step_ >= (format_ == TournamentFormat::pairs ? TournamentLimits::pair_matches : TournamentLimits::triple_matches); }
Fixture Tournament::fixture() const {
    if (finished()) throw std::logic_error("Tournament is complete");
    if (format_ == TournamentFormat::pairs) return {{pairs[step_][0], pairs[step_][1], -1}, 2, pairs[step_][2]};
    return {{triples[step_][0], triples[step_][1], triples[step_][2]}, 3, triples[step_][3]};
}
const Entrant& Tournament::entrant(int index) const { return entries_.at(index); }
TournamentRoute Tournament::route(int index) const { return routes_.at(index); }
int Tournament::automatic_winner(std::span<const Entrant> participants, const std::function<int(int)>& draw) {
    if (participants.size() != 2 && participants.size() != 3) throw std::invalid_argument("Invalid tournament match");
    for (int i = 0; i < int(participants.size()); ++i) if (participants[i].character == FighterType::regenerating_fighter) return i;
    return draw(int(participants.size()));
}
int Tournament::place(std::span<Entrant> participants, Entrant player, const std::function<int(int)>& draw) {
    if (participants.empty() || player.character < 1 || std::none_of(participants.begin(), participants.end(), [](const Entrant& e) { return e.character == 0; }))
        throw std::invalid_argument("Invalid tournament placement");
    int position;
    do { position = draw(int(participants.size())); } while (participants[position].character != 0);
    participants[position] = player;
    return position;
}
void Tournament::fill(std::span<Entrant> participants, int character_count, const std::function<int(int)>& draw) {
    if ((participants.size() != TournamentLimits::triple_entrants && participants.size() != TournamentLimits::pair_entrants) || (character_count != GameLimits::selectable_characters && character_count != GameLimits::characters))
        throw std::invalid_argument("Invalid tournament setup");
    for (auto& entry : participants) if (!entry.character) {
        int character;
        do { character = draw(character_count) + 1; }
        while (std::any_of(participants.begin(), participants.end(), [character](const Entrant& other) { return other.character == character; }));
        entry = {character, 0};
    }
}
int Tournament::decide(std::span<const Combatant> fighters, int participants, const std::function<int(int)>& draw) {
    if ((participants != 2 && participants != 3) || fighters.size() < (participants == 2 ? 4u : 3u))
        throw std::invalid_argument("Invalid tournament battle");
    const auto slot = [participants](int n) { return participants == 2 && n == 1 ? 3 : n; };
    for (int n = 0; n < participants; ++n) {
        const auto& fighter = fighters[slot(n)];
        if (fighter.active && fighter.state / 10 != FighterStateGroup::angel) return n;
    }
    int best = -1, count = 0;
    std::array<int, 3> candidates{};
    for (int n = 0; n < participants; ++n) {
        const int raw = fighters[slot(n)].score;
        int score = raw < 1 ? 0 : int(std::log10(double(raw)) * 10.0 / std::log10(2.0));
        if (raw > 0 && score < 30) score = 1;
        if (score > best) { best = score; count = 0; }
        if (score == best) candidates[count++] = n;
    }
    return candidates[count == 1 ? 0 : draw(count)];
}
void Tournament::advance(int winner) {
    const auto current = fixture();
    if (winner < 0 || winner >= current.count) throw std::invalid_argument("Invalid winner");
    for (int i = 0; i < current.count; ++i) routes_[current.contestants[i]] = i == winner ? TournamentRoute::advanced : TournamentRoute::eliminated;
    entries_[current.destination] = entries_[current.contestants[winner]];
    ++step_;
}
int Tournament::champion() const { return finished() ? (format_ == TournamentFormat::pairs ? TournamentLimits::pair_nodes - 1 : TournamentLimits::triple_nodes - 1) : -1; }
