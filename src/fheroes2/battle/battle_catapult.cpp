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

#include "battle_catapult.h"

#include <algorithm>
#include <cassert>
#include <ostream>
#include <utility>
#include <vector>

#include "artifact.h"
#include "artifact_info.h"
#include "heroes_base.h"
#include "logging.h"
#include "rand.h"
#include "skill.h"
#include "battle_arena.h"
#include "castle.h"

namespace
{
    constexpr int EARTHQUAKE_DAMAGE_BY_SPELLPOWER = 3;

    const fheroes2::Point g_targetPos[Battle::CastleFortress::ALL_TARGETS] = {
        /* UPPER_WALL_1 */ { 452, 38  },
        /* UPPER_WALL_2 */ { 405, 108 },
        /* LOWER_WALL_1 */ { 408, 270 },
        /* LOWER_WALL_2 */ { 458, 360 },
        /* UPPER_TOWER  */ { 430, 42  },
        /* LOWER_TOWER  */ { 430, 294 },
        /* GATE_TOWER_1 */ { 385, 126 },
        /* GATE_TOWER_2 */ { 385, 208 },
        /* GATE         */ { 392, 185 },
        /* MAIN_TOWER   */ { 600, 120 },
        /* EMPTY_TOWER  */ { 480, 380 }
    };

    const int g_targetCellIdx[Battle::CastleFortress::ALL_TARGETS] = {
        /* UPPER_WALL_1 */ Battle::CastleFortress::POS_UPPER_WALL_1,
        /* UPPER_WALL_2 */ Battle::CastleFortress::POS_UPPER_WALL_2,
        /* LOWER_WALL_1 */ Battle::CastleFortress::POS_LOWER_WALL_1,
        /* LOWER_WALL_2 */ Battle::CastleFortress::POS_LOWER_WALL_2,
        /* UPPER_TOWER  */ Battle::CastleFortress::POS_UPPER_TOWER,
        /* LOWER_TOWER  */ Battle::CastleFortress::POS_LOWER_TOWER,
        /* GATE_TOWER_1 */ Battle::CastleFortress::POS_GATE_TOWER_1,
        /* GATE_TOWER_2 */ Battle::CastleFortress::POS_GATE_TOWER_2,
        /* GATE         */ Battle::CastleFortress::POS_GATE,
        /* MAIN_TOWER   */ Battle::POS_CATAPULT, // for safe structure, not be used
        /* EMPTY_TOWER  */ Battle::CastleFortress::POS_EMPTY_TOWER
    };
}

Battle::CastleFortress::CastleFortress()
    : _countTargets( 0 )
    , _countEmptyTowers( 0 )
    , _lastTarget( UPPER_WALL_1 )
    , _damagePerHit( 0 )
    , _countHits( 0 )
    , _totalHp( 0 )
{
}

