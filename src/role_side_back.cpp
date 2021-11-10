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

#include "role_side_back.h"

#include "strategy.h"

#include "bhv_cross.h"
#include "bhv_basic_defensive_kick.h"
#include "bhv_basic_offensive_kick.h"
#include "bhv_keep_shoot_chance.h"

#include "bhv_basic_tackle.h"

#include "bhv_basic_move.h"
#include "bhv_defender_basic_block_move.h"
#include "bhv_danger_area_tackle.h"
#include "bhv_block_dribble.h"
#include "bhv_block_side_attack.h"

#include "body_advance_ball_test.h"
#include "body_dribble2008.h"
#include "body_intercept2008.h"
#include "body_hold_ball2008.h"
#include "bhv_pass_test.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/bhv_go_to_point_look_ball.h>
#include <rcsc/action/bhv_scan_field.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_player_or_scan.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>

#include <rcsc/formation/formation.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/line_2d.h>
#include <rcsc/geom/ray_2d.h>
#include <rcsc/soccer_math.h>
#include <rcsc/math_util.h>

#include "bhv_side_back_cross_block_move.h"
#include "bhv_side_back_danger_move.h"
#include "bhv_side_back_defensive_move.h"
#include "bhv_side_back_stopper_move.h"
#include "bhv_mark_pass_line.h"

#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

