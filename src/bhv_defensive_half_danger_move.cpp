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

#include "bhv_defensive_half_danger_move.h"

#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include "body_intercept2008.h"

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
Bhv_DefensiveHalfDangerMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": Bhv_DefensiveHalfDangerMove" );
    //
    // tackle
    //
    if ( Bhv_DangerAreaTackle().execute( agent ) )
    {
        agent->debugClient().addMessage( "CB:Danger:Tackle" );
        dlog.addText( Logger::ROLE,
                      __FILE__":  tackle" );
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

    //
    // normal move
    //
    doNormalMove( agent );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_DefensiveHalfDangerMove::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( self_min <= opp_min + 1
         && self_min <= mate_min + 1
         && ! wm.existKickableTeammate() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doIntercept) performed" );
        agent->debugClient().addMessage( "DH:Danger:Intercept" );
        Body_Intercept2008().execute( agent );
        if ( opp_min >= self_min + 3 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (doIntercept) offensive turn_neck" );
            agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        }
        else
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (doIntercept) default turn_neck" );
            agent->setNeckAction( new Neck_DefaultInterceptNeck
                                  ( new Neck_TurnToBallOrScan() ) );
        }
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_DefensiveHalfDangerMove::doGetBall( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    int opp_min = wm.interceptTable()->opponentReachCycle();
    const PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();
    Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    if ( wm.existKickableTeammate()
         || ! fastest_opp
         || opp_trap_pos.x > -30.0
         || opp_trap_pos.dist( home_pos ) > 7.0
         || opp_trap_pos.absY() > 13.0
         )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) no get ball situation" );
        return false;
    }

    //
    // search other blocker
    //
    bool exist_blocker = false;

    const double my_dist = wm.self().pos().dist( opp_trap_pos );
    const PlayerPtrCont::const_iterator end = wm.teammatesFromBall().end();
    for ( PlayerPtrCont::const_iterator p = wm.teammatesFromBall().begin();
          p != end;
          ++p )
    {
        if ( (*p)->goalie() ) continue;
        if ( (*p)->isGhost() ) continue;
        if ( (*p)->posCount() >= 3 ) continue;
        if ( (*p)->pos().x > fastest_opp->pos().x ) continue;
        if ( (*p)->pos().x > opp_trap_pos.x ) continue;
        if ( (*p)->pos().dist( opp_trap_pos ) > my_dist ) continue;

        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) exist other blocker %d (%.1f %.1f)",
                      (*p)->unum(),
                      (*p)->pos().x, (*p)->pos().y );
        exist_blocker = true;
        break;
    }

    //
    // try intercept
    //
    if ( exist_blocker )
    {
        int self_min = wm.interceptTable()->selfReachCycle();
        Vector2D self_trap_pos = wm.ball().inertiaPoint( self_min );
        if ( self_min <= 10
             && self_trap_pos.dist( home_pos ) < 10.0 )
        {
            agent->debugClient().addMessage( "DH:Danger:GetBallIntercept" );
            Body_Intercept2008().execute( agent );
            if ( opp_min >= self_min + 3 )
            {
                dlog.addText( Logger::ROLE,
                              __FILE__": (doIntercept) offensive turn_neck" );
                agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
            }
            else
            {
                dlog.addText( Logger::ROLE,
                              __FILE__": (doIntercept) default turn_neck" );
                agent->setNeckAction( new Neck_DefaultInterceptNeck
                                      ( new Neck_TurnToBallOrScan() ) );
            }
            return true;
        }

        return false;
    }

    //
    // try get ball
    //
    dlog.addText( Logger::ROLE,
                  __FILE__": (doGetBall) try" );
    double max_x = -34.0;
    Rect2D bounding_rect( Vector2D( -60.0, home_pos.y - 6.0 ),
                          Vector2D( max_x, home_pos.y + 6.0 ) );
    if ( Bhv_GetBall( bounding_rect ).execute( agent ) )
    {
        agent->debugClient().addMessage( "DH:Danger:GetBall" );
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) performed" );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_DefensiveHalfDangerMove::doNormalMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );
    target_point.x = Strategy::get_defense_line_x( wm, target_point );

#if 0
    if ( wm.defenseLineX() < target_point.x - 1.0 )
    {
        double new_x = target_point.x * 0.5 + wm.defenseLineX() * 0.5;
        dlog.addText( Logger::ROLE,
                      __FILE__": (doNormalMove) correct target x old_x=%.1f  defense_line=%.1f  new_x=%.1f",
                      target_point.x,
                      wm.defenseLineX(),
                      new_x );
        target_point.x = new_x;
    }
#endif

    //target_point.y = target_point.y * 0.9 + wm.ball().pos().y * 0.1;

    double dash_power = Strategy::get_defender_dash_power( wm, target_point );
    double dist_thr = std::fabs( wm.ball().pos().x - wm.self().pos().x ) * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    doGoToPoint( agent, target_point, dist_thr, dash_power, 12.0 );

    agent->setNeckAction( new Neck_CheckBallOwner() );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_DefensiveHalfDangerMove::doGoToPoint( PlayerAgent * agent,
                                          const Vector2D & target_point,
                                          const double & dist_thr,
                                          const double & dash_power,
                                          const double & dir_thr )
{
    const WorldModel & wm = agent->world();

    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( Body_GoToPoint( target_point, dist_thr, dash_power,
                         1, // 1 step
                         false, // no back dash
                         true, // save recovery
                         dir_thr
                         ).execute( agent ) )
    {
        agent->debugClient().addMessage( "DH:Danger:Go%.1f", dash_power );
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGoToPoint) (%.1f %.1f) dash_power=%.1f dist_thr=%.2f",
                      target_point.x, target_point.y,
                      dash_power,
                      dist_thr );
        return;
    }

    // already there

    Vector2D ball_next = wm.ball().pos() + wm.ball().vel();
    Vector2D my_final = wm.self().inertiaFinalPoint();
    AngleDeg ball_angle = ( ball_next - my_final ).th();

    AngleDeg target_angle;
    if ( ball_next.x < -30.0 )
    {
        target_angle = ball_angle + 90.0;
        if ( ball_next.x < -45.0 )
        {
            if ( target_angle.degree() < 0.0 )
            {
                target_angle += 180.0;
            }
        }
        else
        {
            if ( target_angle.degree() > 0.0 )
            {
                target_angle += 180.0;
            }
        }
    }
    else
    {
        target_angle = ball_angle + 90.0;
        if ( ball_next.x > my_final.x + 15.0 )
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

    agent->debugClient().addMessage( "DH:Danger:TurnTo%.0f",
                                     target_angle.degree() );
    dlog.addText( Logger::ROLE,
                  __FILE__": (doGoToPoint) turn to angle=%.1f",
                  target_angle.degree() );
}
