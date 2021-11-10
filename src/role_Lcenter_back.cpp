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

#include "role_Lcenter_back.h"

#include "strategy.h"

#include "bhv_cross.h"
#include "bhv_basic_defensive_kick.h"
#include "bhv_basic_offensive_kick.h"
#include "bhv_keep_shoot_chance.h"

#include "bhv_basic_tackle.h"

#include "bhv_basic_move.h"
#include "bhv_defender_basic_block_move.h"
#include "bhv_mark_close_opponent.h"
#include "bhv_danger_area_tackle.h"

#include "body_dribble2008.h"
#include "body_intercept2008.h"
#include "body_hold_ball2008.h"
#include "body_advance_ball_test.h"
#include "body_clear_ball2008.h"
#include "bhv_pass_test.h"
#include "bhv_block_dribble.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/bhv_go_to_point_look_ball.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_and_player.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_player_or_scan.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>

#include <rcsc/formation/formation.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

#include "bhv_center_back_danger_move.h"
#include "bhv_center_back_defensive_move.h"
#include "bhv_center_back_stopper_move.h"
#include "bhv_line_defense.h"

#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

#define USE_BHV_CENTER_BACK_DANGER_MODE
#define USE_BHV_CENTER_BACK_DEFENSIVE_MODE
#define USE_BHV_CENTER_BACK_STOPPER_MODE

using namespace rcsc;