#define USE_BHV_SIDE_BACK_CROSS_BLOCK_MOVE
#define USE_BHV_SIDE_BACK_DANGER_MOVE
#define USE_BHV_SIDE_BACK_DEFENSIVE_MOVE
#define USE_BHV_SIDE_BACK_STOPPER_MOVE

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleSideBack::execute( rcsc::PlayerAgent * agent )
{
    //////////////////////////////////////////////////////////////
    // play_on play
    bool kickable = agent->world().self().isKickable();
    if ( agent->world().existKickableTeammate()
         && agent->world().teammatesFromBall().front()->distFromBall()
         < agent->world().ball().distFromSelf() )
    {
        kickable = false;
    }

    if ( kickable )
    {
        // kick
        doKick( agent );
    }
    else
    {
        // positioning
        doMove( agent );
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleSideBack::doKick( rcsc::PlayerAgent* agent )
{
    const double nearest_opp_dist
        = agent->world().getDistOpponentNearestToSelf( 5 );

    switch ( Strategy::get_ball_area(agent->world().ball().pos()) ) {
    case Strategy::BA_CrossBlock:
        if ( nearest_opp_dist < 3.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": doKick() danger" );
            if ( ! rcsc::Bhv_PassTest().execute( agent ) )
            {
                Body_AdvanceBallTest().execute( agent );

                if ( agent->effector().queuedNextBallKickable() )
                {
                    agent->setNeckAction( new rcsc::Neck_ScanField() );
                }
                else
                {
                    agent->setNeckAction( new rcsc::Neck_TurnToBall() );
                }
            }
        }
        else
        {
            Bhv_BasicDefensiveKick().execute( agent );
        }
        break;
    case Strategy::BA_Stopper:
    case Strategy::BA_Danger:
        if ( nearest_opp_dist < 3.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": doKick(). very danger" );
            if ( ! rcsc::Bhv_PassTest().execute( agent ) )
            {
                Body_AdvanceBallTest().execute( agent );

                if ( agent->effector().queuedNextBallKickable() )
                {
                    agent->setNeckAction( new rcsc::Neck_ScanField() );
                }
                else
                {
                    agent->setNeckAction( new rcsc::Neck_TurnToBall() );
                }
            }
        }
        else
        {
            Bhv_BasicDefensiveKick().execute( agent );
        }
        break;
    case Strategy::BA_DribbleBlock:
    case Strategy::BA_DefMidField:
        Bhv_BasicDefensiveKick().execute( agent );
        break;
    case Strategy::BA_DribbleAttack:
    case Strategy::BA_OffMidField:
        Bhv_BasicOffensiveKick().execute( agent );
        break;
    case Strategy::BA_Cross:
        Bhv_Cross().execute( agent );
        break;
    case Strategy::BA_ShootChance:
        Bhv_KeepShootChance().execute( agent );
        break;
    default:
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": unknown ball area" );
        rcsc::Body_HoldBall2008().execute( agent );
        break;
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleSideBack::doMove( rcsc::PlayerAgent * agent )
{
    rcsc::Vector2D home_pos = Strategy::i().getPosition( agent->world().self().unum() );
    if ( ! home_pos.isValid() ) home_pos.assign( 0.0, 0.0 );

    switch ( Strategy::get_ball_area( agent->world() ) ) {
    case Strategy::BA_CrossBlock:
#ifdef USE_BHV_SIDE_BACK_CROSS_BLOCK_MOVE
        Bhv_SideBackCrossBlockMove().execute( agent );
#else
        doCrossBlockAreaMove( agent, home_pos );
#endif
        break;
    case Strategy::BA_Stopper:
#ifdef USE_BHV_SIDE_BACK_STOPPER_MOVE
        Bhv_SideBackStopperMove().execute( agent );
#else
        Bhv_DefenderBasicBlockMove( home_pos ).execute( agent );
        //doBasicMove( agent, home_pos );
#endif
        break;
    case Strategy::BA_Danger:
        doDangerAreaMove( agent, home_pos );
        break;
    case Strategy::BA_DribbleBlock:
        doDribbleBlockAreaMove( agent, home_pos );
        break;
    case Strategy::BA_DefMidField:
        doDefMidAreaMove( agent, home_pos );
        break;
    case Strategy::BA_DribbleAttack:
    case Strategy::BA_OffMidField:
    case Strategy::BA_Cross:
    case Strategy::BA_ShootChance:
        doBasicMove( agent, home_pos );
        break;
    default:
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": unknown ball area" );
        Bhv_BasicMove( home_pos ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
        break;
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleSideBack::doBasicMove( rcsc::PlayerAgent * agent,
                           const rcsc::Vector2D & home_pos )
{
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doBasicMove" );

    const rcsc::WorldModel & wm = agent->world();

    //-----------------------------------------------
    // tackle
    if ( Bhv_BasicTackle( 0.85, 60.0 ).execute( agent ) )
    {
        return;
    }

    //--------------------------------------------------
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
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doBasicMove() intercept" );
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
                                  ( new Neck_TurnToBallOrScan() ) );
        }
#endif
        return;
    }

    /*--------------------------------------------------------*/

    if ( doBlockPassLine( agent, home_pos ) )
    {
        return;
    }

    // decide move target point
    rcsc::Vector2D target_point = getBasicMoveTarget( agent, home_pos );

    // decide dash power
    double dash_power = Strategy::get_defender_dash_power( wm, target_point );
    // decide threshold
    double dist_thr = std::fabs( wm.ball().pos().x - wm.self().pos().x ) * 0.1; //wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    agent->debugClient().addMessage( "%.0f", dash_power );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( ! rcsc::Body_GoToPoint( target_point, dist_thr, dash_power ).execute( agent ) )
    {
        rcsc::AngleDeg body_angle = ( wm.ball().pos().y < wm.self().pos().y
                                      ? -90.0
                                      : 90.0 );
        rcsc::Body_TurnToAngle( body_angle ).execute( agent );
    }

    if ( wm.ball().distFromSelf() < 20.0
         && ( wm.existKickableOpponent()
              || opp_min <= 3 )
         )
    {
        agent->setNeckAction( new rcsc::Neck_TurnToBall() );
    }
    else
    {
        agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
rcsc::Vector2D
RoleSideBack::getBasicMoveTarget( rcsc::PlayerAgent * agent,
                                  const rcsc::Vector2D & home_pos )
{
    const rcsc::WorldModel & wm = agent->world();

    rcsc::Vector2D target_point = home_pos;

    const int opp_min = wm.interceptTable()->opponentReachCycle();

    const rcsc::Vector2D opp_trap_pos = ( opp_min < 100
                                          ? wm.ball().inertiaPoint( opp_min )
                                          : wm.ball().inertiaPoint( 20 ) );

    // decide wait position
    // adjust to the defence line

    if ( wm.ball().pos().x < 10.0
         && -36.0 < home_pos.x
         && home_pos.x < 10.0
         && wm.self().pos().x > home_pos.x
         && wm.ball().pos().x > wm.self().pos().x + 3.0 )
    {
        // make static line
        double tmpx = home_pos.x;
        for ( double x = -12.0; x > -27.0; x -= 8.0 )
        {
            if ( opp_trap_pos.x > x )
            {
                tmpx = x - 3.3;
                break;
            }
        }

        if ( std::fabs( wm.self().pos().y - opp_trap_pos.y ) > 5.0 )
        {
            tmpx -= 3.0;
        }
        target_point.x = tmpx;

        agent->debugClient().addMessage( "LineDef%.1f", tmpx );
    }
    else
    {
        agent->debugClient().addMessage( "SB:Normal" );
    }

#if 0
    const bool is_left_side = Strategy::i().getPositionType( wm.self().unum() ) == Position_Left;
    const rcsc::PlayerPtrCont::const_iterator end = wm.opponentsFromSelf().end();

    double first_x = 100.0;

    for ( rcsc::PlayerPtrCont::const_iterator it = wm.opponentsFromSelf().begin();
          it != end;
          ++it )
    {
        if ( (*it)->pos().x < first_x ) first_x = (*it)->pos().x;
    }

    double max_y = 1.0;

    for ( rcsc::PlayerPtrCont::const_iterator it = wm.opponentsFromSelf().begin();
          it != end;
          ++it )
    {
        if ( is_left_side && (*it)->pos().y > 0.0 ) continue;
        if ( ! is_left_side && (*it)->pos().y < 0.0 ) continue;

        //if ( (*it)->pos().x > target_point.x + 15.0 ) continue;
        if ( (*it)->pos().x > first_x + 15.0 ) continue;

        if ( (*it)->pos().absY() > max_y )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": getBasicMoveTarget. updated max y. opp=%d(%.1f %.1f)",
                          (*it)->unum(),
                          (*it)->pos().x, (*it)->pos().y );
            max_y = (*it)->pos().absY();
        }
    }

    if ( max_y > 0.0
         && max_y < target_point.absY() )
    {
        target_point.y = ( is_left_side
                           ? -max_y + 0.5
                           : max_y - 0.5 );
        dlog.addText( Logger::ROLE,
                      __FILE__": getBasicMoveTarget. shrink formation y. %.1f",
                      target_point.y );
        agent->debugClient().addMessage( "ShrinkY" );
    }
#endif

    return target_point;
}


