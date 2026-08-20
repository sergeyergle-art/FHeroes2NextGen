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

#include "battle_board.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <set>
#include <utility>

#include "battle_arena.h"
#include "battle_bridge.h"
#include "battle_troop.h"
#include "castle.h"
#include "game_static.h"
#include "ground.h"
#include "icn.h"
#include "maps_tiles.h"
#include "mp2.h"
#include "rand.h"
#include "tools.h"
#include "translations.h"

namespace
{
    const uint8_t IS_MOAT = 1;
    const uint8_t IS_BRIDGE = 2;
    const uint8_t IS_CASTLE = 4;
    const uint8_t IS_OUT_WALLS = 8;

    static const std::array<uint8_t, Battle::Board::sizeInCells> g_castle_battle_arena = {
        /*        0  1  2  3  4  5  6  7  8  9   10  11  12  13  14 */
        /* 0   */ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,  8,  9,  8,  4,  4,
        /* 15  */ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,  8,  9,  12, 4,  4,
        /* 30  */ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,  9,  8,  4,  4,  4,
        /* 45  */ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,  9,  12, 4,  4,  4,
        /* 60  */ 8, 8, 8, 8, 8, 8, 8, 8, 8, 9,  8,  4,  4,  4,  4,
        /* 75  */ 8, 8, 8, 8, 8, 8, 8, 8, 8, 11, 12, 4,  4,  4,  4,
        /* 90  */ 8, 8, 8, 8, 8, 8, 8, 8, 8, 9,  8,  4,  4,  4,  4,
        /* 105 */ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,  9,  12, 4,  4,  4,
        /* 120 */ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,  9,  8,  4,  4,  4,
        /* 135 */ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,  8,  9,  12, 4,  4,
        /* 150 */ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,  8,  9,  8,  4,  4
    };

    uint32_t GetRandomObstaclePosition( Rand::PCG32 & gen )
    {
        return Rand::GetIndex( Battle::Board::widthInCells - 4, gen ) + 2
            + ( Battle::Board::widthInCells * Rand::GetIndex( Battle::Board::heightInCells, gen ) );
    }

    bool isTwoHexObject( const int icnId )
    {
        switch ( icnId ) {
        case ICN::COBJ0004:
        case ICN::COBJ0005:
        case ICN::COBJ0007:
        case ICN::COBJ0011:
        case ICN::COBJ0014:
        case ICN::COBJ0015:
        case ICN::COBJ0017:
        case ICN::COBJ0018:
        case ICN::COBJ0019:
        case ICN::COBJ0020:
        case ICN::COBJ0022:
        case ICN::COBJ0030:
        case ICN::COBJ0031:
            return true;

        default:
            break;
        }

        return false;
    }

    bool isTallObject( const int icnId )
    {
        switch ( icnId ) {
        case ICN::COBJ0002:
        case ICN::COBJ0009:
        case ICN::COBJ0013:
        case ICN::COBJ0021:
        case ICN::COBJ0027:
        case ICN::COBJ0028:
        case ICN::COBJ0031:
            return true;

        default:
            break;
        }

        return false;
    }
}

bool Battle::Board::IsBridgeCell( int idx )
{
    return idx == BRIDGE_IDX;
}

Battle::Board::Board()
{
    reserve( sizeInCells );

    for ( int i = 0; i < sizeInCells; ++i ) {
        emplace_back( i );
    }
}

void Battle::Board::SetArea( const fheroes2::Rect & area )
{
    std::for_each( begin(), end(), [&area]( Cell & cell ) { cell.SetArea( area ); } );
}

void Battle::Board::removeDeadUnits()
{
    for ( Cell & cell : *this ) {
        Unit * unit = cell.GetUnit();

        if ( unit && !unit->isValid() ) {
            unit->PostKilledAction();
        }
    }
}

uint32_t Battle::Board::GetDistance( const int32_t index1, const int32_t index2 )
{
    if ( !isValidIndex( index1 ) || !isValidIndex( index2 ) ) {
        return 0;
    }

    const int32_t x1 = index1 % widthInCells;
    const int32_t y1 = index1 / widthInCells;

    const int32_t x2 = index2 % widthInCells;
    const int32_t y2 = index2 / widthInCells;

    const int32_t du = y2 - y1;
    const int32_t dv = ( x2 + y2 / 2 ) - ( x1 + y1 / 2 );

    if ( ( du >= 0 && dv >= 0 ) || ( du < 0 && dv < 0 ) ) {
        return std::max( std::abs( du ), std::abs( dv ) );
    }

    return std::abs( du ) + std::abs( dv );
}

uint32_t Battle::Board::GetDistance( const Position & pos1, const Position & pos2 )
{
    if ( pos1.GetHead() == nullptr || pos2.GetHead() == nullptr ) {
        return 0;
    }

    const int32_t head1Idx = pos1.GetHead()->GetIndex();
    const int32_t tail1Idx = pos1.GetTail() ? pos1.GetTail()->GetIndex() : -1;

    const int32_t head2Idx = pos2.GetHead()->GetIndex();
    const int32_t tail2Idx = pos2.GetTail() ? pos2.GetTail()->GetIndex() : -1;

    uint32_t distance = Board::GetDistance( head1Idx, head2Idx );

    if ( tail2Idx != -1 ) {
        distance = std::min( distance, Board::GetDistance( head1Idx, tail2Idx ) );
    }

    if ( tail1Idx != -1 ) {
        distance = std::min( distance, Board::GetDistance( tail1Idx, head2Idx ) );

        if ( tail2Idx != -1 ) {
            distance = std::min( distance, Board::GetDistance( tail1Idx, tail2Idx ) );
        }
    }

    return distance;
}

