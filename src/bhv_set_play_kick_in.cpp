// -*-c++-*-

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

#include "bhv_set_play_kick_in.h"

#include "bhv_set_play.h"
#include "bhv_prepare_set_play_kick.h"

#include "body_advance_ball_test.h"
#include "bhv_pass_test.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/body_kick_collide_with_ball.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/say_message_builder.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

#include <rcsc/math_util.h>

/*-------------------------------------------------------------------*/
/*!
  execute action
*/
bool
Bhv_SetPlayKickIn::execute( rcsc::PlayerAgent * agent )
{
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Bhv_SetPlayKickIn" );

    //---------------------------------------------------
    if ( isKicker( agent ) )
    {
        doKick( agent );
    }
    ///////////////////////////////////////////////////////
    else
    {
        doMove( agent );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!
  check if I am kicker
*/
bool
Bhv_SetPlayKickIn::isKicker( const rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();

    const rcsc::PlayerObject * nearest_mate
        = ( wm.teammatesFromBall().empty()
            ? static_cast< rcsc::PlayerObject* >( 0 )
            : wm.teammatesFromBall().front() );

    if ( wm.setplayCount() < 5
         || ( nearest_mate
              && nearest_mate->distFromBall() < wm.ball().distFromSelf() * 0.9 )
         )
    {
        // is_kicker = false;
    }
    else
    {
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
Bhv_SetPlayKickIn::doKick( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();

    rcsc::AngleDeg ball_place_angle = ( wm.ball().pos().y > 0.0
                                        ? -90.0
                                        : 90.0 );

    if ( Bhv_PrepareSetPlayKick( ball_place_angle, 10 ).execute( agent ) )
    {
        // go to kick point
        return;
    }

    const double max_kick_speed = wm.self().kickRate() * rcsc::ServerParam::i().maxPower();

    const rcsc::Bhv_PassTest::PassRoute * pass
        = rcsc::Bhv_PassTest::get_best_pass( wm );
    if  ( pass
          && pass->receive_point_.x > -35.0
          && pass->receive_point_.x < 50.0 )
    {
        // enforce one step kick
        double ball_speed = std::min( pass->first_speed_, max_kick_speed );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": pass to (%.1f, %.1f) ball_speed+%.1f",
                            pass->receive_point_.x,
                            pass->receive_point_.y,
                            ball_speed );
        rcsc::Body_KickOneStep( pass->receive_point_,
                                ball_speed
                                ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
        return;
    }

    const rcsc::PlayerObject * receiver = wm.getTeammateNearestToBall( 10 );
    if ( receiver
         && receiver->distFromBall() < 10.0
         && receiver->pos().absX() < rcsc::ServerParam::i().pitchHalfLength()
         && receiver->pos().absY() < rcsc::ServerParam::i().pitchHalfWidth() )
    {
        rcsc::Vector2D target_point = receiver->pos();
        target_point.x += 0.6;

        double ball_speed
            = rcsc::calc_first_term_geom_series_last( 1.8, // end speed
                                                      wm.ball().pos().dist( target_point ),
                                                      rcsc::ServerParam::i().ballDecay() );
        ball_speed = std::min( ball_speed, max_kick_speed );

        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__":  kick to nearest teammate (%.1f %.1f) speed=%.2f",
                            target_point.x, target_point.y,
                            ball_speed );
        rcsc::Body_KickOneStep( target_point,
                                ball_speed
                                ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
        return;
    }

    if ( wm.self().pos().x < 20.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": advance(1)" );
        Body_AdvanceBallTest().execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
        return;
    }

    {
        rcsc::Vector2D target_point( rcsc::ServerParam::i().pitchHalfLength() - 2.0,
                                     ( rcsc::ServerParam::i().pitchHalfWidth() - 5.0 )
                                     * ( 1.0 - ( wm.self().pos().x
                                                 / rcsc::ServerParam::i().pitchHalfLength() ) ) );
        if ( wm.self().pos().y < 0.0 )
        {
            target_point.y *= -1.0;
        }
        // enforce one step kick
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": advance(2) to (%.1f, %.1f)",
                            target_point.x, target_point.y );
        rcsc::Body_KickOneStep( target_point,
                                rcsc::ServerParam::i().ballSpeedMax()
                                ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
Bhv_SetPlayKickIn::doMove( rcsc::PlayerAgent * agent )
{
    rcsc::Vector2D target_point = M_home_pos;

    const rcsc::PlayerObject * nearest_opp = agent->world().getOpponentNearestToSelf( 5 );

    if ( nearest_opp && nearest_opp->distFromSelf() < 3.0 )
    {
        rcsc::Vector2D add_vec = ( agent->world().ball().pos() - target_point );
        add_vec.setLength( 3.0 );

        long time_val = agent->world().time().cycle() % 60;
        if ( time_val < 20 )
        {

        }
        else if ( time_val < 40 )
        {
            target_point += add_vec.rotatedVector( 90.0 );
        }
        else
        {
            target_point += add_vec.rotatedVector( -90.0 );
        }

        target_point.x = rcsc::min_max( - rcsc::ServerParam::i().pitchHalfLength(),
                                        target_point.x,
                                        + rcsc::ServerParam::i().pitchHalfLength() );
        target_point.y = rcsc::min_max( - rcsc::ServerParam::i().pitchHalfWidth(),
                                        target_point.y,
                                        + rcsc::ServerParam::i().pitchHalfWidth() );
    }

    double dash_power
        = Bhv_SetPlay::get_set_play_dash_power( agent );
    double dist_thr = agent->world().ball().distFromSelf() * 0.07;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    agent->debugClient().addMessage( "KickInMove" );
    agent->debugClient().setTarget( target_point );
    if ( ! rcsc::Body_GoToPoint( target_point,
                                 dist_thr,
                                 dash_power
                                 ).execute( agent ) )
    {
        // already there
        if ( ! agent->world().teammatesFromBall().empty()
             && agent->world().teammatesFromBall().front()->distFromBall() > 3.0 )
        {
            agent->doTurn( 120.0 );
        }
        else
        {
            rcsc::Body_TurnToBall().execute( agent );
        }
    }

    if ( agent->world().self().pos().dist( target_point )
         > agent->world().ball().pos().dist( target_point ) * 0.2 + 6.0 )
    {
        agent->debugClient().addMessage( "Sayw" );
        agent->addSayMessage( new rcsc::WaitRequestMessage() );
    }

    if ( ! agent->world().teammatesFromBall().empty()
         && agent->world().teammatesFromBall().front()->distFromBall() > 3.0 )
    {
        agent->setViewAction( new rcsc::View_Wide() );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
    }
    else
    {
        agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
    }
}
