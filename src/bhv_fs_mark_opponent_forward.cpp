
/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "bhv_fs_mark_opponent_forward.h"

#include <rcsc/player/player_agent.h>

#include <rcsc/action/body_smart_kick.h>
#include <rcsc/action/neck_turn_to_ball.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_scan_field.h>

#include <rcsc/common/server_param.h>

#include <rcsc/geom/sector_2d.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_turn_to_point.h>
#include <rcsc/action/body_turn_to_ball.h>

#include "bhv_basic_move.h"
#include <rcsc/common/logger.h>
#include <rcsc/action/body_intercept.h>
#include "bhv_basic_tackle.h"

#include <rcsc/player/say_message_builder.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/player/free_message.h>
#include <rcsc/common/audio_codec.h>

#include <boost/tokenizer.hpp>

#include <rcsc/action/neck_turn_to_player_or_scan.h>
#include <rcsc/action/neck_turn_to_ball_and_player.h>

/*-------------------------------------------------------------------*/
/*!

*/

bool
Bhv_FSMarkOpponentForward::execute( rcsc::PlayerAgent * agent ){
  const rcsc::WorldModel & wm = agent->world();

  confirmMarkMembers(agent);

  if ( wm.self().stamina() < rcsc::ServerParam::i().staminaMax() * 0.5 )
    return false;

  if(!decideMarkTarget(agent))
    return v1(agent);

  if(!wm.existKickableTeammate())
    if( Bhv_BasicTackle(0.5,90.0).execute(agent) )
      return true;

  int self_min = wm.interceptTable()->selfReachCycle();
  int mate_min = wm.interceptTable()->teammateReachCycle();
  int opp_min = wm.interceptTable()->opponentReachCycle();
  
  //offensive intercept
  if ( ! wm.existKickableTeammate()
       && wm.ball().pos().x > -40
       && ( self_min <= 4
	    || ( self_min < mate_min + 1
		 && self_min < opp_min + 4 )
	    )
       )	
    {
      rcsc::dlog.addText( rcsc::Logger::TEAM,
			  "%s:%d:offensive intercept"
			  ,__FILE__, __LINE__ );
      rcsc::Body_Intercept2009().execute( agent );
      agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
      return true;
    }

  const rcsc::AbstractPlayerObject * target = getMarkPlayer(agent);
  if(!target)
    return false;

  rcsc::Vector2D target_point = target->pos();
  target_point += target->vel();
  rcsc::Vector2D mark_point = target_point;
  mark_point.x = (4 * target_point.x + agent->world().ball().pos().x)/5;
  mark_point.y = (4 * target_point.y + agent->world().ball().pos().y)/5;
  rcsc::AngleDeg mark_angle = (mark_point - target_point).dir();

  //if(mark_angle > 0)
  //mark_angle 
  mark_point = 
    target_point + rcsc::Vector2D::polar2vector(target_point.dist(mark_point)
						  ,mark_angle);
  //ボールがマーク相手にきそうな時は、ボールに向かっていく
  for(int i=0;i<7;i++){
    if(wm.ball().inertiaPoint(i).dist(target_point) < 1.5){
      bool save_recovery=false;
      rcsc::dlog.addText( rcsc::Logger::TEAM,
			  "%s:%d: intercept"
			  ,__FILE__, __LINE__ );
      rcsc::Body_Intercept2009(save_recovery).execute( agent );
      agent->setNeckAction( new rcsc::Neck_TurnToBall() );
      return true;
    }
  }

  double dash_power =  rcsc::ServerParam::i().maxDashPower();
  bool save_recovery = false;
  if(wm.ball().pos().x > -1
     && target_point.dist(wm.ball().pos()) > 30){
      dash_power =Bhv_BasicMove::getDashPower( agent, mark_point );
      save_recovery = true;
    }


  double dist_thr = wm.ball().distFromSelf() * 0.1;
  if ( dist_thr < 1.0 ) dist_thr = 1.0;
  
  agent->debugClient().addMessage( " PassCutMove%.0f", dash_power );
  agent->debugClient().setTarget( mark_point );
  
  if ( ! rcsc::Body_GoToPoint( mark_point, dist_thr, dash_power
			       ,100,false,save_recovery
			       ).execute( agent ) )
    {
      rcsc::Body_TurnToPoint(mark_point).execute( agent );
    }
  /*
  std::cout << wm.self().unum() << ":[" << wm.time().cycle()
	    << "] opponent forward mark: "
	    << target->unum() << "(" 
	    << mark_point.x << "," << mark_point.y << ")"
	    << ": self pos:("
	    << wm.self().pos().x << "," << wm.self().pos().y << ")"
	    << ": ball pos:("
	    << wm.ball().pos().x << "," << wm.ball().pos().y << ")"
	    << std::endl;
  */
  if(target->isGhost())
    {
      agent->setNeckAction( new rcsc::Neck_TurnToPlayerOrScan(target) );
    }
  else  if ( wm.existKickableOpponent()
	      && wm.ball().distFromSelf() < 18.0 )
    {
      agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
    }
  else
    {
      agent->setNeckAction( new rcsc::Neck_TurnToBallAndPlayer(target) );
    }

  return true;
}

