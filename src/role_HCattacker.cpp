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

#include "role_HCattacker.h"


#include "strategy.h"

#include "bhv_cross.h"
#include "bhv_basic_offensive_kick.h"
#include "bhv_keep_shoot_chance.h"
#include "body_kick_to_corner.h"
#include "bhv_basic_tackle.h"
#include "bhv_basic_move.h"
#include "bhv_attacker_offensive_move.h"
#include "bhv_self_pass.h"
#include "bhv_pass_test.h"
#include "bhv_basic_defensive_kick.h"
#include "bhv_basic_offensive_kick.h"

#include "body_advance_ball_test.h"
#include "body_clear_ball2008.h"
#include "body_dribble2008.h"
#include "body_intercept2008.h"
#include "body_hold_ball2008.h"
#include "body_smart_kick.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/bhv_shoot2008.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/body_intercept2009.h>
#include <rcsc/action/body_dribble.h>
#include <rcsc/action/body_pass.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_goalie_or_scan.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>
#include <rcsc/formation/formation.h>
#include <rcsc/action/view_wide.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/audio_codec.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/sector_2d.h>
#include <rcsc/geom/rect_2d.h>
#include <rcsc/math_util.h>

#include <rcsc/action/body_advance_ball.h>
#include <rcsc/action/body_go_to_point.h>
//#include <rcsc/action/body_smart_kick.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_ball.h>

#include <boost/tokenizer.hpp>

#include "bhv_fs_clear_ball.h"
#include "bhv_fs_shoot.h"
#include "bhv_nutmeg_kick.h"
#include "bhv_fs_through_pass_move.h"
#include "bhv_fs_through_pass_kick.h"

#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

#include "bhv_shoot2009.h"

int RoleHCAttacker::S_count_shot_area_hold = 0;

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleHCAttacker::execute( rcsc::PlayerAgent * agent )
{
    bool kickable = agent->world().self().isKickable();
    if ( agent->world().existKickableTeammate()
         && agent->world().teammatesFromBall().front()->distFromBall()
         < agent->world().ball().distFromSelf() )
    {
        kickable = false;
    }
	
    if ( kickable )
    {
        doKick( agent );
    }
    else
    {
        doMove( agent );
    }

}

/*-------------------------------------------------------------------*/
/*!

*/

