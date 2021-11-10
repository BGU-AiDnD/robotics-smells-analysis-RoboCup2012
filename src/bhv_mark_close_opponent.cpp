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

#include "bhv_mark_close_opponent.h"

#include "bhv_basic_tackle.h"
#include "bhv_basic_move.h"
#include "bhv_defender_basic_block_move.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/bhv_go_to_point_look_ball.h>
#include <rcsc/action/body_go_to_point.h>
#include "body_intercept2008.h"
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
bool
Bhv_MarkCloseOpponent::execute( rcsc::PlayerAgent * agent )
{
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Bhv_MarkCloseOpponent" );

    //-----------------------------------------------
    // tackle
    if ( Bhv_BasicTackle( 0.85, 60.0 ).execute( agent ) )
    {
        return true;
    }

    const rcsc::WorldModel & wm = agent->world();

    //-----------------------------------------------
    // intercept

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->teammateReachCycle();

    if ( ( ! wm.existKickableTeammate() && self_min < 4 )
         || ( self_min < mate_min /* && self_min < opp_min */ ) )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": intercept" );
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

    //--------------------------------------------------
    //if ( wm.interceptTable()->isSelfFastestPlayer() )
    if ( ! wm.existKickableOpponent()
         && ! wm.existKickableTeammate()
         && self_min < mate_min
         && self_min < opp_min )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": I am fastest" );
        rcsc::Body_Intercept2008().execute( agent );
#ifdef NO_USE_NECK_DEFAULT_INTERCEPT_NECK
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
        if ( wm.ball().distFromSelf()
             < rcsc::ServerParam::i().visibleDistance() )
        {
            agent->setNeckAction( new Neck_DefaultInterceptNeck
                                  ( new rcsc::Neck_TurnToLowConfTeammate() ) );
        }
        else
        {
            agent->setNeckAction( new Neck_DefaultInterceptNeck
                                  ( new rcsc::Neck_TurnToBallOrScan() ) );
        }
#endif
        return true;
    }

    if ( wm.ball().pos().x < -36.0
         && wm.ball().pos().absY() < 18.0
         && wm.ball().distFromSelf() < 5.0
         && ! wm.existKickableTeammate() )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": ball near" );
        if ( rcsc::Body_Intercept2008().execute( agent ) )
        {
#ifdef NO_USE_NECK_DEFAULT_INTERCEPT_NECK
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
            if ( wm.ball().distFromSelf()
                 < rcsc::ServerParam::i().visibleDistance() )
            {
                agent->setNeckAction( new Neck_DefaultInterceptNeck
                                      ( new rcsc::Neck_TurnToLowConfTeammate() ) );
            }
            else
            {
                agent->setNeckAction( new Neck_DefaultInterceptNeck
                                      ( new rcsc::Neck_TurnToBallOrScan() ) );
            }
#endif
            return true;
        }
    }

    ////////////////////////////////////////////////////////
    double dist_opp_to_home = 200.0;
    const rcsc::PlayerObject * opp
        = wm.getOpponentNearestTo( M_home_pos, 1, &dist_opp_to_home );

    ////////////////////////////////////////////////////////
    {
        const double dist_opp_to_ball = ( opp ? opp->distFromBall() : 100.0 );
        if ( wm.existKickableTeammate()
             && dist_opp_to_ball > 2.5 )
        {
            // not found
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": teammate kickable. opp is not near ball" );
            return Bhv_BasicMove( M_home_pos ).execute( agent );
        }
    }

    ////////////////////////////////////////////////////////

    rcsc::Vector2D nearest_opp_pos( rcsc::Vector2D::INVALIDATED );
    if ( opp )
    {
        nearest_opp_pos = opp->pos();
    }

    // found candidate opponent
    if ( nearest_opp_pos.isValid()
         && ( dist_opp_to_home < 7.0
              || M_home_pos.x > nearest_opp_pos.x )
         )
    {
        // do mark
    }
    else
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": mark target not found" );
        // not found
        return Bhv_DefenderBasicBlockMove( M_home_pos ).execute( agent );
    }

    ////////////////////////////////////////////////////////
    // check teammate closer than self
    {
        bool opp_is_not_danger
            = ( nearest_opp_pos.x > -35.0 || nearest_opp_pos.absY() > 17.0 );
        const double dist_opp_to_home2 = dist_opp_to_home * dist_opp_to_home;
        const rcsc::PlayerPtrCont::const_iterator end = wm.teammatesFromSelf().end();
        for ( rcsc::PlayerPtrCont::const_iterator
                  it = wm.teammatesFromSelf().begin();
              it != end;
              ++it )
        {
            if ( opp_is_not_danger
                 && (*it)->pos().x < wm.ourDefenseLineX() + 3.0 )
            {
                continue;
            }

            if ( (*it)->pos().dist2( nearest_opp_pos ) < dist_opp_to_home2 )
            {
                rcsc::dlog.addText( rcsc::Logger::TEAM,
                                    __FILE__": exist other marker(%.1f, %.1f)",
                                    (*it)->pos().x, (*it)->pos().y );
                return Bhv_DefenderBasicBlockMove( M_home_pos ).execute( agent );
            }
        }
    }

    ////////////////////////////////////////////////////////
    rcsc::AngleDeg block_angle = ( wm.ball().pos() - nearest_opp_pos ).th();
    rcsc::Vector2D block_point
        = nearest_opp_pos + rcsc::Vector2D::polar2vector( 0.2, block_angle );
    block_point.x -= 0.1;

    ////////////////////////////////////////////////////////
    if ( block_point.x < wm.self().pos().x - 1.5 )
    {
        block_point.y = wm.self().pos().y;
    }


    double dash_power = rcsc::ServerParam::i().maxPower() * 0.9;
    double x_diff = block_point.x - wm.self().pos().x;

    if ( x_diff > 20.0 )
    {
        dash_power *= 0.5;
    }
    else if ( x_diff > 10.0 )
    {
        dash_power *= 0.5;
    }
    else
    {
        dash_power *= 0.7;
    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": mark to (%.1f, %.1f)",
                        block_point.x, block_point.y );

    double dist_thr = wm.ball().distFromSelf() * 0.07;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().addMessage( "marking" );
    agent->debugClient().setTarget( block_point );
    agent->debugClient().addCircle( block_point, dist_thr );

    if ( ! rcsc::Body_GoToPoint( block_point, dist_thr, dash_power ).execute( agent ) )
    {
        rcsc::Body_TurnToBall().execute( agent );
    }
    agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );

    return true;
}
