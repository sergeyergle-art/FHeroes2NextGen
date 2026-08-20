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

#include "battle_arena.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <map>
#include <numeric>
#include <ostream>
#include <type_traits>

#include "ai_battle.h"
#include "army.h"
#include "army_troop.h"
#include "artifact.h"
#include "artifact_info.h"
#include "audio.h"
#include "battle_army.h"
#include "battle_bridge.h"
#include "battle_cell.h"
#include "battle_command.h"
#include "battle_interface.h"
#include "battle_tower.h"
#include "battle_troop.h"
#include "castle.h"
#include "ground.h"
#include "heroes.h"
#include "icn.h"
#include "localevent.h"
#include "logging.h"
#include "maps.h"
#include "maps_tiles.h"
#include "math_base.h"
#include "math_tools.h"
#include "monster.h"
#include "players.h"
#include "rand.h"
#include "skill.h"
#include "speed.h"
#include "spell_info.h"
#include "translations.h"
#include "world.h"

namespace
{
    Battle::Arena * arena = nullptr;

    template <typename T>
    Battle::Unit * getLastResurrectableUnitFromGraveyardTmpl( const Battle::Graveyard & graveyard, const HeroBase * commander, const int32_t index, const T & spells )
    {
        if ( commander == nullptr ) {
            return nullptr;
        }

        // Declare this variable both constexpr and static because different compilers disagree on whether there is a need to capture it in lambda expressions or not
        static constexpr bool isSingleSpell{ std::is_same_v<std::remove_cv_t<std::remove_reference_t<decltype( spells )>>, Spell> };

        if constexpr ( isSingleSpell ) {
            if ( !spells.isResurrect() ) {
                return nullptr;
            }
        }
        else {
            if ( !std::all_of( spells.begin(), spells.end(), []( const Spell & spell ) { return spell.isResurrect(); } ) ) {
                return nullptr;
            }
        }

        const std::vector<Battle::Unit *> units = graveyard.getUnits( index );

        const auto iter = std::find_if( units.rbegin(), units.rend(), [commander, &spells]( const Battle::Unit * unit ) {
            assert( unit != nullptr && !unit->isValid() );

            if ( unit->GetArmyColor() != commander->GetColor() ) {
                return false;
            }

            if constexpr ( isSingleSpell ) {
                return unit->AllowApplySpell( spells, commander );
            }
            else {
                return std::any_of( spells.begin(), spells.end(), [commander, unit]( const Spell & spell ) { return unit->AllowApplySpell( spell, commander ); } );
            }
        } );

        if ( iter == units.rend() ) {
            return nullptr;
        }

        return *iter;
    }

    int32_t GetCovr( int ground, Rand::PCG32 & gen )
    {
        std::vector<int32_t> covrs;
        covrs.reserve( 6 );

        switch ( ground ) {
        case Maps::Ground::SNOW:
            covrs.push_back( ICN::COVR0007 );
            covrs.push_back( ICN::COVR0008 );
            covrs.push_back( ICN::COVR0009 );
            covrs.push_back( ICN::COVR0010 );
            covrs.push_back( ICN::COVR0011 );
            covrs.push_back( ICN::COVR0012 );
            break;

        case Maps::Ground::WASTELAND:
            covrs.push_back( ICN::COVR0019 );
            covrs.push_back( ICN::COVR0020 );
            covrs.push_back( ICN::COVR0021 );
            covrs.push_back( ICN::COVR0022 );
            covrs.push_back( ICN::COVR0023 );
            covrs.push_back( ICN::COVR0024 );
            break;

        case Maps::Ground::DIRT:
            covrs.push_back( ICN::COVR0013 );
            covrs.push_back( ICN::COVR0014 );
            covrs.push_back( ICN::COVR0015 );
            covrs.push_back( ICN::COVR0016 );
            covrs.push_back( ICN::COVR0017 );
            covrs.push_back( ICN::COVR0018 );
            break;

        case Maps::Ground::GRASS:
            covrs.push_back( ICN::COVR0001 );
            covrs.push_back( ICN::COVR0002 );
            covrs.push_back( ICN::COVR0003 );
            covrs.push_back( ICN::COVR0004 );
            covrs.push_back( ICN::COVR0005 );
            covrs.push_back( ICN::COVR0006 );
            break;

        default:
            break;
        }

        return covrs.empty() ? ICN::UNKNOWN : Rand::GetWithGen( covrs, gen );
    }

    Battle::Unit * GetCurrentUnit( Battle::Units & attackingUnits, Battle::Units & defendingUnits, const bool attackingUnitsGoFirst, const bool ordersMode )
    {
        Battle::Unit * result = nullptr;

        const auto unitFilter = []( const Battle::Unit * unit ) { return unit->GetPriority() > 0; };

        const Battle::Units::iterator attackingUnitsIter = std::find_if( attackingUnits.begin(), attackingUnits.end(), unitFilter );
        const Battle::Units::iterator defendingUnitsIter = std::find_if( defendingUnits.begin(), defendingUnits.end(), unitFilter );

        if ( attackingUnitsIter != attackingUnits.end() && defendingUnitsIter != defendingUnits.end() ) {
            if ( ( *attackingUnitsIter )->GetPriority() == ( *defendingUnitsIter )->GetPriority() ) {
                result = attackingUnitsGoFirst ? *attackingUnitsIter : *defendingUnitsIter;
            }
            else {
                result = ( ( *attackingUnitsIter )->GetPriority() > ( *defendingUnitsIter )->GetPriority() ) ? *attackingUnitsIter : *defendingUnitsIter;
            }
        }
        else if ( attackingUnitsIter != attackingUnits.end() ) {
            result = *attackingUnitsIter;
        }
        else if ( defendingUnitsIter != defendingUnits.end() ) {
            result = *defendingUnitsIter;
        }

        if ( result && ordersMode ) {
            if ( attackingUnitsIter != attackingUnits.end() && result == *attackingUnitsIter ) {
                attackingUnits.erase( attackingUnitsIter );
            }
            else if ( defendingUnitsIter != defendingUnits.end() && result == *defendingUnitsIter ) {
                defendingUnits.erase( defendingUnitsIter );
            }
        }

        return result;
    }

