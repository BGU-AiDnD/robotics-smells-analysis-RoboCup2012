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

#include "role_Lcenter_half.h"

#include "strategy.h"

#include "bhv_attacker_offensive_move.h"
#include "bhv_basic_defensive_kick.h"
#include "bhv_basic_offensive_kick.h"
#include "bhv_cross.h"
#include "bhv_keep_shoot_chance.h"
#include "bhv_self_pass.h"

#include "bhv_basic_move.h"
#include "body_kick_to_corner.h"
#include "bhv_defender_basic_block_move.h"
#include "bhv_defender_get_ball.h"
#include "bhv_press.h"
#include "bhv_block_dribble.h"
#include "body_smart_kick.h"

#include "bhv_basic_tackle.h"

#include "body_advance_ball_test.h"
#include "body_clear_ball2008.h"
#include "body_dribble2008.h"
#include "body_intercept2008.h"
#include "body_hold_ball2008.h"
#include "bhv_pass_test.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/bhv_shoot2008.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_pass.h>
#include <rcsc/action/body_kick_one_step.h>
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

#include "bhv_fs_clear_ball.h"
#include "bhv_fs_shoot.h"
#include "bhv_fs_through_pass_kick.h"
#include "bhv_fs_through_pass_move.h"

#include "bhv_shoot2009.h"

int RoleLCenterHalf::S_count_shot_area_hold = 0;

//#define USE_BHV_DEFENSIVE_HALF_DANGER_MOVE
#define USE_BHV_DEFENSIVE_HALF_DEFENSIVE_MOVE

