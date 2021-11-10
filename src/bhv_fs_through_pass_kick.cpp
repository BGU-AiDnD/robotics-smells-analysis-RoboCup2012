
/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "bhv_fs_through_pass_kick.h"

#include <rcsc/player/player_agent.h>

#include <rcsc/common/server_param.h>

#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/body_smart_kick.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/common/logger.h>
#include <rcsc/player/say_message_builder.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/player/free_message.h>
#include <rcsc/common/audio_codec.h>
#include <rcsc/geom/sector_2d.h>

#include <rcsc/action/neck_turn_to_low_conf_teammate.h>

/*-------------------------------------------------------------------*/
/*!

*/

bool
Bhv_FSThroughPassKick::execute( rcsc::PlayerAgent * agent )
{
  /////////////////////////////////////////////////////////////////////
  //
  rcsc::dlog.addText( rcsc::Logger::ACTION,
		      "%s:%d: FSThroughPass. execute()"
		      ,__FILE__, __LINE__ );
  
  if ( ! agent->world().self().isKickable() )
    {
      std::cerr << __FILE__ << ": " << __LINE__
		<< " not ball kickable!"
		<< std::endl;
      rcsc::dlog.addText( rcsc::Logger::ACTION,
			  "%s:%d:  not kickable"
			  ,__FILE__, __LINE__ );
      return false;
    }

  if ( !agent->config().useCommunication() )
    return false;

  static int S_announced_cycle = -1; 
  static int S_connected_cycle = -1;

  static rcsc::Vector2D S_target_point(0.0, 0.0);
  static double S_first_speed = rcsc::ServerParam::i().ballSpeedMax();
  static int S_receiver_unum = 0;
  

  
  if((S_announced_cycle == -1
      || S_announced_cycle < agent->world().time().cycle() - 10)
     && get_through_pass( agent->world()
			  , &S_target_point, &S_first_speed, &S_receiver_unum )){
    std::string pos_message =
      rcsc::AudioCodec::i().encodePosToStr4(S_target_point); 

    agent->addSayMessage( new rcsc::FreeMessage<6>("TP" + pos_message) );
    S_announced_cycle = agent->world().time().cycle();
    S_connected_cycle = -1;
	/*    std::cout << agent->world().self().unum()
	      << ":[" << agent->world().time().cycle()
	      << "] FSThroughPassKick: (" 
	      << S_target_point.x << "," << S_target_point.y 
	      << ") -> " << "TP" + pos_message << std::endl;*/
    return false;
  }

  if(agent->world().audioMemory().freeMessageTime().cycle()
     > agent->world().time().cycle() - 3){
    std::vector<rcsc::AudioMemory::FreeMessage> messages =
      agent->world().audioMemory().freeMessage();

    std::vector<rcsc::AudioMemory::FreeMessage>::const_iterator mes_end =
      messages.end();
    for(std::vector<rcsc::AudioMemory::FreeMessage>::iterator mes_it 
	  = messages.begin();
	mes_it != mes_end;
	++mes_it){
      if(mes_it->message_ == "TPOK"){
 
	S_receiver_unum = mes_it->sender_;
	S_connected_cycle = agent->world().time().cycle();
      }
      break;
    }
  }
  if(S_connected_cycle > agent->world().time().cycle() - 5){
    if(!verifyPassCource(agent->world(),S_target_point,&S_first_speed)
       && get_through_pass( agent->world()
			    , &S_target_point, &S_first_speed, &S_receiver_unum )){
      std::string pos_message =
	rcsc::AudioCodec::i().encodePosToStr4(S_target_point); 
      
      agent->addSayMessage( new rcsc::FreeMessage<6>("TP" + pos_message) );
      S_announced_cycle = agent->world().time().cycle();
      S_connected_cycle = -1;
 /*     std::cout << agent->world().self().unum()
		<< ":[" << agent->world().time().cycle()
		<< "] FSThroughPassKick: (" 
		<< S_target_point.x << "," << S_target_point.y 
		<< ") -> " << "TP" + pos_message << std::endl;*/
      return false;
    }
    
    
    int kick_step = ( agent->world().gameMode().type() != rcsc::GameMode::PlayOn
		      && agent->world().gameMode().type() != rcsc::GameMode::GoalKick_
		      ? 1
		      : 3 );
    if ( ! rcsc::Body_SmartKick( S_target_point,
				 S_first_speed,
				 S_first_speed * 0.96,
				 kick_step ).execute( agent ) )
      {
	if ( agent->world().gameMode().type() != rcsc::GameMode::PlayOn
	     && agent->world().gameMode().type() != rcsc::GameMode::GoalKick_ )
	  {
	    S_first_speed = std::min( agent->world().self().kickRate() * rcsc::ServerParam::i().maxPower(),
				    S_first_speed );
	    rcsc::Body_KickOneStep( S_target_point,
				    S_first_speed
				    ).execute( agent );
	    rcsc::dlog.addText( rcsc::Logger::ACTION,
				__FILE__": execute() one step kick" );
	  }
	else
	  {
	    rcsc::dlog.addText( rcsc::Logger::ACTION,
				__FILE__": execute() failed to pass kick." );
	    return false;
	  }
      }
    
    // reset intention
    agent->setIntention( static_cast< rcsc::SoccerIntention * >( 0 ) );
    
    rcsc::dlog.addText( rcsc::Logger::ACTION,
			__FILE__": execute() set pass communication." );
    agent->addSayMessage( new rcsc::FreeMessage<4>("TPKK") );
    agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
/*    std::cout << agent->world().self().unum()
	      << ":[" << agent->world().time().cycle()
	      << "] FSThroughPassKick(kick):" << S_receiver_unum << "@(" 
	      << S_target_point.x << "," << S_target_point.y 
	      << ")" << std::endl;*/
    
    return true;
  }
  
  return false;
}