uint32_t Battle::Board::GetDistance( const Position & pos, const int32_t index )
{
    if ( pos.GetHead() == nullptr || !isValidIndex( index ) ) {
        return 0;
    }

    const int32_t headIdx = pos.GetHead()->GetIndex();
    const int32_t tailIdx = pos.GetTail() ? pos.GetTail()->GetIndex() : -1;

    uint32_t distance = Board::GetDistance( headIdx, index );

    if ( tailIdx != -1 ) {
        distance = std::min( distance, Board::GetDistance( tailIdx, index ) );
    }

    return distance;
}

std::vector<Battle::Unit *> Battle::Board::GetNearestTroops( const Unit * startUnit, const std::vector<Battle::Unit *> & blackList )
{
    std::vector<std::pair<Battle::Unit *, uint32_t>> foundUnits;

    for ( Cell & cell : *this ) {
        Unit * cellUnit = cell.GetUnit();
        if ( cellUnit == nullptr || startUnit == cellUnit || cell.GetIndex() != cellUnit->GetHeadIndex() ) {
            continue;
        }

        const bool isBlackListed = std::find( blackList.begin(), blackList.end(), cellUnit ) != blackList.end();
        if ( !isBlackListed ) {
            foundUnits.emplace_back( cellUnit, GetDistance( startUnit->GetPosition(), cellUnit->GetPosition() ) );
        }
    }

    std::sort( foundUnits.begin(), foundUnits.end(),
               []( const std::pair<Battle::Unit *, uint32_t> & first, const std::pair<Battle::Unit *, uint32_t> & second ) { return first.second < second.second; } );

    std::vector<Battle::Unit *> units;
    units.reserve( foundUnits.size() );

    for ( const auto & foundUnit : foundUnits ) {
        units.push_back( foundUnit.first );
    }

    return units;
}

Battle::CellDirection Battle::Board::GetDirection( const int32_t index1, const int32_t index2 )
{
    if ( !isValidIndex( index1 ) || !isValidIndex( index2 ) ) {
        return CellDirection::UNKNOWN;
    }

    if ( index1 == index2 ) {
        return CellDirection::CENTER;
    }

    for ( CellDirection dir = CellDirection::TOP_LEFT; dir < CellDirection::CENTER; ++dir ) {
        if ( isValidDirection( index1, dir ) && index2 == GetIndexDirection( index1, dir ) ) {
            return dir;
        }
    }

    return CellDirection::UNKNOWN;
}

Battle::CellDirection Battle::Board::GetReflectDirection( const CellDirection dir )
{
    switch ( dir ) {
    case CellDirection::TOP_LEFT:
        return CellDirection::BOTTOM_RIGHT;
    case CellDirection::TOP_RIGHT:
        return CellDirection::BOTTOM_LEFT;
    case CellDirection::LEFT:
        return CellDirection::RIGHT;
    case CellDirection::RIGHT:
        return CellDirection::LEFT;
    case CellDirection::BOTTOM_LEFT:
        return CellDirection::TOP_RIGHT;
    case CellDirection::BOTTOM_RIGHT:
        return CellDirection::TOP_LEFT;
    default:
        break;
    }

    return CellDirection::UNKNOWN;
}

Battle::CellDirection Battle::Board::GetDirectionFromDelta( const int32_t moveX, const int32_t moveY )
{
    if ( moveX == 0 && moveY == 0 ) {
        return CellDirection::CENTER;
    }

    if ( moveY < 0 ) {
        if ( moveX < 0 ) {
            return CellDirection::TOP_LEFT;
        }
        if ( moveX > 0 ) {
            return CellDirection::TOP_RIGHT;
        }
    }
    else if ( moveY > 0 ) {
        if ( moveX < 0 ) {
            return CellDirection::BOTTOM_LEFT;
        }
        if ( moveX > 0 ) {
            return CellDirection::BOTTOM_RIGHT;
        }
    }

    if ( moveX < 0 ) {
        return CellDirection::LEFT;
    }
    if ( moveX > 0 ) {
        return CellDirection::RIGHT;
    }

    // Should never happen. The above checks should exhaust all directions.
    assert( 0 );
    return CellDirection::UNKNOWN;
}

uint32_t Battle::Board::GetDistanceFromBoardEdgeAlongXAxis( const int32_t index, const bool fromRightEdge )
{
    assert( isValidIndex( index ) );

    const uint32_t x = index % widthInCells;

    return ( fromRightEdge ? widthInCells - x : x + 1 );
}

