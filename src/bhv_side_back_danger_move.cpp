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

#include "bhv_side_back_danger_move.h"

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
Bhv_SideBackDangerMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": Bhv_SideBackDangerMove" );

    //
    // tackle
    //
    if ( Bhv_DangerAreaTackle().execute( agent ) )
    {
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
    // block shoot
    //
    if ( doBlockShoot( agent ) )
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
    // block goal
    //
    if ( doBlockGoal( agent ) )
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
Bhv_SideBackDangerMove::doIntercept( PlayerAgent * agent )
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
         && self_min <= mate_min + 1
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
Bhv_SideBackDangerMove::doBlockShoot( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const double dist_thr = 0.6;

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();
    Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    if ( self_min < opp_min
         || mate_min < opp_min )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doBlockShoot) no shoot block situation"
                      " self_min < opp_min or mate_min < opp_min" );
        return false;
    }

    if ( opp_trap_pos.absY() > 15.0
         || ( position_type == Position_Left
              && opp_trap_pos.y > -4.0 )
         || ( position_type == Position_Right
              && opp_trap_pos.y < 4.0 )
         )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doBlockShoot) no shoot block situation  opp_trap=(%.1f %.1f)",
                      opp_trap_pos.x, opp_trap_pos.y );
        return false;
    }

    Vector2D goal_edge( - ServerParam::i().pitchHalfLength(),
                        - ServerParam::i().goalHalfWidth() + 1.0 );
    if ( position_type == Position_Right )
    {
        goal_edge.y *= -1.0;
    }

    agent->debugClient().addLine( opp_trap_pos, goal_edge );

    //
    // deciede the best point
    //

    Vector2D best_point = goal_edge;
    int best_cycle = 1000;

    const Vector2D unit_vec = ( goal_edge - opp_trap_pos ).setLengthVector( 1.0 );
    double shoot_len = opp_trap_pos.dist( goal_edge );

    dlog.addText( Logger::BLOCK,
                  __FILE__": (doBlockShoot) shoot_len=%.2f",
                  shoot_len );
    {
        Line2D shoot_line( opp_trap_pos, goal_edge );
        Vector2D self_end_pos = wm.self().inertiaFinalPoint();
        bool self_on_shoot_line = ( shoot_line.dist( self_end_pos ) < 0.8
                                    && self_end_pos.x < opp_trap_pos.x );
        if ( self_on_shoot_line )
        {
            best_point = shoot_line.projection( self_end_pos );
            shoot_len = std::max( 1.0, opp_trap_pos.dist( best_point ) - 1.0 );
            dlog.addText( Logger::BLOCK,
                          __FILE__": (doBlockShoot) self on shoot line. shoot_len=%.2f",
                          shoot_len );
//             dlog.addText( Logger::BLOCK,
//                           __FILE__": (doBlockShoot) self on shoot line. cancel",
//                           shoot_len );
//             return false;
        }
    }

    const double dist_step = std::max( 1.0, shoot_len / 12.0 );
    for ( double d = 0.5; d <= shoot_len; d += dist_step )
    {
        Vector2D target_pos = opp_trap_pos + unit_vec * d;
        double stamina = 0.0;
        int self_step = DefenseSystem::predict_self_reach_cycle( wm, target_pos,
                                                                 dist_thr,
                                                                 true, &stamina );
        if ( self_step < best_cycle )
        {
            best_point = target_pos;
            best_cycle = self_step;
        }
    }

    //
    // perform the action
    //

    agent->debugClient().setTarget( best_point );
    agent->debugClient().addCircle( best_point, dist_thr );
    dlog.addText( Logger::BLOCK,
                  __FILE__": (doBlockShoot) block_point=(%.1f %.1f) cycle=%d",
                  best_point.x, best_point.y,
                  best_cycle );
    double dash_power = wm.self().getSafetyDashPower( ServerParam::i().maxPower() );

    if ( Body_GoToPoint( best_point,
                         dist_thr,
                         dash_power,
                         1, // cycle
                         false, // no back mode
                         true, // save recovery
                         15.0 // dir thr
                         ).execute( agent ) )
    {
        agent->debugClient().addMessage( "SB:Danger:BlockShootGo" );
    }
    else
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockShoot) turn." );
        agent->debugClient().addMessage( "SB:Danger:BlockShootTurn" );
        Body_TurnToPoint( opp_trap_pos ).execute( agent );
    }
    agent->setNeckAction( new Neck_CheckBallOwner() );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackDangerMove::doGetBall( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int second_mate_min = wm.interceptTable()->secondTeammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();
    Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    if ( wm.existKickableTeammate()
         || mate_min <= opp_min - 1
         || second_mate_min < self_min
         || opp_trap_pos.x > -36.0
         || opp_trap_pos.dist( home_pos ) > 7.0
         )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) no get ball situation" );
        return false;
    }


    double min_y = home_pos.y - 4.0;
    double max_y = home_pos.y + 4.0;

    Rect2D bounding_rect( Vector2D( -60.0, min_y ),
                          Vector2D( home_pos.x + 4.0, max_y ) );
    if ( Bhv_GetBall( bounding_rect ).execute( agent ) )
    {
        agent->debugClient().addMessage( "SB:Danger:GetBall" );
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) performed" );
        return true;
    }
    else
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) could not find the position" );
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackDangerMove::doBlockGoal( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();
    Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

    if ( opp_min > 10
         && opp_trap_pos.x > -40.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockGoal) no dangerous opponent" );
        return false;
    }

    if ( mate_min <= opp_min - 3
         || ( wm.existKickableTeammate()
              && opp_min >= 2 )
         )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockGoal) our ball" );
        return false;
    }

    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    if ( wm.ball().pos().absY() > ServerParam::i().goalHalfWidth()
         && ( position_type == Position_Left && wm.ball().pos().y > 0.0
              || position_type == Position_Right && wm.ball().pos().y < 0.0 )
         )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockGoal) no block goal situation" );
        return false;
    }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    //--------------------------------------------------
    // block opposite side goal
    Vector2D goal_post( - ServerParam::i().pitchHalfLength(),
                        ServerParam::i().goalHalfWidth() - 0.8 );
    if ( position_type == Position_Left )
    {
        goal_post.y *= -1.0;
    }

    Line2D block_line( wm.ball().pos(), goal_post );
    Vector2D block_point( -48.0, 0.0 );
    dlog.addText( Logger::ROLE,
                  __FILE__": (doBlockGoal) block goal post. original_y = %.1f",
                  block_line.getY( block_point.x ) );
    block_point.y = bound( home_pos.y - 1.0,
                           block_line.getY( block_point.x ),
                           home_pos.y + 1.0 );

    dlog.addText( Logger::ROLE,
                  __FILE__": (doBlockGoal) block goal (%.1f %.1f)",
                  block_point.x, block_point.y );
    agent->debugClient().setTarget( block_point );
    agent->debugClient().addCircle( block_point, 1.0 );

    double dash_power = wm.self().getSafetyDashPower( ServerParam::i().maxPower() );

    Vector2D face_point( wm.self().pos().x, 0.0 );
    if ( wm.self().pos().y * wm.ball().pos().y < 0.0 )
    {
        face_point.assign( -52.5, 0.0 );
    }

