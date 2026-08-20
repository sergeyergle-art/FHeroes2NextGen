/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2019 - 2026                                             *
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

#include "luck.h"

#include <algorithm>

#include "tools.h"
#include "translations.h"

bool Luck::IsMaximum(const int luck)
{
    return luck >= MAX_LUCK;
}

bool Luck::IsMinimum(const int luck)
{
    return luck <= MIN_LUCK;
}

bool Luck::IsCursed(const int luck)
{
    return luck < -10;
}

bool Luck::IsAwful(const int luck)
{
    return luck >= -10 && luck < -5;
}

bool Luck::IsBad(const int luck)
{
    return luck >= -5 && luck < 0;
}

bool Luck::IsGood(const int luck)
{
    return luck > 0 && luck <= 5;
}

bool Luck::IsGreat(const int luck)
{
    return luck > 5 && luck <= 10;
}

bool Luck::IsIrish(const int luck)
{
    return luck > 10;
}

bool Luck::LessGreat(const int luck)
{
    return luck <= 5;
}

std::string Luck::String( int luck )
{
    const char* txt = "Unknown";

    if (luck > 0) {
        if (luck <= 5)
            txt = "luck|Good";
        else if (luck <= 10)
            txt = "luck|Great";
        else
            txt = "luck|Irish";
    }
    else if (luck < 0) {
        if (luck >= -5)
            txt = "luck|Bad";
        else if (luck >= -10)
            txt = "luck|Awful";
        else
            txt = "luck|Cursed";
    }
    else {
        txt = "luck|Normal";
    }

    return _( txt );
}

std::string Luck::Description( int luck )
{
    std::string msg;

    if (luck > 0) {
        msg = _( "%{luck-type} luck sometimes lets the hero's units get lucky attacks (double strength) in combat." );
    }
    else if (luck < 0) {
        msg = _( "%{luck-type} luck sometimes falls on the hero's units in combat, causing their attacks to only do half damage." );
    }
    else {
        msg = _( "%{luck-type} luck means the hero's units will never get lucky or unlucky attacks on the enemy." );
    }

    StringReplace( msg, "%{luck-type}", Luck::String( luck ) );

    return msg;
}

int Luck::Normalize( const int luck )
{
    return std::clamp( luck, Luck::MIN_LUCK, Luck::MAX_LUCK );
}