bool Battle::Board::isValidDirection( const int32_t index, const CellDirection dir )
{
    if ( !isValidIndex( index ) ) {
        return false;
    }

    if ( dir == CellDirection::CENTER ) {
        return true;
    }

    const int32_t x = index % widthInCells;
    const int32_t y = index / widthInCells;

    switch ( dir ) {
    case CellDirection::TOP_LEFT:
        return !( 0 == y || ( 0 == x && ( y % 2 ) ) );
    case CellDirection::TOP_RIGHT:
        return !( 0 == y || ( ( widthInCells - 1 ) == x && !( y % 2 ) ) );
    case CellDirection::LEFT:
        return !( 0 == x );
    case CellDirection::RIGHT:
        return !( ( widthInCells - 1 ) == x );
    case CellDirection::BOTTOM_LEFT:
        return !( ( heightInCells - 1 ) == y || ( 0 == x && ( y % 2 ) ) );
    case CellDirection::BOTTOM_RIGHT:
        return !( ( heightInCells - 1 ) == y || ( ( widthInCells - 1 ) == x && !( y % 2 ) ) );
    default:
        break;
    }

    return false;
}

int32_t Battle::Board::GetIndexDirection( const int32_t index, const CellDirection dir )
{
    if ( !isValidIndex( index ) ) {
        return -1;
    }

    switch ( dir ) {
    case CellDirection::CENTER:
        return index;
    case CellDirection::TOP_LEFT:
        return index - ( ( ( index / widthInCells ) % 2 ) ? widthInCells + 1 : widthInCells );
    case CellDirection::TOP_RIGHT:
        return index - ( ( ( index / widthInCells ) % 2 ) ? widthInCells : widthInCells - 1 );
    case CellDirection::LEFT:
        return index - 1;
    case CellDirection::RIGHT:
        return index + 1;
    case CellDirection::BOTTOM_LEFT:
        return index + ( ( ( index / widthInCells ) % 2 ) ? widthInCells - 1 : widthInCells );
    case CellDirection::BOTTOM_RIGHT:
        return index + ( ( ( index / widthInCells ) % 2 ) ? widthInCells : widthInCells + 1 );
    default:
        break;
    }

    return -1;
}

int32_t Battle::Board::GetIndexAbsPosition( const fheroes2::Point & pt ) const
{
    const_iterator it = begin();

    for ( ; it != end(); ++it )
        if ( ( *it ).isPositionIncludePoint( pt ) )
            break;

    return it != end() ? ( *it ).GetIndex() : -1;
}

bool Battle::Board::isCastleIndex( const int32_t index )
{
    return ( index >= 0 ) ? ( g_castle_battle_arena[index] & IS_CASTLE ) : false;
}

bool Battle::Board::isOutOfWallsIndex( const int32_t index )
{
    return (index >= 0) ? ( g_castle_battle_arena[index] & IS_OUT_WALLS ) : true;
}

bool Battle::Board::isMoatIndex( const int32_t index, const Unit & unit )
{
    if ( index >= 0 ) {
        auto c = g_castle_battle_arena[index];
        if ( c & IS_MOAT ) {
            if ( c & IS_BRIDGE ) {
                const Bridge * bridge = Arena::GetBridge();
                return bridge == nullptr || !bridge->isPassable( unit ) || unit.isFlying();
            }
            return true;
        }
    }
    return false;
}

