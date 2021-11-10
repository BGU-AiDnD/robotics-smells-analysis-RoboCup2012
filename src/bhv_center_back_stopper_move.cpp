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

#include "bhv_center_back_stopper_move.h"

#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/bhv_go_to_point_look_ball.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_ball_and_player.h>
#include <rcsc/action/neck_turn_to_player_or_scan.h>

#include "body_intercept2008.h"
#include "bhv_basic_move.h"
#include "bhv_basic_tackle.h"
#include "bhv_danger_area_tackle.h"

#include "bhv_get_ball.h"
#include "neck_check_ball_owner.h"
#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

#include "strategy.h"

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_CenterBackStopperMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": Bhv_CenterBackStopperMove" );

    //
    // tackle
    //
    if ( Bhv_DangerAreaTackle().execute( agent ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": danger area tackle" );
        return true;
    }

    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

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
    // defender move
    //

    agent->debugClient().addMessage( "CB:Stopper:Move" );
    dlog.addText( Logger::ROLE,
                  __FILE__": no gettable point" );
    doNormalMove( agent );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_CenterBackStopperMove::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( self_min <= opp_min
         && self_min <= mate_min
         && ! wm.existKickableTeammate() )
    {
        agent->debugClient().addMessage( "CB:Stopper:Intercept" );
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
Bhv_CenterBackStopperMove::doGetBall( rcsc::PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    int opp_min = wm.interceptTable()->opponentReachCycle();
    const PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();
    Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    if ( wm.existKickableTeammate()
         || ! fastest_opp
         || opp_trap_pos.dist( home_pos ) > 7.0
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

    Rect2D bounding_rect( Vector2D( -60.0, home_pos.y - 6.0 ),
                          Vector2D( home_pos.x + 5.0, home_pos.y + 6.0 ) );
    if ( Bhv_GetBall( bounding_rect ).execute( agent ) )
    {
        agent->debugClient().addMessage( "CB:Stopper:GetBall" );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_CenterBackStopperMove::doMarkMove( rcsc::PlayerAgent * /*agent*/ )
{

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_CenterBackStopperMove::doNormalMove( PlayerAgent * agent )
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
        agent->debugClient().addMessage( "CG:Stopper:BasicMove" );
        Bhv_BasicMove( home_pos ).execute( agent );
        return;
    }

    double dash_power = Strategy::get_defender_dash_power( wm, home_pos );

    double dist_thr = next_self_pos.dist( next_ball_pos ) * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().addMessage( "CG:Stopper:Normal" );
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
Bhv_CenterBackStopperMove::doGoToPoint( PlayerAgent * agent,
                                        const Vector2D & target_point,
                                        const double & dist_thr,
                                        const double & dash_power,
                                        const double & dir_thr )
{
    if ( Body_GoToPoint( target_point,
                         dist_thr,
                         dash_power,
                         1, // cycle
                         false, // no back
                         true, // stamina save
                         dir_thr
                         ).execute( agent ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": doGoToPoint GoTo" );
        return;
    }

    const WorldModel & wm = agent->world();

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
                  __FILE__": (doGoToPoint) turnToAngle=%.1f",
                  target_angle.degree() );
}