//     if ( wm.self().pos().x < 47.0
//          && wm.self().stamina() > ServerParam::i().staminaMax() * 0.7 )
//     {
//         if ( wm.self().pos().dist( block_point ) > 0.8 )
//         {
//             dlog.addText( Logger::ROLE,
//                           __FILE__": doDangerAreaMove block goal with back move" );
//             agent->debugClient().addMessage( "SB:Danger:BlogkGoal:LookGo%.0f",
//                                              dash_power );
//             Bhv_GoToPointLookBall( block_point,
//                                    1.0,
//                                    dash_power
//                                    ).execute( agent );
//         }
//         else
//         {
//             dlog.addText( Logger::ROLE,
//                           __FILE__": doDangerAreaMove. already block goal. turn to (%.1f %.1f)",
//                           face_point.x, face_point.y );
//             agent->debugClient().addMessage( "SB:Danger:BlogkGoal:TurnTo(1)" );
//             Body_TurnToPoint( face_point ).execute( agent );
//             //agent->setNeckAction( new Neck_TurnToBall() );
//             agent->setNeckAction( new Neck_CheckBallOwner() );
//         }
//         return true;
//     }

    if ( Body_GoToPoint( block_point,
                         1.0,
                         dash_power,
                         1, // cycle
                         false, // no back mode
                         true, // save recovery
                         15.0 // dir thr
                         ).execute( agent ) )
    {
        agent->debugClient().addMessage( "SB:Danger:BlockGoal:Go%.0f",
                                         dash_power );
    }
    else
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": doDangerAreaMove. already block goal(2)" );
        agent->debugClient().addMessage( "SB:Danger:BlockGoal:TurnTo(2)" );
        Body_TurnToPoint( face_point ).execute( agent );
    }
    //agent->setNeckAction( new Neck_TurnToBall() );
    agent->setNeckAction( new Neck_CheckBallOwner() );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SideBackDangerMove::doNormalMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    Vector2D next_self_pos = wm.self().pos() + wm.self().vel();
    Vector2D trap_pos = wm.ball().inertiaPoint( std::min( wm.interceptTable()->teammateReachCycle(),
                                                          wm.interceptTable()->opponentReachCycle() ) );

    double dash_power = wm.self().getSafetyDashPower( ServerParam::i().maxPower() );
    if ( next_self_pos.x < trap_pos.x )
    {
        // behind of trap point
        dash_power *= std::min( 1.0, 7.0 / wm.ball().distFromSelf() );
        dash_power = std::max( 30.0, dash_power );
    }

    double dist_thr = wm.ball().distFromSelf() * 0.07;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().setTarget( home_pos );
    agent->debugClient().addCircle( home_pos, dist_thr );
    dlog.addText( Logger::ROLE,
                  __FILE__": doDangerAreaMove. go to home (%.1f %.1f)  dist_thr=%.3f  dash_power=%.1f",
                  home_pos.x, home_pos.y,
                  dist_thr,
                  dash_power );

    if ( Body_GoToPoint( home_pos, dist_thr, dash_power,
                         1,
                         false,
                         true,
                         12.0
                         ).execute( agent )  )
    {
        agent->debugClient().addMessage( "SB:Danger:GoHome%.0f", dash_power );
    }
    else
    {
        Body_TurnToBall().execute( agent );
        agent->debugClient().addMessage( "SB:Danger:TurnToBall" );
    }
    //agent->setNeckAction( new Neck_TurnToBallOrScan() );
    agent->setNeckAction( new Neck_CheckBallOwner() );
}
