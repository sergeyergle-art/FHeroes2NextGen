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

#include "ground.h"

#include <cassert>

#include "maps_tiles.h"
#include "rand.h"
#include "skill.h"
#include "translations.h"
#include "game_static.h"

uint16_t Maps::Ground::getTerrainStartImageIndex( const int32_t groundId )
{
    switch ( groundId ) {
    case WATER:
        return WATER_START_IMAGE_INDEX;
    case GRASS:
        return GRASS_START_IMAGE_INDEX;
    case SNOW:
        return SNOW_START_IMAGE_INDEX;
    case SWAMP:
        return SWAMP_START_IMAGE_INDEX;
    case LAVA:
        return LAVA_START_IMAGE_INDEX;
    case DESERT:
        return DESERT_START_IMAGE_INDEX;
    case DIRT:
        return DIRT_START_IMAGE_INDEX;
    case WASTELAND:
        return WASTELAND_START_IMAGE_INDEX;
    case BEACH:
        return BEACH_START_IMAGE_INDEX;
    default:
        // Have you added a new ground? Add the logic above!
        assert( 0 );
        return 0;
    }
}

int32_t Maps::Ground::getGroundByImageIndex( const uint16_t terrainImageIndex )
{
    if ( GRASS_START_IMAGE_INDEX > terrainImageIndex ) {
        return Maps::Ground::WATER;
    }
    if ( SNOW_START_IMAGE_INDEX > terrainImageIndex ) {
        return Maps::Ground::GRASS;
    }
    if ( SWAMP_START_IMAGE_INDEX > terrainImageIndex ) {
        return Maps::Ground::SNOW;
    }
    if ( LAVA_START_IMAGE_INDEX > terrainImageIndex ) {
        return Maps::Ground::SWAMP;
    }
    if ( DESERT_START_IMAGE_INDEX > terrainImageIndex ) {
        return Maps::Ground::LAVA;
    }
    if ( DIRT_START_IMAGE_INDEX > terrainImageIndex ) {
        return Maps::Ground::DESERT;
    }
    if ( WASTELAND_START_IMAGE_INDEX > terrainImageIndex ) {
        return Maps::Ground::DIRT;
    }
    if ( BEACH_START_IMAGE_INDEX > terrainImageIndex ) {
        return Maps::Ground::WASTELAND;
    }
    if ( MAX_IMAGE_INDEX > terrainImageIndex ) {
        return Maps::Ground::BEACH;
    }

    // Have you added a new ground? Add the logic above!
    assert( 0 );
    return Maps::Ground::UNKNOWN;
}

bool Maps::Ground::isTerrainTransitionImage( const uint16_t terrainImageIndex )
{
    const int groundId = getGroundByImageIndex( terrainImageIndex );
    switch ( groundId ) {
    case WATER:
    case DIRT:
        return terrainImageIndex < 16U + getTerrainStartImageIndex( groundId );
    case GRASS:
    case SNOW:
    case SWAMP:
    case LAVA:
    case DESERT:
    case WASTELAND:
        return terrainImageIndex < 38U + getTerrainStartImageIndex( groundId );
    case BEACH:
        return false;
    default:
        // Have you added a new ground? Add the logic above!
        assert( 0 );
        return false;
    }
}

bool Maps::Ground::doesTerrainImageIndexContainEmbeddedObjects( const uint16_t terrainImageIndex )
{
    const int groundId = getGroundByImageIndex( terrainImageIndex );
    switch ( groundId ) {
    case WATER:
        return false;
    case GRASS:
    case SWAMP:
    case SNOW:
    case WASTELAND:
    case LAVA:
    case DESERT:
        return terrainImageIndex > 45U + getTerrainStartImageIndex( groundId );
    case DIRT:
        return terrainImageIndex > 23U + getTerrainStartImageIndex( groundId );
    case BEACH:
        return terrainImageIndex > 7U + getTerrainStartImageIndex( groundId );
    default:
        // Have you added a new ground? Add the logic above!
        assert( 0 );
        return false;
    }
}

const char * Maps::Ground::String( const int32_t groundId )
{
    switch ( groundId ) {
    case DESERT:
        return _( "Desert" );
    case SNOW:
        return _( "Snow" );
    case SWAMP:
        return _( "Swamp" );
    case WASTELAND:
        return _( "Wasteland" );
    case BEACH:
        return _( "Beach" );
    case LAVA:
        return _( "Lava" );
    case DIRT:
        return _( "Dirt" );
    case GRASS:
        return _( "Grass" );
    case WATER:
        return _( "Ocean" );
    default:
        return "Unknown";
    }
}

