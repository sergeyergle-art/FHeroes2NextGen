/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2019 - 2026                                             *
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

#include "battle_tower.h"

#include <algorithm>
#include <cassert>
#include <vector>

#include "army_troop.h"
#include "battle.h"
#include "battle_arena.h"
#include "battle_board.h"
#include "battle_cell.h"
#include "castle.h"
#include "monster.h"
#include "tools.h"
#include "translations.h"

Battle::Tower::Tower( const Castle & castle, const TowerType type, const uint32_t uid )
    : Unit( Troop( Monster::ARCHER, std::min<uint32_t>( castle.CountBuildings(), 20U ) ), {}, false, uid )
    , _towerType( type )
    , _attackBonus( castle.GetLevelMageGuild() )
    , _isValid( true )
{
    SetCount( std::max<uint32_t>( _towerType == TowerType::TWR_CENTER ? GetCount() : GetCount() / 2, 1U ) );

    // Virtual archers shooting from this tower should receive bonuses
    // to their attack skill from the commanding hero (if present)
    SetArmy( castle.GetActualArmy() );

    SetModes( CAP_TOWER );
}

const char * Battle::Tower::GetName() const
{
    switch ( _towerType ) {
    case TowerType::TWR_LEFT:
        return _( "Left Turret" );
    case TowerType::TWR_CENTER:
        return _( "Ballista" );
    case TowerType::TWR_RIGHT:
        return _( "Right Turret" );
    case TowerType::TWR_GATE_1:
        return _( "Left Gate Turret" );
    case TowerType::TWR_GATE_2:
        return _( "Right Gate Turret" );
    default:
        // This is not a valid Tower type!
        assert( 0 );
        break;
    }

    return nullptr;
}

bool Battle::Tower::isValid() const
{
    return _isValid;
}

Battle::TowerType Battle::Tower::GetType() const
{
    return _towerType;
}

int Battle::Tower::GetAttackBonus() const
{
    return _attackBonus;
}

int Battle::Tower::GetMonAttack() const
{
    return Unit::GetMonAttack() + _attackBonus;
}

fheroes2::Point Battle::Tower::GetPortPosition() const
{
    switch ( _towerType ) {
    case TowerType::TWR_LEFT:
        return { 555, 85 };
    case TowerType::TWR_CENTER:
        return { 735, 175 };
    case TowerType::TWR_RIGHT:
        return { 555, 335 };
    case TowerType::TWR_GATE_1:
        return { 515, 175 };
    case TowerType::TWR_GATE_2:
        return { 515, 255 };
    default:
        // This is not a valid Tower type!
        assert( 0 );
        break;
    }

    return {};
}

void Battle::Tower::SetDestroyed()
{
    _isValid = false;
}

std::string Battle::Tower::GetInfo( const Castle & castle )
{
    if ( !castle.isBuild( BUILD_CASTLE ) ) {
        return {};
    }

    std::vector<TowerType> towerTypes;

    towerTypes.push_back( TowerType::TWR_CENTER );
    if ( castle.hasUpperArcherTower() ) {
        towerTypes.push_back( TowerType::TWR_LEFT );
        towerTypes.push_back( TowerType::TWR_GATE_1 );
    }
    if ( castle.hasLowerArcherTower() ) {
        towerTypes.push_back( TowerType::TWR_RIGHT );
        towerTypes.push_back( TowerType::TWR_GATE_2 );
    }

    // This method can be called both during combat and outside of it. In the
    // former case, we have to check if the tower was destroyed during the siege.
    const auto isTowerValid = []( const TowerType towerType ) {
        // If the siege is in progress, we need to check the current state of the tower
        if ( GetArena() ) {
            const Tower * tower = Arena::GetTower( towerType );
            assert( tower != nullptr );

            return tower->isValid();
        }

        return true;
    };

    std::string msg;

    for ( std::vector<TowerType>::const_iterator it = towerTypes.begin(); it != towerTypes.end(); ++it ) {
        const TowerType towerType = *it;

        if ( isTowerValid( towerType ) ) {
            const Tower tower( castle, towerType, 0 );

            msg.append( _( "The %{name} fires with the strength of %{count} Archers" ) );
            StringReplace( msg, "%{name}", tower.GetName() );
            StringReplace( msg, "%{count}", std::to_string( tower.GetCount() ) );

            if ( tower.GetAttackBonus() ) {
                msg.append( ", " );
                msg.append( _( "each with a +%{attack} bonus to their attack skill." ) );
                StringReplace( msg, "%{attack}", std::to_string( tower.GetAttackBonus() ) );
            }
            else {
                msg += '.';
            }
        }
        else {
            assert( GetArena() != nullptr );

            const Tower * tower = Arena::GetTower( towerType );
            assert( tower != nullptr );

            msg.append( _( "The %{name} is destroyed." ) );
            StringReplace( msg, "%{name}", tower->GetName() );
        }

        if ( ( it + 1 ) != towerTypes.end() ) {
            msg.append( "\n\n" );
        }
    }

    return msg;
}
