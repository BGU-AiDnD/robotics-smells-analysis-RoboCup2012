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

#include "role_side_half.h"

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
#include "bhv_self_pass.h"

#include "body_kick_to_corner.h"
#include "body_kick_to_front_space.h"
#include "body_advance_ball_test.h"
#include "body_clear_ball2008.h"
#include "body_dribble2008.h"
#include "body_intercept2008.h"
#include "body_hold_ball2008.h"
#include "body_smart_kick.h"
#include "bhv_pass_test.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/bhv_shoot2008.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_pass.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_goalie_or_scan.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>

#include <rcsc/formation/formation.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/player_config.h>
#include <rcsc/player/free_message.h>
#include <rcsc/player/say_message_builder.h>

#include <rcsc/common/audio_codec.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/ray_2d.h>
#include <rcsc/soccer_math.h>

#include "bhv_offensive_half_defensive_move.h"
#include "bhv_offensive_half_offensive_move.h"
#include "bhv_block_ball_owner.h"

#include "neck_offensive_intercept_neck.h"

#include "bhv_fs_clear_ball.h"
#include "bhv_fs_shoot.h"
#include "bhv_fs_through_pass_kick.h"
#include "bhv_fs_through_pass_move.h"
#include "bhv_fs_mark_opponent_forward.h"

#include "bhv_shoot2009.h"


int RoleSideHalf::S_count_shot_area_hold = 0;

#define USE_BHV_OFFENSIVE_HALF_DEFENSIVE_MOVE
#define USE_BHV_OFFENSIVE_HALF_OFFENSIVE_MOVE



/*-------------------------------------------------------------------*/
/*!

*/
void
RoleSideHalf::execute( rcsc::PlayerAgent * agent )
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
RoleSideHalf::doKick( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();
    
    const double nearest_opp_dist
        = agent->world().getDistOpponentNearestToSelf( 5 );
        
    rcsc::Vector2D pass_point;
    rcsc::Vector2D dribble_target(50.0, std::min( wm.self().pos().absY(), rcsc::ServerParam::i().goalHalfWidth()*2/3 ));
    if(wm.self().pos().y < 0)
      dribble_target.y *= -1;

    //nearest opponent
    //const rcsc::PlayerObject *Opp = wm.getOpponentNearestToSelf(10);
    double opp_dist = wm.getDistOpponentNearestToSelf(10);
    //RaidAttack
    bool force = false;

    double opp_offside_x = 0;
    const rcsc::PlayerPtrCont::const_iterator 
      end = wm.opponentsFromSelf().end();
    for(rcsc::PlayerPtrCont::const_iterator 
	  it = wm.opponentsFromSelf().begin();
	it != end;
	++it)
      {
	if(opp_offside_x < (*it)->pos().x
	   && !(*it)->goalie())
	  {
	    opp_offside_x = (*it)->pos().x;
	  }
     }    

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
    case Strategy::BA_DefMidField:
    if(opp_dist<=5.0)
      {
        if ( Bhv_FSThroughPassKick().execute(agent) )
          break;
    	if(rcsc::Body_Pass::get_best_pass(wm,&pass_point,NULL,NULL))
	{
	 rcsc::Body_Pass().execute(agent);
	 agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
	 break;
	}
	       
	agent->debugClient().addMessage( "OffMid" );
        Bhv_BasicOffensiveKick().execute( agent );
        break;
       }
      if (rcsc::Body_Dribble2008(rcsc::Vector2D(52.0,dribble_target.y)
			       ,2.0,rcsc::ServerParam::i().maxDashPower()
			       ,1,true).execute( agent ) )
	{
	        agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan() );
		break; 
	}
	doOffensiveMiddleKick(agent);
	   
    	
    case Strategy::BA_OffMidField:
    case Strategy::BA_DribbleAttack:
      if(Bhv_FSThroughPassKick().execute(agent))
	break;

	doMiddleAreaKick( agent );
	//doOffensiveMiddleKick(agent);
        break;	
      //オフサイドラインを越えてたら、ゴールまで1直線