#define dash_power 33.0

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleLCenterHalf::execute( rcsc::PlayerAgent* agent )
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
RoleLCenterHalf::doKick( rcsc::PlayerAgent* agent )
{
    const rcsc::WorldModel & wm = agent->world();

    const double nearest_opp_dist
        = agent->world().getDistOpponentNearestToSelf( 5 );
        
    rcsc::Vector2D pass_point;

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
 //       Bhv_BasicDefensiveKick().execute( agent );
 //       break;
// 	doMiddleAreaKick( agent );
//        break;	
    case Strategy::BA_DribbleAttack:
    case Strategy::BA_OffMidField:
        if(Bhv_FSThroughPassKick().execute(agent))
	break;
//      doOffensiveMiddleKick(agent);
	doMiddleAreaKick( agent );
        break;		
    case Strategy::BA_Cross:
        Bhv_Cross().execute( agent );
        break;
    case Strategy::BA_ShootChance:      
      if(Bhv_FSShoot().execute(agent))
      	break;
      
      if( Bhv_Shoot2009().execute( agent ) )
	break;

      if( Bhv_Shoot2008().execute( agent ) )
	break;
      
      if(Bhv_FSThroughPassKick().execute(agent))
	  break;
      
      if(rcsc::Body_Pass::get_best_pass( wm, &pass_point, NULL, NULL))
       {
	  if(pass_point.dist(rcsc::Vector2D(52.5,0))<
	     wm.self().pos().dist(rcsc::Vector2D(52.5,0))-5)//パラメータ追加(真崎)無駄パスにならない？
	    {//--ゴールに近い味方がいたらパス--
	      if(rcsc::ServerParam::i().useOffside() 
		 && pass_point.x<wm.offsideLineX()
		 || !rcsc::ServerParam::i().useOffside()){
		rcsc::Body_Pass().execute(agent);
		agent->setNeckAction(new rcsc::Neck_TurnToLowConfTeammate());
		break;
	      }
	    }
       }
/*      if(wm.self().pos().dist(rcsc::Vector2D(52.5,0))<16.0)
       {
        if(Bhv_FSShoot().execute(agent))
          break;

	if(opp_dist<=5.0)force=true;
	rcsc::Body_Dribble2008(rcsc::Vector2D(52.0,0),5.0,
			     rcsc::ServerParam::i().maxDashPower(),
			     2,force).execute( agent );
	agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );   
	break;
       }*/
     doShootAreaKick( agent );	
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
RoleLCenterHalf::doMove( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();
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
        //Bhv_BasicMove( home_pos ).execute( agent );
        //break;
    case Strategy::BA_OffMidField:
        if(Bhv_FSMarkOpponentForward(home_pos).execute(agent))
	break;
        if (wm.ball().pos().x > 25)
        Bhv_AttackerOffensiveMove( home_pos, true ).execute( agent );
        else if (wm.ball().pos().x < 10)
        doDefensiveMove( agent, home_pos );
    else doOffensiveMove( agent, home_pos );
        break;
    case Strategy::BA_Cross:
        doCrossAreaMove( agent, home_pos );
        break;
    case Strategy::BA_ShootChance:
        Bhv_AttackerOffensiveMove( home_pos, true ).execute( agent );
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
RoleLCenterHalf::doCrossBlockMove( rcsc::PlayerAgent* agent,
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

   // double dash_power;

    if ( wm.ball().distFromSelf() > 30.0 )
    {
     //   dash_power = wm.self().playerType().staminaIncMax() * 0.9;
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": ball is too far. dash_power=%.1f",
                            dash_power );
    }
    else if ( wm.ball().distFromSelf() > 20.0 )
    {
       // dash_power = dash_power * 0.5;
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

/*    if ( wm.existKickableTeammate() )
    {
        dash_power = std::min( dash_power * 0.5,
                               dash_power );
    }

    dash_power = wm.self().getSafetyDashPower( dash_power );*/

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
RoleLCenterHalf::doOffensiveMove( rcsc::PlayerAgent * agent,
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


    //double dash_power = 100.0;
    const double my_inc
        = wm.self().playerType().staminaIncMax()
        * wm.self().recovery();

 /*   if ( wm.defenseLineX() > wm.self().pos().x
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
RoleLCenterHalf::doDefensiveMove( rcsc::PlayerAgent * agent,
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
RoleLCenterHalf::doDribbleBlockMove( rcsc::PlayerAgent* agent,
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
    //double dash_power;

    if ( wm.self().pos().x + 5.0 < wm.ball().pos().x )
    {
        if ( wm.self().stamina() < rcsc::ServerParam::i().staminaMax() * 0.7 )
        {
      //      dash_power
        //        = dash_power * 0.7
          //      * ( wm.self().stamina() / rcsc::ServerParam::i().staminaMax() );
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": doDribbleBlockMove() dash_power=%.1f",
                                dash_power );
        }
        else
        {
       //     dash_power
         //       = dash_power * 0.75
           //     - std::min( 30.0, wm.ball().distFromSelf() );
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
        //dash_power = rcsc::min_max( 0.0, dash_power, dash_power );
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doDribbleBlockMove() dash_power=%.1f",
                            dash_power );
    }



/*    if ( wm.existKickableTeammate() )
    {
        dash_power = std::min( dash_power * 0.5,
                               dash_power );
    }

    dash_power = wm.self().getSafetyDashPower( dash_power );*/

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
RoleLCenterHalf::doCrossAreaMove( rcsc::PlayerAgent * agent,
                                    const rcsc::Vector2D & home_pos )
{
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": doCrossAreaMove. normal move" );
    Bhv_BasicMove( home_pos ).execute( agent );
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleLCenterHalf::doShootAreaKick( rcsc::PlayerAgent* agent )
{
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doShootAreaKick" );
    agent->debugClient().addMessage( "KeepChance" );
    const rcsc::WorldModel & wm = agent->world();

    //------------------------------------------------------------//
    // get the nearest opponent
    const rcsc::PlayerObject * nearest_opp = wm.getOpponentNearestToSelf( 10 );
    const double opp_dist = ( nearest_opp
                              ? nearest_opp->distFromSelf()
                              : 1000.0 );
    const rcsc::Vector2D opp_pos = ( nearest_opp
                                     ? nearest_opp->pos()
                                     : rcsc::Vector2D( -1000.0, 0.0 ) );

    //------------------------------------------------------------//
    // get goalie parameters
    const rcsc::PlayerObject * opp_goalie = wm.getOpponentGoalie();
    rcsc::Vector2D goalie_pos( -1000.0, 0.0 );
    double goalie_dist = 15.0;
    if ( opp_goalie )
    {
        goalie_pos = opp_goalie->pos();
        goalie_dist = opp_goalie->distFromSelf();
    }

    //------------------------------------------------------------//
    // count opponent at the front of goal
    const int num_opp_at_goal_front = wm.countOpponentsIn( rcsc::Rect2D( rcsc::Vector2D( 45.0, -6.0 ),
                                                                         rcsc::Size2D( 7.0, 12.0 ) ),
                                                           10,
                                                           false ); // without goalie
    agent->debugClient().addRectangle( rcsc::Rect2D( rcsc::Vector2D( 45.0, -6.0 ),
                                                     rcsc::Size2D( 7.0, 12.0 ) ) );
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doShootAreaKick num_opp_at_goal_front = %d",
                        num_opp_at_goal_front );

    //------------------------------------------------------------//
    // check dribble chance
    if ( num_opp_at_goal_front <= 2
         && wm.self().pos().x < 42.0
         && wm.self().pos().absY() < 8.0
         && wm.self().stamina() > rcsc::ServerParam::i().staminaMax() * 0.5 )
    {
        const rcsc::Rect2D target_rect
            = rcsc::Rect2D::from_center( wm.ball().pos().x + 5.5,
                                         wm.ball().pos().y,
                                         10.0, 12.0 );
        if ( ! nearest_opp
             || ( wm.self().body().abs() < 30.0
                  && opp_pos.x < wm.ball().pos().x
                  && nearest_opp->distFromBall() > 2.0
                  && ! wm.existOpponentIn( target_rect, 10, false ) )
             )
        {
            if ( ! opp_goalie
                 || opp_goalie->distFromBall() > 5.0 )
            {
                rcsc::Vector2D drib_target
                    = wm.self().pos()
                    + rcsc::Vector2D::polar2vector( 10.0, wm.self().body() );

                agent->debugClient().addMessage( "ShotDribSt" );
                agent->debugClient().addRectangle( target_rect );
                rcsc::dlog.addText( rcsc::Logger::ROLE,
                                    __FILE__": doShootAreaKick. dribble straight to (%.1f %.1f)",
                                    drib_target.x, drib_target.y );
                rcsc::Body_Dribble2008( drib_target,
                                        1.0,
                                        dash_power,
                                        5, // dash count
                                        false // no dodge
                                        ).execute( agent );
                if ( opp_goalie )
                {
                    agent->setNeckAction( new rcsc::Neck_TurnToPoint( opp_goalie->pos() ) );
                }
                else
                {
                    agent->setNeckAction( new rcsc::Neck_ScanField() );
                }
                return;
            }
        }

    }

    //------------------------------------------------------------//
    // check pass parameters
    const rcsc::Bhv_PassTest::PassRoute * pass
        = rcsc::Bhv_PassTest::get_best_pass( wm );

    double pass_point_opp_dist = 1000.0;
    if ( pass )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doShootAreaKick. PassPoint(%.1f %.1f)",
                            pass->receive_point_.x,
                            pass->receive_point_.y );
        if ( ! wm.getOpponentNearestTo( pass->receive_point_,
                                        10,
                                        &pass_point_opp_dist ) )
        {
            pass_point_opp_dist = 1000.0;
        }
    }

    //------------------------------------------------------------//
    // check very good pass
    if ( pass
         && pass_point_opp_dist > 5.0
         && ( pass->receive_point_.x > 42.0
              || pass->receive_point_.x > wm.self().pos().x - 1.0 )
         && pass->receive_point_.absY() < 10.0 )
    {
        agent->debugClient().addMessage( "ShotAreaPass(1)" );
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doShootAreaKick. Pass1-1 To(%.1f %.1f)",
                            pass->receive_point_.x,
                            pass->receive_point_.y );
        rcsc::Bhv_PassTest().execute( agent );
        return;
    }

    if ( pass
         && pass_point_opp_dist > 3.0
         && ( pass->receive_point_.x > 42.0
              || pass->receive_point_.x > wm.self().pos().x - 1.0 )
         && pass->receive_point_.absY() < 5.0 )
    {
        agent->debugClient().addMessage( "ShotAreaPass(1-2)" );
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doShootAreaKick. Pass1-2 To(%.1f %.1f)",
                            pass->receive_point_.x,
                            pass->receive_point_.y );
        rcsc::Bhv_PassTest().execute( agent );
        return;
    }

    //------------------------------------------------------------//
    // check cross paramters
    rcsc::Vector2D cross_point( rcsc::Vector2D::INVALIDATED );
    bool can_cross = Bhv_Cross::get_best_point( agent, &cross_point );
    double cross_point_opp_dist = 20.0;
    if ( can_cross )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doShootAreaKick. CrossPoint(%.1f %.1f)",
                            cross_point.x, cross_point.y );

        if ( cross_point.absY() > 10.0
             && cross_point.y * wm.self().pos().y > 0.0 // same side
             && cross_point.absY() > wm.self().pos().absY() )
        {
            can_cross = false;
            agent->debugClient().addMessage( "InvalidCross" );
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": doShootAreaKick. cross point is same side &outer. cancel",
                                cross_point.x, cross_point.y );
        }

        if ( can_cross )
        {
            if ( ! wm.getOpponentNearestTo( cross_point,
                                            10,
                                            &cross_point_opp_dist ) )
            {
                cross_point_opp_dist = 20.0;
            }
        }
    }

    //------------------------------------------------------------//
    // check good cross chance
    if ( can_cross
         && cross_point.x > 42.0
         && cross_point_opp_dist > 3.0 )
    {
        agent->debugClient().addMessage( "ShotAreaCross(1)" );
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doShootAreaKick. found good cross(%.1f %.1f)",
                            cross_point.x, cross_point.y );
        Bhv_Cross().execute( agent );
        return;
    }

    if ( can_cross
         && cross_point.x > 44.0
         && cross_point_opp_dist > 2.0
         && cross_point.absY() < 7.0 )
    {
        agent->debugClient().addMessage( "ShotAreaCross(1-2)" );
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doShootAreaKick. found good cross(2) pos=(%.1f %.1f)",
                            cross_point.x, cross_point.y );
        Bhv_Cross().execute( agent );
        return;
    }

    //------------------------------------------------------------//
    // check dribble chance
    if ( num_opp_at_goal_front <= 2
         && wm.self().pos().x < 46.5
         && ( opp_dist > 3.0 || opp_pos.x < wm.self().pos().x + 0.5 )
         && ( goalie_dist > 5.0
              || wm.self().pos().x < goalie_pos.x - 3.0 )
         )
    {
        bool goalie_front = false;
        if ( opp_goalie
             && opp_goalie->pos().x < 46.0
             && opp_goalie->pos().x < wm.self().pos().x + 7.0
             && std::fabs( opp_goalie->pos().y - wm.self().pos().y ) < 2.0 )
        {
            goalie_front = true;
        }

        if ( ! goalie_front )
        {
            const rcsc::Rect2D target_rect( rcsc::Vector2D( wm.ball().pos().x + 0.5,
                                                            wm.ball().pos().y - 1.4 ),
                                            rcsc::Size2D( 2.5, 2.8 ) );
            const rcsc::Rect2D target_rect2( rcsc::Vector2D( wm.ball().pos().x + 1.0,
                                                             wm.ball().pos().y - 0.7 ),
                                             rcsc::Size2D( 7.0, 1.4 ) );
            if ( ! wm.existOpponentIn( target_rect, 10, true ) // with goalie
                 //&& ! wm.existOpponentIn( target_rect2, 10, false ) // without goalie
                 )
            {
                rcsc::Vector2D drib_target( 47.0, wm.self().pos().y );
#if 1
                if ( wm.self().body().abs() < 20.0 )
                {
                    rcsc::Line2D my_line( wm.self().pos(), wm.self().body() );
                    drib_target.y = my_line.getY( drib_target.x );
                    if ( drib_target.y == rcsc::Line2D::ERROR_VALUE )
                    {
                        drib_target.y = wm.self().pos().y;
                    }
                }
#endif
                if ( drib_target.absY() > 12.0 )
                {
                    drib_target.y = ( drib_target.y > 0.0 ? 12.0 : -12.0 );
                }
#if 1
                if ( wm.ball().pos().x > 42.0
                     && drib_target.absY() > 7.0 )
                {
                    drib_target.y = ( drib_target.y > 0.0 ? 7.0 : -7.0 );
                }
#endif
                if ( opp_goalie
                     && opp_goalie->pos().x > wm.self().pos().x
                     //&& opp_goalie->pos().y * wm.self().pos().y < 0.0
                     && std::fabs( opp_goalie->pos().y - wm.self().pos().y ) < 3.0
                     && opp_goalie->pos().absY() < wm.self().pos().absY() )
                {
                    drib_target.x = opp_goalie->pos().x - 3.0;
                }
                if ( wm.self().pos().x > drib_target.x - 0.8
                     && wm.self().pos().absY() > 9.0 )
                {
                    drib_target.y = 7.0 * ( drib_target.y < 0.0 ? -1.0 : 1.0 );
                }

                if ( wm.self().pos().dist( drib_target ) > 1.0 )
                {
                    agent->debugClient().addMessage( "ShotAreaDrib1" );
                    agent->debugClient().addRectangle( target_rect );
                    agent->debugClient().addRectangle( target_rect2 );
                    rcsc::dlog.addText( rcsc::Logger::ROLE,
                                        __FILE__": doShootAreaKick. dribble to (%.1f %.1f)",
                                        drib_target.x, drib_target.y );
                    rcsc::Body_Dribble2008( drib_target,
                                            1.0,
                                            dash_power,
                                            1, // one dash
                                            false // no dodge
                                            ).execute( agent );
                    if ( ! doCheckCrossPoint( agent ) )
                    {
                        agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
                    }
                    return;
                }
            }
        }
    }

