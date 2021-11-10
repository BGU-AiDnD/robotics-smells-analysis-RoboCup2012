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

#include "role_Ldefensive_half.h"

#include "strategy.h"

#include "bhv_basic_defensive_kick.h"
#include "bhv_basic_offensive_kick.h"
#include "bhv_cross.h"
#include "bhv_keep_shoot_chance.h"

#include "bhv_basic_move.h"
#include "bhv_defender_basic_block_move.h"
#include "bhv_defender_get_ball.h"
#include "bhv_press.h"
#include "bhv_block_dribble.h"

#include "bhv_basic_tackle.h"

#include "body_advance_ball_test.h"
#include "body_clear_ball2008.h"
#include "body_dribble2008.h"
#include "body_intercept2008.h"
#include "body_hold_ball2008.h"
#include "bhv_pass_test.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>
#include <rcsc/action/neck_scan_field.h>

#include <rcsc/formation/formation.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/math_util.h>
#include <rcsc/soccer_math.h>

#include "bhv_defensive_half_danger_move.h"
#include "bhv_defensive_half_defensive_move.h"
#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

#include "bhv_fs_mark_opponent_forward.h"

//#define USE_BHV_DEFENSIVE_HALF_DANGER_MOVE
#define USE_BHV_DEFENSIVE_HALF_DEFENSIVE_MOVE

