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

#pragma once

#include <cstdint>

#include "monster.h"

class IStreamBase;
class OStreamBase;

class WeekInfo
{
public:
    enum class WeekId : uint32_t
    {
        UNNAMED,

        // A regular week (Week Of ...)
        SQUIRREL,
        RABBIT,
        GOPHER,
        BADGER,
        RAT,
        EAGLE,
        WEASEL,
        RAVEN,
        MONGOOSE,
        DOG,
        AARDVARK,
        LIZARD,
        TORTOISE,
        HEDGEHOG,
        CONDOR,

        // A regular first week of the month (Month Of ...)
        ANT,
        GRASSHOPPER,
        DRAGONFLY,
        SPIDER,
        BUTTERFLY,
        BUMBLEBEE,
        LOCUST,
        EARTHWORM,
        HORNET,
        BEETLE,

        // The Week of a monster (the Month of a monster, if it's the first week of the month)
        MONSTERS,

        // The Month of the Plague
        PLAGUE
    };

public:
    WeekInfo()
        : _id( WeekId::UNNAMED )
        , _monster( Monster::UNKNOWN )
    {}

    WeekId GetId() const
    {
        return _id;
    }

    Monster::MonsterType GetMonster() const
    {
        return _monster;
    }

    bool IsRegularWeek() const
    {
        return _id < WeekId::MONSTERS;
    }

    bool IsMonstersWeek() const
    {
        return _id == WeekId::MONSTERS;
    }

    bool IsMonsterWeek(/*Monster::MonsterType*/ int monId) const
    {
        return _monster == monId;
    }

    bool IsPlagueWeek() const
    {
        return _id == WeekId::PLAGUE;
    }

    const char * GetName() const;

    void ChangeWeek(const bool newMonth, const uint32_t seed);

private:
    friend OStreamBase & operator<<( OStreamBase & stream, const WeekInfo & w );
    friend IStreamBase & operator>>( IStreamBase & stream, WeekInfo & w );

    WeekId _id;
    Monster::MonsterType _monster;
};