void Battle::Board::SetCobjObjects( const Maps::Tile & tile, Rand::PCG32 & gen )
{
    std::vector<int> objs;

    if ( tile.getMainObjectType( false ) == MP2::OBJ_GRAVEYARD ) {
        objs.push_back( ICN::COBJ0000 );
        objs.push_back( ICN::COBJ0001 );
        objs.push_back( ICN::COBJ0025 );
    }
    else {
        switch ( tile.GetGround() ) {
        case Maps::Ground::DESERT:
            objs.push_back( ICN::COBJ0009 );
            objs.push_back( ICN::COBJ0024 );
            break;

        case Maps::Ground::SNOW:
            objs.push_back( ICN::COBJ0022 );
            objs.push_back( ICN::COBJ0026 );
            break;

        case Maps::Ground::SWAMP:
            objs.push_back( ICN::COBJ0005 );
            objs.push_back( ICN::COBJ0006 );
            objs.push_back( ICN::COBJ0007 );
            objs.push_back( ICN::COBJ0008 );
            objs.push_back( ICN::COBJ0011 );
            objs.push_back( ICN::COBJ0012 );
            objs.push_back( ICN::COBJ0014 );
            objs.push_back( ICN::COBJ0015 );
            objs.push_back( ICN::COBJ0016 );
            objs.push_back( ICN::COBJ0017 );
            objs.push_back( ICN::COBJ0027 );
            break;

        case Maps::Ground::BEACH:
            objs.push_back( ICN::COBJ0005 );
            objs.push_back( ICN::COBJ0011 );
            objs.push_back( ICN::COBJ0017 );
            break;

        case Maps::Ground::DIRT:
            objs.push_back( ICN::COBJ0002 );
            objs.push_back( ICN::COBJ0005 );
            objs.push_back( ICN::COBJ0007 );
            objs.push_back( ICN::COBJ0011 );
            objs.push_back( ICN::COBJ0014 );
            objs.push_back( ICN::COBJ0019 );
            objs.push_back( ICN::COBJ0027 );
            break;

        case Maps::Ground::GRASS:
            objs.push_back( ICN::COBJ0002 );
            objs.push_back( ICN::COBJ0004 );
            objs.push_back( ICN::COBJ0005 );
            objs.push_back( ICN::COBJ0008 );
            objs.push_back( ICN::COBJ0011 );
            objs.push_back( ICN::COBJ0012 );
            objs.push_back( ICN::COBJ0014 );
            objs.push_back( ICN::COBJ0015 );
            objs.push_back( ICN::COBJ0019 );
            objs.push_back( ICN::COBJ0027 );
            objs.push_back( ICN::COBJ0028 );
            break;

        case Maps::Ground::WASTELAND:
            objs.push_back( ICN::COBJ0009 );
            objs.push_back( ICN::COBJ0013 );
            objs.push_back( ICN::COBJ0018 );
            objs.push_back( ICN::COBJ0020 );
            objs.push_back( ICN::COBJ0021 );
            objs.push_back( ICN::COBJ0024 );
            break;

        case Maps::Ground::LAVA:
            objs.push_back( ICN::COBJ0007 );
            objs.push_back( ICN::COBJ0029 );
            objs.push_back( ICN::COBJ0031 );
            break;

        case Maps::Ground::WATER:
            objs.push_back( ICN::COBJ0003 );
            objs.push_back( ICN::COBJ0010 );
            objs.push_back( ICN::COBJ0023 );
            break;

        default:
            break;
        }
    }

    Rand::ShuffleWithGen( objs, gen );

    const auto largeObstacleHexCount = std::count_if( begin(), end(), []( const Cell & cell ) { return cell.GetObject() == 0x40; } );

    uint8_t maxSmallObstacleCount = 0;
    uint8_t maxTwoHexObstacleCount = 0;

    if ( largeObstacleHexCount == 0 ) {
        maxSmallObstacleCount = 6;
        maxTwoHexObstacleCount = 3;
    }
    else if ( largeObstacleHexCount <= 7 ) {
        maxSmallObstacleCount = 4;
        maxTwoHexObstacleCount = 2;
    }
    else if ( largeObstacleHexCount <= 13 ) {
        maxSmallObstacleCount = 3;
        maxTwoHexObstacleCount = 2;
    }
    else {
        maxSmallObstacleCount = 2;
        maxTwoHexObstacleCount = 1;
    }

    // Limit the number of two-hex obstacles to maxTwoHexObstacleCount
    uint8_t twoHexCount = 0;
    for ( auto iter = objs.begin(); iter != objs.end(); ) {
        if ( isTwoHexObject( *iter ) ) {
            ++twoHexCount;
            if ( twoHexCount > maxTwoHexObstacleCount ) {
                iter = objs.erase( iter );
                continue;
            }
        }
        ++iter;
    }

    const size_t objectsToPlace = std::min( objs.size(), static_cast<size_t>( Rand::GetWithGen( 0, maxSmallObstacleCount, gen ) ) );

    for ( size_t i = 0; i < objectsToPlace; ++i ) {
        // two-hex obstacles are not allowed on column 8 as they would cover column 9 which is reserved for units
        const bool checkRightCell = isTwoHexObject( objs[i] );

        // tall obstacles like trees should not be placed on top 2 rows
        const bool isTallObstacle = isTallObject( objs[i] );

        uint32_t dest;
        do {
            dest = GetRandomObstaclePosition( gen );
        } while ( at( dest ).GetObject() != 0 || ( checkRightCell && ( at( dest + 1 ).GetObject() != 0 || ( dest % widthInCells ) == 8 ) )
                  || ( isTallObstacle && dest < ( widthInCells * 2 ) ) );

        SetCobjObject( objs[i], dest );
    }
}

void Battle::Board::SetCobjObject( const int icn, const uint32_t dst )
{
    at( dst ).SetObject( 0x80 + ( icn - ICN::COBJ0000 ) );

    if ( isTwoHexObject( icn ) ) {
        assert( at( dst + 1 ).GetObject() == 0 );
        at( dst + 1 ).SetObject( 0x40 );
    }
}

