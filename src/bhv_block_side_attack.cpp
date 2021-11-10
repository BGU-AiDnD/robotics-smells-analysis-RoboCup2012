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

#include "bhv_block_side_attack.h"

#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
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
Bhv_BlockSideAttack::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_BlockSideAttack" );
    const WorldModel & wm = agent->world();

    agent->debugClient().addMessage( "BlockSide" );

    //
    // tackle
    //
    if ( Bhv_BasicTackle( 0.85, 60.0 ).execute( agent ) )
    {
        return true;
    }

    //
    // intercept
    //
    if ( doChaseBall( agent ) )
    {
        return true;
    }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    if ( wm.self().pos().x < wm.ball().pos().x - 5.0
         || ( home_pos.x < wm.ball().pos().x - 5.0
              && wm.self().pos().x < home_pos.x )
         )
    {
        return false;
    }

    //
    // block dribble
    //
    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();
    Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );
    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    if ( opp_min < mate_min - 1
         && opp_min < self_min
         && ( position_type == Position_Center
              || ( position_type == Position_Left
                   && opp_trap_pos.y <= 0.0 )
              || ( position_type == Position_Right
                   && opp_trap_pos.y >= 0.0 ) )
         && opp_trap_pos.absY() > 20.0 )
    {
        agent->debugClient().addMessage( "BlockDrib" );
        dlog.addText( Logger::TEAM,
                      __FILE__": block dribble" );
        if ( Bhv_BlockDribble().execute( agent ) )
        {
            return true;
        }
    }

    //
    // block move
    //
    Vector2D block_point = home_pos;

    if ( wm.ball().pos().x < wm.self().pos().x )
    {
        agent->debugClient().addMessage( "Static" );

        block_point.x = -42.0;
        block_point.y = wm.self().pos().y;
        if ( wm.ball().pos().absY() < wm.self().pos().absY() - 0.5 )
        {
            agent->debugClient().addMessage( "Correct" );
            block_point.y = 10.0;
            if ( home_pos.y < 0.0 ) block_point.y *= -1.0;
        }
        dlog.addText( Logger::ROLE,
                      __FILE__": static move (%.1f %.1f)",
                      block_point.x, block_point.y );
    }
    else
    {
        if ( wm.ball().pos().x > -30.0
             && wm.self().pos().x > home_pos.x
             && wm.ball().pos().x > wm.self().pos().x + 3.0 )
        {
            // make static line
            // 0, -10, -20, ...
            //double tmpx = std::floor( wm.ball().pos().x * 0.1 ) * 10.0 - 3.5; //3.0;
            double tmpx;
            if ( wm.ball().pos().x > -10.0 ) tmpx = -13.5;
            else if ( wm.ball().pos().x > -16.0 ) tmpx = -19.5;
            else if ( wm.ball().pos().x > -24.0 ) tmpx = -27.5;
            else if ( wm.ball().pos().x > -30.0 ) tmpx = -33.5;
            else tmpx = home_pos.x;

            if ( tmpx > 0.0 ) tmpx = 0.0;
            if ( tmpx > home_pos.x + 5.0 ) tmpx = home_pos.x + 5.0;

            if ( wm.ourDefenseLineX() < tmpx - 1.0 )
            {
                tmpx = wm.ourDefenseLineX() + 1.0;
            }

            block_point.x = tmpx;

            agent->debugClient().addMessage( "Line&.1f", tmpx );
            dlog.addText( Logger::ROLE,
                          __FILE__": make line (%.1f %.1f)",
                          block_point.x, block_point.y );
        }
        else
        {
            agent->debugClient().addMessage( "Home" );
            dlog.addText( Logger::ROLE,
                          __FILE__": no change, home pos (%.1f %.1f)",
                          block_point.x, block_point.y );
        }
    }

    //-----------------------------------------
    // set dash power
    double dash_power = ServerParam::i().maxPower();
    if ( wm.self().pos().x < wm.ball().pos().x - 8.0 )
    {
        dash_power = wm.self().playerType().staminaIncMax() * 0.8;
    }
    else if ( wm.self().pos().x < wm.ball().pos().x
              && wm.self().pos().absY() < wm.ball().pos().absY() - 1.0
              && wm.self().stamina() < ServerParam::i().staminaMax() * 0.65 )
    {
        dash_power = ServerParam::i().maxPower() * 0.7;
    }


    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 0.8 ) dist_thr = 0.8;

    agent->debugClient().setTarget( block_point );
    agent->debugClient().addCircle( block_point, dist_thr );

    if ( Body_GoToPoint( block_point, dist_thr, dash_power ).execute( agent ) )
    {
        agent->debugClient().addMessage( "Go%.0f", dash_power );
    }
    else
    {
        Vector2D face_point( -48.0, 0.0 );
        Body_TurnToPoint( face_point ).execute( agent );
        agent->debugClient().addMessage( "TurnTo()b%.1f %.1f",
                                         face_point.x, face_point.y );
    }

    agent->setNeckAction( new Neck_TurnToBallOrScan() );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_BlockSideAttack::doChaseBall( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( self_min < opp_min && self_min < mate_min )
    {
        agent->debugClient().addMessage( "Intercept(1)" );
        dlog.addText( Logger::TEAM,
                      __FILE__": Intercept (1)" );

        Body_Intercept2008().execute( agent );
        agent->setNeckAction( new Neck_TurnToBall() );
        return true;
    }

    Vector2D ball_get_pos = wm.ball().inertiaPoint( self_min );
    Vector2D opp_reach_pos = wm.ball().inertiaPoint( opp_min );

    if ( ( ( self_min <= 4 && opp_min >= 4 )
           || self_min <= 2 )
         && wm.self().pos().x < ball_get_pos.x
         && wm.self().pos().x < wm.ball().pos().x - 0.2
         && ( std::fabs( wm.ball().pos().y - wm.self().pos().y ) < 1.5
              || wm.ball().pos().absY() < wm.self().pos().absY()
              )
         )
    {
        agent->debugClient().addMessage( "Intercept(2)" );
        dlog.addText( Logger::TEAM,
                      __FILE__": Intercept (2)" );

        Body_Intercept2008().execute( agent );
        agent->setNeckAction( new Neck_TurnToBall() );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_BlockSideAttack::doBasicMove( PlayerAgent * agent )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": doBasicMove" );

    const WorldModel & wm = agent->world();

    // intercept

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( self_min <= 3
         || ( self_min < 20
              && ( self_min < mate_min
                   || ( self_min < mate_min + 3 && mate_min > 3 ) )
              && self_min <= opp_min + 1 )
         || ( self_min < mate_min
              && opp_min >= 2
              && self_min <= opp_min + 1 )
         )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": doBasicMove() intercept" );
        Body_Intercept2008().execute( agent );
        agent->setNeckAction( new Neck_TurnToBallOrScan() );
        return;
    }

    Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    Vector2D target_point = getBasicMoveTarget( agent );

    // decide dash power
    double dash_power = Strategy::get_defender_dash_power( wm, target_point );
    // decide threshold
    double dist_thr = std::fabs( wm.ball().pos().x - wm.self().pos().x ) * 0.1; //wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    agent->debugClient().addMessage( "BlockSide:Basic%.0f", dash_power );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( ! Body_GoToPoint( target_point, dist_thr, dash_power ).execute( agent ) )
    {
        AngleDeg body_angle = ( wm.ball().pos().y < wm.self().pos().y
                                ? -90.0
                                : 90.0 );
        Body_TurnToAngle( body_angle ).execute( agent );
    }

    if ( wm.ball().distFromSelf() < 20.0
         && ( wm.existKickableOpponent()
              || opp_min <= 3 )
         )
    {
        agent->setNeckAction( new Neck_TurnToBall() );
    }
    else
    {
        agent->setNeckAction( new Neck_TurnToBallOrScan() );
    }
}


/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_BlockSideAttack::getBasicMoveTarget( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    Vector2D target_point = home_pos;

    const int opp_min = wm.interceptTable()->opponentReachCycle();

    const Vector2D opp_trap_pos = ( opp_min < 100
                                    ? wm.ball().inertiaPoint( opp_min )
                                    : wm.ball().inertiaPoint( 20 ) );

    // decide wait position
    // adjust to the defence line

    if ( -36.0 < home_pos.x
         && home_pos.x < 10.0 )
    {
        // make static line
        double tmpx = home_pos.x;
        for ( double x = -10.0; x > -34.0; x -= 5.0 )
        {
            if ( opp_trap_pos.x - 3.3 > x )
            {
                tmpx = x - 3.3;
                break;
            }
        }

        target_point.x = tmpx;
        if ( std::fabs( wm.self().pos().y - opp_trap_pos.y ) > 5.0 )
        {
            target_point.y = target_point.y * 0.8 + opp_trap_pos.y * 0.2;
        }

        agent->debugClient().addMessage( "LineDef%.1f", tmpx );
    }
    else
    {
        agent->debugClient().addMessage( "Home" );
    }

    return target_point;
}
