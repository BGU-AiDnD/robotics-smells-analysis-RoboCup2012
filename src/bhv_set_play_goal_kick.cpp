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

#include "bhv_set_play_goal_kick.h"

#include "bhv_set_play.h"
#include "bhv_prepare_set_play_kick.h"
#include "bhv_go_to_static_ball.h"
#include "bhv_pass_test.h"

#include "body_clear_ball2008.h"
#include "body_intercept2008.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/say_message_builder.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

#include <rcsc/math_util.h>

#include "neck_offensive_intercept_neck.h"

/*-------------------------------------------------------------------*/
/*!
  execute action
*/
bool
Bhv_SetPlayGoalKick::execute( rcsc::PlayerAgent * agent )
{
    if ( isKicker( agent ) )
    {
        doKicker( agent );
    }
    else
    {
        doNoKicker( agent );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_SetPlayGoalKick::isKicker( const rcsc::PlayerAgent * agent )
{
    if ( agent->world().setplayCount() < 5 )
    {
        return false;
    }

    const rcsc::PlayerPtrCont::const_iterator end
        = agent->world().teammatesFromBall().end();
    for ( rcsc::PlayerPtrCont::const_iterator it
              = agent->world().teammatesFromBall().begin();
          it != end;
          ++it )
    {
        if ( ! (*it)->goalie()
             && (*it)->distFromBall() < agent->world().ball().distFromSelf() )
        {
            // I am not a kicer == not the nearest player to the ball
            return false;
        }
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
Bhv_SetPlayGoalKick::doKicker( rcsc::PlayerAgent * agent )
{
    // I am kickaer
    static int S_scan_count = -5;

    // go to ball
    if ( Bhv_GoToStaticBall( 0.0 ).execute( agent ) )
    {
        // tring to reach the ball yet
        return;
    }

    // already ball point

    // face to the ball
    if ( ( agent->world().ball().angleFromSelf()
           - agent->world().self().body() ).abs() > 3.0 )
    {
        rcsc::Body_TurnToBall().execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
        return;
    }

    if ( S_scan_count < 0 )
    {
        S_scan_count++;
        rcsc::Body_TurnToBall().execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
        return;
    }

    if ( S_scan_count < 30
         && agent->world().teammatesFromSelf().empty() )
    {
        S_scan_count++;
        rcsc::Body_TurnToBall().execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
        return;
    }

    const rcsc::Bhv_PassTest::PassRoute * pass
        = rcsc::Bhv_PassTest::get_best_pass( agent->world() );

    if  ( pass
          && pass->receive_point_.dist( rcsc::Vector2D( -50.0, 0.0) ) > 20.0
          )
    {
        double opp_dist = 100.0;
        const rcsc::PlayerObject * opp
            = agent->world().getOpponentNearestTo( pass->receive_point_,
                                                   10,
                                                   &opp_dist );
        if ( ! opp || opp_dist > 5.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": pass to (%.1f, %.1f)",
                                pass->receive_point_.x,
                                pass->receive_point_.y );
            rcsc::Body_KickOneStep( pass->receive_point_,
                                    pass->first_speed_ ).execute( agent );
            if ( agent->effector().queuedNextBallKickable() )
            {
                agent->setNeckAction( new rcsc::Neck_ScanField() );
            }
            else
            {
                agent->setNeckAction( new rcsc::Neck_TurnToBall() );
            }
            return;
        }
    }

    if ( S_scan_count < 30 )
    {
        S_scan_count++;
        rcsc::Body_TurnToBall().execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
        return;
    }

    agent->debugClient().addMessage( "GoalKick:Clear" );
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": clear ball" );

    Body_ClearBall2008().execute( agent );
    agent->setNeckAction( new rcsc::Neck_ScanField() );

    S_scan_count = -5;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
Bhv_SetPlayGoalKick::doNoKicker( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();

    if ( wm.ball().vel().r2() > 0.01 )
    {
        int self_min = wm.interceptTable()->selfReachCycle();
        int mate_min = wm.interceptTable()->teammateReachCycle();
        if ( self_min < mate_min )
        {
            rcsc::Vector2D trap_pos = wm.ball().inertiaPoint( self_min );
            if ( trap_pos.x > rcsc::ServerParam::i().ourPenaltyAreaLineX() + 0.5
                 || trap_pos.y > rcsc::ServerParam::i().penaltyAreaHalfWidth() + 0.5 )
            {
                agent->debugClient().addMessage( "GoalKickIntercept" );
                rcsc::Body_Intercept2008().execute( agent );

#if 0
                int opp_min = wm.interceptTable()->opponentReachCycle();
                if ( self_min == 4 && opp_min >= 5 )
                {
                    agent->setViewAction( new rcsc::View_Wide() );
                }
                else if ( self_min == 3 && opp_min >= 4 )
                {
                    agent->setViewAction( new rcsc::View_Normal() );
                }
                else if ( self_min > 10 )
                {
                    agent->setViewAction( new rcsc::View_Normal() );
                }
                agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
#else
                agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
#endif
                return;
            }
        }
    }


    double dash_power = Bhv_SetPlay::get_set_play_dash_power( agent );
    double dist_thr = wm.ball().distFromSelf() * 0.07;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    rcsc::Vector2D target_point = M_home_pos;
    target_point.y += wm.ball().pos().y * 0.5;

    const rcsc::PlayerObject * nearest_opp = wm.getOpponentNearestToSelf( 5 );

    if ( nearest_opp && nearest_opp->distFromSelf() < 3.0 )
    {
        rcsc::Vector2D add_vec = ( wm.ball().pos() - target_point );
        add_vec.setLength( 3.0 );

        long time_val = wm.time().cycle() % 60;
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

    agent->debugClient().addMessage( "GoalKickMove" );
    agent->debugClient().setTarget( target_point );

    if ( ! rcsc::Body_GoToPoint( target_point,
                                 dist_thr,
                                 dash_power
                                 ).execute( agent ) )
    {
        // already there
        rcsc::Body_TurnToBall().execute( agent );
    }

    if ( wm.self().pos().dist( target_point )
         > wm.ball().pos().dist( target_point ) * 0.2 + 6.0 )
    {
        agent->debugClient().addMessage( "Sayw" );
        agent->addSayMessage( new rcsc::WaitRequestMessage() );
    }

    agent->setNeckAction( new rcsc::Neck_ScanField() );
}
