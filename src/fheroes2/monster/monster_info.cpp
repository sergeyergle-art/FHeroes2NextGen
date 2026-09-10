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

#include "monster_info.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <sstream>

#include "icn.h"
#include "m82.h"
#include "monster.h"
#include "race.h"
#include "speed.h"
#include "spell.h"
#include "tools.h"
#include "translations.h"

#include <stdio.h>

namespace
{
    using namespace fheroes2;

    static_assert(static_cast<uint32_t>(MonAbil::ALL_ABILITIES) <= 64);

    const int g_monsterIcnIds[Monster::MONSTER_COUNT] = {
        ICN::UNKNOWN,  ICN::PEASANT,  ICN::ARCHER,   ICN::ARCHER2,  ICN::PIKEMAN,  ICN::PIKEMAN2, ICN::SWORDSMN, ICN::SWORDSM2, ICN::CAVALRYR,
        ICN::CAVALRYB, ICN::PALADIN,  ICN::PALADIN2, ICN::GOBLIN,   ICN::ORC,      ICN::ORC2,     ICN::WOLF,     ICN::OGRE,     ICN::OGRE2,
        ICN::TROLL,    ICN::TROLL2,   ICN::CYCLOPS,  ICN::SPRITE,   ICN::DWARF,    ICN::DWARF2,   ICN::ELF,      ICN::ELF2,     ICN::DRUID,
        ICN::DRUID2,   ICN::UNICORN,  ICN::PHOENIX,  ICN::CENTAUR,  ICN::GARGOYLE, ICN::GRIFFIN,  ICN::MINOTAUR, ICN::MINOTAU2, ICN::HYDRA,
        ICN::DRAGGREE, ICN::DRAGRED,  ICN::DRAGBLAK, ICN::HALFLING, ICN::BOAR,     ICN::GOLEM,    ICN::GOLEM2,   ICN::ROC,      ICN::MAGE1,
        ICN::MAGE2,    ICN::TITANBLU, ICN::TITANBLA, ICN::SKELETON, ICN::ZOMBIE,   ICN::ZOMBIE2,  ICN::MUMMYW,   ICN::MUMMY2,   ICN::VAMPIRE,
        ICN::VAMPIRE2, ICN::LICH,     ICN::LICH2,    ICN::DRAGBONE, ICN::ROGUE,    ICN::NOMAD,    ICN::GHOST,    ICN::GENIE,    ICN::MEDUSA,
        ICN::EELEM,    ICN::AELEM,    ICN::FELEM,    ICN::WELEM,    ICN::UNKNOWN,  ICN::UNKNOWN,  ICN::UNKNOWN,  ICN::UNKNOWN,  ICN::UNKNOWN
    };

    const char * g_binFileName[Monster::MONSTER_COUNT] = {
        "UNKNOWN",      "PEAS_FRM.BIN", "ARCHRFRM.BIN", "ARCHRFRM.BIN", "PIKMNFRM.BIN", "PIKMNFRM.BIN", "SWRDSFRM.BIN", "SWRDSFRM.BIN", "CVLRYFRM.BIN",
        "CVLR2FRM.BIN", "PALADFRM.BIN", "PALADFRM.BIN", "GOBLNFRM.BIN", "ORC__FRM.BIN", "ORC__FRM.BIN", "WOLF_FRM.BIN", "OGRE_FRM.BIN", "OGRE_FRM.BIN",
        "TROLLFRM.BIN", "TROLLFRM.BIN", "CYCLOFRM.BIN", "SPRITFRM.BIN", "DWARFFRM.BIN", "DWARFFRM.BIN", "ELF__FRM.BIN", "ELF__FRM.BIN", "DRUIDFRM.BIN",
        "DRUIDFRM.BIN", "UNICOFRM.BIN", "PHOENFRM.BIN", "CENTRFRM.BIN", "GARGLFRM.BIN", "GRIFFFRM.BIN", "MINOTFRM.BIN", "MINOTFRM.BIN", "HYDRAFRM.BIN",
        "DRAGGFRM.BIN", "DRAGRFRM.BIN", "DRAGBFRM.BIN", "HALFLFRM.BIN", "BOAR_FRM.BIN", "GOLEMFRM.BIN", "GOLEMFRM.BIN", "ROC__FRM.BIN", "MAGE1FRM.BIN",
        "MAGE1FRM.BIN", "TITANFRM.BIN", "TITA2FRM.BIN", "SKEL_FRM.BIN", "ZOMB_FRM.BIN", "ZOMB_FRM.BIN", "MUMMYFRM.BIN", "MUMMYFRM.BIN", "VAMPIFRM.BIN",
        "VAMPIFRM.BIN", "LICH_FRM.BIN", "LICH_FRM.BIN", "DRABNFRM.BIN", "ROGUEFRM.BIN", "NOMADFRM.BIN", "GHOSTFRM.BIN", "GENIEFRM.BIN", "MEDUSFRM.BIN",
        "FELEMFRM.BIN", "FELEMFRM.BIN", "FELEMFRM.BIN", "FELEMFRM.BIN", "UNKNOWN",      "UNKNOWN",      "UNKNOWN",      "UNKNOWN",      "UNKNOWN"
    };

