/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2019 - 2025                                             *
 *                                                                         *
 *   Free Heroes2 Engine: http://sourceforge.net/projects/fheroes2         *
 *   Copyright (C) 2010 by Andrey Afletdinov <fheroes2@gmail.com>          *
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
#include <map>
#include <vector>

#include "battle.h"
#include "battle_board.h"
#include "math_base.h"

class Castle;
class HeroBase;

namespace Rand
{
    class PCG32;
}

namespace Battle
{
    constexpr int POS_CATAPULT = Board::CellIdxLeft( Board::catapultRow );

    class CastleFortress
    {
    public:
        static constexpr int POS_UPPER_WALL_1 = Board::CellIdxRight( 1 ) - 2;
        static constexpr int POS_UPPER_WALL_2 = Board::CellIdxRight( 3 ) - 3;
        static constexpr int POS_LOWER_WALL_1 = Board::CellIdxRight( 7 ) - 3;
        static constexpr int POS_LOWER_WALL_2 = Board::CellIdxRight( 9 ) - 2;
        static constexpr int POS_UPPER_TOWER  = Board::CellIdxRight( 2 ) - 3;
        static constexpr int POS_LOWER_TOWER  = Board::CellIdxRight( 8 ) - 3;
        static constexpr int POS_GATE_TOWER_1 = Board::CellIdxRight( 4 ) - 4;
        static constexpr int POS_GATE_TOWER_2 = Board::CellIdxRight( 6 ) - 4;
        static constexpr int POS_GATE         = Board::CellIdxRight( 5 ) - 4;
        static constexpr int POS_EMPTY_TOWER  = Board::CellIdxRight( 10 ) - 2;

        static constexpr int POS_UPPER_BOUND_1 = Board::CellIdxRight( 0 ) - 2;
        static constexpr int POS_UPPER_BOUND_2 = POS_UPPER_BOUND_1 + 1;
        static constexpr int POS_UPPER_BOUND_3 = POS_UPPER_BOUND_2 + 1;
        static constexpr int POS_LOWER_BOUND_1 = Board::CellIdxRight( 10 ) - 1;
        static constexpr int POS_LOWER_BOUND_2 = POS_LOWER_BOUND_1 + 1;

        enum TargetId
        {
            UPPER_WALL_1,
            UPPER_WALL_2,
            LOWER_WALL_1,
            LOWER_WALL_2,
            UPPER_TOWER,
            LOWER_TOWER,
            GATE_TOWER_1,
            GATE_TOWER_2,
            GATE,
            MAIN_TOWER,
            EMPTY_TOWER,

            ALL_TARGETS
        };

        static constexpr int MAX_CATAPULT_HITS = 4;
        static constexpr int MAX_EARTHQUAKE_HITS = ALL_TARGETS;

        struct HitInfo
        {
            TargetId stuctId;
            int hp; /* 0, 1 - for towers and bridge, 0 .. 3 - for walls */
        };

    public:
        CastleFortress();

        void Init( const Castle& castle, const HeroBase& attackingHero );

        int AttackCatapult( HitInfo hits[MAX_CATAPULT_HITS], Rand::PCG32& gen );
        int AttackEarthquake( int spellPower, HitInfo hits[MAX_EARTHQUAKE_HITS], Rand::PCG32& gen );

        double GetRelativeEarthquakeDamage( int spellPower ) const;
        static double GetRelativeCatapultPower( const HeroBase& attackingHero );

        static bool IsWall( TargetId tagId );
        static const fheroes2::Point& GetTargetPosition( TargetId tagId );
        static const int GetTargetCellIdx( TargetId tagId );

        bool HasCatapultTargets() const
        {
            return _countTargets > 0 || _countEmptyTowers > 0;
        }

        bool HasEarthquakeTargets() const
        {
            return _countTargets > 0;
        }

    protected:
        TargetId SelectTarget( Rand::PCG32& gen ) const;

    protected:
        static constexpr int MAX_EMPTY_TOWERS = 5;

        int _countTargets;
        int _countEmptyTowers;
        TargetId _lastTarget;
        int _damagePerHit;
        int _countHits;
        int _totalHp;

        int _targetHp[ALL_TARGETS]; // no need to initialize
        TargetId _emptyTowers[MAX_EMPTY_TOWERS]; // no need to initialize
    };
}
