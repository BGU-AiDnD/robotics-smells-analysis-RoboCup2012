
////////////////////////////////////////////////////////////////////

#ifndef BHV_FS_MARK_OPPONENT_FORWARD_H
#define BHV_FS_MARK_OPPONENT_FORWARD_H

#include <rcsc/player/soccer_action.h>
#include <rcsc/geom/vector_2d.h>
#include <vector>
#include <rcsc/types.h>
#include <map>
#include <rcsc/player/player_agent.h>

class Bhv_FSMarkOpponentForward
: public rcsc::SoccerBehavior {
  
 private:

  class FSPlayerInfo{
  private:
    int M_unum;
    rcsc::SideID M_side;
    double M_sum_x;
    double M_sum_y;
    int M_total_count;
    int M_reach_ball_count;

  public:
    FSPlayerInfo( int unum , rcsc::SideID side ){
      M_unum = unum;
      M_side = side;
      M_sum_x = 0.0;
      M_sum_y = 0.0;
      M_total_count = 0;
      M_reach_ball_count = 0;
    }
    bool update( const rcsc::PlayerAgent * agent ){
      if(M_side == agent->world().ourSide()){
	const rcsc::AbstractPlayerObject * 
	  mate = agent->world().ourPlayer(M_unum);
	if(!mate)
	  return false;

	M_sum_x += mate->pos().x;
	M_sum_y += mate->pos().y;
	M_total_count++;
	if(mate->pos().dist(agent->world().ball().pos()) < 2.5)
	  M_reach_ball_count++;
	return true;
      }else if(M_side == agent->world().theirSide()){
	const rcsc::AbstractPlayerObject *
	  opp = agent->world().theirPlayer(M_unum);
	if(!opp)
	  return false;

	M_sum_x += opp->pos().x;
	M_sum_y += opp->pos().y;
	M_total_count++;
	if(opp->pos().dist(agent->world().ball().pos()) < 2.5)
	  M_reach_ball_count++;
	return true;
      }
      return false;
    }
    int unum(){
      return M_unum;
    }
    rcsc::SideID side(){
      return M_side;
    }
    double aveX(){
      if(M_total_count==0)
	return 0.0;
      return M_sum_x/M_total_count;
    }
    double aveY(){
      if(M_total_count==0)
	return 0.0;
      return M_sum_y/M_total_count;
    }
    int countReachBall(){
      return M_reach_ball_count;
    }
  };
  
  rcsc::Vector2D M_home_pos;

  std::vector<FSPlayerInfo> & members( FSPlayerInfo * member ){
    static std::vector<FSPlayerInfo> S_members;
    if(member)
      S_members.push_back(*member);

    return S_members;
  }
   
  std::vector<FSPlayerInfo> & targets( FSPlayerInfo * target ){
    static std::vector<FSPlayerInfo> S_targets;
    if(target)
      S_targets.push_back(*target);

    return S_targets;
  }
  
  std::map<int,int> & relations( int * member , int * target ){
    static std::map<int,int> S_relations;
    if(member && target)
      S_relations.insert(std::map<int,int>::value_type(*member,*target));
    
    return S_relations;
  }

  int target_unum( int * unum ){
    static int S_target_unum = rcsc::Unum_Unknown;
    if(unum)
      S_target_unum = *unum;
    
    return S_target_unum;
  }

  bool v1( rcsc::PlayerAgent * agent );

  bool confirmMarkMembers( rcsc::PlayerAgent * agent );

  bool decideMarkTarget( rcsc::PlayerAgent * agent );

  const rcsc::AbstractPlayerObject * getMarkPlayer( rcsc::PlayerAgent * agent );

 public:
  
 Bhv_FSMarkOpponentForward( const rcsc::Vector2D & home_pos )
   : M_home_pos( home_pos ){}
  
  bool execute( rcsc::PlayerAgent * agent );
  
};

#endif