#if 1
    //------------------------------------------------------------//
    // check dribble chance (2)
    if ( wm.self().pos().x > 44.0
         && goalie_dist > 4.0
         )
    {
        rcsc::Vector2D drib_target( 45.0, 0.0 );
        const rcsc::Rect2D target_rect( rcsc::Vector2D( wm.self().pos().x - 2.0,
                                                        ( wm.self().pos().y > 0.0
                                                          ? wm.self().pos().y - 5.0
                                                          : wm.self().pos().y ) ),
                                        rcsc::Size2D( 4.0, 5.0 ) );
        bool try_dribble = false;
        // opponent check with goalie
        if ( ! wm.existOpponentIn( target_rect, 10, true ) )
        {
            try_dribble = true;

            if ( opp_goalie
                 && opp_goalie->pos().x > wm.self().pos().x
                 && std::fabs( opp_goalie->pos().y - wm.self().pos().y ) < 3.0
                 && opp_goalie->pos().absY() < wm.self().pos().absY() )
            {
                drib_target.x = opp_goalie->pos().x - 3.0;
                drib_target.y += ( opp_goalie->pos().y > wm.self().pos().y
                                   ? - 5.0
                                   : + 5.0 );
            }

            if ( nearest_opp
                 && nearest_opp->distFromSelf() < 0.8 )
            {
                rcsc::Vector2D my_next = wm.self().pos() - wm.self().vel();
                rcsc::AngleDeg target_angle = ( drib_target - my_next ).th();
                if ( ( target_angle - wm.self().body() ).abs() > 20.0 )
                {
                    try_dribble = false;
                }
            }
        }

        if ( try_dribble )
        {
            agent->debugClient().addMessage( "ShotAreaDrib2" );
            agent->debugClient().addRectangle( target_rect );
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": doShootAreaKick. dribble2 to (%.1f %.1f)",
                                drib_target.x, drib_target.y );
            rcsc::Body_Dribble2008( drib_target,
                                    1.0,
                                    dash_power,
                                    1, // one dash
                                    false // no dodge
                                    ).execute( agent );
            if ( ! doCheckCrossPoint( agent ) )
            {
                agent->setNeckAction(new rcsc::Neck_TurnToLowConfTeammate() );
            }

            return;
        }
    }