/*-------------------------------------------------------------------*/
/*!

*/
bool
RoleSideBack::doBlockPassLine( rcsc::PlayerAgent * agent,
                               const rcsc::Vector2D & home_pos )
{
#ifdef USE_BHV_MARK_PASS_LINE
    (void)home_pos;
    return Bhv_MarkPassLine().execute( agent );
#else
    const rcsc::WorldModel & wm = agent->world();

    if ( wm.ball().pos().x < 12.0 )
    {
        return false;
    }

    //if ( wm.ball().pos().x < home_pos.x + 15.0 )
    if ( wm.ball().pos().x < 10.0 )
    {
        return false;
    }

    rcsc::Vector2D target_point = home_pos;


    const rcsc::PlayerObject * nearest_attacker
        = static_cast< const rcsc::PlayerObject * >( 0 );
    double min_dist2 = 100000.0;

    const rcsc::PlayerObject * outside_attacker
        = static_cast< const rcsc::PlayerObject * >( 0 );

    const bool is_left_side = Strategy::i().getPositionType( wm.self().unum() ) == Position_Left;

    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doBlockPassLine() side = %s",
                        is_left_side ? "left" : "right" );


    const rcsc::PlayerPtrCont::const_iterator end = wm.opponentsFromSelf().end();
    for ( rcsc::PlayerPtrCont::const_iterator it = wm.opponentsFromSelf().begin();
          it != end;
          ++it )
    {
        bool same_side = false;
        if ( is_left_side && (*it)->pos().y < 0.0 ) same_side = true;
        if ( ! is_left_side && (*it)->pos().y > 0.0 ) same_side = true;

        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doBlockPassLine. check player %d (%.1f %.1f)",
                            (*it)->unum(),
                            (*it)->pos().x, (*it)->pos().y );

        if ( same_side
             && (*it)->pos().x < wm.defenseLineX() + 20.0
             && (*it)->pos().absY() > home_pos.absY() - 5.0 )
        {
            outside_attacker = *it;
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": doBlockPassLine. found outside attacker %d (%.1f %.1f)",
                                outside_attacker->unum(),
                                outside_attacker->pos().x, outside_attacker->pos().y );
        }

        if ( (*it)->pos().x > wm.defenseLineX() + 10.0 )
        {
            if ( (*it)->pos().x > home_pos.x ) continue;
        }
        if ( (*it)->pos().x > home_pos.x + 10.0 ) continue;
        if ( (*it)->pos().x < home_pos.x - 20.0 ) continue;
        if ( std::fabs( (*it)->pos().y - home_pos.y ) > 20.0 ) continue;

        double d2 = (*it)->pos().dist( home_pos );
        if ( d2 < min_dist2 )
        {
            min_dist2 = d2;
            nearest_attacker = *it;
        }
    }

    bool block_pass = false;

    if ( nearest_attacker )
    {
        block_pass = true;
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doBlockPassLine. exist nearest attacker %d (%.1f %.1f)",
                            nearest_attacker->unum(),
                            nearest_attacker->pos().x, nearest_attacker->pos().y );
    }

    if ( outside_attacker
         && nearest_attacker != outside_attacker )
    {
        block_pass = false;
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doBlockPassLine. exist outside attacker %d (%.1f %.1f)",
                            outside_attacker->unum(),
                            outside_attacker->pos().x, outside_attacker->pos().y );
        return false;
    }

    if ( ! block_pass )
    {
        return false;
    }


    if ( nearest_attacker->isGhost()
         && wm.ball().distFromSelf() > 30.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doBlockPassLine. ghost opponent. scan field" );
        agent->debugClient().addMessage( "ScanOpp" );
        rcsc::Bhv_ScanField().execute( agent );
        return true;
    }


    if ( home_pos.x - nearest_attacker->pos().x > 5.0 )
    {
#if 0
        rcsc::Line2D block_line( nearest_attacker->pos(), wm.ball().pos() );
        target_point.x = nearest_attacker->pos().x + 5.0;
        target_point.y = block_line.getY( target_point.x );
        target_point.y += ( home_pos.y - nearest_attacker->pos().y ) * 0.5;
#else
        target_point
            = nearest_attacker->pos()
            + ( wm.ball().pos() - nearest_attacker->pos() ).setLengthVector( 1.0 );
#endif
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doBlockPassLine.  block pass line front" );
        agent->debugClient().addMessage( "BlockPassFront" );
    }
    else if ( wm.ball().pos().x - nearest_attacker->pos().x > 15.0 )
    {
        target_point
            = nearest_attacker->pos()
            + ( wm.ball().pos() - nearest_attacker->pos() ).setLengthVector( 1.0 );
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doBlockPassLine.  block pass line" );
        agent->debugClient().addMessage( "BlockPass" );
    }
    else
    {
        const rcsc::Vector2D goal_pos( -50.0, 0.0 );

        target_point
            = nearest_attacker->pos()
            + ( goal_pos - nearest_attacker->pos() ).setLengthVector( 1.0 );
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doBlockPassLine.  block through pass" );
        agent->debugClient().addMessage( "BlockThrough" );
    }


    double dash_power = Strategy::get_defender_dash_power( wm, target_point );
    double dist_thr = nearest_attacker->distFromSelf() * 0.1;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    agent->debugClient().addMessage( "%.0f", dash_power );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( ! rcsc::Body_GoToPoint( target_point, dist_thr, dash_power ).execute( agent ) )
    {
        rcsc::AngleDeg body_angle = ( wm.ball().pos().y < wm.self().pos().y
                                      ? -90.0
                                      : 90.0 );
        rcsc::Body_TurnToAngle( body_angle ).execute( agent );
    }

    if ( wm.ball().distFromSelf() < 20.0 )
    {
        agent->setNeckAction( new rcsc::Neck_TurnToBall() );
    }
    else if ( nearest_attacker->posCount() >= 5 )
    {
        agent->setNeckAction( new rcsc::Neck_TurnToPlayerOrScan( nearest_attacker, 5 ) );
    }
    else
    {
        agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
    }

    return true;
