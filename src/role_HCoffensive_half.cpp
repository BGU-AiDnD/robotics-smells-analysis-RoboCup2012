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

#include "role_HCoffensive_half.h"

#include "strategy.h"

#include "bhv_cross.h"
#include "bhv_basic_defensive_kick.h"
#include "bhv_basic_offensive_kick.h"
#include "bhv_keep_shoot_chance.h"

#include "bhv_basic_tackle.h"

#include "bhv_basic_move.h"
#include "bhv_attacker_offensive_move.h"
#include "bhv_block_dribble.h"
#include "bhv_mark_close_opponent.h"
#include "bhv_press.h"
#include <rcsc/action/bhv_shoot2008.h>
#include "bhv_shoot2009.h"

#include "bhv_fs_clear_ball.h"
#include "bhv_fs_shoot.h"
#include "bhv_nutmeg_kick.h"
#include "bhv_fs_through_pass_move.h"
#include "bhv_fs_through_pass_kick.h"

#include "body_advance_ball_test.h"
#include "body_clear_ball2008.h"
#include "body_dribble2008.h"
#include "body_intercept2008.h"
#include "body_hold_ball2008.h"
#include "bhv_pass_test.h"
#include <rcsc/action/body_pass.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>
#include <rcsc/action/neck_scan_field.h>
#include "bhv_self_pass.h"
#include "body_kick_to_corner.h"

#include <rcsc/formation/formation.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/player_config.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/ray_2d.h>
#include <rcsc/soccer_math.h>

#include "bhv_offensive_half_defensive_move.h"
#include "bhv_offensive_half_offensive_move.h"
#include "bhv_block_ball_owner.h"

#include "bhv_fs_mark_opponent_forward.h"

#include "neck_offensive_intercept_neck.h"
#include "neck_default_intercept_neck.h"