    Battle::Unit * GetCurrentUnit( const Battle::Force & attackingArmy, const Battle::Force & defendingArmy, const PlayerColor preferredColor )
    {
        Battle::Units attackingUnits( attackingArmy.getUnits(), Battle::Units::REMOVE_INVALID_UNITS );
        Battle::Units defendingUnits( defendingArmy.getUnits(), Battle::Units::REMOVE_INVALID_UNITS );

        attackingUnits.SortFastest();
        defendingUnits.SortFastest();

        Battle::Unit * result = GetCurrentUnit( attackingUnits, defendingUnits, preferredColor != defendingArmy.GetColor(), false );
        if ( result == nullptr ) {
            return result;
        }

        assert( result->isValid() );

        return result;
    }

    void UpdateOrderOfUnits( const Battle::Force & attackingArmy, const Battle::Force & defendingArmy, const Battle::Unit * currentUnit, PlayerColor preferredColor,
                             const Battle::Units & orderHistory, Battle::Units & orderOfUnits )
    {
        orderOfUnits.assign( orderHistory.begin(), orderHistory.end() );

        Battle::Units attackingUnits( attackingArmy.getUnits(), Battle::Units::REMOVE_INVALID_UNITS );
        Battle::Units defendingUnits( defendingArmy.getUnits(), Battle::Units::REMOVE_INVALID_UNITS );

        attackingUnits.SortFastest();
        defendingUnits.SortFastest();

        while ( true ) {
            Battle::Unit * unit = GetCurrentUnit( attackingUnits, defendingUnits, preferredColor != defendingArmy.GetColor(), true );
            if ( unit == nullptr ) {
                break;
            }

            assert( unit->isValid() );

            if ( unit == currentUnit ) {
                continue;
            }

            preferredColor = ( unit->GetArmyColor() == attackingArmy.GetColor() ) ? defendingArmy.GetColor() : attackingArmy.GetColor();

            orderOfUnits.push_back( unit );
        }
    }
}

Battle::Arena * Battle::GetArena()
{
    return arena;
}

const Castle * Battle::Arena::GetCastle()
{
    assert( arena != nullptr );

    return arena->castle;
}

Battle::Bridge * Battle::Arena::GetBridge()
{
    assert( arena != nullptr );

    return arena->_bridge.get();
}

Battle::Board * Battle::Arena::GetBoard()
{
    assert( arena != nullptr );

    return &arena->board;
}

Battle::Graveyard * Battle::Arena::GetGraveyard()
{
    assert( arena != nullptr );

    return &arena->_graveyard;
}

Battle::Interface * Battle::Arena::GetInterface()
{
    assert( arena != nullptr );

    return arena->_interface.get();
}

Battle::Tower * Battle::Arena::GetTower( const TowerType type )
{
    assert( arena != nullptr );

    switch ( type ) {
    case TowerType::TWR_LEFT:
        return arena->_towers[0].get();
    case TowerType::TWR_CENTER:
        return arena->_towers[1].get();
    case TowerType::TWR_RIGHT:
        return arena->_towers[2].get();
    case TowerType::TWR_GATE_1:
        return arena->_towers[3].get();
    case TowerType::TWR_GATE_2:
        return arena->_towers[4].get();
    default:
        break;
    }
    return nullptr;
}

bool Battle::Arena::isAnyTowerPresent()
{
    assert( arena != nullptr );

    return std::any_of( arena->_towers.begin(), arena->_towers.end(), []( const auto & twr ) { return twr && twr->isValid(); } );
}

void Battle::Arena::InitFortress()
{
    assert( castle );

    if ( castle->isBuild( BUILD_LEFTTURRET ) ) {
        _towers[0] = std::make_unique<Tower>( *castle, TowerType::TWR_LEFT, _uidGenerator.GetUnique() );
        _towers[3] = std::make_unique<Tower>( *castle, TowerType::TWR_GATE_1, _uidGenerator.GetUnique() );
    }

    _towers[1] = std::make_unique<Tower>( *castle, TowerType::TWR_CENTER, _uidGenerator.GetUnique() );

    if ( castle->isBuild( BUILD_RIGHTTURRET ) ) {
        _towers[2] = std::make_unique<Tower>( *castle, TowerType::TWR_RIGHT, _uidGenerator.GetUnique() );
        _towers[4] = std::make_unique<Tower>( *castle, TowerType::TWR_GATE_2, _uidGenerator.GetUnique() );
    }

    _bridge = std::make_unique<Bridge>();

    // catapult cell
    board[POS_CATAPULT].SetObject( 1 );

    // wall (3,2,1,0)
    const int wallObject = castle->isFortificationBuilt() ? 3 : 2;
    board[CastleFortress::POS_UPPER_WALL_1].SetObject( wallObject );
    board[CastleFortress::POS_UPPER_WALL_2].SetObject( wallObject );
    board[CastleFortress::POS_LOWER_WALL_1].SetObject( wallObject );
    board[CastleFortress::POS_LOWER_WALL_2].SetObject( wallObject );

    // NOTE: All towers can be destroyed but can not be passable.
    // Initially their condition is 2 and becomes 1 after their upper part is destroyed.
    // The tower's condition should not be 0 to keep them non-passable by the troops.

    // Turret towers with small ballista.
    board[CastleFortress::POS_UPPER_TOWER].SetObject( 2 );
    board[CastleFortress::POS_LOWER_TOWER].SetObject( 2 );

    // Towers near the bridge. Does not shoot arrows. Can be damaged only by the Earthquake spell.
    board[CastleFortress::POS_GATE_TOWER_1].SetObject( 2 );
    board[CastleFortress::POS_GATE_TOWER_2].SetObject( 2 );

    // bridge
    board[CastleFortress::POS_GATE].SetObject( 1 );

    // top bound
    board[CastleFortress::POS_UPPER_BOUND_1].SetObject( 0x40 );
    board[CastleFortress::POS_UPPER_BOUND_2].SetObject( 0x40 );
    board[CastleFortress::POS_UPPER_BOUND_3].SetObject( 0x40 );

    // bottom bound
    board[CastleFortress::POS_EMPTY_TOWER].SetObject( 2 );
    board[CastleFortress::POS_LOWER_BOUND_1].SetObject( 0x40 );
    board[CastleFortress::POS_LOWER_BOUND_2].SetObject( 0x40 );

    _fortress.Init( *castle, *_attackingArmy->GetCommander() );
}

