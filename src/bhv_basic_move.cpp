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

#include "bhv_basic_move.h"

#include "bhv_basic_tackle.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include "body_intercept2008.h"
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

#include "neck_offensive_intercept_neck.h"

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_BasicMove::execute( rcsc::PlayerAgent * agent )
{
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Bhv_BasicMove" );

    //-----------------------------------------------
    // tackle
    if ( Bhv_BasicTackle( 0.8, 80.0 ).execute( agent ) )
    {
        return true;
    }

    const rcsc::WorldModel & wm = agent->world();
    /*--------------------------------------------------------*/
    // chase ball
    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( ! wm.existKickableTeammate()
         && ( self_min <= 3
              || ( self_min < mate_min + 3
                   && self_min < opp_min + 4 )
              )
         )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": intercept" );
        rcsc::Body_Intercept2008().execute( agent );

        if ( M_turn_neck )
        {
#if 0
            if ( self_min == 4 && opp_min >= 2 )
            {
                agent->setViewAction( new rcsc::View_Wide() );
            }
            else if ( self_min == 3 && opp_min >= 2 )
            {
                agent->setViewAction( new rcsc::View_Normal() );
            }
            else if ( self_min > 10 )
            {
                agent->setViewAction( new rcsc::View_Normal() );
            }

            if ( wm.ball().distFromSelf()
                 < rcsc::ServerParam::i().visibleDistance() )
            {
                agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
            }
            else
            {
                agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
            }
#else
            agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
#endif
        }
        return true;
    }

    const double dash_power = getDashPower( agent, M_home_pos );

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    agent->debugClient().addMessage( "BasicMove%.0f", dash_power );
    agent->debugClient().setTarget( M_home_pos );
    agent->debugClient().addCircle( M_home_pos, dist_thr );
    if ( ! rcsc::Body_GoToPoint( M_home_pos, dist_thr, dash_power
                                 ).execute( agent ) )
    {
        if ( M_turn_at )
        {
            rcsc::Body_TurnToBall().execute( agent );
        }
        else
        {
            return false;
        }
    }
    if ( M_turn_neck )
    {
        if ( wm.existKickableOpponent()
             && wm.ball().distFromSelf() < 18.0 )
        {
            agent->setNeckAction( new rcsc::Neck_TurnToBall() );
        }
        else
        {
            agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
        }
    }
    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
double
Bhv_BasicMove::getDashPower( const rcsc::PlayerAgent * agent,
                             const rcsc::Vector2D & /*target_point*/ )
{
    static bool s_recover_mode = false;

    const rcsc::WorldModel & wm = agent->world();

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    // check recover
    if ( wm.self().stamina() < rcsc::ServerParam::i().staminaMax() * 0.5 )
    {
        s_recover_mode = true;
    }
    else if ( wm.self().stamina() > rcsc::ServerParam::i().staminaMax() * 0.7 )
    {
        s_recover_mode = false;
    }

    /*--------------------------------------------------------*/
    double dash_power = 100.0;
    const double my_inc
        = wm.self().playerType().staminaIncMax()
        * wm.self().recovery();

    if ( wm.ourDefenseLineX() > wm.self().pos().x
         && wm.ball().pos().x < wm.ourDefenseLineX() + 20.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": getDashPower() correct DF line. keep max power" );
        // keep max power
        dash_power = rcsc::ServerParam::i().maxPower();
    }
    else if ( s_recover_mode )
    {
        dash_power = my_inc - 25.0; // preffered recover value
        if ( dash_power < 0.0 ) dash_power = 0.0;

        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": getDashPower() recovering" );
    }
    // exist kickable teammate
    else if ( wm.existKickableTeammate()
              && wm.ball().distFromSelf() < 20.0 )
    {
        dash_power = std::min( my_inc * 1.1,
                               rcsc::ServerParam::i().maxPower() );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": getDashPower() exist kickable teammate. dash_power=%.1f",
                            dash_power );
    }
    // in offside area
    else if ( wm.self().pos().x > wm.offsideLineX() )
    {
        dash_power = rcsc::ServerParam::i().maxPower();
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": in offside area. dash_power=%.1f",
                            dash_power );
    }
    else if ( wm.ball().pos().x > 25.0
              && wm.ball().pos().x > wm.self().pos().x > 10.0
              && self_min < opp_min - 6
              && mate_min < opp_min - 6 )
    {
        dash_power = rcsc::bound( rcsc::ServerParam::i().maxPower() * 0.1,
                                  my_inc * 0.5,
                                  rcsc::ServerParam::i().maxPower() );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": getDashPower() opponent ball dash_power=%.1f",
                            dash_power );
    }
    // normal
    else
    {
        dash_power = std::min( my_inc * 1.7,
                               rcsc::ServerParam::i().maxPower() );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": normal mode dash_power=%.1f",
                            dash_power );
    }

    return dash_power;
}
