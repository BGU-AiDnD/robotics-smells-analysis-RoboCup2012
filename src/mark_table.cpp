// -*-c++-*-

/*!
  \file mark_table.cpp
  \brief mark target table Source File
*/

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.

 This code is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this code; see the file COPYING.  If not, write to
 the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mark_table.h"

#include "strategy.h"

#include <rcsc/player/player_agent.h>
#include <rcsc/player/player_predicate.h>
#include <rcsc/player/world_model.h>
#include <rcsc/common/logger.h>
#include <rcsc/timer.h>

#include <algorithm>
#include <iostream>
#include <sstream>

using namespace rcsc;

#define DEBUG
//#define DEBUG_PRINT_COMBINATION

/*-------------------------------------------------------------------*/
/*!
  \brief compare the distance from our goal (with scaled x)
*/
struct TargetPositionCmp {
private:
    static const double goal_x;
    static const double x_scale;
public:
    bool operator()( const MarkTable::Target * lhs,
                     const MarkTable::Target * rhs ) const
      {
          //return lhs->pos().x < rhs->pos().x;
          return ( std::pow( lhs->self_->pos().x - goal_x, 2.0 ) * x_scale
                   + std::pow( lhs->self_->pos().y, 2.0 ) )
              < ( std::pow( rhs->self_->pos().x - goal_x, 2.0 ) * x_scale
                  + std::pow( rhs->self_->pos().y, 2.0 ) );
      }
};

const double TargetPositionCmp::goal_x = -52.0;
const double TargetPositionCmp::x_scale = 1.2 * 1.2;

/*-------------------------------------------------------------------*/
/*!

*/
struct OpponentDistCmp {
private:
    const MarkTable::Marker & marker_;
public:
    OpponentDistCmp( const MarkTable::Marker & m )
        : marker_( m )
      { }

    bool operator()( const AbstractPlayerObject * lhs,
                     const AbstractPlayerObject * rhs ) const
      {
          return ( lhs->pos().dist2( marker_.pos_ )
                   < rhs->pos().dist2( marker_.pos_ ) );
      }
};

/*-------------------------------------------------------------------*/
/*!
  \brief compare the distance from our goal (with scaled x)
*/
struct MarkerPositionCmp {
private:
    static const double goal_x;
    static const double x_scale;
public:
    bool operator()( const MarkTable::Marker & lhs,
                     const MarkTable::Marker & rhs ) const
      {
          return ( std::pow( lhs.self_->pos().x - goal_x, 2.0 ) * x_scale
                   + std::pow( lhs.self_->pos().y, 2.0 ) )
              < ( std::pow( rhs.self_->pos().x - goal_x, 2.0 ) * x_scale
                  + std::pow( rhs.self_->pos().y, 2.0 ) );
      }
};

const double MarkerPositionCmp::goal_x = -47.0;
const double MarkerPositionCmp::x_scale = 1.5 * 1.5;

/*-------------------------------------------------------------------*/
/*!

*/
struct MarkerMatch {
    const AbstractPlayerObject * marker_;
    MarkerMatch( const AbstractPlayerObject * p )
        : marker_( p )
      { }

    bool operator()( const MarkTable::Marker * val ) const
      {
          return val->self_ == marker_;
      }
};

/*-------------------------------------------------------------------*/
/*!

*/
class AssignmentCmp {
public:
        bool operator()( const MarkTable::Assignment & lhs,
                         const MarkTable::Assignment & rhs ) const
      {
          return lhs.score_ < rhs.score_;
      }
};

/*-------------------------------------------------------------------*/
/*!

*/


/*-------------------------------------------------------------------*/
/*!

*/
MarkTable::MarkTable()
{

}

/*-------------------------------------------------------------------*/
/*!

*/
const
rcsc::AbstractPlayerObject *
MarkTable::getTargetOf( const int marker_unum ) const
{
    const std::vector< Pair >::const_iterator end = M_pairs.end();
    for ( std::vector< Pair >::const_iterator it = M_pairs.begin();
          it != end;
          ++it )
    {
        if ( it->marker_->unum() == marker_unum )
        {
            return it->target_;
        }
    }

    return static_cast< rcsc::AbstractPlayerObject * >( 0 );
}