    const MonsterSound g_monsterSounds[Monster::MONSTER_COUNT] = {
        // melee attack | death | movement | wince | ranged attack | takeoff | landing | explosion
        { M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Unknown Monster
        { M82::PSNTATTK, M82::PSNTKILL, M82::PSNTMOVE, M82::PSNTWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Peasant
        { M82::ARCHATTK, M82::ARCHKILL, M82::ARCHMOVE, M82::ARCHWNCE, M82::ARCHSHOT, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Archer
        { M82::ARCHATTK, M82::ARCHKILL, M82::ARCHMOVE, M82::ARCHWNCE, M82::ARCHSHOT, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Ranger
        { M82::PIKEATTK, M82::PIKEKILL, M82::PIKEMOVE, M82::PIKEWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Pikeman
        { M82::PIKEATTK, M82::PIKEKILL, M82::PIKEMOVE, M82::PIKEWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Veteran Pikeman
        { M82::SWDMATTK, M82::SWDMKILL, M82::SWDMMOVE, M82::SWDMWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Swordsman
        { M82::SWDMATTK, M82::SWDMKILL, M82::SWDMMOVE, M82::SWDMWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Master Swordsman
        { M82::CAVLATTK, M82::CAVLKILL, M82::CAVLMOVE, M82::CAVLWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Cavalry
        { M82::CAVLATTK, M82::CAVLKILL, M82::CAVLMOVE, M82::CAVLWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Champion
        { M82::PLDNATTK, M82::PLDNKILL, M82::PLDNMOVE, M82::PLDNWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Paladin
        { M82::PLDNATTK, M82::PLDNKILL, M82::PLDNMOVE, M82::PLDNWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Crusader
        { M82::GBLNATTK, M82::GBLNKILL, M82::GBLNMOVE, M82::GBLNWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Goblin
        { M82::ORC_ATTK, M82::ORC_KILL, M82::ORC_MOVE, M82::ORC_WNCE, M82::ORC_SHOT, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Orc
        { M82::ORC_ATTK, M82::ORC_KILL, M82::ORC_MOVE, M82::ORC_WNCE, M82::ORC_SHOT, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Orc Chief
        { M82::WOLFATTK, M82::WOLFKILL, M82::WOLFMOVE, M82::WOLFWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Wolf
        { M82::OGREATTK, M82::OGREKILL, M82::OGREMOVE, M82::OGREWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Ogre
        { M82::OGREATTK, M82::OGREKILL, M82::OGREMOVE, M82::OGREWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Ogre Lord
        { M82::TRLLATTK, M82::TRLLKILL, M82::TRLLMOVE, M82::TRLLWNCE, M82::TRLLSHOT, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Troll
        { M82::TRLLATTK, M82::TRLLKILL, M82::TRLLMOVE, M82::TRLLWNCE, M82::TRLLSHOT, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // War Troll
        { M82::CYCLATTK, M82::CYCLKILL, M82::CYCLMOVE, M82::CYCLWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Cyclops
        { M82::SPRTATTK, M82::SPRTKILL, M82::SPRTMOVE, M82::SPRTWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Sprite
        { M82::DWRFATTK, M82::DWRFKILL, M82::DWRFMOVE, M82::DWRFWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Dwarf
        { M82::DWRFATTK, M82::DWRFKILL, M82::DWRFMOVE, M82::DWRFWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Battle Dwarf
        { M82::ELF_ATTK, M82::ELF_KILL, M82::ELF_MOVE, M82::ELF_WNCE, M82::ELF_SHOT, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Elf
        { M82::ELF_ATTK, M82::ELF_KILL, M82::ELF_MOVE, M82::ELF_WNCE, M82::ELF_SHOT, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Grand Elf
        { M82::DRUIATTK, M82::DRUIKILL, M82::DRUIMOVE, M82::DRUIWNCE, M82::DRUISHOT, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Druid
        { M82::DRUIATTK, M82::DRUIKILL, M82::DRUIMOVE, M82::DRUIWNCE, M82::DRUISHOT, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Greater Druid
        { M82::UNICATTK, M82::UNICKILL, M82::UNICMOVE, M82::UNICWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Unicorn
        { M82::PHOEATTK, M82::PHOEKILL, M82::PHOEMOVE, M82::PHOEWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Phoenix
        { M82::CNTRATTK, M82::CNTRKILL, M82::CNTRMOVE, M82::CNTRWNCE, M82::CNTRSHOT, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Centaur
        { M82::GARGATTK, M82::GARGKILL, M82::GARGMOVE, M82::GARGWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Gargoyle
        { M82::GRIFATTK, M82::GRIFKILL, M82::GRIFMOVE, M82::GRIFWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Griffin
        { M82::MINOATTK, M82::MINOKILL, M82::MINOMOVE, M82::MINOWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Minotaur
        { M82::MINOATTK, M82::MINOKILL, M82::MINOMOVE, M82::MINOWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Minotaur King
        { M82::HYDRATTK, M82::HYDRKILL, M82::HYDRMOVE, M82::HYDRWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Hydra
        { M82::DRGNATTK, M82::DRGNKILL, M82::DRGNMOVE, M82::DRGNWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Green Dragon
        { M82::DRGNATTK, M82::DRGNKILL, M82::DRGNMOVE, M82::DRGNWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Red Dragon
        { M82::DRGNATTK, M82::DRGNKILL, M82::DRGNMOVE, M82::DRGNWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Black Dragon
        { M82::HALFATTK, M82::HALFKILL, M82::HALFMOVE, M82::HALFWNCE, M82::HALFSHOT, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Halfling
        { M82::BOARATTK, M82::BOARKILL, M82::BOARMOVE, M82::BOARWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Boar
        { M82::GOLMATTK, M82::GOLMKILL, M82::GOLMMOVE, M82::GOLMWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Iron Golem
        { M82::GOLMATTK, M82::GOLMKILL, M82::GOLMMOVE, M82::GOLMWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Steel Golem
        { M82::ROC_ATTK, M82::ROC_KILL, M82::ROC_MOVE, M82::ROC_WNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Roc
        { M82::MAGEATTK, M82::MAGEKILL, M82::MAGEMOVE, M82::MAGEWNCE, M82::MAGESHOT, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Mage
        { M82::MAGEATTK, M82::MAGEKILL, M82::MAGEMOVE, M82::MAGEWNCE, M82::MAGESHOT, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Archmage
        { M82::TITNATTK, M82::TITNKILL, M82::TITNMOVE, M82::TITNWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Giant
        { M82::TITNATTK, M82::TITNKILL, M82::TITNMOVE, M82::TITNWNCE, M82::TITNSHOT, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Titan
        { M82::SKELATTK, M82::SKELKILL, M82::SKELMOVE, M82::SKELWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Skeleton
        { M82::ZOMBATTK, M82::ZOMBKILL, M82::ZOMBMOVE, M82::ZOMBWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Zombie
        { M82::ZOMBATTK, M82::ZOMBKILL, M82::ZOMBMOVE, M82::ZOMBWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Mutant Zombie
        { M82::MUMYATTK, M82::MUMYKILL, M82::MUMYMOVE, M82::MUMYWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Mummy
        { M82::MUMYATTK, M82::MUMYKILL, M82::MUMYMOVE, M82::MUMYWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Royal Mummy
        { M82::VAMPATTK, M82::VAMPKILL, M82::VAMPMOVE, M82::VAMPWNCE, M82::UNKNOWN, M82::VAMPEXT1, M82::VAMPEXT2, M82::UNKNOWN }, // Vampire
        { M82::VAMPATTK, M82::VAMPKILL, M82::VAMPMOVE, M82::VAMPWNCE, M82::UNKNOWN, M82::VAMPEXT1, M82::VAMPEXT2, M82::UNKNOWN }, // Vampire Lord
        { M82::LICHATTK, M82::LICHKILL, M82::LICHMOVE, M82::LICHWNCE, M82::LICHSHOT, M82::UNKNOWN, M82::UNKNOWN, M82::LICHEXPL }, // Lich
        { M82::LICHATTK, M82::LICHKILL, M82::LICHMOVE, M82::LICHWNCE, M82::LICHSHOT, M82::UNKNOWN, M82::UNKNOWN, M82::LICHEXPL }, // Power Lich
        { M82::BONEATTK, M82::BONEKILL, M82::BONEMOVE, M82::BONEWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Bone Dragon
        { M82::ROGUATTK, M82::ROGUKILL, M82::ROGUMOVE, M82::ROGUWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Rogue
        { M82::NMADATTK, M82::NMADKILL, M82::NMADMOVE, M82::NMADWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Nomad
        { M82::GHSTATTK, M82::GHSTKILL, M82::GHSTMOVE, M82::GHSTWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Ghost
        { M82::GENIATTK, M82::GENIKILL, M82::GENIMOVE, M82::GENIWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Genie
        { M82::MEDSATTK, M82::MEDSKILL, M82::MEDSMOVE, M82::MEDSWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Medusa
        { M82::EELMATTK, M82::EELMKILL, M82::EELMMOVE, M82::EELMWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Earth Elemental
        { M82::AELMATTK, M82::AELMKILL, M82::AELMMOVE, M82::AELMWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Air Elemental
        { M82::FELMATTK, M82::FELMKILL, M82::FELMMOVE, M82::FELMWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Fire Elemental
        { M82::WELMATTK, M82::WELMKILL, M82::WELMMOVE, M82::WELMWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Water Elemental
        { M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Random Monster
        { M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Random Monster 1
        { M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Random Monster 2
        { M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Random Monster 3
        { M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN }, // Random Monster 4
    };

    // Monster abilities will be added later. Base strength is calculated based on abilities.
    const MonsterBattleStats g_monsterBattleStats[Monster::MONSTER_COUNT] = {
/* Unknown Monster  */ { 0  , 0   ,    0  , 0  , 0  , 0  , 0  ,    0  , 0  ,    0   , 0      , 0, 0, 0 },
/* Peasant          */ { 1  , 3   ,    1  , 1  , 1  , 1  , 0  ,    3  , 3  ,    1   , 1989   , 0, 0, 0 },
/* Archer           */ { 4  , 15  ,    7  , 2  , 2  , 3  , 12 ,    4  , 3  ,    8   , 9247   , 0, 0, 0 },
/* Ranger           */ { 5  , 15  ,    7  , 2  , 2  , 3  , 24 ,    6  , 4  ,    12  , 12857  , 0, 0, AbilMask(MonAbil::DOUBLE_SHOOTING) },
/* Pikeman          */ { 6  , 30  ,    8  , 10 , 4  , 6  , 0  ,    5  , 5  ,    15  , 10880  , 0, 0, AbilMask(MonAbil::EXTRA_DAMAGE_TO_BIG_MONSTERS) },
/* Veteran Pikeman  */ { 7  , 40  ,    8  , 10 , 5  , 8  , 0  ,    6  , 6  ,    20  , 14358  , 0, 0, AbilMask(MonAbil::EXTRA_DAMAGE_TO_BIG_MONSTERS) },
/* Swordsman        */ { 8  , 45  ,    10 , 11 , 6  , 11 , 0  ,    6  , 6  ,    25  , 16363  , 0, 0, AbilMask(MonAbil::NATURAL_SHILD) },
/* Master Swordsman */ { 9  , 55  ,    10 , 11 , 7  , 12 , 0  ,    7  , 7  ,    35  , 21850  , 0, 0, AbilMask(MonAbil::NATURAL_SHILD) },
/* Cavalry          */ { 12 , 70  ,    14 , 12 , 10 , 16 , 0  ,    9  , 11 ,    45  , 25742  , 0, 0, AbilMask(MonAbil::DOUBLE_HEX_SIZE) | AbilMask(MonAbil::EXTRA_DAMAGE_TO_BIG_MONSTERS) },
/* Champion         */ { 13 , 80  ,    14 , 12 , 10 , 20 , 0  ,    10 , 12 ,    60  , 33993  , 0, 0, AbilMask(MonAbil::DOUBLE_HEX_SIZE) | AbilMask(MonAbil::EXTRA_DAMAGE_TO_BIG_MONSTERS) },
/* Paladin          */ { 16 , 100 ,    15 , 17 , 12 , 24 , 0  ,    7  , 7  ,    100 , 47232  , 0, 0, AbilMask(MonAbil::DOUBLE_MELEE_ATTACK) | AbilMask(MonAbil::RESISTANCE_OF_DESTRUCTION_MAGIC) },
/* Crusader         */ { 17 , 150 ,    15 , 17 , 15 , 24 , 0  ,    8  , 8  ,    133 , 62287  , 0, 0, AbilMask(MonAbil::DOUBLE_MELEE_ATTACK) | AbilMask(MonAbil::RESISTANCE_OF_DESTRUCTION_MAGIC) | AbilMask(MonAbil::IMMUNE_TO_CURSE) },
/* Goblin           */ { 2  , 6   ,    4  , 2  , 1  , 2  , 0  ,    4  , 4  ,    2   , 3118   , 0, 0, AbilMask(MonAbil::DIRTY_BLOW) },
/* Orc              */ { 5  , 20  ,    4  , 3  , 3  , 4  , 12 ,    3  , 2  ,    10  , 13730  , 0, 0, 0 },
/* Orc Chief        */ { 6  , 25  ,    5  , 3  , 3  , 5  , 12 ,    5  , 3  ,    14  , 15698  , 0, 0, 0 },
/* Wolf             */ { 7  , 35  ,    9  , 3  , 5  , 8  , 0  ,    9  , 11 ,    20  , 18700  , 0, 0, AbilMask(MonAbil::DOUBLE_HEX_SIZE) | AbilMask(MonAbil::DOUBLE_MELEE_ATTACK) },
/* Ogre             */ { 9  , 70  ,    11 , 7  , 8  , 11 , 0  ,    5  , 5  ,    30  , 22726  , 0, 0, AbilMask(MonAbil::LEVEL_DAMAGE) },
/* Ogre Lord        */ { 11 , 90  ,    14 , 8  , 8  , 11 , 0  ,    6  , 6  ,    40  , 25284  , 0, 0, AbilMask(MonAbil::LEVEL_DAMAGE) },
/* Troll            */ { 13 , 70  ,    12 , 7  , 11 , 17 , 18 ,    6  , 5  ,    55  , 36929  , 0, 0, AbilMask(MonAbil::NATURAL_SHILD) },
/* War Troll        */ { 14 , 90  ,    15 , 8  , 11 , 17 , 18 ,    8  , 6  ,    70  , 40983  , 0, 0, AbilMask(MonAbil::NATURAL_SHILD) },
/* Cyclops          */ { 17 , 180 ,    17 , 13 , 17 , 30 , 0  ,    7  , 7  ,    160 , 79025  , 0, 0, AbilMask(MonAbil::TWO_CELL_MELEE_ATTACK) | AbilMask(MonAbil::ETHER_DAMAGE) },
/* Sprite           */ { 2  , 3   ,    5  , 3  , 1  , 1  , 0  ,    6  , 10 ,    3   , 3156   , 0, 0, AbilMask(MonAbil::FLYING) | AbilMask(MonAbil::NO_ENEMY_RETALIATION) },
/* Dwarf            */ { 5  , 35  ,    7  , 7  , 4  , 6  , 0  ,    4  , 4  ,    14  , 11521  , 0, 0, AbilMask(MonAbil::DWARF_MAGIC_RESISTANCE) },
/* Battle Dwarf     */ { 6  , 35  ,    8  , 8  , 4  , 8  , 0  ,    5  , 5  ,    17  , 13080  , 0, 0, AbilMask(MonAbil::DWARF_MAGIC_RESISTANCE) },
/* Elf              */ { 7  , 30  ,    9  , 4  , 2  , 3  , 28 ,    6  , 5  ,    20  , 16254  , 0, 0, AbilMask(MonAbil::DOUBLE_SHOOTING) | AbilMask(MonAbil::BIRD_HUNTING) },
/* Grand Elf        */ { 8  , 30  ,    9  , 8  , 3  , 4  , 28 ,    8  , 6  ,    25  , 17519  , 0, 0, AbilMask(MonAbil::DOUBLE_SHOOTING) | AbilMask(MonAbil::BIRD_HUNTING) },
/* Druid            */ { 10 , 45  ,    10 , 9  , 6  , 8  , 16 ,    7  , 6  ,    30  , 20954  , 0, 0, AbilMask(MonAbil::NO_MELEE_PENALTY) },
/* Greater Druid    */ { 11 , 45  ,    12 , 10 , 8  , 10 , 16 ,    9  , 7  ,    40  , 23001  , 0, 0, AbilMask(MonAbil::NO_MELEE_PENALTY) },
/* Unicorn          */ { 14 , 100 ,    14 , 10 , 12 , 24 , 0  ,    7  , 9  ,    65  , 38882  , Spell::BLIND, 20, AbilMask(MonAbil::DOUBLE_HEX_SIZE) | AbilMask(MonAbil::EXTRA_LUCK) },
/* Phoenix          */ { 17 , 200 ,    16 , 14 , 30 , 55 , 0  ,    10 , 16 ,    200 , 99437  , 0, 0, AbilMask(MonAbil::DOUBLE_HEX_SIZE) | AbilMask(MonAbil::FLYING) | AbilMask(MonAbil::TWO_CELL_MELEE_ATTACK) | AbilMask(MonAbil::IMMUNE_TO_ELEMENTAL_SPELLS) },
/* Centaur          */ { 3  , 10  ,    5  , 3  , 2  , 3  , 10 ,    5  , 6  ,    7   , 7783   , 0, 0, AbilMask(MonAbil::DOUBLE_HEX_SIZE) },
/* Gargoyle         */ { 6  , 25  ,    5  , 8  , 3  , 6  , 0  ,    9  , 13 ,    15  , 13392  , 0, 0, AbilMask(MonAbil::FLYING) },
/* Griffin          */ { 10 , 50  ,    9  , 9  , 6  , 10 , 0  ,    6  , 12 ,    35  , 22726  , 0, 0, AbilMask(MonAbil::DOUBLE_HEX_SIZE) | AbilMask(MonAbil::FLYING) | AbilMask(MonAbil::UNLIMITED_RETALIATION) },
/* Minotaur         */ { 11 , 65  ,    12 , 10 , 8  , 16 , 0  ,    6  , 6  ,    40  , 22816  , 0, 0, 0 },
/* Minotaur King    */ { 13 , 75  ,    14 , 10 , 9  , 18 , 0  ,    7  , 7  ,    50  , 29170  , 0, 0, 0 },
/* Hydra            */ { 16 , 150 ,    11 , 14 , 12 , 24 , 0  ,    3  , 5  ,    100 , 57251  , 0, 0, AbilMask(MonAbil::DOUBLE_HEX_SIZE) | AbilMask(MonAbil::ALL_ADJACENT_MELEE_ATTACK) | AbilMask(MonAbil::NO_ENEMY_RETALIATION) },
/* Green Dragon     */ { 18 , 300 ,    16 , 16 , 35 , 50 , 0  ,    6  , 12 ,    270 , 125490 , 0, 0, AbilMask(MonAbil::DOUBLE_HEX_SIZE) | AbilMask(MonAbil::FLYING) | AbilMask(MonAbil::TWO_CELL_MELEE_ATTACK) | AbilMask(MonAbil::IMMUNE_TO_ALL_MAGIC) | AbilMask(MonAbil::IRON_MUSCLES) },
/* Red Dragon       */ { 19 , 350 ,    18 , 18 , 45 , 75 , 0  ,    7  , 14 ,    400 , 169874 , 0, 0, AbilMask(MonAbil::DOUBLE_HEX_SIZE) | AbilMask(MonAbil::FLYING) | AbilMask(MonAbil::TWO_CELL_MELEE_ATTACK) | AbilMask(MonAbil::IMMUNE_TO_ALL_MAGIC) | AbilMask(MonAbil::IRON_MUSCLES) },
/* Black Dragon     */ { 20 , 400 ,    20 , 20 , 55 , 75 , 0  ,    8  , 16 ,    530 , 207120 , 0, 0, AbilMask(MonAbil::DOUBLE_HEX_SIZE) | AbilMask(MonAbil::FLYING) | AbilMask(MonAbil::TWO_CELL_MELEE_ATTACK) | AbilMask(MonAbil::IMMUNE_TO_ALL_MAGIC) | AbilMask(MonAbil::IRON_MUSCLES) },
/* Halfling         */ { 2  , 4   ,    6  , 2  , 1  , 3  , 12 ,    5  , 4  ,    4   , 4302   , 0, 0, 0 },
/* Boar             */ { 6  , 30  ,    7  , 6  , 3  , 7  , 0  ,    6  , 8  ,    15  , 13356  , 0, 0, AbilMask(MonAbil::DOUBLE_HEX_SIZE) },
/* Iron Golem       */ { 7  , 45  ,    9  , 11 , 5  , 7  , 0  ,    3  , 5  ,    20  , 14634  , 0, 0, AbilMask(MonAbil::MECHANICAL) | AbilMask(MonAbil::IRON_MUSCLES) },
/* Steel Golem      */ { 9  , 60  ,    9  , 11 , 7  , 8  , 0  ,    4  , 6  ,    30  , 19067  , 0, 0, AbilMask(MonAbil::MECHANICAL) | AbilMask(MonAbil::IRON_MUSCLES) },
/* Roc              */ { 11 , 80  ,    11 , 8  , 6  , 12 , 0  ,    7  , 13 ,    45  , 30341  , 0, 0, AbilMask(MonAbil::DOUBLE_HEX_SIZE) | AbilMask(MonAbil::FLYING) },
/* Mage             */ { 14 , 60  ,    12 , 9  , 10 , 14 , 20 ,    7  , 6  ,    75  , 47460  , 0, 0, AbilMask(MonAbil::ETHER_SHOT_DAMAGE) },
/* Archmage         */ { 15 , 60  ,    12 , 12 , 12 , 18 , 20 ,    9  , 7  ,    85  , 48780  , Spell::DISPEL, 20, AbilMask(MonAbil::ETHER_SHOT_DAMAGE) },
/* Giant            */ { 18 , 300 ,    18 , 18 , 45 , 55 , 0  ,    7  , 9  ,    300 , 125122 , 0, 0, AbilMask(MonAbil::IMMUNE_TO_MIND_SPELLS) | AbilMask(MonAbil::IRON_MUSCLES) },
/* Titan            */ { 19 , 350 ,    20 , 18 , 45 , 55 , 22 ,    8  , 9  ,    420 , 171237 , 0, 0, AbilMask(MonAbil::NO_MELEE_PENALTY) | AbilMask(MonAbil::IMMUNE_TO_MIND_SPELLS) | AbilMask(MonAbil::IRON_MUSCLES) },
/* Skeleton         */ { 3  , 9   ,    5  , 1  , 2  , 3  , 0  ,    4  , 4  ,    4   , 5777   , 0, 0, AbilMask(MonAbil::UNDEAD) | AbilMask(MonAbil::BONE_BODY) },
/* Zombie           */ { 4  , 30  ,    4  , 3  , 2  , 4  , 0  ,    4  , 4  ,    10  , 12494  , 0, 0, AbilMask(MonAbil::UNDEAD) | AbilMask(MonAbil::NATURAL_SHILD) },
/* Mutant Zombie    */ { 5  , 35  ,    5  , 3  , 3  , 5  , 0  ,    5  , 5  ,    13  , 15764  , 0, 0, AbilMask(MonAbil::UNDEAD) | AbilMask(MonAbil::NATURAL_SHILD) },
/* Mummy            */ { 7  , 50  ,    7  , 7  , 5  , 8  , 0  ,    6  , 6  ,    20  , 17591  , Spell::SLOW, 20, AbilMask(MonAbil::UNDEAD) },
/* Royal Mummy      */ { 9  , 50  ,    8  , 8  , 5  , 10 , 0  ,    7  , 7  ,    25  , 19984  , Spell::SLOW, 30, AbilMask(MonAbil::UNDEAD) },
/* Vampire          */ { 11 , 50  ,    8  , 8  , 5  , 9  , 0  ,    7  , 11 ,    40  , 28887  , 0, 0, AbilMask(MonAbil::UNDEAD) | AbilMask(MonAbil::FLYING) | AbilMask(MonAbil::NO_ENEMY_RETALIATION) },
/* Vampire Lord     */ { 12 , 50  ,    9  , 9  , 6  , 10 , 0  ,    9  , 13 ,    50  , 33894  , 0, 0, AbilMask(MonAbil::UNDEAD) | AbilMask(MonAbil::FLYING) | AbilMask(MonAbil::NO_ENEMY_RETALIATION) | AbilMask(MonAbil::HP_DRAIN) },
/* Lich             */ { 14 , 65  ,    12 , 16 , 12 , 16 , 18 ,    6  , 5  ,    65  , 34478  , 0, 0, AbilMask(MonAbil::UNDEAD) | AbilMask(MonAbil::AREA_SHOT) },
/* Power Lich       */ { 15 , 85  ,    12 , 18 , 12 , 20 , 18 ,    8  , 6  ,    90  , 43254  , 0, 0, AbilMask(MonAbil::UNDEAD) | AbilMask(MonAbil::AREA_SHOT) },
/* Bone Dragon      */ { 18 , 250 ,    16 , 13 , 30 , 60 , 0  ,    6  , 12 ,    240 , 121385 , 0, 0, AbilMask(MonAbil::DOUBLE_HEX_SIZE) | AbilMask(MonAbil::UNDEAD) | AbilMask(MonAbil::FLYING) | AbilMask(MonAbil::UNLIMITED_RETALIATION) | AbilMask(MonAbil::BONE_BODY) },
/* Rogue            */ { 3  , 8   ,    7  , 3  , 1  , 3  , 0  ,    7  , 7  ,    5   , 5527   , 0, 0, AbilMask(MonAbil::NO_ENEMY_RETALIATION) | AbilMask(MonAbil::DIRTY_BLOW) },
/* Nomad            */ { 9  , 55  ,    11 , 8  , 5  , 8  , 0  ,    8  , 10 ,    25  , 18822  , 0, 0, AbilMask(MonAbil::DOUBLE_HEX_SIZE) },
/* Ghost            */ { 9  , 35  ,    7  , 7  , 6  , 9  , 0  ,    5  , 12 ,    30  , 22929  , 0, 0, AbilMask(MonAbil::UNDEAD) | AbilMask(MonAbil::FLYING) | AbilMask(MonAbil::HP_DRAIN) | AbilMask(MonAbil::ETHERIC_BODY)},
/* Genie            */ { 17 , 100 ,    15 , 15 , 20 , 35 , 0  ,    9  , 16 ,    200 , 98348  , 0, 0, AbilMask(MonAbil::FLYING) | AbilMask(MonAbil::IRON_MUSCLES) | AbilMask(MonAbil::NO_ENEMY_RETALIATION) | AbilMask(MonAbil::ETHER_DAMAGE) | AbilMask(MonAbil::ETHERIC_BODY) },
/* Medusa           */ { 13 , 75  ,    12 , 14 , 8  , 18 , 0  ,    6  , 8  ,    55  , 29441  , Spell::PETRIFY, 20, AbilMask(MonAbil::DOUBLE_HEX_SIZE) },
/* Earth Elemental  */ { 12 , 100 ,    11 , 11 , 8  , 14 , 0  ,    4  , 6  ,    50  , 29349  , 0, 0, AbilMask(MonAbil::ELEMENTAL) | AbilMask(MonAbil::IMMUNE_TO_LIGHTNING) | AbilMask(MonAbil::IMMUNE_TO_ELEMENTALSTORM) | AbilMask(MonAbil::IRON_MUSCLES) },
/* Air Elemental    */ { 11 , 70  ,    9  , 9  , 6  , 14 , 0  ,    7  , 9  ,    35  , 26522  , 0, 0, AbilMask(MonAbil::ELEMENTAL) | AbilMask(MonAbil::IMMUNE_TO_METEORSHOWER) | AbilMask(MonAbil::IRON_MUSCLES) },
/* Fire Elemental   */ { 11 , 80  ,    8  , 8  , 9  , 14 , 0  ,    6  , 8  ,    40  , 31419  , 0, 0, AbilMask(MonAbil::ELEMENTAL) | AbilMask(MonAbil::IMMUNE_TO_FIRE_SPELLS) | AbilMask(MonAbil::IRON_MUSCLES) },
/* Water Elemental  */ { 12 , 90  ,    7  , 10 , 7  , 14 , 0  ,    5  , 7  ,    45  , 32252  , 0, 0, AbilMask(MonAbil::ELEMENTAL) | AbilMask(MonAbil::IMMUNE_TO_COLD_SPELLS) | AbilMask(MonAbil::IRON_MUSCLES) },
/* Random Monster   */ { 0  , 0   ,    0  , 0  , 0  , 0  , 0  ,    0  , 0  ,    0   , 0      , 0, 0, 0 },
/* Random Monster 1 */ { 0  , 0   ,    0  , 0  , 0  , 0  , 0  ,    0  , 0  ,    0   , 0      , 0, 0, 0 },
/* Random Monster 2 */ { 0  , 0   ,    0  , 0  , 0  , 0  , 0  ,    0  , 0  ,    0   , 0      , 0, 0, 0 },
/* Random Monster 3 */ { 0  , 0   ,    0  , 0  , 0  , 0  , 0  ,    0  , 0  ,    0   , 0      , 0, 0, 0 },
/* Random Monster 4 */ { 0  , 0   ,    0  , 0  , 0  , 0  , 0  ,    0  , 0  ,    0   , 0      , 0, 0, 0 }
    };

    const MonsterName g_monsterNames[Monster::MONSTER_COUNT] = {
        { "Unknown Monster", "Unknown Monsters" },
        { "Peasant", "Peasants" },
        { "Archer", "Archers" },
        { "Ranger", "Rangers" },
        { "Pikeman", "Pikemen" },
        { "Veteran Pikeman", "Veteran Pikemen" },
        { "Swordsman", "Swordsmen" },
        { "Master Swordsman", "Master Swordsmen" },
        { "Cavalry", "Cavalries" },
        { "Champion", "Champions" },
        { "Paladin", "Paladins" },
        { "Crusader", "Crusaders" },
        { "Goblin", "Goblins" },
        { "Orc", "Orcs" },
        { "Orc Chief", "Orc Chiefs" },
        { "Wolf", "Wolves" },
        { "Ogre", "Ogres" },
        { "Ogre Lord", "Ogre Lords" },
        { "Troll", "Trolls" },
        { "War Troll", "War Trolls" },
        { "Cyclops", "Cyclopes" },
        { "Sprite", "Sprites" },
        { "Dwarf", "Dwarves" },
        { "Battle Dwarf", "Battle Dwarves" },
        { "Elf", "Elves" },
        { "Grand Elf", "Grand Elves" },
        { "Druid", "Druids" },
        { "Greater Druid", "Greater Druids" },
        { "Unicorn", "Unicorns" },
        { "Phoenix", "Phoenixes" },
        { "Centaur", "Centaurs" },
        { "Gargoyle", "Gargoyles" },
        { "Griffin", "Griffins" },
        { "Minotaur", "Minotaurs" },
        { "Minotaur King", "Minotaur Kings" },
        { "Hydra", "Hydras" },
        { "Green Dragon", "Green Dragons" },
        { "Red Dragon", "Red Dragons" },
        { "Black Dragon", "Black Dragons" },
        { "Halfling", "Halflings" },
        { "Boar", "Boars" },
        { "Iron Golem", "Iron Golems" },
        { "Steel Golem", "Steel Golems" },
        { "Roc", "Rocs" },
        { "Mage", "Magi" },
        { "Archmage", "Archmagi" },
        { "Giant", "Giants" },
        { "Titan", "Titans" },
        { "Skeleton", "Skeletons" },
        { "Zombie", "Zombies" },
        { "Mutant Zombie", "Mutant Zombies" },
        { "Mummy", "Mummies" },
        { "Royal Mummy", "Royal Mummies" },
        { "Vampire", "Vampires" },
        { "Vampire Lord", "Vampire Lords" },
        { "Lich", "Liches" },
        { "Power Lich", "Power Liches" },
        { "Bone Dragon", "Bone Dragons" },
        { "Rogue", "Rogues" },
        { "Nomad", "Nomads" },
        { "Ghost", "Ghosts" },
        { "Genie", "Genies" },
        { "Medusa", "Medusas" },
        { "Earth Elemental", "Earth Elementals" },
        { "Air Elemental", "Air Elementals" },
        { "Fire Elemental", "Fire Elementals" },
        { "Water Elemental", "Water Elementals" },
        { "Random Monster", "Random Monsters" },
        { "Random Monster 1", "Random Monsters 1" },
        { "Random Monster 2", "Random Monsters 2" },
        { "Random Monster 3", "Random Monsters 3" },
        { "Random Monster 4", "Random Monsters 4" }
    };

    const MonsterGeneralStats g_monsterGeneralStats[Monster::MONSTER_COUNT] = {
        { Race::NONE, 0, 0,  0,  { 0    , 0, 0, 0, 0, 0, 0 } },
        { Race::KNGT, 1, 20, 80, { 15   , 0, 0, 0, 0, 0, 0 } },
        { Race::KNGT, 2, 8,  30, { 125  , 0, 0, 0, 0, 0, 0 } },
        { Race::KNGT, 2, 8,  30, { 175  , 0, 0, 0, 0, 0, 0 } },
        { Race::KNGT, 3, 6,  23, { 225  , 0, 0, 0, 0, 0, 0 } },
        { Race::KNGT, 3, 6,  23, { 300  , 0, 0, 0, 0, 0, 0 } },
        { Race::KNGT, 4, 5,  18, { 380  , 0, 0, 0, 0, 0, 0 } },
        { Race::KNGT, 4, 5,  18, { 500  , 0, 0, 0, 0, 0, 0 } },
        { Race::KNGT, 5, 4,  15, { 700  , 0, 0, 0, 0, 0, 0 } },
        { Race::KNGT, 5, 4,  15, { 925  , 0, 0, 0, 0, 0, 0 } },
        { Race::KNGT, 6, 3,  12, { 1500 , 0, 0, 0, 0, 0, 0 } },
        { Race::KNGT, 6, 3,  12, { 2000 , 0, 0, 0, 0, 0, 0 } },
        { Race::BARB, 1, 13, 52, { 35   , 0, 0, 0, 0, 0, 0 } },
        { Race::BARB, 2, 8,  32, { 165  , 0, 0, 0, 0, 0, 0 } },
        { Race::BARB, 2, 8,  32, { 200  , 0, 0, 0, 0, 0, 0 } },
        { Race::BARB, 3, 6,  24, { 300  , 0, 0, 0, 0, 0, 0 } },
        { Race::BARB, 4, 5,  18, { 475  , 0, 0, 0, 0, 0, 0 } },
        { Race::BARB, 4, 5,  18, { 600  , 0, 0, 0, 0, 0, 0 } },
        { Race::BARB, 5, 4,  14, { 800  , 0, 0, 0, 0, 0, 0 } },
        { Race::BARB, 5, 4,  14, { 1000 , 0, 0, 0, 0, 0, 0 } },
        { Race::BARB, 6, 3,  10, { 2400 , 0, 0, 0, 0, 1, 0 } },
        { Race::SORC, 1, 11, 44, { 40   , 0, 0, 0, 0, 0, 0 } },
        { Race::SORC, 2, 7,  26, { 200  , 0, 0, 0, 0, 0, 0 } },
        { Race::SORC, 2, 7,  26, { 250  , 0, 0, 0, 0, 0, 0 } },
        { Race::SORC, 3, 6,  22, { 275  , 0, 0, 0, 0, 0, 0 } },
        { Race::SORC, 3, 6,  22, { 350  , 0, 0, 0, 0, 0, 0 } },
        { Race::SORC, 4, 5,  18, { 450  , 0, 0, 0, 0, 0, 0 } },
        { Race::SORC, 4, 5,  18, { 550  , 0, 0, 0, 0, 0, 0 } },
        { Race::SORC, 5, 4,  14, { 1100 , 0, 0, 0, 0, 0, 0 } },
        { Race::SORC, 6, 2,  9,  { 3000 , 0, 1, 0, 0, 0, 0 } },
        { Race::WRLK, 1, 9,  36, { 100  , 0, 0, 0, 0, 0, 0 } },
        { Race::WRLK, 2, 7,  26, { 225  , 0, 0, 0, 0, 0, 0 } },
        { Race::WRLK, 3, 5,  20, { 475  , 0, 0, 0, 0, 0, 0 } },
        { Race::WRLK, 4, 4,  15, { 550  , 0, 0, 0, 0, 0, 0 } },
        { Race::WRLK, 4, 4,  15, { 750  , 0, 0, 0, 0, 0, 0 } },
        { Race::WRLK, 5, 3,  12, { 1500 , 0, 0, 0, 0, 0, 0 } },
        { Race::WRLK, 6, 1,  4,  { 4000 , 0, 0, 0, 1, 0, 0 } },
        { Race::WRLK, 6, 1,  4,  { 6000 , 0, 0, 0, 2, 0, 0 } },
        { Race::WRLK, 6, 1,  4,  { 8000 , 0, 0, 0, 3, 0, 0 } },
        { Race::WZRD, 1, 12, 48, { 55   , 0, 0, 0, 0, 0, 0 } },
        { Race::WZRD, 2, 7,  26, { 225  , 0, 0, 0, 0, 0, 0 } },
        { Race::WZRD, 3, 5,  21, { 325  , 0, 0, 0, 0, 0, 0 } },
        { Race::WZRD, 3, 5,  21, { 425  , 0, 0, 0, 0, 0, 0 } },
        { Race::WZRD, 4, 4,  17, { 650  , 0, 0, 0, 0, 0, 0 } },
        { Race::WZRD, 5, 3,  13, { 1100 , 0, 0, 0, 0, 0, 0 } },
        { Race::WZRD, 5, 3,  13, { 1250 , 0, 0, 0, 0, 0, 0 } },
        { Race::WZRD, 6, 1,  5,  { 4400 , 0, 0, 0, 0, 0, 1 } },
        { Race::WZRD, 6, 1,  5,  { 6300 , 0, 0, 0, 0, 0, 2 } },
        { Race::NECR, 1, 10, 40, { 65   , 0, 0, 0, 0, 0, 0 } },
        { Race::NECR, 2, 6,  25, { 150  , 0, 0, 0, 0, 0, 0 } },
        { Race::NECR, 2, 6,  25, { 200  , 0, 0, 0, 0, 0, 0 } },
        { Race::NECR, 3, 5,  20, { 310  , 0, 0, 0, 0, 0, 0 } },
        { Race::NECR, 3, 5,  20, { 385  , 0, 0, 0, 0, 0, 0 } },
        { Race::NECR, 4, 4,  16, { 550  , 0, 0, 0, 0, 0, 0 } },
        { Race::NECR, 4, 4,  16, { 700  , 0, 0, 0, 0, 0, 0 } },
        { Race::NECR, 5, 3,  13, { 1000 , 0, 0, 0, 0, 0, 0 } },
        { Race::NECR, 5, 3,  13, { 1300 , 0, 0, 0, 0, 0, 0 } },
        { Race::NECR, 6, 2,  8,  { 3600 , 0, 1, 0, 0, 0, 0 } },
        { Race::NONE, 1, 10, 40, { 80   , 0, 0, 0, 0, 0, 0 } },
        { Race::NONE, 3, 5,  20, { 400  , 0, 0, 0, 0, 0, 0 } },
        { Race::NONE, 3, 5,  20, { 400  , 0, 0, 0, 0, 0, 0 } },
        { Race::NONE, 6, 2,  8,  { 3000 , 0, 0, 0, 0, 0, 1 } },
        { Race::NONE, 4, 5,  20, { 800  , 0, 0, 0, 0, 0, 0 } },
        { Race::NONE, 4, 4,  16, { 700  , 0, 0, 0, 0, 0, 0 } },
        { Race::NONE, 4, 4,  16, { 550  , 0, 0, 0, 0, 0, 0 } },
        { Race::NONE, 4, 4,  16, { 600  , 0, 0, 0, 0, 0, 0 } },
        { Race::NONE, 4, 4,  16, { 650  , 0, 0, 0, 0, 0, 0 } },
        { Race::NONE, 0, 0,  0,  { 0    , 0, 0, 0, 0, 0, 0 } },
        { Race::NONE, 1, 0,  0,  { 0    , 0, 0, 0, 0, 0, 0 } },
        { Race::NONE, 2, 0,  0,  { 0    , 0, 0, 0, 0, 0, 0 } },
        { Race::NONE, 3, 0,  0,  { 0    , 0, 0, 0, 0, 0, 0 } },
        { Race::NONE, 4, 0,  0,  { 0    , 0, 0, 0, 0, 0, 0 } }
    };

    void removeDuplicateSpell( std::set<int> & sortedSpellIds, const int massSpellId, const int spellId )
    {
        if ( sortedSpellIds.count( massSpellId ) > 0 && sortedSpellIds.count( spellId ) > 0 ) {
            sortedSpellIds.erase( massSpellId );
        }
    }

    std::vector<int> replaceMassSpells( const std::vector<int> & spellIds )
    {
        std::set<int> sortedSpellIds( spellIds.begin(), spellIds.end() );

        removeDuplicateSpell( sortedSpellIds, Spell::CHAINLIGHTNING, Spell::LIGHTNINGBOLT );
        removeDuplicateSpell( sortedSpellIds, Spell::MASSCURE, Spell::CURE );
        removeDuplicateSpell( sortedSpellIds, Spell::MASSCURSE, Spell::CURSE );
        removeDuplicateSpell( sortedSpellIds, Spell::MASSBLESS, Spell::BLESS );
        removeDuplicateSpell( sortedSpellIds, Spell::MASSHASTE, Spell::HASTE );
        removeDuplicateSpell( sortedSpellIds, Spell::MASSSHIELD, Spell::SHIELD );
        removeDuplicateSpell( sortedSpellIds, Spell::MASSDISPEL, Spell::DISPEL );
        removeDuplicateSpell( sortedSpellIds, Spell::MASSSLOW, Spell::SLOW );

        return std::vector<int>( sortedSpellIds.begin(), sortedSpellIds.end() );
    }
}

namespace fheroes2
{
    std::vector<MonAbil> MaskToAbilities(uint64_t mask)
    {
        std::vector<MonAbil> out;

        if ( 0 != (mask & AbilMask(MonAbil::DOUBLE_HEX_SIZE)))
            out.emplace_back(MonAbil::DOUBLE_HEX_SIZE);

        if ( 0 != (mask & AbilMask(MonAbil::FLYING)))
            out.emplace_back(MonAbil::FLYING);

        if ( 0 != (mask & AbilMask(MonAbil::UNDEAD)))
            out.emplace_back(MonAbil::UNDEAD);

        if ( 0 != (mask & AbilMask(MonAbil::ELEMENTAL)))
            out.emplace_back(MonAbil::ELEMENTAL);

        if ( 0 != (mask & AbilMask(MonAbil::MECHANICAL)))
            out.emplace_back(MonAbil::MECHANICAL);

        if ( 0 != (mask & AbilMask(MonAbil::DOUBLE_SHOOTING)))
            out.emplace_back(MonAbil::DOUBLE_SHOOTING);

        if ( 0 != (mask & AbilMask(MonAbil::DOUBLE_MELEE_ATTACK)))
            out.emplace_back(MonAbil::DOUBLE_MELEE_ATTACK);

        if ( 0 != (mask & AbilMask(MonAbil::TWO_CELL_MELEE_ATTACK)))
            out.emplace_back(MonAbil::TWO_CELL_MELEE_ATTACK);

        if ( 0 != (mask & AbilMask(MonAbil::AREA_SHOT)))
            out.emplace_back(MonAbil::AREA_SHOT);

        if ( 0 != (mask & AbilMask(MonAbil::ALL_ADJACENT_MELEE_ATTACK)))
            out.emplace_back(MonAbil::ALL_ADJACENT_MELEE_ATTACK);

        if ( 0 != (mask & AbilMask(MonAbil::UNLIMITED_RETALIATION)))
            out.emplace_back(MonAbil::UNLIMITED_RETALIATION);

        if ( 0 != (mask & AbilMask(MonAbil::NO_ENEMY_RETALIATION)))
            out.emplace_back(MonAbil::NO_ENEMY_RETALIATION);

        if ( 0 != (mask & AbilMask(MonAbil::NO_MELEE_PENALTY)))
            out.emplace_back(MonAbil::NO_MELEE_PENALTY);

        if ( 0 != (mask & AbilMask(MonAbil::DOUBLE_DAMAGE_TO_UNDEAD)))
            out.emplace_back(MonAbil::DOUBLE_DAMAGE_TO_UNDEAD);

        if ( 0 != (mask & AbilMask(MonAbil::ENEMY_HALVING)))
            out.emplace_back(MonAbil::ENEMY_HALVING);

        if ( 0 != (mask & AbilMask(MonAbil::IMMUNE_TO_ALL_MAGIC)))
            out.emplace_back(MonAbil::IMMUNE_TO_ALL_MAGIC);

        if ( 0 != (mask & AbilMask(MonAbil::IMMUNE_TO_MIND_SPELLS)))
            out.emplace_back(MonAbil::IMMUNE_TO_MIND_SPELLS);

        if ( 0 != (mask & AbilMask(MonAbil::IMMUNE_TO_ELEMENTAL_SPELLS)))
            out.emplace_back(MonAbil::IMMUNE_TO_ELEMENTAL_SPELLS);

        if ( 0 != (mask & AbilMask(MonAbil::IMMUNE_TO_FIRE_SPELLS)))
            out.emplace_back(MonAbil::IMMUNE_TO_FIRE_SPELLS);

        if ( 0 != (mask & AbilMask(MonAbil::IMMUNE_TO_COLD_SPELLS)))
            out.emplace_back(MonAbil::IMMUNE_TO_COLD_SPELLS);

        if ( 0 != (mask & AbilMask(MonAbil::IMMUNE_TO_LIGHTNING)))
            out.emplace_back(MonAbil::IMMUNE_TO_LIGHTNING);

        if ( 0 != (mask & AbilMask(MonAbil::IMMUNE_TO_METEORSHOWER)))
            out.emplace_back(MonAbil::IMMUNE_TO_METEORSHOWER);

        if ( 0 != (mask & AbilMask(MonAbil::IMMUNE_TO_ELEMENTALSTORM)))
            out.emplace_back(MonAbil::IMMUNE_TO_ELEMENTALSTORM);

        if ( 0 != (mask & AbilMask(MonAbil::IMMUNE_TO_CURSE)))
            out.emplace_back(MonAbil::IMMUNE_TO_CURSE);

        if ( 0 != (mask & AbilMask(MonAbil::DWARF_MAGIC_RESISTANCE)))
            out.emplace_back(MonAbil::DWARF_MAGIC_RESISTANCE);

        if ( 0 != (mask & AbilMask(MonAbil::HP_REGENERATION)))
            out.emplace_back(MonAbil::HP_REGENERATION);

        if ( 0 != (mask & AbilMask(MonAbil::HP_DRAIN)))
            out.emplace_back(MonAbil::HP_DRAIN);

        if ( 0 != (mask & AbilMask(MonAbil::SOUL_EATER)))
            out.emplace_back(MonAbil::SOUL_EATER);

        if ( 0 != (mask & AbilMask(MonAbil::MORAL_DECREMENT)))
            out.emplace_back(MonAbil::MORAL_DECREMENT);

        if ( 0 != (mask & AbilMask(MonAbil::IRON_MUSCLES)))
            out.emplace_back(MonAbil::IRON_MUSCLES);

        if ( 0 != (mask & AbilMask(MonAbil::ETHER_DAMAGE)))
            out.emplace_back(MonAbil::ETHER_DAMAGE);

        if ( 0 != (mask & AbilMask(MonAbil::ETHER_SHOT_DAMAGE)))
            out.emplace_back(MonAbil::ETHER_SHOT_DAMAGE);

        if ( 0 != (mask & AbilMask(MonAbil::BONE_BODY)))
            out.emplace_back(MonAbil::BONE_BODY);

        if ( 0 != (mask & AbilMask(MonAbil::ETHERIC_BODY)))
            out.emplace_back(MonAbil::ETHERIC_BODY);

        if ( 0 != (mask & AbilMask(MonAbil::NATURAL_SHILD)))
            out.emplace_back(MonAbil::NATURAL_SHILD);

        if ( 0 != (mask & AbilMask(MonAbil::EXTRA_DAMAGE_TO_BIG_MONSTERS)))
            out.emplace_back(MonAbil::EXTRA_DAMAGE_TO_BIG_MONSTERS);

        if ( 0 != (mask & AbilMask(MonAbil::RESISTANCE_OF_DESTRUCTION_MAGIC)))
            out.emplace_back(MonAbil::RESISTANCE_OF_DESTRUCTION_MAGIC);

        if ( 0 != (mask & AbilMask(MonAbil::LEVEL_DAMAGE)))
            out.emplace_back(MonAbil::LEVEL_DAMAGE);

        if ( 0 != (mask & AbilMask(MonAbil::DIRTY_BLOW)))
            out.emplace_back(MonAbil::DIRTY_BLOW);

        if ( 0 != (mask & AbilMask(MonAbil::BIRD_HUNTING)))
            out.emplace_back(MonAbil::BIRD_HUNTING);

        if ( 0 != (mask & AbilMask(MonAbil::EXTRA_LUCK)))
            out.emplace_back(MonAbil::EXTRA_LUCK);

        return out;
    }

    bool MonsterBattleStats::hasAbil(const MonAbil abil) const
    {
        const int a = static_cast<int>(abil);
        assert(a >= 0 && a < 64);

        return (abils & (1ULL << a)) != 0;
    }

    const int getMonIcnId(const int monId)
    {
        assert(monId > 0 && monId < Monster::MONSTER_COUNT);
        return g_monsterIcnIds[monId];
    }

    const char* getMonBinFileName(const int monId)
    {
        assert(monId > 0 && monId < Monster::MONSTER_COUNT);
        return g_binFileName[monId];
    }

    const MonsterName& getMonName(const int monId)
    {
        assert(monId > 0 && monId < Monster::MONSTER_COUNT);
        return g_monsterNames[monId];
    }

    const MonsterSound& getMonSounds(const int monId)
    {
        assert(monId > 0 && monId < Monster::MONSTER_COUNT);
        return g_monsterSounds[monId];
    }

    const MonsterBattleStats& getMonBattleStats(const int monId)
    {
        assert(monId > 0 && monId < Monster::MONSTER_COUNT);
        return g_monsterBattleStats[monId];
    }

    const MonsterGeneralStats& getMonGeneralStats(const int monId)
    {
        assert(monId > 0 && monId < Monster::MONSTER_COUNT);
        return g_monsterGeneralStats[monId];
    }

    std::string getMonsterAbilityDescription( const MonAbil ability, const bool ignoreBasicAbilities )
    {
        switch ( ability ) {
        case MonAbil::DOUBLE_SHOOTING:
            return _( "Double shot" );
        case MonAbil::DOUBLE_HEX_SIZE:
            return ignoreBasicAbilities ? "" : _( "2-hex monster" );
        case MonAbil::DOUBLE_MELEE_ATTACK:
            return _( "Double strike" );
        case MonAbil::DOUBLE_DAMAGE_TO_UNDEAD:
            return _( "Double damage to Undead" );
        case MonAbil::DWARF_MAGIC_RESISTANCE:
            return std::to_string( 25 ) + _( "% magic resistance" );
        case MonAbil::IMMUNE_TO_MIND_SPELLS:
            return _( "Immune to Mind spells" );
        case MonAbil::IMMUNE_TO_ELEMENTAL_SPELLS:
            return _( "Immune to Elemental spells" );
        case MonAbil::IMMUNE_TO_FIRE_SPELLS:
            return _( "Immune to Fire spells" );
        case MonAbil::IMMUNE_TO_COLD_SPELLS:
            return _( "Immune to Cold spells" );
        case MonAbil::HP_REGENERATION:
            return _( "HP regeneration" );
        case MonAbil::TWO_CELL_MELEE_ATTACK:
            return _( "Two hexes attack" );
        case MonAbil::FLYING:
            return ignoreBasicAbilities ? "" : _( "Flyer" );
        case MonAbil::MECHANICAL:
            return std::to_string( 50 ) + _( "% damage from Elemental spells" ) + _( " and " ) + Spell( Spell::ARMAGEDDON ).GetName();
        case MonAbil::UNLIMITED_RETALIATION:
            return _( "Unlimited retaliation" );
        case MonAbil::ALL_ADJACENT_MELEE_ATTACK:
            return _( "Attacks all adjacent enemies" );
        case MonAbil::NO_MELEE_PENALTY:
            return _( "No melee penalty" );
        case MonAbil::UNDEAD:
            return _( "Undead" );
        case MonAbil::NO_ENEMY_RETALIATION:
            return _( "No enemy retaliation" );
        case MonAbil::HP_DRAIN:
            return _( "HP drain" );
        case MonAbil::AREA_SHOT:
            return _( "Cloud attack" );
        case MonAbil::MORAL_DECREMENT:
            return _( "Decreases enemy's morale by " ) + std::to_string( 1 );
        case MonAbil::ENEMY_HALVING:
            return std::to_string( 10 ) + _( "% chance to halve enemy" );
        case MonAbil::SOUL_EATER:
            return _( "Soul Eater" );
        case MonAbil::ELEMENTAL:
            return ignoreBasicAbilities ? _( "No Morale" ) : _( "Elemental" );

        // new abilities
        case MonAbil::IRON_MUSCLES :
            return _( "Iron muscles" );
        case MonAbil::IMMUNE_TO_ALL_MAGIC :
            return _( "Magic immunity" );
        case MonAbil::IMMUNE_TO_LIGHTNING:
            return std::string( _( "Immune to " ) ) + Spell( Spell::LIGHTNINGBOLT ).GetName();
        case MonAbil::IMMUNE_TO_METEORSHOWER:
            return std::string( _( "Immune to " ) ) + Spell( Spell::METEORSHOWER ).GetName();
        case MonAbil::IMMUNE_TO_ELEMENTALSTORM:
            return std::string( _( "Immune to " ) ) + Spell( Spell::ELEMENTALSTORM ).GetName();
        case MonAbil::IMMUNE_TO_CURSE:
            return std::string( _( "Immune to " ) ) + Spell( Spell::CURSE ).GetName();

        case MonAbil::ETHER_DAMAGE:
            return _( "Ether damage by 15 hp" );
        case MonAbil::ETHER_SHOT_DAMAGE:
            return _( "Ether shot damage by 10 hp" );
        case MonAbil::BONE_BODY:
            return _( "Reduces obtained physical damage by 33%" );
        case MonAbil::ETHERIC_BODY:
            return _( "Reduces physical damage by 25% and increases magic resistance by 33%" );
        case MonAbil::NATURAL_SHILD:
            return _( "Reduces obtained shooting damage by 40%" );
        case MonAbil::EXTRA_DAMAGE_TO_BIG_MONSTERS:
            return _( "Large monsters take 40% more damage" );
        case MonAbil::RESISTANCE_OF_DESTRUCTION_MAGIC:
            return _( "Damage of destraction magic reduced by 50%" );
        case MonAbil::LEVEL_DAMAGE:
            return _( "Damage to lower-level monsters is increased by 5% per level" );
        case MonAbil::DIRTY_BLOW:
            return _( "Damage is increased by 33% if enemy troops can not respond" );
        case MonAbil::BIRD_HUNTING:
            return _( "Flying monsters take 50% more damage when shot arraws" );
        case MonAbil::EXTRA_LUCK:
            return _( "Increased Luck: +2" );

        default:
            break;
        }

        assert( 0 ); // Did you add a new ability? Please add the implementation!
        return "";
    }

    std::string getMonsterDescription( const int monsterId )
    {
        assert( monsterId > 0 && monsterId < Monster::MONSTER_COUNT );
        if ( monsterId < 0 || monsterId >= Monster::MONSTER_COUNT ) {
            return "";
        }

        auto& battleStats = getMonBattleStats(monsterId);
        auto& generalStats = getMonGeneralStats(monsterId);
        auto& names = getMonName(monsterId);

        std::ostringstream os;
        os << "----------" << std::endl;
        os << "Name: " << names.untranslated << std::endl;
        os << "Plural name: " << names.untranslatedPlural << std::endl;
        os << "Base growth: " << generalStats.baseGrowth << std::endl;
        os << "Race: " << Race::String( generalStats.race ) << std::endl;
        os << "Level: " << generalStats.level << std::endl;
        os << "Cost: " << Funds( generalStats.cost ).String() << std::endl;
        os << std::endl;
        os << "Battle level: " << battleStats.level << std::endl;
        os << "Attack: " << battleStats.attack << std::endl;
        os << "Defense: " << battleStats.defense << std::endl;
        os << "Min damage: " << battleStats.damageMin << std::endl;
        os << "Max damage: " << battleStats.damageMax << std::endl;
        os << "Hit Points: " << battleStats.hp << std::endl;
        os << "Speed: " << std::to_string/*Speed::String*/( battleStats.speed ) << std::endl;
        os << "Priority: " << std::to_string( battleStats.priority ) << std::endl;
        os << "Number of shots: " <<battleStats.shots << std::endl;
        if ( battleStats.abils ) {
            os << std::endl;
            os << "Abilities:" << std::endl;
            for ( const MonAbil & ability : MaskToAbilities( battleStats.abils ) ) {
                os << "   " << getMonsterAbilityDescription( ability, false ) << std::endl;
            }
        }
        if (battleStats.spellCast > 0) {
            os << ( std::to_string( battleStats.spellPercent ) + _( "% chance to cast " ) + Spell( battleStats.spellCast ).GetName() ) << std::endl;
        }

        return os.str();
    }

    std::vector<std::string> getMonsterPropertiesDescription( const int monsterId )
    {
        std::vector<std::string> output;

        /*
        const auto listSpells = []( const std::vector<int> & sortedSpells ) {
            std::string result;

            for ( size_t i = 0; i < sortedSpells.size(); ++i ) {
                if ( i > 0 ) {
                    result += ", ";
                }

                if ( sortedSpells[i] == Spell::LIGHTNINGBOLT ) {
                    result += _( "Lightning" );
                }
                else {
                    result += Spell( sortedSpells[i] ).GetName();
                }
            }

            result += '.';

            return result;
        };
        */

        const MonsterBattleStats & battleStats = getMonBattleStats( monsterId );

        for ( const MonAbil & ability : MaskToAbilities( battleStats.abils ) ) {
            if ( const std::string description = getMonsterAbilityDescription( ability, true ); !description.empty() ) {
                output.emplace_back( description + '.' );
            }
        }

        if (battleStats.spellCast > 0) {
            output.emplace_back( std::to_string( battleStats.spellPercent ) + _( "% chance to cast " ) + Spell( battleStats.spellCast ).GetName() );
        }

        return output;
    }

    uint32_t getSpellResistance( const int monsterId, const int spellId )
    {
        const auto & battleStats = getMonBattleStats( monsterId );

        Spell spell( spellId );

        if ( spell.isMindInfluence() ) {
            if ( battleStats.hasAbil( MonAbil::IMMUNE_TO_MIND_SPELLS ) ) {
                return 100;
            }

            if ( battleStats.hasAbil( MonAbil::UNDEAD ) ) {
                return 100;
            }

            if ( battleStats.hasAbil( MonAbil::ELEMENTAL ) ) {
                return 100;
            }
        }

        if ( spell.isAliveOnly() && battleStats.hasAbil( MonAbil::UNDEAD ) ) {
            return 100;
        }

        if ( spell.isUndeadOnly() && !battleStats.hasAbil( MonAbil::UNDEAD ) ) {
            return 100;
        }

        if ( spell.isCold() && battleStats.hasAbil( MonAbil::IMMUNE_TO_COLD_SPELLS ) ) {
            return 100;
        }

        if ( spell.isFire() && battleStats.hasAbil( MonAbil::IMMUNE_TO_FIRE_SPELLS ) ) {
            return 100;
        }

        if ( spell.isLightning() && battleStats.hasAbil( MonAbil::IMMUNE_TO_LIGHTNING ) ) {
            return 100;
        }

        if ( (spellId == Spell::ELEMENTALSTORM) && battleStats.hasAbil( MonAbil::IMMUNE_TO_ELEMENTALSTORM ) ) {
            return 100;
        }

        if ( spell.isElementalSpell() && battleStats.hasAbil( MonAbil::IMMUNE_TO_ELEMENTAL_SPELLS ) ) {
            return 100;
        }

        if ( (spellId == Spell::METEORSHOWER) && battleStats.hasAbil( MonAbil::IMMUNE_TO_METEORSHOWER ) ) {
            return 100;
        }

        if ( spell.isCurse() && battleStats.hasAbil( MonAbil::IMMUNE_TO_CURSE ) ) {
            return 100;
        }

        if ( battleStats.hasAbil( MonAbil::IMMUNE_TO_ALL_MAGIC ) ) {
            return 100;
        }

        if ( battleStats.hasAbil( MonAbil::DWARF_MAGIC_RESISTANCE ) ) {
            if ( spell.isDamage() || spell.isApplyToEnemies() ) {
                return 25;
            }
        }

        if ( battleStats.hasAbil( MonAbil::ETHERIC_BODY ) ) {
            if ( spell.isDamage() ) {
                return 33;
            }
        }

        return 0;
    }
}