void Battle::Board::SetCovrObjects( int icn )
{
    constexpr int dx = 2;
    constexpr int dy = 0;

    switch ( icn ) {
    case ICN::COVR0001:
    case ICN::COVR0007:
    case ICN::COVR0013:
    case ICN::COVR0019:
        AtCell( dx + 3, dy + 2 ).SetObject( 0x40 );
        AtCell( dx + 4, dy + 2 ).SetObject( 0x40 );
        AtCell( dx + 5, dy + 2 ).SetObject( 0x40 );
        AtCell( dx + 6, dy + 2 ).SetObject( 0x40 );
        AtCell( dx + 7, dy + 3 ).SetObject( 0x40 );
        AtCell( dx + 7, dy + 4 ).SetObject( 0x40 );
        break;

    case ICN::COVR0002:
    case ICN::COVR0008:
    case ICN::COVR0014:
    case ICN::COVR0020:
        AtCell( dx + 3, dy + 4 ).SetObject( 0x40 );
        AtCell( dx + 4, dy + 4 ).SetObject( 0x40 );
        AtCell( dx + 5, dy + 4 ).SetObject( 0x40 );
        AtCell( dx + 6, dy + 4 ).SetObject( 0x40 );
        AtCell( dx + 7, dy + 4 ).SetObject( 0x40 );
        break;

    case ICN::COVR0003:
    case ICN::COVR0015:
    case ICN::COVR0021:
        AtCell( dx + 2, dy + 3 ).SetObject( 0x40 );
        AtCell( dx + 8, dy + 3 ).SetObject( 0x40 );
        AtCell( dx + 2, dy + 4 ).SetObject( 0x40 );
        AtCell( dx + 3, dy + 4 ).SetObject( 0x40 );
        AtCell( dx + 4, dy + 4 ).SetObject( 0x40 );
        AtCell( dx + 5, dy + 4 ).SetObject( 0x40 );
        AtCell( dx + 6, dy + 4 ).SetObject( 0x40 );
        AtCell( dx + 7, dy + 4 ).SetObject( 0x40 );
        break;

    case ICN::COVR0009:
        AtCell( dx + 2, dy + 3 ).SetObject( 0x40 );
        AtCell( dx + 7, dy + 3 ).SetObject( 0x40 );
        AtCell( dx + 2, dy + 4 ).SetObject( 0x40 );
        AtCell( dx + 3, dy + 4 ).SetObject( 0x40 );
        AtCell( dx + 4, dy + 4 ).SetObject( 0x40 );
        AtCell( dx + 5, dy + 4 ).SetObject( 0x40 );
        AtCell( dx + 6, dy + 4 ).SetObject( 0x40 );
        break;

    case ICN::COVR0004:
    case ICN::COVR0010:
    case ICN::COVR0016:
    case ICN::COVR0022:
        AtCell( dx + 8, dy + 3 ).SetObject( 0x40 );
        AtCell( dx + 7, dy + 4 ).SetObject( 0x40 );
        AtCell( dx + 3, dy + 5 ).SetObject( 0x40 );
        AtCell( dx + 4, dy + 5 ).SetObject( 0x40 );
        AtCell( dx + 5, dy + 5 ).SetObject( 0x40 );
        AtCell( dx + 6, dy + 5 ).SetObject( 0x40 );
        AtCell( dx + 7, dy + 5 ).SetObject( 0x40 );
        break;

    case ICN::COVR0005:
    case ICN::COVR0017:
        AtCell( dx + 2, dy + 2 ).SetObject( 0x40 );
        AtCell( dx + 3, dy + 2 ).SetObject( 0x40 );
        AtCell( dx + 4, dy + 2 ).SetObject( 0x40 );
        AtCell( dx + 5, dy + 2 ).SetObject( 0x40 );
        AtCell( dx + 6, dy + 2 ).SetObject( 0x40 );
        AtCell( dx + 7, dy + 2 ).SetObject( 0x40 );
        AtCell( dx + 8, dy + 2 ).SetObject( 0x40 );
        AtCell( dx + 3, dy + 5 ).SetObject( 0x40 );
        AtCell( dx + 4, dy + 5 ).SetObject( 0x40 );
        AtCell( dx + 5, dy + 5 ).SetObject( 0x40 );
        AtCell( dx + 6, dy + 5 ).SetObject( 0x40 );
        AtCell( dx + 7, dy + 5 ).SetObject( 0x40 );
        AtCell( dx + 8, dy + 5 ).SetObject( 0x40 );
        AtCell( dx + 2, dy + 6 ).SetObject( 0x40 );
        AtCell( dx + 8, dy + 6 ).SetObject( 0x40 );
        break;

    case ICN::COVR0006:
    case ICN::COVR0018:
        AtCell( dx + 3, dy + 1 ).SetObject( 0x40 );
        AtCell( dx + 4, dy + 1 ).SetObject( 0x40 );
        AtCell( dx + 5, dy + 1 ).SetObject( 0x40 );
        AtCell( dx + 6, dy + 1 ).SetObject( 0x40 );
        AtCell( dx + 7, dy + 1 ).SetObject( 0x40 );
        AtCell( dx + 2, dy + 2 ).SetObject( 0x40 );
        AtCell( dx + 2, dy + 6 ).SetObject( 0x40 );
        AtCell( dx + 3, dy + 7 ).SetObject( 0x40 );
        AtCell( dx + 4, dy + 7 ).SetObject( 0x40 );
        AtCell( dx + 5, dy + 7 ).SetObject( 0x40 );
        AtCell( dx + 6, dy + 7 ).SetObject( 0x40 );
        AtCell( dx + 7, dy + 7 ).SetObject( 0x40 );
        break;

    case ICN::COVR0011:
    case ICN::COVR0023:
        AtCell( dx + 4, dy + 1 ).SetObject( 0x40 );
        AtCell( dx + 3, dy + 2 ).SetObject( 0x40 );
        AtCell( dx + 3, dy + 3 ).SetObject( 0x40 );
        AtCell( dx + 7, dy + 4 ).SetObject( 0x40 );
        AtCell( dx + 7, dy + 5 ).SetObject( 0x40 );
        AtCell( dx + 5, dy + 6 ).SetObject( 0x40 );
        AtCell( dx + 6, dy + 6 ).SetObject( 0x40 );
        break;

    case ICN::COVR0012:
    case ICN::COVR0024:
        AtCell( dx + 7, dy + 1 ).SetObject( 0x40 );
        AtCell( dx + 7, dy + 2 ).SetObject( 0x40 );
        AtCell( dx + 8, dy + 3 ).SetObject( 0x40 );
        AtCell( dx + 4, dy + 5 ).SetObject( 0x40 );
        AtCell( dx + 4, dy + 6 ).SetObject( 0x40 );
        AtCell( dx + 5, dy + 7 ).SetObject( 0x40 );
        AtCell( dx + 6, dy + 7 ).SetObject( 0x40 );
        break;

    default:
        break;
    }
}

