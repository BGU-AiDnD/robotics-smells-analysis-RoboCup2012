
/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "bhv_fs_clear_ball.h"

#include <rcsc/player/player_agent.h>

#include <rcsc/common/server_param.h>

#include <rcsc/action/body_smart_kick.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_ball.h>

#include <rcsc/geom/sector_2d.h>

#include <rcsc/action/body_clear_ball.h>

/*-------------------------------------------------------------------*/
/*!

*/

bool
Bhv_FSClearBall::execute( rcsc::PlayerAgent * agent ){
  if(!agent->world().self().isKickable())
    return false;

  if(v2(agent)){
    // reset intention
    agent->setIntention( static_cast< rcsc::SoccerIntention * >( 0 ) );
    return true;
  }
  if(v1(agent)){
    // reset intention
    agent->setIntention( static_cast< rcsc::SoccerIntention * >( 0 ) );
    return true;
  }

  if(rcsc::Body_ClearBall().execute(agent)){
    agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
    // reset intention
    agent->setIntention( static_cast< rcsc::SoccerIntention * >( 0 ) );
    return true;
  }
  return false;
}

bool
Bhv_FSClearBall::v2( rcsc::PlayerAgent * agent ){

  const rcsc::WorldModel & wm = agent->world();

  double first_speed = rcsc::ServerParam::i().maxPower();
  const double max_dist = 
    rcsc::calc_sum_inf_geom_series(first_speed
			     ,rcsc::ServerParam::i().ballDecay());

  int fewest_area_num_first=-1;
  int min_opps_count=7;
  
  int startAngle = -75;
  int angleOffset = 50;
  if(wm.self().pos().y < 0){
    startAngle *= -1;
    angleOffset *= -1;
  }
  //first search
  for(int i=0;i<3;i++){
    rcsc::Sector2D checkArea(wm.self().pos()
			     ,0.0,max_dist
			     ,i*angleOffset+startAngle
			     ,(i+1)*angleOffset+startAngle);
    int opps_count 
      = wm.countOpponentsIn(checkArea,7,false);
    if(opps_count<min_opps_count){
      min_opps_count=opps_count;
      fewest_area_num_first=i;
    } 
  }

  if(fewest_area_num_first==-1)
    return false;

  int fewest_area_num_second=-1;
  min_opps_count=5;
  //second search
  for(int i=0;i<3;i++){
    rcsc::Sector2D checkArea(wm.self().pos()
			     ,0.0,max_dist
			     ,fewest_area_num_first*angleOffset 
			     + i*angleOffset/3 +startAngle
			     ,fewest_area_num_first*angleOffset 
			     + (i+1)*angleOffset/3 +startAngle);
    int opps_count 
      = wm.countOpponentsIn(checkArea,7,false);
    if(opps_count<min_opps_count){
      min_opps_count=opps_count;
      fewest_area_num_second=i;
    } 
  }

  if(fewest_area_num_second==-1){
    rcsc::Vector2D target_pos
      = rcsc::Vector2D().setPolar(max_dist
				  ,fewest_area_num_first*angleOffset 
				  +angleOffset/2 +startAngle);
    target_pos += wm.self().pos();
  
 

    if(rcsc::Body_SmartKick(target_pos
			    ,first_speed
			    ,first_speed * 0.96
			    ,2
			    ).execute(agent)){
      agent->setNeckAction(new rcsc::Neck_TurnToBall());
      return true;
    }
  }
  
  int fewest_area_num_third=-1;
  min_opps_count=3;
  //third search
  for(int i=0;i<4;i++){
    rcsc::Sector2D checkArea(wm.self().pos()
			     ,0.0,max_dist
			     ,fewest_area_num_first*angleOffset 
			     + fewest_area_num_second*angleOffset/3
			     + i*angleOffset/3/4 +startAngle
			     ,fewest_area_num_first*angleOffset
			     + fewest_area_num_second*angleOffset/3
			     + (i+1)*angleOffset/3/4 +startAngle);
    int opps_count 
      = wm.countOpponentsIn(checkArea,7,false);
    if(opps_count<min_opps_count){
      min_opps_count=opps_count;
      fewest_area_num_third=i;
    } 
  }

  if(fewest_area_num_third==-1){
    rcsc::Vector2D target_pos
      = rcsc::Vector2D().setPolar(max_dist
				  ,fewest_area_num_first*angleOffset
				  +fewest_area_num_second*angleOffset/3
				  +angleOffset/3/2 +startAngle);
    target_pos += wm.self().pos();

    if(rcsc::Body_SmartKick(target_pos
			    ,first_speed
			    ,first_speed * 0.96
			    ,2
			    ).execute(agent)){
      agent->setNeckAction(new rcsc::Neck_TurnToBall());
      return true;
    }
  }

  int fewest_area_num_fourth=-1;
  min_opps_count=1;
  //fourth search
  for(int i=0;i<5;i++){
    rcsc::Sector2D checkArea(wm.self().pos()
			     ,0.0,max_dist
			     ,fewest_area_num_first*angleOffset 
			     + fewest_area_num_second*angleOffset/3
			     + i*angleOffset/3/4/5 +startAngle
			     ,fewest_area_num_first*angleOffset
			     + fewest_area_num_second*angleOffset/3
			     + fewest_area_num_third*angleOffset/3/4 
			     + (i+1)*angleOffset/3/4/5 +startAngle);
    int opps_count 
      = wm.countOpponentsIn(checkArea,7,false);
    if(opps_count<min_opps_count){
      min_opps_count=opps_count;
      fewest_area_num_fourth=i;
    } 
  }

  if(fewest_area_num_fourth==-1){
    rcsc::Vector2D target_pos
      = rcsc::Vector2D().setPolar(max_dist
				  ,fewest_area_num_first*angleOffset
				  + fewest_area_num_second*angleOffset/3
				  + fewest_area_num_third*angleOffset/3/4 
				  + angleOffset/3/4/2 +startAngle);
    target_pos += wm.self().pos();

    if(rcsc::Body_SmartKick(target_pos
			    ,first_speed
			    ,first_speed * 0.96
			    ,2
			    ).execute(agent)){
      agent->setNeckAction(new rcsc::Neck_TurnToBall());
      return true;
    }
  }
  
  rcsc::Vector2D target_pos
    = rcsc::Vector2D().setPolar(max_dist
				,fewest_area_num_first*angleOffset
				+ fewest_area_num_second*angleOffset/3
				+ fewest_area_num_third*angleOffset/3/4 
				+ fewest_area_num_fourth*angleOffset/3/4/5
				+ angleOffset/3/4/5/2 +startAngle);
  target_pos += wm.self().pos();
  
  if(rcsc::Body_SmartKick(target_pos
			  ,first_speed
			  ,first_speed * 0.96
			  ,2
			  ).execute(agent)){
    agent->setNeckAction(new rcsc::Neck_TurnToBall());
    return true;
  }

  return false;
}