#endif

    //------------------------------------------------------------//
    // check opponent
    // if opponent is close
    if ( opp_dist < 2.0
         || goalie_dist < 4.0 )
    {
        if ( can_cross )
        {
            agent->debugClient().addMessage( "ShotAreaCross(2)" );
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": doShootAreaKick. cross(2)  opp_dist=%.2f goalie_dist=%.2f",
                                opp_dist, goalie_dist );
            Bhv_Cross().execute( agent );
            return;
        }

        if ( pass )
        {
            agent->debugClient().addMessage( "ShotAreaPass(2)" );
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": doShootAreaKick. pass  opp_dist=%.2f goalie_dist=%.2f",
                                opp_dist, goalie_dist );
            rcsc::Bhv_PassTest().execute( agent );
            return;
        }

        rcsc::Rect2D goal_area( rcsc::Vector2D( rcsc::ServerParam::i().pitchHalfLength() - 10.0,
                                                -10.0 ),
                                rcsc::Size2D( 13.0, 20.0 ) );
        if ( wm.existTeammateIn( goal_area, 10, true ) )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": doShootAreaKick. enforce cross" );

            rcsc::Vector2D kick_target( 45.0, 20.0 );

            kick_target.x = std::max( 45.0, wm.self().pos().x - 1.0 );
            if ( wm.self().pos().y > 0.0 ) kick_target.y *= -1.0;

            agent->debugClient().setTarget( kick_target );

            if ( goalie_dist < 3.0 )
            {
                agent->debugClient().addMessage( "ForceCross1K" );
                rcsc::Body_KickOneStep( kick_target,
                                        rcsc::ServerParam::i().ballSpeedMax()
                                        ).execute( agent );
            }
            else
            {
                agent->debugClient().addMessage( "ForceCross2K" );
                rcsc::Body_SmartKick( kick_target,
                                      rcsc::ServerParam::i().ballSpeedMax(),
                                      rcsc::ServerParam::i().ballSpeedMax() * 0.8,
                                      2 ).execute( agent );
            }

            if ( ! doCheckCrossPoint( agent ) )
            {
                agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
            }
            return;
        }
    }

    ++S_count_shot_area_hold;

    if ( S_count_shot_area_hold >= 30 )
    {
        rcsc::Vector2D drib_target( wm.self().pos().x + 5.0, wm.self().pos().y );
        agent->debugClient().addMessage( "ShotAreaDribFinal" );
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doShootAreaKick. dribble final to (%.1f %.1f)",
                            drib_target.x, drib_target.y );
        rcsc::Body_Dribble2008( drib_target,
                                1.0,
                                dash_power,
                                1, // one dash
                                false // no dodge
                                ).execute( agent );
        if ( ! doCheckCrossPoint( agent ) )
        {
            agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
        }
        return;
    }