Battle::Cell * Battle::Board::GetCell( const int32_t position )
{
    if ( !isValidIndex( position ) ) {
        return nullptr;
    }

    Board * board = Arena::GetBoard();
    assert( board != nullptr );

#ifdef WITH_DEBUG
    return &board->at( position );
#else
    return &board->operator[]( position );
#endif
}

Battle::Cell * Battle::Board::GetCell( const int32_t position, const CellDirection dir )
{
    if ( !isValidDirection( position, dir ) ) {
        return nullptr;
    }

    Board * board = Arena::GetBoard();
    assert( board != nullptr );

    const int32_t idx = GetIndexDirection( position, dir );
    assert( isValidIndex( idx ) );

#ifdef WITH_DEBUG
    return &board->at( idx );
#else
    return &board->operator[]( idx );
#endif
}

Battle::Cell& Battle::Board::AtCell( int x, int y )
{
    assert(x >= 0 && x < widthInCells);
    assert(y >= 0 && y < heightInCells);

    return at(y * widthInCells + x);
}

const Battle::Cell& Battle::Board::AtCell(int x, int y) const
{
    assert(x >= 0 && x < widthInCells);
    assert(y >= 0 && y < heightInCells);

    return at(y * widthInCells + x);
}

Battle::Indexes Battle::Board::GetMoveWideIndexes( const int32_t head, const bool reflect )
{
    if ( !isValidIndex( head ) ) {
        return {};
    }

    Indexes result;
    result.reserve( 4 );

    if ( isValidDirection( head, CellDirection::LEFT ) ) {
        result.push_back( GetIndexDirection( head, CellDirection::LEFT ) );
    }
    if ( isValidDirection( head, CellDirection::RIGHT ) ) {
        result.push_back( GetIndexDirection( head, CellDirection::RIGHT ) );
    }

    if ( reflect ) {
        if ( isValidDirection( head, CellDirection::TOP_LEFT ) ) {
            result.push_back( GetIndexDirection( head, CellDirection::TOP_LEFT ) );
        }
        if ( isValidDirection( head, CellDirection::BOTTOM_LEFT ) ) {
            result.push_back( GetIndexDirection( head, CellDirection::BOTTOM_LEFT ) );
        }
    }
    else {
        if ( isValidDirection( head, CellDirection::TOP_RIGHT ) ) {
            result.push_back( GetIndexDirection( head, CellDirection::TOP_RIGHT ) );
        }
        if ( isValidDirection( head, CellDirection::BOTTOM_RIGHT ) ) {
            result.push_back( GetIndexDirection( head, CellDirection::BOTTOM_RIGHT ) );
        }
    }

    return result;
}

Battle::Indexes Battle::Board::GetAroundIndexes( const int32_t center )
{
    if ( !isValidIndex( center ) ) {
        return {};
    }

    Indexes result;
    result.reserve( 6 );

    for ( CellDirection dir = CellDirection::TOP_LEFT; dir < CellDirection::CENTER; ++dir ) {
        if ( !isValidDirection( center, dir ) ) {
            continue;
        }

        result.push_back( GetIndexDirection( center, dir ) );
    }

    return result;
}

Battle::Indexes Battle::Board::GetAroundIndexes( const Unit & unit )
{
    return GetAroundIndexes( unit.GetPosition() );
}