#endif
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleSideBack::doCrossBlockAreaMove( rcsc::PlayerAgent * agent,
                                    const rcsc::Vector2D & home_pos )
{
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doCrossBlockAreaMove()" );

    //-----------------------------------------------
    // tackle
    if ( Bhv_DangerAreaTackle( 0.8 ).execute( agent ) )
    {
        return;
    }

    const rcsc::WorldModel & wm = agent->world();

    //--------------------------------------------------
    // intercept
    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( self_min < mate_min && self_min < opp_min
         || self_min <= 2 && wm.ball().pos().absY() < 20.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doCrossBlockAreaMove() intercept" );
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
                                  ( new Neck_TurnToBallOrScan() ) );
        }
#endif
        return;
    }

    //-----------------------------------------
    // set move point

    rcsc::Vector2D center( -41.5, 0.0 );

    int min_cyc = std::min( mate_min, opp_min );

    rcsc::Vector2D ball_future
        = rcsc::inertia_n_step_point( wm.ball().pos(),
                                      wm.ball().vel(),
                                      min_cyc,
                                      rcsc::ServerParam::i().ballDecay() );

    if ( ball_future.y * home_pos.y < 0.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doCrossBlockAreaMove() opposite side. basic move" );
        doBasicMove( agent, home_pos );
        return;
    }

    if ( ball_future.x < -45.0 )
    {
        center.x = ball_future.x + 1.0;
        center.x = std::max( -48.0, center.x );
    }

    rcsc::Line2D block_line( ball_future, center );

    rcsc::Vector2D block_point = home_pos;