/*-------------------------------------------------------------------*/
/*!

*/
const
rcsc::AbstractPlayerObject *
MarkTable::getTargetOf( const rcsc::AbstractPlayerObject * marker ) const
{
    const std::vector< Pair >::const_iterator end = M_pairs.end();
    for ( std::vector< Pair >::const_iterator it = M_pairs.begin();
          it != end;
          ++it )
    {
        if ( it->marker_ == marker )
        {
            return it->target_;;
        }
    }

    return static_cast< rcsc::AbstractPlayerObject * >( 0 );
}

/*-------------------------------------------------------------------*/
/*!

*/
const
rcsc::AbstractPlayerObject *
MarkTable::getMarkerOf( const int target_unum ) const
{
    const std::vector< Pair >::const_iterator end = M_pairs.end();
    for ( std::vector< Pair >::const_iterator it = M_pairs.begin();
          it != end;
          ++it )
    {
        if ( it->target_->unum() == target_unum )
        {
            return it->marker_;
        }
    }

    return static_cast< rcsc::AbstractPlayerObject * >( 0 );
}

/*-------------------------------------------------------------------*/
/*!

*/
const
rcsc::AbstractPlayerObject *
MarkTable::getMarkerOf( const rcsc::AbstractPlayerObject * target ) const
{
    const std::vector< Pair >::const_iterator end = M_pairs.end();
    for ( std::vector< Pair >::const_iterator it = M_pairs.begin();
          it != end;
          ++it )
    {
        if ( it->target_ == target )
        {
            return it->marker_;
        }
    }

    return static_cast< rcsc::AbstractPlayerObject * >( 0 );
}

/*-------------------------------------------------------------------*/
/*!

*/
const
MarkTable::Pair *
MarkTable::getPairOfMarker( const int unum ) const
{
    const std::vector< Pair >::const_iterator end = M_pairs.end();
    for ( std::vector< Pair >::const_iterator it = M_pairs.begin();
          it != end;
          ++it )
    {
        if ( it->marker_->unum() == unum )
        {
            return &(*it);
        }
    }

    return static_cast< const Pair * >( 0 );
}

/*-------------------------------------------------------------------*/
/*!

*/
const
MarkTable::Pair *
MarkTable::getPairOfTarget( const int unum ) const
{
    const std::vector< Pair >::const_iterator end = M_pairs.end();
    for ( std::vector< Pair >::const_iterator it = M_pairs.begin();
          it != end;
          ++it )
    {
        if ( it->target_->unum() == unum )
        {
            return &(*it);
        }
    }

    return static_cast< const Pair * >( 0 );
}

/*-------------------------------------------------------------------*/
/*!

*/
const
MarkTable::UnumPair *
MarkTable::getUnumPairOfMarker( const int unum ) const
{
    const std::vector< UnumPair >::const_iterator end = M_unum_pairs.end();
    for ( std::vector< UnumPair >::const_iterator it = M_unum_pairs.begin();
          it != end;
          ++it )
    {
        if ( it->marker_ == unum )
        {
            return &(*it);
        }
    }

    return static_cast< const UnumPair * >( 0 );
}

/*-------------------------------------------------------------------*/
/*!

*/
const
MarkTable::UnumPair *
MarkTable::getUnumPairOfTarget( const int unum ) const
{
    const std::vector< UnumPair >::const_iterator end = M_unum_pairs.end();
    for ( std::vector< UnumPair >::const_iterator it = M_unum_pairs.begin();
          it != end;
          ++it )
    {
        if ( it->target_ == unum )
        {
            return &(*it);
        }
    }

    return static_cast< const UnumPair * >( 0 );
}