Battle::Indexes Battle::Board::GetAroundIndexes( const Position & pos )
{
    if ( pos.GetHead() == nullptr ) {
        return {};
    }

    const int32_t headIdx = pos.GetHead()->GetIndex();

    if ( pos.GetTail() == nullptr ) {
        return GetAroundIndexes( headIdx );
    }

    const int32_t tailIdx = pos.GetTail()->GetIndex();

    if ( !isValidIndex( headIdx ) || !isValidIndex( tailIdx ) ) {
        return {};
    }

    Indexes result;
    result.reserve( 8 );

    // Traversing cells in a clockwise direction
    if ( headIdx > tailIdx ) {
        if ( isValidDirection( tailIdx, CellDirection::TOP_LEFT ) ) {
            result.push_back( GetIndexDirection( tailIdx, CellDirection::TOP_LEFT ) );
        }
        if ( isValidDirection( tailIdx, CellDirection::TOP_RIGHT ) ) {
            result.push_back( GetIndexDirection( tailIdx, CellDirection::TOP_RIGHT ) );
        }
        if ( isValidDirection( headIdx, CellDirection::TOP_RIGHT ) ) {
            result.push_back( GetIndexDirection( headIdx, CellDirection::TOP_RIGHT ) );
        }
        if ( isValidDirection( headIdx, CellDirection::RIGHT ) ) {
            result.push_back( GetIndexDirection( headIdx, CellDirection::RIGHT ) );
        }
        if ( isValidDirection( headIdx, CellDirection::BOTTOM_RIGHT ) ) {
            result.push_back( GetIndexDirection( headIdx, CellDirection::BOTTOM_RIGHT ) );
        }
        if ( isValidDirection( tailIdx, CellDirection::BOTTOM_RIGHT ) ) {
            result.push_back( GetIndexDirection( tailIdx, CellDirection::BOTTOM_RIGHT ) );
        }
        if ( isValidDirection( tailIdx, CellDirection::BOTTOM_LEFT ) ) {
            result.push_back( GetIndexDirection( tailIdx, CellDirection::BOTTOM_LEFT ) );
        }
        if ( isValidDirection( tailIdx, CellDirection::LEFT ) ) {
            result.push_back( GetIndexDirection( tailIdx, CellDirection::LEFT ) );
        }
    }
    else if ( headIdx < tailIdx ) {
        if ( isValidDirection( headIdx, CellDirection::TOP_LEFT ) ) {
            result.push_back( GetIndexDirection( headIdx, CellDirection::TOP_LEFT ) );
        }
        if ( isValidDirection( headIdx, CellDirection::TOP_RIGHT ) ) {
            result.push_back( GetIndexDirection( headIdx, CellDirection::TOP_RIGHT ) );
        }
        if ( isValidDirection( tailIdx, CellDirection::TOP_RIGHT ) ) {
            result.push_back( GetIndexDirection( tailIdx, CellDirection::TOP_RIGHT ) );
        }
        if ( isValidDirection( tailIdx, CellDirection::RIGHT ) ) {
            result.push_back( GetIndexDirection( tailIdx, CellDirection::RIGHT ) );
        }
        if ( isValidDirection( tailIdx, CellDirection::BOTTOM_RIGHT ) ) {
            result.push_back( GetIndexDirection( tailIdx, CellDirection::BOTTOM_RIGHT ) );
        }
        if ( isValidDirection( headIdx, CellDirection::BOTTOM_RIGHT ) ) {
            result.push_back( GetIndexDirection( headIdx, CellDirection::BOTTOM_RIGHT ) );
        }
        if ( isValidDirection( headIdx, CellDirection::BOTTOM_LEFT ) ) {
            result.push_back( GetIndexDirection( headIdx, CellDirection::BOTTOM_LEFT ) );
        }
        if ( isValidDirection( headIdx, CellDirection::LEFT ) ) {
            result.push_back( GetIndexDirection( headIdx, CellDirection::LEFT ) );
        }
    }

    return result;
}

Battle::Indexes Battle::Board::GetDistanceIndexes( const int32_t center, const uint32_t radius )
{
    if ( !isValidIndex( center ) ) {
        return {};
    }

    const int32_t centerX = center % widthInCells;
    const int32_t centerY = center / widthInCells;

    // Axial coordinates
    const int32_t centerQ = centerX - ( centerY + ( centerY % 2 ) ) / 2;
    const int32_t centerR = centerY;

    Indexes result;
    result.reserve( 3 * radius * ( radius + 1 ) + 1 );

    const int32_t intRadius = radius;

    for ( int32_t dq = -intRadius; dq <= intRadius; ++dq ) {
        for ( int32_t dr = std::max( -intRadius, -intRadius - dq ); dr <= std::min( intRadius, intRadius - dq ); ++dr ) {
            // Center should not be included
            if ( dq == 0 && dr == 0 ) {
                continue;
            }

            const int32_t q = centerQ + dq;
            const int32_t r = centerR + dr;

            const int32_t x = q + ( r + ( r % 2 ) ) / 2;
            const int32_t y = r;

            if ( x < 0 || x >= widthInCells || y < 0 || y >= heightInCells ) {
                continue;
            }

            const int32_t idx = y * widthInCells + x;
            assert( isValidIndex( idx ) );

            result.push_back( idx );
        }
    }

    return result;
}

Battle::Indexes Battle::Board::GetDistanceIndexes( const Position & pos, const uint32_t radius )
{
    const std::array<int32_t, 2> posIndexes = { pos.GetHead() ? pos.GetHead()->GetIndex() : -1, pos.GetTail() ? pos.GetTail()->GetIndex() : -1 };

    std::set<int32_t> boardIndexes;

    for ( const int32_t posIdx : posIndexes ) {
        if ( !Board::isValidIndex( posIdx ) ) {
            continue;
        }

        for ( const int32_t idx : Board::GetDistanceIndexes( posIdx, radius ) ) {
            assert( Board::isValidIndex( idx ) );

            if ( std::find( posIndexes.begin(), posIndexes.end(), idx ) != posIndexes.end() ) {
                continue;
            }

            boardIndexes.insert( idx );
        }
    }

    Indexes result;

    result.reserve( boardIndexes.size() );
    result.assign( boardIndexes.begin(), boardIndexes.end() );

    return result;
}

Battle::Indexes Battle::Board::GetDistanceIndexes( const Unit & unit, const uint32_t radius )
{
    return GetDistanceIndexes( unit.GetPosition(), radius );
}

bool Battle::Board::isValidMirrorImageIndex( const int32_t index, const Unit * unit )
{
    if ( unit == nullptr ) {
        return false;
    }

    const Position mirrorPos = Position::GetPosition( *unit, index );
    if ( !mirrorPos.isValidForUnit( unit ) ) {
        return false;
    }

    if ( unit->GetPosition().contains( mirrorPos.GetHead()->GetIndex() ) ) {
        return false;
    }
    if ( unit->isWide() && unit->GetPosition().contains( mirrorPos.GetTail()->GetIndex() ) ) {
        return false;
    }

    return true;
}