/*      if(wm.self().pos().x>opp_offside_x){
	if(!rcsc::Body_Dribble2008(dribble_target,4.0,
				   rcsc::ServerParam::i().maxDashPower(),5,
				   force).execute( agent )){
				  agent->setNeckAction(new rcsc::Neck_TurnToLowConfTeammate());
	}else{
	  agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
	  break;
	}
      }
      //---パスが出来るのであればパス---
      if(rcsc::Body_Pass::get_best_pass(wm, &pass_point, NULL, NULL)
	 &&opp_dist<=5.0)
	{
	  if( wm.self().pos().dist(pass_point) > 6.0
	      && (wm.self().pos().x - 4.0) < pass_point.x
	      && (rcsc::ServerParam::i().useOffside() 
		  && pass_point.x<wm.offsideLineX()
		  || !rcsc::ServerParam::i().useOffside())
	      && wm.countOpponentsIn(rcsc::Circle2D(pass_point,3.0)
				     ,7,true)<=1 )
	    {
	      rcsc::Body_Pass().execute(agent);
	      agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
	      break;
	    }
	}
      if(opp_dist<=5.0)
      {
      	force=true;//回避モード //パラメータ変更(真崎)
      	if(Bhv_FSThroughPassKick().execute(agent))
	break;
	
	if(rcsc::Body_Pass::get_best_pass(wm,&pass_point,NULL,NULL))
	       {
		 rcsc::Body_Pass().execute(agent);
		 agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
		 break;
	       }
	       
	agent->debugClient().addMessage( "OffMid" );
        Bhv_BasicOffensiveKick().execute( agent );
        break;
       }       
       
      if(wm.self().pos().x<25.0)
	{//---SideLine Dribble---
	  if(wm.self().pos().y> 0)dribble_target.y= 31.0;
	  if(wm.self().pos().y< 0)dribble_target.y=-31.0;
	}
      else
	{
	  dribble_target.y = wm.self().pos().y;
	}
      if(countKickableCycleAtSamePoint(agent)<5){
	//printf("%2d:kickable\n",countKickableCycleAtSamePoint(agent));
	rcsc::Body_Dribble2008(dribble_target
			       ,2.0,rcsc::ServerParam::i().maxDashPower()
			       ,3,force).execute( agent );
	agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );			       
      }else
      {
	//ドリブルできないときは、てきとークリア
	if(Bhv_FSThroughPassKick().execute(agent))
	break;
	
	agent->debugClient().addMessage( "OffMid" );
        Bhv_BasicOffensiveKick().execute( agent );
        break;
      }
      break;*/
    case Strategy::BA_Cross:{
      if(rcsc::Body_Pass::get_best_pass( wm, &pass_point, NULL, NULL)
	 && Strategy::get_ball_area(pass_point) 
	 == Strategy::BA_ShootChance
	 &&(rcsc::ServerParam::i().useOffside() 
	    && pass_point.x<wm.offsideLineX()
	    || !rcsc::ServerParam::i().useOffside()))
	{//PassPoint is in ShootChanceArea
	  rcsc::Body_Pass().execute(agent);
	  agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
	  break;
	}
	
	if( rcsc::Circle2D(rcsc::Vector2D(50,0),20).contains(wm.self().pos())
	  &&Bhv_FSShoot().execute(agent))
	break;
	
      if(Bhv_FSThroughPassKick().execute(agent))
	  break;
	  
      if(countKickableCycleAtSamePoint(agent)<5){
	rcsc::Body_Dribble2008(rcsc::Vector2D(52.0,dribble_target.y)
			       ,2.0,rcsc::ServerParam::i().maxDashPower()
			       ,1,true).execute( agent );
        agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan() );
	break;
      }else{
	if(rcsc::Body_Pass::get_best_pass( wm, &pass_point, NULL, NULL)){
	  rcsc::Body_Pass().execute(agent);
	  agent->setNeckAction(new rcsc::Neck_TurnToLowConfTeammate());
	  break;
	}
      }
      doCrossAreaKick( agent );
      break;
      
      doCrossAreaKick( agent );
        break;
