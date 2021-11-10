
/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "bhv_fs_shoot.h"

#include <rcsc/player/player_agent.h>

#include <rcsc/action/body_smart_kick.h>
#include <rcsc/action/neck_turn_to_goalie_or_scan.h>

#include <rcsc/common/server_param.h>

#include <rcsc/geom/sector_2d.h>

#include <rcsc/action/body_kick_one_step.h>

/*-------------------------------------------------------------------*/
/*!

*/

bool
Bhv_FSShoot::execute( rcsc::PlayerAgent * agent )
{
  const rcsc::WorldModel & wm = agent->world();

  if(!rcsc::Sector2D(rcsc::Vector2D(52.5,0.0)
		     ,0.0,30.0
		     ,120,240).contains(wm.self().pos())
     &&!(wm.self().pos().x > 
	 rcsc::ServerParam::i().pitchHalfLength() 
	 - rcsc::ServerParam::i().penaltyAreaLength()
	 && wm.self().pos().absY() < 
	 rcsc::ServerParam::i().penaltyAreaHalfWidth())
     || !wm.self().isKickable()){
    return false;
  }

  const rcsc::PlayerObject *goalie = wm.getOpponentGoalie();
  rcsc::Vector2D opp_goalie_pos(50,0);
  if(goalie!=NULL)
    opp_goalie_pos = goalie->pos();

  if(opp_goalie_pos.dist(wm.self().pos()) < 1.0){
    if(opp_goalie_pos.y > 0){
      rcsc::Body_KickOneStep(opp_goalie_pos - rcsc::Vector2D(1.0,0.0)
			     ,rcsc::ServerParam::i().ballSpeedMax()
			     ,true
			     ).execute(agent);
    }else{
      rcsc::Body_KickOneStep(opp_goalie_pos + rcsc::Vector2D(1.0,0.0)
			     ,rcsc::ServerParam::i().ballSpeedMax()
			     ,true
			     ).execute(agent);
    }
    agent->setNeckAction(new rcsc::Neck_TurnToGoalieOrScan());
 /*   std::cout << wm.self().unum() << ":[" << wm.time().cycle()
	      << "] fs shoot(force):goalie pos(" 
	      << opp_goalie_pos.x << "," << opp_goalie_pos.y << ")"
	      << ": my pos:("
	      << wm.self().pos().x << "," << wm.self().pos().x << ")"
	      << std::endl;*/
    return true;
  }

  double first_speed = rcsc::ServerParam::i().ballSpeedMax();
  double thr = 0.96;
  int step = 2;
  rcsc::Vector2D 
    shoot_point(52.5, rcsc::ServerParam::i().goalHalfWidth() - 1.0);

  double offset = 1;
  int end_num = (rcsc::ServerParam::i().goalHalfWidth() - 1.0) * 2;

  //ゴーリーと逆サイド
  if( opp_goalie_pos.y > 0 )
    {
      shoot_point.y *= -1;
      offset *= -1;
    }
  
  //ポストにあたることがあるので、開始地点と終了地点を調整(遠いときもノイズが入るので)
  if(wm.self().pos().absY() > shoot_point.absY()){
    if(wm.self().pos().y * shoot_point.y > 0)
      shoot_point.y -= offset;
    else
      end_num--;
  }

  if(wm.self().pos().dist(shoot_point) > 10)
    shoot_point.y -= offset;
  if(wm.self().pos().dist(rcsc::Vector2D(shoot_point.x,-shoot_point.y)) > 10)
    end_num--;
  if(wm.self().pos().dist(shoot_point) > 20)
    shoot_point.y -= offset;
  if(wm.self().pos().dist(rcsc::Vector2D(shoot_point.x,-shoot_point.y)) > 20)
    end_num--;
  
  //シュートに関してちょっと実験（扇形で敵の有無を調べてからシュートしたい（敵がいても平然と蹴っちゃうので。））	

  bool shoot_flag = false;
  offset /= 2;
  for (int i = 0; i < end_num*2; i++)	//tune this!!
    {
      rcsc::AngleDeg shoot_angle = (shoot_point - wm.self().pos()).th();
      double shoot_dist = shoot_point.dist(wm.self().pos());
      rcsc::Sector2D checkSector( wm.self().pos()
				  ,0.0//min_r
				  ,shoot_dist//max_r
				  ,shoot_angle - 10//startAngleDeg
				  ,shoot_angle + 10//endAngleDeg
				  );
      //printf("(%f,%f) countOpp=%d \n",shoot_point.x,shoot_point.y,wm.countOpponentsIn(checkSector,5,true));		
      
      //セクター内に敵がいるか判定
      if ( wm.countOpponentsIn(checkSector,3,true) == 0 
	   && wm.countOpponentsIn(rcsc::Circle2D(shoot_point
						 ,shoot_dist/10)
				  ,3,true) == 0)
	{
	  //到達時間を計算
	  double arrival_cycle_len 
	    = rcsc::calc_length_geom_series( rcsc::ServerParam::i().ballSpeedMax(),
					     shoot_dist,
					     rcsc::ServerParam::i().ballDecay() );
	  if(arrival_cycle_len < 0)
	    continue;

	  bool exist_course = true;
	  //////////////////////////////////////////////////////////////////
	  //敵がボールに到達できるか
	  const rcsc::PlayerPtrCont::const_iterator
	    opp_end = wm.opponentsFromSelf().end();
	  for(rcsc::PlayerPtrCont::const_iterator 
		opp_it = wm.opponentsFromSelf().begin();
	      opp_it != opp_end;++opp_it){
	    //oppがcycle時にballに到達できるか
	    for(int cycle = 0;cycle < arrival_cycle_len;cycle++){
	      rcsc::Vector2D ball_through_pos
		= wm.self().pos() 
		+ rcsc::Vector2D::polar2vector(rcsc::calc_sum_geom_series(rcsc::ServerParam::i().ballSpeedMax()
									  ,rcsc::ServerParam::i().ballDecay()
									  ,cycle)
					       ,shoot_angle);
	      double opp_dist_toThroughPos
		= ball_through_pos.dist( (*opp_it)->pos() );
	      double opp_reachable_dist 
		= rcsc::ServerParam::i().defaultPlayerSpeedMax() * cycle;
	      if((*opp_it)->playerTypePtr() != NULL);
	      opp_reachable_dist 
		= (*opp_it)->playerTypePtr()->playerSpeedMax() * cycle;
	      
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
	      = shoot_point.dist( (*opp_it)->pos() );
	    double opp_reachable_dist_ball_arrived
	      = rcsc::ServerParam::i().defaultPlayerSpeedMax() * arrival_cycle_len;
	    if((*opp_it)->playerTypePtr() != NULL)
	      opp_reachable_dist_ball_arrived
		= (*opp_it)->playerTypePtr()->playerSpeedMax() * arrival_cycle_len;
	    if(opp_dist_toTarget  < opp_reachable_dist_ball_arrived){
	      exist_course = false;
	      break;
	    }
	  }
	  
	  if(exist_course){
	    shoot_flag = true;
	    break;
	  }
	}
      
      //線形探索
      shoot_point.y-=offset;
    }
  
  //シュートコースが見つかったので、シュート
  if( shoot_flag && shoot_point.dist(wm.self().pos()) < 30)
    {
      if(rcsc::Body_SmartKick( shoot_point
			       ,first_speed
			       ,first_speed * thr
			       ,step).execute(agent)){
	agent->setNeckAction(new rcsc::Neck_TurnToGoalieOrScan());
/*	std::cout << wm.self().unum() << ":[" << wm.time().cycle()
		  << "] fs shoot:(" 
		  << shoot_point.x << "," << shoot_point.y << ")"
		  << ": my pos:("
		  << wm.self().pos().x << "," << wm.self().pos().x << ")"
		  << std::endl;*/
	return true;
      }
    }

  //ゴールが近い時は、強制的にシュート
  if(wm.self().pos().dist(rcsc::Vector2D(52.5,0.0))<7
     || wm.self().pos().dist(rcsc::Vector2D(52.5
					   ,rcsc::ServerParam::i().goalHalfWidth() - 0.3))<7
     || wm.self().pos().dist(rcsc::Vector2D(52.5
					    ,-(rcsc::ServerParam::i().goalHalfWidth() - 0.3)))<7)
    {
      shoot_point 
	= rcsc::Vector2D(52.5
			 , rcsc::ServerParam::i().goalHalfWidth() - 0.5);
      end_num = (rcsc::ServerParam::i().goalHalfWidth() - 0.5) * 2 * 10;
      offset = 0.1;
      if(wm.self().pos().y < 0){
	shoot_point.y *= -1;
	offset *= -1;
      }

      if(wm.existKickableOpponent()){
	rcsc::Body_KickOneStep(shoot_point
			       ,rcsc::ServerParam::i().ballSpeedMax()
			       ,true
			       ).execute(agent);
	agent->setNeckAction(new rcsc::Neck_TurnToGoalieOrScan());
/*	std::cout << wm.self().unum() << ":[" << wm.time().cycle()
		  << "] fs shoot(force):exist kickable opponent" 
		  << ": my pos:("
		  << wm.self().pos().x << "," << wm.self().pos().x << ")"
		  << std::endl;*/
      }

      if(wm.self().pos().absY() > shoot_point.absY()){
	if(wm.self().pos().y * shoot_point.y > 0)
	  shoot_point.y -= offset*2;
	else
	  end_num -= 2;
      }

      for (int i = 0; i < end_num*2; i++)
	{
	  rcsc::AngleDeg shoot_angle = (shoot_point - wm.self().pos()).th();
	  rcsc::Sector2D checkSector( wm.self().pos()
				      ,0.0
				      ,wm.self().pos().dist( shoot_point )
				      ,shoot_angle - 10
				      ,shoot_angle + 10
				      );	
	  
	  if ( shoot_point.dist(wm.self().pos()) < 7
	       && wm.countOpponentsIn(checkSector,3,true) == 0 
	       && wm.countOpponentsIn(rcsc::Circle2D(shoot_point
						     ,shoot_point.dist(wm.self().pos())/10)
				      ,3,true) == 0)
	    {
	      shoot_flag = true;
	      break;
	    }
	  shoot_point.y-=offset;
	}
     
      if(shoot_flag){
	if(rcsc::Body_SmartKick( shoot_point
				 ,first_speed
				 ,first_speed * thr
				 ,step).execute(agent)){
	  agent->setNeckAction(new rcsc::Neck_TurnToGoalieOrScan());
/*	  std::cout << wm.self().unum() << ":[" << wm.time().cycle()
		    << "] fs shoot(force):(" 
		    << shoot_point.x << "," << shoot_point.y << ")"
		    << ": my pos:("
		    << wm.self().pos().x << "," << wm.self().pos().x << ")"
		    << std::endl;*/
	  return true;
	}
      }
    }
  
  return false; 
}
