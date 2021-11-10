
/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "bhv_nutmeg_kick.h"

#include <rcsc/player/player_agent.h>

#include <rcsc/action/body_smart_kick.h>
#include <rcsc/action/neck_turn_to_ball.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/sector_2d.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_turn_to_point.h>

/*-------------------------------------------------------------------*/
/*!

*/

bool
Bhv_NutmegKick::execute( rcsc::PlayerAgent * agent )
{
  const rcsc::WorldModel & wm = agent->world();
  if(!wm.self().isKickable())
    return false;

  rcsc::PlayerObject * target_opp = NULL;
  const rcsc::PlayerPtrCont::const_iterator
    opp_end = wm.opponentsFromSelf().end();
  for(rcsc::PlayerPtrCont::const_iterator 
	opp_it = wm.opponentsFromSelf().begin();
      opp_it != opp_end;++opp_it){
    if(rcsc::Sector2D(wm.self().pos()
		      ,0.0,10.0
		      ,-30,30).contains((*opp_it)->pos())){
      target_opp = *opp_it;
      break;
    }
  }
  if(target_opp == NULL
     || wm.self().pos().dist(target_opp->pos()) > 7
     ){
    return false;
  }

  //パス先までの距離と角度
  const double target_dist 
    = wm.self().pos().dist( target_opp->pos() );
  const double target_angle
    = (target_opp->pos() - wm.self().pos()).dir().degree();

  //初速度計算
  double end_speed  = 0.7;
  double first_speed = rcsc::ServerParam::i().ballSpeedMax();
  first_speed
    = rcsc::calc_first_term_geom_series_last
    ( end_speed,
      target_dist + 3,
      rcsc::ServerParam::i().ballDecay() );
  
  first_speed = std::min(first_speed,rcsc::ServerParam::i().ballSpeedMax());
  
  //到達時間を計算
  double arrival_cycle_len 
    = rcsc::calc_length_geom_series( first_speed,
				     target_dist + 3,
				     rcsc::ServerParam::i().ballDecay() );
  if(arrival_cycle_len < 0)
    return false;

  /////////////////////////////////////////////////////////////////////////
  //コースが空いてるか探索
  //targetの左右で敵の少ないほうだけ、探索
  int sign = 1;
  if(wm.countOpponentsIn(rcsc::Sector2D(wm.self().pos()
				       ,0.0,target_dist * 2
				       ,target_angle
				       ,target_angle + 30)
			,5,false)
     > wm.countOpponentsIn(rcsc::Sector2D(wm.self().pos()
					 ,0.0,target_dist * 2
					 ,target_angle - 30
					 ,target_angle)
			  ,5,false)
     )
    sign *= -1;
  for(int angle=10;angle<25;angle+=1){
    rcsc::Vector2D target_pos 
      = wm.self().pos() + rcsc::Vector2D::polar2vector(target_dist+3
						       ,target_angle+sign*angle);
    if(!rcsc::Rect2D(rcsc::Vector2D(-rcsc::ServerParam::i().pitchHalfLength()
				    ,-rcsc::ServerParam::i().pitchHalfWidth())
		     ,rcsc::Vector2D(rcsc::ServerParam::i().pitchHalfLength()
				     ,rcsc::ServerParam::i().pitchHalfWidth())
		     ).contains(target_pos))
      continue;
    /*
    if(!wm.existOpponentIn(rcsc::Sector2D(wm.self().pos()
					  ,0.0,wm.self().pos().dist(target_pos)
					  ,target_angle + sign*angle
					  ,target_angle + sign*(angle+1))
			   ,5,false)
       && wm.countOpponentsIn(rcsc::Circle2D(target_pos,7.0)
			     ,5,false) < 2){
      rcsc::Body_SmartKick(target_pos
			   ,first_speed
			   ,first_speed * 0.96
			   ,2
			   ).execute(agent);
      std::cout << wm.self().unum() << ":[" << wm.time().cycle()
		<< "] nutmeg kick:(" 
		<< target_pos.x << "," << target_pos.y << ")"
		<< std::endl;
      
      agent->setNeckAction(new rcsc::Neck_TurnToBall());
      return true;
    }
    */
    
    bool exist_course = true;
    for(rcsc::PlayerPtrCont::const_iterator 
	  opp_it = wm.opponentsFromSelf().begin();
	opp_it != opp_end;++opp_it)
      {      
	//oppがcycle時にballに到達できるか
	for(int cycle = 1;cycle < arrival_cycle_len;cycle++){
	  rcsc::Vector2D ball_through_pos
	    = wm.self().pos() 
	    + rcsc::Vector2D::polar2vector(rcsc::calc_sum_geom_series(first_speed
								      ,rcsc::ServerParam::i().ballDecay()
								      ,cycle)
					   ,target_angle+sign*angle);
	  double opp_dist_toThroughPos
	    = ball_through_pos.dist( (*opp_it)->pos() );
	  double opp_reachable_dist 
	    = rcsc::calc_sum_geom_series(rcsc::ServerParam::i().defaultPlayerSpeedMax()
					 ,rcsc::ServerParam::i().defaultPlayerDecay()
					 ,cycle);
	  if((*opp_it)->playerTypePtr() != NULL);
	    opp_reachable_dist 
	      = rcsc::calc_sum_geom_series((*opp_it)->playerTypePtr()->playerSpeedMax()
					   ,(*opp_it)->playerTypePtr()->playerDecay() 
					   ,cycle);
	  
	  //oppに到達されてしまうので、このコースは空いてない
	  if(opp_dist_toThroughPos  < opp_reachable_dist){
	    exist_course = false;
	    break;
	  }
	}

	//コースが空いてないので、oppのループを抜ける
	if(!exist_course)
	  break;

	//oppに到達されるか
	double opp_dist_toTarget
	  = target_pos.dist( (*opp_it)->pos() );
	double opp_reachable_dist_ball_arrived
	   = rcsc::calc_sum_geom_series(rcsc::ServerParam::i().defaultPlayerSpeedMax()
					 ,rcsc::ServerParam::i().defaultPlayerDecay()
					 ,arrival_cycle_len);
	if((*opp_it)->playerTypePtr() != NULL)
	  opp_reachable_dist_ball_arrived
	      = rcsc::calc_sum_geom_series((*opp_it)->playerTypePtr()->playerSpeedMax()
					   ,(*opp_it)->playerTypePtr()->playerDecay() 
					   ,arrival_cycle_len);
	if(opp_dist_toTarget  < opp_reachable_dist_ball_arrived){
	  exist_course = false;
	  break;
	}
	double opp_arrival
	  = rcsc::calc_length_geom_series(rcsc::ServerParam::i().defaultPlayerSpeedMax(),
					  target_dist + 3,
					  rcsc::ServerParam::i().defaultPlayerDecay());
	if((*opp_it)->playerTypePtr() != NULL)
	  opp_arrival
	    = rcsc::calc_length_geom_series((*opp_it)->playerTypePtr()->playerSpeedMax(),
					   target_dist + 3,
					   (*opp_it)->playerTypePtr()->playerDecay());
	double self_arrival
	  = rcsc::calc_length_geom_series(wm.self().playerTypePtr()->playerSpeedMax(),
					   target_dist + 3,
					  wm.self().playerTypePtr()->playerDecay());

	if(self_arrival < opp_arrival - 1){
	  exist_course = false;
	  break;
	}
      }
 
    //コースが空いてれば
    if(exist_course){
      rcsc::Body_SmartKick(target_pos
			   ,first_speed
			   ,first_speed * 0.96
			   ,2
			   ).execute(agent);
      std::cout << wm.self().unum() << ":[" << wm.time().cycle()
		<< "] nutmeg kick:(" 
		<< target_pos.x << "," << target_pos.y << ")"
		<< std::endl;
      
      agent->setNeckAction(new rcsc::Neck_TurnToBall());
      return true;
    }
    
  }

  return false;
}