/*      if( kickCentering( agent ) )
      {//センタリング
        break;
      }
      
      rcsc::Vector2D dribble_pos(52.0,0);
      if(wm.self().pos().x < -50) dribble_pos.y=wm.self().pos().y;
      
      rcsc::Body_Dribble2008(dribble_pos,1.0,
			     rcsc::ServerParam::i().maxDashPower(),
			     1,true).execute( agent );
      agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
      break;*/
    }
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
RoleSideHalf::doMove( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();
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
    if (wm.ball().pos().x > 15)
        Bhv_AttackerOffensiveMove( home_pos, true ).execute( agent );
    else doOffensiveMove( agent, home_pos );
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
RoleSideHalf::doDribbleAttackKick( rcsc::PlayerAgent * agent )
{
    agent->debugClient().addMessage( "DribAtt" );
    Bhv_BasicOffensiveKick().execute( agent );
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleSideHalf::doShootChanceKick( rcsc::PlayerAgent * agent )
{
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
RoleSideHalf::doOffensiveMove( rcsc::PlayerAgent * agent,
                                    const rcsc::Vector2D & home_pos )
{
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
            double dash_power = rcsc::ServerParam::i().maxPower();
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
RoleSideHalf::doDefensiveMove( rcsc::PlayerAgent * agent,
                                    const rcsc::Vector2D & home_pos )
{
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
RoleSideHalf::doShootAreaMove( rcsc::PlayerAgent * agent,
                                    const rcsc::Vector2D & home_pos )
{
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
    double dash_power;
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
        dash_power
            = wm.self().playerType().staminaIncMax() * wm.self().recovery()
            - 25.0; // preffered recover value
        if ( dash_power < 0.0 ) dash_power = 0.0;
    }
    else if ( opp_min < self_min - 2
              && opp_min < mate_min - 5 )
    {
        dash_power = std::max( rcsc::ServerParam::i().maxPower() * 0.2,
                               wm.self().playerType().staminaIncMax() * 0.8 );
    }
    else
    {
        dash_power
            = wm.self().getSafetyDashPower( rcsc::ServerParam::i().maxPower() );
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
RoleSideHalf::doShootAreaIntercept( rcsc::PlayerAgent * agent,
                                         const rcsc::Vector2D & /*home_pos*/ )
{
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

int
RoleSideHalf::countKickableCycleAtSamePoint(rcsc::PlayerAgent * agent){
  static int startCycle = 0;
  static int prevCycle = 0;
  static rcsc::Vector2D point(0,0);

  if(agent->world().self().isKickable()
     && point.dist(agent->world().ball().pos()) < 3.5
     && agent->world().time().cycle() - prevCycle <= 7){
    prevCycle = agent->world().time().cycle();
  }else{
    startCycle = agent->world().time().cycle();
    prevCycle = agent->world().time().cycle();
    point = agent->world().ball().pos();
  }
  //printf("start:%2d,prev:%2d(%.2lf,%.2lf)\n"
  // ,startCycle
  // ,prevCycle,point.x,point.y);
  return agent->world().time().cycle()-startCycle;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleSideHalf::doShootAreaKick( rcsc::PlayerAgent* agent )
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
                                        rcsc::ServerParam::i().maxPower(),
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
                                            rcsc::ServerParam::i().maxPower(),
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
                                    rcsc::ServerParam::i().maxPower() * 0.7,
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
                                rcsc::ServerParam::i().maxPower() * 0.7,
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
                                rcsc::ServerParam::i().maxPower(),
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
RoleSideHalf::doCheckCrossPoint( rcsc::PlayerAgent * agent )
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

//-----------------------------------------

void RoleSideHalf::doOffensiveMiddleKick( PlayerAgent * agent )
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
                                    rcsc::ServerParam::i().maxPower(),
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
                                    rcsc::ServerParam::i().maxPower(),
                                    5
                                    ).execute( agent );
        }
        else
        {
            agent->debugClient().addMessage( "MidDribSlow" );
            rcsc::Body_Dribble2008( drib_target,
                                    1.0,
                                    rcsc::ServerParam::i().maxPower() * 0.5,
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
                                rcsc::ServerParam::i().maxPower(),
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

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleSideHalf::doCrossAreaKick( rcsc::PlayerAgent* agent )
{
    agent->debugClient().addMessage( "CrossArea" );
    if ( Bhv_Cross::get_best_point( agent, NULL ) )
    {
        Bhv_Cross().execute( agent );
        return;
    }

    Bhv_BasicOffensiveKick().execute( agent );
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleSideHalf::doDribbleAttackAreaKick( rcsc::PlayerAgent* agent )
{
    // !!!check stamina!!!
    static bool S_recover_mode = false;

    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doDribbleAttackAreaKick" );

    const rcsc::WorldModel & wm = agent->world();

    if ( wm.self().stamina()
         < rcsc::ServerParam::i().recoverDecThrValue() + 200.0 )
    {
        S_recover_mode = true;
    }
    else if ( wm.self().stamina()
              > rcsc::ServerParam::i().staminaMax() * 0.6 )
    {
        S_recover_mode = false;
    }

    const rcsc::PlayerObject * nearest_opp = wm.getOpponentNearestToSelf( 10 );
    const double nearest_opp_dist = ( nearest_opp
                                      ? nearest_opp->distFromSelf()
                                      : 1000.0 );
    rcsc::Vector2D nearest_opp_pos( -1000.0, 0.0 );
    if ( nearest_opp ) nearest_opp_pos = nearest_opp->pos();

    const rcsc::Bhv_PassTest::PassRoute * pass
        = rcsc::Bhv_PassTest::get_best_pass( wm );

    if ( pass )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": exist pass point" );
        double opp_to_pass_dist = 100.0;
        wm.getOpponentNearestTo( pass->receive_point_, 10, &opp_to_pass_dist );

        if ( pass->receive_point_.x > 35.0
             && opp_to_pass_dist > 10.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": very challenging pass" );
            rcsc::Bhv_PassTest().execute( agent );
            return;
        }


        if ( nearest_opp
             && nearest_opp_pos.x > wm.self().pos().x - 1.0
             && nearest_opp->angleFromSelf().abs() < 90.0
             && nearest_opp_dist < 5.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": dribble attack. pass to avoid front opp" );
            rcsc::Bhv_PassTest().execute( agent );
            return;
        }

        if ( nearest_opp_pos.x > wm.self().pos().x - 0.5
             && nearest_opp_dist < 5.8
             && pass->receive_point_.x > wm.self().pos().x + 3.0
             )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": dribble attack. keep away & attack pass" );
            rcsc::Bhv_PassTest().execute( agent );
            return;
        }

        if ( nearest_opp_pos.x > wm.self().pos().x - 1.0
             && nearest_opp_dist < 5.0
             && ( pass->receive_point_.x > 20.0
                  || pass->receive_point_.x > wm.self().pos().x - 3.0 )
             )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": dribble attack(2). keep away & attack pass" );
            rcsc::Bhv_PassTest().execute( agent );
            return;
        }

        if ( S_recover_mode
             && ( pass->receive_point_.x > 30.0
                  || pass->receive_point_.x > wm.self().pos().x - 3.0 )
             )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": dribble attack. pass for stamina recover" );
            rcsc::Bhv_PassTest().execute( agent );
            return;
        }

        //if ( pass_point.x > wm.self().pos().x )
        if ( pass->receive_point_.x > wm.self().pos().x - 3.0
             && opp_to_pass_dist > 8.0 )
        {
            const rcsc::Sector2D sector( wm.self().pos(),
                                         0.5, 8.0,
                                         -30.0, 30.0 );
            // opponent check with goalie
            if ( wm.existOpponentIn( sector, 10, true ) )
            {
                rcsc::dlog.addText( rcsc::Logger::ROLE,
                                    __FILE__": dribble attack. forward pass to avoid opp" );
                rcsc::Bhv_PassTest().execute( agent );
                return;
            }
        }
    }

