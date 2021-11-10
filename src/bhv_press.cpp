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

#include "bhv_press.h"

#include "bhv_basic_move.h"
#include "bhv_basic_tackle.h"
#include "bhv_mark_close_opponent.h"

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
#include <rcsc/geom/ray_2d.h>

#include "neck_default_intercept_neck.h"

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_Press::execute( rcsc::PlayerAgent * agent )
{
    static bool S_recover_mode = false;

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Bhv_Press" );

    const rcsc::WorldModel & wm = agent->world();

    if ( wm.opponentsFromBall().empty() )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": no opponent. basic move" );
        return Bhv_BasicMove( M_home_pos ).execute( agent );
    }

    ///////////////////////////////////
    // tackle
    if ( Bhv_BasicTackle( 0.85, 90.0 ).execute( agent ) )
    {
        return true;
    }

    ///////////////////////////////////

    const int self_min = wm.interceptTable()->selfReachCycle();
    //const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( ! wm.existKickableTeammate()
         && ! wm.existKickableOpponent()
         && ( self_min < opp_min + 3 || self_min < 4 )
         )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": get ball" );
        agent->debugClient().addMessage( "Press:get" );
        rcsc::Body_Intercept2008().execute( agent );
#ifdef NO_USE_NECK_DEFAULT_INTERCEPT_NECK
        if ( wm.existKickableOpponent() )
        {
            agent->setNeckAction( new rcsc::Neck_TurnToBall() );
        }
        else
        {
            agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
        }
#else
        if ( wm.existKickableOpponent() )
        {
            agent->setNeckAction( new Neck_DefaultInterceptNeck
                                  ( new rcsc::Neck_TurnToBall() ) );
        }
        else
        {
            agent->setNeckAction( new Neck_DefaultInterceptNeck
                                  ( new rcsc::Neck_TurnToBallOrScan() ) );
        }
#endif
        return true;
    }

    ///////////////////////////////////
    // set target point

    const rcsc::PlayerObject * opponent = wm.opponentsFromBall().front();

    if ( opponent->distFromSelf() > 10.0
         && std::fabs( opponent->pos().y - M_home_pos.y ) > 10.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": y diff is too big. target=(%.1f, %.1f),"
                            " myhome=(%.1f, %.1f)",
                            opponent->pos().x, opponent->pos().y,
                            M_home_pos.x, M_home_pos.y );
        Bhv_MarkCloseOpponent( M_home_pos ).execute( agent );
        return true;
    }

    rcsc::Vector2D target_point = opponent->pos();

    ////////////////////////////////////////////////////////
    // check teammate closer than self
    {
        const double x_thr = wm.ourDefenseLineX() + 5.0;
        double my_dist2 = wm.self().pos().dist2( target_point );

        const rcsc::PlayerPtrCont::const_iterator t_end = wm.teammatesFromSelf().end();
        for ( rcsc::PlayerPtrCont::const_iterator t = wm.teammatesFromSelf().begin();
              t != t_end;
              ++t )
        {
            if ( (*t)->posCount() > 10 ) continue;
            if ( (*t)->pos().x < x_thr ) continue;

            double d2 = (*t)->pos().dist2( target_point );
            if ( d2 < my_dist2 || d2 < 3.0 * 3.0 )
            {
                rcsc::dlog.addText( rcsc::Logger::TEAM,
                                    __FILE__": exist other pressor(%.1f, %.1f)",
                                    (*t)->pos().x, (*t)->pos().y );
                Bhv_BasicMove( M_home_pos ).execute( agent );
                return true;
            }
        }
    }

    if ( std::fabs( target_point.y - wm.self().pos().y ) > 1.0 )
    {
        rcsc::Line2D opp_move_line( opponent->pos(),
                                    opponent->pos() + rcsc::Vector2D( -3.0, 0.0 ) );
        rcsc::Ray2D my_move_ray( wm.self().pos(),
                                 wm.self().body() );
        rcsc::Vector2D intersection = my_move_ray.intersection( opp_move_line );
        if ( intersection.isValid()
             && intersection.x < target_point.x
             && wm.self().pos().dist( intersection ) < target_point.dist( intersection ) * 1.1 )
        {
            target_point.x = intersection.x;
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": press. attack opponent. keep body direction" );
        }
        else
        {
            target_point.x -= 5.0;
            if ( target_point.x > wm.self().pos().x
                 && wm.self().pos().x > M_home_pos.x - 2.0 )
            {
                target_point.x = wm.self().pos().x;
            }
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": press. attack opponent. back " );
        }
    }

    ///////////////////////////////////
    // set dash power
    if ( wm.self().stamina() < rcsc::ServerParam::i().staminaMax() * 0.48 )
    {
        S_recover_mode = true;
    }
    if ( wm.self().stamina() > rcsc::ServerParam::i().staminaMax() * 0.75 )
    {
        S_recover_mode = false;
    }

    double dash_power;
    if ( S_recover_mode )
    {
        dash_power = wm.self().playerType().staminaIncMax();
        dash_power *= wm.self().recovery();
        dash_power *= 0.8;
    }
#if 0
    else if ( wm.ball().pos().x < wm.self().pos().x - 1.0 )
    {
        dash_power = rcsc::ServerParam::i().maxPower() * 0.9;
    }
    else if ( wm.ball().distFromSelf() > 10.0 )
    {
        dash_power = rcsc::ServerParam::i().maxPower() * 0.7;
    }
    else if ( wm.ball().distFromSelf() > 2.0 )
    {
        dash_power = rcsc::ServerParam::i().maxPower() * 0.75;
    }
#endif
    else
    {
        dash_power = rcsc::ServerParam::i().maxPower();
    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": go to (%.1f, %.1f)",
                        target_point.x, target_point.y );

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().addMessage( "press%.0f", dash_power );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( ! rcsc::Body_GoToPoint( target_point, dist_thr, dash_power
                                 ).execute( agent ) )
    {
        rcsc::Body_TurnToPoint( target_point ).execute( agent );
    }

    if ( wm.existKickableOpponent() )
    {
        agent->setNeckAction( new rcsc::Neck_TurnToBall() );
    }
    else
    {
        agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
    }

    return true;
}
