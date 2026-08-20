/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2021 - 2025                                             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

/***************************************************************************
 *   Mod    : NextGen, 2026                                                *
 *   Author : Sergey Ergle                                                 *
 ***************************************************************************/

#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "resource.h"

namespace fheroes2
{
    // Spell power value, based on which the effect of the monsters' built-in spells is calculated
    inline constexpr int spellPowerForBuiltinMonsterSpells{ 3 };
    inline constexpr uint32_t MONSTER_GROWTH_FACTOR { 28 }; /* days in month */

    enum class MonAbil : uint32_t
    {
        // Basic abilities (usually not shown in the unit description).
        DOUBLE_HEX_SIZE,
        FLYING,

        // Advanced abilities (shown in the unit description).
        UNDEAD,
        ELEMENTAL,
        MECHANICAL,

        DOUBLE_SHOOTING,
        DOUBLE_MELEE_ATTACK,
        TWO_CELL_MELEE_ATTACK,
        AREA_SHOT,
        ALL_ADJACENT_MELEE_ATTACK,
        UNLIMITED_RETALIATION,
        NO_ENEMY_RETALIATION,
        NO_MELEE_PENALTY,

        DOUBLE_DAMAGE_TO_UNDEAD,
        ENEMY_HALVING,

        IMMUNE_TO_ALL_MAGIC,
        IMMUNE_TO_MIND_SPELLS,
        IMMUNE_TO_ELEMENTAL_SPELLS,
        IMMUNE_TO_FIRE_SPELLS,
        IMMUNE_TO_COLD_SPELLS,
        IMMUNE_TO_LIGHTNING,
        IMMUNE_TO_METEORSHOWER,
        IMMUNE_TO_ELEMENTALSTORM,
        IMMUNE_TO_CURSE,

        DWARF_MAGIC_RESISTANCE,

        HP_REGENERATION,
        HP_DRAIN,
        SOUL_EATER,

        MORAL_DECREMENT,
        IRON_MUSCLES,
        ETHER_DAMAGE,
        ETHER_SHOT_DAMAGE,
        BONE_BODY,
        ETHERIC_BODY,
        NATURAL_SHILD,
        EXTRA_DAMAGE_TO_BIG_MONSTERS,
        RESISTANCE_OF_DESTRUCTION_MAGIC,
        LEVEL_DAMAGE,
        DIRTY_BLOW,
        BIRD_HUNTING,
        EXTRA_LUCK,

        ALL_ABILITIES
    };

    constexpr uint64_t AbilMask(const MonAbil abil)
    {
        return 1ULL << (static_cast<int>(abil));
    }

    std::vector<MonAbil> MaskToAbilities(uint64_t mask);

    struct MonsterBattleStats
    {
        uint32_t level;
        uint32_t hp;

        int32_t attack;
        int32_t defense;
        uint32_t damageMin;
        uint32_t damageMax;
        uint32_t shots;

        uint32_t priority;
        uint32_t speed;
        uint32_t bonusSpeed;

        uint32_t experience;
        int32_t monsterBaseStrength;

        int32_t  spellCast;
        uint32_t spellPercent;

        uint64_t abils;

        static constexpr int32_t STRENGTH_FACTOR { 1024 };

        bool hasAbil(const MonAbil abil) const;
    };

    struct MonsterGeneralStats
    {
        uint32_t race;
        uint32_t level;
        uint32_t baseGrowth;
        uint32_t speedGrowth;  /* monsters born in 1 month */

        Cost cost;

    };

    struct MonsterName
    {
        const char* untranslated;
        const char* untranslatedPlural;
    };

    struct MonsterSound
    {
        int meleeAttack;
        int death;
        int movement;
        int wince;
        int rangeAttack;
        int takeoff;
        int landing;
        int explosion;
    };

    const int getMonIcnId(const int monId);
    const char* getMonBinFileName(const int monId);
    const MonsterName& getMonName(const int monId);
    const MonsterSound& getMonSounds(const int monId);
    const MonsterBattleStats& getMonBattleStats(const int monId);
    const MonsterGeneralStats& getMonGeneralStats(const int monId);

    std::string getMonsterAbilityDescription( const MonAbil ability, const bool ignoreBasicAbilities );

    std::string getMonsterDescription( const int monsterId ); // To be utilized in future.

    std::vector<std::string> getMonsterPropertiesDescription( const int monsterId );

    uint32_t getSpellResistance( const int monsterId, const int spellId );
}