Battle::Arena::Arena( Army & attackingArmy, Army & defendingArmy, const int32_t tileIndex, const bool isShowInterface, Rand::PCG32 & randomGenerator )
    : castle( world.getCastleEntrance( Maps::GetPoint( tileIndex ) ) )
    , _isTown( castle != nullptr )
    , _randomGenerator( randomGenerator )
{
    _usedSpellsByAttacker.reserve( 8 );
    _usedSpellsByDefender.reserve( 8 );

    assert( arena == nullptr );
    arena = this;

    _attackingArmy = std::make_unique<Force>( attackingArmy, false, _uidGenerator );
    _defendingArmy = std::make_unique<Force>( defendingArmy, true, _uidGenerator );

    // If this is a siege of a town, then there is in fact no castle
    if ( castle && !castle->isCastle() ) {
        castle = nullptr;
    }

    if ( isShowInterface ) {
        _interface = std::make_unique<Interface>( *this, tileIndex );
        board.SetArea( _interface->GetArea() );

        _orderOfUnits = std::make_shared<Units>();
        _orderOfUnits->reserve( 25 );
        _interface->SetOrderOfUnits( _orderOfUnits );
    }
    else {
        // There is no interface - force the auto combat mode for the human player
        if ( attackingArmy.isControlHuman() ) {
            _autoCombatColors |= attackingArmy.GetColor();
        }
        if ( defendingArmy.isControlHuman() ) {
            _autoCombatColors |= defendingArmy.GetColor();
        }
    }

    if ( castle ) {
        InitFortress();
    }
    else
    // set obstacles
    {
        Rand::PCG32 seededGen( world.GetMapSeed() + static_cast<uint32_t>( tileIndex ) );

        _covrIcnId = Rand::GetWithGen( 0, 99, seededGen ) < 40 ? GetCovr( world.getTile( tileIndex ).GetGround(), seededGen ) : ICN::UNKNOWN;
        if ( _covrIcnId != ICN::UNKNOWN ) {
            board.SetCovrObjects( _covrIcnId );
        }

        board.SetCobjObjects( world.getTile( tileIndex ), seededGen );
    }

    AI::BattlePlanner::Get().battleBegins();

    if ( _interface ) {
        _interface->fullRedraw();

        // Wait for the end of M82::PREBATTL playback. Make sure that we check the music status first as HandleEvents() call is not instant.
        LocalEvent & le = LocalEvent::Get();
        while ( Mixer::isPlaying( -1 ) && le.HandleEvents() ) {
            if ( le.isKeyPressed( fheroes2::Key::KEY_ESCAPE ) || le.MouseClickMiddle() || le.MouseClickRight() ) {
                // Cancel waiting for M82::PREBATTL to over and start the battle.
                break;
            }
        }
    }
}

Battle::Arena::~Arena()
{
    assert( arena == this );
    arena = nullptr;
}

void Battle::Arena::UnitTurn( const Units & orderHistory )
{
    assert( _currentUnit && _currentUnit->isValid() );

    DEBUG_LOG( DBG_BATTLE, DBG_TRACE, _currentUnit->String( true ) )

    if ( _currentUnit->isAffectedByMorale() ) {
        _currentUnit->SetRandomMorale( _randomGenerator );
    }

    assert( !_currentUnit->AllModes( MORALE_GOOD | MORALE_BAD ) );

    bool endOfTurn = false;

    while ( !endOfTurn ) {
        // There should be no dead units on the board at the beginning of each iteration
        assert( std::all_of( board.begin(), board.end(), []( const Cell & cell ) { return ( cell.GetUnit() == nullptr || cell.GetUnit()->isValid() ); } ) );

        Actions actions;

        if ( _interface ) {
            _interface->getPendingActions( actions );
        }

        if ( !actions.empty() ) {
            // Pending actions from the user interface (such as toggling the auto combat on/off) have "already occurred"
            // and therefore should be handled first, before any other actions. Just skip the rest of the branches.
        }
        else if ( _currentUnit->GetSpeed() == Speed::STANDING ) {
            // Unit has either finished its turn, is dead, or has become immovable due to some spell. Even if the
            // unit died or has become immovable during its turn without performing any action, its turn is still
            // considered completed.
            _currentUnit->SetModes( TR_MOVED );

            endOfTurn = true;
        }
        else if ( _currentUnit->Modes( MORALE_BAD ) ) {
            actions.emplace_back( Command::MORALE, _currentUnit->GetUID(), false );
        }
        else {
            // This unit will certainly perform at least one full-fledged action
            _lastActiveUnitArmyColor = _currentUnit->GetArmyColor();

            if ( _bridge ) {
                _bridge->SetPassability( *_currentUnit );
            }

            if ( ( _currentUnit->GetCurrentControl() & CONTROL_AI ) || ( _autoCombatColors & _currentUnit->GetCurrentColor() ) ) {
                AI::BattlePlanner::Get().BattleTurn( *this, *_currentUnit, actions );
            }
            else {
                assert( _interface != nullptr );

                _interface->HumanTurn( *_currentUnit, actions );
            }
        }

        const uint64_t newStream = std::accumulate( actions.cbegin(), actions.cend(), _randomGenerator.getStream(),
                                                    []( const uint64_t stream, const Command & cmd ) { return cmd.updatePCG32Stream( stream ); } );
        _randomGenerator.setStream( newStream );

        while ( !actions.empty() ) {
            ApplyAction( actions.front() );
            actions.pop_front();

            board.removeDeadUnits();

            if ( _orderOfUnits ) {
                // Applied action could kill someone or affect the speed of some unit, update the order of units
                UpdateOrderOfUnits( *_attackingArmy, *_defendingArmy, _currentUnit, GetOppositeColor( _currentUnit->GetArmyColor() ), orderHistory, *_orderOfUnits );
            }

            if ( !BattleValid() ) {
                endOfTurn = true;
                break;
            }

            if ( _currentUnit->AllModes( TR_MOVED | MORALE_GOOD ) && !_currentUnit->Modes( TR_SKIP ) && _currentUnit->GetSpeed( false, true ) > Speed::STANDING ) {
                actions.emplace_back( Command::MORALE, _currentUnit->GetUID(), true );
            }
        }

        // There should be no dead units on the board at the end of each iteration
        assert( std::all_of( board.begin(), board.end(), []( const Cell & cell ) { return ( cell.GetUnit() == nullptr || cell.GetUnit()->isValid() ); } ) );
    }
}

