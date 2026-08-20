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

#include "battle_troop.h"

#include <algorithm>
#include <cassert>
#include <optional>
#include <sstream>

#include "army.h"
#include "artifact.h"
#include "artifact_info.h"
#include "battle.h"
#include "battle_arena.h"
#include "battle_army.h"
#include "battle_board.h"
#include "battle_cell.h"
#include "battle_grave.h"
#include "battle_interface.h"
#include "battle_tower.h"
#include "castle.h"
#include "color.h"
#include "game_assets.h"
#include "game_static.h"
#include "heroes_base.h"
#include "image.h"
#include "logging.h"
#include "m82.h"
#include "monster.h"
#include "monster_anim.h"
#include "monster_info.h"
#include "morale.h"
#include "luck.h"
#include "rand.h"
#include "resource.h"
#include "skill.h"
#include "speed.h"
#include "spell.h"
#include "spell_info.h"
#include "tools.h"
#include "translations.h"

namespace
{
    Artifact getImmunityArtifactForSpell( const HeroBase * hero, const Spell & spell )
    {
        if ( hero == nullptr ) {
            return { Artifact::UNKNOWN };
        }

        switch ( spell.GetID() ) {
        case Spell::CURSE:
        case Spell::MASSCURSE:
            return hero->GetBagArtifacts().getFirstArtifactWithBonus( fheroes2::ArtifactBonusType::CURSE_SPELL_IMMUNITY );
        case Spell::HYPNOTIZE:
            return hero->GetBagArtifacts().getFirstArtifactWithBonus( fheroes2::ArtifactBonusType::HYPNOTIZE_SPELL_IMMUNITY );
        case Spell::DEATHRIPPLE:
        case Spell::DEATHWAVE:
            return hero->GetBagArtifacts().getFirstArtifactWithBonus( fheroes2::ArtifactBonusType::DEATH_SPELL_IMMUNITY );
        case Spell::BERSERKER:
            return hero->GetBagArtifacts().getFirstArtifactWithBonus( fheroes2::ArtifactBonusType::BERSERK_SPELL_IMMUNITY );
        case Spell::BLIND:
            return hero->GetBagArtifacts().getFirstArtifactWithBonus( fheroes2::ArtifactBonusType::BLIND_SPELL_IMMUNITY );
        case Spell::PARALYZE:
            return hero->GetBagArtifacts().getFirstArtifactWithBonus( fheroes2::ArtifactBonusType::PARALYZE_SPELL_IMMUNITY );
        case Spell::HOLYWORD:
        case Spell::HOLYSHOUT:
            return hero->GetBagArtifacts().getFirstArtifactWithBonus( fheroes2::ArtifactBonusType::HOLY_SPELL_IMMUNITY );
        case Spell::DISPEL:
        case Spell::MASSDISPEL:
            return hero->GetBagArtifacts().getFirstArtifactWithBonus( fheroes2::ArtifactBonusType::DISPEL_SPELL_IMMUNITY );
        default:
            break;
        }

        return { Artifact::UNKNOWN };
    }
}

uint32_t Battle::ModesAffected::GetMode( const uint32_t mode ) const
{
    const const_iterator it = std::find_if( begin(), end(), [mode]( const Battle::ModeDuration & v ) { return v.isMode( mode ); } );
    return it == end() ? 0 : it->second;
}

void Battle::ModesAffected::AddMode( const uint32_t mode, const uint32_t duration )
{
    const iterator it = std::find_if( begin(), end(), [mode]( const Battle::ModeDuration & v ) { return v.isMode( mode ); } );
    if ( it == end() ) {
        emplace_back( mode, duration );
    }
    else {
        it->second = duration;
    }
}

void Battle::ModesAffected::RemoveMode( const uint32_t mode )
{
    erase( std::remove_if( begin(), end(), [mode]( const Battle::ModeDuration & v ) { return v.isMode( mode ); } ), end() );
}

void Battle::ModesAffected::DecreaseDuration()
{
    std::for_each( begin(), end(), []( Battle::ModeDuration & v ) { v.DecreaseDuration(); } );
}

uint32_t Battle::ModesAffected::FindZeroDuration() const
{
    const const_iterator it = std::find_if( begin(), end(), []( const Battle::ModeDuration & v ) { return v.isZeroDuration(); } );
    return it == end() ? 0 : ( *it ).first;
}

Battle::Unit::Unit( const Troop & troop, const Position & pos, const bool isReflected, const uint32_t uid )
    : ArmyTroop( nullptr, troop )
    , animation( id )
    , _uid( uid )
    , _hitPoints( troop.GetHitPoints() )
    , _initialCount( troop.GetCount() )
    , _maxCount( troop.GetCount() )
    , _shotsLeft( troop.GetShots() )
    , _idleTimer( animation.getIdleDelay() )
    , _isReflected( isReflected )
    , _moral( Morale::CHANCE_IN_PERCENT )
    , _luck( Luck::CHANCE_IN_PERCENT )
    , _supportLuck( 1 )
{
    SetPosition( pos );
}

void Battle::Unit::SetPosition( const int32_t index )
{
    if ( _position.GetHead() ) {
        _position.GetHead()->SetUnit( nullptr );
    }
    if ( _position.GetTail() ) {
        _position.GetTail()->SetUnit( nullptr );
    }

    _position.Set( index, isWide(), _isReflected );

    if ( _position.GetHead() ) {
        _position.GetHead()->SetUnit( this );
    }
    if ( _position.GetTail() ) {
        _position.GetTail()->SetUnit( this );
    }
}

void Battle::Unit::SetPosition( const Position & pos )
{
    // Position may be empty if this unit is a castle tower
    assert( pos.isValidForUnit( this ) || pos.isEmpty() );

    if ( _position.GetHead() ) {
        _position.GetHead()->SetUnit( nullptr );
    }
    if ( _position.GetTail() ) {
        _position.GetTail()->SetUnit( nullptr );
    }

    _position = pos;

    if ( _position.GetHead() ) {
        _position.GetHead()->SetUnit( this );
    }
    if ( _position.GetTail() ) {
        _position.GetTail()->SetUnit( this );
    }

    if ( isWide() && _position.GetHead() && _position.GetTail() ) {
        _isReflected = GetHeadIndex() < GetTailIndex();
    }
}

void Battle::Unit::SetReflection( const bool isReflected )
{
    if ( _isReflected != isReflected ) {
        _position.Swap();

        _isReflected = isReflected;
    }
}

void Battle::Unit::UpdateDirection()
{
    const Arena * arena = GetArena();
    assert( arena != nullptr );

    SetReflection( arena->getAttackingArmyColor() != GetArmyColor() );
}

bool Battle::Unit::UpdateDirection( const fheroes2::Rect & pos )
{
    if ( _position.GetRect().x == pos.x ) {
        return false;
    }

    const bool isReflected = _position.GetRect().x > pos.x;
    if ( isReflected != _isReflected ) {
        SetReflection( isReflected );
        return true;
    }

    return false;
}

bool Battle::Unit::isBattle() const
{
    return true;
}

bool Battle::Unit::isModes( const uint32_t modes_ ) const
{
    return Modes( modes_ );
}

std::string Battle::Unit::GetShotString() const
{
    if ( Troop::GetShots() == GetShots() ) {
        return std::to_string( Troop::GetShots() );
    }

    std::string output( std::to_string( Troop::GetShots() ) );
    output += " (";
    output += std::to_string( GetShots() );
    output += ')';

    return output;
}

std::string Battle::Unit::GetSpeedString() const
{
    const uint32_t speed = GetSpeed( true, false );
    return Troop::GetSpeedString( speed );
}