void Battle::CastleFortress::Init( const Castle& castle, const HeroBase& attackingHero )
{
    constexpr int WALL_HP = 6;
    constexpr int WALL_HP_EX = 8;
    constexpr int ARCHER_TOWER_HP = 6;
    constexpr int BRIDGE_HP = 6;

    constexpr int NORMAL_DAMAGE = 2;
    constexpr int STRENGTH_DAMAGE = 3;
    constexpr int ULTRA_DAMAGE = 6;

    int wallHp = castle.isFortificationBuilt() ? WALL_HP_EX : WALL_HP;

    _targetHp[UPPER_WALL_1] = wallHp;
    _targetHp[UPPER_WALL_2] = wallHp;
    _targetHp[LOWER_WALL_1] = wallHp;
    _targetHp[LOWER_WALL_2] = wallHp;
    _targetHp[UPPER_TOWER] = 0;
    _targetHp[LOWER_TOWER] = 0;
    _targetHp[GATE_TOWER_1] = 0;
    _targetHp[GATE_TOWER_2] = 0;
    _targetHp[GATE] = BRIDGE_HP;
    _targetHp[MAIN_TOWER] = ARCHER_TOWER_HP;
    _targetHp[EMPTY_TOWER] = 0;
    _countTargets = 6;

    _emptyTowers[0] = EMPTY_TOWER;
    _countEmptyTowers = 1;

    if ( castle.hasUpperArcherTower() ) {
        _targetHp[UPPER_TOWER] = ARCHER_TOWER_HP;
        _targetHp[GATE_TOWER_1] = ARCHER_TOWER_HP;
        _countTargets += 2;
    }
    else {
        _emptyTowers[_countEmptyTowers++] = UPPER_TOWER;
        _emptyTowers[_countEmptyTowers++] = GATE_TOWER_1;
    }

    if ( castle.hasLowerArcherTower() ) {
        _targetHp[LOWER_TOWER] = ARCHER_TOWER_HP;
        _targetHp[GATE_TOWER_2] = ARCHER_TOWER_HP;
        _countTargets += 2;
    }
    else {
        _emptyTowers[_countEmptyTowers++] = LOWER_TOWER;
        _emptyTowers[_countEmptyTowers++] = GATE_TOWER_2;
    }

    _lastTarget = ALL_TARGETS;

    int allTargetsHp = 0;
    for ( int i = 0; i < ALL_TARGETS; ++i ) {
        allTargetsHp += _targetHp[i];
    }
    _totalHp = allTargetsHp;

    int skill = attackingHero.GetLevelSkill( Skill::Secondary::BALLISTICS );
    switch( skill ) {
    case Skill::Level::NONE:
        _damagePerHit = NORMAL_DAMAGE;
        _countHits = 1;
        break;
    case Skill::Level::BASIC:
        _damagePerHit = STRENGTH_DAMAGE;
        _countHits = 1;
        break;
    case Skill::Level::ADVANCED:
        _damagePerHit = NORMAL_DAMAGE;
        _countHits = 2;
        break;
    case Skill::Level::MASTER:
        _damagePerHit = STRENGTH_DAMAGE;
        _countHits = 2;
        break;
    case Skill::Level::EXPERT:
        _damagePerHit = STRENGTH_DAMAGE;
        _countHits = 3;
        break;
    case Skill::Level::GENIUS:
        _damagePerHit = ULTRA_DAMAGE;
        _countHits = 3;
        break;
    default:
        assert( 0 );
    }

    _countHits += attackingHero.GetBagArtifacts().getTotalArtifactEffectValue( fheroes2::ArtifactBonusType::EXTRA_CATAPULT_SHOTS );
    _countHits = std::clamp( _countHits, 1, MAX_CATAPULT_HITS );
}

Battle::CastleFortress::TargetId Battle::CastleFortress::SelectTarget( Rand::PCG32& gen ) const
{
    if ( _lastTarget < ALL_TARGETS && _targetHp[_lastTarget] > 0 ) {
        return _lastTarget;
    }

    if ( _countTargets > 0 ) {
        TargetId wallIds[4];
        uint32_t countWalls = 0;

        if ( _targetHp[UPPER_WALL_1] > 0 ) {
            wallIds[countWalls++] = UPPER_WALL_1;
        }
        if ( _targetHp[UPPER_WALL_2] > 0 ) {
            wallIds[countWalls++] = UPPER_WALL_2;
        }
        if ( _targetHp[LOWER_WALL_1] > 0 ) {
            wallIds[countWalls++] = LOWER_WALL_1;
        }
        if ( _targetHp[LOWER_WALL_2] > 0 ) {
            wallIds[countWalls++] = LOWER_WALL_2;
        }

        if ( countWalls > 2 ) {
            return wallIds[Rand::GetIndex( countWalls, gen )];
        }
        else {
            TargetId bridgeTowerIds[2];
            uint32_t countBridgeTowers = 0;

            if ( _targetHp[GATE_TOWER_1] > 0 ) {
                bridgeTowerIds[countBridgeTowers++] = GATE_TOWER_1;
            }
            if ( _targetHp[GATE_TOWER_2] > 0 ) {
                bridgeTowerIds[countBridgeTowers++] = GATE_TOWER_2;
            }

            if ( countBridgeTowers > 1 ) {
                return bridgeTowerIds[Rand::GetIndex( countBridgeTowers, gen )];
            }
            else {
                if ( countWalls > 0 ) {
                    return wallIds[Rand::GetIndex( countWalls, gen )];
                }

                if ( countBridgeTowers > 0 ) {
                    return bridgeTowerIds[0];
                }

                if ( _targetHp[GATE] > 0 ) {
                    return GATE;
                }

                TargetId towerIds[2];
                uint32_t countTowers = 0;

                if ( _targetHp[UPPER_TOWER] > 0 ) {
                    towerIds[countTowers++] = UPPER_TOWER;
                }
                if ( _targetHp[LOWER_TOWER] > 0 ) {
                    towerIds[countTowers++] = LOWER_TOWER;
                }

                if ( countTowers > 0 ) {
                    return towerIds[Rand::GetIndex( countTowers, gen )];
                }
                else {
                    if ( _targetHp[MAIN_TOWER] > 0 ) {
                        return MAIN_TOWER;
                    }
                }
            }
        }
    }

    return ALL_TARGETS;
}