/*-------------------------------------------------------------------*/
/*!

*/
void
MarkTable::update( const WorldModel & wm )
{
    static GameTime update_time( 0, 0 );

    static std::vector< Target > target_opponents;
    static std::vector< Assignment > assignments;
    static std::vector< const Marker * > combination_stack;

    if ( update_time == wm.time() )
    {
        return;
    }

    update_time = wm.time();

    //
    // clear old variables
    //
    M_pairs.clear();

    if ( wm.gameMode().type() == GameMode::BeforeKickOff
         || wm.gameMode().type() == GameMode::AfterGoal_ )
    {
        M_unum_pairs.clear();
        return;
    }

    MSecTimer timer;

    //
    // create combinations and eavluate them
    //

    createMarkTargets( wm, target_opponents );
#ifdef DEBUG
    dlog.addText( Logger::MARK,
                  "MarkTable: create target elapsed %.3f [ms]",
                  timer.elapsedReal() );
#endif
    if ( target_opponents.empty() )
    {
        dlog.addText( Logger::MARK,
                      "MarkTable: no target opponent" );
        M_unum_pairs.clear();
        return;
    }

    //
    // create the pointer container
    //
    std::vector< Target * > ptr_target_opponents;
    ptr_target_opponents.reserve( target_opponents.size() );
    for ( std::vector< Target >::iterator it = target_opponents.begin();
          it != target_opponents.end();
          ++it )
    {
        ptr_target_opponents.push_back( &(*it) );
    }

    // sort by distance from our goal
    std::sort( ptr_target_opponents.begin(),
               ptr_target_opponents.end(),
               TargetPositionCmp() );

#ifdef DEBUG
    for ( std::vector< Target * >::const_iterator o = ptr_target_opponents.begin();
          o != ptr_target_opponents.end();
          ++o )
    {
        for ( std::vector< Marker >::const_iterator m = (*o)->markers_.begin();
              m != (*o)->markers_.end();
              ++m )
        {
            dlog.addText( Logger::MARK,
                          "_ opp %d (%.1f %.1f) : marker=%d (%.1f %.1f) dist=%.2f",
                          (*o)->self_->unum(),
                          (*o)->self_->pos().x, (*o)->self_->pos().y,
                          m->self_->unum(),
                          m->self_->pos().x, m->self_->pos().y,
                          std::sqrt( m->dist2_ ) );
        }
    }
#endif

    //
    // create combinations
    //
    createCombination( ptr_target_opponents.begin(), ptr_target_opponents.end(),
                       combination_stack,
                       assignments );
#ifdef DEBUG
    dlog.addText( Logger::MARK,
                  "MarkTable: create combination elapsed %.3f [ms]",
                  timer.elapsedReal() );
#endif

    //
    // evaluate all combinations
    //
    evaluate( assignments );
#ifdef DEBUG
    dlog.addText( Logger::MARK,
                  "MarkTable: evaluate elapsed %.3f [ms]",
                  timer.elapsedReal() );
#endif

    std::vector< Assignment >::iterator it
        = std::max_element( assignments.begin(),
                            assignments.end(),
                            AssignmentCmp() );

    //
    // clear old variables
    //
    M_unum_pairs.clear();

    if ( it != assignments.end() )
    {
        dlog.addText( Logger::MARK,
                      "MarkTable: total combination= %d, best assignment score= %f",
                      (int)assignments.size(),
                      it->score_ );

        const std::vector< const Marker * >::iterator end = it->markers_.end();
        for ( std::vector< const Marker * >::iterator m = it->markers_.begin();
              m != end;
              ++m )
        {
            M_pairs.push_back( Pair( (*m)->self_, (*m)->target_ ) );
            if ( (*m)->self_->unum() != Unum_Unknown
                 && (*m)->target_->unum() != Unum_Unknown )
            {
                M_unum_pairs.push_back( UnumPair( (*m)->self_->unum(),
                                                  (*m)->target_->unum() ) );
            }

            dlog.addText( Logger::MARK,
                          "<<< pair (%d, %d) marker(%.1f %.1f)(%.1f %.1f) - target(%.1f %.1f)",
                          (*m)->self_->unum(),
                          (*m)->target_->unum(),
                          (*m)->self_->pos().x, (*m)->self_->pos().y,
                          (*m)->pos_.x, (*m)->pos_.y,
                          (*m)->target_->pos().x, (*m)->target_->pos().y );
        }
    }
    else
    {

    }

    target_opponents.clear();
    assignments.clear();
    combination_stack.clear();

    dlog.addText( Logger::MARK,
                  "MarkTable: elapsed %.3f [ms]",
                  timer.elapsedReal() );
}