uint32_t Battle::Unit::GetHitPointsLeft() const
{
    return GetHitPoints() - ( GetCount() - 1 ) * Monster::GetHitPoints();
}

uint32_t Battle::Unit::GetMissingHitPoints() const
{
    const uint32_t totalHitPoints = _maxCount * Monster::GetHitPoints();
    assert( totalHitPoints >= _hitPoints );
    return totalHitPoints - _hitPoints;
}

uint32_t Battle::Unit::GetAffectedDuration( const uint32_t mode ) const
{
    return _affected.GetMode( mode );
}

uint32_t Battle::Unit::GetSpeed() const
{
    return GetSpeed( false, false );
}

uint32_t Battle::Unit::GetPriority() const
{
    return GetPriority( false, false );
}

int Battle::Unit::GetMorale() const
{
    const Arena * arena = GetArena();
    assert( arena != nullptr );

    int armyTroopMorale = ArmyTroop::GetMorale();

    // enemy Bone dragons affect morale
    //if ( isAffectedByMorale() && arena->getEnemyForce( GetArmyColor() ).HasMonster( Monster::BONE_DRAGON ) && armyTroopMorale > Morale::TREASON ) {
    //    armyTroopMorale -= 1; /* MORAL_DECREMENT */
    //}

    return armyTroopMorale;
}

int32_t Battle::Unit::GetHeadIndex() const
{
    return _position.GetHead() ? _position.GetHead()->GetIndex() : -1;
}

int32_t Battle::Unit::GetTailIndex() const
{
    return _position.GetTail() ? _position.GetTail()->GetIndex() : -1;
}

void Battle::Unit::SetRandomMorale( Rand::PCG32 & randomGenerator )
{
    int morale = GetMorale();

    if ( morale > 0 ) {
        if ( _moral.Check( morale, randomGenerator ) ) {
            SetModes( MORALE_GOOD );
        }
    }
    else if ( morale < 0 ) {
        morale = -morale;

        // AI is given a cheeky 25% chance to avoid it - because they build armies from random troops
        if ( !isControlHuman() ) {
            morale = (morale * 3 + 1) / 4;
        }

        if ( _moral.Check( morale, randomGenerator ) ) {
            SetModes( MORALE_BAD );
        }
    }
}

void Battle::Unit::SetRandomLuck( Rand::PCG32 & randomGenerator )
{
    int luck = GetLuck();

    if (luck > 0) {
        if ( _luck.Check( luck, randomGenerator ) ) {
            SetModes( LUCK_GOOD );
        }
    }
    else if (luck < 0) {
        if ( _moral.Check( -luck, randomGenerator ) ) {
            SetModes( LUCK_BAD );
        }
    }

    // Bless, Curse and Luck do stack
}

bool Battle::Unit::isFlying() const
{
    return ( ArmyTroop::isFlying() && !Modes( SP_SLOW ) ) || Modes( TELEPORT_ABILITY );
}

bool Battle::Unit::isOutOfCastleWalls() const
{
    return Board::isOutOfWallsIndex( GetHeadIndex() ) || ( isWide() && Board::isOutOfWallsIndex( GetTailIndex() ) );
}

bool Battle::Unit::isHandFighting() const
{
    assert( isValid() );

    // Towers never fight in close combat
    if ( Modes( CAP_TOWER ) ) {
        return false;
    }

    for ( const int32_t nearbyIdx : Board::GetAroundIndexes( *this ) ) {
        const Unit * nearbyUnit = Board::GetCell( nearbyIdx )->GetUnit();
        if ( nearbyUnit == nullptr ) {
            continue;
        }

        if ( nearbyUnit->GetColor() == GetCurrentColor() ) {
            continue;
        }

        return true;
    }

    return false;
}

bool Battle::Unit::isHandFighting( const Unit & attacker, const Unit & defender )
{
    assert( attacker.isValid() );

    // Towers never fight in close combat
    if ( attacker.Modes( CAP_TOWER ) || defender.Modes( CAP_TOWER ) ) {
        return false;
    }

    const uint32_t distance = Board::GetDistance( attacker.GetPosition(), defender.GetPosition() );
    assert( distance > 0 );

    // If the attacker and the defender are next to each other, then this is a melee attack, otherwise it's a shot
    return ( distance == 1 );
}

bool Battle::Unit::isIdling() const
{
    return GetAnimationState() == Monster_Info::IDLE;
}

void Battle::Unit::NewTurn()
{
    if ( isRegenerating() ) {
        _hitPoints = ArmyTroop::GetHitPoints();
    }

    ResetModes( TR_RETALIATED );
    ResetModes( TR_MOVED );
    ResetModes( TR_SKIP );
    ResetModes( LUCK_GOOD );
    ResetModes( LUCK_BAD );
    ResetModes( MORALE_GOOD );
    ResetModes( MORALE_BAD );

    _affected.DecreaseDuration();

    for ( uint32_t mode = _affected.FindZeroDuration(); mode != 0; mode = _affected.FindZeroDuration() ) {
        assert( Modes( mode ) );

        if ( mode == CAP_MIRROROWNER ) {
            assert( _mirrorUnit != nullptr && _mirrorUnit->Modes( CAP_MIRRORIMAGE ) && _mirrorUnit->_mirrorUnit == this );

            if ( Arena::GetInterface() ) {
                Arena::GetInterface()->RedrawActionRemoveMirrorImage( { _mirrorUnit } );
            }

            _mirrorUnit->_hitPoints = 0;
            _mirrorUnit->SetCount( 0 );
            // Affection will be removed here
            _mirrorUnit->PostKilledAction();
        }
        else {
            removeAffection( mode );
        }
    }
}

uint32_t Battle::Unit::GetSpeed( const bool skipStandingCheck, const bool skipMovedCheck ) const
{
    if ( !skipStandingCheck ) {
        uint32_t modesToCheck = SP_BLIND | IS_PARALYZE_MAGIC;
        if ( !skipMovedCheck ) {
            modesToCheck |= TR_MOVED;
        }

        if ( GetCount() == 0 || Modes( modesToCheck ) ) {
            return Speed::STANDING;
        }
    }

    // order is important!
    uint32_t speed = Monster::GetBaseSpeed();

    if ( Modes( SP_SLOW ) ) {
        speed = Speed::getSlowSpeedFromSpell( speed );
    }
    if ( Modes( SP_HASTE ) ) {
        speed = Speed::getHasteSpeedFromSpell( speed );
    }

    return speed + Monster::GetBonusSpeed();
}

uint32_t Battle::Unit::GetPriority( const bool skipStandingCheck, const bool skipMovedCheck ) const
{
    if ( !skipStandingCheck ) {
        uint32_t modesToCheck = SP_BLIND | IS_PARALYZE_MAGIC;
        if ( !skipMovedCheck ) {
            modesToCheck |= TR_MOVED;
        }

        if ( GetCount() == 0 ) {
            return 0;
        }
        if (Modes( modesToCheck )) {
            return 0;
        }
    }

    // order is important!
    uint32_t priority = Monster::GetPriority();

    if ( Modes( SP_SLOW ) ) {
        priority = Speed::getSlowPriorityFromSpell( priority );
    }
    if ( Modes( SP_HASTE ) ) {
        priority = Speed::getHastePriorityFromSpell( priority );
    }

    assert(priority);
    return priority;
}