#if 0
    for ( int dash_step = 16; dash_step >= 6; dash_step -= 2 )
    {
        if ( doSelfPass( agent, dash_step ) )
        {
            return;
        }
    }
#else
    if ( Bhv_SelfPass().execute( agent ) )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doDribbleAttackAreaKick SelfPass" );
        return;
    }
#endif

    if ( doStraightDribble( agent ) )
    {
        return;
    }

    // recover mode
    if ( S_recover_mode )
    {
        if ( nearest_opp_dist > 5.0 )
        {
            rcsc::Body_HoldBall2008( true, rcsc::Vector2D(36.0, wm.self().pos().y * 0.9 )//0.0)
                                     ).execute( agent );
            agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
            return;
        }
        else if ( rcsc::Bhv_PassTest().execute( agent ) )
        {
            return;
        }
        else
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": dribble attack. exhaust kick to corner" );
            Body_KickToCorner( ( wm.self().pos().y < 0.0 )
                               ).execute( agent );
            agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
            return;
        }
        return;
    }

    if ( nearest_opp_dist < rcsc::ServerParam::i().defaultPlayerSize() * 2.0 + 0.2
         || wm.existKickableOpponent() )
    {
        if ( rcsc::Bhv_PassTest().execute( agent ) )
        {
            return;
        }

        if ( wm.self().pos().x < 25.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": dribble attack. kick to near side" );
            Body_KickToCorner( ( wm.self().pos().y < 0.0 )
                               ).execute( agent );
        }
        else if ( wm.self().pos().x < 35.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": dribble attack. kick to space for keep away" );
            Body_KickToFrontSpace().execute( agent );
        }
        else
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": dribble attack. opponent very near. cross to center." );
            Bhv_Cross().execute( agent );
            return;
        }
        agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
        return;
    }


    if ( nearest_opp
         && wm.self().pos().x > 20.0
         && nearest_opp->distFromSelf() < 1.0
         && nearest_opp->pos().x > wm.self().pos().x
         && std::fabs( nearest_opp->pos().y - wm.self().pos().y ) < 0.9 )
    {
        Body_KickToCorner( ( wm.self().pos().y < 0.0 )
                           ).execute( agent );

        agent->setNeckAction( new rcsc::Neck_ScanField() );
        agent->debugClient().addMessage( "SF:BaseKickAvoidOpp" );
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": dribble attack. kick to near side to avoid opponent" );
        return;
    }

    // wing dribble
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doDribbleAttackAreaKick. dribble attack. normal dribble" );
    doSideForwardDribble( agent );
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
RoleSideHalf::doStraightDribble( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();

    // dribble straight
    if ( wm.self().pos().x > 36.0 )
    {
        return false;
    }

    if ( wm.self().pos().absY() > rcsc::ServerParam::i().pitchHalfWidth() - 2.0 )
    {
        if ( wm.self().pos().y < 0.0 && wm.self().body().degree() < 1.5 )
        {
            return false;
        }
        if ( wm.self().pos().y > 0.0 && wm.self().body().degree() > -1.5 )
        {
            return false;
        }
    }

    const rcsc::PlayerObject * nearest_opp = wm.getOpponentNearestToSelf( 10 );
    if ( nearest_opp
         && nearest_opp->pos().x > wm.self().pos().x + 0.5
         && std::fabs( nearest_opp->pos().y -  wm.self().pos().y ) < 2.0
         && nearest_opp->distFromSelf() < 2.0
         )
    {
        return false;
    }

    const rcsc::Rect2D target_rect
        = rcsc::Rect2D::from_center( wm.ball().pos().x + 5.5,
                                     wm.ball().pos().y,
                                     10.0, 9.0 );
    if ( wm.existOpponentIn( target_rect, 10, false ) )
    {
        return false;
    }

    const rcsc::Rect2D safety_rect
        = rcsc::Rect2D::from_center( wm.ball().pos().x + 6.5,
                                     wm.ball().pos().y,
                                     13.0, 13.0 );
    //int dash_count = 3;
    int dash_count = 10;
    if ( wm.existOpponentIn( safety_rect, 10, false ) )
    {
        //dash_count = 1;
        dash_count = 8;
    }

    rcsc::Vector2D drib_target( 47.0, wm.self().pos().y );
    if ( wm.self().body().abs() < 20.0 )
    {
        drib_target
            = wm.self().pos()
            + rcsc::Vector2D::polar2vector( 10.0, wm.self().body() );
    }

    agent->debugClient().addMessage( "StraightDrib" );
    agent->debugClient().addRectangle( target_rect );

    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": dribble straight to (%.1f %.1f) dash_count = %d",
                        drib_target.x, drib_target.y,
                        dash_count );
    rcsc::Body_Dribble2008( drib_target,
                            1.0,
                            rcsc::ServerParam::i().maxPower(),
                            dash_count,
                            false // no dodge
                            ).execute( agent );

    const rcsc::PlayerObject * opponent = static_cast< rcsc::PlayerObject * >( 0 );

    const rcsc::PlayerPtrCont::const_iterator end = wm.opponentsFromSelf().end();
    for ( rcsc::PlayerPtrCont::const_iterator it = wm.opponentsFromSelf().begin();
          it != end;
          ++it )
    {
        if ( (*it)->distFromSelf() > 10.0 ) break;
        if ( (*it)->posCount() > 5 ) continue;
        if ( (*it)->posCount() == 0 ) continue;
        if ( (*it)->angleFromSelf().abs() > 90.0 ) continue;
        if ( wm.dirCount( (*it)->angleFromSelf() ) < (*it)->posCount() ) continue;

        // TODO: check the next visible range

        opponent = *it;
        break;
    }

    if ( opponent )
    {
        agent->debugClient().addMessage( "LookOpp" );
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": dribble straight look opp %d (%.1f %.1f)",
                            opponent->unum(),
                            opponent->pos().x, opponent->pos().y );
        agent->setNeckAction( new rcsc::Neck_TurnToPoint( opponent->pos() ) );
    }
    else
    {
        agent->setNeckAction( new rcsc::Neck_ScanField() );
    }
    return true;

}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleSideHalf::doSideForwardDribble( rcsc::PlayerAgent * agent )
{
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doSideForwardDribble" );

    const rcsc::Vector2D drib_target = getDribbleTarget( agent );
    rcsc::AngleDeg target_angle = ( drib_target - agent->world().self().pos() ).th();

    // decide dash count & looking point
    int dash_count = getDribbleAttackDashStep( agent, target_angle );

    bool dodge = true;
    if ( agent->world().self().pos().x > 43.0
         && agent->world().self().pos().absY() < 20.0 )
    {
        dodge = false;
    }

    agent->debugClient().setTarget( drib_target );
    agent->debugClient().addMessage( "DribAtt%d", dash_count );

    rcsc::Body_Dribble2008( drib_target,
                            1.0,
                            rcsc::ServerParam::i().maxPower(),
                            dash_count,
                            dodge
                            ).execute( agent );

    if ( agent->world().dirCount( target_angle ) > 1 )
    {
        target_angle
            = ( drib_target - agent->effector().queuedNextMyPos() ).th();
        rcsc::AngleDeg next_body = agent->effector().queuedNextMyBody();
        if ( ( target_angle - next_body ).abs() < 90.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": SB Dribble LookDribTarget" );
            agent->debugClient().addMessage( "LookDribTarget" );
            agent->setNeckAction( new rcsc::Neck_TurnToPoint( drib_target ) );
            return;
        }
    }

    agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
}
/*-------------------------------------------------------------------*/
/*!

*/
rcsc::Vector2D
RoleSideHalf::getDribbleTarget( rcsc::PlayerAgent * agent )
{
    const double base_x = 47.5; //50.0; // old 45.0
    const double second_x = 40.0;
    const rcsc::WorldModel & wm = agent->world();

    rcsc::Vector2D drib_target( rcsc::Vector2D::INVALIDATED );

    bool goalie_near = false;
    //--------------------------------------------------
    // goalie check
    const rcsc::PlayerObject * opp_goalie = agent->world().getOpponentGoalie();
    if ( opp_goalie
         && opp_goalie->pos().x > rcsc::ServerParam::i().theirPenaltyAreaLineX()
         && opp_goalie->pos().absY() < rcsc::ServerParam::i().penaltyAreaHalfWidth()
         && ( opp_goalie->distFromSelf()
              < rcsc::ServerParam::i().catchAreaLength()
              + rcsc::ServerParam::i().defaultPlayerSpeedMax() * 3.5 )
         )
    {
        goalie_near = true;
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": dribble. goalie close" );
    }

    //--------------------------------------------------
    // goalie is close
    if ( goalie_near
         && std::fabs( opp_goalie->pos().y - wm.self().pos().y ) < 3.0 )
    {
        drib_target.assign( base_x, 25.0 );
        if ( wm.self().pos().y < 0.0 ) drib_target.y *= -1.0;

        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": getDribbleTarget. goalie colse. normal target" );

        return drib_target;
    }

    // goalie is safety
    const rcsc::PlayerObject * nearest_opp = wm.getOpponentNearestToSelf( 6 );

    if ( nearest_opp
         && nearest_opp->distFromSelf() < 2.0
         && wm.self().body().abs() < 40.0 )
    {
        rcsc::Vector2D tmp_target
            = wm.self().pos()
            + rcsc::Vector2D::polar2vector( 5.0, wm.self().body() );
        if ( tmp_target.x < 50.0 )
        {
            rcsc::Sector2D body_sector( wm.self().pos(),
                                        0.5, 10.0,
                                        wm.self().body() - 30.0,
                                        wm.self().body() + 30.0 );
            if ( ! wm.existOpponentIn( body_sector, 10, true ) )
            {
                rcsc::dlog.addText( rcsc::Logger::ROLE,
                                    __FILE__": getDribbleTarget. opp near. dribble to my body dir" );
                return tmp_target;
            }
        }
    }


    // cross area
    if ( wm.self().pos().x > second_x )
    {
        rcsc::Vector2D rect_center( ( wm.self().pos().x + base_x ) * 0.5,
                                    wm.self().pos().y < 0.0
                                    ? wm.self().pos().y + 4.0
                                    : wm.self().pos().y - 4.0 );
        rcsc::Rect2D side_rect
            = rcsc::Rect2D::from_center( rect_center, 6.0, 8.0 );
        agent->debugClient().addRectangle( side_rect );

        if ( wm.countOpponentsIn( side_rect, 10, false ) <= 1 )
        {
            drib_target.assign( base_x, 12.0 );
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": getDribbleTarget. cut in (%.1f, %.1f)",
                                drib_target.x, drib_target.y );
        }
        else
        {
            drib_target.assign( second_x,
                                rcsc::min_max( 15.0,
                                               wm.self().pos().absY() - 1.0,
                                               25.0 ) );
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": getDribbleTarget. keep away (%.1f, %.1f)",
                                drib_target.x, drib_target.y );
        }

        if ( Strategy::i().getPositionType( wm.self().unum() ) == Position_Left )
        {
            drib_target.y *= -1.0;
        }


        if ( opp_goalie )
        {
            rcsc::Vector2D drib_rel = drib_target - wm.self().pos();
            rcsc::AngleDeg drib_angle = drib_rel.th();
            rcsc::Line2D drib_line( wm.self().pos(), drib_target );
            if ( ( opp_goalie->angleFromSelf() - drib_angle ).abs() < 90.0
                 && drib_line.dist( opp_goalie->pos() ) < 4.0
                 )
            {
                double drib_dist = std::max( 0.0, opp_goalie->distFromSelf() - 5.0 );
                drib_target
                    = wm.self().pos()
                    + drib_rel.setLengthVector( drib_dist );

                rcsc::dlog.addText( rcsc::Logger::ROLE,
                                    __FILE__": getDribbleTarget. attend goalie. dist=%.2f (%.1f, %.1f)",
                                    drib_dist,
                                    drib_target.x, drib_target.y );
            }
        }

        return drib_target;
    }

    if ( wm.self().pos().x > 36.0 )
    {
        if ( wm.self().pos().absY() < 5.0 )
        {
            drib_target.assign( base_x, 22.0 );
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": getDribbleTarget. keep away to (%.1f, %.1f)",
                                drib_target.x, drib_target.y );
        }
        else if ( wm.self().pos().absY() > 19.0 )
        {
            drib_target.assign( base_x, 12.0 );
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": getDribbleTarget. go their penalty area (%.1f, %.1f)",
                                drib_target.x, drib_target.y );
        }
        else
        {
            drib_target.assign( base_x, wm.self().pos().absY() );
            if ( drib_target.y > 30.0 ) drib_target.y = 30.0;
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": getDribbleTarget. go my Y (%.1f, %.1f)",
                                drib_target.x, drib_target.y );
        }

        if ( Strategy::i().getPositionType( wm.self().unum() ) == Position_Left )
        {
            drib_target.y *= -1.0;
        }

        return drib_target;
    }

    drib_target.assign( base_x, wm.self().pos().absY() );
    if ( drib_target.y > 30.0 ) drib_target.y = 30.0;

    if ( Strategy::i().getPositionType( wm.self().unum() ) == Position_Left )
    {
        drib_target.y *= -1.0;
    }

    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": getDribbleTarget. normal target (%.1f, %.1f)",
                        drib_target.x, drib_target.y );

    return drib_target;
}