#define dash_power 33.0

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleLDefensiveHalf::execute( rcsc::PlayerAgent* agent )
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
RoleLDefensiveHalf::doKick( rcsc::PlayerAgent* agent )
{
    const double nearest_opp_dist
        = agent->world().getDistOpponentNearestToSelf( 5 );

    switch ( Strategy::get_ball_area(agent->world().ball().pos()) ) {
    case Strategy::BA_CrossBlock:
        if ( nearest_opp_dist < 3.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": cross block area. danger" );
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
                                __FILE__": stopper or danger" );
            if ( ! rcsc::Bhv_PassTest().execute( agent ) )
            {
                Body_ClearBall2008().execute( agent );

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
        if ( Bhv_Cross::get_best_point( agent, NULL ) )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": shoot chance area" );
            Bhv_Cross().execute( agent );
        }
        else
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": keep shoot chance" );
            Bhv_KeepShootChance().execute( agent );
        }
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
RoleLDefensiveHalf::doMove( rcsc::PlayerAgent * agent )
{
    rcsc::Vector2D home_pos = Strategy::i().getPosition( agent->world().self().unum() );
    if ( ! home_pos.isValid() ) home_pos.assign( 0.0, 0.0 );

    switch ( Strategy::get_ball_area( agent->world() ) ) {
    case Strategy::BA_CrossBlock:
    case Strategy::BA_Stopper:
        doCrossBlockMove( agent, home_pos );
        break;
    case Strategy::BA_Danger:
#ifdef USE_BHV_DEFENSIVE_HALF_DANGER_MOVE
        Bhv_DefensiveHalfDangerMove().execute( agent );
#else
        if ( agent->world().existKickableTeammate()
             && ! agent->world().existKickableOpponent() )
        {
            rcsc::Vector2D new_target = home_pos;
            new_target.x += 5.0;
            Bhv_BasicMove( new_target ).execute( agent );
        }
        else
        {
            Bhv_DefenderBasicBlockMove( home_pos ).execute( agent );
        }
#endif
        break;
    case Strategy::BA_DribbleBlock:
        doDribbleBlockMove( agent, home_pos );
        break;
    case Strategy::BA_DefMidField:
        doDefensiveMove( agent, home_pos );
        break;
    case Strategy::BA_DribbleAttack:
        Bhv_BasicMove( home_pos ).execute( agent );
        break;
    case Strategy::BA_OffMidField:
        if(Bhv_FSMarkOpponentForward(home_pos).execute(agent))
	        break;
        doOffensiveMove( agent, home_pos );
        break;
    case Strategy::BA_Cross:
        doCrossAreaMove( agent, home_pos );
        break;
    case Strategy::BA_ShootChance:
        Bhv_BasicMove( home_pos ).execute( agent );
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
RoleLDefensiveHalf::doCrossBlockMove( rcsc::PlayerAgent* agent,
                                     const rcsc::Vector2D& home_pos )
{
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doCrossBlockMove" );

    //-----------------------------------------------
    // tackle
    if ( Bhv_BasicTackle( 0.8, 80.0 ).execute( agent ) )
    {
        return;
    }

    const rcsc::WorldModel & wm = agent->world();

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    //if ( wm.interceptTable()->isSelfFastestPlayer() )
    if ( ! wm.existKickableTeammate()
         && ! wm.existKickableOpponent()
         && self_min < mate_min
         && self_min < opp_min )
    {
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
                                  ( new rcsc::Neck_TurnToBallOrScan() ) );
        }
#endif
        return;
    }

//     if ( Bhv_DefenderGetBall( home_pos ).execute( agent ) )
//     {
//         rcsc::dlog.addText( rcsc::Logger::ROLE,
//                             __FILE__": get ball" );
//         return;
//     }

  //  double dash_power;

    if ( wm.ball().distFromSelf() > 30.0 )
    {
    //    dash_power = wm.self().playerType().staminaIncMax() * 0.9;
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": ball is too far. dash_power=%.1f",
                            dash_power );
    }
    else if ( wm.ball().distFromSelf() > 20.0 )
    {
      //  dash_power = dash_power * 0.5;
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": ball far. dash_power=%.1f",
                            dash_power );
    }
    else
    {
        //dash_power = dash_power * 0.9;
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": near. dash_power = %.1f",
                            dash_power );
    }

    if ( wm.existKickableTeammate() )
    {
       // dash_power = std::min( dash_power * 0.5,
//                               dash_power );
    }

    //dash_power = wm.self().getSafetyDashPower( dash_power );

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().addMessage( "CrossBlock" );
    agent->debugClient().setTarget( home_pos );
    agent->debugClient().addCircle( home_pos, dist_thr );

    if ( ! rcsc::Body_GoToPoint( home_pos, dist_thr, dash_power ).execute( agent ) )
    {
        rcsc::AngleDeg body_angle( wm.ball().angleFromSelf().abs() < 80.0
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
RoleLDefensiveHalf::doOffensiveMove( rcsc::PlayerAgent * agent,
                                    const rcsc::Vector2D & home_pos )
{
    static bool s_recover_mode = false;

    //-----------------------------------------------
    // tackle
    if ( Bhv_BasicTackle( 0.8, 80.0 ).execute( agent ) )
    {
        return;
    }

    const rcsc::WorldModel & wm = agent->world();
    /*--------------------------------------------------------*/
    // chase ball
    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( ! wm.existKickableTeammate()
         && ( self_min <= 3
              || ( self_min < mate_min + 3
                   && self_min <= opp_min + 1 )
              )
         )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": doOffensiveMove() intercept" );
        rcsc::Body_Intercept2008().execute( agent );
#if 0
        if ( self_min == 4 && opp_min >= 2 )
        {
            agent->setViewAction( new rcsc::View_Wide() );
        }
        else if ( self_min == 3 && opp_min >= 2 )
        {
            agent->setViewAction( new rcsc::View_Normal() );
        }

        if ( wm.ball().distFromSelf()
             < rcsc::ServerParam::i().visibleDistance() )
        {
            agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
        }
        else
        {
            agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );

        }
#else
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
#endif
        return;
    }

    if ( wm.self().stamina() < rcsc::ServerParam::i().staminaMax() * 0.5 )
    {
        s_recover_mode = true;
    }
    else if ( wm.self().stamina() > rcsc::ServerParam::i().staminaMax() * 0.7 )
    {
        s_recover_mode = false;
    }


   // double dash_power = 100.0;
    const double my_inc
        = wm.self().playerType().staminaIncMax()
        * wm.self().recovery();