uint32_t Battle::Unit::EstimateRetaliatoryDamage( const uint32_t damageTaken ) const
{
    // The entire unit is destroyed, no retaliation
    if ( damageTaken >= _hitPoints ) {
        return 0;
    }

    // Mirror images are destroyed anyway and hypnotized units never respond to an attack
    if ( Modes( TR_RETALIATED | CAP_MIRRORIMAGE | SP_HYPNOTIZE ) ) {
        return 0;
    }

    // Units with this ability retaliate even when under the influence of paralyzing spells
    if ( Modes( IS_PARALYZE_MAGIC ) && !isAbilityPresent( fheroes2::MonAbil::UNLIMITED_RETALIATION ) ) {
        return 0;
    }

    const uint32_t unitsLeft = GetCountFromHitPoints( *this, _hitPoints - damageTaken );
    assert( unitsLeft > 0 );

    const uint32_t damagePerUnit = [this]() {
        if ( Modes( SP_CURSE ) ) {
            return Monster::GetDamageMin();
        }

        if ( Modes( SP_BLESS ) ) {
            return Monster::GetDamageMax();
        }

        return ( Monster::GetDamageMin() + Monster::GetDamageMax() ) / 2;
    }();

    const uint32_t retaliatoryDamage = unitsLeft * damagePerUnit;

    if ( !Modes( SP_BLIND ) ) {
        return retaliatoryDamage;
    }

    // The retaliatory damage of a blinded unit is reduced
    const uint32_t reductionPercent = Spell( Spell::BLIND ).ExtraValue();
    assert( reductionPercent <= 100 );

    return retaliatoryDamage * ( 100 - reductionPercent ) / 100;
}

uint32_t Battle::Unit::CalculateMinDamage( const Unit & enemy ) const
{
    return CalculateDamageUnit( enemy, ArmyTroop::GetDamageMin() );
}

uint32_t Battle::Unit::CalculateMaxDamage( const Unit & enemy ) const
{
    return CalculateDamageUnit( enemy, ArmyTroop::GetDamageMax() );
}

uint32_t Battle::Unit::getPotentialDamage( const Unit & enemy ) const
{
    if ( Modes( Battle::SP_CURSE ) ) {
        return CalculateMinDamage( enemy );
    }

    if ( Modes( Battle::SP_BLESS ) ) {
        return CalculateMaxDamage( enemy );
    }

    return ( CalculateMinDamage( enemy ) + CalculateMaxDamage( enemy ) ) / 2;
}

uint32_t Battle::Unit::CalculateDamageUnit( const Unit & enemy, double dmg ) const
{
    if ( isArchers() ) {
        // Melee penalty can be applied either if the archer is blocked by enemy units and cannot shoot,
        // or if he is not blocked, but was attacked by a friendly unit (for example, in the case of using
        // Berserk or Hypnotize spells) and deals retaliatory damage
        if ( !isHandFighting() && !isHandFighting( *this, enemy ) ) {
            // Hero's Archery skill may increase damage
            double archerFactor = 0;
            auto commander = GetCommander();
            if ( commander ) {
                archerFactor += GetCommander()->GetSecondarySkillValue( Skill::Secondary::ARCHERY ) / 100.0;
            }

            dmg *= 1.0 + archerFactor;

            if ( isAbilityPresent( fheroes2::MonAbil::BIRD_HUNTING ) ) {
                if (enemy.isAbilityPresent( fheroes2::MonAbil::FLYING ) ) {
                    dmg *= 1.5;
                }
            }

            const Arena * arena = GetArena();
            assert( arena != nullptr );

            // Penalty for damage to castle defenders behind the castle walls
            if ( arena->IsShootingPenalty( *this, enemy ) && archerFactor < 1 ) {
                dmg *= ( 1 - archerFactor ) * GameStatic::getCastleWallRangedPenalty() / 100.0;
            }

            // The Shield spell does not affect the damage of the castle towers
            if ( enemy.Modes( SP_SHIELD ) ) {
                dmg /= Spell( Spell::SHIELD ).ExtraValue();
            }

            if ( enemy.isAbilityPresent( fheroes2::MonAbil::NATURAL_SHILD ) ) {
                dmg *= 0.6;
            }
        }
        else if ( !isAbilityPresent( fheroes2::MonAbil::NO_MELEE_PENALTY ) ) {
            dmg /= 2;
        }
    }

    // The retaliatory damage of a blinded unit is reduced
    if ( _blindRetaliation ) {
        // Petrified units cannot attack, respectively, there should be no retaliation
        assert( !enemy.Modes( SP_STONE ) );

        const uint32_t reductionPercent = Spell( Spell::BLIND ).ExtraValue();
        assert( reductionPercent <= 100 );

        dmg = dmg * ( 100 - reductionPercent ) / 100;
    }

    // A petrified unit takes only half of the damage
    if ( enemy.Modes( SP_STONE ) ) {
        // Petrified units cannot attack, respectively, there should be no retaliation
        assert( !_blindRetaliation );

        dmg /= 2;
    }

    // If multiple options are suitable at the same time, the damage should be doubled only once
    if ( isAbilityPresent( fheroes2::MonAbil::DOUBLE_DAMAGE_TO_UNDEAD ) && enemy.isAbilityPresent( fheroes2::MonAbil::UNDEAD ) ) {
        dmg *= 2;
    }

    if ( isAbilityPresent( fheroes2::MonAbil::EXTRA_DAMAGE_TO_BIG_MONSTERS ) && enemy.isAbilityPresent( fheroes2::MonAbil::DOUBLE_HEX_SIZE ) ) {
        dmg *= 1.4;
    }

    if ( isAbilityPresent( fheroes2::MonAbil::LEVEL_DAMAGE ) ) {
        int deltaLevels = GetBattleLevel() - enemy.GetBattleLevel();
        if ( deltaLevels > 0 ) {
            dmg *= 1.0 + deltaLevels * 0.05;
        }
    }

    if (isAbilityPresent( fheroes2::MonAbil::DIRTY_BLOW ) ) {
        if ( enemy.EstimateRetaliatoryDamage( 0 ) > 0 ) {
            dmg *= 1.33;
        }
    }

    if ( enemy.isAbilityPresent( fheroes2::MonAbil::BONE_BODY ) ) {
        dmg *= 0.67;
    }

    if ( enemy.isAbilityPresent( fheroes2::MonAbil::ETHERIC_BODY ) ) {
        dmg *= 0.75;
    }

    int r = GetMonAttack() - enemy.GetMonDefense();
    if ( enemy.isTopMonster() && Modes( SP_DRAGONSLAYER ) ) {
        r += Spell( Spell::DRAGONSLAYER ).ExtraValue();
    }

    if (r >= 0)
    {
        if (r > 100)
            r = 100;

        dmg *= 1 + r * 0.1;
    }
    else
    {
        if (r < -100)
            r = -100;

        dmg /= 1 - r * 0.1;
    }

    int64_t vd = static_cast<int64_t>( dmg + 0.5 );
    assert( vd >= 0 && vd <= std::numeric_limits<uint32_t>::max() );

    return ( vd > 0 ) ? ( vd <= std::numeric_limits<uint32_t>::max() ? static_cast<uint32_t>( vd ) : std::numeric_limits<uint32_t>::max() ) : 1;

    //dmg *= 1 + ( 0 < r ? 0.1 * std::min( r, 20 ) : 0.05 * std::max( r, -16 ) );
    //return std::max<uint32_t>( fheroes2::checkedCast<uint32_t>( dmg ).value(), 1U );
}

