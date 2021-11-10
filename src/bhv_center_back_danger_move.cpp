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

#include "bhv_center_back_danger_move.h"

#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/bhv_go_to_point_look_ball.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_player_or_scan.h>
#include <rcsc/action/neck_turn_to_ball_and_player.h>

#include "bhv_basic_move.h"
#include "bhv_basic_tackle.h"
#include "bhv_danger_area_tackle.h"
#include "bhv_get_ball.h"
#include "body_intercept2008.h"
#include "neck_check_ball_owner.h"
#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

#include "strategy.h"

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_CenterBackDangerMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_CenterBackDangerMove" );

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
    // mark
    //
    if ( doMarkMove( agent ) )
    {
        return true;
    }

    //
    // block center
    //
//     if ( doBlockCenter( agent ) )
//     {
//         return true;
//     }

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
Bhv_CenterBackDangerMove::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( self_min <= opp_min
         && self_min <= mate_min
         && ! wm.existKickableTeammate() )
    {
        agent->debugClient().addMessage( "CB:Danger:Intercept" );
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
Bhv_CenterBackDangerMove::doGetBall( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    int opp_min = wm.interceptTable()->opponentReachCycle();
    const PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();
    Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    if ( wm.existKickableTeammate()
         || ! fastest_opp
         || opp_trap_pos.x > -36.0
         || opp_trap_pos.dist( home_pos ) > 7.0
         || opp_trap_pos.absY() > 9.0
         )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) no get ball situation" );
        return false;
    }

    if ( ( position_type == Position_Left
           && home_pos.y + 3.0 < opp_trap_pos.y )
         || ( position_type == Position_Right
              && opp_trap_pos.y < home_pos.y - 3.0 )
         )
    {
        bool exist_blocker = false;

        const double my_dist = wm.self().pos().dist( opp_trap_pos );
        const PlayerPtrCont::const_iterator end = wm.teammatesFromBall().end();
        for ( PlayerPtrCont::const_iterator p = wm.teammatesFromBall().begin();
              p != end;
              ++p )
        {
            if ( (*p)->goalie() ) continue;
            if ( (*p)->isGhost() ) continue;
            if ( (*p)->posCount() >= 10 ) continue;
            if ( (*p)->pos().x > fastest_opp->pos().x + 2.0 ) continue;
            if ( (*p)->pos().dist( opp_trap_pos ) > my_dist + 1.0 ) continue;

            dlog.addText( Logger::ROLE,
                          __FILE__": (doGetBall) exist other blocker %d (%.1f %.1f)",
                          (*p)->unum(),
                          (*p)->pos().x, (*p)->pos().y );
            exist_blocker = true;
            break;
        }

        if ( exist_blocker )
        {
            return false;
        }
    }

    double max_x = -36.0; // home_pos.x + 4.0;
    Rect2D bounding_rect( Vector2D( -60.0, home_pos.y - 4.0 ),
                          Vector2D( max_x, home_pos.y + 4.0 ) );
    if ( Bhv_GetBall( bounding_rect ).execute( agent ) )
    {
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_CenterBackDangerMove::doMarkMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    int opp_min = wm.interceptTable()->opponentReachCycle();
    const PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();
    Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

    //
    // search mark target opponent
    //
    const PlayerObject * mark_target = wm.getOpponentNearestTo( home_pos, 1, NULL );

    if ( ! mark_target
         || mark_target->pos().x > -39.0
         || mark_target->pos().dist( home_pos ) > 3.0
         //|| mark_target->distFromSelf() > 7.0
         || mark_target->distFromBall() < mark_target->playerTypePtr()->kickableArea() + 0.5
         )
    {
        // not found
        dlog.addText( Logger::ROLE,
                      __FILE__": (doMarkMove) mark not found" );
        return false;
    }

    //
    // check teammate marker
    //
    double marker_dist = 100.0;
    const PlayerObject * marker = wm.getTeammateNearestTo( mark_target->pos(),
                                                           30,
                                                           &marker_dist );
    if ( marker
         && marker->pos().x < mark_target->pos().x + 1.0
         && marker_dist < mark_target->distFromSelf() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doMarkMove) exist other marker" );
        return false;
    }

    //
    // set target point
    //
    Vector2D mark_point = mark_target->pos();
    mark_point += mark_target->vel();
    mark_point.x -= 0.9;
    mark_point.y += ( mark_target->pos().y > wm.ball().pos().y
                       ? -0.6
                       : 0.6 );

    dlog.addText( Logger::ROLE,
                  __FILE__": (doMarkMove). mark point=(%.1f %.1f)",
                  mark_point.x, mark_point.y );

    if ( mark_point.x > wm.ball().pos().x + 5.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doMarkMove) (mark_point.x - ball.x) X diff is big" );
        return false;
    }

    double dash_power = ServerParam::i().maxPower();
    double x_diff = mark_point.x - wm.self().pos().x;

    if ( x_diff > 20.0 )
    {
        dash_power = wm.self().playerType().staminaIncMax() * wm.self().recovery();
        dlog.addText( Logger::ROLE,
                      __FILE__": (doMarkMove) power change(1) X diff=%.2f dash_power=%.1f",
                      x_diff, dash_power );
    }
    else if ( x_diff > 10.0 )
    {
        dash_power *= 0.7;
        dlog.addText( Logger::ROLE,
                      __FILE__": (doMarkMove) power change(2) X diff=%.2f dash_power=%.1f",
                      x_diff, dash_power );
    }
    else if ( wm.ball().pos().dist( mark_point ) > 20.0 )
    {
        dash_power *= 0.6;
        dlog.addText( Logger::ROLE,
                      __FILE__": (doMarkMove) mark point is far. dash power=%.1f",
                      dash_power );
    }

    double dist_thr = wm.ball().distFromSelf() * 0.05;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().setTarget( mark_point );
    agent->debugClient().addCircle( mark_point, dist_thr );

    //
    // turn
    //
    if ( wm.self().pos().x < mark_point.x
         && wm.self().pos().dist( mark_point ) < dist_thr )
    {
        // TODO: check mark target only with turn_neck (if possible)
        if ( mark_target
             && mark_target->unum() == Unum_Unknown )
        {
            Vector2D target_pos = mark_target->pos() + mark_target->vel();
            dlog.addText( Logger::ROLE,
                          __FILE__": (doMarkMove) already there. check mark target" );
            agent->debugClient().addMessage( "CB:Danger:MarkCheck" );
            agent->debugClient().setTarget( target_pos );

            Bhv_NeckBodyToPoint( target_pos, 10.0 ).execute( agent );
            return true;
        }

        AngleDeg body_angle = ( wm.ball().pos().x < wm.self().pos().x - 5.0
                                ? 0.0
                                : 180.0 );
        dlog.addText( Logger::ROLE,
                      __FILE__": (doMarkMove) already there. turn to angle %.1f",
                      body_angle.degree() );
        agent->debugClient().addMessage( "CB:Danger:MarkTurn%.0f", body_angle.degree() );

        Body_TurnToAngle( body_angle ).execute( agent );

        if ( mark_target
             && ( mark_target->unum() == Unum_Unknown
                  || mark_target->posCount() >= 1 )
             // TODO: check if player can look the mark target
             )
        {
            agent->setNeckAction( new Neck_TurnToPlayerOrScan( mark_target, -1 ) );
        }
        else
        {
            agent->setNeckAction( new Neck_CheckBallOwner() );
        }
        return true;
    }

    //
    // forward dash
    //
    if ( wm.self().pos().dist( mark_point ) > 3.0 )
    {
        agent->debugClient().addMessage( "CB:Danger:MarkF-Dash" );
        agent->debugClient().setTarget( mark_point );
        agent->debugClient().addCircle( mark_point, dist_thr );

        dlog.addText( Logger::ROLE,
                      __FILE__": (doMarkMove) forward move" );

        doGoToPoint( agent, mark_point, dist_thr, dash_power, 15.0 );

        if ( mark_target
             && mark_target->unum() == Unum_Unknown )
        {
            agent->setNeckAction( new Neck_TurnToPlayerOrScan( mark_target, -1 ) );
        }
        else if ( wm.existKickableOpponent()
                  || wm.ball().distFromSelf() > 15.0 )
        {
            if ( fastest_opp && opp_min <= 1 )
            {
                agent->setNeckAction( new Neck_TurnToBallAndPlayer( fastest_opp ) );
            }
            else
            {
                //agent->setNeckAction( new Neck_TurnToBall() );
                agent->setNeckAction( new Neck_CheckBallOwner() );
            }
        }
        else
        {
            //agent->setNeckAction( new Neck_TurnToBallOrScan() );
            agent->setNeckAction( new Neck_CheckBallOwner() );
        }
        return true;
    }

    //
    // go to & look ball
    //

    dlog.addText( Logger::ROLE,
                  __FILE__": (doMarkMove) ball looking move" );

    Bhv_GoToPointLookBall( mark_point,
                           dist_thr,
                           dash_power
                           ).execute( agent );

    if ( ! fastest_opp
         || fastest_opp->distFromBall() > ( fastest_opp->playerTypePtr()->kickableArea()
                                            + wm.ball().distFromSelf() * 0.05
                                            + 0.2 ) )
    {
        if ( mark_target
             && mark_target->unum() == Unum_Unknown )
        {
            agent->setNeckAction( new Neck_TurnToPlayerOrScan( mark_target, -1 ) );
        }
        else
        {
            //agent->setNeckAction( new Neck_TurnToBallOrScan() );
            agent->setNeckAction( new Neck_CheckBallOwner() );
        }
    }

    return true;
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_CenterBackDangerMove::doBlockCenter( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( wm.ball().pos().x > -42.0
         && wm.ball().pos().absY() < 5.0
         && wm.ball().pos().x < wm.self().pos().x + 1.0
         && ( wm.ball().vel().x < -0.7
              || wm.existKickableOpponent()
              || opp_min <= 1 )
         )
    {
        Vector2D target_point( -47.0, 0.0 );
        Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );
        double y_sign = ( opp_trap_pos.y > 0.0 ? 1.0 : -1.0 );
        target_point.y = y_sign * 1.5;
        if ( opp_trap_pos.absY() > 5.0 ) target_point.y = y_sign * 5.0;
        if ( opp_trap_pos.absY() > 3.0 ) target_point.y = y_sign * 3.0;
        if ( position_type == Position_Left && target_point.y > 0.0 ) target_point.y = 0.0;
        if ( position_type == Position_Right && target_point.y < 0.0 ) target_point.y = 0.0;

        // decide dash power
        double dash_power = Strategy::get_defender_dash_power( wm, target_point );

        double dist_thr = wm.ball().pos().dist( target_point ) * 0.1;
        if ( dist_thr < 0.5 ) dist_thr = 0.5;

        dlog.addText( Logger::ROLE,
                      __FILE__": correct target point to block center" );
        agent->debugClient().addMessage( "CB:Danger:BlockCenter%.0f", dash_power );
        agent->debugClient().setTarget( target_point );
        agent->debugClient().addCircle( target_point, dist_thr );

        doGoToPoint( agent, target_point, dist_thr, dash_power, 15.0 );

        agent->setNeckAction( new Neck_CheckBallOwner() );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_CenterBackDangerMove::doNormalMove( PlayerAgent * agent )
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
        dlog.addText( Logger::TEAM,
                      __FILE__": (doNormalMove) ball is front and our team keep ball" );
        Bhv_BasicMove( home_pos ).execute( agent );
        return;
    }

    double dash_power = Strategy::get_defender_dash_power( wm, home_pos );

    double dist_thr = next_self_pos.dist( next_ball_pos ) * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().addMessage( "CG:Danger:Normal" );
    agent->debugClient().setTarget( home_pos );
    agent->debugClient().addCircle( home_pos, dist_thr );
    dlog.addText( rcsc::Logger::TEAM,
                  __FILE__": go home (%.1f %.1f) dist_thr=%.2f power=%.1f",
                  home_pos.x, home_pos.y,
                  dist_thr,
                  dash_power );

    doGoToPoint( agent, home_pos, dist_thr, dash_power, 12.0 );

    //agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
    agent->setNeckAction( new Neck_CheckBallOwner() );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_CenterBackDangerMove::doGoToPoint( PlayerAgent * agent,
                                       const Vector2D & target_point,
                                       const double & dist_thr,
                                       const double & dash_power,
                                       const double & dir_thr )
{
    const WorldModel & wm = agent->world();

    if ( Body_GoToPoint( target_point, dist_thr, dash_power,
                         1, // 1 step
                         false, // no back dash
                         true, // save recovery
                         dir_thr
                         ).execute( agent ) )
    {
        agent->debugClient().addMessage( "Go%.1f", dash_power );
        dlog.addText( Logger::ROLE,
                      __FILE__": GoToPoint (%.1f %.1f) dash_power=%.1f dist_thr=%.2f",
                      target_point.x, target_point.y,
                      dash_power,
                      dist_thr );
        return;
    }

    // already there

    int min_step = std::min( wm.interceptTable()->teammateReachCycle(),
                             wm.interceptTable()->opponentReachCycle() );

    Vector2D ball_pos = wm.ball().inertiaPoint( min_step );
    Vector2D self_pos = wm.self().inertiaFinalPoint();
    AngleDeg ball_angle = ( ball_pos - self_pos ).th();

    AngleDeg target_angle = ball_angle + 90.0;
    if ( ball_pos.x < -47.0 )
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

    Body_TurnToAngle( target_angle ).execute( agent );

    agent->debugClient().addMessage( "TurnTo%.0f",
                                     target_angle.degree() );
    dlog.addText( Logger::ROLE,
                  __FILE__": TurnToAngle %.1f",
                  target_angle.degree() );
}