void
RoleHCAttacker::doKick( rcsc::PlayerAgent * agent )
{
    double dash_power = 100.0;

    const rcsc::WorldModel & wm = agent->world();
    const rcsc::PlayerObject *opp = wm.getOpponentNearestToSelf(5);
    
    //nearest opponent
    //const rcsc::PlayerObject *Opp = wm.getOpponentNearestToSelf(10);
    double opp_dist = wm.getDistOpponentNearestToSelf(10);
    //const rcsc::PlayerObject *goalie = wm.getOpponentGoalie();
    rcsc::Vector2D pass_point;
    rcsc::Vector2D dribble_target(50.0, std::min( wm.self().pos().absY(), rcsc::ServerParam::i().goalHalfWidth()*2/3 ));
    if(wm.self().pos().y < 0)
      dribble_target.y *= -1;
    rcsc::AngleDeg dribble_angle = (dribble_target - wm.self().pos()).th();
    rcsc::Sector2D checkSector( wm.self().pos()
		     ,0.1,7.0	//r=0.1~7
		     ,dribble_angle-15,dribble_angle+15);

    //if(Bhv_NutmegKick().execute(agent))
    //	return;
    switch ( Strategy::get_ball_area( agent->world().ball().pos() ) ) {
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
        
      if(opp_dist<=2.5 && Bhv_KeepShootChance().execute( agent ))
        break;
       
/*      if (rcsc::Body_Dribble2008(dribble_target,2.0,
	  		       rcsc::ServerParam::i().maxDashPower(),2,false
			       ).execute( agent ))
      {			       
      	agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan() );
      	break;
      }*/
      
      doShootAreaKick( agent );
        break;

    case Strategy::BA_Cross:
      //-----クロス時-----
      if( rcsc::Circle2D(rcsc::Vector2D(50,0),20).contains(wm.self().pos())
	  &&Bhv_FSShoot().execute(agent))
	break;

      if(countKickableCycleAtSamePoint(agent)<5){
	rcsc::Body_Dribble2008(rcsc::Vector2D(52.0,dribble_target.y)
			       ,2.0,dash_power
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
      Bhv_BasicOffensiveKick().execute( agent );
      break;
      //--------------------------------------------------------
    case Strategy::BA_DribbleAttack:
    case Strategy::BA_OffMidField:
    case Strategy::BA_DefMidField:
    {
      if( rcsc::Circle2D(rcsc::Vector2D(50,0),20).contains(wm.self().pos())
	 &&Bhv_FSShoot().execute(agent))
	  break;

      //セクター内に敵がいるか判定
      if ( wm.countOpponentsIn(checkSector,3,true) == 0 )
       {
        rcsc::Body_Dribble2008(dribble_target,3.0,
			       dash_power,5,false
			       ).execute( agent );
	agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan() );
	break;
       }
/*      else if( wm.countOpponentsIn(checkSector,3,true) <=1 )
       {
        rcsc::Body_Dribble2008(dribble_target,5.0,
			       rcsc::ServerParam::i().maxDashPower(),2,true
			       ).execute( agent );
	agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan() );
	break;
       }*/
        if(Bhv_FSThroughPassKick().execute(agent))
	  break;

      if(opp!=NULL && (wm.self().pos().dist(opp->pos())<2.5) &&
	 (wm.self().pos().x + 1 < opp->pos().x) )
	{
	  //---Pass---
	  if(rcsc::Body_Pass::get_best_pass(wm,&pass_point,NULL,NULL))
	    {
	      if(rcsc::ServerParam::i().useOffside() 
		     && pass_point.x<wm.offsideLineX()
		     || !rcsc::ServerParam::i().useOffside()
		     && wm.countOpponentsIn(rcsc::Circle2D(pass_point,2.5)
					,7,true) <=1)
		{
		  rcsc::Body_Pass().execute(agent);
		  agent->setNeckAction(new rcsc::Neck_TurnToLowConfTeammate());
		  break;
		}
	    }
	  if(wm.self().pos().x<28.0)
	    {//---適当キック（for Corner）---
	      rcsc::Vector2D target_point(47.0,32.0);
	      if(wm.self().pos().y<0)
		target_point *= -1;
	      double first_speed = rcsc::ServerParam::i().ballSpeedMax();
	      double thr = 0.96;
	      int step = 2;
	      double end_speed = 0.35;
	      double target_dist = target_point.dist(wm.ball().pos());

	      if(!wm.existOpponentIn(rcsc::Circle2D(target_point,7)
				     ,7,true)
		 && !wm.existOpponentIn(rcsc::Sector2D(wm.self().pos()
						       ,target_point.dist(wm.self().pos())*0.1
						       ,target_point.dist(wm.self().pos())
						       ,(target_point - wm.self().pos()).dir() - 7
						       ,(target_point - wm.self().pos()).dir() + 7)
					,7,false)
		 )
		end_speed = 0.1;
	      first_speed
		= rcsc::calc_first_term_geom_series_last
		( end_speed,
		  target_dist,
		  rcsc::ServerParam::i().ballDecay() );
	      		
	      first_speed = std::min(first_speed
				     ,rcsc::ServerParam::i().ballSpeedMax());
	      rcsc::Body_SmartKick(target_point,
				   first_speed,
				   first_speed * thr,
				   step).execute(agent);
	    }
	  else
	    {
		Bhv_BasicOffensiveKick().execute( agent );
		break;
	    }
	  agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
	  break;
	}
      else
	{
	   if(countKickableCycleAtSamePoint(agent)<5){
	     //printf("%2d:kickable\n",countKickableCycleAtSamePoint(agent));
	     rcsc::Body_Dribble2008(rcsc::Vector2D(47.0, wm.self().pos().y)
				    ,3.0,dash_power
				    ,3,true).execute( agent );
             agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
	     break;
	   }else{
	     if(rcsc::Body_Pass::get_best_pass(wm,&pass_point,NULL,NULL))
	       {
		 rcsc::Body_Pass().execute(agent);
		 agent->setNeckAction(new rcsc::Neck_TurnToLowConfTeammate());
		 break;
	       }
	   }
	}
      doOffensiveMiddleKick( agent );
	break;;
    }
    case Strategy::BA_CrossBlock:
    case Strategy::BA_Stopper:
    	Body_AdvanceBallTest().execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
        break;
    case Strategy::BA_Danger:
        Body_ClearBall2008().execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
        break;
    case Strategy::BA_DribbleBlock:
    if ( Bhv_SelfPass().execute( agent ) )
        {
            return;
        }
    else if(rcsc::Body_Pass::get_best_pass(wm,&pass_point,NULL,NULL))
	{
	 rcsc::Body_Pass().execute(agent);
	 agent->setNeckAction(new rcsc::Neck_TurnToLowConfTeammate());
	 break;
	}
    else if(Bhv_FSThroughPassKick().execute(agent))
	  break;
    else 
    {
    	Bhv_BasicOffensiveKick().execute( agent );
        break;
    }	
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
RoleHCAttacker::doMove( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();

    int ball_reach_step = 0;
    if ( ! wm.existKickableTeammate() && ! wm.existKickableOpponent())
    {
        ball_reach_step
            = std::min( wm.interceptTable()->teammateReachCycle(),
                        wm.interceptTable()->opponentReachCycle() );
    }
    const rcsc::Vector2D base_pos
        = wm.ball().inertiaPoint( ball_reach_step );

    rcsc::Vector2D home_pos = Strategy::i().getPosition( agent->world().self().unum() );
    if ( rcsc::ServerParam::i().useOffside() )
    {
        home_pos.x = std::min( home_pos.x, wm.offsideLineX() - 1.0 );
    }
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        "%s: HOME POSITION=(%.2f, %.2f) base_point(%.1f %.1f)"
                        ,__FILE__,
                        home_pos.x, home_pos.y,
                        base_pos.x, base_pos.y );

    //各種サイクルの取得
    int self_cycle = wm.interceptTable()->selfReachCycle();
    int mate_cycle = wm.interceptTable()->teammateReachCycle();
    int opp_cycle = wm.interceptTable()->opponentReachCycle();
    int min_cycle = std::min ( self_cycle, std::min( mate_cycle, opp_cycle ) );

    /*//tries to have clear sight of the ball

    if(wm.existKickableTeammate() && wm.self().pos().dist(wm.ball().pos()) < 20.0)      //if our team has ball possession
    {
        rcsc::Rect2D clearArea = rcsc::Rect2D(wm.self().pos(),wm.ball().pos());

        if(wm.existOpponentIn(clearArea,1,false)) {         //if there is an opponent
            rcsc::Triangle2D newArea[2] = {rcsc::Triangle2D(wm.ball().pos(),
                                                              rcsc::Vector2D(wm.offsideLineX(),wm.self().pos().y-3),
                                                              rcsc::Vector2D(wm.offsideLineX(),wm.self().pos().y-13)),
                                           rcsc::Triangle2D(wm.ball().pos(),
                                                              rcsc::Vector2D(wm.offsideLineX(),wm.self().pos().y+3),
                                                              rcsc::Vector2D(wm.offsideLineX(),wm.self().pos().y+13)) };

            rcsc::Rect2D upArea = rcsc::Rect2D(rcsc::Vector2D(wm.self().pos().x,wm.self().pos().y+10),wm.ball().pos());
            rcsc::Rect2D downArea = rcsc::Rect2D(rcsc::Vector2D(wm.self().pos().x,wm.self().pos().y-10),wm.ball().pos());

            //vars to doGoToPoint function
            rcsc::Vector2D target_point;
            double dist_thr = wm.ball().distFromSelf() * 0.1;
            dash_power = rcsc::ServerParam::i().maxPower();

            //trying to find clear area
            if(!wm.existOpponentIn(newArea[0],1,false)) {
                target_point.x = wm.offsideLineX();
                target_point.y = wm.self().pos().y-13;
                doGoToPoint(agent, target_point, dist_thr, dash_power, 15.0 );
                agent->setNeckAction( new rcsc::Neck_TurnToBall() );
            }
            else if(!wm.existOpponentIn(newArea[0],1,false)) {
                target_point.x = wm.offsideLineX();
                target_point.y = wm.self().pos().y+13;
                doGoToPoint(agent, target_point, dist_thr, dash_power, 15.0 );
                agent->setNeckAction( new rcsc::Neck_TurnToBall() );
            }
            else if(!wm.existOpponentIn(upArea,1,false)) {
                target_point.x = wm.self().pos().x;
                target_point.y = wm.self().pos().y+13;
                doGoToPoint(agent, target_point, dist_thr, dash_power, 15.0 );
                agent->setNeckAction( new rcsc::Neck_TurnToBall() );
            }
            else if(!wm.existOpponentIn(downArea,1,false)) {
                target_point.x = wm.self().pos().x;
                target_point.y = wm.self().pos().y-13;
                doGoToPoint(agent, target_point, dist_thr, dash_power, 15.0 );
                agent->setNeckAction( new rcsc::Neck_TurnToBall() );
            }
            else {
                //if there's no solution, choose random position and move
                srand(time(NULL));
                int randFactor = rand()%2;
                if(rand){
                    target_point.x = wm.offsideLineX();
                    target_point.y = wm.self().pos().y + wm.theirPlayer(1)->pos().y;
                    doGoToPoint(agent, target_point, dist_thr, dash_power, 15.0 );
                    agent->setNeckAction( new rcsc::Neck_TurnToBall() );
                }
                else{
                    target_point.x = wm.offsideLineX();
                    target_point.y = wm.self().pos().y - wm.theirPlayer(1)->pos().y;
                    doGoToPoint(agent, target_point, dist_thr, dash_power, 15.0 );
                    agent->setNeckAction( new rcsc::Neck_TurnToBall() );
                }
            }


        }
    }*/

      //---------------
     // エリアごとの処理
    switch ( Strategy::get_ball_area( base_pos ) ) {
    case Strategy::BA_DribbleAttack:
    case Strategy::BA_OffMidField:
    	if ( agent->world().ball().pos().x > 25.0 )
        {
            doGoToCrossPoint( agent, home_pos );
        }
        else
        {
            Bhv_AttackerOffensiveMove( home_pos, true ).execute( agent );
        }
        break;
    case Strategy::BA_ShootChance:
    case Strategy::BA_Cross:
     if(Bhv_FSThroughPassMove().execute(agent))
	break;
     if(moveCentering(agent))
        break;
      if(!wm.existKickableTeammate()
         &&!wm.existKickableOpponent()
         &&self_cycle < mate_cycle 
         &&wm.ball().inertiaPoint( min_cycle ).x>wm.offsideLineX() )
       {//-----インターセプト-----
        if(Bhv_BasicTackle( 0.5, 90.0 ).execute( agent ) ){break;}
        rcsc::Body_Intercept2009(false).execute( agent );
        agent->setNeckAction(new rcsc::Neck_TurnToBallOrScan());
        break;
       }
      doGoToCrossPoint( agent, home_pos );
      break;
      
    case Strategy::BA_DribbleBlock:
    case Strategy::BA_DefMidField:
     if(Bhv_FSThroughPassMove().execute(agent))
	break;
	else
        {
            Bhv_AttackerOffensiveMove( home_pos, true ).execute( agent );
        } break;
    case Strategy::BA_CrossBlock:
    case Strategy::BA_Stopper:
    case Strategy::BA_Danger:
    default:
        Bhv_AttackerOffensiveMove( home_pos, true ).execute( agent );
        break;
    }
}

int
RoleHCAttacker::countKickableCycleAtSamePoint(rcsc::PlayerAgent * agent){
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

bool
RoleHCAttacker::moveCentering( rcsc::PlayerAgent * agent )
{
  const rcsc::WorldModel & wm = agent->world();
  static int catched_cycle = -1;
  static rcsc::Vector2D target_point(0.0,0.0);

  if( agent->world().audioMemory().freeMessageTime().cycle()
      > agent->world().time().cycle() - 3
      ){
    std::vector<rcsc::AudioMemory::FreeMessage> messages =
      agent->world().audioMemory().freeMessage();
    
    std::vector<rcsc::AudioMemory::FreeMessage>::const_iterator 
      mes_end = messages.end();
    for(std::vector<rcsc::AudioMemory::FreeMessage>::iterator mes_it 
	  = messages.begin();
	mes_it != mes_end;
	++mes_it){
      const rcsc::PlayerObject * 
	kicker = agent->world().getTeammateNearestToBall(3,false);
      if(kicker == NULL
	 || kicker->unum() != mes_it->sender_
	 || kicker->unum() == rcsc::Unum_Unknown)
	continue;
      
      // 2文字、4文字に分割してみる。
      const int offsets[] = {2,4};
      boost::offset_separator oss( offsets, offsets+2 );
      boost::tokenizer<boost::offset_separator> tokens( mes_it->message_, oss );
      boost::tokenizer<boost::offset_separator>::iterator 
	token_it = tokens.begin();
      
      if(token_it != tokens.end()
	 && *token_it == "KC"){
	++token_it;
	if(token_it == tokens.end())
	  continue;
	target_point = rcsc::AudioCodec::i().decodeStr4ToPos(*token_it);
	
	catched_cycle = agent->world().time().cycle();
	
	std::cout << agent->world().self().unum()
		  << ":[" << agent->world().time().cycle()
		  << "] MoveCentering:" << mes_it->sender_ 
		  << ":" << mes_it->message_ << " -> (" 
		  << target_point.x << "," << target_point.y 
		  << ")" << std::endl;
	break;
      }
    }
  }
  if(catched_cycle > agent->world().time().cycle() - 10
     &&!wm.existKickableTeammate()
     &&!wm.existKickableOpponent()){
    rcsc::Body_Intercept2009(false).execute( agent );
    agent->setNeckAction(new rcsc::Neck_TurnToBall());
    std::cout << agent->world().self().unum()
	      << ":[" << agent->world().time().cycle()
	      << "] MoveCentering(Intercept)" << std::endl;
    return true;
  }
  return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
RoleHCAttacker::doSelfPass( rcsc::PlayerAgent  * agent,
                               const int max_dash_step )
{
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doSelfPass dash_step=%d",
                        max_dash_step );

    const rcsc::WorldModel & wm = agent->world();

    rcsc::Vector2D receive_pos( rcsc::Vector2D::INVALIDATED );

    //const double dash_power = rcsc::ServerParam::i().maxPower();
    double dash_power = 100.0;
    //const int max_dash_step = 16;

    int dash_step = max_dash_step;
    {
        double my_effort = wm.self().effort();
        double my_recovery = wm.self().recovery();
        // next cycles my stamina
        double my_stamina
            = wm.self().stamina()
            + ( wm.self().playerType().staminaIncMax()
                * wm.self().recovery() );

        rcsc::Vector2D my_pos = wm.self().pos();
        rcsc::Vector2D my_vel = wm.self().vel();
        rcsc::AngleDeg accel_angle = wm.self().body();

        my_pos += my_vel;

        for ( int i = 0; i < max_dash_step; ++i )
        {
            if ( my_stamina < rcsc::ServerParam::i().recoverDecThrValue() + 400.0 )
            {
                dash_step = i;
                break;
            }

            double available_stamina
                =  std::max( 0.0,
                             my_stamina
                             - rcsc::ServerParam::i().recoverDecThrValue()
                             - 300.0 );
            double consumed_stamina = dash_power;
            consumed_stamina = std::min( available_stamina,
                                         consumed_stamina );
            double used_power = consumed_stamina;
            double max_accel_mag = ( std::fabs( used_power )
                                     * wm.self().playerType().dashPowerRate()
                                     * my_effort );
            double accel_mag = max_accel_mag;
            if ( wm.self().playerType().normalizeAccel( my_vel,
                                                        accel_angle,
                                                        &accel_mag ) )
            {
                used_power *= accel_mag / max_accel_mag;
            }

            rcsc::Vector2D dash_accel
                = rcsc::Vector2D::polar2vector( std::fabs( used_power )
                                                * my_effort
                                                * wm.self().playerType().dashPowerRate(),
                                                accel_angle );
            my_vel += dash_accel;
            my_pos += my_vel;

            my_vel *= wm.self().playerType().playerDecay();

            wm.self().playerType().getOneStepStaminaComsumption();
        }

        if ( dash_step <= 5 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": selfPass too short dash step %d",
                                dash_step );
            return false;
        }

        receive_pos = my_pos;
    }

    //if ( receive_pos.x > rcsc::ServerParam::i().theirPenaltyAreaLineX() )
    if ( receive_pos.x > rcsc::ServerParam::i().pitchHalfLength() - 3.5 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": selfPass receive pos(%.1f %.1f) may be out of pitch",
                            receive_pos.x, receive_pos.y );
        return false;
    }


    const rcsc::PlayerPtrCont::const_iterator end = wm.opponentsFromSelf().end();
    for ( rcsc::PlayerPtrCont::const_iterator it = wm.opponentsFromSelf().begin();
          it != end;
          ++it )
    {
        //if ( (*it)->posCount() > 20 ) continue;
        //if ( (*it)->isGhost() && (*it)->posCount() > 5 ) continue;

        double control_area = (*it)->playerTypePtr()->kickableArea();
        if ( (*it)->goalie()
             && receive_pos.x > rcsc::ServerParam::i().theirPenaltyAreaLineX()
             && receive_pos.absY() < rcsc::ServerParam::i().penaltyAreaHalfWidth() )
        {
            control_area = rcsc::ServerParam::i().catchableArea();
        }

        rcsc::Vector2D opp_to_target = receive_pos - (*it)->pos();

        double dist = opp_to_target.r();
        dist -= control_area;
        dist -= 0.2;
        dist -= (*it)->distFromSelf() * 0.02;

        if ( dist < 0.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": selfPass opponent %d(%.1f %.1f) is already at target point"
                                " (%.1f %.1f)",
                                (*it)->unum(),
                                (*it)->pos().x, (*it)->pos().y,
                                receive_pos.x, receive_pos.y );
            return false;
        }

        if ( (*it)->velCount() <= 1 )
        {
            rcsc::Vector2D vel = (*it)->vel();
            vel.rotate( - wm.self().body() );
            dist -= ( vel.x
                      * ( 1.0 - std::pow( (*it)->playerTypePtr()->playerDecay(), dash_step ) )
                      / ( 1.0 - (*it)->playerTypePtr()->playerDecay() ) );
        }

        int opp_target_cycle = (*it)->playerTypePtr()->cyclesToReachDistance( dist );

        if ( (*it)->bodyCount() <= 1 )
        {
            if ( ( opp_to_target.th() - (*it)->body() ).abs() > 40.0 )
            {
                opp_target_cycle += 1;
            }
        }
        opp_target_cycle -= rcsc::bound( 0, (*it)->posCount() - 1, 5 );

        if ( opp_target_cycle < dash_step + 0.5 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": selfPass opponent %d (%.1f %.1f) can reach faster then self."
                                " target=(%.1f %.1f) opp_step=%d  my_step=%d",
                                (*it)->unum(),
                                (*it)->pos().x, (*it)->pos().y,
                                receive_pos.x, receive_pos.y,
                                opp_target_cycle, dash_step );
            return false;
        }
    }


    double first_speed = rcsc::calc_first_term_geom_series( wm.ball().pos().dist( receive_pos ),
                                                            rcsc::ServerParam::i().ballDecay(),
                                                            dash_step + 1 );

    rcsc::AngleDeg target_angle = ( receive_pos - wm.ball().pos() ).th();
    rcsc::Vector2D max_vel
        = rcsc::Body_KickOneStep::get_max_possible_vel( target_angle,
                                                        wm.self().kickRate(),
                                                        wm.ball().vel() );
    if ( max_vel.r() < first_speed )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": selfPass cannot kick by one step. first_speed=%.2f > max_speed=%.2f",
                            first_speed,
                            max_vel.r() );
        return false;
    }

    rcsc::Vector2D ball_next = wm.ball().pos()
        + ( receive_pos - wm.ball().pos() ).setLengthVector( first_speed );
    rcsc::Vector2D my_next = wm.self().pos() + wm.self().vel();
    if ( my_next.dist( ball_next ) < ( wm.self().playerType().playerSize()
                                       + rcsc::ServerParam::i().ballSize() + 0.1 ) )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": selfPass maybe collision. first_speed=%.2f",
                            first_speed );
        return false;
    }

    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": selfPass receive pos(%.1f %.1f) first_speed=%.2f",
                        receive_pos.x, receive_pos.y,
                        first_speed );

    rcsc::Body_KickOneStep( receive_pos,
                            first_speed ).execute( agent );
    agent->setViewAction( new rcsc::View_Wide );
    agent->setNeckAction( new rcsc::Neck_TurnToBall );

    agent->debugClient().addMessage( "SelfPass" );
    agent->debugClient().setTarget( receive_pos );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleHCAttacker::doMiddleAreaKick( rcsc::PlayerAgent* agent )
{
    double dash_power = 100.0;
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doMiddleAreaKick" );

    const rcsc::WorldModel & wm = agent->world();

    const rcsc::PlayerObject * nearest_opp = wm.getOpponentNearestToSelf( 6 );

    if ( nearest_opp )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doMiddleAreaKick. nearest opp=(%.1f %.1f)",
                            nearest_opp->pos().x, nearest_opp->pos().y );
    }

    if ( Bhv_SelfPass().execute( agent ) )
    {
        return;
    }

    const rcsc::Bhv_PassTest::PassRoute * pass
        = rcsc::Bhv_PassTest::get_best_pass( wm );
    if ( pass )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doMiddleAreaKick. passable point (%.1f %.1f)",
                            pass->receive_point_.x,
                            pass->receive_point_.y );
    }

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
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": doMiddleAreaKick. dribble straight to (%.1f %.1f)",
                                drib_target.x, drib_target.y );
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
        rcsc::Bhv_PassTest().execute( agent );
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doMiddleAreaKick pass(1)" );
        agent->debugClient().addMessage( "MidPass(1)" );
        return;
    }

    //------------------------------------------------
    // pass to side area
    rcsc::Rect2D front_rect( rcsc::Vector2D( wm.self().pos().x,
                                             wm.self().pos().y - 5.0 ),
                             rcsc::Size2D( 10.0, 10.0 ) );
    if ( pass
         && pass->receive_point_.x > wm.self().pos().x
         && pass->receive_point_.absY() > 20.0
         && wm.existOpponentIn( front_rect, 10, false ) ) // without goalie
    {
        agent->debugClient().addMessage( "MidPassSide(1)" );
        rcsc::Bhv_PassTest().execute( agent );
        return;
    }

    if ( pass
         && pass->receive_point_.x > wm.self().pos().x + 10.0
         && pass->receive_point_.absY() > 20.0 )
    {
        agent->debugClient().addMessage( "MidPassSide(2)" );
        rcsc::Bhv_PassTest().execute( agent );
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
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": doMiddleAreaKick() fast dribble" );
            agent->debugClient().addMessage( "MidDribFast" );
            rcsc::Body_Dribble2008( drib_target,
                                    1.0,
                                    dash_power,
                                    5
                                    ).execute( agent );
        }
        else
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": doMiddleAreaKick() slow dribble" );
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
         && pass->receive_point_.x > wm.offsideLineX() )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doMiddleAreaKick() chance pass" );
        agent->debugClient().addMessage( "MidPassOverOffside" );
        rcsc::Bhv_PassTest().execute( agent );
        return;
    }

    //------------------------------------------------
    // opp is far from me
    if ( ! nearest_opp
         || nearest_opp->distFromSelf() > 3.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doMiddleAreaKick() opp far. dribble" );
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
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doMiddleAreaKick() pass" );
        agent->debugClient().addMessage( "MidPassNorm" );
        rcsc::Bhv_PassTest().execute( agent );
        return;
    }

    //------------------------------------------------
    // other
    // near opponent may exist.

    // added 2006/06/11 00:56
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doMiddleAreaKick() opponent is near. keep" );
    agent->debugClient().addMessage( "MidAreaHold" );
    rcsc::Vector2D face_point( 52.5, 0.0 );
    rcsc::Body_HoldBall2008( true, face_point ).execute( agent );
    agent->setNeckAction( new rcsc::Neck_ScanField() );

    /*
      agent->debugClient().addMessage( "MidToCorner" );
      Body_KickToCorner( (agent->world().self().pos().y < 0.0) ).execute( agent );
      agent->setNeckAction( new rcsc::Neck_ScanField() );
    */
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleHCAttacker::doCrossAreaKick( rcsc::PlayerAgent* agent )
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
RoleHCAttacker::doShootAreaKick( rcsc::PlayerAgent* agent )
{
    double dash_power = 100.0;
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
                        agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan( 0 ) );
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
                agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan( 0 ) );
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
                agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan() );
            }
            return;
        }
    }