bool
Bhv_FSThroughPassKick::get_through_pass( const rcsc::WorldModel & world
					 , rcsc::Vector2D * target_point 
					 , double * first_speed 
					 , int * receiver_unum )
{
  rcsc::Vector2D mate_better_point(0.0,0.0);
  int mate_better_unum = 0;
  double better_speed = rcsc::ServerParam::i().ballSpeedMax();
  
  const rcsc::PlayerPtrCont::const_iterator 
    team_end = world.teammatesFromSelf().end();
  for(rcsc::PlayerPtrCont::const_iterator
	team_it = world.teammatesFromSelf().begin();
      team_it != team_end;
      ++team_it){
    if((*team_it)->pos().x > mate_better_point.x){
      mate_better_point = (*team_it)->pos();
      mate_better_unum = (*team_it)->unum();
    }
  }
  if(mate_better_point.x <= 0)
    return false;

  if(world.offsideLineX() < 48.0)
    mate_better_point.x = std::min(48.0,world.offsideLineX() + 10);
  else
    mate_better_point.x = world.offsideLineX();

  bool valid = false;
  for(int i = 0;i <= 10;i++){
    mate_better_point.y=(i*world.self().pos().y+(10-i)*mate_better_point.y)/10;
    double end_speed = 0.7;
    better_speed
      = rcsc::calc_first_term_geom_series_last
      ( end_speed,
	world.self().pos().dist(mate_better_point),
	rcsc::ServerParam::i().ballDecay() );
    better_speed = std::min(better_speed,rcsc::ServerParam::i().ballSpeedMax());    
    //ボールの到達時間を計算
    double arrival_cycle_len 
      = rcsc::calc_length_geom_series( better_speed,
				       world.ball().pos().dist(mate_better_point),
				       rcsc::ServerParam::i().ballDecay() );
    if(arrival_cycle_len < 0)
      continue;
    
    double mate_dist;
    double opp_dist;
    const rcsc::PlayerObject * 
      nearest_mate = world.getTeammateNearestTo(mate_better_point
						,5
						,&mate_dist);
    const rcsc::PlayerObject * 
      nearest_opp = world.getOpponentNearestTo(mate_better_point
					       ,5
					       ,&opp_dist);
    double arrival_cycle_mate = 100;
    double arrival_cycle_opp = 100;
    double safe_dist 
      = arrival_cycle_len * rcsc::ServerParam::i().defaultPlayerSpeedMax();
    if(nearest_mate){
      if(nearest_mate->playerTypePtr()){
	arrival_cycle_mate = mate_better_point.dist(nearest_mate->pos()) / nearest_mate->playerTypePtr()->playerSpeedMax();
      }else{
	arrival_cycle_mate = mate_better_point.dist(nearest_mate->pos()) / rcsc::ServerParam::i().defaultPlayerSpeedMax();
      }
    }
    if(nearest_opp){
      if(nearest_opp->playerTypePtr()){
	arrival_cycle_opp = mate_better_point.dist(nearest_opp->pos()) / nearest_opp->playerTypePtr()->playerSpeedMax();
	safe_dist = arrival_cycle_len * nearest_opp->playerTypePtr()->playerSpeedMax(); 
      }else{
	arrival_cycle_opp = mate_better_point.dist(nearest_opp->pos()) / rcsc::ServerParam::i().defaultPlayerSpeedMax();
      }
    }

    if(world.countOpponentsIn(rcsc::Sector2D(world.self().pos()
					     ,0.0,world.self().pos().dist(mate_better_point)
					     ,(mate_better_point - world.self().pos()).dir().degree() - 30.0
					     ,(mate_better_point - world.self().pos()).dir().degree() + 30.0
					     )
			      ,5,false) <= 0
       && world.countOpponentsIn(rcsc::Circle2D(mate_better_point
						,safe_dist
						)
				 ,5,false) <= 1
       && world.self().pos().dist(mate_better_point) < 35
       && arrival_cycle_mate <= arrival_cycle_opp + 2){
      valid = true;

      break;
    }
  }

  if(valid){
    if(target_point)
      *target_point = mate_better_point;
    if(first_speed)
      *first_speed = better_speed;
    if(receiver_unum)
      *receiver_unum = mate_better_unum;
  }
  return valid;
}

