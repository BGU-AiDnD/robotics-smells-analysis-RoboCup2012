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

#include "intention_receive.h"

#include "body_intercept2008.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

/*-------------------------------------------------------------------*/
/*!

*/
IntentionReceive::IntentionReceive( const rcsc::Vector2D & target_point,
                                    const double & dash_power,
                                    const double & buf,
                                    const int max_step,
                                    const rcsc::GameTime & start_time )
    : M_target_point( target_point )
    , M_dash_power( dash_power )
    , M_buffer( buf )
    , M_step( max_step )
    , M_last_execute_time( start_time )
{

}

/*-------------------------------------------------------------------*/
/*!

*/
bool
IntentionReceive::finished( const rcsc::PlayerAgent * agent )
{
    if ( M_step <= 0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": finished. time 0" );
        return true;
    }

    if ( agent->world().ball().distFromSelf() < 3.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": finished. ball very near" );
        return true;
    }

    if ( M_last_execute_time.cycle() < agent->world().time().cycle() - 1 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": finished. strange time." );
        return true;
    }

    if ( agent->world().self().pos().dist( M_target_point ) < M_buffer )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": finished. already there." );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
IntentionReceive::execute( rcsc::PlayerAgent * agent )
{
    if ( M_step <= 0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": execute. empty intention." );
        return false;
    }

    const rcsc::WorldModel & wm = agent->world();

    M_step -= 1;
    M_last_execute_time = wm.time();

    agent->debugClient().setTarget( M_target_point );
    agent->debugClient().addMessage( "IntentionRecv" );

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": execute. try to receive" );

    int self_min = wm.interceptTable()->selfReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( self_min < 6 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": execute. point very near. intercept" );
        agent->debugClient().addMessage( "Intercept_1" );
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
                                  ( new rcsc::Neck_TurnToBallOrScan() ) );
        }
#endif
        return true;
    }

    if ( ! wm.existKickableTeammate() )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": execute. not exist kickable teammate" );

        if ( opp_min < self_min )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": execute. opponent will reach faster than self" );
            if ( rcsc::Body_Intercept2008().execute( agent ) )
            {
                agent->debugClient().addMessage( "Intercept_2" );
#if 0
                if ( self_min == 4 && opp_min >= 3 )
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
                return true;
            }
        }
    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": execute. intercept cycle=%d. go to receive point",
                        self_min );
    agent->debugClient().addMessage( "GoTo" );
    agent->debugClient().setTarget( M_target_point );
    agent->debugClient().addCircle( M_target_point, M_buffer );

    rcsc::Body_GoToPoint( M_target_point,
                          M_buffer,
                          M_dash_power
                          ).execute( agent );
    agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );

    return true;
}