#if 1
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
            agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan( 0 ) );
        }
        return;
    }
#endif

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
        agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan( 0 ) );
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleHCAttacker::doGoToCrossPoint( rcsc::PlayerAgent * agent,
                                     const rcsc::Vector2D & home_pos )
{
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doGoToCrossPoint" );

    const rcsc::WorldModel & wm = agent->world();
    //------------------------------------------------------
    // tackle
    if ( Bhv_BasicTackle( 0.75, 90.0 ).execute( agent ) )
    {
        return;
    }

    //----------------------------------------------
    // intercept check
    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( self_min < mate_min
         || ( mate_min != 0 // ! wm.existKickableTeammate()
              && self_min <= 6
              && wm.ball().pos().dist( home_pos ) < 10.0 )
         //|| wm.interceptTable()->isSelfFastestPlayer()
         )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doGoToCross. get ball" );
        agent->debugClient().addMessage( "CrossGetBall" );

        rcsc::Body_Intercept2008().execute( agent );

#if 0
        if ( self_min == 3 && opp_min >= 3 )
        {
            agent->setViewAction( new rcsc::View_Normal() );
        }

        if ( wm.self().pos().x > 30.0 )
        {
            if ( ! doCheckCrossPoint( agent ) )
            {
                agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan() );
            }
        }
        else
        {

            agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
        }
