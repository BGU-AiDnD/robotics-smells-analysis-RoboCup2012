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

#include "bhv_defender_basic_block_move.h"

#include "bhv_basic_move.h"
#include "bhv_defender_get_ball.h"
#include "bhv_danger_area_tackle.h"

#include "body_intercept2008.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

#include "strategy.h"

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_DefenderBasicBlockMove::execute( rcsc::PlayerAgent * agent )
{
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Bhv_DefenderBasicBlock" );

    // tackle
    if ( Bhv_DangerAreaTackle().execute( agent ) )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": tackle" );
        return true;
    }

    // get ball
    if ( Bhv_DefenderGetBall( M_home_pos ).execute( agent ) )
    {
        return true;
    }

    const rcsc::WorldModel & wm = agent->world();

    // positioning

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    const double ball_xdiff
        = agent->world().ball().pos().x - agent->world().self().pos().x;

    if ( ball_xdiff > 10.0
         && ( wm.existKickableTeammate()
              || mate_min < opp_min - 1
              || self_min < opp_min - 1 )
        //&& agent->world().interceptTable()->isOurTeamBallPossessor() )
         )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": ball is front and our team keep ball" );
        Bhv_BasicMove( M_home_pos ).execute( agent );
        return true;
    }

    double dash_power = Strategy::get_defender_dash_power( wm, M_home_pos );

    double dist_thr = agent->world().ball().distFromSelf() * 0.1;
    if ( dist_thr < 1.5 ) dist_thr = 1.5;

    agent->debugClient().addMessage( "DefMove%.0f", dash_power );
    agent->debugClient().setTarget( M_home_pos );
    agent->debugClient().addCircle( M_home_pos, dist_thr );
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": go home. power=%.1f",
                        dash_power );

    if ( ! rcsc::Body_GoToPoint( M_home_pos,
                                 dist_thr,
                                 dash_power
                                 ).execute( agent ) )
    {
        rcsc::AngleDeg body_angle = 180.0;
        if ( agent->world().ball().angleFromSelf().abs() < 80.0 )
        {
            body_angle = 0.0;
        }
        rcsc::Body_TurnToAngle( body_angle ).execute( agent );
    }

    agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );

    return true;
}