#if 0
    // test 2008-04-30
    {
        rcsc::Vector2D drib_target( 40.0, 0.0 );
        agent->debugClient().addMessage( "ShotAreaDribAvoid" );
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doShootAreaKick. dribble avoid to (%.1f %.1f)",
                            drib_target.x, drib_target.y );
        rcsc::Body_Dribble2008( drib_target,
                                1.0,
                                dash_power,
                                1, // one dash
                                true // dodge
                                ).execute( agent );
        if ( ! doCheckCrossPoint( agent ) )
        {
            agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan( 0 ) );
        }
        return;
    }
#endif

    agent->debugClient().addMessage( "ShotAreaHold" );
    rcsc::Vector2D face_point( 52.5, 0.0 );
    rcsc::Body_HoldBall2008( true, face_point ).execute( agent );

    if ( ! doCheckCrossPoint( agent ) )
    {
        agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
    }
}
/*-------------------------------------------------------------------*/
/*!

*/
bool
RoleLCenterHalf::doCheckCrossPoint( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();

    if ( wm.self().pos().x < 35.0 )
    {
        return false;
    }

    const rcsc::PlayerObject * opp_goalie = wm.getOpponentGoalie();
    if ( opp_goalie && opp_goalie->posCount() > 2 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doCheckCrossTarget  goalie should be checked" );
        return false;
    }

    rcsc::Vector2D opposite_pole( 46.0, 7.0 );
    if ( wm.self().pos().y > 0.0 ) opposite_pole.y *= -1.0;

    rcsc::AngleDeg opposite_pole_angle = ( opposite_pole - wm.self().pos() ).th();


    if ( wm.dirCount( opposite_pole_angle ) <= 1 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doCheckCrossTarget enough accuracy to angle %.1f",
                            opposite_pole_angle.degree() );
        return false;
    }

    rcsc::AngleDeg angle_diff
        = agent->effector().queuedNextAngleFromBody( opposite_pole );
    if ( angle_diff.abs() > 100.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doCheckCrossPoint. want to face opposite pole,"
                            " but over view range. angle_diff=%.1f",
                            angle_diff.degree() );
        return false;
    }


    agent->setNeckAction( new rcsc::Neck_TurnToPoint( opposite_pole ) );
    agent->debugClient().addMessage( "NeckToOpposite" );
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doCheckCrossPoint Neck to oppsite pole" );
    return true;
}
/*-------------------------------------------------------------------*/
/*!

*/
void
RoleLCenterHalf::doMiddleAreaKick( rcsc::PlayerAgent * agent )
{
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doMiddleAreaKick" );

    const rcsc::WorldModel & wm = agent->world();

#if 0
    {
        for ( int dash_step = 16; dash_step >= 6; dash_step -= 2 )
        {
            if ( doSelfPass( agent, dash_step ) )
            {
                rcsc::dlog.addText( rcsc::Logger::ROLE,
                                    __FILE__": doMiddleAreaKick SelfPass" );
                return;
            }
        }
    }
#else
    if ( Bhv_SelfPass().execute( agent ) )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doMiddleAreaKick SelfPass" );
        return;
    }