uint32_t Battle::Unit::GetDamage( const Unit & enemy, Rand::PCG32 & randomGenerator ) const
{
    uint32_t res = 0;

    if ( Modes( SP_BLESS ) ) {
        res = CalculateMaxDamage( enemy );
    }
    else if ( Modes( SP_CURSE ) ) {
        res = CalculateMinDamage( enemy );
    }
    else {
        res = Rand::GetWithGen( CalculateMinDamage( enemy ), CalculateMaxDamage( enemy ), randomGenerator );
    }

    if ( isAbilityPresent( fheroes2::MonAbil::ETHER_DAMAGE ) ) {
        res += GetCount() * 15;
    }

    if ( isAbilityPresent( fheroes2::MonAbil::ETHER_SHOT_DAMAGE ) ) {
        if ( isArchers() && !isHandFighting() ) {
            res += GetCount() * 10;
        }
    }

    if ( Modes( LUCK_GOOD ) ) {
        res = (res * 3 + 1) / 2;
    }
    else if ( Modes( LUCK_BAD ) ) {
        res /= 2;
    }

    return std::max<uint32_t>( res, 1U );
}

uint32_t Battle::Unit::HowManyWillBeKilled( const uint32_t dmg ) const
{
    if ( Modes( CAP_MIRRORIMAGE ) ) {
        return GetCount();
    }

    return dmg >= _hitPoints ? GetCount() : GetCount() - Monster::GetCountFromHitPoints( *this, _hitPoints - dmg );
}

uint32_t Battle::Unit::_applyDamage( const uint32_t dmg )
{
    assert( !AllModes( CAP_MIRROROWNER | CAP_MIRRORIMAGE ) );

    if ( dmg == 0 || GetCount() == 0 ) {
        return 0;
    }

    const uint32_t killed = HowManyWillBeKilled( dmg );

    DEBUG_LOG( DBG_BATTLE, DBG_TRACE, dmg << " to " << String() << " and killed: " << killed )

    if ( Modes( IS_PARALYZE_MAGIC ) ) {
        // Units with this ability retaliate even when under the influence of paralyzing spells
        if ( !isAbilityPresent( fheroes2::MonAbil::UNLIMITED_RETALIATION ) ) {
            SetModes( TR_RETALIATED );
        }

        SetModes( TR_MOVED );

        removeAffection( IS_PARALYZE_MAGIC );
    }

    if ( Modes( SP_BLIND ) ) {
        SetModes( TR_MOVED );

        removeAffection( SP_BLIND );
    }

    if ( killed >= GetCount() ) {
        _deadCount += GetCount();

        SetCount( 0 );
    }
    else {
        _deadCount += killed;

        SetCount( GetCount() - killed );
    }

    if ( Modes( CAP_MIRRORIMAGE ) ) {
        _hitPoints = 0;
    }
    else {
        _hitPoints -= std::min( _hitPoints, dmg );
    }

    if ( Modes( CAP_MIRROROWNER ) && !isValid() ) {
        assert( _mirrorUnit != nullptr && _mirrorUnit->Modes( CAP_MIRRORIMAGE ) && _mirrorUnit->_mirrorUnit == this );

        _mirrorUnit->_applyDamage( _mirrorUnit->_hitPoints );
    }

    return killed;
}

void Battle::Unit::PostKilledAction()
{
    assert( !AllModes( CAP_MIRROROWNER | CAP_MIRRORIMAGE ) );

    if ( Modes( CAP_MIRROROWNER ) ) {
        assert( _mirrorUnit != nullptr && _mirrorUnit->Modes( CAP_MIRRORIMAGE ) && _mirrorUnit->_mirrorUnit == this );

        _mirrorUnit = nullptr;

        removeAffection( CAP_MIRROROWNER );
    }

    if ( Modes( CAP_MIRRORIMAGE ) ) {
        // CAP_MIRROROWNER may have already been removed from the mirror owner,
        // since this method may already have been called for it
        assert( _mirrorUnit != nullptr );

        // But we still need to remove it if it is present
        if ( _mirrorUnit->Modes( CAP_MIRROROWNER ) ) {
            assert( _mirrorUnit->_mirrorUnit == this );

            _mirrorUnit->_mirrorUnit = nullptr;
            _mirrorUnit->removeAffection( CAP_MIRROROWNER );
        }

        _mirrorUnit = nullptr;
    }

    // Remove all spells
    removeAffection( IS_MAGIC );
    assert( _affected.empty() );

    // Save to the graveyard if possible
    if ( !Modes( CAP_MIRRORIMAGE ) && !isElemental() ) {
        Graveyard * graveyard = Arena::GetGraveyard();
        assert( graveyard != nullptr );

        graveyard->addUnit( this );
    }

    Cell * head = _position.GetHead();
    assert( head != nullptr );

    head->SetUnit( nullptr );

    if ( isWide() ) {
        Cell * tail = _position.GetTail();
        assert( tail != nullptr );

        tail->SetUnit( nullptr );
    }

    DEBUG_LOG( DBG_BATTLE, DBG_TRACE, String() )
}

uint32_t Battle::Unit::_resurrect( const uint32_t points, const bool allowToExceedMaxCount, const bool isTemporary )
{
    uint32_t resurrect = Monster::GetCountFromHitPoints( *this, _hitPoints + points ) - GetCount();

    SetCount( GetCount() + resurrect );
    _hitPoints += points;

    if ( allowToExceedMaxCount ) {
        _maxCount = std::max( _maxCount, GetCount() );
    }
    else if ( GetCount() > _maxCount ) {
        resurrect -= GetCount() - _maxCount;
        SetCount( _maxCount );
        _hitPoints = ArmyTroop::GetHitPoints();
    }

    if ( !isTemporary ) {
        _deadCount -= ( resurrect < _deadCount ? resurrect : _deadCount );
    }

    return resurrect;
}

bool Battle::Unit::isImmovable() const
{
    return Modes( SP_BLIND | IS_PARALYZE_MAGIC );
}

uint32_t Battle::Unit::ApplyDamage( Unit & enemy, const uint32_t dmg, uint32_t & killed, uint32_t * ptrResurrected )
{
    killed = _applyDamage( dmg );

    if ( killed == 0 ) {
        if ( ptrResurrected != nullptr ) {
            *ptrResurrected = 0;
        }
        return killed;
    }

    uint32_t resurrected = 0;

    if ( enemy.isAbilityPresent( fheroes2::MonAbil::SOUL_EATER ) ) {
        resurrected = enemy._resurrect( killed * enemy.Monster::GetHitPoints(), true, false );
    }
    else if ( enemy.isAbilityPresent( fheroes2::MonAbil::HP_DRAIN ) ) {
        resurrected = enemy._resurrect( killed * Monster::GetHitPoints() / 2, false, false );
    }

    if ( resurrected > 0 ) {
        DEBUG_LOG( DBG_BATTLE, DBG_TRACE, String() << ", enemy: " << enemy.String() << ", resurrected: " << resurrected )
    }

    if ( ptrResurrected != nullptr ) {
        *ptrResurrected = resurrected;
    }

    return killed;
}

bool Battle::Unit::AllowApplySpell( const Spell & spell, const HeroBase * applyingHero, const bool forceApplyToAlly /* = false */ ) const
{
    if ( Modes( CAP_MIRRORIMAGE ) && ( spell == Spell::ANTIMAGIC || spell == Spell::MIRRORIMAGE ) ) {
        return false;
    }

    if ( Modes( CAP_MIRROROWNER ) && spell == Spell::MIRRORIMAGE ) {
        return false;
    }

    if ( applyingHero && spell.isApplyToFriends() && GetColor() != applyingHero->GetColor() ) {
        return false;
    }
    if ( applyingHero && spell.isApplyToEnemies() && GetColor() == applyingHero->GetColor() && !forceApplyToAlly ) {
        return false;
    }

    return ( GetMagicResist( spell, applyingHero ) < 100 );
}

