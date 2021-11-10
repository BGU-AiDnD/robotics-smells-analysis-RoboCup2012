
/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "bhv_GEAR_shoot.h"

#include <rcsc/player/player_agent.h>

#include <rcsc/action/body_smart_kick.h>
#include <rcsc/action/neck_turn_to_goalie_or_scan.h>

#include <rcsc/common/server_param.h>

#include <rcsc/geom/sector_2d.h>

#include <rcsc/action/body_kick_one_step.h>

#include <rcsc/geom/vector_2d.h>
#include <rcsc/geom/triangle_2d.h>
#include <rcsc/geom/line_2d.h>

/*-------------------------------------------------------------------*/
/*!

*/

bool
Bhv_GEARShoot::execute( rcsc::PlayerAgent * agent )
{
  const rcsc::WorldModel & wm = agent->world();

  if((!rcsc::Sector2D(rcsc::Vector2D(52.5,0.0)
		     ,0.0,30.0
		     ,120,240).contains(wm.self().pos())
     &&!(wm.self().pos().x > 
	 rcsc::ServerParam::i().pitchHalfLength() 
	 - rcsc::ServerParam::i().penaltyAreaLength()
	 && wm.self().pos().absY() < 
	 rcsc::ServerParam::i().penaltyAreaHalfWidth()))
     || !wm.self().isKickable()){
    return false;
  }

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
    else
    {
    	 rcsc::Body_KickOneStep(rcsc::Vector2D(52.5,0.0),
			     rcsc::ServerParam::i().ballSpeedMax(),
			     true
			     ).execute(agent);

	agent->setNeckAction(new rcsc::Neck_TurnToGoalieOrScan());
/*    std::cout << wm.self().unum() << ":[" << wm.time().cycle()
	      << "] GEAR shoot (no goalie)"
	      << ": my pos:("
	      << wm.self().pos().x << "," << wm.self().pos().y << ")"
	      << std::endl;    */
    }

  if(goalie_dist < 4.0)
  {
    if(goalie_pos.y > 0){
      rcsc::Body_KickOneStep(rcsc::Vector2D(goalie_pos.x + 0.5, goalie_pos.y + 0.5),
			     rcsc::ServerParam::i().ballSpeedMax(),
			     true
			     ).execute(agent);
    }else{
      rcsc::Body_KickOneStep(rcsc::Vector2D(goalie_pos.x + 0.5, goalie_pos.y - 0.5),
			     rcsc::ServerParam::i().ballSpeedMax(),
			     true
			     ).execute(agent);
    }
    agent->setNeckAction(new rcsc::Neck_TurnToGoalieOrScan());
 /*   std::cout << wm.self().unum() << ":[" << wm.time().cycle()
	      << "] GEAR shoot (goalie too close): goalie pos(" 
	      << goalie_pos.x << "," << goalie_pos.y << ")"
	      << ": my pos:("
	      << wm.self().pos().x << "," << wm.self().pos().y << ")"
	      << std::endl;*/
    return true;
  }

	//check if there are opponents in front of goal

     rcsc::Triangle2D triangle_goal[2] = { rcsc::Triangle2D( wm.self().pos() , rcsc::Vector2D(52.5,8.0) , rcsc::Vector2D(goalie_pos.x,goalie_pos.y+1) ),
	                                 rcsc::Triangle2D( wm.self().pos() , rcsc::Vector2D(52.5,-8.0) , rcsc::Vector2D(goalie_pos.x,goalie_pos.y-1) )};
	                                 
     rcsc::Rect2D goalArea[2] = {rcsc::Rect2D(52.5,8.0,8,7), rcsc::Rect2D(52.5,0.0,8,7)};                               
                                 	                                 
	int  i = 0;//counter
	rcsc::Vector2D * shoot_point = new rcsc::Vector2D [2];
	
	rcsc::Vector2D * intersection = new rcsc::Vector2D[2];
	
	//only avaliate kick if goalie isn't very close to goalpost
	//if ( fabs(goalie_pos.y) > 5.5 && fabs(wm.self().pos().y > 5.5))
	//	return false;
		
	//if ( wm.self().pos().x < 40 || fabs(wm.self().pos().y) > 10)
	//	return false;		
		
	double time_to_score_goal[2];	
	double time_to_goalie_move[2];
	
	for( i = 0 ; i < 2 ; i ++ )
	{
		shoot_point[i] = ( triangle_goal[i].b()) ;
		
		if(!wm.existOpponentIn(triangle_goal[i],20,false) &&
		   !wm.existOpponentIn(goalArea[i],20,true) && wm.self().pos().dist(shoot_point[i]) < 12)
		{	     
			intersection[i] = getIntersectionPoint( goalie_pos, wm.self().pos(), shoot_point[i]);
			//std::cout <<  "intersection Point:(" << intersection[i].x << "," << intersection[i].y << ")" << std::endl;
			//std::cout <<  "player Point:(" << wm.self().pos().x << "," << wm.self().pos().y << ")" << std::endl;
			//std::cout <<  "goalie Point:(" << goalie_pos.x << "," << goalie_pos.y << ")" << std::endl;
		
			time_to_score_goal[i]  = ( wm.self().pos().dist ( intersection[i] ) /
						(rcsc::ServerParam::i().ballSpeedMax() - 0.4));		//ball decay must be considered
						//(rcsc::ServerParam::i().ballSpeedMax()*
						//rcsc::ServerParam::i().ballDecay()*
						//wm.self().pos().dist(intersection//[i]));						
					
			time_to_goalie_move[i] = ( ( goalie_pos.dist( intersection[i]))/
						rcsc::ServerParam::i().defaultPlayerSpeedMax() );

		}
		else
		{
			time_to_score_goal[i] = 1000.0;		//arbitrary parameters
			time_to_goalie_move[i] = 0.5;
		}
	}       
	
	//can't kick, there are opponents in shoot area
	
	if( time_to_score_goal[0]/time_to_goalie_move[0] == time_to_score_goal[1]/time_to_goalie_move[1] &&
	    int (time_to_score_goal[0]) == 1000.0)  		
	{
		//std::cout << "GEAR shoot failed due to opponents in shoot area " << std::endl;    
		return false;
	}
	
	//if goalie will intercept the ball, don't kick
	
	if ((time_to_score_goal[0] > time_to_goalie_move[0]) && (time_to_score_goal[1] > time_to_goalie_move[1]))
	{
		//std::cout << "goalie will intercept ball, dont shoot" << std::endl;
		return false;
	}
		
	//if both chances are good, choose the best
	
	if ((time_to_score_goal[0] < time_to_goalie_move[0]) && (time_to_score_goal[1] < time_to_goalie_move[1])) 	
	{
		if( time_to_score_goal[0]/time_to_goalie_move[0] < time_to_score_goal[1]/time_to_goalie_move[1] )
			rcsc::Body_KickOneStep(rcsc::Vector2D(52.5,rcsc::ServerParam::i().goalHalfWidth() - 0.6),
	    						rcsc::ServerParam::i().ballSpeedMax(),
	     						true
	     						).execute(agent);
	     						
		else rcsc::Body_KickOneStep(rcsc::Vector2D(52.5,-(rcsc::ServerParam::i().goalHalfWidth() - 0.6)),
	    						rcsc::ServerParam::i().ballSpeedMax(),
	     						true
	     						).execute(agent);	     						
	     												
		     						
		agent->setNeckAction(new rcsc::Neck_TurnToGoalieOrScan());
/*		std::cout << wm.self().unum() << ":[" << wm.time().cycle()
			<< "] GEAR shoot: goalie pos(" 
   			<< goalie_pos.x << "," << goalie_pos.y << ")"
      			<< ": my pos:("
      			<< wm.self().pos().x << "," << wm.self().pos().y << ")"
     			 << std::endl;*/
		return true;
	}
	
	else if (time_to_score_goal[0] < time_to_goalie_move[0])
		rcsc::Body_KickOneStep(rcsc::Vector2D(52.5,rcsc::ServerParam::i().goalHalfWidth() - 0.6),
	    						rcsc::ServerParam::i().ballSpeedMax(),
	     						true
	     						).execute(agent);
	else rcsc::Body_KickOneStep(rcsc::Vector2D(52.5,-(rcsc::ServerParam::i().goalHalfWidth() - 0.6)),
	    						rcsc::ServerParam::i().ballSpeedMax(),
	     						true
	     						).execute(agent);	
	     						
	agent->setNeckAction(new rcsc::Neck_TurnToGoalieOrScan());
/*	std::cout << wm.self().unum() << ":[" << wm.time().cycle()
		<< "] GEAR shoot: goalie pos(" 
   		<< goalie_pos.x << "," << goalie_pos.y << ")"
      		<< ": my pos:("
      		<< wm.self().pos().x << "," << wm.self().pos().y << ")"
     			 << std::endl;*/
	return true;	     						     						
}


rcsc::Vector2D
Bhv_GEARShoot::getIntersectionPoint( rcsc::Vector2D & goalie, const rcsc::Vector2D & player, rcsc::Vector2D & shootPoint)
{
	double x,y;			//final coordinates
	double m;			//slope
	
	m = (shootPoint.y - player.y)/(shootPoint.x - player.x);

	x = ((goalie.x/m) + goalie.y - player.y + (m*player.x))/(m+(1/m));
	y = ( (-1/m) * (x - goalie.x) ) + goalie.y;

	rcsc::Vector2D point(x,y);
	
	return point;
}