#if 0
#if 1
    block_point.y = ball_future.absY() - 8.0;
    block_point.y = std::max( block_point.y, 16.5 );
#elif 0
    block_point.y = ball_future.absY() - 7.0;
    block_point.y = std::max( block_point.y, 18.0 );
    block_point.y = std::min( block_point.y, home_pos.absY() );
    block_point.y = std::min( ball_future.absY() - 2.0, block_point.y );
#elif 0
    block_point.y = ball_future.absY() - 5.0;
    block_point.y = std::max( block_point.y, 20.0 );
    block_point.y = std::min( ball_future.absY() - 2.0, block_point.y );
#else
    block_point.y = ball_future.absY() - 10.0;
    block_point.y = std::max( block_point.y, 15.0 );
#endif
    if ( ball_future.y < 0.0 ) block_point.y *= -1.0;

    block_point.x = block_line.getX( block_point.y );

#if 1
    if ( block_point.x < wm.self().pos().x - 3.0 )
    {
        rcsc::Ray2D body_ray( wm.self().pos(), wm.self().body() );
        rcsc::Vector2D intersect = body_ray.intersection( block_line );
        if ( intersect.valid()
             && 18.0 < intersect.absY()
             && intersect.absY() < ball_future.absY() )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__":  doCrossBlockAreaMove()"
                                " ball is back. changed to intersect. (%.1f %.1f)->(%.1f %.1f)",
                                block_point.x, block_point.y,
                                intersect.x, intersect.y );
            block_point = intersect;
        }
    }