bool Battle::Unit::ApplySpell( const Spell & spell, const HeroBase * applyingHero, TargetInfo & target )
{
    // HACK!!! Chain lightning is the only spell which can't be cast on allies but could be applied on them
    const bool isForceApply = ( spell.GetID() == Spell::CHAINLIGHTNING );

    if ( !AllowApplySpell( spell, applyingHero, isForceApply ) ) {
        return false;
    }

    DEBUG_LOG( DBG_BATTLE, DBG_TRACE, spell.GetName() << " to " << String() )

    const uint32_t spoint = applyingHero ? applyingHero->GetPower() : fheroes2::spellPowerForBuiltinMonsterSpells;

    if ( spell.isDamage() ) {
        _spellApplyDamage( spell, spoint, applyingHero, target );
    }
    else if ( spell.isRestore() || spell.isResurrect() ) {
        _spellRestoreAction( spell, spoint, applyingHero );
    }
    else {
        _spellModesAction( spell, spoint, applyingHero );
    }

    return true;
}

std::vector<Spell> Battle::Unit::getCurrentSpellEffects() const
{
    std::vector<Spell> spellList;

    if ( Modes( SP_BLESS ) ) {
        spellList.emplace_back( Spell::BLESS );
    }
    if ( Modes( SP_CURSE ) ) {
        spellList.emplace_back( Spell::CURSE );
    }
    if ( Modes( SP_HASTE ) ) {
        spellList.emplace_back( Spell::HASTE );
    }
    if ( Modes( SP_SLOW ) ) {
        spellList.emplace_back( Spell::SLOW );
    }
    if ( Modes( SP_SHIELD ) ) {
        spellList.emplace_back( Spell::SHIELD );
    }
    if ( Modes( SP_BLOODLUST ) ) {
        spellList.emplace_back( Spell::BLOODLUST );
    }
    if ( Modes( SP_STONESKIN ) ) {
        spellList.emplace_back( Spell::STONESKIN );
    }
    if ( Modes( SP_STEELSKIN ) ) {
        spellList.emplace_back( Spell::STEELSKIN );
    }
    if ( Modes( SP_BLIND ) ) {
        spellList.emplace_back( Spell::BLIND );
    }
    if ( Modes( SP_PARALYZE ) ) {
        spellList.emplace_back( Spell::PARALYZE );
    }
    if ( Modes( SP_STONE ) ) {
        spellList.emplace_back( Spell::PETRIFY );
    }
    if ( Modes( SP_DRAGONSLAYER ) ) {
        spellList.emplace_back( Spell::DRAGONSLAYER );
    }
    if ( Modes( SP_BERSERKER ) ) {
        spellList.emplace_back( Spell::BERSERKER );
    }
    if ( Modes( SP_HYPNOTIZE ) ) {
        spellList.emplace_back( Spell::HYPNOTIZE );
    }
    if ( Modes( CAP_MIRROROWNER ) ) {
        spellList.emplace_back( Spell::MIRRORIMAGE );
    }

    return spellList;
}

std::string Battle::Unit::String( const bool more /* = false */ ) const
{
    std::stringstream ss;

    ss << "Unit: "
       << "[ " <<
        // info
        GetCount() << " " << GetName() << ", " << Color::String( GetColor() ) << ", pos: " << GetHeadIndex() << ", " << GetTailIndex()
       << ( _isReflected ? ", reflect" : "" );

    if ( more )
        ss << ", mode(" << GetHexString( modes ) << ")"
           << ", uid(" << GetHexString( _uid ) << ")"
           << ", speed(" /*<< Speed::String( GetSpeed() ) << ", "*/ << GetSpeed() << ")"
           << ", priority(" << GetPriority() << ")"
           << ", hp(" << _hitPoints << ")"
           << ", died(" << _deadCount << ")";

    ss << " ]";

    return ss.str();
}

bool Battle::Unit::isRetaliationAllowed() const
{
    // Hypnotized units never respond to an attack
    if ( Modes( SP_HYPNOTIZE ) ) {
        return false;
    }

    // Blindness can be cast by an attacking unit. There should never be any retaliation in this case.
    if ( Modes( SP_BLIND ) && !_blindRetaliation ) {
        return false;
    }

    // Paralyzing magic can be cast by an attacking unit. There should never be any retaliation in this case.
    if ( Modes( IS_PARALYZE_MAGIC ) ) {
        return false;
    }

    return ( !Modes( TR_RETALIATED ) );
}

void Battle::Unit::setRetaliationAsCompleted()
{
    if ( isAbilityPresent( fheroes2::MonAbil::UNLIMITED_RETALIATION ) ) {
        return;
    }

    SetModes( TR_RETALIATED );
}

void Battle::Unit::PostAttackAction( const Unit & enemy )
{
    if ( isArchers() && !isHandFighting( *this, enemy ) ) {
        const HeroBase * hero = GetCommander();

        if ( hero == nullptr || !hero->GetBagArtifacts().isArtifactBonusPresent( fheroes2::ArtifactBonusType::ENDLESS_AMMUNITION ) ) {
            assert( !Modes( CAP_TOWER ) && _shotsLeft > 0 );

            --_shotsLeft;
        }
    }

    ResetModes( LUCK_GOOD | LUCK_BAD );
}

int Battle::Unit::GetMonAttack() const
{
    int res = ArmyTroop::GetMonAttack();

    if ( Modes( SP_BLOODLUST ) ) {
        res += Spell( Spell::BLOODLUST ).ExtraValue();
    }

    return res;
}

int Battle::Unit::GetMonDefense() const
{
    int res = ArmyTroop::GetMonDefense();

    if ( Modes( SP_STONESKIN ) ) {
        res += Spell( Spell::STONESKIN ).ExtraValue();
    }
    else if ( Modes( SP_STEELSKIN ) ) {
        res += Spell( Spell::STEELSKIN ).ExtraValue();
    }

    if ( _disruptingRaysNum ) {
        const int step = _disruptingRaysNum * Spell( Spell::DISRUPTINGRAY ).ExtraValue();

        if ( step >= res ) {
            res = 1;
        }
        else {
            res -= step;
        }
    }

    const Castle * castle = Arena::GetCastle();

    if ( castle && castle->isBuild( BUILD_MOAT ) && ( Board::isMoatIndex( GetHeadIndex(), *this ) || Board::isMoatIndex( GetTailIndex(), *this ) ) ) {
        const int step = GameStatic::GetBattleMoatReduceDefense();

        if ( step >= res ) {
            res = 1;
        }
        else {
            res -= step;
        }
    }

    return res;
}

