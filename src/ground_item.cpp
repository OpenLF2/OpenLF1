// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "ground_item.hpp"
#include <algorithm>
#include <cstdlib>

GroundItem GroundItem::create(const std::function<int(int)>& draw) {
    GroundItem result;
    for (;;) {
        result.type = draw(10);
        const bool rejected = result.type == ItemType::bomb || result.type == ItemType::sword || result.type == ItemType::plus || result.type == ItemType::bow;
        if (!rejected) {
            if (result.type == ItemType::drink) draw(1);
            break;
        }
        result.variant = draw(2);
    }
    result.x = draw(267) + 27;
    result.y = draw(66) + 120;
    result.facing = 1 - draw(2) * 2;
    result.durability = result.type == ItemType::stone || result.type == ItemType::box || result.type == ItemType::claw ? 2 : 6;
    return result;
}
void GroundItem::advance(const ProjectileType& d, int camera, int upper, int lower, const std::function<int(int)>& draw) {
    falling_impact.reset();
    if (!active || owner >= 0) return;
    z += vz;
    const int ground = type == ItemType::stone || type == ItemType::box ? 0 : 3;
    if (z >= 200) active = false;
    if (z > ground) {
        if (vz > 10) {
            vz = -vz / std::max(1, d.bounce);
            z = ground - 1;
            if (vx == 0) vx = draw(19) - 9;
        } else {
            vz = 0; z = ground; frame = 3; animation = 1;
            if (--durability <= 0) { active = false; vy = 0; }
            if (durability <= 4 && type == ItemType::bomb) { active = false; detonate = true; }
        }
        vx /= 2;
    }
    const bool random_facing = draw(2) != 0;
    settled = z == ground && vz == 0;
    if (settled) vy = 0;
    if (z < ground) {
        do {
            if (++animation > 6) animation = 1;
            facing = animation == 1 || animation == 4 || animation == 5 ? 1 : -1;
            frame = animation == 1 || animation == 6 ? 1 : animation == 3 || animation == 4 ? 2 : 3;
        } while ((d.motion == 2 || d.motion == ItemMotion::homing || d.motion == ItemMotion::ignore_downed) && frame == 3);
        if (ground == 0) {
            const int direction = vx != 0 ? (vx > 0 ? 1 : -1) : (random_facing ? 2 : -1);
            falling_impact = ImpactRegion{x, y, z, 10, 13, direction, vx / 2, -7, d.damage, 500};
        }
        vz += 3;
    }
    if (settled) vx = 0;
    x += vx; y = std::clamp(y + vy, upper - 3, lower + 2);
    if (x <= camera - 190 || x >= camera + 510) {
        if (durability > 8) {
            settled = false; animation = 1; frame = 3; vz = -17;
            vx = x <= camera - 190 ? 10 : -10;
        } else active = false;
    }
}

bool GroundItem::react_to_fighter_hit(const ProjectileType& d, int damage, int source_action, const std::function<int(int)>& draw) {
    if (damage / ImpactCode::scale >= ImpactCode::fire_kind || d.effect == ImpactCode::piercing_kind || (source_action >= FighterAction::weapon_swing_first && source_action <= FighterAction::weapon_swing_last)) return false;
    if (durability < 500) {
        animation = 1; frame = 3;
        if (d.category == ItemCategory::throwable) { vz = -3; vx /= 2; }
        else { vz = -7; vx = draw(7) - 3; }
    }
    return true;
}

