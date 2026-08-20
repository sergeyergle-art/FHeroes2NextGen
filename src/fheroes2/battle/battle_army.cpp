/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2019 - 2025                                             *
 *                                                                         *
 *   Free Heroes2 Engine: http://sourceforge.net/projects/fheroes2         *
 *   Copyright (C) 2012 by Andrey Afletdinov <fheroes2@gmail.com>          *
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

#include "battle_army.h"

#include <algorithm>
#include <cstddef>
#include <numeric>

#include "artifact.h"
#include "artifact_info.h"
#include "battle_arena.h"
#include "battle_cell.h"
#include "battle_troop.h"
#include "heroes.h"
#include "heroes_base.h"
#include "monster_anim.h"
#include "resource.h"
#include "skill.h"

namespace
{
    const size_t unitSizeCapacity = 16;
}

Battle::Units::Units()
{
    reserve( unitSizeCapacity );
}

void Battle::Units::SortFastest()
{
    // It is important to maintain the initial order of units having the same speed for the proper operation of the unit turn queue
    std::stable_sort( begin(), end(), Army::FastestTroop );
}

Battle::Unit * Battle::Units::FindUID( const uint32_t uid ) const
{
    const auto iter = std::find_if( begin(), end(), [uid]( const Unit * unit ) { return unit->isUID( uid ); } );

    return iter == end() ? nullptr : *iter;
}

Battle::Unit * Battle::Units::FindMode( const uint32_t mod ) const
{
    const auto iter = std::find_if( begin(), end(), [mod]( const Unit * unit ) { return unit->Modes( mod ); } );

    return iter == end() ? nullptr : *iter;
}

Battle::Force::Force( Army & parent, bool opposite, TroopsUidGenerator & generator )
    : army( parent )
{
    uids.reserve( army.Size() );

    int troopCell = Board::FirstRowCellIdx( Board::GetFirstTroopsRow( army.isSpreadFormation() ), opposite );
    int troopCellInc = army.isSpreadFormation() ? Board::widthInCells * 2 : Board::widthInCells;

    for ( size_t i = 0; i < army.Size(); ++i, troopCell += troopCellInc ) {
        const Troop * troop = army.GetTroop( i );

        if ( troop == nullptr || !troop->isValid() ) {
            uids.push_back( 0 );

            continue;
        }

        int32_t idx = troopCell;
        bool wideTroop = troop->isWide();
        if ( wideTroop ) {
            idx += !opposite ? 1 : -1;
        }

        Position pos;
        pos.Set( idx, wideTroop, opposite );

        assert( pos.GetHead() != nullptr && ( wideTroop ? pos.GetTail() != nullptr : pos.GetTail() == nullptr ) );

        push_back( new Unit( *troop, pos, opposite, generator.GetUnique() ) );
        back()->SetArmy( army );

        uids.push_back( back()->GetUID() );
    }
}

Battle::Force::~Force()
{
    std::for_each( begin(), end(), []( Unit * unit ) {
        assert( unit != nullptr );

        delete unit;
    } );
}

const HeroBase * Battle::Force::GetCommander() const
{
    return army.GetCommander();
}

HeroBase * Battle::Force::GetCommander()
{
    return army.GetCommander();
}

const Battle::Units & Battle::Force::getUnits() const
{
    return *this;
}

PlayerColor Battle::Force::GetColor() const
{
    return army.GetColor();
}

int Battle::Force::GetControl() const
{
    return army.GetControl();
}

bool Battle::Force::isValid( const bool considerBattlefieldArmy /* = true */ ) const
{
    // Consider the state of the army on the battlefield (including resurrected units, summoned units, etc)
    if ( considerBattlefieldArmy ) {
        return std::any_of( begin(), end(), []( const Unit * unit ) { return unit->isValid(); } );
    }

    // Consider only the state of the original army
    for ( size_t index = 0; index < army.Size(); ++index ) {
        const Troop * troop = army.GetTroop( index );
        if ( troop == nullptr || !troop->isValid() ) {
            continue;
        }

        const Unit * unit = FindUID( uids.at( index ) );
        if ( unit == nullptr || unit->GetDead() >= unit->GetMaxCount() ) {
            continue;
        }

        return true;
    }

    return false;
}