/*    if ( wm.defenseLineX() > wm.self().pos().x
         && wm.ball().pos().x < wm.defenseLineX() + 20.0 )
    {
        dash_power = dash_power;
    }
    else if ( s_recover_mode )
    {
        dash_power = my_inc - 25.0; // preffered recover value
        if ( dash_power < 0.0 ) dash_power = 0.0;
    }
    else if ( wm.existKickableTeammate()
              && wm.ball().distFromSelf() < 20.0 )
    {
        dash_power = std::min( my_inc * 1.1,
                               dash_power );
    }
    // in offside area
    else if ( wm.self().pos().x > wm.offsideLineX() )
    {
        dash_power = dash_power;
    }
    else if ( home_pos.x < wm.self().pos().x
              && wm.self().stamina() > rcsc::ServerParam::i().staminaMax() * 0.7 )
    {
        dash_power = dash_power;
    }
    // normal
    else
    {
        dash_power = std::min( my_inc * 1.7,
                               dash_power );
    }*/


    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 1.5 ) dist_thr = 1.5;

    agent->debugClient().addMessage( "OffMove%.0f", dash_power );
    agent->debugClient().setTarget( home_pos );
    agent->debugClient().addCircle( home_pos, dist_thr );

    if ( ! rcsc::Body_GoToPoint( home_pos, dist_thr, dash_power
                                 ).execute( agent ) )
    {
        rcsc::Vector2D my_next = wm.self().pos() + wm.self().vel();
        rcsc::Vector2D ball_next = ( wm.existKickableOpponent()
                                     ? wm.ball().pos()
                                     : wm.ball().pos() + wm.ball().vel() );
        rcsc::AngleDeg body_angle = ( ball_next - my_next ).th() + 90.0;
        if ( body_angle.abs() < 90.0 ) body_angle -= 180.0;

        rcsc::Body_TurnToAngle( body_angle ).execute( agent );
    }

    if ( wm.existKickableOpponent()
         && wm.ball().distFromSelf() < 18.0 )
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
void
RoleLDefensiveHalf::doDefensiveMove( rcsc::PlayerAgent * agent,
                                    const rcsc::Vector2D & home_pos )
{
#ifdef USE_BHV_DEFENSIVE_HALF_DEFENSIVE_MOVE
    (void)home_pos;
    Bhv_DefensiveHalfDefensiveMove().execute( agent );
#else
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doDefensiveMove" );

    const rcsc::WorldModel & wm = agent->world();

    //////////////////////////////////////////////////////
    // tackle
    if ( Bhv_BasicTackle( 0.85, 75.0 ).execute( agent ) )
    {
        return;
    }

    //----------------------------------------------
    // intercept
    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    const rcsc::Vector2D self_reach_point = wm.ball().inertiaPoint( self_min );
    const rcsc::Vector2D opp_reach_point = wm.ball().inertiaPoint( opp_min );

    if ( ! wm.existKickableTeammate()
#if 1
         && self_reach_point.dist( home_pos ) < 13.0
#endif
         && ( self_min < mate_min
              || self_min <= 3 && wm.ball().pos().dist2( home_pos ) < 10.0*10.0
              || self_min <= 5 && wm.ball().pos().dist2( home_pos ) < 8.0*8.0 )
         )
    {
#if 1
        if ( opp_min < mate_min - 1
             && opp_min < self_min - 2 )
        {
            if ( Bhv_BlockDribble().execute( agent ) )
            {
                rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doDefensiveMove. BlockDribble" );
                agent->debugClient().addMessage( "DefMidBlockDrib" );
                return;
            }
        }
#endif
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doDefensiveMove. self < mate" );
        rcsc::Body_Intercept2008().execute( agent );

#if 0
        if ( self_min == 4 && opp_min >= 2 )
        {
            agent->setViewAction( new rcsc::View_Wide() );
        }
        else if ( self_min == 3 && opp_min >= 2 )
        {
            agent->setViewAction( new rcsc::View_Normal() );
        }

        agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
#else
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
#endif
        return;
    }

//     if ( wm.interceptTable()->isSelfFastestPlayer() )
//     {
//         rcsc::dlog.addText( rcsc::Logger::ROLE,
//                             __FILE__": doDefensiveMove. fastest." );
//         rcsc::Body_Intercept2008().execute( agent );
//         agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
//         return;
//     }

    //////////////////////////////////////////////////////

    rcsc::Vector2D ball_future
        = rcsc::inertia_n_step_point( wm.ball().pos(),
                                      wm.ball().vel(),
                                      mate_min,
                                      rcsc::ServerParam::i().ballDecay() );

    const double future_x_diff
        = ball_future.x - wm.self().pos().x;

    double dash_power = dash_power;

    if ( wm.existKickableTeammate() )
    {
        dash_power *= 0.5;
    }
    else if ( future_x_diff < 0.0 ) // ball is behind
    {
        dash_power *= 0.9;
    }
    else if ( wm.self().stamina()
              > rcsc::ServerParam::i().staminaMax() * 0.8 )
    {
        dash_power *= 0.8;
    }
    else
    {
        dash_power = wm.self().playerType().staminaIncMax();
        dash_power *= wm.self().recovery();
        dash_power *= 0.9;
    }

    // save recovery
    dash_power = wm.self().getSafetyDashPower( dash_power );

    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doDefensiveMove() go to home. dash_power=%.1f",
                        dash_power );

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().addMessage( "DMFDefMove" );
    agent->debugClient().setTarget( home_pos );
    agent->debugClient().addCircle( home_pos, dist_thr );

    if ( ! rcsc::Body_GoToPoint( home_pos, dist_thr, dash_power ).execute( agent ) )
    {
        rcsc::AngleDeg body_angle = 0.0;
        if ( wm.ball().angleFromSelf().abs() > 80.0 )
        {
            body_angle = ( wm.ball().pos().y > wm.self().pos().y
                           ? 90.0
                           : -90.0 );
        }
        rcsc::Body_TurnToAngle( body_angle ).execute( agent );
    }
    agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
