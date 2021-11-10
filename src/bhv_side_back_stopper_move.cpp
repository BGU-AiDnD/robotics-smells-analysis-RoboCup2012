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

#include "bhv_side_back_stopper_move.h"

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include "body_intercept2008.h"
#include "bhv_basic_move.h"
#include "bhv_defender_get_ball.h"
#include "bhv_danger_area_tackle.h"
#include "bhv_get_ball.h"
#include "neck_check_ball_owner.h"
#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

#include "strategy.h"

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackStopperMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_DefenderBasicBlock" );

    //
    // tackle
    //
    if ( Bhv_DangerAreaTackle().execute( agent ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": tackle" );
        return true;
    }

    //
    // intercept
    //
    if ( doIntercept( agent ) )
    {
        return true;
    }

    //
    // get ball
    //
    if ( doGetBall( agent ) )
    {
        return true;
    }

    doNormalMove( agent );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackStopperMove::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( self_min <= opp_min
         && self_min <= mate_min + 1
         && ! wm.existKickableTeammate() )
    {
        agent->debugClient().addMessage( "CB:Stopper:Intercept" );
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

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackStopperMove::doGetBall( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": (doGetBall)" );

    const WorldModel & wm = agent->world();

    int self_min = wm.interceptTable()->selfReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );
    opp_trap_pos.x -= 0.5;

    double self_dist = wm.self().pos().dist( opp_trap_pos );

    double teammate_dist = 1000.0;
    const PlayerObject * nearest_teammate = wm.getTeammateNearestTo( opp_trap_pos, 10, &teammate_dist );

    bool get_ball = false;

    dlog.addText( Logger::TEAM,
                  __FILE__": (doGetBall) judge situation. opp_trap_pos=(%.1f %.1f)  my_dist=%.2f  teammate_dist=%.2f",
                  opp_trap_pos.x, opp_trap_pos.y,
                  self_dist,
                  teammate_dist );

    if ( ( nearest_teammate
           && ( teammate_dist > self_dist
                || self_dist < 5.0 ) )
         && wm.self().pos().x < home_pos.x + 5.0
         && ( wm.existKickableOpponent() || opp_min < 3 )
         )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doGetBall) decide get_ball line=%d",
                      __LINE__ );
        get_ball = true;
    }

    if ( ! get_ball
         && ! wm.existKickableTeammate()
         && self_min <= 2 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doGetBall) decide get_ball line=%d",
                      __LINE__ );
        get_ball = true;
    }

    if ( ! get_ball
         && ! wm.existKickableOpponent()
         && opp_min < self_min
         && wm.self().pos().x < home_pos.x + 5.0
         && self_dist < 5.0
         &&  ( ! nearest_teammate
               || teammate_dist > self_dist )
         )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doGetBall) decide get_ball line=%d",
                      __LINE__ );
        get_ball = true;
    }

    if ( get_ball )
    {
        rcsc::Rect2D bounding_rect( rcsc::Vector2D( -52.0, -17.0 ),
                                    rcsc::Vector2D( -30.0, 17.0 ) );
        if ( Bhv_GetBall( bounding_rect ).execute( agent ) )
        {
            agent->debugClient().addMessage( "SB:GetBall" );
            return true;
        }
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": (doGetBall) failed" );
    return false;
}


/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SideBackStopperMove::doNormalMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    Vector2D next_self_pos = wm.self().pos() + wm.self().vel();
    Vector2D next_ball_pos = wm.ball().pos() + wm.ball().vel();

    const double ball_xdiff = next_ball_pos.x  - next_self_pos.x;

    if ( ball_xdiff > 10.0
         && ( wm.existKickableTeammate()
              || mate_min < opp_min - 1
              || self_min < opp_min - 1 )
         )
    {
        agent->debugClient().addMessage( "SB:Stopper:BasicMove" );
        dlog.addText( Logger::TEAM,
                      __FILE__": ball is front and our team keep ball" );
        Bhv_BasicMove( home_pos ).execute( agent );
        return;
    }

    double dash_power = Strategy::get_defender_dash_power( wm, home_pos );

    double dist_thr = agent->world().ball().distFromSelf() * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().setTarget( home_pos );
    agent->debugClient().addCircle( home_pos, dist_thr );

    dlog.addText( Logger::TEAM,
                  __FILE__": go home (%.1f %.1f). power=%.1f  dist_thr=%.2f",
                  home_pos.x, home_pos.y,
                  dash_power,
                  dist_thr );

    if ( Body_GoToPoint( home_pos,
                         dist_thr,
                         dash_power,
                         1, // step
                         false, // back dash
                         true, // save recovery
                         12.0 // dir thr
                         ).execute( agent ) )
    {
        agent->debugClient().addMessage( "SB:Stopper:Go%.1f", dash_power );
        dlog.addText( Logger::ROLE,
                      __FILE__": GoToPoint" );
    }
    else
    {
        AngleDeg body_angle = 180.0;
        if ( wm.ball().angleFromSelf().abs() < 80.0 )
        {
            body_angle = 0.0;
        }
        Body_TurnToAngle( body_angle ).execute( agent );

        agent->debugClient().addMessage( "SB:Stopper:Turn%.1f", dash_power );
        dlog.addText( Logger::TEAM,
                      __FILE__": turn to angle=%.1f",
                      body_angle.degree() );
    }

    //agent->setNeckAction( new Neck_TurnToBallOrScan() );
    agent->setNeckAction( new Neck_CheckBallOwner() );
}