ItemCollision GroundItem::collide(const ImpactRegion& hit, Combatant& attacker, const ProjectileType& d,
                        std::span<Combatant> fighters, int attacker_index, const std::function<int(int)>& draw) {
    if (!active || hit.damage <= 0 || (owner == attacker_index && d.category != ItemCategory::throwable) || !attacker.active) return ItemCollision::none;
    if (owner >= 0 && owner < int(fighters.size()) && fighters[owner].team == attacker.team && d.category != ItemCategory::throwable) return ItemCollision::none;
    if (attacker.priority <= AttackPriority::none && attacker.action != FighterAction::punch_hit && attacker.action != FighterAction::punch_hit_alt && attacker.action != FighterAction::kick_hit && attacker.action != FighterAction::kick_hit_alt) return ItemCollision::none;
    if (std::abs(attacker.y - y) >= 8 || std::abs(attacker.x - x) >= hit.width + 12 ||
        std::abs(attacker.z - z) >= hit.height + 7) return ItemCollision::none;
    if (d.effect != ImpactCode::none_kind && d.effect != ImpactCode::normal_kind && d.effect != ImpactCode::fire_kind && d.effect != ImpactCode::explosion_kind && d.effect != ImpactCode::piercing_kind && d.effect != ImpactCode::freeze_kind) return ItemCollision::none;
    if (owner >= 0 && owner < int(fighters.size())) {
        auto& holder = fighters[owner];
        holder.carried = ItemSlot::none;
        if (holder.state / 50 == FighterStateGroup::item) { holder.state = FighterState::idle; holder.action = FighterAction::idle; }
    }
    owner = ItemOwnership::unowned;
    last_owner = attacker_index;
    attacker.energy = std::min(attacker.max_energy, attacker.energy + 7);
    if (d.effect == ImpactCode::fire_kind || d.effect == ImpactCode::explosion_kind || d.effect == ImpactCode::piercing_kind || d.effect == ImpactCode::freeze_kind) {
        active = false;
        return ItemCollision::destroyed;
    }
    if (d.effect == ImpactCode::none_kind || d.effect == ImpactCode::normal_kind) {
        settled = false; animation = 1; frame = 3;
        if (d.category == ItemCategory::throwable) { vz = -5; vx = hit.push_x; }
        else { vz = -7; vx = draw(7) - 3; }
        return ItemCollision::bounced;
    }
    return ItemCollision::none;
}
bool GroundItem::claim(Combatant& fighter, int fighter_index, int item_index, std::span<GroundItem> items, const std::function<int(int)>& draw) {
    if (!active || owner >= 0 || !settled || !fighter.active || fighter.priority > AttackPriority::none || fighter.z != 0 ||
        fighter.history.back() != InputKey::attack || (fighter.state != FighterState::punch_active && fighter.state != FighterState::kick_active && fighter.state / 50 != FighterStateGroup::item) ||
        std::abs(fighter.y - y) >= 8 || std::abs(fighter.x - fighter.facing * 2 - x) >= 15 ||
        std::abs(fighter.z - 10 - z) >= 18) return false;
    if (fighter.carried >= 0) {
        if (fighter.carried >= int(items.size()) || &items[fighter.carried] == this) return false;
        auto& previous = items[fighter.carried];
        previous.owner = ItemOwnership::unowned;
        previous.last_owner = ItemOwnership::detached;
        if (previous.type == ItemType::summoned_weapon) previous.active = false;
        else {
            previous.settled = false; previous.frame = 3; previous.vz = -7;
            previous.vx = draw ? draw(7) - 3 : 0;
        }
    }
    owner = fighter_index;
    last_owner = fighter_index;
    fighter.carried = item_index;
    fighter.state = FighterState::holding_item;
    fighter.action = FighterAction::landing;
    fighter.remember(InputKey::interrupted);
    return true;
}
void GroundItem::animate_holder(Combatant& fighter, int holder, const ProjectileType& d, int first_special_cost, bool round_active) {
    if (fighter.state < FighterState::item_throw_base) {
        if (fighter.action == FighterAction::landing) fighter.action = FighterAction::idle;
        if (fighter.velocity_x != 0 || fighter.velocity_y != 0)
            if (++fighter.animation_step > CombatTuning::animation_cycle_frames) fighter.animation_step = 1;
        if (!(active && owner == holder && fighter.state != FighterState::idle && round_active)) {
            fighter.carried = ItemSlot::none; fighter.state = FighterState::idle; fighter.action = FighterAction::idle;
        }
        fighter.action = fighter.animation_step == CombatTuning::animation_cycle_frames ? FighterAction::walk_first : fighter.animation_step;
        if (type == ItemType::summoned_weapon) fighter.action += FighterAction::summon_weapon_offset;
        if (d.category == ItemCategory::throwable && fighter.action < FighterAction::punch_hit) fighter.action += FighterAction::carry_offset;
    }
    if (fighter.state / 20 == FighterStateGroup::consuming) {
        fighter.action = fighter.state % 2 == 0 ? FighterAction::drink_first : FighterAction::drink_second;
        ++fighter.state;
        if (fighter.state == FighterState::consume_last_charge) durability = 1;
        if (variant == 0) {
            if (fighter.health < fighter.max_health) fighter.health = std::min(fighter.max_health, fighter.health + 3);
            if (fighter.energy < fighter.max_energy) fighter.energy = std::min(fighter.max_energy, fighter.energy + 3);
        } else if (fighter.energy < fighter.max_energy) fighter.energy = std::min(fighter.max_energy, fighter.energy + 11);
        if (fighter.state > FighterState::consume_finish) {
            fighter.state = FighterState::idle; fighter.action = FighterAction::idle; fighter.carried = ItemSlot::none; active = false; owner = ItemOwnership::unowned;
            return;
        }
    }
    if (fighter.state / 10 == FighterStateGroup::using_weapon) {
        if (fighter.state == FighterState::weapon_active && fighter.action == FighterAction::item_windup) {
            fighter.action = FighterAction::weapon_idle; fighter.state = FighterState::item_idle_base;
            if (fighter.archetype == FighterType::sword_user && type == ItemType::sword && fighter.energy >= first_special_cost) {
                fighter.energy -= first_special_cost; fighter.launched_projectile = ItemType::sword_projectile; fighter.projectile_count = 1;
            }
        } else if (fighter.state == FighterState::weapon_active && fighter.action == FighterAction::bow_use) {
            if (fighter.energy >= first_special_cost) {
                fighter.energy -= first_special_cost; fighter.action = FighterAction::bow_release; fighter.launched_projectile = ItemType::arrow; fighter.projectile_count = 1;
            } else { fighter.state = FighterState::idle; fighter.action = FighterAction::idle; }
        }
        if (fighter.state == FighterState::weapon_finish) fighter.state = FighterState::item_idle_base;
        ++fighter.state;
    }
}