bool Battle::Board::CanAttackFromCell( const Unit & unit, const int32_t from )
{
    const Position pos = Position::GetReachable( unit, from );

    // Target unit cannot be attacked if out of reach
    if ( pos.GetHead() == nullptr ) {
        return false;
    }

    assert( pos.isValidForUnit( unit ) );

    const Castle * castle = Arena::GetCastle();

    // No moat - no further restrictions
    if ( !castle || !castle->isBuild( BUILD_MOAT ) ) {
        return true;
    }

    // Target unit isn't attacked from the moat
    if ( !isMoatIndex( from, unit ) ) {
        return true;
    }

    // The moat doesn't stop flying units
    if ( unit.isFlying() ) {
        return true;
    }

    // Attacker is already near the target
    if ( from == unit.GetHeadIndex() || ( unit.isWide() && from == unit.GetTailIndex() ) ) {
        return true;
    }

    // In all other cases, the attack is prohibited
    return false;
}

bool Battle::Board::CanAttackTargetFromPosition( const Unit & attacker, const Unit & target, const int32_t dst )
{
    // Get the actual position of the attacker before attacking
    const Position pos = Position::GetReachable( attacker, dst );

    // Check that the attacker is actually capable of attacking the target from this position
    const std::array<const Cell *, 2> cells = { pos.GetHead(), pos.GetTail() };

    for ( const Cell * cell : cells ) {
        if ( cell == nullptr ) {
            continue;
        }

        const int32_t cellIdx = cell->GetIndex();

        if ( Board::GetDistance( target.GetPosition(), cellIdx ) > 1 ) {
            continue;
        }

        if ( !CanAttackFromCell( attacker, cellIdx ) ) {
            continue;
        }

        return true;
    }

    return false;
}

std::string Battle::Board::GetMoatInfo()
{
    std::string msg = _( "The Moat interrupts any movement through it and reduces the defense skill of troops present in it by %{count}." );
    StringReplace( msg, "%{count}", GameStatic::GetBattleMoatReduceDefense() );

    return msg;
}

fheroes2::Rect Battle::Board::GetMoatCellMask( const Cell & cell )
{
    // Bottom part of the cell, inset horizontally
    constexpr int32_t waterTopOffset = 34; // from cell top
    constexpr int32_t waterHeight = 10;
    constexpr int32_t insetX = 3;

    const auto & pos = cell.GetPos();

    fheroes2::Rect mask{
        pos.x + insetX,
        pos.y + waterTopOffset,
        Cell::widthPx - insetX,
        waterHeight,
    };

    // Additional, per-tile adjustments
    switch ( cell.GetIndex() )
    {
    case MOAT_IDX_0:
    case MOAT_IDX_1:
        mask.height -= 3;
        mask.y += 3;
        break;
    case MOAT_IDX_2:
    case MOAT_IDX_3:
        mask.x -= 2;
        break;
    case MOAT_IDX_4:
    case MOAT_IDX_5:
    case MOAT_IDX_6:
        mask.x += 12;
        mask.width += 8;
        break;
    case MOAT_IDX_7:
    case MOAT_IDX_8:
        break;
    case MOAT_IDX_9:
    case MOAT_IDX_10:
        mask.width += 4;
        mask.x += 6;
        break;
    default:
        // All possible moat cells have been handled above.
        // Reaching here means cell is not a moat cell.
        assert( 0 );
        return {};
    }

    return mask;
}

std::pair<const Battle::Cell *, const Battle::Cell *> Battle::Board::GetMoatCellsForUnit( const Unit & unit, const CellDirection movementDirection )
{
    auto resolveForCell = [&]( const Cell * cell ) -> std::pair<const Cell *, const Cell *> {
        std::pair<const Cell *, const Cell *> pair{ nullptr, nullptr };

        if ( cell == nullptr ) {
            return pair;
        }

        const int32_t currentIndex = cell->GetIndex();
        assert( isValidIndex( currentIndex ) );

        if ( isMoatIndex( currentIndex, unit ) ) {
            pair.first = cell;
        }

        const int32_t nextIndex = GetIndexDirection( currentIndex, movementDirection );
        if ( isValidIndex( nextIndex ) ) {
            const Cell * nextCell = GetCell( nextIndex );
            if ( nextCell && isMoatIndex( nextCell->GetIndex(), unit ) ) {
                pair.second = nextCell;
            }
        }

        return pair;
    };

    // 1. Head first, then try tail
    const Position & pos = unit.GetPosition();
    std::pair<const Cell *, const Cell *> result = resolveForCell( pos.GetHead() );
    if ( result.first == nullptr && result.second == nullptr && unit.isWide() ) {
        result = resolveForCell( pos.GetTail() );
    }

    if ( result.first == nullptr && result.second == nullptr ) {
        return result;
    }

    // Check if we have a bridge and that it's down
    const Bridge * bridge = Arena::GetBridge();
    if ( bridge == nullptr || !bridge->isDown() ) {
        return result;
    }

    // Check if unit is on the bridge.
    // We don't know which part of the unit is valid, so we need to check both
    if ( ( result.first != nullptr && IsBridgeCell( result.first->GetIndex() ) ) || ( result.second != nullptr && IsBridgeCell( result.second->GetIndex() ) ) ) {
        return { nullptr, nullptr };
    }

    return result;
}