#endif
#endif

    //------------------------------------------
    // set dash power
    double dash_power = rcsc::ServerParam::i().maxPower();
    if ( wm.self().pos().x < block_point.x + 5.0 )
    {
        dash_power -= wm.ball().distFromSelf() * 2.0;
        dash_power = std::max( 20.0, dash_power );
    }

    double dist_thr = wm.ball().distFromSelf() * 0.07;
    if ( dist_thr < 0.8 ) dist_thr = 0.8;

    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__":  doCrossBlockAreaMove() go to (%.1f, %.1f) dash_powe=%.1f",
                        block_point.x, block_point.y, dash_power );
    agent->debugClient().addMessage( "CrossBlock%.0f", dash_power );
    agent->debugClient().setTarget( block_point );
    agent->debugClient().addCircle( block_point, dist_thr );

    if ( ! rcsc::Body_GoToPoint( block_point,
                                 dist_thr,
                                 dash_power
                                 ).execute( agent )
         )
    {
        rcsc::AngleDeg body_angle = ( wm.ball().angleFromSelf().abs() < 80.0
                                      ? 0.0
                                      : 180.0 );
        rcsc::Body_TurnToAngle( body_angle ).execute( agent );
    }

    agent->setNeckAction( new rcsc::Neck_TurnToBall() );
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleSideBack::doDangerAreaMove( rcsc::PlayerAgent * agent,
                                const rcsc::Vector2D & home_pos )
{
#ifdef USE_BHV_SIDE_BACK_DANGER_MOVE
    (void)home_pos;
    Bhv_SideBackDangerMove().execute( agent );
#else
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doDangerAreaMove()" );

    //-----------------------------------------------
    // tackle
    if ( Bhv_DangerAreaTackle().execute( agent ) )
    {
        return;
    }

    const rcsc::WorldModel & wm = agent->world();

    const bool ball_is_same_side = ( wm.ball().pos().y * home_pos.y > 0.0 );

    //--------------------------------------------------
    // intercept
    if ( ! wm.existKickableOpponent()
         && ! wm.existKickableTeammate()
         && wm.ball().vel().x < 0.0 )
    {
        rcsc::Line2D ball_line( wm.ball().pos(),
                                wm.ball().vel().th() );
        double goal_line_y = ball_line.getY( rcsc::ServerParam::i().pitchHalfLength() );
        if ( std::fabs( goal_line_y ) < rcsc::ServerParam::i().goalHalfWidth() + 1.5 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": doDangerAreaMove() very very danger. try intercept" );

            if ( rcsc::Body_Intercept2008().execute( agent ) )
            {
                agent->debugClient().addMessage( "DangerGetBall(0)" );
#ifdef NO_USE_NECK_DEFAULT_INTERCEPT_NECK
                agent->setNeckAction( new rcsc::Neck_TurnToBall() );
#else
                agent->setNeckAction( new Neck_DefaultInterceptNeck
                                      ( new rcsc::Neck_TurnToBall() ) );
#endif
                return;
            }
        }
    }

    //--------------------------------------------------
    // intercept
    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( ball_is_same_side
         && ! wm.existKickableTeammate()
         && self_min <= 3 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doDangerAreaMove() ball is same side. try get" );
        if ( rcsc::Body_Intercept2008().execute( agent ) )
        {
            agent->debugClient().addMessage( "DangerGetBall(1)" );
#ifdef NO_USE_NECK_DEFAULT_INTERCEPT_NECK
            agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
#else
            agent->setNeckAction( new Neck_DefaultInterceptNeck
                                  ( new rcsc::Neck_TurnToBallOrScan() ) );
#endif
            return;
        }
    }

    //--------------------------------------------------
    // attack opponent
    const rcsc::PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();

    if ( opp_min <= 1
         && fastest_opp
         && wm.self().pos().x < wm.ball().pos().x + 1.0
         && wm.self().pos().absY() < wm.ball().pos().absY()
         && wm.ball().distFromSelf() < 3.0 )
    {
        rcsc::Vector2D attack_pos = fastest_opp->pos() + fastest_opp->vel();

        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doDangerAreaMove() exist dangerous opp. attack." );
        agent->debugClient().addMessage( "DangetAttack" );
        agent->debugClient().setTarget( attack_pos );

        if ( ! rcsc::Body_GoToPoint( attack_pos,
                                     0.1,
                                     rcsc::ServerParam::i().maxPower()
                                     ).execute( agent )
             )
        {
            rcsc::Body_TurnToPoint( attack_pos ).execute( agent );
        }
        agent->setNeckAction( new rcsc::Neck_TurnToBall() );
        return;
    }

    //--------------------------------------------------
    const double nearest_opp_dist = wm.getDistOpponentNearestToSelf( 10 );

    if ( nearest_opp_dist > 3.0
         && ! wm.existKickableOpponent()
         && ! wm.existKickableTeammate()
         && ( self_min == 1
              || ( self_min < opp_min
                   && self_min < mate_min ) )
         )
        //&& wm.interceptTable()->isSelfFastestPlayer() )
    {
        rcsc::Body_Intercept2008().execute( agent );
        agent->debugClient().addMessage( "DangerGetBall(2)" );
#ifdef NO_USE_NECK_DEFAULT_INTERCEPT_NECK
        agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
#else
        agent->setNeckAction( new Neck_DefaultInterceptNeck
                              ( new rcsc::Neck_TurnToBallOrScan() ) );
#endif
        return;
    }

    //--------------------------------------------------
    if ( ball_is_same_side
         && wm.ball().pos().x < -44.0
         && wm.ball().pos().absY() < 18.0
         && ! wm.existKickableTeammate() )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doDangerAreaMove() ball is same side. and very danger. try intercept" );
        if ( rcsc::Body_Intercept2008().execute( agent ) )
        {
            agent->debugClient().addMessage( "DangerGetBall(3)" );
#ifdef NO_USE_NECK_DEFAULT_INTERCEPT_NECK
            agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
#else
            agent->setNeckAction( new Neck_DefaultInterceptNeck
                                  ( new rcsc::Neck_TurnToBallOrScan() ) );
#endif
            return;
        }
    }

    //--------------------------------------------------
    bool pole_block = true;
    if ( opp_min <= 10 )
    {
        rcsc::Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );
        if ( opp_trap_pos.x > -40.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": doDangerAreaMove() no pole block" );
            pole_block = false;
        }
    }

    //--------------------------------------------------
    if ( ! pole_block
         || ( ball_is_same_side
              && wm.ball().pos().absY() > rcsc::ServerParam::i().goalHalfWidth() )
         )
    {
        double dash_power
            = wm.self().getSafetyDashPower( rcsc::ServerParam::i().maxPower() );
        // I am behind of ball
        if ( wm.self().pos().x < wm.ball().pos().x )
        {
            dash_power *= std::min( 1.0, 7.0 / wm.ball().distFromSelf() );
            dash_power = std::max( 30.0, dash_power );
        }
        double dist_thr = wm.ball().distFromSelf() * 0.07;
        if ( dist_thr < 0.5 ) dist_thr = 0.5;

        agent->debugClient().addMessage( "DangerGoHome%.0f", dash_power );
        agent->debugClient().setTarget( home_pos );
        agent->debugClient().addCircle( home_pos, dist_thr );
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doDangerAreaMove. go to home" );

        if ( ! rcsc::Body_GoToPoint( home_pos,
                                     dist_thr,
                                     dash_power
                                     ).execute( agent )
             )
        {
            rcsc::Body_TurnToBall().execute( agent );
        }
        agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
        return;
    }

    //--------------------------------------------------
    // block opposite side goal
    rcsc::Vector2D goal_post( - rcsc::ServerParam::i().pitchHalfLength(),
                              rcsc::ServerParam::i().goalHalfWidth() - 0.8 );
    if ( home_pos.y < 0.0 )
    {
        goal_post.y *= -1.0;
    }

    rcsc::Line2D block_line( wm.ball().pos(), goal_post );
    rcsc::Vector2D block_point( -48.0, 0.0 );
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doDangerAreaMove. block goal post. original_y = %.1f",
                        block_line.getY( block_point.x ) );
    block_point.y = rcsc::min_max( home_pos.y - 1.0,
                                   block_line.getY( block_point.x ),
                                   home_pos.y + 1.0 );

    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doDangerAreaMove. block goal" );
    agent->debugClient().addMessage( "DangerBlockPole" );
    agent->debugClient().setTarget( block_point );
    agent->debugClient().addCircle( block_point, 1.0 );

    double dash_power
        = wm.self().getSafetyDashPower( rcsc::ServerParam::i().maxPower() );