std::optional<ImpactRegion> GroundItem::follow(Combatant& fighter, const ProjectileType& d, bool holding, const std::function<int(int)>& draw) {
    if (owner < 0 || !active) return std::nullopt;
    if (!holding || !fighter.active || (fighter.state / 50 != FighterStateGroup::item && fighter.state / SpecialMoveId::state_stride != SpecialState::chung_kem)) {
        owner = ItemOwnership::unowned; last_owner = ItemOwnership::detached; if (holding) fighter.carried = ItemSlot::none; settled = false;
        frame = 3; vz = -7; vx = draw ? draw(7) - 3 : 0;
        if (type == ItemType::summoned_weapon) active = false;
        return std::nullopt;
    }
    int step = fighter.animation_step == CombatTuning::animation_cycle_frames ? 2 : fighter.animation_step;
    facing = fighter.facing;
    x = fighter.x; y = fighter.y + 1; z = d.category == ItemCategory::melee ? step - 11 : d.category == ItemCategory::ranged ? step - 7 : -16;
    frame = type == ItemType::summoned_weapon ? 3 - frame : 2;
    if (frame < 1 || frame > 2) frame = 1;
    if (fighter.action == FighterAction::bow_use || fighter.action == FighterAction::bow_release) frame = 4;
    if (d.category == ItemCategory::melee && fighter.action >= FighterAction::weapon_swing_first && fighter.action <= FighterAction::weapon_swing_last) {
        x += facing * 12; y = fighter.y - 1; z = -1; frame = 3;
        return ImpactRegion{x + facing * 13, y, z, 5, 14, facing, facing * 5, -7, d.damage * 2 / 3 + d.effect * ImpactCode::scale, 1059};
    }
    if (d.category == ItemCategory::melee && fighter.action == FighterAction::item_windup) { x -= facing * 19; z = -14; facing = -facing; }
    if (d.category == ItemCategory::melee && fighter.action == FighterAction::weapon_idle) {
        x += facing * 18; z = -7; frame = 3;
        const int effect = (d.knockdown == ItemKnockdown::knockdown ? ImpactCode::knockdown : 25) + (d.effect == ImpactCode::normal_kind ? 1000 : 0);
        return ImpactRegion{x, y, z, 9, 17, fighter.facing, fighter.facing * 7, -7, d.damage / 2 + d.effect * ImpactCode::scale, effect};
    }
    if (d.category == ItemCategory::ranged) {
        if (fighter.action >= FighterAction::summon_weapon) { x -= facing * 7; z = -23; }
        else if (fighter.action == FighterAction::item_windup) { x -= facing * (type == ItemType::summoned_weapon ? 12 : 14); z = type == ItemType::summoned_weapon ? -20 : -10; facing = -facing; }
        else if (fighter.action == FighterAction::drink_first || fighter.action == FighterAction::drink_second) {
            x -= facing * (variant == 1 ? 1 : 4); y = fighter.y - 1;
            z = -(fighter.action == FighterAction::drink_first ? (variant == 1 ? 13 : 11) : (variant == 1 ? 12 : 10));
            frame = 1;
        } else x -= facing * 6;
    }
    if (d.category == ItemCategory::throwable) x -= facing * 2;
    return std::nullopt;
}

void equip_initial_bows(std::span<Combatant> fighters, std::span<const int> controls, std::vector<GroundItem>& items) {
    while (items.size() < 5) { GroundItem empty; empty.active = false; items.push_back(empty); }
    for (int i = 0; i < int(fighters.size()) && i < int(controls.size()); ++i) {
        auto& fighter = fighters[i];
        if (!fighter.active || fighter.archetype != FighterType::bow_user || controls[i] < 0) continue;
        const auto slot = std::find_if(items.begin(), items.end(), [](const auto& item) { return !item.active; });
        if (slot == items.end()) break;
        const int index = int(slot - items.begin());
        *slot = GroundItem{};
        slot->slot = 7 + int(std::count_if(items.begin(), items.end(), [](const auto& item) { return item.active; }));
        slot->type = ItemType::bow; slot->owner = slot->last_owner = i; slot->durability = 500;
        slot->x = fighter.x; slot->y = fighter.y + 1; slot->z = fighter.animation_step - 11; slot->facing = fighter.facing;
        fighter.carried = index; fighter.state = FighterState::holding_item; fighter.action = FighterAction::idle;
    }
}
