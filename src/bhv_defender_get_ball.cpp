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

#include "bhv_defender_get_ball.h"

#include "body_intercept2008.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include "neck_default_intercept_neck.h"

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/soccer_math.h>

#include "bhv_get_ball.h"

#define USE_BHV_GET_BALL

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_DefenderGetBall::execute( rcsc::PlayerAgent * agent )
{
#ifdef USE_BHV_GET_BALL
    rcsc::Rect2D bounding_rect( rcsc::Vector2D( -50.0, -30.0 ),
                                rcsc::Vector2D( -5.0, 30.0 ) );
    return Bhv_GetBall( bounding_rect ).execute( agent );
#else
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Bhv_DefenderGetBall" );

    const rcsc::WorldModel & wm = agent->world();
    //--------------------------------------------------
    // intercept
    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    //////////////////////////////////////////////////////////////////
    // intercept normal
    if ( ! wm.existKickableTeammate()
         && self_min <= mate_min
         && self_min <= opp_min - 2 )
    {
        agent->debugClient().addMessage( "DefGetBall(1)" );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": get ball normal" );
        rcsc::Body_Intercept2008().execute( agent );
#ifdef NO_USE_NECK_DEFAULT_INTERCEPT_NECK
        agent->setNeckAction( new rcsc::Neck_TurnToBall() );
#else
        if ( opp_min >= self_min + 3 )
        {
            agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        }
        else
        {
            agent->setNeckAction( new Neck_DefaultInterceptNeck
                                  ( new Neck_TurnToBall() ) );
        }
#endif
        return true;
    }

    //////////////////////////////////////////////////////////////////
    // attack to ball owner
    const rcsc::PlayerObject * ball_nearest = wm.getTeammateNearestToBall( 10 );
    if ( ( ball_nearest
           && ( ball_nearest->distFromBall() > wm.ball().distFromSelf()
                || wm.ball().distFromSelf() < 5.0 ) )
         && wm.self().pos().x < M_home_pos.x + 5.0
         && ( wm.existKickableOpponent() || opp_min < 3 )
         )
    {
        if ( wm.existKickableOpponent() )
        {
            rcsc::Vector2D target_point;
            rcsc::Vector2D opp_pos = wm.opponentsFromBall().front()->pos();
            target_point = opp_pos;
            target_point.x -= 0.2;
#if 1
            double y_diff = std::fabs( opp_pos.y - wm.self().pos().y );
            if ( y_diff > 2.0 ) target_point.x -= 1.0;
            if ( y_diff > 4.0 ) target_point.x -= 1.0;
#else
            double y_diff = std::fabs( opp_pos.y - wm.self().pos().y );
            if ( y_diff > 2.0 )
            {
                if ( opp_pos.x > -25.0 ) target_point.x -= 0.0;
                else if ( opp_pos.x > -30.0 ) target_point.x = -36.0;
                else if ( opp_pos.x > -36.0 ) target_point.x = -41.0;
                else if ( opp_pos.x > -42.0 ) target_point.x = -47.5;
                else target_point.x = -48.0;
            }

            if ( std::fabs( opp_pos.y - wm.self().pos().y ) < 0.8 )
            {
                target_point.x = opp_pos.x - 0.2;
            }

            target_point.y = opp_pos.y;
            if ( opp_pos.y < 0.0 ) target_point.y += 0.5;
            if ( opp_pos.y > 0.0 ) target_point.y -= 0.5;
#endif
            agent->debugClient().setTarget( target_point );
            agent->debugClient().addCircle( target_point, 0.1 );
            agent->debugClient().addMessage( "DefGetBall(2)" );
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": attack ball owner (%.1f, %.1f)",
                                target_point.x, target_point.y );
            rcsc::Body_GoToPoint( target_point,
                                  0.1,
                                  rcsc::ServerParam::i().maxPower()
                                  ).execute( agent );
            agent->setNeckAction( new rcsc::Neck_TurnToBall() );
        }
        else
        {
            rcsc::Vector2D ball_future
                = rcsc::inertia_n_step_point( wm.ball().pos(),
                                              wm.ball().vel(),
                                              opp_min,
                                              rcsc::ServerParam::i().ballDecay() );
            ball_future.x -= 0.2;
            agent->debugClient().setTarget( ball_future );
            agent->debugClient().addCircle( ball_future, 0.1 );
            agent->debugClient().addMessage( "DefGetBall(3)" );
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": attack ball future (%.1f, %.1f)",
                                ball_future.x, ball_future.y );
            rcsc::Body_GoToPoint( ball_future,
                                  0.1,
                                  rcsc::ServerParam::i().maxPower()
                                  ).execute( agent );
            agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
        }
        return true;
    }

    //////////////////////////////////////////////////////////////////
    // intercept close
    if ( ! wm.existKickableTeammate()
         && self_min <= 2 )
    {
        agent->debugClient().addMessage( "DefGetBall(4)" );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": get ball close" );
        rcsc::Body_Intercept2008().execute( agent );
#ifdef NO_USE_NECK_DEFAULT_INTERCEPT_NECK
        agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
#else
        if ( opp_min >= self_min + 3 )
        {
            agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        }
        else
        {
            agent->setNeckAction( new Neck_DefaultInterceptNeck
                                  ( new Neck_TurnToBallOrScan() ) );
        }
#endif
        return true;
    }

    //////////////////////////////////////////////////////////////////
    // attack to ball owner
    if ( ! wm.existKickableOpponent()
         && opp_min < self_min
         && wm.self().pos().x < M_home_pos.x + 5.0
         )
    {
        rcsc::Vector2D ball_future_pos
            = rcsc::inertia_n_step_point( wm.ball().pos(),
                                          wm.ball().vel(),
                                          opp_min,
                                          rcsc::ServerParam::i().ballDecay() );
        ball_future_pos.x -= 0.5;
        double my_dist = wm.self().pos().dist(ball_future_pos);
        if ( my_dist < 5.0 )
        {
            double mate_dist = 1000.0;
            const rcsc::PlayerObject * close_mate
                = wm.getTeammateNearestTo( ball_future_pos, 10, &mate_dist );
            if ( ! close_mate || mate_dist > my_dist )
            {
                agent->debugClient().addMessage( "DefGetBall(5)" );
                agent->debugClient().setTarget( ball_future_pos );
                agent->debugClient().addCircle( ball_future_pos, 0.1 );
                rcsc::dlog.addText( rcsc::Logger::TEAM,
                                    __FILE__": attack future ball owner (%.1f, %.1f)",
                                    ball_future_pos.x, ball_future_pos.y );
                rcsc::Body_GoToPoint( ball_future_pos,
                                      0.1,
                                      rcsc::ServerParam::i().maxPower()
                                      ).execute( agent );
                agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
                return true;
            }
        }

    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": failed" );
    return false;
#endif
}