#else
        if ( wm.self().pos().x > 30.0
             && wm.self().pos().absY() < 20.0 )
        {
            if ( self_min == 3 && opp_min >= 3 )
            {
                agent->setViewAction( new rcsc::View_Normal() );
            }

            if ( ! doCheckCrossPoint( agent ) )
            {
                agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan() );
            }
        }
        else
        {
            agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        }
#endif
        return;
    }

    //----------------------------------------------
    // ball owner check
    //if ( ! wm.interceptTable()->isOurTeamBallPossessor() )
    if ( opp_min <= mate_min - 3 )
    {
//         const rcsc::PlayerObject * opp = wm.getOpponentNearestToBall( 3 );
//         if ( opp
//              && opp->distFromBall() < 2.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": doGoToCross. opp has ball" );
            agent->debugClient().addMessage( "CrossOppOwn(2)" );
            Bhv_BasicMove( home_pos ).execute( agent );
            return;
        }
    }

    //----------------------------------------------
    // set target

    rcsc::Vector2D target_point = home_pos;
    rcsc::Vector2D trap_pos = ( mate_min < 100
                                ? wm.ball().inertiaPoint( mate_min )
                                : wm.ball().pos() );

    if ( mate_min <= opp_min
         && mate_min < 3
         && target_point.x < 38.0
         && wm.self().pos().x < wm.offsideLineX() - 1.0
         //&& target_point.x < wm.self().pos().x
         //&& std::fabs( target_point.x - wm.self().pos().x ) < 20.0
         && std::fabs( target_point.y - wm.self().pos().y ) < 5.0
         && std::fabs( wm.self().pos().y - trap_pos.y ) < 13.0
         )
    {
        target_point.y = wm.self().pos().y * 0.9 + home_pos.y * 0.1;
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doGoToCross. chance keep current." );
        agent->debugClient().addMessage( "CrossCurPos" );
    }

    // consider near opponent
    if ( target_point.x > 36.0 )
    {
        double opp_dist = 200.0;
        const rcsc::PlayerObject * opp = wm.getOpponentNearestTo( target_point,
                                                                  10,
                                                                  &opp_dist );
        if ( opp && opp_dist < 2.0 )
        {
            rcsc::Vector2D tmp_target = target_point;
            for ( int i = 0; i < 3; ++i )
            {
                tmp_target.x -= 1.0;

                double d = 0.0;
                opp = wm.getOpponentNearestTo( tmp_target, 10, &d );
                if ( ! opp )
                {
                    opp_dist = 0.0;
                    target_point = tmp_target;
                    break;
                }
                if ( opp
                     && opp_dist < d )
                {
                    opp_dist = d;
                    target_point = tmp_target;
                }
            }
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": doGoToCross. avoid(%.2f, %.2f)->(%.2f, %.2f)",
                                home_pos.x, home_pos.y,
                                target_point.x, target_point.y );
            agent->debugClient().addMessage( "Avoid" );
        }
    }

    if ( target_point.dist( trap_pos ) < 6.0 )
    {
        rcsc::Circle2D target_circle( trap_pos, 6.0 );
        rcsc::Line2D target_line( target_point, rcsc::AngleDeg( 90.0 ) );
        rcsc::Vector2D sol_pos1, sol_pos2;
        int n_sol = target_circle.intersection( target_line, &sol_pos1, &sol_pos2 );

        if ( n_sol == 1 ) target_point = sol_pos1;
        if ( n_sol == 2 )
        {
            target_point = ( wm.self().pos().dist2( sol_pos1 ) < wm.self().pos().dist2( sol_pos2 )
                             ? sol_pos1
                             : sol_pos2 );

        }

        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doGoToCross. adjust ot avoid the ball owner." );
        agent->debugClient().addMessage( "Adjust" );
    }

    //----------------------------------------------
    // set dash power
    // check X buffer & stamina
    static bool s_recover_mode = false;
    if ( wm.self().pos().x > 35.0
         && wm.self().stamina()
         < rcsc::ServerParam::i().recoverDecThrValue() + 500.0 )
    {
        s_recover_mode = true;
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doGoToCross. recover on" );
    }

    if ( wm.self().stamina() > rcsc::ServerParam::i().staminaMax() * 0.5 )
    {
        s_recover_mode = false;
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doGoToCross. recover off" );
    }

    //double dash_power = rcsc::ServerParam::i().maxPower();
    double dash_power = 100.0;
    if ( s_recover_mode )
    {
        const double my_inc
            = wm.self().playerType().staminaIncMax()
            * wm.self().recovery();
        //dash_power = std::max( 1.0, my_inc - 25.0 );
        //dash_power = wm.self().playerType().staminaIncMax() * 0.6;
    }
    else if ( wm.ball().pos().x > wm.self().pos().x )
    {
        if ( wm.existKickableTeammate()
             && wm.ball().distFromSelf() < 10.0
             && std::fabs( wm.self().pos().x - wm.ball().pos().x ) < 5.0
             && wm.self().pos().x > 30.0
             && wm.ball().pos().x > 35.0 )
        {
            dash_power *= 0.5;
        }
    }
    else if ( wm.self().pos().dist( target_point ) < 3.0 )
    {
        const double my_inc
            = wm.self().playerType().staminaIncMax()
            * wm.self().recovery();
        //dash_power = std::min( rcsc::ServerParam::i().maxPower(),
//                               my_inc + 10.0 );
        //dash_power = rcsc::ServerParam::i().maxPower() * 0.8;
    }
    else if ( mate_min <= 1
              && wm.ball().pos().x > 33.0
              && wm.ball().pos().absY() < 7.0
              && wm.ball().pos().x < wm.self().pos().x
              && wm.self().pos().x < wm.offsideLineX()
              && wm.self().pos().absY() < 9.0
              && std::fabs( wm.ball().pos().y - wm.self().pos().y ) < 3.5
              && std::fabs( target_point.y - wm.self().pos().y ) > 5.0 )
    {
        //dash_power = wm.self().playerType()
//            .getDashPowerToKeepSpeed( 0.3, wm.self().effort() );
        //dash_power = std::min( rcsc::ServerParam::i().maxPower() * 0.75,
//                               dash_power );
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doGoToCross. slow for cross. power=%.1f",
                            dash_power );
    }

    //----------------------------------------------
    // positioning to make the cross course!!

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().addMessage( "GoToCross%.0f", dash_power );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doGoToCross. to (%.2f, %.2f)",
                        target_point.x, target_point.y );

    if ( wm.self().pos().x > target_point.x + dist_thr
         && std::fabs( wm.self().pos().x - target_point.x ) < 3.0
         && wm.self().body().abs() < 10.0 )
    {
        agent->debugClient().addMessage( "Back" );
        //double back_dash_power
        //    = wm.self().getSafetyDashPower( -dash_power );
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doShootAreaMove. Back Move" );
        agent->doDash( dash_power );
    }
    else
    {
        if ( ! rcsc::Body_GoToPoint( target_point, dist_thr, dash_power,
                                     5, // cycle
                                     false, // no back dash
                                     true, // save recovery
                                     30.0 // dir thr
                                     ).execute( agent ) )
        {
            rcsc::Body_TurnToAngle( 0.0 ).execute( agent );
        }
    }

    if ( wm.self().pos().x > 30.0 )
    {
        agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan() );
    }
    else
    {
        agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
RoleHCAttacker::doCheckCrossPoint( rcsc::PlayerAgent * agent )
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

void RoleHCAttacker::doOffensiveMiddleKick( PlayerAgent * agent )
{
    double dash_power = 100.0;
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

void
RoleHCAttacker::doGoToPoint( PlayerAgent * agent,
                             const Vector2D & target_point,
                             const double & dist_thr,
                             const double & dash_power,
                             const double & dir_thr )
{

    if ( Body_GoToPoint( target_point,
                         dist_thr,
                         dash_power,
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