#if 1
    if ( wm.self().pos().x < 47.0
         && wm.self().stamina() > rcsc::ServerParam::i().staminaMax() * 0.7 )
    {
        if ( wm.self().pos().dist( block_point ) > 0.8 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": doDangerAreaMove block goal with back move" );
            rcsc::Bhv_GoToPointLookBall( block_point,
                                         1.0,
                                         dash_power
                                         ).execute( agent );
        }
        else
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": doDangerAreaMove. already block goal(1)" );
            rcsc::Vector2D face_point( wm.self().pos().x, 0.0 );
            rcsc::Body_TurnToPoint( face_point ).execute( agent );
            agent->setNeckAction( new rcsc::Neck_TurnToBall() );
        }
        return;
    }
#endif
    if ( ! rcsc::Body_GoToPoint( block_point,
                                 1.0,
                                 dash_power,
                                 100, // cycle
                                 false, // no back mode
                                 true, // save recovery
                                 15.0 // dir thr
                                 ).execute( agent ) )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doDangerAreaMove. already block goal(2)" );
        rcsc::Vector2D face_point( wm.self().pos().x, 0.0 );
        if ( wm.self().pos().y * wm.ball().pos().y < 0.0 )
        {
            face_point.assign( -52.5, 0.0 );
        }

        rcsc::Body_TurnToPoint( face_point ).execute( agent );
    }
    agent->setNeckAction( new rcsc::Neck_TurnToBall() );