bool Battle::Arena::BattleValid() const
{
    return _attackingArmy->isValid() && _defendingArmy->isValid() && 0 == _battleResult.attacker && 0 == _battleResult.defender;
}

void Battle::Arena::Turns()
{
    ++_turnNumber;

    DEBUG_LOG( DBG_BATTLE, DBG_TRACE, _turnNumber )

    if ( _interface ) {
        _interface->RedrawActionNewTurn();
    }

    _attackingArmy->NewTurn();
    _defendingArmy->NewTurn();

    UpdateSpellsInBookByBattle();

    // History of unit order on the current turn
    Units orderHistory;

    if ( _orderOfUnits ) {
        orderHistory.reserve( 25 );

        // Build the initial order of units
        UpdateOrderOfUnits( *_attackingArmy, *_defendingArmy, nullptr, GetOppositeColor( _lastActiveUnitArmyColor ), orderHistory, *_orderOfUnits );
    }

    {
        bool towersActed = false;
        bool catapultActed = false;

        while ( BattleValid() ) {
            // We can get the nullptr here if there are no units left waiting for their turn
            _currentUnit = GetCurrentUnit( *_attackingArmy, *_defendingArmy, GetOppositeColor( _lastActiveUnitArmyColor ) );

            if ( _orderOfUnits ) {
                // Add unit to the history
                if ( _currentUnit ) {
                    orderHistory.push_back( _currentUnit );
                }

                // Update the order of units
                UpdateOrderOfUnits( *_attackingArmy, *_defendingArmy, _currentUnit,
                                    GetOppositeColor( _currentUnit ? _currentUnit->GetArmyColor() : _lastActiveUnitArmyColor ), orderHistory, *_orderOfUnits );
            }

            if ( castle ) {
                // Catapult acts either during the turn of the first unit from the attacking army, or at the end of the
                // turn if none of the units from the attacking army are able to act (for example, all are blinded)
                if ( !catapultActed && ( _currentUnit == nullptr || _currentUnit->GetColor() == _attackingArmy->GetColor() ) ) {
                    CatapultAction();

                    catapultActed = true;
                }

                // Castle towers act either during the turn of the first unit from the defending army, or at the end of
                // the turn if none of the units from the defending army are able to act (for example, all are blinded)
                if ( !towersActed && ( _currentUnit == nullptr || _currentUnit->GetColor() == _defendingArmy->GetColor() ) ) {
                    const auto towerAction = [this, &orderHistory]( const size_t idx ) {
                        assert( idx < std::size( _towers ) );

                        if ( _towers[idx] == nullptr || !_towers[idx]->isValid() ) {
                            return;
                        }

                        TowerAction( *_towers[idx] );

                        if ( _orderOfUnits ) {
                            // Tower could kill someone, update the order of units
                            UpdateOrderOfUnits( *_attackingArmy, *_defendingArmy, _currentUnit,
                                                GetOppositeColor( _currentUnit ? _currentUnit->GetArmyColor() : _lastActiveUnitArmyColor ), orderHistory,
                                                *_orderOfUnits );
                        }
                    };

                    towerAction( 1 );
                    towerAction( 3 );
                    towerAction( 4 );
                    towerAction( 0 );
                    towerAction( 2 );

                    towersActed = true;

                    // If the towers have killed the last enemy unit, the battle is over
                    if ( !BattleValid() ) {
                        break;
                    }
                }
            }

            if ( _currentUnit == nullptr ) {
                // There are no units left waiting for their turn
                break;
            }

            UnitTurn( orderHistory );
        }
    }

    // Check if the battle is over
    if ( !_attackingArmy->isValid() || ( _battleResult.attacker & ( RESULT_RETREAT | RESULT_SURRENDER ) ) ) {
        _battleResult.attacker |= RESULT_LOSS;
        // Check if any of the original troops in the defending army are still alive
        _battleResult.defender = _defendingArmy->isValid( false ) ? RESULT_WINS : RESULT_LOSS;
    }
    else if ( !_defendingArmy->isValid() || ( _battleResult.defender & ( RESULT_RETREAT | RESULT_SURRENDER ) ) ) {
        _battleResult.defender |= RESULT_LOSS;
        // Check if any of the original troops in the attacking army are still alive
        _battleResult.attacker = _attackingArmy->isValid( false ) ? RESULT_WINS : RESULT_LOSS;
    }

    // If the battle is over, calculate the experience and the number of units killed
    if ( _battleResult.attacker || _battleResult.defender ) {
        _battleResult.attackerExperience = _defendingArmy->calculateExperienceBasedOnLosses();
        _battleResult.defenderExperience = _attackingArmy->calculateExperienceBasedOnLosses();

        const HeroBase * attackingArmyCommander = _attackingArmy->GetCommander();
        const HeroBase * defendingArmyCommander = _defendingArmy->GetCommander();

        // Attacker (or defender) gets an experience bonus if the enemy army was under the command of a hero who was defeated (i.e. did not retreat or surrender)
        if ( attackingArmyCommander && attackingArmyCommander->isHeroes() && !( _battleResult.attacker & ( RESULT_RETREAT | RESULT_SURRENDER ) ) ) {
            _battleResult.defenderExperience += 500;
        }
        if ( defendingArmyCommander && defendingArmyCommander->isHeroes() && !( _battleResult.defender & ( RESULT_RETREAT | RESULT_SURRENDER ) ) ) {
            _battleResult.attackerExperience += 500;
        }

        // Attacker gets an additional experience bonus after successfully besieging a town or castle
        if ( _isTown ) {
            _battleResult.attackerExperience += 500;
        }

        const Force * losingArmy
            = ( _battleResult.attacker & RESULT_LOSS ? _attackingArmy.get() : ( _battleResult.defender & RESULT_LOSS ? _defendingArmy.get() : nullptr ) );
        _battleResult.numOfDeadUnitsForNecromancy = losingArmy ? losingArmy->calculateNumberOfDeadUnitsForNecromancy() : 0;
    }
}