#endif

    const rcsc::PlayerObject * nearest_opp = wm.getOpponentNearestToSelf( 5 );

    const double nearest_opp_dist = ( nearest_opp
                                      ? nearest_opp->distFromSelf()
                                      : 1000.0 );
    const rcsc::Vector2D nearest_opp_pos = ( nearest_opp
                                             ? nearest_opp->pos()
                                             : rcsc::Vector2D( -1000.0, 0.0 ) );

    const rcsc::Bhv_PassTest::PassRoute * pass
        = rcsc::Bhv_PassTest::get_best_pass( wm );

    if ( pass
         && pass->receive_point_.x > wm.self().pos().x + 1.5 )
    {
        double opp_dist = 200.0;
        wm.getOpponentNearestTo( pass->receive_point_, 10, &opp_dist );
        if ( opp_dist > 5.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": first pass" );
            agent->debugClient().addMessage( "MidPass(1)" );
            rcsc::Bhv_PassTest().execute( agent );
            return;
        }
    }

    // passable & opp near
    if ( nearest_opp_dist < 3.0 )
    {
        if ( rcsc::Bhv_PassTest().execute( agent ) )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": pass" );
            agent->debugClient().addMessage( "MidPass(2)" );
            return;
        }
    }

    rcsc::Vector2D drib_target;

    if ( nearest_opp_dist >
         rcsc::ServerParam::i().tackleDist() + rcsc::ServerParam::i().defaultPlayerSpeedMax() )
    {
        // dribble to my body dir
        rcsc::AngleDeg ang_l, ang_r;
        if ( wm.self().pos().y > 0.0 )
        {
            ang_l = ( rcsc::Vector2D( 52.5, -7.0 ) - wm.self().pos() ).th();
            ang_r = ( rcsc::Vector2D( 52.5, 38.0 ) - wm.self().pos() ).th();
        }
        else
        {
            ang_l = ( rcsc::Vector2D( 52.5, -38.0 ) - wm.self().pos() ).th();
            ang_r = ( rcsc::Vector2D( 52.5, 7.0 ) - wm.self().pos() ).th();
        }

        if ( wm.self().body().isWithin( ang_l, ang_r ) )
        {
            drib_target
                = wm.self().pos()
                + rcsc::Vector2D::polar2vector( 5.0, wm.self().body() );

            int max_dir_count = 0;
            wm.dirRangeCount( wm.self().body(), 20.0, &max_dir_count, NULL, NULL );

            if ( drib_target.absX() < rcsc::ServerParam::i().pitchHalfLength() - 1.0
                 && drib_target.absY() < rcsc::ServerParam::i().pitchHalfWidth() - 1.0
                 && max_dir_count <= 5 )
            {
                const rcsc::Sector2D sector(wm.self().pos(), 0.5, 10.0,
                                            wm.self().body() - 30.0,
                                            wm.self().body() + 30.0);
                // oponent check with goalie
                if ( ! wm.existOpponentIn( sector, 10, true ) )
                {
                    rcsc::dlog.addText( rcsc::Logger::ROLE,
                                        __FILE__": dribble to may body dir" );
                    agent->debugClient().addMessage( "MidDribToBody(1)" );
                    rcsc::Body_Dribble2008( drib_target,
                                            1.0,
                                            dash_power,
                                            10 //2
                                            ).execute( agent );
                    agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
                    return;
                }
            }
        }

    }

    drib_target.x = 50.0;
    drib_target.y = ( wm.self().pos().y > 0.0
                      ? 7.0
                      : -7.0 );
    const rcsc::AngleDeg target_angle = (drib_target - wm.self().pos()).th();
    const rcsc::Sector2D sector( wm.self().pos(),
                                 0.5, 15.0,
                                 target_angle - 30.0,
                                 target_angle + 30.0 );
    // opp's is behind of me
    if ( nearest_opp_pos.x < wm.self().pos().x + 1.0
         && nearest_opp_dist > 2.0 )
    {
        // oponent check with goalie
        if ( ! wm.existOpponentIn( sector, 10, true ) )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": opp behind. dribble 3 dashes" );
            agent->debugClient().addMessage( "MidDrib3D(1)" );
            rcsc::Body_Dribble2008( drib_target,
                                    1.0,
                                    dash_power,
                                    10 //3
                                    ).execute( agent );
        }
        else
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": opp behind. dribble 1 dash" );
            agent->debugClient().addMessage( "MidDrib1D(1)" );
            rcsc::Body_Dribble2008( drib_target,
                                    1.0,
                                    dash_power,
                                    10 //1
                                    ).execute( agent );
        }
        agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
        return;
    }


    // opp is far from me
    if ( nearest_opp_dist > 8.0 )
    {
        // oponent check with goalie
        if ( ! wm.existOpponentIn( sector, 10, true ) )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": opp far. dribble 3 dashes" );
            agent->debugClient().addMessage( "MidDrib3D(2)" );
            rcsc::Body_Dribble2008( drib_target,
                                    1.0,
                                    dash_power,
                                    10 //3
                                    ).execute( agent );
        }
        else
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": opp far. dribble 1 dash" );
            agent->debugClient().addMessage( "MidDrib1D(2)" );
            rcsc::Body_Dribble2008( drib_target,
                                    1.0,
                                    dash_power,
                                    10 //1
                                    ).execute( agent );
        }
        agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
        return;
    }

    // opponent is close

    if ( nearest_opp_dist > 5.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": opp middle close. dribble" );
        agent->debugClient().addMessage( "MidDribSlow" );
        rcsc::Body_Dribble2008( drib_target,
                                1.0,
                                dash_power,
                                10 //1
                                ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
        return;
    }

    // exist close opponent
    // pass
    if ( rcsc::Bhv_PassTest().execute( agent ) )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": opp close. pass" );
        agent->debugClient().addMessage( "MidPass" );
        return;
    }

    // dribble to my body dir
    if ( nearest_opp_dist > rcsc::ServerParam::i().tackleDist()
         || nearest_opp_pos.x < wm.self().pos().x + 1.0 )
    {
        // dribble to my body dir
        if ( wm.self().body().abs() < 100.0 )
        {
            const rcsc::Vector2D body_dir_drib_target
                = wm.self().pos()
                + rcsc::Vector2D::polar2vector(5.0, wm.self().body());
            int max_dir_count = 0;
            wm.dirRangeCount( wm.self().body(), 20.90, &max_dir_count, NULL, NULL );

            if ( max_dir_count <= 5
                 && body_dir_drib_target.x < rcsc::ServerParam::i().pitchHalfLength() - 1.0
                 && body_dir_drib_target.absY() < rcsc::ServerParam::i().pitchHalfWidth() - 1.0
                 )
            {
                // check opponents
                // 10m, +-30 degree
                const rcsc::Sector2D body_dir_sector( wm.self().pos(),
                                                      0.5, 10.0,
                                                      wm.self().body() - 30.0,
                                                      wm.self().body() + 30.0);
                // oponent check with goalie
                if ( ! wm.existOpponentIn( sector, 10, true ) )
                {
                    rcsc::dlog.addText( rcsc::Logger::ROLE,
                                        __FILE__": opp close. dribble to my body" );
                    agent->debugClient().addMessage( "MidDribToBody(2)" );
                    rcsc::Body_Dribble2008( body_dir_drib_target,
                                            1.0,
                                            dash_power,
                                            10 //2
                                            ).execute( agent );
                    agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
                    return;
                }
            }
        }
    }

    if ( wm.self().pos().x > wm.offsideLineX() - 5.0 )
    {
        // kick to corner
        rcsc::Vector2D corner_pos( rcsc::ServerParam::i().pitchHalfLength() - 8.0,
                                   rcsc::ServerParam::i().pitchHalfWidth() - 8.0 );
        if ( wm.self().pos().y < 0.0 )
        {
            corner_pos.y *= -1.0;
        }

        // near side
        if ( wm.self().pos().x < 25.0
             || wm.self().pos().absY() > 18.0 )
        {
            const rcsc::Sector2D front_sector( wm.self().pos(),
                                               0.5, 10.0,
                                               -30.0, 30.0 );
            // oponent check with goalie
            if ( ! wm.existOpponentIn( front_sector, 10, true ) )
            {
                rcsc::dlog.addText( rcsc::Logger::ROLE,
                                    __FILE__": kick to corner" );
                agent->debugClient().addMessage( "MidToCorner(1)" );
                Body_KickToCorner( (wm.self().pos().y < 0.0) ).execute( agent );
                agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
                return;
            }
        }

        const rcsc::AngleDeg corner_angle = (corner_pos - wm.self().pos()).th();
        const rcsc::Sector2D corner_sector( wm.self().pos(),
                                            0.5, 10.0,
                                            corner_angle - 15.0,
                                            corner_angle + 15.0 );
        // oponent check with goalie
        if ( ! wm.existOpponentIn( corner_sector, 10, true ) )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": kick to near side" );
            agent->debugClient().addMessage( "MidToCorner(2)" );
            Body_KickToCorner( ( wm.self().pos().y < 0.0 )
                               ).execute( agent );
            agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
            return;
        }
    }

    agent->debugClient().addMessage( "MidHold" );
    rcsc::Body_HoldBall2008().execute( agent );
    agent->setNeckAction( new rcsc::Neck_ScanField() );
}