double Battle::Unit::evaluateThreatForUnit( const Unit & defender ) const
{
    const Unit & attacker = *this;

    const uint32_t attackerDamageToDefender = attacker.getPotentialDamage( defender );
    double attackerThreat = attackerDamageToDefender;

    {
        const double distanceModifier = [&defender, &attacker]() {
            if ( defender.Modes( CAP_TOWER ) ) {
                return 1.0;
            }

            if ( attacker.isFlying() || attacker.isArchers() ) {
                return 1.0;
            }

            const uint32_t attackerSpeed = attacker.GetSpeed( true, false );
            assert( attackerSpeed > Speed::STANDING );

            const uint32_t attackRange = attackerSpeed + 1;

            const uint32_t distance = Board::GetDistance( attacker.GetPosition(), defender.GetPosition() );
            assert( distance > 0 );

            if ( distance <= attackRange ) {
                return 1.0;
            }

            return 1.5 * static_cast<double>( distance ) / static_cast<double>( attackerSpeed );
        }();

        attackerThreat /= distanceModifier;
    }

    if ( attacker.isDoubleAttack() ) {
        // The following logic of accounting for potential retaliatory damage is intended only for the case when one unit directly attacks another, and will not work in
        // cases where one unit attacks several units at once
        assert( !attacker.isAbilityPresent( fheroes2::MonAbil::TWO_CELL_MELEE_ATTACK )
                && !attacker.isAbilityPresent( fheroes2::MonAbil::ALL_ADJACENT_MELEE_ATTACK )
                && !attacker.isAbilityPresent( fheroes2::MonAbil::AREA_SHOT ) );

        const uint32_t retaliatoryDamage = [&defender, &attacker, attackerDamageToDefender]() -> uint32_t {
            if ( defender.Modes( CAP_TOWER ) ) {
                return 0;
            }

            if ( attacker.isIgnoringRetaliation() ) {
                return 0;
            }

            if ( attacker.isArchers() && !attacker.isHandFighting() ) {
                return 0;
            }

            return defender.EstimateRetaliatoryDamage( attackerDamageToDefender );
        }();

        // If the defender is able to retaliate the attacker after his first attack, then the attacker's second attack can cause less damage than the first
        if ( retaliatoryDamage > 0 ) {
            assert( attacker.GetHitPoints() > 0 );

            // Rough but quick estimate
            attackerThreat += attackerThreat * ( 1.0 - std::min( static_cast<double>( retaliatoryDamage ) / static_cast<double>( attacker.GetHitPoints() ), 1.0 ) );
        }
        // Otherwise, estimate the second attack as approximately equal to the first in damage
        else {
            attackerThreat *= 2;
        }
    }

    if ( attacker.isAbilityPresent( fheroes2::MonAbil::ENEMY_HALVING ) ) {
        attackerThreat *= 2;
    }

    if ( attacker.isAbilityPresent( fheroes2::MonAbil::SOUL_EATER ) ) {
        attackerThreat *= 3;
    }

    if ( attacker.isAbilityPresent( fheroes2::MonAbil::HP_DRAIN ) ) {
        attackerThreat *= 1.3;
    }

    if ( attacker.isAbilityPresent( fheroes2::MonAbil::ETHER_DAMAGE ) ) {
        attackerThreat *= 1.2;
    }

    if ( attacker.isAbilityPresent( fheroes2::MonAbil::ETHER_SHOT_DAMAGE ) ) {
        attackerThreat *= 1.1;
    }

    {
        auto& attackerStats = fheroes2::getMonBattleStats( id );

        if ( attackerStats.spellCast > 0 ) {
            const auto getDefenderDamage = [&defender]() {
                if ( defender.Modes( SP_CURSE ) ) {
                    return defender.GetDamageMin();
                }

                if ( defender.Modes( SP_BLESS ) ) {
                    return defender.GetDamageMax();
                }

                return ( defender.GetDamageMin() + defender.GetDamageMax() ) / 2;
            };

            switch ( attackerStats.spellCast ) {
            case Spell::BLIND:
            case Spell::PARALYZE:
            case Spell::PETRIFY:
                // Creature's built-in magic resistance (not 100% immunity but resistance, as, for example, with Dwarves) never works against the built-in magic of
                // another creature (for example, Unicorn's Blind ability). Only the probability of triggering the built-in magic matters.
                if ( defender.AllowApplySpell( attackerStats.spellCast, nullptr ) ) {
                    attackerThreat += static_cast<double>( getDefenderDamage() ) * attackerStats.spellPercent / 100.0;
                }
                break;
            case Spell::DISPEL:
                // TODO: add the logic to evaluate this spell value.
                break;
            case Spell::SLOW:
                // Creature's built-in magic resistance (not 100% immunity but resistance, as, for example, with Dwarves) never works against the built-in magic of
                // another creature (for example, Unicorn's Blind ability). Only the probability of triggering the built-in magic matters.
                if ( defender.AllowApplySpell( attackerStats.spellCast, nullptr ) ) {
                    attackerThreat += static_cast<double>( getDefenderDamage() ) * attackerStats.spellPercent / 100.0 / 4;
                }
                break;
            default:
                // Did you add a new spell casting ability? Add the logic above!
                assert( 0 );
                break;
            }
        }
    }

    // Give the mirror images a higher priority, as they can be destroyed in 1 hit
    if ( attacker.Modes( CAP_MIRRORIMAGE ) ) {
        attackerThreat *= 10;
    }

    // Heavy penalty for hitting our own units
    if ( attacker.GetColor() == defender.GetCurrentColor() ) {
        // TODO: remove this temporary assertion
        assert( dynamic_cast<const Battle::Tower *>( this ) == nullptr );

        attackerThreat *= -2;
    }
    // Negative value of units that changed the side
    else if ( attacker.GetColor() != attacker.GetCurrentColor() ) {
        attackerThreat *= -1;
    }
    // Ignore disabled enemy units
    else if ( attacker.isImmovable() ) {
        attackerThreat = 0;
    }
    // Reduce the priority of those enemy units that have already got their turn
    else if ( attacker.Modes( TR_MOVED ) ) {
        attackerThreat /= 1.25;
    }

    return attackerThreat;
}

Funds Battle::Unit::GetSurrenderCost() const
{
    // Resurrected (not truly resurrected) units should not be taken into account when calculating the cost of surrender
    return GetCost() * ( GetDead() > GetMaxCount() ? 0 : GetMaxCount() - GetDead() );
}

int Battle::Unit::GetControl() const
{
    return !GetArmy() ? CONTROL_AI : GetArmy()->GetControl();
}

void Battle::Unit::_spellModesAction( const Spell & spell, uint32_t duration, const HeroBase * applyingHero )
{
    if ( applyingHero ) {
        duration += applyingHero->GetBagArtifacts().getTotalArtifactEffectValue( fheroes2::ArtifactBonusType::EVERY_COMBAT_SPELL_DURATION );
    }

    switch ( spell.GetID() ) {
    case Spell::BLESS:
    case Spell::MASSBLESS:
        _replaceAffection( SP_CURSE, SP_BLESS, duration );
        break;

    case Spell::BLOODLUST:
        _addAffection( SP_BLOODLUST, duration );
        break;

    case Spell::CURSE:
    case Spell::MASSCURSE:
        _replaceAffection( SP_BLESS, SP_CURSE, duration );
        break;

    case Spell::HASTE:
    case Spell::MASSHASTE:
        _replaceAffection( SP_SLOW, SP_HASTE, duration );
        break;

    case Spell::DISPEL:
    case Spell::MASSDISPEL:
        removeAffection( IS_MAGIC );
        break;

    case Spell::SHIELD:
    case Spell::MASSSHIELD:
        _addAffection( SP_SHIELD, duration );
        break;

    case Spell::SLOW:
    case Spell::MASSSLOW:
        _replaceAffection( SP_HASTE, SP_SLOW, duration );
        break;

    case Spell::STONESKIN:
        _replaceAffection( SP_STEELSKIN, SP_STONESKIN, duration );
        break;

    case Spell::BLIND:
        _addAffection( SP_BLIND, duration );
        // Blindness can be cast by an attacking unit. In this case, there should be no response to the attack.
        _blindRetaliation = false;
        break;

    case Spell::DRAGONSLAYER:
        _addAffection( SP_DRAGONSLAYER, duration );
        break;

    case Spell::STEELSKIN:
        _replaceAffection( SP_STONESKIN, SP_STEELSKIN, duration );
        break;

    case Spell::ANTIMAGIC:
        _replaceAffection( IS_MAGIC, SP_ANTIMAGIC, duration );
        break;

    case Spell::PARALYZE:
        _addAffection( SP_PARALYZE, duration );
        break;

    case Spell::BERSERKER:
        _replaceAffection( SP_HYPNOTIZE, SP_BERSERKER, duration );
        break;

    case Spell::HYPNOTIZE:
        _replaceAffection( SP_BERSERKER, SP_HYPNOTIZE, duration );
        break;

    case Spell::PETRIFY:
        _addAffection( SP_STONE, duration );
        break;

    case Spell::MIRRORIMAGE:
        // Special case, CAP_MIRROROWNER mode will be set when mirror image unit will be created
        _affected.AddMode( CAP_MIRROROWNER, duration );
        break;

    case Spell::DISRUPTINGRAY:
        ++_disruptingRaysNum;
        break;

    default:
        assert( 0 );
        break;
    }
}

