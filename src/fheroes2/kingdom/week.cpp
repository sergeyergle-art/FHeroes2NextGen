/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2019 - 2025                                             *
 *                                                                         *
 *   Free Heroes2 Engine: http://sourceforge.net/projects/fheroes2         *
 *   Copyright (C) 2009 by Andrey Afletdinov <fheroes2@gmail.com>          *
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

#include "week.h"

#include <cassert>
#include <ostream>

#include "rand.h"
#include "translations.h"
#include "serialize.h"

namespace
{
    Monster::MonsterType RandomMonsterWeekOf( const uint32_t seed )
    {
        return Rand::GetWithSeed( Monster::PEASANT, Monster::BONE_DRAGON, Rand::combineSeedWithValue(seed, 886473) ); // Salt
    }

    Monster::MonsterType RandomMonsterMonthOf( const uint32_t seed )
    {
        uint32_t weight = Rand::GetWithSeed( 1, 12, Rand::combineSeedWithValue(seed, 1130906) ); // Salt

        switch ( weight ) {
        case 1:
            return Monster::PEASANT;
        case 2:
            return Monster::WOLF;
        case 3:
            return Monster::OGRE;
        case 4:
            return Monster::TROLL;
        case 5:
            return Monster::DWARF;
        case 6:
            return Monster::DRUID;
        case 7:
            return Monster::UNICORN;
        case 8:
            return Monster::CENTAUR;
        case 9:
            return Monster::GARGOYLE;
        case 10:
            return Monster::ROC;
        case 11:
            return Monster::VAMPIRE;
        case 12:
            return Monster::LICH;
        default:
            assert( 0 );
        }

        return Monster::UNKNOWN;
    }
}

const char * WeekInfo::GetName() const
{
    switch ( _id )
    {
    case WeekId::UNNAMED:
        break;

    case WeekId::SQUIRREL:
        return _( "week|Squirrel" );
    case WeekId::RABBIT:
        return _( "week|Rabbit" );
    case WeekId::GOPHER:
        return _( "week|Gopher" );
    case WeekId::BADGER:
        return _( "week|Badger" );
    case WeekId::RAT:
        return _( "week|Rat" );
    case WeekId::EAGLE:
        return _( "week|Eagle" );
    case WeekId::WEASEL:
        return _( "week|Weasel" );
    case WeekId::RAVEN:
        return _( "week|Raven" );
    case WeekId::MONGOOSE:
        return _( "week|Mongoose" );
    case WeekId::DOG:
        return _( "week|Dog" );
    case WeekId::AARDVARK:
        return _( "week|Aardvark" );
    case WeekId::LIZARD:
        return _( "week|Lizard" );
    case WeekId::TORTOISE:
        return _( "week|Tortoise" );
    case WeekId::HEDGEHOG:
        return _( "week|Hedgehog" );
    case WeekId::CONDOR:
        return _( "week|Condor" );

    case WeekId::ANT:
        return _( "week|Ant" );
    case WeekId::GRASSHOPPER:
        return _( "week|Grasshopper" );
    case WeekId::DRAGONFLY:
        return _( "week|Dragonfly" );
    case WeekId::SPIDER:
        return _( "week|Spider" );
    case WeekId::BUTTERFLY:
        return _( "week|Butterfly" );
    case WeekId::BUMBLEBEE:
        return _( "week|Bumblebee" );
    case WeekId::LOCUST:
        return _( "week|Locust" );
    case WeekId::EARTHWORM:
        return _( "week|Earthworm" );
    case WeekId::HORNET:
        return _( "week|Hornet" );
    case WeekId::BEETLE:
        return _( "week|Beetle" );

    case WeekId::MONSTERS:
        return Monster( _monster ).GetName();

    case WeekId::PLAGUE:
        return _( "week|PLAGUE" );

    default:
        assert( 0 );
    }

    return _( "week|Unnamed" );
}

void WeekInfo::ChangeWeek(const bool newMonth, const uint32_t seed)
{
    if ( !newMonth ) {
        uint32_t weight = Rand::GetWithSeed( 0, 3, Rand::combineSeedWithValue( seed, 367245 ) ); // Salt

        // A regular week, probability 75%
        if ( weight < 3 ) {
            _id = Rand::GetWithSeed( WeekId::SQUIRREL, WeekId::CONDOR, Rand::combineSeedWithValue( seed, 1946256 ) );
            _monster = Monster::UNKNOWN;
        }
        else {
            _id = WeekId::MONSTERS;
            _monster = RandomMonsterWeekOf( seed );
        }
    }
    else {
        uint32_t weight = Rand::GetWithSeed( 0, 9, Rand::combineSeedWithValue( seed, 9536582 ) ); // Salt

        // A regular month, probability 50%
        if ( weight < 5 ) {
            _id = Rand::GetWithSeed( WeekId::ANT, WeekId::BEETLE, Rand::combineSeedWithValue( seed, 5544783 ) ); // Salt
            _monster = Monster::UNKNOWN;
        }
        // The Month of a monster, probability 40%
        else if (weight < 9) {
            _id = WeekId::MONSTERS;
            _monster = RandomMonsterMonthOf( seed );
        }
        // The Month of the Plague, probability 10%
        else {
            _id = WeekId::PLAGUE;
            _monster = Monster::UNKNOWN;
        }
    }
}

OStreamBase & operator<<( OStreamBase & stream, const WeekInfo & w )
{
    return stream << w._id << w._monster;
}

IStreamBase & operator>>( IStreamBase & stream, WeekInfo & w )
{
    stream >> w._id >> w._monster;

    return stream;
}