void Battle::Arena::TowerAction( const Tower & twr )
{
    // There should be no dead units on the board at this moment
    assert( std::all_of( board.begin(), board.end(), []( const Cell & cell ) { return ( cell.GetUnit() == nullptr || cell.GetUnit()->isValid() ); } ) );

    // Target unit and its threat level
    std::pair<const Unit *, double> targetInfo{ nullptr, std::numeric_limits<double>::lowest() };

    for ( const Cell & cell : board ) {
        const Unit * unit = cell.GetUnit();

        if ( unit == nullptr || unit->GetColor() == twr.GetColor() || ( unit->isWide() && unit->GetTailIndex() == cell.GetIndex() ) ) {
            continue;
        }

        const double unitThreatLevel = unit->evaluateThreatForUnit( twr );

        if ( targetInfo.first == nullptr || targetInfo.second < unitThreatLevel ) {
            targetInfo = { unit, unitThreatLevel };
        }
    }

    if ( targetInfo.first == nullptr ) {
        DEBUG_LOG( DBG_BATTLE, DBG_INFO, "No target found for the tower" )

        return;
    }

    using TowerGetTypeUnderlyingType = typename std::underlying_type_t<decltype( twr.GetType() )>;
    static_assert( std::is_same_v<TowerGetTypeUnderlyingType, uint8_t> );

    Command cmd( Command::TOWER, static_cast<TowerGetTypeUnderlyingType>( twr.GetType() ), targetInfo.first->GetUID() );

    ApplyAction( cmd );

    board.removeDeadUnits();
}

void Battle::Arena::CatapultAction()
{
    if ( !_fortress.HasCatapultTargets() ) {
        return;
    }

    CastleFortress::HitInfo hits[CastleFortress::MAX_CATAPULT_HITS];
    int shots = _fortress.AttackCatapult( hits, _randomGenerator );
    if ( shots <= 0 ) {
        assert( 0 );
        return;
    }

    Command cmd( Command::CATAPULT );

    // reverse order
    for ( int i = shots - 1; i >= 0; --i ) {
        cmd << hits[i].hp << static_cast<int>( hits[i].stuctId );
    }

    cmd << shots;

    ApplyAction( cmd );
}

Battle::Indexes Battle::Arena::GetPath( const Unit & unit, const Position & position )
{
    const Indexes result = _battlePathfinder.buildPath( unit, position );

    if ( IS_DEBUG( DBG_BATTLE, DBG_TRACE ) && !result.empty() ) {
        std::string pathStr;
        pathStr.reserve( Speed::INSTANT * 4 );

        std::for_each( result.begin(), result.end(), [&pathStr]( const int32_t item ) {
            pathStr += std::to_string( item );
            pathStr += ", ";
        } );

        DEBUG_LOG( DBG_BATTLE, DBG_TRACE, pathStr )
    }

    return result;
}

Battle::Unit * Battle::Arena::GetTroopBoard( int32_t index )
{
    return Board::isValidIndex( index ) ? board[index].GetUnit() : nullptr;
}

const Battle::Unit * Battle::Arena::GetTroopBoard( int32_t index ) const
{
    return Board::isValidIndex( index ) ? board[index].GetUnit() : nullptr;
}

HeroBase * Battle::Arena::getAttackingArmyCommander() const
{
    return _attackingArmy->GetCommander();
}

HeroBase * Battle::Arena::getDefendingArmyCommander() const
{
    return _defendingArmy->GetCommander();
}

PlayerColor Battle::Arena::getAttackingArmyColor() const
{
    return _attackingArmy->GetColor();
}

PlayerColor Battle::Arena::getDefendingArmyColor() const
{
    return _defendingArmy->GetColor();
}

PlayerColor Battle::Arena::GetCurrentColor() const
{
    // This method should never be called in cases where there may not be an active unit
    if ( _currentUnit == nullptr ) {
        assert( 0 );
        return PlayerColor::NONE;
    }

    return _currentUnit->GetCurrentOrArmyColor();
}

PlayerColor Battle::Arena::GetOppositeColor( const PlayerColor col ) const
{
    return col == getAttackingArmyColor() ? getDefendingArmyColor() : getAttackingArmyColor();
}

Battle::Unit * Battle::Arena::GetTroopUID( uint32_t uid )
{
    if ( const auto iter = std::find_if( _attackingArmy->begin(), _attackingArmy->end(), [uid]( const Unit * unit ) { return unit->isUID( uid ); } );
         iter != _attackingArmy->end() ) {
        return *iter;
    }

    if ( const auto iter = std::find_if( _defendingArmy->begin(), _defendingArmy->end(), [uid]( const Unit * unit ) { return unit->isUID( uid ); } );
         iter != _defendingArmy->end() ) {
        return *iter;
    }

    return nullptr;
}

const Battle::Unit * Battle::Arena::GetTroopUID( uint32_t uid ) const
{
    if ( const auto iter = std::find_if( _attackingArmy->begin(), _attackingArmy->end(), [uid]( const Unit * unit ) { return unit->isUID( uid ); } );
         iter != _attackingArmy->end() ) {
        return *iter;
    }

    if ( const auto iter = std::find_if( _defendingArmy->begin(), _defendingArmy->end(), [uid]( const Unit * unit ) { return unit->isUID( uid ); } );
         iter != _defendingArmy->end() ) {
        return *iter;
    }

    return nullptr;
}

void Battle::Arena::FadeArena( bool clearMessageLog ) const
{
    if ( !_interface ) {
        return;
    }

    _interface->FadeArena( clearMessageLog );
}

int32_t Battle::Arena::GetFreePositionNearHero( const PlayerColor heroColor ) const
{
    int cellIds[3];
    if ( _attackingArmy->GetColor() == heroColor ) {
        cellIds[0] = Board::FirstRowCellIdx( 1 );
        cellIds[1] = Board::FirstRowCellIdx( 2 );
        cellIds[2] = Board::FirstRowCellIdx( 3 );
    }
    else if ( _defendingArmy->GetColor() == heroColor ) {
        cellIds[0] = Board::LastRowCellIdx( 1 );
        cellIds[1] = Board::LastRowCellIdx( 2 );
        cellIds[2] = Board::LastRowCellIdx( 3 );
    }
    else {
        // Some third color?
        return -1;
    }

    for ( int i = 0; i < 3; i++ ) {
        int cellId = cellIds[i];
        if ( board[cellId].isPassable( true ) ) {
            // TODO: remove this temporary assertion
            assert( board[cellId].GetUnit() == nullptr );

            return cellId;
        }
    }

    return -1;
}

bool Battle::Arena::CanSurrenderOpponent( PlayerColor color ) const
{
    const HeroBase * hero = getCommander( color );
    const HeroBase * enemyHero = getEnemyCommander( color );
    return hero && hero->isHeroes() && enemyHero && ( enemyHero->isHeroes() || enemyHero->isCaptain() );
}