void Battle::Unit::_spellApplyDamage( const Spell & spell, const uint32_t spellPower, const HeroBase * applyingHero, TargetInfo & target )
{
    assert( spell.isDamage() );

    const uint32_t dmg = CalculateSpellDamage( spell, spellPower, applyingHero, target.damage, false /* ignore defending hero */ );

    // apply damage
    if ( dmg ) {
        target.damage = dmg;
        target.killed = _applyDamage( dmg );
    }
}

uint32_t Battle::Unit::CalculateSpellDamage( const Spell & spell, uint32_t spellPower, const HeroBase * applyingHero, const uint32_t targetDamage,
                                             const bool ignoreDefendingHero ) const
{
    assert( spell.isDamage() );

    uint32_t dmg = spell.Damage() * spellPower;

    // If multiple options are suitable at the same time, then the abilities are considered first (in order from more specific to less specific)
    {
        // Abilities
        //
        auto& monStats = fheroes2::getMonBattleStats( GetID() );

        if ( monStats.hasAbil( fheroes2::MonAbil::RESISTANCE_OF_DESTRUCTION_MAGIC ) ) {
            dmg = ( dmg + 1 ) / 2;
        }

        if ( monStats.hasAbil( fheroes2::MonAbil::MECHANICAL ) ) {
            if ( spell.isElementalSpell() || spell.GetID() == Spell::ARMAGEDDON ) {
                dmg = ( dmg + 1 ) / 2;
            }
        }
    }

    if ( applyingHero ) {
        const HeroBase * defendingHero = GetCommander();
        const bool useDefendingHeroArts = defendingHero && !ignoreDefendingHero;

        switch ( spell.GetID() ) {
        case Spell::COLDRAY:
        case Spell::COLDRING: {
            std::vector<int32_t> extraDamagePercent
                = applyingHero->GetBagArtifacts().getTotalArtifactMultipliedPercent( fheroes2::ArtifactBonusType::COLD_SPELL_EXTRA_EFFECTIVENESS_PERCENT );
            for ( const int32_t value : extraDamagePercent ) {
                dmg = ( dmg * ( 100 + value ) + 50 ) / 100;
            }

            if ( useDefendingHeroArts ) {
                const std::vector<int32_t> damageReductionPercent
                    = defendingHero->GetBagArtifacts().getTotalArtifactMultipliedPercent( fheroes2::ArtifactBonusType::COLD_SPELL_DAMAGE_REDUCTION_PERCENT );
                for ( const int32_t value : damageReductionPercent ) {
                    dmg = ( dmg * ( 100 - value ) + 50 ) / 100;
                }

                extraDamagePercent = defendingHero->GetBagArtifacts().getTotalArtifactMultipliedPercent( fheroes2::ArtifactCurseType::COLD_SPELL_EXTRA_DAMAGE_PERCENT );
                for ( const int32_t value : extraDamagePercent ) {
                    dmg = ( dmg * ( 100 + value ) + 50 ) / 100;
                }
            }

            break;
        }
        case Spell::FIREBALL:
        case Spell::FIREBLAST: {
            std::vector<int32_t> extraDamagePercent
                = applyingHero->GetBagArtifacts().getTotalArtifactMultipliedPercent( fheroes2::ArtifactBonusType::FIRE_SPELL_EXTRA_EFFECTIVENESS_PERCENT );
            for ( const int32_t value : extraDamagePercent ) {
                dmg = ( dmg * ( 100 + value ) + 50 ) / 100;
            }

            if ( useDefendingHeroArts ) {
                const std::vector<int32_t> damageReductionPercent
                    = defendingHero->GetBagArtifacts().getTotalArtifactMultipliedPercent( fheroes2::ArtifactBonusType::FIRE_SPELL_DAMAGE_REDUCTION_PERCENT );
                for ( const int32_t value : damageReductionPercent ) {
                    dmg = ( dmg * ( 100 - value ) + 50 ) / 100;
                }

                extraDamagePercent = defendingHero->GetBagArtifacts().getTotalArtifactMultipliedPercent( fheroes2::ArtifactCurseType::FIRE_SPELL_EXTRA_DAMAGE_PERCENT );
                for ( const int32_t value : extraDamagePercent ) {
                    dmg = ( dmg * ( 100 + value ) + 50 ) / 100;
                }
            }

            break;
        }
        case Spell::LIGHTNINGBOLT:
        case Spell::CHAINLIGHTNING: {
            const std::vector<int32_t> extraDamagePercent
                = applyingHero->GetBagArtifacts().getTotalArtifactMultipliedPercent( fheroes2::ArtifactBonusType::LIGHTNING_SPELL_EXTRA_EFFECTIVENESS_PERCENT );
            for ( const int32_t value : extraDamagePercent ) {
                dmg = ( dmg * ( 100 + value ) + 50 ) / 100;
            }

            if ( useDefendingHeroArts ) {
                const std::vector<int32_t> damageReductionPercent
                    = defendingHero->GetBagArtifacts().getTotalArtifactMultipliedPercent( fheroes2::ArtifactBonusType::LIGHTNING_SPELL_DAMAGE_REDUCTION_PERCENT );
                for ( const int32_t value : damageReductionPercent ) {
                    dmg = ( dmg * ( 100 - value ) + 50 ) / 100;
                }
            }

            if ( spell.GetID() == Spell::CHAINLIGHTNING ) {
                switch ( targetDamage ) {
                case 0:
                    break;
                case 1:
                    dmg /= 2;
                    break;
                case 2:
                    dmg /= 4;
                    break;
                case 3:
                    dmg /= 8;
                    break;
                default:
                    break;
                }
            }

            break;
        }
        case Spell::ELEMENTALSTORM:
        case Spell::ARMAGEDDON: {
            if ( useDefendingHeroArts ) {
                const std::vector<int32_t> damageReductionPercent
                    = defendingHero->GetBagArtifacts().getTotalArtifactMultipliedPercent( fheroes2::ArtifactBonusType::ELEMENTAL_SPELL_DAMAGE_REDUCTION_PERCENT );
                for ( const int32_t value : damageReductionPercent ) {
                    dmg = ( dmg * ( 100 - value ) + 50 ) / 100;
                }
            }

            break;
        }
        default:
            break;
        }
    }

    return dmg;
}

void Battle::Unit::_spellRestoreAction( const Spell & spell, const uint32_t spellPoints, const HeroBase * applyingHero )
{
    switch ( spell.GetID() ) {
    case Spell::CURE:
    case Spell::MASSCURE:
        // clear bad magic
        removeAffection( IS_BAD_MAGIC );

        // restore
        _hitPoints += fheroes2::getHPRestorePoints( spell, spellPoints, applyingHero );

        _hitPoints = std::min( _hitPoints, ArmyTroop::GetHitPoints() );
        break;

    case Spell::RESURRECT:
    case Spell::ANIMATEDEAD:
    case Spell::RESURRECTTRUE: {
        if ( !isValid() ) {
            Graveyard * graveyard = Arena::GetGraveyard();
            assert( graveyard != nullptr );

            graveyard->removeUnit( this );
        }

        const uint32_t restore = fheroes2::getResurrectPoints( spell, spellPoints, applyingHero );
        const uint32_t resurrect = _resurrect( restore, false, ( spell == Spell::RESURRECT ) );

        // Put the unit back on the board
        SetPosition( GetPosition() );

        if ( Arena::GetInterface() ) {
            std::string str( _n( "%{count} %{name} rises from the dead!", "%{count} %{name} rise from the dead!", resurrect ) );
            StringReplace( str, "%{count}", resurrect );
            StringReplace( str, "%{name}", Monster::GetPluralName( resurrect ) );
            Arena::GetInterface()->setStatus( str, true );
        }
        break;
    }

    default:
        assert( 0 );
        break;
    }
}

