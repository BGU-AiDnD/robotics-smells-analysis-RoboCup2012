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

#include "bhv_line_defense.h"

#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>
#include <rcsc/action/neck_turn_to_ball_and_player.h>

#include "body_intercept2008.h"
#include "bhv_basic_tackle.h"
#include "bhv_block_dribble.h"

#include "strategy.h"

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_LineDefense::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_LineDefense" );

    if ( doIntercept( agent ) )
    {
        return true;
    }

    return doAction( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_LineDefense::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    bool chase_ball = false;

    if ( self_min == 1 ) chase_ball = true;
    if ( self_min == 2 && opp_min >= 1 ) chase_ball = true;
    if ( self_min >= 3 && opp_min >= self_min - 1 ) chase_ball = true;
    if ( mate_min <= self_min - 2 ) chase_ball = false;
    if ( wm.existKickableTeammate() || mate_min == 0 ) chase_ball = false;

    if ( ! chase_ball )
    {
        return false;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": intercept." );
    agent->debugClient().addMessage( "LineDef:Intercept" );

    Body_Intercept2008().execute( agent );

    if ( self_min == 3 && opp_min >= 3 )
    {
        agent->setViewAction( new rcsc::View_Normal() );
    }

    agent->setNeckAction( new Neck_TurnToBallOrScan() );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_LineDefense::doAction( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    Vector2D target_point = getTargetPoint( agent );
    double dash_power = Strategy::get_defender_dash_power( wm, target_point );

    double dist_thr = wm.ball().pos().dist( target_point ) * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    dlog.addText( Logger::ROLE,
                  __FILE__": go to (%.1f %.1f). power=%.1f",
                  target_point.x, target_point.y,
                  dash_power );
    //
    // body action
    //

    if ( Body_GoToPoint( target_point,
                         dist_thr,
                         dash_power,
                         100, // cycle
                         false, // no back
                         true, // stamina save
                         15.0 // dir threshold
                         ).execute( agent ) )
    {
        // still need to turn and/or dash
        agent->debugClient().addMessage( "LineDef%.0f", dash_power );
        agent->debugClient().setTarget( target_point );
        agent->debugClient().addCircle( target_point, dist_thr );

        dlog.addText( Logger::ROLE,
                      __FILE__": GoToPoint" );
    }
    else
    {
        Vector2D ball_next = wm.ball().pos() + wm.ball().vel();
        Vector2D my_final = wm.self().inertiaFinalPoint();
        AngleDeg ball_angle = ( ball_next - my_final ).th();

        AngleDeg target_angle;
        if ( wm.ball().pos().x < -30.0 )
        {
            target_angle = ball_angle + 90.0;
            if ( ball_next.x < -45.0 )
            {
                if ( target_angle.degree() < 0.0 )
                {
                    target_angle += 180.0;
                }
            }
            else if ( target_angle.degree() > 0.0 )
            {
                target_angle += 180.0;
            }
        }
        else
        {
            target_angle = ball_angle + 90.0;
            if ( wm.ball().pos().x > wm.self().pos().x + 15.0 )
            {
                if ( target_angle.abs() > 90.0 )
                {
                    target_angle += 180.0;
                }
            }
            else
            {
                if ( target_angle.abs() < 90.0 )
                {
                    target_angle += 180.0;
                }
            }
        }

        Body_TurnToAngle( target_angle ).execute( agent );

        agent->debugClient().addMessage( "LineDefTurn%.0f",
                                         target_angle.degree() );
        agent->debugClient().setTarget( target_point );
        agent->debugClient().addCircle( target_point, dist_thr );

        dlog.addText( Logger::ROLE,
                      __FILE__": TurnToAngle %.1f",
                      target_angle.degree() );
    }

    //
    // view action
    //

    // TODO

    //
    // neck action
    //

    const PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();
    int opp_min = wm.interceptTable()->opponentReachCycle();


    if ( fastest_opp
         && ( opp_min <= 2
              || fastest_opp->distFromBall() < 2.0 )
         )
    {
        agent->debugClient().addMessage( "LookBallAndPlayer" );
        dlog.addText( Logger::ROLE,
                      __FILE__": neck to ball and player %d (%.1f %.1f)",
                      fastest_opp->unum(),
                      fastest_opp->pos().x, fastest_opp->pos().y );
        agent->setNeckAction( new Neck_TurnToBallAndPlayer( fastest_opp ) );
    }
    else
    {
        agent->debugClient().addMessage( "LoockBallOrScan" );
        dlog.addText( Logger::ROLE,
                      __FILE__": neck to ball or scan" );
        agent->setNeckAction( new Neck_TurnToBallOrScan() );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_LineDefense::getTargetPoint( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    Vector2D target_point = home_pos;

    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();
    int access_step = std::min( mate_min, opp_min );
    const Vector2D trap_pos = ( access_step < 100
                                ? wm.ball().inertiaPoint( access_step )
                                : wm.ball().inertiaFinalPoint() );

    dlog.addText( Logger::ROLE,
                  __FILE__": getTargetPoint trap_pos=(%.1f %.1f)",
                  trap_pos.x, trap_pos.y );

    /*--------------------------------------------------------*/
    // decide wait position
    // adjust to the defense line
    if ( -30.0 < home_pos.x
         && home_pos.x < -10.0
         //&& wm.self().pos().x > home_pos.x
         //&& wm.ball().pos().x > wm.self().pos().x
         )
    {
        // make static line
        double tmpx = home_pos.x;
        for ( double x = -10.0; x >= -34.0; x -= 5.0 )
        {
            if ( trap_pos.x - 3.3 > x )
            {
                tmpx = x;
                dlog.addText( Logger::ROLE,
                              __FILE__": getTargetPoint found line_x=%.1f  new_x=&.1f",
                              x, tmpx );
                break;
            }
        }

        target_point.x = tmpx;

        if ( std::fabs( home_pos.y - trap_pos.y ) < 10.0 )
        {
            target_point.y = target_point.y * 0.7 + trap_pos.y * 0.3;
        }

        agent->debugClient().addMessage( "LineDef%.1f,%.1f",
                                         target_point.x,
                                         target_point.y );
    }

    return target_point;
}