/*-------------------------------------------------------------------*/
/*!

*/
int
RoleSideHalf::getDribbleAttackDashStep( const rcsc::PlayerAgent * agent,
                                           const rcsc::AngleDeg & target_angle )
{
    bool goalie_near = false;
    //--------------------------------------------------
    // goalie check
    const rcsc::PlayerObject * opp_goalie = agent->world().getOpponentGoalie();
    if ( opp_goalie
         && opp_goalie->pos().x > rcsc::ServerParam::i().theirPenaltyAreaLineX()
         && opp_goalie->pos().absY() < rcsc::ServerParam::i().penaltyAreaHalfWidth()
         && ( opp_goalie->distFromSelf()
              < rcsc::ServerParam::i().catchAreaLength()
              + rcsc::ServerParam::i().defaultPlayerSpeedMax() * 3.5 )
         )
    {
        goalie_near = true;
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": dribble. goalie close" );
    }

    const rcsc::Sector2D target_sector( agent->world().self().pos(),
                                        0.5, 10.0,
                                        target_angle - 30.0,
                                        target_angle + 30.0 );
    const int default_step = 10;
    double dash_dist = 4.0;
    try {
        dash_dist = agent->world().self().playerType()
            .dashDistanceTable().at( default_step );
        dash_dist += agent->world().self().playerType()
            .inertiaTravel( agent->world().self().vel(), default_step ).r();
    }
    catch ( ... )
    {
        std::cerr << __FILE__ << ": " << __LINE__
                  << " Exception caught" << std::endl;
    }

    rcsc::Vector2D next_point
        = rcsc::Vector2D::polar2vector( dash_dist, target_angle );
    next_point += agent->world().self().pos();
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": getDribbleAttackDashStep. my_dash_dsit= %.2f  point(%.1f %.1f)",
                        dash_dist, next_point.x, next_point.y  );

    const rcsc::PlayerPtrCont & opps = agent->world().opponentsFromSelf();
    const rcsc::PlayerPtrCont::const_iterator opps_end = opps.end();
    bool exist_opp = false;
    for ( rcsc::PlayerPtrCont::const_iterator it = opps.begin();
          it != opps_end;
          ++it )
    {
        if ( (*it)->posCount() >= 10 )
        {
            continue;
        }
        if ( ( (*it)->angleFromSelf() - target_angle ).abs() > 120.0 )
        {
            continue;
        }

        if ( (*it)->distFromSelf() > 10.0 )
        {
            break;
        }
        if ( target_sector.contains( (*it)->pos() ) )
        {
            exist_opp = true;
            break;
        }
    }

    if ( exist_opp )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": getDribbleAttackDashStep. exist opp" );
        return 6;
    }

    if ( agent->world().self().pos().x > 36.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": getDribbleAttackDashStep. x > 36" );
        return 8;
        //return 3;
        //return 2;
    }

    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": getDribbleAttackDashStep. default" );
    return 10;
    //return 3;
    //return 2;
}

