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

#include "role_sample.h"

#include "body_clear_ball_test.h"
#include "body_dribble2008.h"
#include "body_intercept2008.h"
#include "body_hold_ball2008.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/bhv_before_kick_off.h>
#include <rcsc/action/bhv_scan_field.h>
#include <rcsc/action/bhv_neck_body_to_ball.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_go_to_point_dodge.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/body_kick_to_relative.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

/*-------------------------------------------------------------------*/
/*!

 */
void
RoleSample::execute( rcsc::PlayerAgent* agent )
{

    //////////////////////////////////////////////////////////////
    // play_on play
    if ( agent->world().self().isKickable() )
    {
        // kick
        doKick( agent );
    }
    else
    {
        // positioning
        doMove( agent );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
RoleSample::doKick( rcsc::PlayerAgent * agent )
{
    static rcsc::Vector2D target( 12.0, 12.0 );

    if ( agent->world().self().pos().dist( target ) < 3.0 )
    {
        target *= -1.0;
    }

    rcsc::Body_Dribble2008( target, 3.0, 100.0, 5 ).execute( agent );
    agent->setNeckAction( new rcsc::Neck_ScanField() );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
RoleSample::doMove( rcsc::PlayerAgent* agent )
{
    const rcsc::WorldModel & wm = agent->world();

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    // estimate fastest player to ball
    //if ( agent->world().interceptTable()->isSelfFastestPlayer() )
    if ( ( ! wm.existKickableTeammate()
           && ! wm.existKickableOpponent() )
         && self_min < opp_min
         && self_min < mate_min
        )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": doMove" );

        rcsc::Body_Intercept2008().execute( agent );

        if ( agent->world().ball().distFromSelf()
             < rcsc::ServerParam::i().visibleDistance() )
        {
            agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
        }
        else
        {
            agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
        }
    }
    else
    {
        std::cerr << "Sample: no formation. go to ball" << std::endl;
        if ( ! rcsc::Body_GoToPoint( agent->world().ball().pos(),
                                     0.5,
                                     rcsc::ServerParam::i().maxPower() * 0.9
                                     ).execute( agent ) )
        {
            rcsc::Body_TurnToBall().execute( agent );
        }

        agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
    }
}
