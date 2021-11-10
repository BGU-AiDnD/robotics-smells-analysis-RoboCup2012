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

#include "bhv_defensive_half_defensive_move.h"

#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include "body_intercept2008.h"

#include "bhv_basic_tackle.h"
#include "bhv_block_dribble.h"
#include "bhv_get_ball.h"
#include "bhv_danger_area_tackle.h"
#include "neck_check_ball_owner.h"
#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

#include "strategy.h"

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_DefensiveHalfDefensiveMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": Bhv_DefensiveHalfDefensiveMove" );
    //
    // tackle
    //
    if ( Bhv_BasicTackle( 0.85, 75.0 ).execute( agent ) )
    {
        return true;
    }

    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    //
    // intercept
    //
    if ( ! wm.existKickableTeammate()
         && self_min <= mate_min
         && self_min <= opp_min + 1 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept" );
        Body_Intercept2008().execute( agent );
#ifdef NO_USE_NECK_DEFAULT_INTERCEPT_NECK
        agent->setNeckAction( new Neck_TurnToBallOrScan() );
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

    //
    // get ball
    //
    if ( ! wm.existKickableTeammate()
         && opp_min <= mate_min
         && opp_min <= self_min )
    {
        Rect2D bounding_rect( Vector2D( home_pos.x - 10.0, home_pos.y - 15.0 ),
                              Vector2D( home_pos.x + 6.0, home_pos.y + 15.0 ) );
        dlog.addText( Logger::ROLE,
                      __FILE__": try GetBall. rect=(%.1f %.1f)(%.1f %.1f)",
                      bounding_rect.left(), bounding_rect.top(),
                      bounding_rect.right(), bounding_rect.bottom() );
        if ( Bhv_GetBall( bounding_rect ).execute( agent ) )
        {
            agent->debugClient().addMessage( "DH:GetBall" );
            return true;
        }
    }

    //
    // block dribble or intercept
    //
    const Vector2D self_reach_point = wm.ball().inertiaPoint( self_min );
    const Vector2D opp_reach_point = wm.ball().inertiaPoint( opp_min );

    if ( ! wm.existKickableTeammate()
         && self_reach_point.dist( home_pos ) < 13.0
         && ( self_min < mate_min
              || self_min <= 3 && wm.ball().pos().dist2( home_pos ) < 10.0*10.0
              || self_min <= 5 && wm.ball().pos().dist2( home_pos ) < 8.0*8.0 )
         )
    {
        if ( opp_min < mate_min - 1
             && opp_min < self_min - 2 )
        {
            if ( Bhv_BlockDribble().execute( agent ) )
            {
                dlog.addText( Logger::ROLE,
                              __FILE__": BlockDribble" );
                agent->debugClient().addMessage( "DH:BlockDrib" );
                return true;
            }
        }

        dlog.addText( Logger::ROLE,
                      __FILE__": intercept line=%d", __LINE__ );
        Body_Intercept2008().execute( agent );
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        return true;
    }

    //
    //
    //

    Vector2D trap_pos = wm.ball().inertiaPoint( std::min( mate_min, opp_min ) );

    double dash_power = ServerParam::i().maxPower();

    //
    // decide dash_power
    //
    if ( wm.existKickableTeammate() )
    {
        if ( wm.self().pos().dist( trap_pos ) > 10.0 )
        {
            if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.8 )
            {
                dash_power = ( wm.self().playerType().staminaIncMax()
                               * wm.self().recovery() );
                dlog.addText( Logger::ROLE,
                              __FILE__": dash_power, teammate kickable, stamina save" );
            }
        }
    }
    else if ( trap_pos.x < wm.self().pos().x ) // ball is behind
    {
        dash_power *= 0.9;
        dlog.addText( Logger::ROLE,
                      __FILE__": dash_power, trap_pos is behind. trap_pos=(%.1f %.1f)",
                      trap_pos.x, trap_pos.y );
    }
    else if ( wm.self().stamina() > ServerParam::i().staminaMax() * 0.8 )
    {
        dash_power *= 0.8;
        dlog.addText( Logger::ROLE,
                      __FILE__": dash_power, enough stamina" );
    }
    else
    {
        dash_power = ( wm.self().playerType().staminaIncMax()
                       * wm.self().recovery()
                       * 0.9 );
        dlog.addText( Logger::ROLE,
                      __FILE__": dash_power, default" );
    }

    // save recovery
    dash_power = wm.self().getSafetyDashPower( dash_power );

    //
    // register action
    //

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    dlog.addText( Logger::ROLE,
                  __FILE__": go to home (%.1f %.1f) dist_thr=%.3f. dash_power=%.1f",
                  home_pos.x, home_pos.y,
                  dist_thr,
                  dash_power );

    agent->debugClient().setTarget( home_pos );
    agent->debugClient().addCircle( home_pos, dist_thr );

    if ( Body_GoToPoint( home_pos, dist_thr, dash_power,
                         1, // 1 step
                         false, // no back dash
                         true // save recovery
                         ).execute( agent ) )
    {
        agent->debugClient().addMessage( "DH:DefMove:Go%.0f", dash_power );
    }
    else
    {
        AngleDeg body_angle = 0.0;
        if ( wm.ball().angleFromSelf().abs() > 80.0 )
        {
            body_angle = ( wm.ball().pos().y > wm.self().pos().y
                           ? 90.0
                           : -90.0 );
        }
        Body_TurnToAngle( body_angle ).execute( agent );
        agent->debugClient().addMessage( "DH:DefMove:Turn%.0f",
                                         body_angle.degree() );
    }

    agent->setNeckAction( new Neck_TurnToBallOrScan() );

    return true;
}