bool
Bhv_FSMarkOpponentForward::v1( rcsc::PlayerAgent * agent ){
  const rcsc::WorldModel & wm = agent->world();

  ////////////////////////////////////////////////////////////////////////
  //ホームポジションから離れすぎてるときは、reject
  if(wm.self().pos().dist(M_home_pos) > 25)
    return false;

  if(!wm.existKickableTeammate())
    if( Bhv_BasicTackle(0.5,90.0).execute(agent) )
      return true;

  int self_min = wm.interceptTable()->selfReachCycle();
  int mate_min = wm.interceptTable()->teammateReachCycle();
  int opp_min = wm.interceptTable()->opponentReachCycle();
  
  //offensive intercept
  if ( ! wm.existKickableTeammate()
       && wm.ball().pos().x > -40
       && ( self_min <= 4
	    || ( self_min < mate_min + 1
		 && self_min < opp_min + 4 )
	    )
       )	
    {
      rcsc::dlog.addText( rcsc::Logger::TEAM,
			  "%s:%d:offensive intercept"
			  ,__FILE__, __LINE__ );
      rcsc::Body_Intercept2009().execute( agent );
      agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
      return true;
    }

  ////////////////////////////////////////////////////////////////////////
  //マーク対象を探す
  rcsc::Vector2D mark_point = wm.ball().pos();
  bool mark_flag = false;

  rcsc::Vector2D first_opp_pos = rcsc::ServerParam::i().theirTeamGoalPos();
  rcsc::Vector2D second_opp_pos = rcsc::ServerParam::i().theirTeamGoalPos();
  rcsc::Vector2D third_opp_pos = rcsc::ServerParam::i().theirTeamGoalPos();
  rcsc::Vector2D fourth_opp_pos = rcsc::ServerParam::i().theirTeamGoalPos();
  const rcsc::PlayerPtrCont::const_iterator
    opps_end = wm.opponentsFromSelf().end();
  for ( rcsc::PlayerPtrCont::const_iterator
	  opps_it = wm.opponentsFromSelf().begin();
	opps_it != opps_end; ++opps_it ){
    if(!(*opps_it)->isKickable()){
      if(first_opp_pos.x > (*opps_it)->pos().x){
	fourth_opp_pos = third_opp_pos;
	third_opp_pos = second_opp_pos;
	second_opp_pos = first_opp_pos;
	first_opp_pos = (*opps_it)->pos();
      }else if(second_opp_pos.x > (*opps_it)->pos().x){
	fourth_opp_pos = third_opp_pos;
	third_opp_pos = second_opp_pos;
	second_opp_pos = (*opps_it)->pos();
      }else if(third_opp_pos.x  > (*opps_it)->pos().x){
	fourth_opp_pos = third_opp_pos;
	third_opp_pos = (*opps_it)->pos();
      }else if(fourth_opp_pos.x > (*opps_it)->pos().x){
	fourth_opp_pos = (*opps_it)->pos();
      }
    }
  }

  //first
  if(!mark_flag
     && first_opp_pos.dist(wm.ball().pos()) < 30
     && first_opp_pos.dist(wm.self().pos()) < 20
     && first_opp_pos.dist(M_home_pos) < 25
     && first_opp_pos.x < wm.ball().pos().x -10
     && first_opp_pos.y * wm.self().pos().y > 0
    ){
    
    //味方がすでに、その敵をマークしてないとき、その敵をマークする
    const rcsc::PlayerObject * nearest_mate 
      = wm.getTeammateNearestTo(first_opp_pos,5,NULL);
    if(nearest_mate == NULL
       || (nearest_mate->pos().dist(first_opp_pos) * 0.6 > 
	   wm.self().pos().dist(first_opp_pos)
	   || nearest_mate->unum() < wm.self().unum()
	   && nearest_mate->pos().dist(first_opp_pos) * 2.0 > 
	   wm.self().pos().dist(first_opp_pos)
	   )
       ){
      mark_flag = true;
      mark_point.x = (9 * first_opp_pos.x + wm.ball().pos().x) / 10;
      mark_point.y = (9 * first_opp_pos.y + wm.ball().pos().y) / 10;
    }
  }

  //second
  if(!mark_flag
     && second_opp_pos.dist(wm.ball().pos()) < 30
     && second_opp_pos.dist(wm.self().pos()) < 20
     && second_opp_pos.dist(M_home_pos) < 25
     && second_opp_pos.x - first_opp_pos.x < 15
     && second_opp_pos.x < wm.ball().pos().x -10
     && second_opp_pos.y * wm.self().pos().y > 0
    ){
    
    //味方がすでに、その敵をマークしてないとき、その敵をマークする
    const rcsc::PlayerObject * nearest_mate 
      = wm.getTeammateNearestTo(second_opp_pos,5,NULL);
    if(nearest_mate == NULL
       || (nearest_mate->pos().dist(second_opp_pos) * 0.6 > 
	   wm.self().pos().dist(second_opp_pos)
	   || nearest_mate->unum() > wm.self().unum()
	   && nearest_mate->pos().dist(second_opp_pos) * 2.0 > 
	   wm.self().pos().dist(second_opp_pos)
	   )
       ){
      mark_flag = true;
      mark_point.x = (9 * second_opp_pos.x + wm.ball().pos().x) / 10;
      mark_point.y = (9 * second_opp_pos.y + wm.ball().pos().y) / 10;
    }
  }
  //third
  if(!mark_flag
     && third_opp_pos.dist(wm.ball().pos()) < 30
     && third_opp_pos.dist(wm.self().pos()) < 20
     && third_opp_pos.dist(M_home_pos) < 25
     && third_opp_pos.x - first_opp_pos.x < 15
     && third_opp_pos.x < wm.ball().pos().x -10
     && third_opp_pos.y * wm.self().pos().y > 0
    ){
    
    //味方がすでに、その敵をマークしてないとき、その敵をマークする
    const rcsc::PlayerObject * nearest_mate 
      = wm.getTeammateNearestTo(third_opp_pos,5,NULL);
    if(nearest_mate == NULL
       || (nearest_mate->pos().dist(third_opp_pos) * 0.6 > 
	   wm.self().pos().dist(third_opp_pos)
	   || nearest_mate->unum() > wm.self().unum()
	   && nearest_mate->pos().dist(third_opp_pos) * 2.0 > 
	   wm.self().pos().dist(third_opp_pos)
	   )
       ){
      mark_flag = true;
      mark_point.x = (9 * third_opp_pos.x + wm.ball().pos().x) / 10;
      mark_point.y = (9 * third_opp_pos.y + wm.ball().pos().y) / 10;
    }
  }

  //fourth
  if(!mark_flag
     && fourth_opp_pos.dist(wm.ball().pos()) < 30
     && fourth_opp_pos.dist(wm.self().pos()) < 20
     && fourth_opp_pos.dist(M_home_pos) < 25
     && fourth_opp_pos.x - first_opp_pos.x < 15
     && fourth_opp_pos.x < wm.ball().pos().x - 10
     && fourth_opp_pos.y * wm.self().pos().y > 0
     ){
    
    //味方がすでに、その敵をマークしてないとき、その敵をマークする
    const rcsc::PlayerObject * nearest_mate 
      = wm.getTeammateNearestTo(fourth_opp_pos,5,NULL);
    if(nearest_mate == NULL
       || (nearest_mate->pos().dist(fourth_opp_pos) * 0.6 > 
	   wm.self().pos().dist(fourth_opp_pos)
	   || nearest_mate->unum() > wm.self().unum()
	   && nearest_mate->pos().dist(fourth_opp_pos) * 2.0 > 
	   wm.self().pos().dist(fourth_opp_pos)
	   )
       ){
      mark_flag = true;
      mark_point.x = (9 * fourth_opp_pos.x + wm.ball().pos().x) / 10;
      mark_point.y = (9 * fourth_opp_pos.y + wm.ball().pos().y) / 10;
    }
  }

  if(!mark_flag)
    return false;

  //ボールがマーク相手にきそうな時は、ボールに向かっていく
  for(int i=0;i<7;i++){
    if(wm.ball().inertiaPoint(i).dist(mark_point) < 1.5){
      bool save_recovery=false;
      rcsc::dlog.addText( rcsc::Logger::TEAM,
			  "%s:%d: intercept"
			  ,__FILE__, __LINE__ );
      rcsc::Body_Intercept2009(save_recovery).execute( agent );
      agent->setNeckAction( new rcsc::Neck_TurnToBall() );
      return true;
    }
  }

  const double dash_power = Bhv_BasicMove::getDashPower( agent, mark_point );
  
  double dist_thr = wm.ball().distFromSelf() * 0.1;
  if ( dist_thr < 1.0 ) dist_thr = 1.0;
  
  agent->debugClient().addMessage( " PassCutMove%.0f", dash_power );
  agent->debugClient().setTarget( mark_point );
  
  if ( ! rcsc::Body_GoToPoint( mark_point, dist_thr, dash_power
			       ).execute( agent ) )
    {
      rcsc::Body_TurnToBall().execute( agent );
    }
  /*
  std::cout << wm.self().unum() << ":[" << wm.time().cycle()
	    << "] opponent forward mark(v1):(" 
	    << mark_point.x << "," << mark_point.y << ")"
	    << ": ball pos:("
	    << wm.ball().pos().x << "," << wm.ball().pos().y << ")"
	    << std::endl;
  */
  if ( wm.existKickableOpponent()
       && wm.ball().distFromSelf() < 18.0 )
    {
      agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
    }
  else
    {
      agent->setNeckAction( new rcsc::Neck_ScanField() );
    }
    
  return true;
}