/*-------------------------------------------------------------------*/
/*!

*/
void
MarkTable::createMarkTargets( const rcsc::WorldModel & wm,
                              std::vector< Target > & target_opponents )
{
    const double dist_thr2 = 20.0 * 20.0;
    const double x_thr = 12.0;
    const double y_thr = 10.0;
    //const double opponent_max_x = 25.0;
    const double opponent_max_x = ( std::fabs( wm.offsideLineX() - wm.ourDefenseLineX() ) > 10.0
                                    ? wm.offsideLineX() * 0.5 + wm.ourDefenseLineX() * 0.5
                                    : wm.ourDefenseLineX() + 5.0 );
    const double current_position_weight = 0.5;
    const size_t max_partial_size = 3;

    const Strategy & strategy = Strategy::i();

    dlog.addText( Logger::MARK,
                  "MarkTable::createMarkTargets() opponent_max_x = %.2f",
                  opponent_max_x );

    //
    // create marker teammates
    //
    std::vector< Marker > markers;
    markers.reserve( wm.ourPlayers().size() );
    {
        const AbstractPlayerCont::const_iterator end = wm.ourPlayers().end();
        for ( AbstractPlayerCont::const_iterator t = wm.ourPlayers().begin();
              t != end;
              ++t )
        {
            if ( (*t)->goalie() ) continue;
            if ( (*t)->pos().x < wm.ourDefenseLineX() - 3.0 ) continue;
            //if ( ! strategy.playerIsMarkable( (*t)->unum() ) ) continue;

            Vector2D pos = (*t)->unum() != Unum_Unknown
                ? strategy.getPosition( (*t)->unum() )
                : (*t)->pos();
            pos = ( pos * current_position_weight )
                + ( (*t)->pos() * ( 1.0 - current_position_weight ) );

            markers.push_back( Marker( *t, pos ) );
        }

        if ( markers.empty() )
        {
            dlog.addText( Logger::MARK,
                          "MarkTable::createMarkTargets() no marker" );
            return;
        }
    }
    // sort by distance from our goal
    std::sort( markers.begin(), markers.end(), MarkerPositionCmp() );

    const std::vector< Marker >::iterator m_end = markers.end();

    //
    // create mark target opponents
    //
    AbstractPlayerCont wm_opponents;
    wm_opponents.reserve( 10 );
    wm.getPlayerCont( wm_opponents,
                      new AndPlayerPredicate
                      ( new OpponentOrUnknownPlayerPredicate( wm ),
                        new FieldPlayerPredicate(),
                        //new NoGhostPlayerPredicate( 20 ),
                        new XCoordinateBackwardPlayerPredicate( opponent_max_x ) ) );

    if ( wm_opponents.empty() )
    {
        dlog.addText( Logger::MARK,
                      "MarkTable::createMarkTargets() no opponent" );
        return;
    }

    target_opponents.reserve( wm_opponents.size() );
    {
        const AbstractPlayerCont::const_iterator end = wm_opponents.end();
        for ( AbstractPlayerCont::const_iterator o = wm_opponents.begin();
              o != end;
              ++o )
        {
            target_opponents.push_back( Target( *o ) );
        }
    }

    const std::vector< Target >::iterator o_end = target_opponents.end();

    if ( target_opponents.empty() )
    {
        dlog.addText( Logger::MARK,
                      "MarkTable::createMarkTargets() marker_size=%d opponent_size=%d",
                      markers.size(), wm_opponents.size() );
        return;
    }

    //
    // create mark candidates for each marker teammate
    //
    const size_t partial_size = std::min( max_partial_size, wm_opponents.size() );
    double d2 = 0.0;
    for ( std::vector< Marker >::iterator m = markers.begin() + 1; // TODO: check markable player
          m != m_end;
          ++m )
    {
        std::partial_sort( wm_opponents.begin(),
                           wm_opponents.begin() + partial_size,
                           wm_opponents.end(),
                           OpponentDistCmp( *m ) );
//         dlog.addText( Logger::MARK,
//                       "__  marker %d (%.1f %.1f)",
//                       m->self_->unum(),
//                       m->self_->pos().x, m->self_->pos().y );

        for ( size_t i = 0; i < partial_size; ++i )
        {
//             dlog.addText( Logger::MARK,
//                           "____  target %d (%.1f %.1f) address=%d",
//                           wm_opponents[i]->unum(),
//                           wm_opponents[i]->pos().x,
//                           wm_opponents[i]->pos().y,
//                           wm_opponents[i] );
            for ( std::vector< Target >::iterator o = target_opponents.begin();
                  o != o_end;
                  ++o )
            {
                if ( o->self_ == wm_opponents[i] )
                {
                    if ( //std::fabs( o->self_->pos().x - m->pos_.x ) < x_thr
                         o->self_->pos().x < m->pos_.x + x_thr
                         && std::fabs( o->self_->pos().y - m->pos_.y ) < y_thr
                         && ( d2 = o->self_->pos().dist2( m->pos_ ) ) < dist_thr2 )
                    {
                        m->target_ = o->self_;
                        m->dist2_ = d2;
                        o->markers_.push_back( *m );
                    }
                    break;
                }
            }
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
MarkTable::createCombination( std::vector< Target *>::const_iterator first,
                              std::vector< Target * >::const_iterator last,
                              std::vector< const Marker * > & combination_stack,
                              std::vector< Assignment > & assignments )
{
    if ( first == last )
    {
        assignments.push_back( Assignment() );
        assignments.back().markers_ = combination_stack;
#ifdef DEBUG_PRINT_COMBINATION
        std::ostringstream os;
        for ( std::vector< const Marker * >::const_iterator p = combination_stack.begin();
              p != combination_stack.end();
              ++p )
        {
            os << (*p)->self_->unum() << ' ';
        }
        dlog.addText( Logger::MARK,
                      "-> add combination: %s",
                      os.str().c_str() );
#endif
        return;
    }

    std::size_t prev_size = assignments.size();

    const std::vector< Marker >::const_iterator m_end = (*first)->markers_.end();
    for ( std::vector< Marker >::const_iterator m = (*first)->markers_.begin();
          m != m_end;
          ++m )
     {
         if ( std::find_if( combination_stack.begin(), combination_stack.end(), MarkerMatch( m->self_ ) )
              == combination_stack.end() )
         {
             combination_stack.push_back( &(*m) );
             createCombination( first + 1, last, combination_stack, assignments );
             combination_stack.pop_back();
         }
#ifdef DEBUG_PRINT_COMBINATION
         else
         {
             std::ostringstream os;
             for ( std::vector< const Marker * >::const_iterator p = combination_stack.begin();
                   p != combination_stack.end();
                   ++p )
             {
                 os << (*p)->self_->unum() << ' ';
             }
             dlog.addText( Logger::MARK,
                           "xxx cancel: %s (%d)",
                           os.str().c_str(),
                           m->self_->unum() );
         }
#endif
     }

     if ( prev_size == assignments.size()
          && ! combination_stack.empty() )
     {
#ifdef DEBUG_PRINT_COMBINATION
        std::ostringstream os;
        for ( std::vector< const Marker * >::const_iterator p = combination_stack.begin();
              p != combination_stack.end();
              ++p )
        {
            os << (*p)->self_->unum() << ' ';
        }
        dlog.addText( Logger::MARK,
                      "-> add sub combination: %s",
                      os.str().c_str() );
#endif
        assignments.push_back( Assignment() );
        assignments.back().markers_ = combination_stack;
     }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
MarkTable::evaluate( std::vector< Assignment > & assignments )
{
    const double decay = 0.99;

    const std::vector< Assignment >::iterator end = assignments.end();
    for ( std::vector< Assignment >::iterator it = assignments.begin();
          it != end;
          ++it )
    {
        double k = 1.0;
        double score = 0.0;
        const std::vector< const Marker * >::const_iterator m_end = it->markers_.end();
        for ( std::vector< const Marker * >::const_iterator m = it->markers_.begin();
              m != m_end;
              ++m )
        {
            score += k / (*m)->dist2_;
            k *= decay;
        }

        it->score_ = score;
#ifdef DEBUG
        std::ostringstream os;
        for ( std::vector< const Marker * >::const_iterator m = it->markers_.begin();
              m != m_end;
              ++m )
        {
            os << '(' << (*m)->self_->unum() << ',' << (*m)->target_->unum() << ')';
        }
        dlog.addText( Logger::MARK,
                      "** eval: %s : %lf",
                      os.str().c_str(),
                      it->score_ );
#endif
    }
}