bool Battle::Arena::CanRetreatOpponent( const PlayerColor color ) const
{
    const HeroBase * hero = getCommander( color );
    return hero && hero->isHeroes() && ( color == _attackingArmy->GetColor() || hero->inCastle() == nullptr );
}

bool Battle::Arena::isSpellcastDisabled() const
{
    const HeroBase * attackingHero = _attackingArmy->GetCommander();
    if ( attackingHero != nullptr && attackingHero->GetBagArtifacts().isArtifactBonusPresent( fheroes2::ArtifactBonusType::DISABLE_ALL_SPELL_COMBAT_CASTING ) ) {
        return true;
    }

    const HeroBase * defendingHero = _defendingArmy->GetCommander();
    if ( defendingHero != nullptr && defendingHero->GetBagArtifacts().isArtifactBonusPresent( fheroes2::ArtifactBonusType::DISABLE_ALL_SPELL_COMBAT_CASTING ) ) {
        return true;
    }

    return false;
}

bool Battle::Arena::isDisableCastSpell( const Spell & spell, std::string * msg /* = nullptr */ ) const
{
    if ( isSpellcastDisabled() ) {
        if ( msg ) {
            *msg = _( "The Sphere of Negation artifact is in effect for this battle, disabling all combat spells." );
        }
        return true;
    }

    const HeroBase * commander = GetCurrentCommander();
    if ( commander == nullptr ) {
        // This should not normally happen, but there are places where the ability to "basically"
        // cast a spell is checked before checking for the presence of a commanding hero.
        if ( msg ) {
            *msg = _( "You cannot cast spells without a commanding hero." );
        }
        return true;
    }

    if ( commander->Modes( Heroes::SPELLCASTED ) ) {
        if ( msg ) {
            *msg = _( "You have already cast a spell this round." );
        }
        return true;
    }

    // An empty spell can be used to test the ability to cast spells in principle
    if ( !spell.isValid() ) {
        return false;
    }

    if ( spell == Spell::EARTHQUAKE ) {
        if ( castle == nullptr ) {
            if ( msg ) {
                *msg = _( "That spell will have no effect!" );
            }
            return true;
        }

        return false;
    }

    if ( spell.isSummon() ) {
        const Monster mons( spell );
        assert( mons.isValid() && mons.isElemental() );

        const Unit * elem = GetCurrentForce().FindMode( CAP_SUMMONELEM );
        if ( elem && elem->GetID() != mons.GetID() ) {
            if ( msg ) {
                *msg = _( "You may only summon one type of elemental per combat." );
            }
            return true;
        }

        if ( GetFreePositionNearHero( GetCurrentColor() ) < 0 ) {
            if ( msg ) {
                *msg = _( "There is no open space adjacent to your hero where you can summon an Elemental to." );
            }
            return true;
        }

        return false;
    }

    for ( const Cell & cell : board ) {
        if ( const Battle::Unit * unit = cell.GetUnit(); unit != nullptr ) {
            if ( unit->AllowApplySpell( spell, commander ) ) {
                return false;
            }

            continue;
        }

        if ( isAbleToResurrectFromGraveyard( cell.GetIndex(), spell ) ) {
            return false;
        }
    }

    if ( msg ) {
        *msg = _( "That spell will have no effect!" );
    }
    return true;
}

bool Battle::Arena::isAbleToResurrectFromGraveyard( const int32_t index, const Spell & spell ) const
{
    if ( !spell.isResurrect() ) {
        return false;
    }

    const HeroBase * hero = GetCurrentCommander();
    if ( hero == nullptr ) {
        return false;
    }

    const Unit * unit = getLastResurrectableUnitFromGraveyard( index, spell );
    if ( unit == nullptr ) {
        return false;
    }

    if ( !unit->AllowApplySpell( spell, hero ) ) {
        return false;
    }

    const int headIndex = unit->GetHeadIndex();
    assert( Board::isValidIndex( headIndex ) );

    // No other unit should stand on a corpse
    if ( Board::GetCell( headIndex )->GetUnit() != nullptr ) {
        return false;
    }

    if ( !unit->isWide() ) {
        return true;
    }

    const int tailIndex = unit->GetTailIndex();
    assert( Board::isValidIndex( tailIndex ) );

    // No other unit should stand on a corpse
    if ( Board::GetCell( tailIndex )->GetUnit() != nullptr ) {
        return false;
    }

    return true;
}

const Battle::Unit * Battle::Arena::getLastUnitFromGraveyard( const int32_t index ) const
{
    return _graveyard.getLastUnit( index );
}

const Battle::Unit * Battle::Arena::getLastResurrectableUnitFromGraveyard( const int32_t index ) const
{
    static const std::vector<Spell> resurrectionSpells = []() {
        std::vector<Spell> result;

        for ( int spellId = Spell::NONE; spellId < Spell::SPELL_COUNT; ++spellId ) {
            const Spell spell( spellId );
            if ( !spell.isResurrect() ) {
                continue;
            }

            result.push_back( spell );
        }

        return result;
    }();

    return getLastResurrectableUnitFromGraveyardTmpl( _graveyard, GetCurrentCommander(), index, resurrectionSpells );
}

Battle::Unit * Battle::Arena::getLastResurrectableUnitFromGraveyard( const int32_t index, const Spell & spell ) const
{
    return getLastResurrectableUnitFromGraveyardTmpl( _graveyard, GetCurrentCommander(), index, spell );
}

std::vector<const Battle::Unit *> Battle::Arena::getGraveyardUnits( const int32_t index ) const
{
    const std::vector<Battle::Unit *> units = _graveyard.getUnits( index );

    return { units.begin(), units.end() };
}

Battle::Indexes Battle::Arena::getCellsOccupiedByGraveyard() const
{
    return _graveyard.getOccupiedCells();
}