bool
Bhv_FSMarkOpponentForward::confirmMarkMembers( rcsc::PlayerAgent * agent )
{
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
      /*
      std::cout << agent->world().self().unum() << ":[" 
		<< agent->world().time().cycle() 
		<< "] FSMarkOpponentForward message: "
		<< "sender:" << mes_it->sender_ 
		<< " message:" << mes_it->message_
		<< std::endl;
      */
      if(mes_it->sender_ == rcsc::Unum_Unknown)
	continue;
      
      // 3文字、1文字、1文字に分割してみる。
      const int offsets[] = {3,1,1};
      boost::offset_separator oss( offsets, offsets+3 );
      boost::tokenizer<boost::offset_separator> tokens( mes_it->message_, oss );
      boost::tokenizer<boost::offset_separator>::iterator 
	token_it = tokens.begin();
      
      if(token_it != tokens.end()
	 && *token_it == "MFI"){
	++token_it;
	if(token_it == tokens.end())
	  continue;
	int member_unum = rcsc::AudioCodec::hex2unum((*token_it->c_str()));
	if(member_unum != mes_it->sender_
	   || member_unum == rcsc::Unum_Unknown)
	  continue;
	
	++token_it;
	if(token_it == tokens.end())
	  continue;
	int target_unum = rcsc::AudioCodec::hex2unum(*(token_it->c_str()));
	if(target_unum == rcsc::Unum_Unknown)
	  continue;
	
	bool found = false;
	const std::vector<FSPlayerInfo>::iterator members_end = members(NULL).end();
	for(std::vector<FSPlayerInfo>::iterator members_it = members(NULL).begin();
	    members_it != members_end;
	    ++members_it){
	  if(members_it->unum() == member_unum){
	    found = true;
	    break;
	  }
	}
	if(!found){
	  FSPlayerInfo member(member_unum,agent->world().ourSide());
	  members(&member);
	}
	
	found = false;
	const std::vector<FSPlayerInfo>::iterator targets_end = targets(NULL).end();
	for(std::vector<FSPlayerInfo>::iterator targets_it = targets(NULL).begin();
	    targets_it != targets_end;
	    ++targets_it){
	  if(targets_it->unum() == target_unum){
	    found = true;
	    break;
	  }
	}
	if(!found){
	  FSPlayerInfo target(target_unum,agent->world().theirSide());
	  targets(&target);
	}
	if(member_unum != rcsc::Unum_Unknown 
	   && target_unum != rcsc::Unum_Unknown)
	  relations(&member_unum,&target_unum);
	
	break;
      }
    }
  }
  
  const std::vector<FSPlayerInfo>::iterator members_end = members(NULL).end();
  for(std::vector<FSPlayerInfo>::iterator members_it = members(NULL).begin();
      members_it != members_end;
      ++members_it){
    members_it->update(agent);
  }
  const std::vector<FSPlayerInfo>::iterator targets_end = targets(NULL).end();
  for(std::vector<FSPlayerInfo>::iterator targets_it = targets(NULL).begin();
      targets_it != targets_end;
      ++targets_it){
    targets_it->update(agent);
  }
  return true;
}

