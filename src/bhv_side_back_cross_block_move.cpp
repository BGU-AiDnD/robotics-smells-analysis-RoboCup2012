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

#include "bhv_side_back_cross_block_move.h"

#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include "body_intercept2008.h"

#include "bhv_danger_area_tackle.h"
#include "bhv_get_ball.h"
#include "neck_check_ball_owner.h"
#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

#include "defense_system.h"
#include "strategy.h"

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackCrossBlockMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": Bhv_SideBackCrossBlockMove" );

    //
    // tackle
    //
    if ( Bhv_DangerAreaTackle( 0.8 ).execute( agent ) )
    {
        agent->debugClient().addMessage( "SB:CrossBlock:Tackle" );
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
    // emergency move
    //
    if ( doEmergencyMove( agent ) )
    {
        return true;
    }

    //
    // block cross
    //
    if ( doBlockCrossLine( agent ) )
    {
        return true;
    }

    doNormalMove( agent );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackCrossBlockMove::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    const PlayerObject * teammate = wm.interceptTable()->fastestTeammate();
    if ( teammate
         && teammate->goalie() )
    {
        mate_min = wm.interceptTable()->secondTeammateReachCycle();
    }

    if ( self_min <= opp_min
         && self_min <= mate_min
         && ! wm.existKickableTeammate() )
    {
        agent->debugClient().addMessage( "SB:CrossBlock:Intercept" );
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
Bhv_SideBackCrossBlockMove::doGetBall( rcsc::PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    int opp_min = wm.interceptTable()->opponentReachCycle();
    Vector2D trap_pos = wm.ball().inertiaPoint( opp_min );

    Vector2D next_self_pos = wm.self().pos() + wm.self().vel();

    if ( std::fabs( trap_pos.y - home_pos.y ) < 5.0
         && trap_pos.absY() < next_self_pos.absY()
         && ( ( position_type == Position_Left
                && trap_pos.y < 0.0
                && next_self_pos.x < trap_pos.x )
              || ( position_type == Position_Right
                   && trap_pos.y > 0.0
          && next_self_pos.x < trap_pos.x )
              )
         )
    {
        Rect2D bounding_rect( Vector2D( -50.0, home_pos.y - 5.0 ),
                              Vector2D( home_pos.x + 4.0, home_pos.y + 5.0 ) );
        if ( Bhv_GetBall( bounding_rect ).execute( agent ) )
        {
            agent->debugClient().addMessage( "SB:CrossBlock:GetBall" );
            dlog.addText( Logger::ROLE,
                          __FILE__": (doGetBall) done Bhv_GetBall" );
            return true;
        }
        else
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (doGetBall) could not find the position." );
        }
    }
    else
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) no get ball situation" );
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackCrossBlockMove::doEmergencyMove( rcsc::PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();
    const PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();

    if ( mate_min + 4 <= opp_min )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doEmergencyMove) ball owner is teammate" );
        return false;
    }

    if ( ! fastest_opp )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doEmergencyMove) no opponent" );
        return false;
    }

    const Vector2D next_opp_pos = fastest_opp->pos() + fastest_opp->vel();
    const Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

    const Vector2D next_self_pos = wm.self().pos() + wm.self().vel();
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    double stamina = 0.0;
    int self_step = DefenseSystem::predict_self_reach_cycle( wm, home_pos, 1.0, true, &stamina );

    dlog.addText( Logger::ROLE,
                  __FILE__": (doEmergencyMove) home=(%.1f %.1f) next_self=(%.1f %.1f) step=%d  stamina=%.1f",
                  home_pos.x, home_pos.y,
                  next_self_pos.x, next_self_pos.y,
                  self_step, stamina );
    dlog.addText( Logger::ROLE,
                  __FILE__": (doEmergencyMove) opp_step=%d opp_trap_pos=(%.1f %.1f) next_opp=(%.1f %.1f)",
                  opp_min,
                  opp_trap_pos.x, opp_trap_pos.y,
                  next_opp_pos.x, next_opp_pos.y );

    if ( opp_trap_pos.x < next_self_pos.x
         //&& home_pos.x < next_self_pos.x
         && opp_min < self_step
         && next_opp_pos.x < next_self_pos.x + 1.0
         && ( opp_trap_pos.absY() < 7.0
              || ( position_type == Position_Left && opp_trap_pos.y < 0.0 )
              || ( position_type == Position_Right && opp_trap_pos.y > 0.0 )
              )
         )
    {
        Vector2D target_point( -48.0, 7.0 );

        if ( opp_trap_pos.absY() > 23.0 )
        {
            target_point.y = 20.0;
        }
        else if ( opp_trap_pos.absY() > 16.0 )
        {
            target_point.y = 14.0;
        }
        else if ( opp_trap_pos.absY() < 7.0 )
        {
            target_point.y = 4.0;
        }

        if ( position_type == Position_Left )
        {
            target_point.y *= -1.0;
        }

        double dash_power = wm.self().getSafetyDashPower( ServerParam::i().maxPower() );
        double dist_thr = std::fabs( wm.ball().pos().x - wm.self().pos().x ) * 0.1;
        if ( dist_thr < 0.5 ) dist_thr = 0.5;

        agent->debugClient().addMessage( "SB:CrossBlock:Emergency" );
        agent->debugClient().setTarget( target_point );
        agent->debugClient().addCircle( target_point, dist_thr );

        doGoToPoint( agent, target_point, dist_thr, dash_power, 18.0 );
        agent->setNeckAction( new Neck_CheckBallOwner() );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackCrossBlockMove::doBlockCrossLine( rcsc::PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    int opp_min = wm.interceptTable()->opponentReachCycle();
    Vector2D trap_pos = wm.ball().inertiaPoint( opp_min );

    if ( ( position_type == Position_Left
           && trap_pos.y > 0.0 )
         || ( position_type == Position_Right
              && trap_pos.y < 0.0 )
         )
    {
        return false;
    }

    Vector2D block_point = home_pos;

    double dash_power = ServerParam::i().maxPower();
    if ( wm.self().pos().x < block_point.x + 5.0 )
    {
        dash_power -= wm.ball().distFromSelf() * 2.0;
        dash_power = std::max( 20.0, dash_power );
    }

    double dist_thr = wm.ball().distFromSelf() * 0.07;
    if ( dist_thr < 0.8 ) dist_thr = 0.8;

    dlog.addText( Logger::ROLE,
                  __FILE__": cross block. go to (%.1f, %.1f) dash_powe=%.1f",
                  block_point.x, block_point.y, dash_power );

    agent->debugClient().setTarget( block_point );
    agent->debugClient().addCircle( block_point, dist_thr );

    if ( Body_GoToPoint( block_point,
                         dist_thr,
                         dash_power,
                         1, // cycle
                         false, // no back dash
                         true, // save recovery
                         15.0 // dir threshold
                         ).execute( agent )
         )
    {
        agent->debugClient().addMessage( "SB:CrossBlock:Go%.0f",
                                         dash_power );
    }
    else
    {
        AngleDeg body_angle = ( wm.ball().angleFromSelf().abs() < 70.0
                                ? 0.0
                                : 180.0 );
        if ( trap_pos.x < - 47.0 )
        {
            body_angle = 0.0;
        }
        Body_TurnToAngle( body_angle ).execute( agent );
        agent->debugClient().addMessage( "SB:CrossBlock:TurnTo%.0f",
                                         body_angle.degree() );
    }

    agent->setNeckAction( new Neck_CheckBallOwner() );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SideBackCrossBlockMove::doNormalMove( PlayerAgent * agent )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": doNormalMove" );

    const WorldModel & wm = agent->world();

    Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );

    double dash_power = Strategy::get_defender_dash_power( wm, target_point );
    double dist_thr = std::fabs( wm.ball().pos().x - wm.self().pos().x ) * 0.1;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    dlog.addText( Logger::ROLE,
                  __FILE__": doBasicMove() target_point=(%.1f %.1f) dist_thr=%.2f",
                  target_point.x, target_point.y,
                  dist_thr );

    if ( Body_GoToPoint( target_point, dist_thr, dash_power,
                         1, // cycle
                         false, // no back dash
                         true, // save recovery
                         15.0 // dir threshold
                         ).execute( agent ) )
    {
        agent->debugClient().addMessage( "SB:CrossBlock:Normal:Go%.0f",
                                         dash_power );
    }
    else
    {
        AngleDeg body_angle = ( wm.ball().pos().y < wm.self().pos().y
                                ? -90.0
                                : 90.0 );
        agent->debugClient().addMessage( "SB:CrossBlock:Normal:TurnTo%.0f",
                                         body_angle.degree() );
        Body_TurnToAngle( body_angle ).execute( agent );
    }

    if ( wm.ball().distFromSelf() < 20.0
         && wm.interceptTable()->opponentReachCycle() <= 3 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": doBasicMove() look ball" );
        agent->setNeckAction( new Neck_TurnToBall() );
    }
    else
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": doBasicMove() look ball or scan" );
        agent->setNeckAction( new Neck_TurnToBallOrScan() );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SideBackCrossBlockMove::doGoToPoint( rcsc::PlayerAgent * agent,
                                         const rcsc::Vector2D & target_point,
                                         const double & dist_thr,
                                         const double & dash_power,
                                         const double & dir_thr )
{
    if ( Body_GoToPoint( target_point, dist_thr, dash_power,
                         1, false, true, dir_thr
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
    // turn to somewhere

    const WorldModel & wm = agent->world();

    Vector2D ball_next = wm.ball().pos() + wm.ball().vel();
    Vector2D my_final = wm.self().inertiaFinalPoint();
    AngleDeg ball_angle = ( ball_next - my_final ).th();

    AngleDeg body_angle = ( ball_angle.abs() < 70.0
                            ? 0.0
                            : 180.0 );
    if ( ball_next.x < - 47.0 )
    {
        body_angle = 0.0;
    }

    Body_TurnToAngle( body_angle ).execute( agent );

    agent->debugClient().addMessage( "TurnTo%.0f",
                                     body_angle.degree() );
    dlog.addText( Logger::ROLE,
                  __FILE__": TurnToAngle %.1f",
                  body_angle.degree() );
}