#endif
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleSideBack::doDribbleBlockAreaMove( rcsc::PlayerAgent * agent,
                                      const rcsc::Vector2D & home_pos )
{
#ifdef USE_BHV_SIDE_BACK_DEFENSIVE_MOVE
    (void)home_pos;
    Bhv_SideBackDefensiveMove().execute( agent );
#else
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doDribbleBlockAreaMove." );
    // same side
    if ( agent->world().ball().pos().y * home_pos.y > 0.0
         && agent->world().self().stamina() > rcsc::ServerParam::i().staminaMax() * 0.6 )
    {
        doBlockSideAttacker( agent, home_pos );
    }
    else
    {
        doBasicMove( agent, home_pos );
    }
#endif
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleSideBack::doDefMidAreaMove( rcsc::PlayerAgent * agent,
                                const rcsc::Vector2D & home_pos )
{
#ifdef USE_BHV_SIDE_BACK_DEFENSIVE_MOVE
    (void)home_pos;
    Bhv_SideBackDefensiveMove().execute( agent );
#else
    if ( agent->world().ball().pos().x < agent->world().self().pos().x + 15.0
         && agent->world().ball().pos().y * home_pos.y > 0.0 // same side
         && agent->world().ball().pos().absY() > 7.0
         && agent->world().self().stamina() > rcsc::ServerParam::i().staminaMax() * 0.6
         )
    {
        doBlockSideAttacker( agent, home_pos );
    }
    else
    {
        doBasicMove( agent, home_pos );
    }
#endif
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleSideBack::doBlockSideAttacker( rcsc::PlayerAgent * agent,
                                   const rcsc::Vector2D & home_pos )
{
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doBlockSideAttacker()" );

    const rcsc::WorldModel & wm = agent->world();

    //-----------------------------------------------
    // tackle
    if ( Bhv_BasicTackle( 0.85, 60.0 ).execute( agent ) )
    {
        return;
    }

    //--------------------------------------------------
    // intercept

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( self_min < opp_min && self_min < mate_min )
    {
        agent->debugClient().addMessage( "BlockGetBall(1)" );
        rcsc::Body_Intercept2008().execute( agent );
#ifdef NO_USE_NECK_DEFAULT_INTERCEPT_NECK
        agent->setNeckAction( new rcsc::Neck_TurnToBall() );
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
        return;
    }

    rcsc::Vector2D ball_get_pos = wm.ball().inertiaPoint( self_min );
    rcsc::Vector2D opp_reach_pos = wm.ball().inertiaPoint( opp_min );

    if ( ( ( self_min <= 4 && opp_min >= 4 )
           || self_min <= 2 )
         && wm.self().pos().x < ball_get_pos.x
         && wm.self().pos().x < wm.ball().pos().x - 0.2
         && ( std::fabs( wm.ball().pos().y - wm.self().pos().y ) < 1.5
              || wm.ball().pos().absY() < wm.self().pos().absY()
              )
         )
    {
        agent->debugClient().addMessage( "BlockGetBall(2)" );
        rcsc::Body_Intercept2008().execute( agent );
#ifdef NO_USE_NECK_DEFAULT_INTERCEPT_NECK
        agent->setNeckAction( new rcsc::Neck_TurnToBall() );
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
        return;
    }

    if ( wm.self().pos().x < wm.ball().pos().x - 5.0
         || ( home_pos.x < wm.ball().pos().x - 5.0
              && wm.self().pos().x < home_pos.x )
         )
    {
        agent->debugClient().addMessage( "BlockToBasic" );
        doBasicMove( agent, home_pos );
        return;
    }

    //--------------------------------------------------
    // block move
    if ( opp_min < mate_min - 1
         && opp_reach_pos.absY() > 20.0
         && opp_reach_pos.y * home_pos.y > 0.0 )
    {
        if ( Bhv_BlockDribble().execute( agent ) )
        {
            return;
        }
    }

    rcsc::Vector2D block_point = home_pos;

    if ( wm.ball().pos().x < wm.self().pos().x )
    {
        agent->debugClient().addMessage( "SideBlockStatic" );

        block_point.x = -42.0;
        block_point.y = wm.self().pos().y;
        if ( wm.ball().pos().absY() < wm.self().pos().absY() - 0.5 )
        {
            agent->debugClient().addMessage( "Correct" );
            block_point.y = 10.0;
            if ( home_pos.y < 0.0 ) block_point.y *= -1.0;
        }
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doBlockSideAttacker() static move" );
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
        }
    }

    //-----------------------------------------
    // set dash power
    double dash_power = rcsc::ServerParam::i().maxPower();
    if ( wm.self().pos().x < wm.ball().pos().x - 8.0 )
    {
        dash_power = wm.self().playerType().staminaIncMax() * 0.8;
    }
    else if ( wm.self().pos().x < wm.ball().pos().x
              && wm.self().pos().absY() < wm.ball().pos().absY() - 1.0
              && wm.self().stamina() < rcsc::ServerParam::i().staminaMax() * 0.65 )
    {
        dash_power = rcsc::ServerParam::i().maxPower() * 0.7;
    }

    agent->debugClient().addMessage( "BlockSideAttack%.0f", dash_power );

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 0.8 ) dist_thr = 0.8;

    agent->debugClient().setTarget( block_point );
    agent->debugClient().addCircle( block_point, dist_thr );

    if ( ! rcsc::Body_GoToPoint( block_point, dist_thr, dash_power ).execute( agent ) )
    {
        rcsc::Vector2D face_point( -48.0, 0.0 );
        rcsc::Body_TurnToPoint( face_point ).execute( agent );
    }

    agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
}