bool 
Bhv_FSMarkOpponentForward::decideMarkTarget( rcsc::PlayerAgent * agent )
{
  static int S_last_decision_cycle = -1;
  static bool S_init = false;
  static int S_last_cycle = -1;

  //最初に行う処理
  if(!S_init
     && S_last_cycle != agent->world().time().cycle()){
    const rcsc::PlayerPtrCont::const_iterator
      opps_end = agent->world().opponentsFromSelf().end();
    for ( rcsc::PlayerPtrCont::const_iterator
	    opps_it = agent->world().opponentsFromSelf().begin();
	  opps_it != opps_end; ++opps_it ){
      int target_unum = (*opps_it)->unum();
      if((*opps_it)->pos().x < -1
	 && target_unum != rcsc::Unum_Unknown){
	bool found = false;
	const std::vector<FSPlayerInfo>::iterator 
	  targets_end = targets(NULL).end();
	for(std::vector<FSPlayerInfo>::iterator 
	      targets_it = targets(NULL).begin();
	    targets_it != targets_end;
	    ++targets_it){
	  if(targets_it->unum() == target_unum){
	    found = true;
	    break;
	  }
	}
	if(!found){
	  FSPlayerInfo target(target_unum,agent->world().theirSide());
	  targets(&target);
	}
      }
    }
    bool found = false;
    const std::vector<FSPlayerInfo>::iterator 
      members_end = members(NULL).end();
    for(std::vector<FSPlayerInfo>::iterator 
	  members_it = members(NULL).begin();
	members_it != members_end;
	++members_it){
      if(members_it->unum() == agent->world().self().unum()){
	found = true;
	break;
      }
    }
    if(!found){
      FSPlayerInfo member(agent->world().self().unum()
			   ,agent->world().ourSide());
      members(&member);
    }
    S_init = true;
  }

  //100サイクルごとに、マーク相手を見直す
  bool marked = false;
  const std::map<int,int>::iterator 
    relations_end = relations(NULL,NULL).end();
  for(std::map<int,int>::iterator 
	relations_it = relations(NULL,NULL).begin();
      relations_it != relations_end;
      ++relations_it){
    if(relations_it->second == target_unum(NULL)){
      marked = true;
      break;
    }
  }
  if(S_last_cycle != agent->world().time().cycle()
     &&(agent->world().time().cycle() / 20)% 10 == (agent->world().self().unum() - 2)     
     && (agent->world().time().cycle() - S_last_decision_cycle > 100
	 || marked)){
    bool found_target = false;

    double self_ave_y = agent->world().self().pos().y;
    const std::vector<FSPlayerInfo>::iterator
      members_end = members(NULL).end();
    for(std::vector<FSPlayerInfo>::iterator
	  members_it = members(NULL).begin();
	members_it != members_end;
	++members_it){
      if(members_it->unum() == agent->world().self().unum()){
	self_ave_y = members_it->aveY();
	break;
      }
    }
    //y座標マイナス側からの自分の順番を出す。
    int order_from_left = 0;
    for(std::vector<FSPlayerInfo>::iterator
	  members_it = members(NULL).begin();
	members_it != members_end;
	++members_it){
      if(members_it->unum() != agent->world().self().unum()
	 && members_it->aveY() < self_ave_y){
	order_from_left++;
      }
    }
    
    double target_min_ave_x = -1.0;
    int target_min_unum = rcsc::Unum_Unknown;
    const std::vector<FSPlayerInfo>::iterator 
      targets_end = targets(NULL).end();
    for(std::vector<FSPlayerInfo>::iterator 
	  targets_it = targets(NULL).begin();
	targets_it != targets_end;
	++targets_it){
      if(targets_it->aveX() < target_min_ave_x){
	target_min_ave_x = targets_it->aveX();
	target_min_unum = targets_it->unum();
      }
    }
 
    marked = false;
    for(std::map<int,int>::iterator relations_it = relations(NULL,NULL).begin();
	relations_it != relations_end;
	++relations_it){
      if(relations_it->second == target_min_unum){
	marked = true;
	break;
      }
    }
    if(!marked && target_min_unum != rcsc::Unum_Unknown){
      const rcsc::AbstractPlayerObject * 
	target_min = agent->world().theirPlayer(target_min_unum);
      if(target_min && target_min->pos().dist(agent->world().self().pos()) < 15.0){
	found_target = true;
	target_unum(&target_min_unum);
      }
    }
    
    if(!found_target){
      double target_min_ave_x = -1.0;
      double target_second_ave_x = -1.0;
      double target_third_ave_x = -1.0;
      double target_second_ave_y = 0.0;
      double target_third_ave_y = 0.0;
      int target_second_unum = rcsc::Unum_Unknown;
      int target_third_unum = rcsc::Unum_Unknown;
      const std::vector<FSPlayerInfo>::iterator 
	targets_end = targets(NULL).end();
      for(std::vector<FSPlayerInfo>::iterator 
	    targets_it = targets(NULL).begin();
	  targets_it != targets_end;
	  ++targets_it){
	if(targets_it->aveX() < target_min_ave_x){
	  target_third_ave_x = target_second_ave_x;
	  target_second_ave_x = target_min_ave_x;
	  target_min_ave_x = targets_it->aveX();
	}else if(targets_it->aveX() < target_second_ave_x){
	  target_third_ave_x = target_second_ave_x;
	  target_second_ave_x = targets_it->aveX();
	  target_second_ave_y = targets_it->aveY();
	  target_second_unum = targets_it->unum();
	}else if(targets_it->aveX() < target_third_ave_x){
	  target_third_ave_x = targets_it->aveX();
	  target_third_ave_y = targets_it->aveY();
	  target_third_unum = targets_it->unum();
	}
      }
      if(target_third_ave_x - target_second_ave_x < 3.5
	 && target_third_ave_y * ((double)order_from_left - (double)members(NULL).size()/2) > 0){
	int swap = target_second_unum;
	target_second_unum = target_third_unum;
	target_third_unum = swap;
      }

      marked = false;
      for(std::map<int,int>::iterator 
	    relations_it = relations(NULL,NULL).begin();
	  relations_it != relations_end;
	  ++relations_it){
	if(relations_it->second == target_second_unum){
	  marked = true;
	  break;
	}
      }
      if(!marked && target_second_unum != rcsc::Unum_Unknown){
	const rcsc::AbstractPlayerObject * 
	  target_second = agent->world().theirPlayer(target_second_unum);
	if(target_second && target_second->pos().dist(agent->world().self().pos()) < 15.0){
	  found_target = true;
	  target_unum(&target_second_unum);
	}
      }
      if(!found_target){
	marked = false;
	for(std::map<int,int>::iterator 
	      relations_it = relations(NULL,NULL).begin();
	    relations_it != relations_end;
	    ++relations_it){
	  if(relations_it->second == target_third_unum){
	    marked = true;
	    break;
	  }
	}
	if(!marked && target_third_unum != rcsc::Unum_Unknown){
	  const rcsc::AbstractPlayerObject * 
	    target_third = agent->world().theirPlayer(target_third_unum);
	  if(target_third && target_third->pos().dist(agent->world().self().pos()) < 15.0){
	    found_target = true;
	    target_unum(&target_third_unum);
	  }
	}
      }
    }
    
    if(!marked && found_target){
      std::string message("MFI");
      message += rcsc::AudioCodec::unum2hex(agent->world().self().unum());
      message += rcsc::AudioCodec::unum2hex(target_unum(NULL));
      if(message.size() == 5
	 &&rcsc::AudioCodec::unum2hex(agent->world().self().unum())!='\0'
	 &&rcsc::AudioCodec::unum2hex(target_unum(NULL))!='\0'){
	agent->addSayMessage( new rcsc::FreeMessage<5>(message));
/*	std::cout << agent->world().self().unum() << ":[" 
		  << agent->world().time().cycle()
		  << "] FSMarkOpponentForward dicide target: " 
		  << target_unum(NULL) << " -> " << message 
		  << std::endl
		  << "relations: ";*/
	for(std::map<int,int>::iterator relations_it = relations(NULL,NULL).begin();
	    relations_it != relations_end;
	    ++relations_it){
/*	  std::cout << "(" << relations_it->first << " -> "
		    << relations_it->second << ") ";*/
	}
//	std::cout << std::endl;
	S_last_decision_cycle = agent->world().time().cycle();
	return true;
      }
    }else{
      S_init = false;
    }
  }
  /*
else if(S_last_cycle != agent->world().time().cycle()
	   && target_unum(NULL) != rcsc::Unum_Unknown
	   &&((agent->world().time().cycle() / 10)% 10 == (agent->world().self().unum() - 2)
	      || agent->world().existTeammateIn(rcsc::Circle2D(agent->world().self().pos()
							       ,2.0)
						,5,false))
	   ){
    std::string message("MFI");
    message += rcsc::AudioCodec::unum2hex(agent->world().self().unum());
    message += rcsc::AudioCodec::unum2hex(target_unum(NULL));
    if(message.size() == 5
       &&rcsc::AudioCodec::unum2hex(agent->world().self().unum())!='\0'
       &&rcsc::AudioCodec::unum2hex(target_unum(NULL))!='\0'){
      agent->addSayMessage( new rcsc::FreeMessage<5>(message));
      
      std::cout << agent->world().self().unum() << ":[" 
		<< agent->world().time().cycle()
		<< "] FSMarkOpponentForward say target again: " 
		<< target_unum(NULL) << " -> " << message 
		<< std::endl;
      
    }
  }
*/
    
  S_last_cycle = agent->world().time().cycle();

  if(S_last_decision_cycle == -1
     || agent->world().time().cycle() - S_last_decision_cycle > 150)
    return false;

  if(target_unum(NULL) != rcsc::Unum_Unknown)
    return true;

  return false;
}

const rcsc::AbstractPlayerObject * 
Bhv_FSMarkOpponentForward::getMarkPlayer( rcsc::PlayerAgent * agent ) 
{
  return agent->world().theirPlayer(target_unum(NULL));
}