#endif
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleLDefensiveHalf::doDribbleBlockMove( rcsc::PlayerAgent* agent,
                                       const rcsc::Vector2D& home_pos )
{
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doDribbleBlockMove()" );

    const rcsc::WorldModel & wm = agent->world();

    ///////////////////////////////////////////////////
    // tackle
    if ( Bhv_BasicTackle( 0.8, 80.0 ).execute( agent ) )
    {
        return;
    }

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    //if ( wm.interceptTable()->isSelfFastestPlayer() )
    if ( ! wm.existKickableTeammate()
         && ! wm.existKickableOpponent()
         && self_min < mate_min
         && self_min < opp_min )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doDribbleBlockMove() I am fastest. intercept" );
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
                                  ( new rcsc::Neck_TurnToBallOrScan() ) );
        }
#endif
        return;
    }

    ///////////////////////////////////////////////////
   // double dash_power;

    if ( wm.self().pos().x + 5.0 < wm.ball().pos().x )
    {
        if ( wm.self().stamina() < rcsc::ServerParam::i().staminaMax() * 0.7 )
        {
     //       dash_power
       //         = dash_power * 0.7
         //       * ( wm.self().stamina() / rcsc::ServerParam::i().staminaMax() );
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": doDribbleBlockMove() dash_power=%.1f",
                                dash_power );
        }
        else
        {
   //         dash_power
     //           = dash_power * 0.75
       //         - std::min( 30.0, wm.ball().distFromSelf() );
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": doDribbleBlockMove() dash_power=%.1f",
                                dash_power );
        }
    }
    else
    {
    //    dash_power
      //      = dash_power
        //    + 10.0
          //  - wm.ball().distFromSelf();
      //  dash_power = rcsc::min_max( 0.0, dash_power, dash_power );
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doDribbleBlockMove() dash_power=%.1f",
                            dash_power );
    }



    if ( wm.existKickableTeammate() )
    {
   //     dash_power = std::min( dash_power * 0.5,
     //                          dash_power );
    }

    //dash_power = wm.self().getSafetyDashPower( dash_power );

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().addMessage( "DribBlockMove" );
    agent->debugClient().setTarget( home_pos );
    agent->debugClient().addCircle( home_pos, dist_thr );

    if ( ! rcsc::Body_GoToPoint( home_pos, dist_thr, dash_power ).execute( agent ) )
    {
        // face to front or side
        rcsc::AngleDeg body_angle = 0.0;
        if ( wm.ball().angleFromSelf().abs() > 90.0 )
        {
            body_angle = ( wm.ball().pos().y > wm.self().pos().y
                           ? 90.0
                           : -90.0 );
        }
        rcsc::Body_TurnToAngle( body_angle ).execute( agent );
    }
    agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleLDefensiveHalf::doCrossAreaMove( rcsc::PlayerAgent * agent,
                                    const rcsc::Vector2D & home_pos )
{
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": doCrossAreaMove. normal move" );
    Bhv_BasicMove( home_pos ).execute( agent );
}