//-----------------------------------------

void RoleLCenterHalf::doOffensiveMiddleKick( PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();

    const rcsc::PlayerObject * nearest_opp = wm.getOpponentNearestToSelf( 6 );

    if ( nearest_opp )
    {

    }

    if ( Bhv_SelfPass().execute( agent ) )
    {
        return;
    }

    Vector2D receiver;

    bool pass = Body_Pass().get_best_pass( wm , &receiver , NULL , NULL );

    // dribble straight
    if ( wm.self().stamina() > //rcsc::ServerParam::i().staminaMax() * 0.5
         rcsc::ServerParam::i().recoverDecThrValue() + 200.0
         && ( ! nearest_opp
              || nearest_opp->pos().x < wm.self().pos().x + 0.5
              || std::fabs( nearest_opp->pos().y -  wm.self().pos().y ) > 2.0
              || nearest_opp->distFromSelf() > 2.0 )
         )
    {
        const rcsc::Rect2D target_rect
            = rcsc::Rect2D::from_center( wm.ball().pos().x + 5.5,
                                         wm.ball().pos().y,
                                         10.0, 12.0 );
        if ( ! wm.existOpponentIn( target_rect, 10, false ) )
        {
            rcsc::Vector2D drib_target( 47.0, wm.self().pos().y );
            if ( wm.self().body().abs() < 20.0 )
            {
                drib_target
                    = wm.self().pos()
                    + rcsc::Vector2D::polar2vector( 10.0, wm.self().body() );
            }

            int dash_step = 10;

            const rcsc::Rect2D safety_rect
                = rcsc::Rect2D::from_center( wm.ball().pos().x + 7.0,
                                             wm.ball().pos().y,
                                             14.0, 15.0 );
            if ( wm.existOpponentIn( target_rect, 10, false ) )
            {
                dash_step = 3;
            }

            agent->debugClient().addMessage( "MidDribSt" );
            agent->debugClient().addRectangle( target_rect );
            agent->debugClient().addRectangle( safety_rect );

            rcsc::Body_Dribble2008( drib_target,
                                    1.0,
                                    dash_power,
                                    dash_step, // dash count
                                    false // no dodge
                                    ).execute( agent );
            agent->setNeckAction( new rcsc::Neck_ScanField() );
            return;
        }
    }


    // pass in order to avoid opponent
    if ( pass
         && nearest_opp
         && nearest_opp->distFromSelf() < 5.0 )
    {
        rcsc::Body_Pass().execute( agent );
        agent->debugClient().addMessage( "MidPass(1)" );
        return;
    }

    //------------------------------------------------
    // pass to side area
    rcsc::Rect2D front_rect( rcsc::Vector2D( wm.self().pos().x,
                                             wm.self().pos().y - 5.0 ),
                             rcsc::Size2D( 10.0, 10.0 ) );
    if ( pass
         && receiver.x > wm.self().pos().x
         && receiver.absY() > 20.0
         && wm.existOpponentIn( front_rect, 10, false ) ) // without goalie
    {
        agent->debugClient().addMessage( "MidPassSide(1)" );
        rcsc::Body_Pass().execute( agent );
        return;
    }

    if ( pass
         && receiver.x > wm.self().pos().x + 10.0
         && receiver.absY() > 20.0 )
    {
        agent->debugClient().addMessage( "MidPassSide(2)" );
        rcsc::Body_Pass().execute( agent );
        return;
    }

    rcsc::Vector2D drib_target( 50.0, 0.0 );
    drib_target.y = ( agent->world().self().pos().y > 0.0
                      ? 5.0 : -5.0 );

    //------------------------------------------------
    // opp's is behind of me
    if ( ! nearest_opp
         || ( nearest_opp->distFromSelf()
              > rcsc::ServerParam::i().defaultPlayerSize() * 2.0 + 0.2
              && nearest_opp->pos().x < wm.self().pos().x )
         )
    {
        rcsc::AngleDeg drib_angle = ( drib_target - wm.self().pos() ).th();
        const rcsc::Sector2D sector( agent->world().self().pos(),
                                     0.5, 15.0,
                                     drib_angle - 30.0, drib_angle + 30.0 );
        // oppnent check with goalie
        if ( ! agent->world().existOpponentIn( sector, 10, true ) )
        {
            agent->debugClient().addMessage( "MidDribFast" );
            rcsc::Body_Dribble2008( drib_target,
                                    1.0,
                                    dash_power,
                                    5
                                    ).execute( agent );
        }
        else
        {
            agent->debugClient().addMessage( "MidDribSlow" );
            rcsc::Body_Dribble2008( drib_target,
                                    1.0,
                                    dash_power,
                                    5
                                    ).execute( agent );
        }
        agent->setNeckAction( new rcsc::Neck_ScanField() );
        return;
    }

    //------------------------------------------------
    // check pass point
    if ( pass
         && receiver.x > wm.offsideLineX() )
    {
        agent->debugClient().addMessage( "MidPassOverOffside" );
        rcsc::Body_Pass().execute( agent );
        return;
    }

    //------------------------------------------------
    // opp is far from me
    if ( ! nearest_opp
         || nearest_opp->distFromSelf() > 3.0 )
    {
        agent->debugClient().addMessage( "MidDribD1" );
        rcsc::Body_Dribble2008( drib_target,
                                1.0,
                                dash_power,
                                1
                                ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
        return;
    }

    //------------------------------------------------
    // opp is close
    // pass
    if ( pass )
    {
        agent->debugClient().addMessage( "MidPassNorm" );
        rcsc::Body_Pass().execute( agent );
        return;
    }

    //------------------------------------------------
    // other
    // near opponent may exist.

    agent->debugClient().addMessage( "MidAreaHold" );
    rcsc::Vector2D face_point( 52.5, 0.0 );
    rcsc::Body_HoldBall2008( true, face_point ).execute( agent );
    agent->setNeckAction( new rcsc::Neck_ScanField() );

}