bool
Bhv_FSClearBall::v1( rcsc::PlayerAgent *agent ){
  const rcsc::WorldModel & wm = agent->world();

  rcsc::PlayerObject * first_opp = NULL;
  rcsc::PlayerObject * second_opp = NULL;
  const rcsc::PlayerPtrCont::const_iterator
    end=wm.opponentsFromSelf().end();
  for(rcsc::PlayerPtrCont::const_iterator
	it=wm.opponentsFromSelf().begin();
      it!=end;++it){
    if((*it)->pos().x < wm.self().pos().x)
      continue;

    if(first_opp == NULL){
      first_opp = *it;
    }else if(second_opp == NULL){
      second_opp = *it;
      break;
    }
  }

  if(first_opp == NULL)
    return false;

  if(second_opp == NULL
     || first_opp->pos().dist(second_opp->pos()) < 7){
    rcsc::Line2D course_right(wm.self().pos()
			      ,first_opp->pos() + rcsc::Vector2D(-1.5,1.5));
    rcsc::Line2D course_left(wm.self().pos()
			     ,first_opp->pos() + rcsc::Vector2D(-1.5,-1.5));
    double 
      value_course_right = 0
      ,value_course_left = 0;
    for(rcsc::PlayerPtrCont::const_iterator
	  it=wm.opponentsFromSelf().begin();
	it!=end && (*it)->distFromSelf()<40;++it){
      if((*it)->pos().x > wm.self().pos().x
	 && !(*it)->goalie()){
	value_course_right += course_right.dist2((*it)->pos());
	value_course_left += course_left.dist2((*it)->pos());
      }
    }
    const double first_speed = rcsc::ServerParam::i().ballSpeedMax();
    const double thr = 0.96;
    const int steps = 2;
    if(value_course_left < value_course_right){
      if(rcsc::Body_SmartKick(first_opp->pos() + rcsc::Vector2D(-1.5,1.5),
			      first_speed,
			      first_speed * thr,
			      steps).execute(agent)){
	agent->setNeckAction(new rcsc::Neck_TurnToBallOrScan());
	return true;
      }
    }else{
      if(rcsc::Body_SmartKick(first_opp->pos() + rcsc::Vector2D(-1.5,-1.5),
			      first_speed,
			      first_speed * thr,
			      steps).execute(agent)){
	agent->setNeckAction(new rcsc::Neck_TurnToBallOrScan());
	return true;
      }
    }
  }

  if(second_opp == NULL)
    return false;

  rcsc::Vector2D target_point = (first_opp->pos() + second_opp->pos())/2;
  if(rcsc::Line2D(first_opp->pos(),second_opp->pos()).dist(wm.self().pos())
     /wm.self().pos().dist(target_point) > 0.7){
    double first_speed = rcsc::ServerParam::i().ballSpeedMax();
    double thr = 0.96;
    int step = 2;
    if(rcsc::Body_SmartKick(target_point,
			    first_speed,
			    first_speed * thr,
			    step).execute(agent)){
      agent->setNeckAction(new rcsc::Neck_TurnToBallOrScan());
      return true;
    }
  }
  return false;
}
