// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "field.hpp"
#include <algorithm>
#include <cstdlib>
#include <numeric>

namespace {
enum class HitKind { strike, extra, body, weapon, projectile, blast, item_fall, fighter_marker, item_marker };
struct HitEntry { ImpactRegion zone; HitKind kind; int index; };
const std::array<int, GameLimits::battle_sounds + 1> battle_sounds = [] {
    std::array<int, GameLimits::battle_sounds + 1> samples{};
    std::iota(samples.begin(), samples.end(), 0);
    return samples;
}();
}

bool Field::thing_in_slot(int slot) const {
    for (const auto& item : ground_items) if (item.active && item.slot == slot) return true;
    for (const auto& projectile : projectiles) if (projectile.active && projectile.slot == slot) return true;
    for (const auto& effect : hit_effects) if (effect.slot == slot) return true;
    return false;
}

bool Field::slot_used(int slot) const {
    for (const auto& item : ground_items) if (item.active && item.slot == slot) return true;
    for (const auto& projectile : projectiles) if (projectile.active && projectile.slot == slot) return true;
    for (const auto& blast : blasts) if (blast.active && blast.slot == slot) return true;
    for (const auto& effect : hit_effects) if (effect.slot == slot) return true;
    return false;
}

int Field::free_slot(int first) const {
    for (int slot = first; slot < 30; ++slot) if (!slot_used(slot)) return slot;
    return 30;
}

void Field::clear() {
    fighters.clear(); controllers.clear(); projectiles.clear(); ground_items.clear(); blasts.clear(); hit_effects.clear();
    plays.clear(); round = {}; tick = 0; camera = 0; power_phase = false;
}

void Field::press(Action input) {
    const auto draw = [this](int limit) { return random.draw(limit); };
    const auto& boundaries = tables_.bounds;
    const auto& item_types = tables_.item_types;
    struct AppearanceView { const std::array<SpecialMove, GameLimits::special_moves>& specials; };
    struct Appearances { const FieldTables& t; AppearanceView operator[](std::size_t i) const { return {t.specials[i]}; } } appearance{tables_};
                for (std::size_t i = 0; i < fighters.size(); ++i) {
                    if (!round.ongoing() || !fighters[i].active || fighters[i].death_pose > 0) continue;
                    if (controllers[i] < 0) {
                        if (input != Action::none)
                            Opponent::process_event(fighters, i, difficulty, draw, projectiles, boundaries[stage + 1].top, boundaries[stage + 1].bottom);
                        continue;
                    }
                    if (input != Action::none) {
                        const int control = controllers[i];
                        fighters[i].steer(display.held(control, Action::left), display.held(control, Action::right), display.held(control, Action::up), display.held(control, Action::down));
                    }
                    if (controllers[i] != display.player()) continue;
                    auto& f = fighters[i];
                    const char key = input == Action::left ? InputKey::left : input == Action::right ? InputKey::right : input == Action::up ? InputKey::up : input == Action::down ? InputKey::down : input == Action::accept ? InputKey::attack : input == Action::jump ? InputKey::jump : 0;
                    if (key) f.remember(key);
                    if (input == Action::left || input == Action::right || input == Action::up || input == Action::down) {
                        const int control = controllers[i];
                        f.steer(input == Action::left || display.held(control, Action::left),
                                input == Action::right || display.held(control, Action::right),
                                input == Action::up || display.held(control, Action::up),
                                input == Action::down || display.held(control, Action::down));
                    }
                    if (input == Action::accept && Grapple::strike(fighters, int(i))) continue;
                    if (input == Action::jump && Grapple::throw_clinched(fighters, int(i))) { display.sound(battle_sounds[BattleSound::attack]); continue; }
                    if ((input == Action::accept || input == Action::jump) && Grapple::throw_held(fighters, int(i))) { display.sound(battle_sounds[BattleSound::attack]); continue; }
                    if (f.carried >= 0 && f.carried < int(ground_items.size()) && f.state / 10 == FighterStateGroup::holding && f.action != FighterAction::landing) {
                        auto& item = ground_items[f.carried];
                        const int category = item_types[item.type].category;
                        if (input == Action::accept && category == ItemCategory::melee && f.action != FighterAction::bow_release) { f.state = FighterState::use_weapon; f.action = item.type == ItemType::bow && f.archetype == FighterType::bow_user ? FighterAction::bow_use : FighterAction::item_windup; f.velocity_x /= 3; }
                        else if (input == Action::accept && item.type == ItemType::drink && category == ItemCategory::ranged) { f.state = FighterState::consume_item; f.action = FighterAction::drink_first; f.velocity_x /= 3; f.remember(InputKey::interrupted); }
                        else if (((input == Action::jump || input == Action::accept) && category == ItemCategory::ranged) || (input == Action::jump && category == ItemCategory::melee)) {
                            f.state = FighterState::throw_item; f.action = FighterAction::item_windup; f.velocity_x /= 3;
                        } else if ((input == Action::jump || input == Action::accept) && category == ItemCategory::throwable) {
                            item.owner = ItemOwnership::unowned; item.last_owner = int(i); item.settled = false; item.facing = f.facing;
                            item.x += f.facing * 12; item.vx = f.facing * 8; item.vz = -4; item.z = -16;
                            f.carried = ItemSlot::none; f.state = FighterState::recovery; f.action = FighterAction::throwing; f.velocity_x /= 3;
                        }
                    } else {
                        if (input == Action::accept && f.can_strike()) f.request_strike(draw(2) != 0);
                        if (input == Action::jump) f.request_jump();
                    }
                }
}