void Battle::Arena::applyDamageToCastleDefenseStructure( CastleFortress::TargetId target, int hp )
{
    assert( castle );

    switch ( target ) {
    // Sections of the castle wall can be completely destroyed and it will be possible to pass through the corresponding cells.
    case CastleFortress::UPPER_WALL_1:
    case CastleFortress::UPPER_WALL_2:
    case CastleFortress::LOWER_WALL_1:
    case CastleFortress::LOWER_WALL_2:
    {
        auto& cell = board[CastleFortress::GetTargetCellIdx( target )];
        int objHp = cell.GetObject();
        int newHp = hp;

        assert( newHp >= 0 && newHp <= objHp );

        if ( newHp >= 0 && newHp < objHp ) {
            cell.SetObject( newHp );
        }
        break;
    }

    // Tete-de-pont towers can be damaged, but not fully demolished (it is still impossible to pass through the corresponding cells).
    case CastleFortress::EMPTY_TOWER:
    {
        auto& cell = board[CastleFortress::GetTargetCellIdx( target )];
        int objHp = cell.GetObject();
        int newHp = hp + 1;

        assert( newHp >= 1 && newHp <= objHp );

        if ( newHp >= 1 && newHp < objHp ) {
            cell.SetObject( newHp );
        }
        break;
    }

    case CastleFortress::GATE_TOWER_1:
    case CastleFortress::GATE_TOWER_2:
    {
        auto& cell = board[CastleFortress::GetTargetCellIdx( target )];
        int objHp = cell.GetObject();
        int newHp = hp + 1;

        assert( newHp >= 1 && newHp <= objHp );

        if ( newHp >= 1 && newHp < objHp ) {
            cell.SetObject( newHp );

            int towerIdx = ( target == CastleFortress::GATE_TOWER_1 ) ? 3 : 4;
            if ( _towers[towerIdx] ) {
                _towers[towerIdx]->SetDestroyed();
            }
        }
        break;
    }

    case CastleFortress::UPPER_TOWER:
    case CastleFortress::LOWER_TOWER:
    {
        auto& cell = board[CastleFortress::GetTargetCellIdx( target )];
        int objHp = cell.GetObject();
        int newHp = hp + 1;

        assert( newHp >= 1 && newHp <= objHp );

        if ( newHp >= 1 && newHp < objHp ) {
            cell.SetObject( newHp );

            int towerIdx = ( target == CastleFortress::UPPER_TOWER ) ? 0 : 2;
            if ( _towers[towerIdx] ) {
                _towers[towerIdx]->SetDestroyed();
            }
        }
        break;
    }

    case CastleFortress::MAIN_TOWER:
    {
        assert( hp == 0 || hp == 1 );

        if ( hp == 0 ) {
            if ( _towers[1] ) {
                _towers[1]->SetDestroyed();
            }
        }
        break;
    }

    case CastleFortress::GATE:
    {
        assert( hp == 0 || hp == 1 );

        if ( hp == 0 ) {
            _bridge->SetDestroyed();
        }
        break;
    }

    default:
        assert( 0 );
        break;
    }
}

double Battle::Arena::GetRelativeEarthquakeDamage( int spellPower ) const
{
    return _fortress.GetRelativeEarthquakeDamage( spellPower );
}

const HeroBase * Battle::Arena::getCommander( const PlayerColor color ) const
{
    return ( _attackingArmy->GetColor() == color ) ? _attackingArmy->GetCommander() : _defendingArmy->GetCommander();
}

const HeroBase * Battle::Arena::getEnemyCommander( const PlayerColor color ) const
{
    return ( _attackingArmy->GetColor() == color ) ? _defendingArmy->GetCommander() : _attackingArmy->GetCommander();
}

const HeroBase * Battle::Arena::GetCurrentCommander() const
{
    return getCommander( GetCurrentColor() );
}

Battle::Unit * Battle::Arena::CreateElemental( const Spell & spell )
{
    // TODO: this assertion is here to thoroughly check all the complex limitations of the Summon Elemental spell
    assert( !isDisableCastSpell( spell ) );

    const HeroBase * hero = GetCurrentCommander();
    assert( hero != nullptr );

    const int32_t idx = GetFreePositionNearHero( GetCurrentColor() );
    assert( Board::isValidIndex( idx ) );

    const Monster mons( spell );
    assert( mons.isValid() && mons.isElemental() && !mons.isWide() );

    DEBUG_LOG( DBG_BATTLE, DBG_TRACE, mons.GetName() << ", position: " << idx )

    const bool reflect = ( hero == _defendingArmy->GetCommander() );
    const uint32_t count = fheroes2::getSummonMonsterCount( spell, hero->GetPower(), hero );

    Position pos;
    pos.Set( idx, mons.isWide(), reflect );

    // An elemental could not be a wide unit
    assert( pos.GetHead() != nullptr && pos.GetTail() == nullptr );

    Unit * elem = new Unit( Troop( mons, count ), pos, reflect, _uidGenerator.GetUnique() );

    elem->SetModes( CAP_SUMMONELEM );
    elem->SetArmy( hero->GetArmy() );

    GetCurrentForce().push_back( elem );

    return elem;
}

Battle::Unit * Battle::Arena::CreateMirrorImage( Unit & unit )
{
    Unit * mirrorUnit = new Unit( unit, {}, unit.isReflect(), _uidGenerator.GetUnique() );

    mirrorUnit->SetArmy( *unit.GetArmy() );
    mirrorUnit->SetMirror( &unit );
    mirrorUnit->SetModes( CAP_MIRRORIMAGE );

    unit.SetMirror( mirrorUnit );
    unit.SetModes( CAP_MIRROROWNER );

    GetCurrentForce().push_back( mirrorUnit );

    return mirrorUnit;
}

void Battle::Arena::UpdateSpellsInBookByBattle()
{
    // attacker
    if ( !_usedSpellsByDefender.empty() ) {
        auto* commander = _attackingArmy->GetCommander();
        if ( commander && commander->HaveSpellBook() ) {
            int skill = commander->GetLevelSkill( Skill::Secondary::EAGLE_EYE );
            if ( skill > 0 ) {
                SpellStorage addSpells;
                bool viewSpells = _interface && commander->isControlHuman();

                for ( auto& spell : _usedSpellsByDefender ){
                    if ( spell.Level() <= skill && !commander->HaveSpell( spell, true ) ) {
                        commander->AppendSpellToBook( spell );

                        _addedSpellsToAttacker.push_back( spell );

                        if ( viewSpells ) {
                            addSpells.push_back( spell );
                        }
                    }
                }

                if ( !addSpells.empty() ) {
                    Interface::ShowSpellsList( addSpells, commander );
                }
            }
        }

        _usedSpellsByDefender.clear();
    }

    // defender
    if ( !_usedSpellsByAttacker.empty() ) {
        auto* commander = _defendingArmy->GetCommander();
        if ( commander && commander->HaveSpellBook() ) {
            int skill = commander->GetLevelSkill( Skill::Secondary::EAGLE_EYE );
            if ( skill > 0 ) {
                SpellStorage addSpells;
                bool viewSpells = _interface && commander->isControlHuman();

                for ( auto& spell : _usedSpellsByAttacker ){
                    if ( spell.Level() <= skill && !commander->HaveSpell( spell, true ) ) {
                        commander->AppendSpellToBook( spell );

                        _addedSpellsToDefender.push_back( spell );

                        if ( viewSpells ) {
                            addSpells.push_back( spell );
                        }
                    }
                }

                if ( !addSpells.empty() ) {
                    Interface::ShowSpellsList( addSpells, commander );
                }
            }
        }

        _usedSpellsByAttacker.clear();
    }
}

