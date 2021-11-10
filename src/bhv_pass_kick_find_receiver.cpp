// -*-c++-*-

/*!
  \file bhv_pass_kick_find_receiver.cpp
  \brief search the pass receiver player and perform pass kick if possible
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

#include "bhv_pass_kick_find_receiver.h"

#include <cmath>

#include <vector>
#include <algorithm>

#include <rcsc/math_util.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/abstract_player_object.h>

#include <rcsc/action/bhv_scan_field.h>
#include <rcsc/action/body_stop_ball.h>
#include <rcsc/action/body_turn_to_point.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_point.h>
#include <rcsc/action/neck_turn_to_player_or_scan.h>
#include <rcsc/action/view_synch.h>

#include "kick_table.h"
#include "bhv_pass_test.h"

using namespace rcsc;

const int Bhv_PassKickFindReceiver::COUNT_THRESHOLD = 0;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_PassKickFindReceiver::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_PassKickFindReceiver" );

    const WorldModel & wm = agent->world();

    if ( ! wm.self().isKickable() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": no kickable" );
        return false;
    }

    //
    // validate the pass
    //
    const Bhv_PassTest::PassRoute * pass = Bhv_PassTest::get_best_pass( wm );

    if ( ! pass )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": no pass course" );
        return false;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": pass receiver unum=%d (%.1f %.1f)",
                  pass->receiver_->unum(),
                  pass->receiver_->pos().x, pass->receiver_->pos().y );

    if ( pass->receiver_->isGhost() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": receiver is a ghost." );

        return false;
    }

    if ( pass->receiver_->posCount() <= COUNT_THRESHOLD )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": already found. pos_count=%d",
                      pass->receiver_->posCount() );
        return false;
    }

    //
    // set view mode
    //
    agent->setViewAction( new View_Synch() );

    const double next_view_half_width = agent->effector().queuedNextViewWidth().width() * 0.5;
    const double neck_min = ServerParam::i().minNeckAngle() - next_view_half_width + 10.0;
    const double neck_max = ServerParam::i().maxNeckAngle() + next_view_half_width - 10.0;

    //
    // check the current body dir
    //
    const Vector2D next_self_pos = wm.self().pos() + wm.self().vel();
    const Vector2D player_pos = pass->receiver_->pos() + pass->receiver_->vel();
    const AngleDeg player_angle = ( player_pos - next_self_pos ).th();

    double angle_diff = ( player_angle - wm.self().body() ).abs();
    if ( neck_min < angle_diff
         && angle_diff < neck_max )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": can look the receiver without turn. angle_diff=%.1f",
                      angle_diff );
        if ( doFirstKick( agent ) )
        {
            agent->setNeckAction( new Neck_TurnToPlayerOrScan( pass->receiver_,
                                                               COUNT_THRESHOLD ) );
            return true;
        }
    }

    //
    // stop the ball
    //
    if ( next_self_pos.dist( wm.ball().pos() + wm.ball().vel() )
         > wm.self().playerType().kickableArea() - 0.1
         && wm.ball().vel().r() >= 0.1 )
    {
        // TODO: try another kick to keep the ball
        // doKeepBall( abent );
        // return true;

        if ( Body_StopBall().execute( agent ) )
        {
            agent-> setNeckAction( new Neck_TurnToPoint( player_pos ) );

            agent->debugClient().addMessage( "PassKickFind:StopBall" );
            agent->debugClient().setTarget( pass->receiver_->unum() );
            dlog.addText( Logger::TEAM,
                          __FILE__": stop the ball." );
            return true;
        }

        dlog.addText( Logger::TEAM,
                      __FILE__": could not stop the ball." );
        return false;
    }


    //
    // perform turn & turn_neck
    //
    doFindReceiver( agent );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_PassKickFindReceiver::doFirstKick( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Bhv_PassTest::PassRoute * pass = Bhv_PassTest::get_best_pass( wm );
    if ( ! pass )
    {
         return false;
    }

    KickTable::Sequence kick_sequence;

    double first_speed = std::min( pass->first_speed_, ServerParam::i().ballSpeedMax() );
    double allowable_speed = first_speed * 0.96;

    if ( rcsc::KickTable::instance().simulate( wm,
                                               pass->receive_point_,
                                               first_speed,
                                               allowable_speed,
                                               3,
                                               kick_sequence )
         || kick_sequence.speed_ >= allowable_speed )
    {
        if ( kick_sequence.pos_list_.size() == 1 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": doFirstKick() can pass by 1 step. cancel." );
            return false;
        }

        agent->debugClient().addMessage( "PassKickFind:FirstKick%d",
                                         kick_sequence.pos_list_.size() );
        dlog.addText( Logger::TEAM,
                      __FILE__": doFirstKick() required several kick. perform the first kick." );

        Vector2D vel = kick_sequence.pos_list_.front() - wm.ball().pos();
        Vector2D kick_accel = vel - wm.ball().vel();
        agent->doKick( kick_accel.r() / wm.self().kickRate(),
                       kick_accel.th() - wm.self().body() );
        return true;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": doFirstKick() failed to search the kick sequence." );

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_PassKickFindReceiver::doFindReceiver( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Bhv_PassTest::PassRoute * pass = Bhv_PassTest::get_best_pass( wm );
    if ( ! pass )
    {
         return;
    }

    const double next_view_half_width = agent->effector().queuedNextViewWidth().width() * 0.5;
    const double neck_min = ServerParam::i().minNeckAngle() - next_view_half_width + 10.0;
    const double neck_max = ServerParam::i().maxNeckAngle() + next_view_half_width - 10.0;

    //
    // create candidate body target points
    //

    std::vector< Vector2D > body_points;
    body_points.reserve( 16 );

    const Vector2D next_self_pos = wm.self().pos() + wm.self().vel();
    const Vector2D player_pos = pass->receiver_->pos() + pass->receiver_->vel();
    const AngleDeg player_angle = ( player_pos - next_self_pos ).th();

    const double max_x = ServerParam::i().pitchHalfLength() - 7.0;
    const double min_x = ( max_x - 10.0 < next_self_pos.x
                           ? max_x - 10.0
                           : next_self_pos.x );
    const double max_y = std::max( ServerParam::i().pitchHalfLength() - 5.0,
                                   player_pos.absY() );
    const double y_step = std::max( 3.0, max_y / 5.0 );
    const double y_sign = sign( player_pos.y );

    // on the static x line (x = max_x)
    for ( double y = 0.0; y < max_y + 0.001; y += y_step )
    {
        body_points.push_back( Vector2D( max_x, y * y_sign ) );
    }

    // on the static y line (y == player_pos.y)
    for ( double x_rate = 0.9; x_rate >= 0.0; x_rate -= 0.1 )
    {
        double x = std::min( max_x,
                             max_x * x_rate + min_x * ( 1.0 - x_rate ) );
        body_points.push_back( Vector2D( x, player_pos.y ) );
    }

    //
    // evaluate candidate points
    //

    const double max_turn = wm.self().playerType().effectiveTurn( ServerParam::i().maxMoment(),
                                                                  wm.self().vel().r() );
    Vector2D best_point = Vector2D::INVALIDATED;
    double min_turn = 360.0;

    const std::vector< Vector2D >::const_iterator p_end = body_points.end();
    for ( std::vector< Vector2D >::const_iterator p = body_points.begin();
          p != p_end;
          ++p )
    {
        AngleDeg target_body_angle = ( *p - next_self_pos ).th();
        double turn_moment_abs = ( target_body_angle - wm.self().body() ).abs();

        dlog.addText( Logger::TEAM,
                      "____ body_point=(%.1f %.1f) angle=%.1f moment=%.1f",
                      p->x, p->y,
                      target_body_angle.degree(),
                      turn_moment_abs );

//         if ( turn_moment_abs > max_turn )
//         {
//             dlog.addText( Logger::TEAM,
//                           "____ xxxx cannot turn by 1 step" );
//             continue;
//         }

        double angle_diff = ( player_angle - target_body_angle ).abs();
        if ( neck_min < angle_diff
             && angle_diff < neck_max )
        {
            if ( turn_moment_abs < max_turn )
            {
                best_point = *p;
                dlog.addText( Logger::TEAM,
                              "____ oooo can turn and look" );
                break;
            }

            if ( turn_moment_abs < min_turn )
            {
                best_point = *p;
                min_turn = turn_moment_abs;
                dlog.addText( Logger::TEAM,
                              "____ ---- update candidate point min_turn=%.1f",
                              min_turn );
            }
        }
        else
        {
            dlog.addText( Logger::TEAM,
                          "____ xxxx cannot look" );
        }
    }

    if ( ! best_point.isValid() )
    {
        best_point.assign( max_x * 0.7 + wm.self().pos().x * 0.3,
                           wm.self().pos().y * 0.9 );
        dlog.addText( Logger::TEAM,
                      "__ could not find the target point" );
    }

    //
    // perform the action
    //

    Body_TurnToPoint( best_point ).execute( agent );
    agent->debugClient().addLine( next_self_pos, best_point );

    agent->setNeckAction( new Neck_TurnToPlayerOrScan( pass->receiver_,
                                                       COUNT_THRESHOLD ) );

    agent->debugClient().addMessage( "PassKickFind:Turn" );
    agent->debugClient().setTarget( pass->receiver_->unum() );

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": doFindReceiver() turn to receiver %d (%.1f %.1f)  target=(%.1f %.1f)",
                        pass->receiver_->unum(),
                        pass->receiver_->pos().x,
                        pass->receiver_->pos().y,
                        best_point.x, best_point.y );
}