bool Field::step() {
    const auto draw = [this](int limit) { return random.draw(limit); };
    const auto& boundaries = tables_.bounds;
    const auto& item_types = tables_.item_types;
    const auto& ranges = tables_.ranges;
    const auto& special_profiles = tables_.specials;
    struct AppearanceView { const std::array<SpecialMove, GameLimits::special_moves>& specials; };
    struct Appearances { const FieldTables& t; AppearanceView operator[](std::size_t i) const { return {t.specials[i]}; } } appearance{tables_};
        const auto create_pending_item = [&](std::size_t fighter_index) {
            auto& fighter = fighters[fighter_index];
            if (!fighter.item_pending) return;
            const int slot = free_slot(8);
            if (slot < 30) {
                const auto free_item = std::find_if(ground_items.begin(), ground_items.end(), [](const auto& item) { return !item.active; });
                const int index = free_item == ground_items.end() ? int(ground_items.size()) : int(free_item - ground_items.begin());
                if (free_item == ground_items.end()) ground_items.emplace_back();
                auto& created = ground_items[index];
                created = GroundItem{};
                created.slot = slot;
                created.type = *fighter.item_pending; created.owner = created.last_owner = int(fighter_index); created.durability = *fighter.item_pending == ItemType::summoned_sword ? 5 : 1;
                created.x = fighter.x; created.y = fighter.y + 1; created.z = fighter.animation_step - 11; created.facing = fighter.facing;
                fighter.carried = index;
            } else { fighter.state = FighterState::idle; fighter.action = FighterAction::idle; }
            fighter.item_pending.reset();
        };
        const auto show_hit = [&](const Combatant& target, const ImpactRegion& impact, int facing, int original_z, const std::array<int, 2>& offsets) {
            const int effect = impact.damage / ImpactCode::scale;
            if (effect == ImpactCode::normal_kind || effect == ImpactCode::explosion_kind || effect == ImpactCode::piercing_kind)
            {
                const int slot = free_slot(24);
                if (slot < 30) hit_effects.push_back({target.x - facing * 7 + offsets[0], target.y + offsets[1], original_z - 12, facing, 1, 0, slot});
            }
        };
                ++tick;
                if (tick & 1) power_phase = !power_phase;
                std::array<int, GameLimits::battle_sounds + 1> sounds{};
                const auto emit = [&](int sample, int x) {
                    int volume = 2;
                    bool listener = false;
                    for (std::size_t p = 0; p < fighters.size(); ++p) if (fighters[p].active && controllers[p] >= 0 && fighters[p].state / 10 != FighterStateGroup::angel) {
                        listener = true;
                        volume = std::max(volume, 25 - std::abs(fighters[p].x - x) / 8);
                    }
                    if (!listener) volume = std::max(volume, 25 - std::abs(160 - x) / 8);
                    sounds[sample] = std::min(36, sounds[sample] + volume);
                };
                for (auto& fighter : fighters) fighter.priority = AttackPriority::none;
                if (round.ongoing()) {
                    std::size_t cursor = 0;
                    while (cursor < fighters.size()) {
                        auto selected = cursor;
                        while (selected < fighters.size() && controllers[selected] >= 0) ++selected;
                        if (selected == fighters.size()) break;
                        ++cursor;
                        for (std::size_t i = 0; i < fighters.size(); ++i) {
                            auto& f = fighters[i];
                            if (!f.active || f.death_pose > 0) continue;
                            const int control = controllers[i];
                            if (i == selected) {
                                Opponent::act(fighters, i, difficulty, draw, ground_items, item_types, appearance[f.archetype - 1].specials, ranges, projectiles, boundaries[stage + 1].top, boundaries[stage + 1].bottom, special_profiles, true, [&] {
                                    create_pending_item(i);
                                    return std::span<GroundItem>(ground_items);
                                });
                            }
                            if (control < 0) {
                                if (i != selected)
                                    Opponent::process_event(fighters, i, difficulty, draw, projectiles, boundaries[stage + 1].top, boundaries[stage + 1].bottom);
                            } else f.steer(display.held(control, Action::left), display.held(control, Action::right), display.held(control, Action::up), display.held(control, Action::down));
                        }
                    }
                    for (std::size_t i = 0; i < fighters.size(); ++i)
                        if (fighters[i].active && fighters[i].death_pose == 0 && controllers[i] >= 0)
                            fighters[i].steer(display.held(controllers[i], Action::left), display.held(controllers[i], Action::right), display.held(controllers[i], Action::up), display.held(controllers[i], Action::down));
                }
                std::vector<HitEntry> hits;
                const auto add_hit = [&](const ImpactRegion& zone, HitKind kind, int index) {
                    if (hits.size() >= Field::hit_slots) return;
                    int belong = index;
                    if (kind == HitKind::projectile) belong = projectiles[index].slot;
                    else if (kind == HitKind::item_fall || kind == HitKind::item_marker) belong = ground_items[index].slot;
                    else if (kind == HitKind::blast) belong = blasts[index].owner;
                    hit_belong_[hits.size()] = belong;
                    if (zone.damage == 0) return;
                    hits.push_back({zone, kind, index});
                };
                std::vector<Projectile> pending_effects;
                const auto claim_slot = [&](int first) {
                    for (int slot = first; slot < 30; ++slot) {
                        const bool pending = std::any_of(pending_effects.begin(), pending_effects.end(), [&](const auto& effect) { return effect.slot == slot; });
                        if (!slot_used(slot) && !pending) return slot;
                    }
                    return 30;
                };
                std::vector<int> deferred_drops;
                std::vector<int> wall_dropped;
                const auto drop_projectile = [&](const Projectile& p, int vx, int vz) {
                    GroundItem dropped; dropped.slot = p.slot; dropped.type = p.type; dropped.x = p.x; dropped.y = p.y; dropped.z = p.z;
                    dropped.facing = p.facing; dropped.frame = 3; dropped.last_owner = p.owner; dropped.durability = p.durability;
                    dropped.vx = vx; dropped.vy = p.vy; dropped.vz = vz;
                    const auto slot = std::find_if(ground_items.begin(), ground_items.end(), [](const auto& item) { return !item.active; });
                    int index;
                    if (slot == ground_items.end()) { ground_items.push_back(dropped); index = int(ground_items.size() - 1); }
                    else { index = int(slot - ground_items.begin()); *slot = dropped; }
                    deferred_drops.push_back(index);
                    return index;
                };
                const auto launch_for = [&](std::size_t i) {
                    auto& f = fighters[i];
                    if (f.launched_projectile) {
                        const int type = *f.launched_projectile;
                        if (type >= 0 && type < int(item_types.size())) {
                            std::array<int, 3> directions{};
                            const bool burst = f.burst_launch;
                            for (int n = 0; n < f.projectile_count; ++n) {
                                Projectile p; p.slot = free_slot(8); p.owner = int(i); p.type = type; p.x = f.x + f.facing * (burst ? 4 : 7);
                                p.y = f.y + 1; p.z = -8; p.facing = f.facing;
                                if (type == ItemType::sword_projectile) p.lifetime = 5;
                                if (burst) {
                                    p.vy = Projectile::burst_direction(std::span(directions).first(n), draw);
                                    directions[n] = p.vy; p.lifetime = 15;
                                }
                                p.frame = !burst && (type == ItemType::sword_projectile || type == ItemType::arrow) ? 3 : 1 + draw(2);
                                if (p.slot < 30) projectiles.push_back(p);
                            }
                            emit(burst || type == ItemType::sword_projectile || type == ItemType::arrow ? BattleSound::attack : BattleSound::projectile, f.x);
                        }
                        f.launched_projectile.reset(); f.projectile_count = 0; f.burst_launch = false;
                    }
                };
                const auto process_fighter = [&](std::size_t i) {
                    auto& f = fighters[i];
                    std::optional<ImpactRegion> strike, extra, body;
                    if (!f.active || f.death_pose > 0) return;
                    if (f.state / 10 == FighterStateGroup::angel) {
                        f.advance((tick & 1) == 0, power_phase, round.ongoing(), difficulty);
                    f.move(camera, boundaries[stage + 1].top, boundaries[stage + 1].bottom, stage == 3, round.winner());
                        return;
                    }
                    if (f.state >= FighterState::item_idle_base && f.state < FighterState::item_throw_base && f.carried < 0 && !f.item_pending) { f.state = FighterState::idle; f.action = FighterAction::idle; }
                    if (round.ongoing()) {
                        const bool holding_melee = f.carried >= 0 && f.carried < int(ground_items.size()) && item_types[ground_items[f.carried].type].category == ItemCategory::melee;
                        f.try_sword(holding_melee);
                    }
                    f.advance((tick & 1) == 0, power_phase, round.ongoing(), difficulty);
                    if (round.ongoing() && (f.state / 10 <= FighterStateGroup::kicking || f.state / 10 == FighterStateGroup::running || f.state / 20 == FighterStateGroup::clinching ||
                                            f.state / 10 == FighterStateGroup::lifting || f.state / 50 == FighterStateGroup::item)) {
                        const bool holding_melee = f.carried >= 0 && f.carried < int(ground_items.size()) && item_types[ground_items[f.carried].type].category == ItemCategory::melee;
                        f.try_special(appearance[f.archetype - 1].specials, holding_melee);
                    }
                    const bool forward_roll = (f.action == FighterAction::forward_roll || f.action == FighterAction::backward_roll) && f.velocity_x * f.facing > 3;
                    f.priority = f.action == FighterAction::roll || forward_roll ? AttackPriority::evasive : AttackPriority::none;
                    const int old_state = f.state;
                    const int old_z = f.z;
                    if (old_state == FighterState::punch_active || old_state == FighterState::kick_active) emit(BattleSound::attack, f.x);
                    if (old_state == special_state(SpecialState::make_sword_2, 0)) emit(BattleSound::sword_creation, f.x);
                    if (old_state / SpecialMoveId::state_stride == SpecialState::heal_self && old_state % 5 == 1) emit(BattleSound::healing, f.x);
                    if (old_state / 20 == FighterStateGroup::consuming && old_state % 2 == 0) emit(BattleSound::drinking, f.x);
                    strike = f.special_frame((tick & 1) != 0);
                    if (old_state / SpecialMoveId::state_stride == SpecialState::ball_kat && f.state == FighterState::idle && f.computer) f.ranged_cooldown = difficulty + 4;
                    extra = f.secondary_impact;
                    f.secondary_impact.reset();
                    create_pending_item(i);
                    if (f.blast_pending != BlastPattern::none) {
                        const auto spawn = [&](int x, int y, int offset) {
                            const int slot = free_slot(8); if (slot < 30) { Blast blast; blast.slot = slot; blast.owner = int(i); blast.x = x; blast.y = y; blast.offset = offset; blasts.push_back(blast); }
                        };
                        if (f.blast_pending == BlastPattern::forward) spawn(f.x + f.facing * 37, f.y, 0);
                        else if (f.blast_pending == BlastPattern::centered) spawn(f.x, f.y + 1, 0);
                        else {
                            constexpr int pattern[] = {-1, 0, 1, 0, -1};
                            for (int n = 0; n < 4; ++n) {
                                spawn(f.x + (n % 2 * 2 - 1) * 17, f.y + (n / 2 * 2 - 1) * 12 + 1, 3);
                                spawn(f.x + pattern[n] * 30, f.y + pattern[n + 1] * 20 + 1, 3);
                            }
                        }
                        f.blast_pending = BlastPattern::none;
                    }
                    if (f.teleport_pending) {
                        f.teleport(fighters, draw, f.state / SpecialMoveId::state_stride == SpecialState::shun_kan_2);
                        f.teleport_pending = false;
                        emit(BattleSound::teleport, f.x);
                    }
                    const auto launch_projectiles = [&] { launch_for(i); };
                    launch_projectiles();
                    if (!strike && (f.state / 10 == FighterStateGroup::punching || f.state / 10 == FighterStateGroup::kicking)) {
                        const bool variation = (f.state == FighterState::punch_start || f.state == FighterState::kick_start) && draw(2) != 0;
                        strike = f.strike_frame(variation, f.z == 0 && f.dash_ready(round.ongoing()));
                    }
                    if (f.carried >= 0 && f.carried < int(ground_items.size())) {
                        auto& item = ground_items[f.carried];
                        if (f.state == FighterState::release_item) {
                            if (item.type == ItemType::bomb) {
                                item.owner = ItemOwnership::unowned; item.last_owner = ItemOwnership::detached; item.settled = false;
                                item.x = f.x + f.facing * 10; item.z = -16;
                                item.vx = f.facing * 8; item.vz = -6; item.frame = 3;
                            } else {
                                Projectile p; p.slot = item.slot; p.owner = int(i); p.type = item.type; p.x = item.x + f.facing * 24;
                                p.y = item.y; p.z = item.z - 8; p.facing = f.facing; p.frame = 3; p.durability = item.durability;
                                projectiles.push_back(p); item.active = false; item.owner = ItemOwnership::unowned;
                            }
                            f.carried = ItemSlot::none;
                            f.state = FighterState::throw_finish - 1; f.action = FighterAction::throwing;
                        } else if (old_state / 50 == FighterStateGroup::item) {
                            item.animate_holder(f, int(i), item_types[item.type], appearance[f.archetype - 1].specials[0].cost, round.winner() <= 0);
                            if (f.state / 10 == FighterStateGroup::throwing_item) ++f.state;
                        }
                    }
                    if (old_state / 10 == FighterStateGroup::lifting) Grapple::carry(fighters, int(i));
                    if (old_state / 20 == FighterStateGroup::clinching) Grapple::hold(fighters, int(i));
                    launch_projectiles();
                    const bool landing = f.state >= FighterState::falling && f.state < FighterState::downed_last && (f.action == FighterAction::landing || f.z + f.velocity_z > 0);
                    const int landing_variant = landing ? draw(2) : 0;
                    const int revival_velocity = f.state == FighterState::downed_last && f.health > 0 && f.health == f.max_health && f.archetype == FighterType::cloning_fighter ? draw(21) - 10 : 0;
                    if (f.state == FighterState::run_start || f.state == FighterState::run_step_sound) emit(BattleSound::footstep, f.x);
                    f.dispatch_state = old_state;
                    f.move(camera, boundaries[stage + 1].top, boundaries[stage + 1].bottom, stage == 3, round.winner(), landing_variant, revival_velocity);
                    if (landing) emit(BattleSound::landing, f.x);
                    if (old_state == FighterState::thrown && f.state == FighterState::thrown_bounce) emit(BattleSound::bounce, f.x);
                    if (f.body_impact) { body = f.body_impact; f.body_impact.reset(); }
                    launch_projectiles();
                    if (old_z < 0 && f.z == 0 && old_state < FighterState::punch_start) emit(BattleSound::footstep, f.x);
                                    if (strike) add_hit(*strike, HitKind::strike, int(i));
                    if (extra) add_hit(*extra, HitKind::extra, int(i));
                    if (body) add_hit(*body, HitKind::body, int(i));
                    if (f.state > FighterState::downed_first && f.state < FighterState::downed_end && old_state >= FighterState::downed_first && f.health > 0)
                        add_hit(ImpactRegion{f.x, f.y, f.z - 12, 8, 12, f.facing, f.facing * 5, -7, -2, 25}, HitKind::fighter_marker, int(i));
                    if (f.state == FighterState::staggered && f.balance < CombatTuning::balance_recovery)
                        add_hit(ImpactRegion{f.x, f.y, f.z - 12, 12, 12, f.facing, f.facing * 5, -7, -1, 25}, HitKind::fighter_marker, int(i));
                };
                for (int n = 0; n < Slots::objects_end; ++n) {
                    if (n < int(fighters.size())) { process_fighter(std::size_t(n)); continue; }
                    int projectile_index = -1, item_index = -1, blast_index = -1, effect_index = -1;
                    for (std::size_t k = 0; k < projectiles.size(); ++k) if (projectiles[k].active && projectiles[k].slot == n) projectile_index = int(k);
                    for (std::size_t k = 0; k < ground_items.size(); ++k) if (ground_items[k].active && ground_items[k].slot == n) item_index = int(k);
                    for (std::size_t k = 0; k < blasts.size(); ++k) if (blasts[k].active && blasts[k].slot == n) blast_index = int(k);
                    for (std::size_t k = 0; k < hit_effects.size(); ++k) if (hit_effects[k].slot == n) effect_index = int(k);
                    if (projectile_index >= 0) {
                        auto& p = projectiles[projectile_index];
                        if (p.exploding) {
                            if (++p.frame >= 4 || p.type == ItemType::short_effect) p.active = false;
                            p.x += p.vx; p.y = std::clamp(p.y + p.vy, boundaries[stage + 1].top - 3, boundaries[stage + 1].bottom + 2);
                            if (p.x <= camera - 190 || p.x >= camera + 510) p.active = false;
                        }
                        else {
                            add_hit(p.advance(item_types[p.type], fighters, camera, boundaries[stage + 1].top, boundaries[stage + 1].bottom, draw), HitKind::projectile, projectile_index);
                            if (projectiles[projectile_index].wall_bounce) {
                                auto& bounced = projectiles[projectile_index];
                                wall_dropped.push_back(projectile_index);
                                drop_projectile(bounced, bounced.x <= camera - 190 ? 10 : -10, -17);
                            }
                        }
                    } else if (item_index >= 0) {
                        auto& item = ground_items[item_index];
                        if (item.owner >= 0) {
                            auto& holder = fighters[item.owner];
                            const bool carrying = holder.carried == item_index || (holder.carried == ItemSlot::knocked && item.slot == ItemSlot::knocked_slot);
                            const auto swing = item.follow(holder, item_types[item.type], carrying && round.winner() <= 0, draw);
                            if (item.active) {
                                item.x += item.vx; item.y = std::clamp(item.y + item.vy, boundaries[stage + 1].top - 3, boundaries[stage + 1].bottom + 2);
                                if (item.x <= camera - 190 || item.x >= camera + 510) {
                                    if (item.durability > 8) { item.owner = ItemOwnership::unowned; item.settled = false; item.animation = 1; item.frame = 3; item.vz = -17; item.vx = item.x <= camera - 190 ? 10 : -10; }
                                    else { item.active = false; item.vy = 0; }
                                }
                            }
                            if (carrying && item.active) launch_for(std::size_t(item.owner));
                            if (swing) add_hit(*swing, HitKind::weapon, item.owner);
                        } else {
                            item.advance(item_types[item.type], camera, boundaries[stage + 1].top, boundaries[stage + 1].bottom, draw);
                            if (item.detonate) {
                                Blast blast; blast.slot = item.slot; blast.owner = item.last_owner; blast.x = item.x; blast.y = item.y; blast.z = item.z;
                                blasts.push_back(blast); item.detonate = false; item.active = false;
                            }
                            if (item.rest_marker) add_hit(ImpactRegion{item.x, item.y, item.z, 5, 7, item.facing, item.vx, -7, -3, 25}, HitKind::item_marker, item_index);
                            if (item.falling_impact && item.last_owner != ItemOwnership::detached) add_hit(*item.falling_impact, HitKind::item_fall, item_index);
                        }
                    } else if (blast_index >= 0) {
                        auto& blast = blasts[blast_index];
                        if (blast.frame == 1) { emit(BattleSound::explosion, blast.x); emit(BattleSound::explosion, blast.x); }
                        if (const auto zone = blast.advance(item_types[ItemType::bomb].damage)) add_hit(*zone, HitKind::blast, blast_index);
                    } else if (effect_index >= 0) {
                        auto& effect = hit_effects[effect_index];
                        ++effect.age;
                        effect.frame = effect.age == 2 ? 2 : 1;
                        effect.y = std::clamp(effect.y, boundaries[stage + 1].top - 3, boundaries[stage + 1].bottom + 2);
                        if (effect.age >= 3 || effect.x <= camera - 190 || effect.x >= camera + 510) effect.slot = -1;
                    } else if (n >= Slots::first_spawn && draw(300) == 0 && spawn_items) {
                        GroundItem created = GroundItem::create(draw);
                        created.slot = n;
                        const auto spare = std::find_if(ground_items.begin(), ground_items.end(), [](const auto& item) { return !item.active; });
                        if (spare == ground_items.end()) ground_items.push_back(created); else *spare = created;
                    }
                }
                std::erase_if(hit_effects, [](const auto& effect) { return effect.slot < 0; });
                for (const auto& item : ground_items) if (item.slot >= 0 && !item.active) slot_memory_[item.slot] = {item.type, item.y, item.owner >= 0 ? item.owner : item.last_owner >= 0 ? item.last_owner : 8};
                for (const auto& p : projectiles) if (p.slot >= 0 && !p.active) slot_memory_[p.slot] = {p.type, p.y, p.owner};
                for (const auto& item : ground_items) if (item.slot >= 0 && item.active) slot_memory_[item.slot] = {item.type, item.y, item.owner >= 0 ? item.owner : item.last_owner >= 0 ? item.last_owner : 8};
                for (const auto& p : projectiles) if (p.slot >= 0 && p.active) slot_memory_[p.slot] = {p.type, p.y, p.owner};
                for (const auto& blast : blasts) if (blast.slot >= 0 && blast.active) slot_memory_[blast.slot] = {ItemType::bomb, blast.y, blast.owner};
                for (const auto& effect : hit_effects) if (effect.slot >= 0) slot_memory_[effect.slot] = {ItemType::hit_effect, effect.y, slot_memory_[effect.slot].owner};
                const auto owner_y = [&](const HitEntry& entry) {
                    if (entry.kind == HitKind::projectile) return projectiles[entry.index].y;
                    if (entry.kind == HitKind::item_fall || entry.kind == HitKind::item_marker) return ground_items[entry.index].y;
                    if (entry.kind == HitKind::blast) return fighters[blasts[entry.index].owner].y;
                    return fighters[entry.index].y;
                };
                const auto fighter_of = [&](const HitEntry& entry) {
                    if (entry.kind == HitKind::blast) return blasts[entry.index].owner >= 0 && blasts[entry.index].owner < int(fighters.size()) ? blasts[entry.index].owner : -1;
                    if (entry.kind == HitKind::projectile || entry.kind == HitKind::item_fall || entry.kind == HitKind::item_marker) return -1;
                    return entry.index;
                };
                // An object that bounces belongs to the fighter that knocked it, or to nobody when another object did.
                constexpr int nobody = -2;
                std::vector<int> knocked_by(projectiles.size(), -1);
                std::vector<bool> knocked_loose(projectiles.size(), false);
                std::vector<int> knocked_item(projectiles.size(), -1);
                const auto group_of = [&](std::size_t index) {
                    if (knocked_by[index] == nobody) return -1;
                    return fighters[knocked_by[index] >= 0 ? knocked_by[index] : projectiles[index].owner].team;
                };
                for (std::size_t ex_slot = 0; ex_slot < hits.size(); ++ex_slot) {
                    const auto ex = hits[ex_slot];
                    if (ex.zone.damage <= 0) continue;
                    const bool ex_is_projectile = ex.kind == HitKind::projectile;
                    const bool ex_is_item = ex.kind == HitKind::item_fall;
                    const int ty = owner_y(ex);
                    bool handled = false;
                    int bounced_index = -1;
                    for (std::size_t slot = 0; slot < Field::hit_slots; ++slot) {
                        const int tmp2 = hit_belong_[slot];
                        const bool pending_thing = std::any_of(pending_effects.begin(), pending_effects.end(), [&](const auto& effect) { return effect.slot == tmp2; });
                        const bool arrows_only = ex_is_projectile && projectiles[ex.index].type == ItemType::arrow && (thing_in_slot(tmp2) || pending_thing) && draw(3) != 2;
                        if (arrows_only && slot_memory_[tmp2].type != ItemType::arrow) continue;
                        const bool listed = slot < hits.size();
                        const int other_y = listed ? owner_y(hits[slot]) : (tmp2 >= 8 ? slot_memory_[tmp2].y : fighters[tmp2].y);
                        if (std::abs(other_y - ty) >= 8) continue;
                        if (slot == ex_slot) continue;
                        if (tmp2 >= 8 && slot_memory_[tmp2].type == ItemType::plus && draw(3) == 0) continue;
                        if (!ex_is_projectile && !ex_is_item) continue;
                        if (!listed) continue;
                        const auto& other = hits[slot];
                        if (other.zone.damage <= 0) continue;
                        if (ex_is_item) {
                            auto& item = ground_items[ex.index];
                            if (!(item.active && item.owner < 0 && (item.type == ItemType::stone || item.type == ItemType::box) && item.z + item.vz < 0 && item_types[item.type].damage > 0)) continue;
                            const int hitter = fighter_of(other);
                            if (hitter < 0) continue;
                            const auto& definition = item_types[item.type];
                            const auto reaction = item.collide(other.zone, fighters[hitter], definition, fighters, hitter, draw);
                            if (reaction == ItemCollision::bounced) emit(item.type == ItemType::sword || item.type == ItemType::seven || item.type == ItemType::plus || item.type == ItemType::claw ? BattleSound::weapon_impact : BattleSound::landing, item.x);
                            if (reaction == ItemCollision::destroyed) {
                                const int replacement = definition.effect == ImpactCode::freeze_kind ? ItemType::freeze_effect : definition.images[2];
                                const int free = free_slot(8);
                                if (replacement >= 0 && replacement < int(item_types.size()) && free < 30) {
                                    Projectile explosion; explosion.slot = free; explosion.owner = other.index; explosion.type = replacement; explosion.x = item.x;
                                    explosion.y = item.y; explosion.z = item.z; explosion.frame = 0; explosion.exploding = true;
                                    projectiles.push_back(explosion);
                                    emit(replacement == ItemType::short_effect ? BattleSound::heavy_impact : BattleSound::explosion, item.x);
                                }
                            }
                        } else {
                            auto& p = projectiles[ex.index];
                            if (bounced_index < 0 && !(p.active && !p.exploding)) continue;
                            const int registered_kind = ex.zone.damage / ImpactCode::scale;
                            const auto convert = [&](ProjectileCollision outcome, int effect_owner) {
                                const int replacement = registered_kind == ImpactCode::freeze_kind ? ItemType::freeze_effect : item_types[p.type].images[2];
                                if (replacement >= 0 && replacement < int(item_types.size())) {
                                    if (outcome == ProjectileCollision::impact) { p.type = replacement; p.frame = 1; p.exploding = true; p.vx = p.vy = 0; }
                                    else {
                                        const int free = claim_slot(8);
                                        Projectile effect; effect.slot = free; effect.owner = free < 30 && slot_memory_[free].owner >= 0 && slot_memory_[free].owner < int(fighters.size()) ? slot_memory_[free].owner : effect_owner; effect.type = replacement; effect.x = p.x; effect.y = p.y; effect.z = p.z; effect.exploding = true;
                                        if (free < 30) { pending_effects.push_back(effect); slot_memory_[free] = {effect.type, effect.y, effect.owner}; }
                                    }
                                    emit(replacement == ItemType::short_effect ? BattleSound::heavy_impact : BattleSound::explosion, p.x);
                                }
                                handled = true;
                            };
                            const auto bounce = [&](int hitter_owner, int hitter_x, int hitter_z, bool hitter_is_fighter, int hitter_slot) {
                                if (std::abs(p.x - hitter_x) >= other.zone.width + 12 || std::abs(p.z - hitter_z) >= other.zone.height + 7) return;
                                const auto& def = item_types[p.type];
                                const bool throwable = def.category == ItemCategory::throwable;
                                if (hitter_is_fighter) {
                                    auto& hitter = fighters[hitter_owner];
                                    if (group_of(ex.index) == hitter.team && !throwable) return;
                                    if (hitter.priority <= AttackPriority::none && hitter.action != FighterAction::punch_hit && hitter.action != FighterAction::punch_hit_alt &&
                                        hitter.action != FighterAction::kick_hit && hitter.action != FighterAction::kick_hit_alt) return;
                                } else {
                                    if (group_of(ex.index) == group_of(std::size_t(hitter_slot))) return;
                                }
                                const int sound = p.type == ItemType::sword || p.type == ItemType::seven || p.type == ItemType::plus || p.type == ItemType::claw || p.type == ItemType::knife
                                    ? BattleSound::weapon_impact : BattleSound::landing;
                                emit(sound, p.x);
                                const int vx = throwable ? other.zone.push_x : draw(7) - 3;
                                const int vz = throwable ? -5 : -7;
                                if (bounced_index < 0) {
                                    bounced_index = drop_projectile(p, vx, vz);
                                    knocked_loose[ex.index] = true;
                                    knocked_item[ex.index] = bounced_index;
                                    p.active = false; p.wall_bounce = false;
                                } else {
                                    ground_items[bounced_index].vx = vx; ground_items[bounced_index].vz = vz;
                                }
                                ground_items[bounced_index].last_owner = hitter_is_fighter ? hitter_owner : ItemOwnership::unowned;
                                knocked_by[ex.index] = hitter_is_fighter ? hitter_owner : nobody;
                                if (hitter_is_fighter) fighters[hitter_owner].energy = std::min(fighters[hitter_owner].max_energy, fighters[hitter_owner].energy + 7);
                            };
                            if (registered_kind < ImpactCode::fire_kind) {
                                if (other.kind == HitKind::projectile) {
                                    const auto& rival = projectiles[other.index];
                                    if (rival.active || knocked_loose[other.index]) bounce(rival.owner, rival.x, rival.z, false, other.index);
                                } else if (fighter_of(other) >= 0) {
                                    bounce(fighter_of(other), fighters[fighter_of(other)].x, fighters[fighter_of(other)].z, true, 0);
                                }
                                continue;
                            }
                            if (p.exploding) continue;
                            if (fighter_of(other) >= 0) {
                                if (arrows_only) continue;
                                const auto outcome = p.intercept(other.zone, fighters[fighter_of(other)], fighter_of(other), fighters, item_types[p.type], true);
                                if (outcome != ProjectileCollision::none) convert(outcome, fighter_of(other));
                            } else if (other.kind == HitKind::projectile) {
                                const Projectile* occupant = &projectiles[other.index];
                                if (!occupant->active && !knocked_loose[other.index])
                                    for (const auto& effect : pending_effects) if (effect.slot == tmp2) occupant = &effect;
                                const auto& rival = *occupant;
                                if (arrows_only && rival.type != ItemType::arrow) continue;
                                if (knocked_loose[other.index] && (p.type == ItemType::reflected_projectile || p.type == ItemType::summoned_sword) &&
                                    rival.type != ItemType::reflected_projectile && rival.type != ItemType::summoned_sword) continue;
                                if (knocked_loose[other.index] && rival.type != ItemType::reflected_projectile && rival.type != ItemType::summoned_sword) {
                                    const int kind = item_types[p.type].damage / ImpactCode::scale + item_types[p.type].effect;
                                    const bool burns = kind == ImpactCode::fire_kind || kind == ImpactCode::explosion_kind || kind == ImpactCode::piercing_kind || kind == ImpactCode::freeze_kind;
                                    if (burns && std::abs(p.x - rival.x) < other.zone.width + 12 && std::abs(p.z - rival.z) < other.zone.height + 7) convert(ProjectileCollision::impact, p.owner);
                                    continue;
                                }
                                const auto outcome = p.clash(rival, fighters, item_types[p.type], !arrows_only, true);
                                if (outcome != ProjectileCollision::none) convert(outcome, p.owner);
                            }
                        }
                    }
                }
                struct ProjectileState { bool consumed = false; int dropped = -1; };
                std::vector<ProjectileState> projectile_state(hits.size());
                for (std::size_t j = 0; j < fighters.size(); ++j) {
                    const bool detecting = round.ongoing() && fighters[j].active && fighters[j].state / 10 != 5 && (fighters[j].state / 10 != 4 || fighters[j].state == FighterState::falling);
                    const bool left = controllers[j] >= 0 ? display.held(controllers[j], Action::left) : display.held(1, Action::right);
                    if (detecting) for (std::size_t h = 0; h < hits.size(); ++h) {
                        const auto& entry = hits[h];
                        if (entry.kind == HitKind::fighter_marker) {
                            if (entry.zone.damage == -1) Grapple::clinch(fighters, int(j), entry.index, left);
                            else Grapple::lift(fighters, int(j), entry.index);
                            continue;
                        }
                        if (entry.kind == HitKind::item_marker) {
                            if (ground_items[entry.index].claim(fighters[j], int(j), entry.index, ground_items, draw)) emit(BattleSound::light_impact, fighters[j].x);
                            continue;
                        }
                        if (entry.kind == HitKind::strike || entry.kind == HitKind::extra || entry.kind == HitKind::body || entry.kind == HitKind::weapon) {
                            const std::size_t i = std::size_t(entry.index);
                            if (entry.zone.damage == ImpactCode::grapple) { Grapple::rush(fighters, int(i), int(j), entry.zone); continue; }
                            const int original_z = fighters[j].z;
                            std::array<int, 2> hit_offsets{};
                            if (fighters[j].receive(entry.zone, fighters[i], draw, 2, ImpactOrigin::fighter, 8, &hit_offsets)) {
                                show_hit(fighters[j], entry.zone, fighters[i].facing, original_z, hit_offsets);
                                if (entry.zone.damage > 0) emit(entry.zone.damage / ImpactCode::scale == ImpactCode::freeze_kind ? BattleSound::freeze : entry.zone.effect % ImpactCode::scale == ImpactCode::knockdown ? BattleSound::heavy_impact : BattleSound::light_impact, fighters[j].x);
                            }
                        } else if (entry.kind == HitKind::blast) {
                            Combatant neutral;
                            auto& blast = blasts[entry.index];
                            const bool owned = blast.owner >= 0 && blast.owner < int(fighters.size());
                            auto& source = owned ? fighters[blast.owner] : neutral;
                            auto& target = fighters[j];
                            const int original_z = target.z;
                            std::array<int, 2> hit_offsets{};
                            if (target.receive(entry.zone, source, draw, 2, owned ? ImpactOrigin::blast : ImpactOrigin::explosion, 8, &hit_offsets)) {
                                show_hit(target, entry.zone, 1, original_z, hit_offsets);
                                emit(BattleSound::heavy_impact, target.x);
                            }
                        } else if (entry.kind == HitKind::projectile && knocked_item[entry.index] >= 0) {
                            auto& p = projectiles[entry.index];
                            auto& dropped = ground_items[knocked_item[entry.index]];
                            const int owner = knocked_by[entry.index];
                            Combatant neutral;
                            auto& source = owner >= 0 ? fighters[owner] : neutral;
                            const auto origin = owner < 0 ? ImpactOrigin::unowned_item : p.type == ItemType::claw ? ImpactOrigin::friendly_item : ImpactOrigin::item;
                            auto& target = fighters[j];
                            if (target.state / 10 == FighterStateGroup::injured && item_types[p.type].motion == ItemMotion::ignore_downed) continue;
                            const int original_z = target.z;
                            std::array<int, 2> hit_offsets{};
                            const auto& hit = entry.zone;
                            const auto react = [&] {
                                dropped.react_to_fighter_hit(item_types[dropped.type], hit.damage, source.action, draw);
                                if (hit.effect % ImpactCode::scale == ImpactCode::recoil) dropped.vx = -hit.push_x;
                            };
                            if (!target.receive(hit, source, draw, 2, origin, 8, &hit_offsets, react)) continue;
                            show_hit(target, hit, p.facing, original_z, hit_offsets);
                            emit(hit.damage / ImpactCode::scale == ImpactCode::freeze_kind ? BattleSound::freeze : hit.effect % ImpactCode::scale == ImpactCode::knockdown ? BattleSound::heavy_impact : BattleSound::light_impact, target.x);
                        } else if (entry.kind == HitKind::projectile) {
                            auto& p = projectiles[entry.index];
                            auto& state = projectile_state[h];
                            const auto& hit = entry.zone;
                            const int registered_kind = hit.damage / ImpactCode::scale;
                            const int impact_type = p.type;
                            const int impact_owner = p.owner;
                            const bool vanished = !p.active && !p.wall_bounce && state.dropped < 0;
                            if (h == 0 || !state.consumed) state.consumed = state.consumed || p.exploding || vanished;
                            auto& target = fighters[j];
                            const int original_z = target.z;
                            std::array<int, 2> hit_offsets{};
                            const auto origin = vanished ? ImpactOrigin::explosion : impact_type == ItemType::claw ? ImpactOrigin::friendly_item : ImpactOrigin::item;
                            const auto react = [&] {
                                if (state.consumed) {
                                    if (state.dropped >= 0) {
                                        auto& dropped = ground_items[state.dropped];
                                        dropped.react_to_fighter_hit(item_types[impact_type], hit.damage, fighters[impact_owner].action, draw);
                                        if (hit.effect % ImpactCode::scale == ImpactCode::recoil) dropped.vx = -hit.push_x;
                                    } else if (p.active && hit.effect % ImpactCode::scale == ImpactCode::recoil) p.vx = -hit.push_x;
                                    return;
                                }
                                if (registered_kind < ImpactCode::fire_kind && item_types[p.type].effect != ImpactCode::piercing_kind &&
                                    p.durability < 500 && (fighters[p.owner].action < FighterAction::weapon_swing_first || fighters[p.owner].action > FighterAction::weapon_swing_last)) {
                                    state.dropped = drop_projectile(p, item_types[p.type].category == ItemCategory::throwable ? p.vx / 2 : draw(7) - 3,
                                                                    item_types[p.type].category == ItemCategory::throwable ? -3 : -7);
                                    if (hit.effect % ImpactCode::scale == ImpactCode::recoil) ground_items[state.dropped].vx = -hit.push_x;
                                    p.active = false; p.wall_bounce = false;
                                    state.consumed = true;
                                    return;
                                }
                                if (registered_kind == ImpactCode::fire_kind || registered_kind == ImpactCode::explosion_kind || registered_kind == ImpactCode::freeze_kind) {
                                    const int replacement = registered_kind == ImpactCode::freeze_kind ? ItemType::freeze_effect : item_types[p.type].images[2];
                                    if (replacement >= 0 && replacement < int(item_types.size())) {
                                        p.type = replacement; p.frame = 1; p.exploding = true; p.vx = p.vy = 0;
                                        emit(replacement == ItemType::short_effect ? BattleSound::heavy_impact : BattleSound::explosion, p.x);
                                    } else p.active = false;
                                    state.consumed = true;
                                }
                                if (hit.effect % ImpactCode::scale == ImpactCode::recoil) p.vx = -hit.push_x;
                            };
                            if ((target.state / 10 == FighterStateGroup::injured && item_types[impact_type].motion == ItemMotion::ignore_downed) || !target.receive(hit, fighters[impact_owner], draw, vanished || impact_type == ItemType::sword_projectile ? 0 : 2, origin, 8, &hit_offsets, react)) continue;
                            show_hit(target, hit, p.facing, original_z, hit_offsets);
                            emit(hit.damage / ImpactCode::scale == ImpactCode::freeze_kind ? BattleSound::freeze : hit.effect % ImpactCode::scale == ImpactCode::knockdown ? BattleSound::heavy_impact : BattleSound::light_impact, target.x);
                        } else if (entry.kind == HitKind::item_fall) {
                            auto& item = ground_items[entry.index];
                            if (!item.falling_impact) continue;
                            Combatant neutral;
                            const bool owned = item.last_owner >= 0 && item.last_owner < int(fighters.size());
                            auto& source = owned ? fighters[item.last_owner] : neutral;
                            const auto origin = !owned ? ImpactOrigin::unowned_item : item.type == ItemType::claw ? ImpactOrigin::friendly_item : ImpactOrigin::item;
                            auto& target = fighters[j];
                            if (target.state / 10 == FighterStateGroup::injured && item_types[item.type].motion == ItemMotion::ignore_downed) continue;
                            const int source_action = source.action;
                            const int original_z = target.z;
                            std::array<int, 2> hit_offsets{};
                            const auto react = [&] {
                                const int damage_kind = item.falling_impact->damage / ImpactCode::scale;
                                if (item.active && (damage_kind == ImpactCode::fire_kind || damage_kind == ImpactCode::explosion_kind || damage_kind == ImpactCode::freeze_kind)) {
                                    const int replacement = damage_kind == ImpactCode::freeze_kind ? ItemType::freeze_effect : item.type >= ItemType::clear_effect && item.type <= ItemType::knife ? item.type : item_types[item.type].images[2];
                                    const int slot = free_slot(8);
                                    if (replacement >= 0 && replacement < int(item_types.size()) && slot < 30) {
                                        Projectile effect; effect.slot = slot; effect.owner = owned ? item.last_owner : 0; effect.type = replacement;
                                        effect.x = item.x; effect.y = item.y; effect.z = item.z; effect.exploding = true;
                                        projectiles.push_back(effect);
                                        emit(replacement == ItemType::short_effect ? BattleSound::heavy_impact : BattleSound::explosion, item.x);
                                    }
                                    item.active = false;
                                } else if (item.active && item.react_to_fighter_hit(item_types[item.type], item.falling_impact->damage, source_action, draw)) emit(BattleSound::heavy_impact, target.x);
                            };
                            if (target.receive(*item.falling_impact, source, draw, 2, origin, 8, &hit_offsets, react))
                                show_hit(target, *item.falling_impact, item.facing, original_z, hit_offsets);
                        }
                    }
                    if (fighters[j].active && fighters[j].state / 20 == 8) Grapple::clinched(fighters, int(j));
                    if (fighters[j].active && fighters[j].state / 20 == FighterStateGroup::lifted) Grapple::lifted(fighters, int(j), draw);
                }
                for (std::size_t h = 0; h < hits.size(); ++h) {
                    if (hits[h].kind != HitKind::projectile) continue;
                    auto& p = projectiles[hits[h].index];
                    if (p.wall_bounce && !p.exploding && std::find(wall_dropped.begin(), wall_dropped.end(), hits[h].index) == wall_dropped.end()) { drop_projectile(p, p.x <= camera - 190 ? 10 : -10, -17); }
                    p.wall_bounce = false;
                }
                projectiles.insert(projectiles.end(), pending_effects.begin(), pending_effects.end());
                for (std::size_t index = 0; index < ground_items.size(); ++index) {
                    auto& item = ground_items[index];
                    if (!item.active || item.owner < 0 || item.owner >= int(fighters.size())) continue;
                    auto& holder = fighters[item.owner];
                    if (holder.carried == int(index) || (holder.carried == ItemSlot::knocked && item.slot == ItemSlot::knocked_slot)) continue;
                    item.owner = ItemOwnership::unowned; item.last_owner = ItemOwnership::detached; item.settled = false;
                    item.frame = 3; item.vz = -7; item.vx = draw(7) - 3;
                    item.x = holder.x; item.y = holder.y + 1;
                }

                for (const auto& p : projectiles) if (p.slot >= 0 && !p.active) slot_memory_[p.slot] = {p.type, p.y, p.owner};
                std::erase_if(projectiles, [](const auto& p) { return !p.active; });
                std::erase_if(blasts, [](const auto& blast) { return !blast.active; });
                for (auto& item : ground_items) if (!item.active && item.slot >= 0) { slot_memory_[item.slot] = {item.type, item.y, item.owner >= 0 ? item.owner : item.last_owner >= 0 ? item.last_owner : 8}; item.slot = -1; }
                for (const auto& p : projectiles) if (p.slot >= 0) slot_memory_[p.slot] = {p.type, p.y, p.owner};
                for (const auto& item : ground_items) if (item.slot >= 0) slot_memory_[item.slot] = {item.type, item.y, item.owner >= 0 ? item.owner : item.last_owner >= 0 ? item.last_owner : 8};
                round.advance(fighters);
                for (auto& fighter : fighters) fighter.coordinated = false;
                for (std::size_t target = 0; target < fighters.size(); ++target) {
                    int best = -1;
                    for (std::size_t i = 0; i < fighters.size(); ++i) {
                        const auto& fighter = fighters[i];
                        if (fighter.active && fighter.computer && fighter.opponent_target == int(target) &&
                            (best < 0 || fighter.health >= fighters[best].health)) best = int(i);
                    }
                    if (best >= 0) fighters[best].coordinated = true;
                }
                if (round.end_signal()) sounds[BattleSound::round_end] = 30;
                for (int i = 1; i <= GameLimits::battle_sounds; ++i) if (sounds[i])
                    display.sound(battle_sounds[i], float(sounds[i] * 3 / 5) / 63.0f);
                if (round.finished()) return false;
                int sum = 0, count = 0, human_sum = 0, human_count = 0;
                for (std::size_t i = 0; i < fighters.size(); ++i) if (fighters[i].active && fighters[i].state / 10 != FighterStateGroup::angel) {
                    sum += fighters[i].x; ++count;
                    if (controllers[i] >= 0) { human_sum += fighters[i].x; ++human_count; }
                }
                if (human_count) { sum = human_sum; count = human_count; }
                if (sum && count) {
                    int shift = (sum / count - 160) / 6;
                    if (camera - shift < -160) shift = camera + 160;
                    if (camera - shift > 160) shift = camera - 160;
                    camera -= shift;
                    for (auto& fighter : fighters) if (fighter.active) fighter.x -= shift;
                    for (auto& projectile : projectiles) projectile.x -= shift;
                    for (auto& item : ground_items) item.x -= shift;
                    for (auto& blast : blasts) blast.x -= shift;
                    for (auto& effect : hit_effects) effect.x -= shift;
                }
                return true;

}