uint32_t Maps::Ground::GetPenalty( const Maps::Tile & tile, const uint32_t pathfindingLevel, bool isRoad, bool isDiagonal )
{
    //              none   basc   advd   expr   exp2   exp3
    //    Desert    2.00   1.75   1.50   1.00   1.00   0.87
    //    Swamp     1.75   1.50   1.25   1.00   1.00   0.87
    //    Snow      1.50   1.25   1.00   1.00   1.00   0.87
    //    Wasteland 1.25   1.00   1.00   1.00   1.00   0.87
    //    Beach     1.25   1.00   1.00   1.00   1.00   0.87
    //    Lava      1.00   1.00   1.00   1.00   1.00   0.87
    //    Dirt      1.00   1.00   1.00   1.00   1.00   0.87
    //    Grass     1.00   1.00   1.00   1.00   1.00   0.87
    //    Water     1.00   1.00   1.00   1.00   1.00   1.00
    //    Road      0.75   0.75   0.75   0.75   0.75   0.75

    uint32_t penalty = defaultGroundPenalty;

    if ( isRoad ) {
        penalty = roadPenalty;
    }
    else {
        if ( pathfindingLevel < Skill::MaxLevel ) {
            uint32_t delta = 0;

            switch (tile.GetGround() ) {
            case DESERT:
                delta = 100;
                break;
            case SWAMP:
                delta = 80;
                break;
            case SNOW:
                delta = 60;
                break;
            case LAVA:
            case BEACH:
                delta = 40;
                break;
            case WASTELAND:
                delta = 20;
                break;
            }

            uint32_t factor = GameStatic::GetSecondarySkillValue( Skill::Secondary::PATHFINDING, pathfindingLevel );
            penalty += delta * ( 100 - factor ) / 100;
        }
        else {
            penalty = 87;
        }
    }

    if ( isDiagonal ) {
        penalty = (penalty * 1148 + 512) / 1024;
    }

    return penalty;
}

uint16_t Maps::Ground::getRandomTerrainImageIndex( const int32_t groundId, const bool allowEmbeddedObjectsAppearOnTerrain )
{
    if ( groundId == WATER ) {
        return static_cast<uint16_t>( Rand::Get( 3 ) ) + 16U + getTerrainStartImageIndex( groundId );
    }

    // Terrain images, except Water, can contain extra objects that are a part of the image.
    if ( allowEmbeddedObjectsAppearOnTerrain && Rand::Get( 6 ) == 0 ) {
        switch ( groundId ) {
        case GRASS:
        case SWAMP:
            return static_cast<uint16_t>( Rand::Get( 15 ) ) + 46U + getTerrainStartImageIndex( groundId );
        case SNOW:
        case WASTELAND:
        case LAVA:
            return static_cast<uint16_t>( Rand::Get( 7 ) ) + 46U + getTerrainStartImageIndex( groundId );
        case DESERT:
            return static_cast<uint16_t>( Rand::Get( 12 ) ) + 46U + getTerrainStartImageIndex( groundId );
        case DIRT:
            return static_cast<uint16_t>( Rand::Get( 15 ) ) + 24U + getTerrainStartImageIndex( groundId );
        case BEACH:
            return static_cast<uint16_t>( Rand::Get( 8 ) ) + 8U + getTerrainStartImageIndex( groundId );
        default:
            // Have you added a new ground? Add the logic above!
            assert( 0 );
            return 0;
        }
    }

    const uint16_t indexOffset = static_cast<uint16_t>( Rand::Get( 7 ) );
    switch ( groundId ) {
    case GRASS:
    case SNOW:
    case SWAMP:
    case LAVA:
    case DESERT:
    case WASTELAND:
        return indexOffset + 38U + getTerrainStartImageIndex( groundId );
    case DIRT:
        return indexOffset + 16U + getTerrainStartImageIndex( groundId );
    case BEACH:
        return indexOffset + getTerrainStartImageIndex( groundId );
    default:
        // Have you added a new ground? Add the logic above!
        assert( 0 );
        return 0;
    }
}