void Battle::Arena::ResetSpellsInBookByBattle()
{
    if ( !_addedSpellsToAttacker.empty() ) {
        auto* commander = _attackingArmy->GetCommander();
        if ( commander && commander->HaveSpellBook() ) {
            for ( auto& spell : _addedSpellsToAttacker) {
                commander->RemoveSpellFromBook( spell );
            }
        }

        _addedSpellsToAttacker.clear();
    }

    if ( !_addedSpellsToDefender.empty() ) {
        auto* commander = _defendingArmy->GetCommander();
        if ( commander && commander->HaveSpellBook() ) {
            for ( auto& spell : _addedSpellsToDefender) {
                commander->RemoveSpellFromBook( spell );
            }
        }

        _addedSpellsToDefender.clear();
    }
}

void Battle::Arena::ShowAllAddedSpellsInBookByBattle()
{
    if ( !_addedSpellsToAttacker.empty() ) {
        auto* commander = _attackingArmy->GetCommander();
        if ( commander && commander->isControlHuman() ) {
            Interface::ShowSpellsList( _addedSpellsToAttacker, commander );
        }

        _addedSpellsToAttacker.clear();
    }

    if ( !_addedSpellsToDefender.empty() ) {
        auto* commander = _defendingArmy->GetCommander();
        if ( commander && commander->isControlHuman() ) {
            Interface::ShowSpellsList( _addedSpellsToDefender, commander );
        }

        _addedSpellsToDefender.clear();
    }
}

bool Battle::Arena::IsShootingPenalty( const Unit & attacker, const Unit & defender ) const
{
    // no castle - no castle walls, penalty does not apply
    if ( castle == nullptr ) {
        return false;
    }

    // penalty does not apply to towers
    if ( defender.Modes( CAP_TOWER ) || attacker.Modes( CAP_TOWER ) )
        return false;

    // penalty does not apply if the attacker's hero has certain artifacts or skills
    const HeroBase * hero = attacker.GetCommander();
    if ( hero ) {
        // golden bow artifact
        if ( hero->GetBagArtifacts().isArtifactBonusPresent( fheroes2::ArtifactBonusType::NO_SHOOTING_PENALTY ) )
            return false;
    }

    // penalty does not apply if the attacking unit (be it a castle attacker or a castle defender) is inside the castle walls
    if ( !attacker.isOutOfCastleWalls() ) {
        return false;
    }

    // penalty does not apply if both units are on the same side relative to the castle walls
    if ( attacker.isOutOfCastleWalls() == defender.isOutOfCastleWalls() ) {
        return false;
    }

    // penalty does not apply if the target unit is exposed due to the broken castle wall
    const std::vector<fheroes2::Point> points = getLinePoints( attacker.GetBackPoint(), defender.GetBackPoint(), Cell::widthPx / 3 );

    for ( std::vector<fheroes2::Point>::const_iterator it = points.begin(); it != points.end(); ++it ) {
        if ( (    0 == board[CastleFortress::POS_UPPER_WALL_1].GetObject() && ( board[CastleFortress::POS_UPPER_WALL_1].GetPos() & *it ) )
             || ( 0 == board[CastleFortress::POS_UPPER_WALL_2].GetObject() && ( board[CastleFortress::POS_UPPER_WALL_2].GetPos() & *it ) )
             || ( 0 == board[CastleFortress::POS_LOWER_WALL_1].GetObject() && ( board[CastleFortress::POS_LOWER_WALL_1].GetPos() & *it ) )
             || ( 0 == board[CastleFortress::POS_LOWER_WALL_2].GetObject() && ( board[CastleFortress::POS_LOWER_WALL_2].GetPos() & *it ) ) )
            return false;
    }

    return true;
}

Battle::Force & Battle::Arena::getForce( const PlayerColor color ) const
{
    return ( _attackingArmy->GetColor() == color ) ? *_attackingArmy : *_defendingArmy;
}

Battle::Force & Battle::Arena::getEnemyForce( const PlayerColor color ) const
{
    return ( _attackingArmy->GetColor() == color ) ? *_defendingArmy : *_attackingArmy;
}

Battle::Force & Battle::Arena::GetCurrentForce() const
{
    return getForce( GetCurrentColor() );
}

bool Battle::Arena::IsAttacker(const Force& force) const
{
    return &force == _attackingArmy.get();
}

bool Battle::Arena::IsDefender(const Force& force) const
{
    return &force == _defendingArmy.get();
}

bool Battle::Arena::AutoCombatInProgress() const
{
    if ( _currentUnit == nullptr ) {
        return false;
    }

    if ( _autoCombatColors & GetCurrentColor() ) {
        // Auto combat mode cannot be enabled for a player controlled by the AI
        assert( !( GetCurrentForce().GetControl() & CONTROL_AI ) );

        return true;
    }

    return false;
}

bool Battle::Arena::EnemyOfAIHasAutoCombatInProgress() const
{
    if ( _currentUnit == nullptr ) {
        return false;
    }

    if ( !( GetCurrentForce().GetControl() & CONTROL_AI ) ) {
        return false;
    }

    const Force & enemyForce = getEnemyForce( GetCurrentColor() );

    if ( enemyForce.GetControl() & CONTROL_AI ) {
        return false;
    }

    return ( _autoCombatColors & enemyForce.GetColor() );
}

bool Battle::Arena::CanToggleAutoCombat() const
{
    if ( _currentUnit == nullptr ) {
        return false;
    }

    return !( GetCurrentForce().GetControl() & CONTROL_AI );
}