int Battle::CastleFortress::AttackCatapult( HitInfo hits[MAX_CATAPULT_HITS], Rand::PCG32& gen )
{
    int countHits = 0;

    if ( _countTargets > 0 ) {
        for ( int i = 0; i < _countHits; ++i ) {
            TargetId tagId = SelectTarget( gen );

            if ( tagId < ALL_TARGETS ) {
                HitInfo& hit = hits[countHits++];
                hit.stuctId = tagId;

                int hp = _targetHp[tagId] - _damagePerHit;
                if ( hp > 0 ) {
                    hit.hp = IsWall( tagId ) ? ( ( hp + 2 ) / 3 ) : 1;
                    _targetHp[tagId] = hp;
                    _lastTarget = tagId;
                }
                else {
                    hit.hp = 0;
                    _targetHp[tagId] = 0;
                    _lastTarget = ALL_TARGETS;
                    --_countTargets;
                }
            }
            else {
                break;
            }
        }
    }

    // destroy empty towers after destruction main structures
    int countAddHits = _countHits - countHits;
    if ( countAddHits > 0 && _countEmptyTowers > 0 ) {
        if ( countAddHits > _countEmptyTowers ) {
            countAddHits = _countEmptyTowers;
        }

        for ( int i = 0; i < countAddHits; ++i ) {
            auto idx = static_cast<int>( Rand::GetIndex( _countEmptyTowers, gen ) );
            TargetId tagId = _emptyTowers[idx];

            if ( idx < --_countEmptyTowers ) {
                _emptyTowers[idx] = _emptyTowers[_countEmptyTowers];
            }

            HitInfo& hit = hits[countHits++];
            hit.stuctId = tagId;
            hit.hp = 0;
        }
    }

    return countHits;
}