bool 
Bhv_FSThroughPassKick::verifyPassCource(const rcsc::WorldModel & world
					, const rcsc::Vector2D & target_point 
					, double * first_speed 
					)
{
  rcsc::Vector2D better_point = target_point;
  double better_speed = rcsc::ServerParam::i().ballSpeedMax();
  if(first_speed)
    better_speed = *first_speed;
  bool valid = false;
  
  double end_speed = 0.7;
  better_speed
    = rcsc::calc_first_term_geom_series_last
    ( end_speed,
      world.self().pos().dist(better_point),
      rcsc::ServerParam::i().ballDecay() );
  better_speed = std::min(better_speed,rcsc::ServerParam::i().ballSpeedMax());    
  //ボールの到達時間を計算
  double arrival_cycle_len 
    = rcsc::calc_length_geom_series( better_speed,
				     world.ball().pos().dist(better_point),
				     rcsc::ServerParam::i().ballDecay() );
  if(arrival_cycle_len < 0)
    return false;
    
  double mate_dist;
  double opp_dist;
  const rcsc::PlayerObject * 
    nearest_mate = world.getTeammateNearestTo(better_point
					      ,5
					      ,&mate_dist);
  const rcsc::PlayerObject * 
    nearest_opp = world.getOpponentNearestTo(better_point
					     ,5
					     ,&opp_dist);
  double arrival_cycle_mate = 100;
  double arrival_cycle_opp = 100;
  double safe_dist 
    = arrival_cycle_len * rcsc::ServerParam::i().defaultPlayerSpeedMax();
  if(nearest_mate){
    if(nearest_mate->playerTypePtr()){
      arrival_cycle_mate = better_point.dist(nearest_mate->pos()) / nearest_mate->playerTypePtr()->playerSpeedMax();
    }else{
      arrival_cycle_mate = better_point.dist(nearest_mate->pos()) / rcsc::ServerParam::i().defaultPlayerSpeedMax();
    }
  }
  if(nearest_opp){
    if(nearest_opp->playerTypePtr()){
      arrival_cycle_opp = better_point.dist(nearest_opp->pos()) / nearest_opp->playerTypePtr()->playerSpeedMax();
      safe_dist = arrival_cycle_len * nearest_opp->playerTypePtr()->playerSpeedMax(); 
    }else{
      arrival_cycle_opp = better_point.dist(nearest_opp->pos()) / rcsc::ServerParam::i().defaultPlayerSpeedMax();
    }
  }
  
  if(world.countOpponentsIn(rcsc::Sector2D(world.self().pos()
					   ,0.0,world.self().pos().dist(better_point)
					   ,(better_point - world.self().pos()).dir().degree() - 30.0
					   ,(better_point - world.self().pos()).dir().degree() + 30.0
					   )
			    ,5,false) <= 0
     && world.countOpponentsIn(rcsc::Circle2D(better_point
					      ,safe_dist
					      )
			       ,5,false) <= 1
     && world.self().pos().dist(better_point) < 35
     && arrival_cycle_mate <= arrival_cycle_opp + 2){
    valid = true;
  }

  if(valid){
    if(first_speed)
      *first_speed = better_speed;
  }

  return valid;
}