bool Battle::Unit::isDoubleAttack() const
{
    if ( !isArchers() || isHandFighting() ) {
        return isAbilityPresent( fheroes2::MonAbil::DOUBLE_MELEE_ATTACK );
    }

    // Archers with double shooting ability can only fire a second shot if they have enough ammo
    if ( isAbilityPresent( fheroes2::MonAbil::DOUBLE_SHOOTING ) ) {
        return GetShots() > 1;
    }

    return false;
}

uint32_t Battle::Unit::GetMagicResist( const Spell & spell, const HeroBase * applyingHero ) const
{
    if ( Modes( SP_ANTIMAGIC ) ) {
        return 100;
    }

    switch ( spell.GetID() ) {
    case Spell::CURE:
    case Spell::MASSCURE:
        if ( !isHaveDamage() && !( modes & IS_BAD_MAGIC ) ) {
            return 100;
        }
        break;

    case Spell::RESURRECT:
    case Spell::RESURRECTTRUE:
    case Spell::ANIMATEDEAD:
        if ( GetCount() == _maxCount ) {
            return 100;
        }
        break;

    case Spell::DISPEL:
    case Spell::MASSDISPEL:
        if ( !( modes & IS_MAGIC ) ) {
            return 100;
        }
        break;

    case Spell::HYPNOTIZE:
        assert( applyingHero != nullptr );

        if ( fheroes2::getHypnotizeMonsterHPPoints( spell, applyingHero->GetPower(), applyingHero ) < _hitPoints ) {
            return 100;
        }
        break;

    default:
        break;
    }

    const Artifact spellImmunityArt = getImmunityArtifactForSpell( GetCommander(), spell );
    if ( spellImmunityArt.isValid() ) {
        return 100;
    }

    return fheroes2::getSpellResistance( id, spell.GetID() );
}

Spell Battle::Unit::GetSpellMagic( Rand::PCG32 & randomGenerator )
{
    const fheroes2::MonsterBattleStats & battleStats = fheroes2::getMonBattleStats( GetID() );

    if ( battleStats.spellCast && battleStats.spellPercent ) {
        if ( _supportLuck.Check( battleStats.spellPercent, randomGenerator ) ) {
            // No luck to cast the spell.
            return battleStats.spellCast;
        }
    }

    return Spell::NONE;
}

bool Battle::Unit::isHaveDamage() const
{
    return _hitPoints < _maxCount * Monster::GetHitPoints();
}

bool Battle::Unit::SwitchAnimation( const int rule, const bool reverse /* = false */ )
{
    if ( rule == Monster_Info::STATIC && GetAnimationState() != Monster_Info::IDLE ) {
        // Reset the delay before switching to the 'IDLE' animation from 'STATIC'.
        checkIdleDelay();
    }

    return ( animation.switchAnimation( rule, reverse ) && animation.isValid() );
}

bool Battle::Unit::SwitchAnimation( const std::vector<int> & animationList, const bool reverse /* = false */ )
{
    return ( animation.switchAnimation( animationList, reverse ) && animation.isValid() );
}

int Battle::Unit::M82Attk( const Unit & enemy ) const
{
    const fheroes2::MonsterSound & sounds = fheroes2::getMonSounds( id );

    if ( isArchers() && !isHandFighting( *this, enemy ) ) {
        // Added a new shooter without sound? Grant him a voice!
        assert( sounds.rangeAttack != M82::UNKNOWN );
        return sounds.rangeAttack;
    }

    assert( sounds.meleeAttack != M82::UNKNOWN );
    return sounds.meleeAttack;
}

int Battle::Unit::M82Kill() const
{
    return fheroes2::getMonSounds( id ).death;
}

int Battle::Unit::M82Move() const
{
    return fheroes2::getMonSounds( id ).movement;
}

int Battle::Unit::M82Wnce() const
{
    return fheroes2::getMonSounds( id ).wince;
}

int Battle::Unit::M82Expl() const
{
    return fheroes2::getMonSounds( id ).explosion;
}

int Battle::Unit::M82Tkof() const
{
    return fheroes2::getMonSounds( id ).takeoff;
}

int Battle::Unit::M82Land() const
{
    return fheroes2::getMonSounds( id ).landing;
}

fheroes2::Point Battle::Unit::GetBackPoint() const
{
    const fheroes2::Rect & rt = _position.GetRect();
    return _isReflected ? fheroes2::Point( rt.x + rt.width, rt.y + rt.height / 2 ) : fheroes2::Point( rt.x, rt.y + rt.height / 2 );
}

fheroes2::Point Battle::Unit::GetCenterPoint() const
{
    const fheroes2::Sprite & sprite = Assets::getImage( GetMonsterSprite(), GetFrame() );

    const fheroes2::Rect & pos = _position.GetRect();
    const int32_t centerY = pos.y + pos.height + sprite.y() / 2 - 10;

    return { pos.x + pos.width / 2, centerY };
}

fheroes2::Point Battle::Unit::GetStartMissileOffset( const size_t direction ) const
{
    return animation.getProjectileOffset( direction );
}

PlayerColor Battle::Unit::GetCurrentColor() const
{
    if ( Modes( SP_BERSERKER ) ) {
        return PlayerColor::UNUSED; // Be aware of unknown color
    }

    if ( Modes( SP_HYPNOTIZE ) ) {
        const Arena * arena = GetArena();
        assert( arena != nullptr );

        return arena->GetOppositeColor( GetArmyColor() );
    }

    return GetColor();
}

PlayerColor Battle::Unit::GetCurrentOrArmyColor() const
{
    const PlayerColor color = GetCurrentColor();

    // Unknown color in case of SP_BERSERKER mode
    if ( color == PlayerColor::UNUSED ) {
        return GetArmyColor();
    }

    return color;
}

int Battle::Unit::GetCurrentControl() const
{
    // Let's say that berserkers belong to AI, which is not present in the battle
    if ( Modes( SP_BERSERKER ) ) {
        return CONTROL_AI;
    }

    if ( Modes( SP_HYPNOTIZE ) ) {
        const Arena * arena = GetArena();
        assert( arena != nullptr );

        return arena->getForce( GetCurrentColor() ).GetControl();
    }

    return GetControl();
}

const HeroBase * Battle::Unit::GetCommander() const
{
    return GetArmy() ? GetArmy()->GetCommander() : nullptr;
}

const HeroBase * Battle::Unit::GetCurrentOrArmyCommander() const
{
    const Arena * arena = GetArena();
    assert( arena != nullptr );

    return arena->getCommander( GetCurrentOrArmyColor() );
}

void Battle::Unit::_addAffection( const uint32_t mode, const uint32_t duration )
{
    assert( CountBits( mode ) == 1 );

    SetModes( mode );

    _affected.AddMode( mode, duration );
}

void Battle::Unit::removeAffection( const uint32_t mode )
{
    ResetModes( mode );

    _affected.RemoveMode( mode );
}

void Battle::Unit::_replaceAffection( const uint32_t modeToReplace, const uint32_t replacementMode, const uint32_t duration )
{
    removeAffection( modeToReplace );
    _addAffection( replacementMode, duration );
}