int Battle::CastleFortress::AttackEarthquake( int spellPower, HitInfo hits[MAX_EARTHQUAKE_HITS], Rand::PCG32& gen )
{
    assert( spellPower > 0 );

    int countHits = 0;

    if ( _countTargets > 0 && spellPower > 0 ) {
        TargetId targets[ALL_TARGETS];
        int countTargets = 0;
        int totalHp = 0;

        for ( int i = 0; i < ALL_TARGETS; ++i ) {
            if ( _targetHp[i] > 0 ) {
                targets[countTargets++] = static_cast<TargetId>( i );
                totalHp += _targetHp[i];
            }
        }

        int totalDamage = spellPower * EARTHQUAKE_DAMAGE_BY_SPELLPOWER;
        if ( totalDamage < totalHp ) {
            totalHp -= totalDamage;

            do {
                auto idx = static_cast<int>( Rand::GetIndex( countTargets, gen ) );
                TargetId tagId = targets[idx];

                if ( idx < --countTargets ) {
                    targets[idx] = targets[countTargets];
                }

                HitInfo& hit = hits[countHits++];
                hit.stuctId = tagId;

                int hp = _targetHp[tagId];
                if ( hp <= totalDamage ) {
                    totalDamage -= hp;
                    hit.hp = 0;
                    _targetHp[tagId] = 0;
                    --_countTargets;
                }
                else {
                    hp -= totalDamage;
                    hit.hp = IsWall( tagId ) ? ( ( hp + 2 ) / 3 ) : 1;
                    _targetHp[tagId] = hp;
                    totalDamage = 0;
                }
            } while ( totalDamage > 0 );

            if ( _countEmptyTowers > 0 ) {
                int countAddHits = ( ( _totalHp - totalHp ) * _countEmptyTowers + _totalHp / 2 ) / _totalHp;
                if ( countAddHits > 0 ) {
                    for ( int i = 0; i < countAddHits; ++i ) {
                        auto idx = static_cast<int>( Rand::GetIndex( _countEmptyTowers, gen ) );
                        TargetId tagId = _emptyTowers[idx];

                        if ( idx < --_countEmptyTowers ) {
                            _emptyTowers[idx] = _emptyTowers[_countEmptyTowers];
                        }

                        HitInfo& hit = hits[countHits++];
                        hit.stuctId = tagId;
                        hit.hp = 0;
                    }
                }
            }
        }
        else {
            for ( int i = 0; i < countTargets; ++i ) {
                TargetId tagId = targets[i];
                _targetHp[tagId] = 0;

                HitInfo& hit = hits[countHits++];
                hit.stuctId = tagId;
                hit.hp = 0;
            }

            for ( int i = 0; i < _countEmptyTowers; ++i ) {
                HitInfo& hit = hits[countHits++];
                hit.stuctId = _emptyTowers[i];
                hit.hp = 0;
            }

            _countTargets = 0;
            _countEmptyTowers = 0;
        }
    }

    assert( countHits <= MAX_EARTHQUAKE_HITS );
    return countHits;
}

double Battle::CastleFortress::GetRelativeEarthquakeDamage( int spellPower ) const
{
    assert( spellPower > 0 );

    if ( _countTargets > 0 && spellPower > 0 ) {
        int totalHp = 0;
        for ( int i = 0; i < ALL_TARGETS; ++i ) {
            if ( _targetHp[i] > 0 ) {
                totalHp += _targetHp[i];
            }
        }

        int totalDamage = spellPower * EARTHQUAKE_DAMAGE_BY_SPELLPOWER;
        return ( totalDamage < totalHp ) ? ( static_cast<double>( totalDamage ) / totalHp ) : 1.0;
    }

    return 0;
}

double Battle::CastleFortress::GetRelativeCatapultPower( const HeroBase& attackingHero )
{
    double value = 1 + 0.5 * attackingHero.GetLevelSkill( Skill::Secondary::BALLISTICS );
    if ( attackingHero.GetBagArtifacts().getTotalArtifactEffectValue( fheroes2::ArtifactBonusType::EXTRA_CATAPULT_SHOTS ) > 0 ) {
        value += 0.5;
    }
    return value;
}

bool Battle::CastleFortress::IsWall( TargetId tagId )
{
    return
        ( tagId == Battle::CastleFortress::UPPER_WALL_1 ) ||
        ( tagId == Battle::CastleFortress::UPPER_WALL_2 ) ||
        ( tagId == Battle::CastleFortress::LOWER_WALL_1 ) ||
        ( tagId == Battle::CastleFortress::LOWER_WALL_2 );
}

const fheroes2::Point& Battle::CastleFortress::GetTargetPosition( TargetId tagId )
{
    assert( tagId >= UPPER_WALL_1 && tagId < ALL_TARGETS );
    return g_targetPos[tagId];
}

const int Battle::CastleFortress::GetTargetCellIdx( TargetId tagId )
{
    assert( tagId >= UPPER_WALL_1 && tagId < ALL_TARGETS && tagId != MAIN_TOWER );
    return g_targetCellIdx[tagId];
}
