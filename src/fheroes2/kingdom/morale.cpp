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

#include "morale.h"

#include <algorithm>

#include "tools.h"
#include "translations.h"

bool Morale::IsMaximum(const int morale)
{
    return morale >= MAX_MORALE;
}

bool Morale::IsMinimum(const int morale)
{
    return morale <= MIN_MORALE;
}

bool Morale::IsTreason(const int morale)
{
    return morale < -10;
}

bool Morale::IsAwful(const int morale)
{
    return morale >= -10 && morale < -5;
}

bool Morale::IsPoor(const int morale)
{
    return morale >= -5 && morale < 0;
}

bool Morale::IsGood(const int morale)
{
    return morale > 0 && morale <= 5;
}

bool Morale::IsGreat(const int morale)
{
    return morale > 5 && morale <= 10;
}

bool Morale::IsBlood(const int morale)
{
    return morale > 10;
}

bool Morale::LessGreat(const int morale)
{
    return morale <= 5;
}

std::string Morale::String( int morale )
{
    const char* txt = "Unknown";

    if (morale > 0) {
        if (morale <= 5)
            txt = "morale|Good";
        else if (morale <= 10)
            txt = "morale|Great";
        else
            txt = "morale|Blood!";
    }
    else if (morale < 0) {
        if (morale >= -5)
            txt = "morale|Poor";
        else if (morale >= -10)
            txt = "morale|Awful";
        else
            txt = "morale|Treason";
    }
    else {
        txt = "morale|Normal";
    }

    return _( txt );
}

std::string Morale::Description( int morale )
{
    std::string msg;

    if (morale > 0) {
        msg = _( "%{morale-type} morale may give the hero's units extra attacks in combat." );
    }
    else if (morale < 0) {
        msg = _( "%{morale-type} morale may cause the hero's units to freeze in combat." );
    }
    else {
        msg = _( "%{morale-type} morale means the hero's units will never be blessed with extra attacks or freeze in combat." );
    }

    StringReplace( msg, "%{morale-type}", !IsBlood(morale) ? Morale::String( morale ) : _( "Blood" ) );

    return msg;
}

int Morale::Normalize( const int morale )
{
    return std::clamp( morale, Morale::MIN_MORALE, Morale::MAX_MORALE );
}