#define dash_power 33.0

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleLCenterBack::execute( PlayerAgent * agent )
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
RoleLCenterBack::doKick( PlayerAgent * agent )
{
    const double nearest_opp_dist
        = agent->world().getDistOpponentNearestToSelf( 1 );

    switch ( Strategy::get_ball_area( agent->world().ball().pos()) ) {
    case Strategy::BA_CrossBlock:
        if ( nearest_opp_dist < 3.0 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": cross block area. danger" );
            if ( ! Bhv_PassTest().execute( agent ) )
            {
                Body_AdvanceBallTest().execute( agent );

                if ( agent->effector().queuedNextBallKickable() )
                {
                    agent->setNeckAction( new Neck_ScanField() );
                }
                else
                {
                    agent->setNeckAction( new Neck_TurnToBall() );
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
            dlog.addText( Logger::ROLE,
                          __FILE__": stopper or danger area." );
            if ( ! Bhv_PassTest().execute( agent ) )
            {
                Body_ClearBall2008().execute( agent );

                if ( agent->effector().queuedNextBallKickable() )
                {
                    agent->setNeckAction( new Neck_ScanField() );
                }
                else
                {
                    agent->setNeckAction( new Neck_TurnToBall() );
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
        dlog.addText( Logger::ROLE,
                      __FILE__": unknown ball area" );
        Body_HoldBall2008().execute( agent );
        break;
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleLCenterBack::doMove( PlayerAgent * agent )
{
    rcsc::Vector2D home_pos = Strategy::i().getPosition( agent->world().self().unum() );
    if ( ! home_pos.isValid() ) home_pos.assign( 0.0, 0.0 );

    switch ( Strategy::get_ball_area( agent->world() ) ) {
    case Strategy::BA_Danger:
        doDangerAreaMove( agent, home_pos );
        break;
    case Strategy::BA_CrossBlock:
        doCrossBlockAreaMove( agent, home_pos );
        break;
    case Strategy::BA_Stopper:
        doStopperMove( agent, home_pos );
        break;
    case Strategy::BA_DefMidField:
        doDefMidMove( agent, home_pos );
        break;
    case Strategy::BA_DribbleBlock:
#ifdef USE_BHV_CENTER_BACK_DEFENSIVE_MODE
        Bhv_CenterBackDefensiveMove().execute( agent );
        break;
#endif
    case Strategy::BA_DribbleAttack:
    case Strategy::BA_OffMidField:
    case Strategy::BA_Cross:
    case Strategy::BA_ShootChance:
        agent->debugClient().addMessage( "NormalMove" );
        doBasicMove( agent, home_pos );
        break;
    default:
        dlog.addText( Logger::ROLE,
                      __FILE__": unknown ball pos" );
        Bhv_BasicMove( home_pos ).execute( agent );
        agent->setNeckAction( new Neck_TurnToBallOrScan() );
        break;
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleLCenterBack::doBasicMove( PlayerAgent * agent,
                             const Vector2D & home_pos )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": doBasicMove" );

    const WorldModel & wm = agent->world();

    //-----------------------------------------------
    // tackle
    if ( Bhv_BasicTackle( 0.87, 70.0 ).execute( agent ) )
    {
        return;
    }

    //--------------------------------------------------
    // check intercept chance
    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( ! wm.existKickableTeammate()
         && mate_min > 0 )
    {
        int intercept_state = 0;
        if ( self_min <= 1 && opp_min >= 1 )
        {
            intercept_state = 1;
        }
        else if ( self_min <= 2
                  && opp_min >= 3 )
        {
            intercept_state = 2;
        }
        else if ( self_min <= 3
                  && opp_min >= 4 )
        {
            intercept_state = 3;
        }
        else if ( self_min < 20
                  && self_min < mate_min
                  && ( self_min <= opp_min - 1
                       && opp_min >= 2 ) )
        {
            intercept_state = 4;
        }
        else if ( opp_min >= 2
                  && self_min <= opp_min + 1
                  && self_min <= mate_min )
        {
            intercept_state = 5;
        }

        rcsc::Vector2D intercept_pos = wm.ball().inertiaPoint( self_min );
        if ( self_min < 30
             && wm.self().pos().x < -20.0
             && intercept_pos.x < wm.self().pos().x + 1.0 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": doBasicMove reset intercept state %d",
                          intercept_state );
            intercept_state = 0;

            if ( self_min <= opp_min + 1
                 && opp_min >= 2
                 && self_min <= mate_min )
            {
                intercept_state = 9;
            }
            else if ( self_min <= opp_min - 3
                      && self_min <= mate_min )
            {
                intercept_state = 10;
            }
            else if ( self_min <= opp_min - 3
                      && self_min <= mate_min )
            {
                intercept_state = 12;
            }
            else if ( self_min <= 1 )
            {
                intercept_state = 13;
            }
        }

        if ( intercept_state != 0 )
        {
            // chase ball
            dlog.addText( Logger::ROLE,
                          __FILE__": doBasicMove intercept. state=%d",
                          intercept_state );
            agent->debugClient().addMessage( "CBBasicMove:Intercept%d",
                                             intercept_state );
            Body_Intercept2008().execute( agent );
            const PlayerObject * opp = ( wm.opponentsFromBall().empty()
                                         ? NULL
                                         : wm.opponentsFromBall().front() );
            if ( opp && opp->distFromBall() < 2.0 )
            {
                agent->setNeckAction( new Neck_TurnToBall() );
            }
            else
            {
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
            }
            return;
        }
    }

    /*--------------------------------------------------------*/

    double dist_thr = 0.5;
    Vector2D target_point = getBasicMoveTarget( agent, home_pos, &dist_thr );

    // decide dash power
    //double dash_power = Strategy::get_defender_dash_power( wm, target_point );

    //double dist_thr = wm.ball().pos().dist( target_point ) * 0.1;
    //if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().addMessage( "CB:basic %.0f", dash_power );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    dlog.addText( Logger::ROLE,
                  __FILE__": doBasicMove go to (%.1f %.1f) dist_thr=%.2f power=%.1f",
                  target_point.x, target_point.y,
                  dist_thr,
                  dash_power );

    if ( wm.ball().pos().x < -35.0 )
    {
        if ( wm.self().stamina() > ServerParam::i().staminaMax() * 0.6
             && wm.self().pos().x < target_point.x - 4.0
             && wm.self().pos().dist( target_point ) > dist_thr )
        {
            Bhv_GoToPointLookBall( target_point,
                                   dist_thr,
                                   dash_power
                                   ).execute( agent );
            return;
        }
    }

    doGoToPoint( agent, target_point, dist_thr, dash_power,
                 15.0 ); // dir_thr

    if ( opp_min <= 3
         || wm.ball().distFromSelf() < 10.0 )
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
RoleLCenterBack::getBasicMoveTarget( PlayerAgent * agent,
                                    const Vector2D & home_pos,
                                    double * dist_thr )
{
    const WorldModel & wm = agent->world();
    const bool is_left_side = ( Strategy::i().getPositionType( wm.self().unum() ) == Position_Left );
    const bool is_right_side = ( Strategy::i().getPositionType( wm.self().unum() ) == Position_Right );

    Vector2D target_point = home_pos;

    *dist_thr = wm.ball().pos().dist( target_point ) * 0.1;
    if ( *dist_thr < 0.5 ) *dist_thr = 0.5;

    // get mark target player

    //if ( wm.ball().pos().x > home_pos.x + 15.0 )
    if ( wm.ball().pos().x > 10.0 )
    {
        const PlayerPtrCont::const_iterator end = wm.opponentsFromSelf().end();

        double first_x = 100.0;

        for ( PlayerPtrCont::const_iterator it = wm.opponentsFromSelf().begin();
              it != end;
              ++it )
        {
            if ( (*it)->pos().x < first_x ) first_x = (*it)->pos().x;
        }

        const PlayerObject * nearest_attacker
            = static_cast< const PlayerObject * >( 0 );
        double min_dist2 = 100000.0;

        for ( PlayerPtrCont::const_iterator it = wm.opponentsFromSelf().begin();
              it != end;
              ++it )
        {
            if ( (*it)->pos().x > wm.ourDefenseLineX() + 10.0 ) continue;
            if ( (*it)->pos().x > home_pos.x + 10.0 ) continue;
            if ( (*it)->pos().x < home_pos.x - 10.0 ) continue;
            if ( std::fabs( (*it)->pos().y - home_pos.y ) > 10.0 ) continue;
#if 1
            if ( is_left_side && (*it)->pos().y < home_pos.y - 2.0 ) continue;
            if ( is_right_side && (*it)->pos().y > home_pos.y + 2.0 ) continue;
#endif

            double d2 = (*it)->pos().dist2( home_pos );
            if ( d2 < min_dist2 )
            {
                min_dist2 = d2;
                nearest_attacker = *it;
            }
            break;
        }

        if ( nearest_attacker )
        {
            //const Vector2D goal_pos( -50.0, 0.0 );

            target_point.x = nearest_attacker->pos().x - 2.0;
            if ( target_point.x > home_pos.x )
            {
                target_point.x = home_pos.x;
            }

            target_point.y
                = nearest_attacker->pos().y
                + ( nearest_attacker->pos().y > home_pos.y
                    ? -1.0
                    : 1.0 );

            *dist_thr = std::min( 1.0,
                                  wm.ball().pos().dist( target_point ) * 0.1 );

            dlog.addText( Logger::ROLE,
                          __FILE__":d getBasicMoveTarget."
                          "  block opponent front. dist_thr=%.2f",
                          *dist_thr );
            agent->debugClient().addMessage( "BlockOpp" );
        }
    }


    return target_point;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleLCenterBack::doDefMidMove( PlayerAgent * agent,
                              const Vector2D & home_pos )
{
#ifdef USE_BHV_CENTER_BACK_DEFENSIVE_MODE
    (void)home_pos;
    Bhv_CenterBackDefensiveMove().execute( agent );
#else
    agent->debugClient().addMessage( "DefMidMove" );

    dlog.addText( Logger::ROLE,
                  __FILE__": doDefMidMove" );

    const WorldModel & wm = agent->world();

    //-----------------------------------------------
    // tackle
    if ( Bhv_BasicTackle( 0.85, 50.0 ).execute( agent ) )
    {
        return;
    }

    //--------------------------------------------------
    // check intercept chance
    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

#if 1
    if ( self_min <= mate_min
         && self_min > opp_min )
    {
        //const rcsc::Vector2D self_reach_point = wm.ball().inertiaPoint( self_min );
        const rcsc::Vector2D opp_reach_point = wm.ball().inertiaPoint( opp_min );
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doDefMidMove. try BlockDribble"
                            " home=(%.1f %.1f) opp_reach_pos=(%.1f %.1f)",
                            home_pos.x, home_pos.y,
                            opp_reach_point.x, opp_reach_point.y );

        if ( opp_reach_point.x < home_pos.x + 5.0
             &&  opp_reach_point.dist( home_pos ) < 5.0
             && Bhv_BlockDribble().execute( agent ) )
        {
            agent->debugClient().addMessage( "DefMidBlockDrib" );
            return;
        }
    }
#endif

    const PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();

    bool chase_ball = false;

    //if ( self_min == 1 && opp_min >= 1 ) chase_ball = true;
    if ( self_min == 1 ) chase_ball = true;
    if ( self_min == 2 && opp_min >= 1 ) chase_ball = true;
    if ( self_min >= 3 && opp_min >= self_min - 1 ) chase_ball = true;
    if ( mate_min <= self_min - 2 ) chase_ball = false;
    if ( wm.existKickableTeammate() || mate_min == 0 ) chase_ball = false;

    if ( chase_ball )
    {
        // chase ball
        dlog.addText( Logger::ROLE,
                      __FILE__": doDefMidMove intercept." );
        agent->debugClient().addMessage( "CB:DefMid:Intercept" );

        Body_Intercept2008().execute( agent );

        if ( fastest_opp && opp_min <= 1 )
        {
            agent->setNeckAction( new Neck_TurnToBallAndPlayer( fastest_opp ) );
        }
        else
        {
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
        }
        return;
    }

    const Vector2D opp_trap_pos = ( opp_min < 100
                                    ? wm.ball().inertiaPoint( opp_min )
                                    : wm.ball().inertiaPoint( 20 ) );

    /*--------------------------------------------------------*/
    // ball is in very safety area (very front & our ball)
    // go to home position

    if ( opp_trap_pos.x > home_pos.x + 4.0
         && opp_trap_pos.x > wm.self().pos().x + 5.0
         && ( mate_min <= opp_min - 2 || self_min <= opp_min - 4 ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": doDefMidMove. opp trap pos is front. cycle=%d (%.1f %.1f)",
                      opp_min, opp_trap_pos.x, opp_trap_pos.y );
        agent->debugClient().addMessage( "CB:safety" );
        Bhv_BasicMove( home_pos ).execute( agent );
        return;
    }

    Vector2D target_point = getDefMidMoveTarget( agent, home_pos );

    // decide dash power
    double dash_power = Strategy::get_defender_dash_power( wm, target_point );

    //double dist_thr = wm.ball().distFromSelf() * 0.1;
    //if ( dist_thr < 0.5 ) dist_thr = 0.5;
    double dist_thr = wm.ball().pos().dist( target_point ) * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    dlog.addText( Logger::ROLE,
                  __FILE__": doDefMidMove go to home. power=%.1f",
                  dash_power );
    agent->debugClient().addMessage( "CB:DefMid%.0f", dash_power );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( wm.ball().pos().x < -35.0 )
    {
        if ( wm.self().stamina() > ServerParam::i().staminaMax() * 0.6
             && wm.self().pos().x < target_point.x - 4.0
             && wm.self().pos().dist( target_point ) > dist_thr )
        {
            Bhv_GoToPointLookBall( target_point,
                                   dist_thr,
                                   dash_power
                                   ).execute( agent );
            return;
        }
    }

    doGoToPoint( agent, target_point, dist_thr, dash_power,
                 15.0 ); // dir_thr

    const PlayerObject * ball_nearest_opp = wm.getOpponentNearestToBall( 5 );

    if ( ball_nearest_opp && ball_nearest_opp->distFromBall() < 2.0 )
    {
        agent->setNeckAction( new Neck_TurnToBallAndPlayer( ball_nearest_opp ) );
    }
    if ( opp_min <= 3
         || wm.ball().distFromSelf() < 10.0 )
    {
        if ( fastest_opp && opp_min <= 1 )
        {
            agent->setNeckAction( new Neck_TurnToBallAndPlayer( fastest_opp ) );
        }
        else
        {
            agent->setNeckAction( new Neck_TurnToBall() );
        }
    }
    else
    {
        agent->setNeckAction( new Neck_TurnToBallOrScan() );
    }
#endif
}

/*-------------------------------------------------------------------*/
/*!

*/
Vector2D
RoleLCenterBack::getDefMidMoveTarget( PlayerAgent * agent,
                                     const Vector2D & home_pos )
{
    const WorldModel & wm = agent->world();

    Vector2D target_point = home_pos;

    int opp_min = wm.interceptTable()->opponentReachCycle();
    const Vector2D opp_trap_pos = ( opp_min < 100
                                    ? wm.ball().inertiaPoint( opp_min )
                                    : wm.ball().inertiaPoint( 20 ) );

    /*--------------------------------------------------------*/
    // decide wait position
    // adjust to the defense line
    if ( -30.0 < home_pos.x
         && home_pos.x < -10.0
         && wm.self().pos().x > home_pos.x
         && wm.ball().pos().x > wm.self().pos().x )
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


    if ( wm.ball().pos().absY() < 7.0
         && wm.ball().pos().x < wm.self().pos().x + 1.0
         && wm.ball().pos().x > -42.0
         && ( wm.ball().vel().x < -0.7
              || wm.existKickableOpponent()
              || opp_min <= 1 )
         )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": getDefMidMoveTarget. correct target point to block center" );
        agent->debugClient().addMessage( "BlockCenter" );
        target_point.assign( -47.0, 0.0 );
        if ( wm.ball().pos().x > -38.0 ) target_point.x = -41.0;
    }

    return target_point;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleLCenterBack::doCrossBlockAreaMove( PlayerAgent * agent,
                                      const Vector2D & home_pos )
{
    agent->debugClient().addMessage( "CrossBlock" );

    doMarkMove( agent, home_pos );
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleLCenterBack::doStopperMove( PlayerAgent * agent,
                               const Vector2D & home_pos )
{
#ifdef  USE_BHV_CENTER_BACK_STOPPER_MODE
    (void)home_pos;
    Bhv_CenterBackStopperMove().execute( agent );
#else
    dlog.addText( Logger::ROLE,
                  __FILE__": doStopperMove" );

    if ( Bhv_DangerAreaTackle().execute( agent ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": doStppperMove danger area tackle" );
        return;
    }

    const WorldModel & wm = agent->world();
    const int opp_min = wm.interceptTable()->opponentReachCycle();
    const PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();

    if ( wm.self().pos().x > 35.0
         && wm.ball().pos().absY() < 5.0
         && wm.ball().pos().x < wm.self().pos().x + 1.0
         && wm.ball().pos().x > -42.0
         && ( wm.ball().vel().x < -0.7
              || wm.existKickableOpponent()
              || opp_min <= 1 )
         )
    {
        Vector2D target_point( -48.0, 0.0 );
        if ( wm.ball().pos().x > -38.0 ) target_point.x = -38.0;

        double dash_power = Strategy::get_defender_dash_power( wm, target_point );
        double dist_thr = wm.ball().pos().dist( target_point ) * 0.1;
        if ( dist_thr < 0.5 ) dist_thr = 0.5;

        dlog.addText( Logger::ROLE,
                      __FILE__": doStopperMove block center. (%.1f %.1f) thr=%.1f power=%.1f",
                      target_point.x, target_point.y,
                      dist_thr,
                      dash_power );
        agent->debugClient().addMessage( "StopperBlockCenter" );
        agent->debugClient().setTarget( target_point );
        agent->debugClient().addCircle( target_point, dist_thr );

        doGoToPoint( agent, target_point, dist_thr, dash_power,
                     15.0 ); // dir_thr

        if ( opp_min <= 3
             || wm.ball().distFromSelf() < 10.0 )
        {
            if ( fastest_opp && opp_min <= 1 )
            {
                agent->setNeckAction( new Neck_TurnToBallAndPlayer( fastest_opp ) );
            }
            else
            {
                agent->setNeckAction( new Neck_TurnToBall() );
            }
        }
        else
        {
            agent->setNeckAction( new Neck_TurnToBallOrScan() );
        }
        return;
    }



    if ( agent->world().ball().pos().x > home_pos.x + 1.0 )
    {
        agent->debugClient().addMessage( "StopperMove(1)" );
        doBasicMove( agent, home_pos );
        return;
    }

    if ( agent->world().ball().pos().absY()
         > ServerParam::i().goalHalfWidth() + 2.0 )
    {
        agent->debugClient().addMessage( "StopperMove(2)" );
        doMarkMove( agent, home_pos );
    }


    ////////////////////////////////////////////////////////
    // search mark target opponent
    if ( doDangerGetBall( agent, home_pos ) )
    {
        agent->debugClient().addMessage( "StopperMove(3)" );
        dlog.addText( Logger::ROLE,
                      __FILE__": doStopperMove. done doDangetGetBall" );
        return;
    }

    agent->debugClient().addMessage( "StopperMove(4)" );
    dlog.addText( Logger::ROLE,
                  __FILE__": doStopperMove no gettable point" );
    Bhv_DefenderBasicBlockMove( home_pos ).execute( agent );
#endif
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleLCenterBack::doDangerAreaMove( PlayerAgent * agent,
                                  const Vector2D & home_pos )
{
#ifdef USE_BHV_CENTER_BACK_DANGER_MODE
    (void)home_pos;
    Bhv_CenterBackDangerMove().execute( agent );
#else
    dlog.addText( Logger::ROLE,
                  __FILE__": doDangerAreaMove" );

    //-----------------------------------------------
    // tackle
    if ( Bhv_DangerAreaTackle().execute( agent ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": doDangerAreaMove. tackle" );
        return;
    }

    const WorldModel & wm = agent->world();

    agent->debugClient().addMessage( "DangerMove" );

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();
    const PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();

    Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

    if ( opp_trap_pos.absY()
         >= ServerParam::i().goalHalfWidth() )
    {
        agent->debugClient().addMessage( "Danger:Mark" );
        doMarkMove( agent, home_pos );
        return;
    }

    if ( opp_min < mate_min
         && opp_min < self_min + 3
         && wm.ball().pos().x > -42.0
         && wm.ball().pos().absY() < 5.0
         && wm.ball().pos().x < wm.self().pos().x + 1.0
         && ( wm.ball().vel().x < -0.7
              || wm.existKickableOpponent()
              || opp_min <= 1 )
         )
    {
        Vector2D target_point( -47.0, 0.0 );
        double y_sign = ( opp_trap_pos.y > 0.0 ? 1.0 : -1.0 );
        if ( opp_trap_pos.absY() > 5.0 ) target_point.y = y_sign * 5.0;
        if ( opp_trap_pos.absY() > 3.0 ) target_point.y = y_sign * 3.0;
        if ( home_pos.y * target_point.y < 0.0 ) target_point.y = 0.0;

        // decide dash power
        double dash_power = Strategy::get_defender_dash_power( wm, target_point );


        //double dist_thr = wm.ball().distFromSelf() * 0.1;
        double dist_thr = wm.ball().pos().dist( target_point ) * 0.1;
        if ( dist_thr < 0.5 ) dist_thr = 0.5;

        dlog.addText( Logger::ROLE,
                      __FILE__": doDangerMove correct target point to block center" );
        agent->debugClient().addMessage( "CB:DangerBlockCenter%.0f", dash_power );
        agent->debugClient().setTarget( target_point );
        agent->debugClient().addCircle( target_point, dist_thr );

        doGoToPoint( agent, target_point, dist_thr, dash_power,
                     15.0 ); // dir_thr
        if ( fastest_opp && opp_min <= 1 )
        {
            agent->setNeckAction( new Neck_TurnToBallAndPlayer( fastest_opp ) );
        }
        else
        {
            agent->setNeckAction( new Neck_TurnToBall() );
        }

        return;
    }


    ////////////////////////////////////////////////////////
    // search mark target opponent
    if ( doDangerGetBall( agent, home_pos ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": doDangerMove. done doDangetGetBall" );
        return;
    }
    dlog.addText( Logger::ROLE,
                  __FILE__": no gettable point in danger mvve" );

    Bhv_DefenderBasicBlockMove( home_pos ).execute( agent );
#endif
}


/*-------------------------------------------------------------------*/
/*!

*/

void
RoleLCenterBack::doMarkMove( PlayerAgent * agent,
                            const Vector2D & home_pos )
{
    //-----------------------------------------------
    // tackle
    if ( Bhv_DangerAreaTackle().execute( agent ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": doMarkMove. done tackle" );
        return;
    }

    ////////////////////////////////////////////////////////

    const WorldModel & wm = agent->world();

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();
    const PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();

    if ( ! wm.existKickableTeammate() )
    {
        if ( ( self_min <= 1 && opp_min >= 1 )
             || ( self_min <= 2 && mate_min >= 2 && opp_min >= 3 )
             || ( self_min <= 3 && mate_min >= 3 && opp_min >= 4 )
             || ( self_min <= 4 && mate_min >= 5 && opp_min >= 6 )
             || ( self_min < opp_min + 3 && self_min < mate_min + 1 ) )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": doMarkMove intercept." );
            agent->debugClient().addMessage( "CBMark.Intercept" );
            Body_Intercept2008().execute( agent );
            if ( fastest_opp && opp_min <= 1 )
            {
                agent->setNeckAction( new Neck_TurnToBallAndPlayer( fastest_opp ) );
            }
            else
            {
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
            }
            return;
        }
    }
#if 0
    if ( opp_min < self_min
         && wm.self().pos().x > -40.0 )
    {
        Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

        if ( opp_trap_pos.absY() < 20.0
             && opp_trap_pos.x < wm.self().pos().x + 2.0 )
        {
            Vector2D move_pos( -47.0, 5.0 );
            if ( home_pos.y < 0.0 ) move_pos.y *= -1.0;

            double dist_thr = 1.0;

            agent->debugClient().addMessage( "CBMark.FastBack" );
            agent->debugClient().setTarget( move_pos );
            agent->debugClient().addCircle( move_pos, dist_thr );

            doGoToPoint( agent,
                         move_pos, dist_thr,
                         ServerParam::i().maxPower(),
                         15.0 ); // dir_thr

            if ( fastest_opp && opp_min <= 1 )
            {
                agent->setNeckAction( new Neck_TurnToBallAndPlayer( fastest_opp ) );
            }
            else if ( opp_min <= 3 )
            {
                agent->setNeckAction( new Neck_TurnToBall() );
            }
            else
            {
                agent->setNeckAction( new Neck_TurnToBallOrScan() );
            }

            return;
        }
    }
#endif

    ////////////////////////////////////////////////////////

    if ( doDangerGetBall( agent, home_pos ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": doMarkMove. done get ball" );
        return;
    }

    ////////////////////////////////////////////////////////
    // search mark target opponent

    const PlayerObject * mark_target = wm.getOpponentNearestTo( home_pos,
                                                                1, NULL );

    ////////////////////////////////////////////////////////
    // check candidate opponent
    if ( ! mark_target
         || mark_target->posCount() > 0
         || mark_target->pos().x > -39.0
         || mark_target->distFromSelf() > 7.0
         || mark_target->distFromBall() < ServerParam::i().defaultKickableArea() + 0.5
         || mark_target->pos().dist( home_pos ) > 3.0
         )
    {
        // not found
        dlog.addText( Logger::ROLE,
                      __FILE__": doMarkMove. mark not found. change to basic move" );
        agent->debugClient().addMessage( "MarkNotFound" );
        doBasicMove( agent, home_pos );
        return;
    }

    ////////////////////////////////////////////////////////
    // check teammate closer than self
    {
        double marker_dist = 100.0;
        const PlayerObject * marker = wm.getTeammateNearestTo( mark_target->pos(),
                                                               30,
                                                               &marker_dist );
        if ( marker && marker_dist < mark_target->distFromSelf() )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": doMarkMove. exist other marker" );
            Bhv_DefenderBasicBlockMove( home_pos ).execute( agent );
            return;
        }
    }

    ////////////////////////////////////////////////////////
    // set target point
    Vector2D block_point = mark_target->pos();
    //block_point += mark_target->vel() / 0.6;
    block_point += mark_target->vel();
    block_point.x -= 0.9;
    block_point.y += ( mark_target->pos().y > wm.ball().pos().y
                       ? -0.6
                       : 0.6 );

    dlog.addText( Logger::ROLE,
                  __FILE__": doMarkMove. mark point(%.1f %.1f)",
                  block_point.x, block_point.y );

    if ( block_point.x > wm.ball().pos().x + 5.0 )
    {
        // not found
        agent->debugClient().addMessage( "MarkToBasic" );
        dlog.addText( Logger::ROLE,
                      __FILE__": doMarkMove. (mark_point.x - ball.x) X diff is big" );
        doBasicMove( agent, home_pos );
        return;
    }

    //double dash_power = ServerParam::i().maxPower();
    double x_diff = block_point.x - wm.self().pos().x;

    if ( x_diff > 20.0 )
    {
        //dash_power *= 0.5;
        //dash_power = wm.self().playerType().staminaIncMax() * wm.self().recovery();
        dlog.addText( Logger::ROLE,
                      __FILE__": doMarkMove. power change(1) X diff=%.2f dash_power=%.1f",
                      x_diff, dash_power );
    }
    else if ( x_diff > 10.0 )
    {
        //dash_power *= 0.7;
        dlog.addText( Logger::ROLE,
                      __FILE__": doMarkMove. power change(2) X diff=%.2f dash_power=%.1f",
                      x_diff, dash_power );
    }
    else if ( wm.ball().pos().dist( block_point ) > 20.0 )
    {
        //dash_power *= 0.6;
        dlog.addText( Logger::ROLE,
                      __FILE__": doMarkMove. mark point is far. dash power=%.1f",
                      dash_power );
    }

    double dist_thr = wm.ball().distFromSelf() * 0.05;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().addMessage( "MarkDashPower%.0f", dash_power );
    agent->debugClient().setTarget( block_point );
    agent->debugClient().addCircle( block_point, dist_thr );

    if ( wm.self().pos().x < block_point.x
         && wm.self().pos().dist( block_point ) < dist_thr )
    {
        AngleDeg body_angle = ( wm.ball().pos().x < wm.self().pos().x - 5.0
                                ? 0.0
                                : 180.0 );
        dlog.addText( Logger::ROLE,
                      __FILE__": doMarkMove. already there. body to angle %.1f",
                      body_angle.degree() );
        Body_TurnToAngle( body_angle ).execute( agent );

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
                agent->setNeckAction( new Neck_TurnToBall() );
            }
        }
        else
        {
            agent->setNeckAction( new Neck_TurnToBallOrScan() );
        }
        return;
    }

    if ( wm.self().pos().dist( block_point ) > 3.0 )
    {
        agent->debugClient().addMessage( "MarkForwardDash" );
        agent->debugClient().setTarget( block_point );
        agent->debugClient().addCircle( block_point, dist_thr );

        dlog.addText( Logger::ROLE,
                      __FILE__": doMarkMove. forward move" );

        doGoToPoint( agent, block_point, dist_thr, dash_power, 15.0 );

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
                agent->setNeckAction( new Neck_TurnToBall() );
            }
        }
        else
        {
            agent->setNeckAction( new Neck_TurnToBallOrScan() );
        }
        return;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": doMarkMove. ball looking move" );

    Bhv_GoToPointLookBall( block_point,
                           dist_thr,
                           dash_power
                           ).execute( agent );

    const PlayerObject * ball_owner = wm.getOpponentNearestToBall( 5 );

    if ( ! ball_owner
         || ball_owner->distFromBall() > 1.2 + wm.ball().distFromSelf() * 0.05 )
    {
        if ( mark_target
             && mark_target->unum() == Unum_Unknown )
        {
            agent->setNeckAction( new Neck_TurnToPlayerOrScan( mark_target, -1 ) );
        }
        else
        {
            agent->setNeckAction( new Neck_TurnToBallOrScan() );
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
RoleLCenterBack::doDangerGetBall( PlayerAgent * agent,
                                 const Vector2D & home_pos )
{
    //////////////////////////////////////////////////////////////////
    // tackle
    if ( Bhv_DangerAreaTackle().execute( agent ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": doDangetGetBall done tackle" );
        return true;
    }

    const WorldModel & wm = agent->world();

    //--------------------------------------------------
    if ( wm.existKickableTeammate()
         || wm.ball().pos().x > -36.0
         || wm.ball().pos().dist( home_pos ) > 7.0
         || wm.ball().pos().absY() > 9.0
         )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": doDangetGetBall. no danger situation" );
        return false;
    }

    const PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();

    if ( wm.existKickableOpponent() )
    {
        Vector2D opp_pos = wm.opponentsFromBall().front()->pos();

        if ( wm.self().pos().x < opp_pos.x
             && std::fabs( wm.self().pos().y - opp_pos.y ) < 0.8 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": doDangetGetBall attack to ball owner" );

            Vector2D opp_vel = wm.opponentsFromBall().front()->vel();
            opp_pos.x -= 0.2;
            if ( opp_vel.x < -0.1 )
            {
                opp_pos.x -= 0.5;
            }
            if ( opp_pos.x > wm.self().pos().x
                 && fabs(opp_pos.y - wm.self().pos().y) > 0.8 )
            {
                opp_pos.x = wm.self().pos().x;
            }

            agent->debugClient().addMessage( "DangerAttackOpp" );
            agent->debugClient().setTarget( opp_pos );
            agent->debugClient().addCircle( opp_pos, 0.1 );

            dlog.addText( Logger::ROLE,
                          __FILE__": doDangetGetBall (%.2f %.2f)",
                          opp_pos.x, opp_pos.y );
            Body_GoToPoint( opp_pos,
                            0.1,
                            dash_power
                            ).execute( agent );
            if ( fastest_opp )
            {
                agent->setNeckAction( new Neck_TurnToBallAndPlayer( fastest_opp ) );
            }
            else
            {
                agent->setNeckAction( new Neck_TurnToBall() );
            }
            return true;
        }
    }
    else
    {
        int self_min = wm.interceptTable()->selfReachCycle();
        rcsc::Vector2D intercept_pos = wm.ball().inertiaPoint( self_min );

        if ( self_min <= 2
             && intercept_pos.dist( home_pos ) < 10.0 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": doDangetGetBall. intercept. reach cycle = %d",
                          self_min );
            agent->debugClient().addMessage( "DangerGetBall" );

            Body_Intercept2008().execute( agent );
            agent->setNeckAction( new Neck_TurnToBallOrScan() );
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleLCenterBack::doGoToPoint( PlayerAgent * agent,
                             const Vector2D & target_point,
                             const double & dist_thr,
                             const double & dashpower,
                             const double & dir_thr )
{

    if ( Body_GoToPoint( target_point,
                         dist_thr,
                         dashpower,
                         100, // cycle
                         false, // no back
                         true, // stamina save
                         dir_thr
                         ).execute( agent ) )
    {
        return;
    }

    const WorldModel & wm = agent->world();

    AngleDeg body_angle;
    if ( wm.ball().pos().x < -30.0 )
    {
        body_angle = wm.ball().angleFromSelf() + 90.0;
        if ( wm.ball().pos().x < -45.0 )
        {
            if (  body_angle.degree() < 0.0 )
            {
                body_angle += 180.0;
            }
        }
        else if ( body_angle.degree() > 0.0 )
        {
            body_angle += 180.0;
        }
    }
    else // if ( std::fabs( wm.self().pos().y - wm.ball().pos().y ) > 4.0 )
    {
        //body_angle = wm.ball().angleFromSelf() + ( 90.0 + 20.0 );
        body_angle = wm.ball().angleFromSelf() + 90.0;
        if ( wm.ball().pos().x > wm.self().pos().x + 15.0 )
        {
            if ( body_angle.abs() > 90.0 )
            {
                body_angle += 180.0;
            }
        }
        else
        {
            if ( body_angle.abs() < 90.0 )
            {
                body_angle += 180.0;
            }
        }
    }
    /*
      else
      {
      body_angle = ( wm.ball().pos().y < wm.self().pos().y
      ? -90.0
      : 90.0 );
      }
    */

    Body_TurnToAngle( body_angle ).execute( agent );

}
