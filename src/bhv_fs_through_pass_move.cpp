
/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "bhv_fs_through_pass_move.h"

#include <rcsc/player/player_agent.h>

#include <rcsc/common/server_param.h>

#include <rcsc/player/debug_client.h>
#include <rcsc/common/logger.h>
#include <rcsc/player/say_message_builder.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/player/free_message.h>
#include <rcsc/action/neck_turn_to_ball.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/common/audio_codec.h>
#include <rcsc/action/body_intercept.h>

#include <boost/tokenizer.hpp>

#include <rcsc/action/neck_turn_to_low_conf_teammate.h>

/*-------------------------------------------------------------------*/
/*!

*/

bool
Bhv_FSThroughPassMove::execute( rcsc::PlayerAgent * agent )
{
  /////////////////////////////////////////////////////////////////////
  //
  rcsc::dlog.addText( rcsc::Logger::ACTION,
		      "%s:%d: FSThroughPassMove. execute()"
		      ,__FILE__, __LINE__ );
  
  if ( agent->world().self().isKickable() 
       || agent->world().existKickableOpponent())
    {
      return false;
    }
  
  static int S_catched_cycle = -1;
  static int S_reconfirmed_cycle = -1;
  static int S_reconfirmed_counter = 0;
  static rcsc::Vector2D S_target_point(0.0,0.0);

  if(S_catched_cycle > agent->world().time().cycle() - 10){
    if((S_reconfirmed_cycle == -1
       || S_reconfirmed_cycle < agent->world().time().cycle() - 10
	)&& agent->world().audioMemory().freeMessageTime().cycle()
       > agent->world().time().cycle() - 3){
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

	if(mes_it->message_=="TPKK"){
	  S_reconfirmed_cycle = agent->world().time().cycle();
	  S_reconfirmed_counter=0;
	  break;
	}
      }
    }
    if(S_reconfirmed_cycle + 5 < S_catched_cycle){
      S_reconfirmed_counter++;
      if(S_reconfirmed_counter > 3){
	S_reconfirmed_counter=0;
	return false;
      }
    }
  }

  if(S_catched_cycle > agent->world().time().cycle() - 20){
    //ボールがマーク相手にきそうな時は、ボールに向かっていく
    for(int i=0;i<7;i++){
      if(agent->world().ball().inertiaPoint(i).dist(S_target_point) < 1.5
	 && !agent->world().existKickableTeammate()){
	bool save_recovery=false;
	rcsc::dlog.addText( rcsc::Logger::TEAM,
			    "%s:%d: intercept"
			    ,__FILE__, __LINE__ );
	rcsc::Body_Intercept2009(save_recovery).execute( agent );
	agent->setNeckAction( new rcsc::Neck_TurnToBall() );
/*	std::cout << agent->world().self().unum()
		  << ":[" << agent->world().time().cycle()
		  << "] FSThroughPassMove(intercept): (" 
		  << S_target_point.x << "," << S_target_point.y 
		  << "): now(" 
		  << agent->world().self().pos().x << "," 
		  << agent->world().self().pos().y << ")" << std::endl;*/
	return true;
      }
    }
  }

  if( (S_catched_cycle == -1
       || S_catched_cycle < agent->world().time().cycle() - 5)
      && agent->world().audioMemory().freeMessageTime().cycle()
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
	 && *token_it == "TP"){
	++token_it;
	if(token_it == tokens.end()
	   || *token_it == "KK" || *token_it == "OK")
	  continue;
	S_target_point = rcsc::AudioCodec::i().decodeStr4ToPos(*token_it);
	double first_speed = rcsc::ServerParam::i().ballSpeedMax();
	double end_speed = 0.7;
	first_speed
	  = rcsc::calc_first_term_geom_series_last
	  ( end_speed,
	    agent->world().ball().pos().dist(S_target_point),
	    rcsc::ServerParam::i().ballDecay() );
	first_speed = std::min(first_speed,rcsc::ServerParam::i().ballSpeedMax());

	//ボールの到達時間を計算
	double arrival_cycle_len 
	  = rcsc::calc_length_geom_series( first_speed,
					   agent->world().ball().pos().dist(S_target_point),
					   rcsc::ServerParam::i().ballDecay() );
	if(arrival_cycle_len < 0)
	  break;

	if(arrival_cycle_len + 2 > agent->world().self().pos().dist(S_target_point)/agent->world().self().playerType().playerSpeedMax()){

	  agent->addSayMessage( new rcsc::FreeMessage<4>("TPOK") );
	  S_catched_cycle = agent->world().time().cycle();
	  S_reconfirmed_counter=0;
/*	  std::cout << agent->world().self().unum()
		    << ":[" << agent->world().time().cycle()
		    << "] FSThroughPassMove:" << mes_it->sender_ 
		    << ":" << mes_it->message_ << " -> (" 
		    << S_target_point.x << "," << S_target_point.y 
		    << ")" << std::endl;*/
	  break;
	}
      }
    }
  }
  if(S_catched_cycle > agent->world().time().cycle() - 10){
    rcsc::Body_GoToPoint(S_target_point,0.25
			 ,rcsc::ServerParam::i().maxDashPower()
			 ,100,false,false,30
			 ).execute(agent);
    agent->setNeckAction( new rcsc::Neck_TurnToBall() );
/*    std::cout << agent->world().self().unum()
	      << ":[" << agent->world().time().cycle()
	      << "] FSThroughPassMove(goToPoint): (" 
	      << S_target_point.x << "," << S_target_point.y 
	      << "): now(" 
	      << agent->world().self().pos().x << "," 
	      << agent->world().self().pos().y << ")" << std::endl;*/
    return true;
  }
  
  return false;
}