uint32_t Battle::Force::GetSurrenderCost() const
{
    double result = 0.0;

    // Consider only the units from the original army
    for ( size_t index = 0; index < army.Size(); ++index ) {
        const Troop * troop = army.GetTroop( index );
        if ( troop == nullptr || !troop->isValid() ) {
            continue;
        }

        const Unit * unit = FindUID( uids.at( index ) );
        if ( unit == nullptr || !unit->isValid() ) {
            continue;
        }

        result += unit->GetSurrenderCost().gold;
    }

    const HeroBase * commander = GetCommander();
    if ( commander ) {
        double mod = Skill::GetDiplomacySurrenderCostDiscount( commander->GetLevelSkill( Skill::Secondary::DIPLOMACY ) ) / 100.0;

        auto costReductionPercent = commander->GetBagArtifacts().getTotalArtifactMultipliedPercent( fheroes2::ArtifactBonusType::SURRENDER_COST_REDUCTION_PERCENT );
        if ( !costReductionPercent.empty() ) {
            double modEx = 1;
            for ( const int32_t value : costReductionPercent ) {
                modEx *= value / 100.0;
            }
            mod *= modEx;
        }

        result *= mod;
    }

    int32_t cost = static_cast<int32_t>( result + 0.5 );

    // Total cost should always be at least 1 gold
    return std::max(cost, 1);
}

void Battle::Force::NewTurn()
{
    if ( GetCommander() ) {
        GetCommander()->ResetModes( Heroes::SPELLCASTED );
    }

    std::for_each( begin(), end(), []( Unit * unit ) { unit->NewTurn(); } );
}

Troops Battle::Force::GetKilledTroops() const
{
    Troops result;

    for ( const Unit * unit : *this ) {
        assert( unit != nullptr );

        result.PushBack( *unit, std::min( unit->GetInitialCount(), unit->GetDead() ) );
    }

    return result;
}

bool Battle::Force::animateIdleUnits() const
{
    bool redrawNeeded = false;

    for ( Unit * unit : *this ) {
        // Check if unit is alive.
        if ( unit->isValid() ) {
            if ( unit->isIdling() ) {
                // Go to 'STATIC' animation state if idle animation is over or if unit is blinded or paralyzed.
                if ( unit->isFinishAnimFrame() || unit->isImmovable() ) {
                    redrawNeeded = unit->SwitchAnimation( Monster_Info::STATIC ) || redrawNeeded;
                }
                else {
                    unit->IncreaseAnimFrame();
                    redrawNeeded = true;
                }
            }
            // checkIdleDelay() sets and checks unit's internal timer if we're ready to switch to next one.
            // Do not start idle animations for paralyzed or blinded units.
            else if ( unit->GetAnimationState() == Monster_Info::STATIC && !unit->isImmovable() && unit->checkIdleDelay() ) {
                redrawNeeded = unit->SwitchAnimation( Monster_Info::IDLE ) || redrawNeeded;
            }
        }
    }
    return redrawNeeded;
}

void Battle::Force::resetIdleAnimation() const
{
    for ( Unit * unit : *this ) {
        // Check if unit is alive, not paralyzed or blinded and is in 'STATIC' animation state.
        if ( unit->isValid() && unit->GetAnimationState() == Monster_Info::STATIC && !unit->isImmovable() ) {
            unit->checkIdleDelay();
        }
    }
}

bool Battle::Force::HasMonster( const Monster & mons ) const
{
    return std::any_of( begin(), end(), [&mons]( const Unit * unit ) { return unit->isMonster( mons.GetID() ); } );
}

uint32_t Battle::Force::getTotalNumberOfDeadUnits() const
{
    return std::accumulate( begin(), end(), static_cast<uint32_t>( 0 ), []( const uint32_t total, const Battle::Unit * unit ) {
        assert( unit != nullptr );

        return total + unit->GetDead();
    } );
}

uint32_t Battle::Force::calculateNumberOfDeadUnitsForNecromancy() const
{
    return std::accumulate( begin(), end(), static_cast<uint32_t>( 0 ), []( const uint32_t total, const Battle::Unit * unit ) {
        assert( unit != nullptr );

        return total + std::min( unit->GetInitialCount(), unit->GetDead() );
    } );
}

uint32_t Battle::Force::calculateExperienceBasedOnLosses() const
{
    uint32_t result = 0;

    _applyActionToTroopsFromOriginalArmy(
        [&result]( const Troop &, const Unit & unit ) { result += unit.Monster::GetExperience() * std::min( unit.GetInitialCount(), unit.GetDead() ); } );

    return result;
}

void Battle::Force::syncOriginalArmy() const
{
    _applyActionToTroopsFromOriginalArmy(
        []( Troop & troop, const Unit & unit ) { troop.SetCount( unit.GetDead() >= unit.GetMaxCount() ? 0 : unit.GetMaxCount() - unit.GetDead() ); } );
}