bool
RoleSideHalf::kickCentering( rcsc::PlayerAgent * agent )
{
  //センタリング 090704
  const rcsc::WorldModel & wm = agent->world();
  if ( !agent->config().useCommunication() )
    return false;

  rcsc::Circle2D checkCircle(rcsc::ServerParam::i().theirTeamGoalPos(),15);
  if( wm.countTeammatesIn(checkCircle,3,false)==0
      //|| wm.countOpponentsIn(checkCircle,3,false>3)
    )
    return false;
  
  double dist;
  const rcsc::PlayerObject *mate_front = wm.getTeammateNearestTo(rcsc::ServerParam::i().theirTeamGoalPos(),5,&dist);
  if(mate_front==NULL)
    return false;

  rcsc::Vector2D target_pos = mate_front->pos();
  double min_dist_from_segment=0;
  double best_target_pos_x = 0;
  
  for(int i=mate_front->pos().x + 3;i>mate_front->pos().x;i--)
    {
      double dist_from_segment=99;
      
      rcsc::Segment2D centering_course(wm.ball().pos(),target_pos);
      const rcsc::PlayerPtrCont::const_iterator end = wm.opponentsFromSelf().end();
      for(rcsc::PlayerPtrCont::const_iterator 
	    it = wm.opponentsFromSelf().begin();
	  it != end;
	  ++it)
	{
          double tmp_dist = centering_course.dist((*it)->pos()) + wm.self().pos().dist((*it)->pos())*0.1;
	  if ( tmp_dist < dist_from_segment ){
	    dist_from_segment = tmp_dist;
	  }
	}
      if( dist_from_segment > min_dist_from_segment )
	{
	  min_dist_from_segment = dist_from_segment;
	  best_target_pos_x = i;
	}
    }
  
  //あまりにも無謀
  if(min_dist_from_segment < 0.5) //tune this.超簡易
    return false;

  target_pos.x = best_target_pos_x;

  const double first_speed = rcsc::ServerParam::i().ballSpeedMax();
  if(rcsc::Body_SmartKick(target_pos
			  ,first_speed
			  ,first_speed * 0.96
			  ,2
			  ).execute(agent))
    {
      std::string pos_message =
	rcsc::AudioCodec::i().encodePosToStr4(target_pos); 
      
      agent->addSayMessage( new rcsc::FreeMessage<6>("KC" + pos_message) );
      agent->setNeckAction(new rcsc::Neck_ScanField());
    std::cout << agent->world().self().unum()
	      << ":[" << agent->world().time().cycle()
	      << "] KickCentering(kick):(" 
	      << target_pos.x << "," << target_pos.y 
	      << ")" << std::endl;
      return true;
    }
    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleSideHalf::doMiddleAreaKick( rcsc::PlayerAgent * agent )
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
                                            rcsc::ServerParam::i().maxPower(),
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
                                    rcsc::ServerParam::i().maxPower(),
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
                                    rcsc::ServerParam::i().maxPower(),
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
                                    rcsc::ServerParam::i().maxPower(),
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
                                    rcsc::ServerParam::i().maxPower() * 0.8,
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
                                rcsc::ServerParam::i().maxPower() * 0.5,
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
                                            rcsc::ServerParam::i().maxPower(),
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
