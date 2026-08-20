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

#include <string>

namespace Luck
{
    constexpr int NORMAL = 0;
    constexpr int MIN_LUCK = -15;
    constexpr int MAX_LUCK = 15;
    constexpr int CHANCE_IN_PERCENT = 5;

    //enum
    //{
    //    UNKNOWN = -4,
    //    CURSED = -3,
    //    AWFUL = -2,
    //    BAD = -1,
    //    NORMAL = 0,
    //    GOOD = 1,
    //    GREAT = 2,
    //    IRISH = 3
    //};

    std::string String( int );
    std::string Description( int );
    int Normalize( const int luck );

    bool IsMaximum(const int luck);
    bool IsMinimum(const int luck);

    bool IsCursed(const int luck);
    bool IsAwful(const int luck);
    bool IsBad(const int luck);

    bool IsGood(const int luck);
    bool IsGreat(const int luck);
    bool IsIrish(const int luck);

    bool LessGreat(const int luck);
}