#define USE_BHV_OFFENSIVE_HALF_DEFENSIVE_MOVE
#define USE_BHV_OFFENSIVE_HALF_OFFENSIVE_MOVE

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleHCOffensiveHalf::execute( rcsc::PlayerAgent * agent )
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
RoleHCOffensiveHalf::doKick( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();
    double dash_power = 100.0;
    const double nearest_opp_dist
        = agent->world().getDistOpponentNearestToSelf( 5 );
        
        rcsc::Vector2D pass_point;

    switch ( Strategy::get_ball_area( agent->world().ball().pos() ) ) {
    case Strategy::BA_CrossBlock:
    case Strategy::BA_Stopper:
        if ( nearest_opp_dist < rcsc::ServerParam::i().tackleDist() )
        {
            Body_AdvanceBallTest().execute( agent );
            agent->setNeckAction( new rcsc::Neck_ScanField() );
        }
        else
        {
            Bhv_BasicOffensiveKick().execute( agent );
        }
        break;
    case Strategy::BA_Danger:
        agent->debugClient().addMessage( "DangerArea" );
        if ( nearest_opp_dist < rcsc::ServerParam::i().tackleDist() )
        {
            Body_ClearBall2008().execute( agent );
            agent->setNeckAction( new rcsc::Neck_ScanField() );
        }
        else
        {
            Bhv_BasicDefensiveKick().execute( agent );
        }
        break;
    case Strategy::BA_DribbleBlock:
        Bhv_BasicDefensiveKick().execute( agent );
        break;
    case Strategy::BA_DefMidField:
        //Bhv_BasicOffensiveKick().execute( agent );
        //break;
    case Strategy::BA_DribbleAttack:
        //doDribbleAttackKick( agent );
        //break;
    case Strategy::BA_OffMidField:
        //agent->debugClient().addMessage( "OffMid" );
        //Bhv_BasicOffensiveKick().execute( agent );
        //break;
        if(Bhv_FSThroughPassKick().execute(agent))
	        break;

        if( rcsc::Circle2D(rcsc::Vector2D(50,0),20).contains(wm.self().pos())
	        &&Bhv_FSShoot().execute(agent))
	            break;
	  
        doMiddleAreaKick( agent );
        break;
    case Strategy::BA_ShootChance:
      //こいつはここだとパスしないんだね。まぁいいか
      if(Bhv_FSShoot().execute(agent))
      break;
      
      if( Bhv_Shoot2009().execute( agent ) )
	{
		break;
	}
      if( Bhv_Shoot2008().execute( agent ) )
      {
		break;
      }
      
      if(Bhv_FSThroughPassKick().execute(agent))
	  break;
        
      if(Bhv_KeepShootChance().execute( agent ))
        break;
       
/*      if (rcsc::Body_Dribble2008(dribble_target,2.0,
	  		       rcsc::ServerParam::i().maxDashPower(),2,false
			       ).execute( agent ))
      {			       
      	agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan() );
      	break;
      }*/
      
      Bhv_BasicOffensiveKick().execute( agent );
        break;

    case Strategy::BA_Cross:
      //-----クロス時-----
      if( rcsc::Circle2D(rcsc::Vector2D(50,0),20).contains(wm.self().pos())
	  &&Bhv_FSShoot().execute(agent))
	break;

	if(rcsc::Body_Pass::get_best_pass( wm, &pass_point, NULL, NULL)){
	  rcsc::Body_Pass().execute(agent);
	  agent->setNeckAction(new rcsc::Neck_TurnToLowConfTeammate());
	  break;
	}
      Bhv_BasicOffensiveKick().execute( agent );
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
RoleHCOffensiveHalf::doMove( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();

    double dash_power = 100.0;
    rcsc::Vector2D home_pos = Strategy::i().getPosition( agent->world().self().unum() );
    if ( ! home_pos.isValid() ) home_pos.assign( 0.0, 0.0 );

    switch ( Strategy::get_ball_area( agent->world() ) ) {
    case Strategy::BA_CrossBlock:
    case Strategy::BA_Stopper:
    case Strategy::BA_Danger:
    case Strategy::BA_DribbleBlock:
    case Strategy::BA_DefMidField:
        doDefensiveMove( agent, home_pos );
        break;
    case Strategy::BA_DribbleAttack:
    case Strategy::BA_OffMidField:

        if(Bhv_FSMarkOpponentForward(home_pos).execute(agent))
        	break;

        if(Bhv_AttackerOffensiveMove( home_pos, true ).execute( agent ))
        break;

        if(wm.existKickableOpponent())
            doDefensiveMove( agent, home_pos );
            break;

        doOffensiveMove( agent, home_pos );
        break;
    case Strategy::BA_Cross:
    case Strategy::BA_ShootChance:
        doShootAreaMove( agent, home_pos );
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
RoleHCOffensiveHalf::doDribbleAttackKick( rcsc::PlayerAgent * agent )
{double dash_power = 100.0;
    agent->debugClient().addMessage( "DribAtt" );
    Bhv_BasicOffensiveKick().execute( agent );
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleHCOffensiveHalf::doShootChanceKick( rcsc::PlayerAgent * agent )
{double dash_power = 100.0;
    agent->debugClient().addMessage( "ShootChance" );
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doShootChanceKick()" );

    const rcsc::Bhv_PassTest::PassRoute * pass
        = rcsc::Bhv_PassTest::get_best_pass( agent->world() );
    if ( pass )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__":  pass found (%.1f, %.1f)",
                            pass->receive_point_.x,
                            pass->receive_point_.y );
        const rcsc::PlayerObject * nearest_opp
            = agent->world().getOpponentNearestToSelf( 1 );

        if ( pass->receive_point_.x > 36.0
             && nearest_opp
             && nearest_opp->distFromSelf() < 2.0 )
        {
            agent->debugClient().addMessage( "KeepChancePass1" );
            rcsc::Bhv_PassTest().execute( agent );
            return;
        }
    }

    Bhv_KeepShootChance().execute( agent );
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleHCOffensiveHalf::doOffensiveMove( rcsc::PlayerAgent * agent,
                                    const rcsc::Vector2D & home_pos )
{double dash_power = 100.0;
#ifdef USE_BHV_OFFENSIVE_HALF_OFFENSIVE_MOVE
    (void)home_pos;
    Bhv_OffensiveHalfOffensiveMove().execute( agent );
#else
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doOffensiveMove" );

    if ( home_pos.x > 30.0
         && home_pos.absY() < 7.0 )
    {
        agent->debugClient().addMessage( "ShotSupport" );
        Bhv_AttackerOffensiveMove( home_pos, false ).execute( agent );
        return;
    }

    //----------------------------------------------
    // tackle
    if ( Bhv_BasicTackle( 0.8, 90.0 ).execute( agent ) )
    {
        return;
    }

    const rcsc::WorldModel & wm = agent->world();
    //----------------------------------------------
    // intercept
    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    //------------------------------------------------------
    const rcsc::PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();

    if ( fastest_opp
         && opp_min < self_min - 2
         && opp_min < mate_min
         && wm.ball().pos().dist( home_pos ) < 10.0 )
    {
        rcsc::Vector2D attack_point = ( opp_min >= 3
                                        ? wm.ball().inertiaPoint( opp_min )
                                        : fastest_opp->pos() + fastest_opp->vel() );
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doOffensiveMove. attack opponent. pos=(%.1f %.1f) ",
                            attack_point.x, attack_point.y );

        if ( std::fabs( attack_point.y - wm.self().pos().y ) > 1.0 )
        {
            rcsc::Line2D opp_move_line( fastest_opp->pos(),
                                        fastest_opp->pos() + rcsc::Vector2D( -3.0, 0.0 ) );
            rcsc::Ray2D my_move_line( wm.self().pos(),
                                      wm.self().body() );
            rcsc::Vector2D intersection = my_move_line.intersection( opp_move_line );
            if ( intersection.valid()
                 && attack_point.x - 6.0 < intersection.x
                 && intersection.x < attack_point.x - 1.0 )
            {
                attack_point.x = intersection.x;
                rcsc::dlog.addText( rcsc::Logger::ROLE,
                                    __FILE__": doOffensiveMove. attack opponent. keep body direction" );
            }
            else
            {
                attack_point.x -= 4.0;
                if ( std::fabs( fastest_opp->pos().y - wm.self().pos().y ) > 10.0 )
                {
                    attack_point.x -= 2.0;
                }

                rcsc::dlog.addText( rcsc::Logger::ROLE,
                                    __FILE__": doOffensiveMove. attack opponent. back " );
            }
        }

        if ( attack_point.x < home_pos.x + 5.0 )
        {
            double dash_power = dash_power;
            if ( wm.self().stamina() < rcsc::ServerParam::i().staminaMax() * 0.6 )
            {
                dash_power *= wm.self().stamina() / rcsc::ServerParam::i().staminaMax();
            }
            agent->debugClient().addMessage( "OffAttackBall" );
            agent->debugClient().setTarget( attack_point );
            agent->debugClient().addCircle( attack_point, 0.5 );

            rcsc::Body_GoToPoint( attack_point, 0.5, dash_power ).execute( agent );
            agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
            return;
        }
    }


    if ( self_min < mate_min )
    {
        if ( ( opp_min <= 3
               && opp_min <= self_min - 1 )
             || opp_min <= self_min - 4 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": doOffensiveMove. I am fastest in teammate. but opp is faster." );
            agent->debugClient().addMessage( "OffPress(1)" );
            Bhv_Press( home_pos ).execute( agent );
            return;
        }
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doOffensiveMove. I am fastest in teammate" );
        agent->debugClient().addMessage( "OffGetBall(1)" );
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

    if ( opp_min < mate_min
         && wm.ball().pos().dist( home_pos ) < 10.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doOffensiveMove. ball owner is not our. press" );
        agent->debugClient().addMessage( "OffPress(2)" );
        Bhv_Press( home_pos ).execute( agent );
        return;
    }

    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doOffensiveMove. change to basic move" );
    agent->debugClient().addMessage( "OffMove" );
    Bhv_BasicMove( home_pos ).execute( agent );
#endif
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleHCOffensiveHalf::doDefensiveMove( rcsc::PlayerAgent * agent,
                                    const rcsc::Vector2D & home_pos )
{double dash_power = 100.0;
#ifdef USE_BHV_OFFENSIVE_HALF_DEFENSIVE_MOVE
    (void)home_pos;
    Bhv_OffensiveHalfDefensiveMove().execute( agent );
#else
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doDefensiveMove" );

    agent->debugClient().addMessage( "DefMove" );

    //----------------------------------------------
    // tackle
    if ( Bhv_BasicTackle( 0.8, 90.0 ).execute( agent ) )
    {
        return;
    }

    const rcsc::WorldModel & wm = agent->world();

    // intercept
    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( self_min < 16 )
    {
        rcsc::Vector2D self_trap_pos
            = rcsc::inertia_n_step_point( wm.ball().pos(),
                                          wm.ball().vel(),
                                          self_min,
                                          rcsc::ServerParam::i().ballDecay() );
        bool enough_stamina = true;
        double estimated_consume
            = wm.self().playerType().getOneStepStaminaComsumption( rcsc::ServerParam::i() )
            * self_min;
        if ( wm.self().stamina() - estimated_consume < rcsc::ServerParam::i().recoverDecThrValue() )
        {
            enough_stamina = false;
        }

        if ( enough_stamina
             && opp_min < 3
             && ( home_pos.dist( self_trap_pos ) < 10.0
                  || ( home_pos.absY() < self_trap_pos.absY()
                       && home_pos.y * self_trap_pos.y > 0.0 ) // same side
                  || self_trap_pos.x < home_pos.x
                  )
             )
        {
            agent->debugClient().addMessage( "Intercept(1)" );
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
            agent->setNeckAction( new rcsc::Neck_TurnToBall() );
#else
            agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
#endif
            return;
        }
    }

    if ( self_min < 15
         && self_min < mate_min + 2
         && ! wm.existKickableTeammate() )
    {
        agent->debugClient().addMessage( "Intercept(2)" );
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


    rcsc::Vector2D target_point = home_pos;
//     if ( wm.existKickableTeammate()
//          || ( mate_min < 2 && opp_min > 2 )
//          )
//     {
//         target_point.x += 10.0;
//     }
// #if 1
//     else if ( home_pos.y * wm.ball().pos().y > 0.0 ) // same side
//     {
//         target_point.x = wm.ball().pos().x + 1.0;
//     }
// #endif
    Bhv_BasicMove( target_point ).execute( agent );
#endif
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleHCOffensiveHalf::doShootAreaMove( rcsc::PlayerAgent * agent,
                                    const rcsc::Vector2D & home_pos )
{double dash_power = 100.0;
    agent->debugClient().addMessage( "ShotAreaMove" );

    //----------------------------------------------
    // tackle
    if ( Bhv_BasicTackle( 0.8, 90.0 ).execute( agent ) )
    {
        return;
    }

    const rcsc::WorldModel & wm = agent->world();
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doShootAreaMove()" );

    if ( doShootAreaIntercept( agent, home_pos ) )
    {
        return;
    }

    if ( home_pos.x > 30.0
         && home_pos.absY() < 7.0 )
    {
        agent->debugClient().addMessage( "ShotSupport" );
        Bhv_AttackerOffensiveMove( home_pos, false ).execute( agent );
        return;
    }

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( self_min < mate_min )
    {
        agent->debugClient().addMessage( "GetBall" );
        rcsc::Body_Intercept2008().execute( agent );
#ifdef NO_USE_NECK_DEFAULT_INTERCEPT_NECK
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

    if ( wm.existKickableOpponent()
         && ( wm.teammatesFromBall().empty()
              || wm.teammatesFromBall().front()->distFromBall() > 2.0 )
         )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doShootAreaMove() ball owner is opponent. change to normal move" );
        Bhv_BasicMove( home_pos ).execute( agent );
        return;
    }

    // set dash power
    static bool S_recover_mode = false;
    //double dash_power;
    if ( wm.self().stamina()
         < rcsc::ServerParam::i().recoverDecThrValue() + 300.0 )
    {
        S_recover_mode = true;
    }
    else if ( wm.self().stamina()
              > rcsc::ServerParam::i().staminaMax() * 0.7 )
    {
        S_recover_mode = false;
    }

    // recover
    if ( S_recover_mode )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doShootAreaMove() recovering" );
      //  dash_power
        //    = wm.self().playerType().staminaIncMax() * wm.self().recovery()
          //  - 25.0; // preffered recover value
        //if ( dash_power < 0.0 ) dash_power = 0.0;
    }
    else if ( opp_min < self_min - 2
              && opp_min < mate_min - 5 )
    {
      //  dash_power = std::max( dash_power * 0.2,
        //                       wm.self().playerType().staminaIncMax() * 0.8 );
    }
    else
    {
    //    dash_power
      //      = wm.self().getSafetyDashPower( dash_power );
    }

    rcsc::Vector2D target_point = home_pos;
    if ( wm.self().pos().dist( home_pos ) < 8.0 )
    {
        const rcsc::PlayerObject * nearest_opp = wm.getOpponentNearestToSelf( 1 );
        // check opponent
        if ( nearest_opp
             && nearest_opp->pos().dist( home_pos ) < 3.0 )
        {
            if ( home_pos.y > 0.0 ) target_point.y -= 5.0;
            if ( home_pos.y < 0.0 ) target_point.y += 5.0;
        }
    }

    // move
    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().addMessage( "ShootMove%.0f", dash_power );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doShootAreaMove() chance area move to (%.1f, %.1f) power=%.1f",
                        target_point.x, target_point.y, dash_power );

    if ( ! rcsc::Body_GoToPoint( target_point, dist_thr, dash_power ).execute( agent ) )
    {
        // face to front or side
        rcsc::AngleDeg body_angle = 0.0;
        if ( wm.ball().angleFromSelf().abs() > 100.0 )
        {
            body_angle = ( wm.ball().pos().y > wm.self().pos().y
                           ? 90.0
                           : -90.0 );
        }
        rcsc::Body_TurnToAngle( body_angle ).execute( agent );
    }

    if ( wm.ball().distFromSelf() < rcsc::ServerParam::i().visibleDistance() )
    {
        agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
    }
    else
    {
        agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
RoleHCOffensiveHalf::doShootAreaIntercept( rcsc::PlayerAgent * agent,
                                         const rcsc::Vector2D & /*home_pos*/ )
{double dash_power = 100.0;
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
        if ( rcsc::Body_Intercept2008().execute( agent ) )
        {
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
            return true;
        }
    }


    return false;
}

void
RoleHCOffensiveHalf::doMiddleAreaKick( rcsc::PlayerAgent * agent )
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
                                    dash_power * 0.8,
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
                                dash_power * 0.5,
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

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleHCOffensiveHalf::doDribbleBlockMove( rcsc::PlayerAgent* agent,
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
    double dash_power;

    if ( wm.self().pos().x + 5.0 < wm.ball().pos().x )
    {
        if ( wm.self().stamina() < rcsc::ServerParam::i().staminaMax() * 0.7 )
        {
            dash_power
                = dash_power * 0.7
                * ( wm.self().stamina() / rcsc::ServerParam::i().staminaMax() );
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": doDribbleBlockMove() dash_power=%.1f",
                                dash_power );
        }
        else
        {
            dash_power
                = dash_power * 0.75
                - std::min( 30.0, wm.ball().distFromSelf() );
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": doDribbleBlockMove() dash_power=%.1f",
                                dash_power );
        }
    }
    else
    {
        dash_power
            = dash_power
            + 10.0
            - wm.ball().distFromSelf();
        dash_power = rcsc::min_max( 0.0, dash_power, dash_power );
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doDribbleBlockMove() dash_power=%.1f",
                            dash_power );
    }



    if ( wm.existKickableTeammate() )
    {
        dash_power = std::min( dash_power * 0.5,
                               dash_power );
    }

    dash_power = wm.self().getSafetyDashPower( dash_power );

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
RoleHCOffensiveHalf::doCrossBlockMove( rcsc::PlayerAgent* agent,
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

