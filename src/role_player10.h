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

#ifndef GEAR_ROLE_PLAYER10_H
#define GEAR_ROLE_PLAYER10_H

#include "soccer_role.h"

namespace rcsc {
class Vector2D;
class PlayerAgent;
class Formation;
}

class RolePlayer10
    : public SoccerRole {
private:

    static int S_count_shot_area_hold;
	
public:

    RolePlayer10()
      { }

    ~RolePlayer10()
      {
          //std::cerr << "delete RolePlayer10" << std::endl;
      }

    virtual
    void execute( rcsc::PlayerAgent * agent );

    static
    std::string name()
      {
          return std::string( "Player10" );
      }

    static
    SoccerRole * create()
      {
          return ( new RolePlayer10() );
      }

    //swarm
      
    void swarmUpdatePosition(rcsc::PlayerAgent * agent);  
    
    void swarmAttack(rcsc::PlayerAgent * agent);
    
    void swarmDefense(rcsc::PlayerAgent * agent);
    
private:
  void doKick( rcsc::PlayerAgent * agent );
  
  void doMove( rcsc::PlayerAgent * agent );

  int countKickableCycleAtSamePoint(rcsc::PlayerAgent * agent);

  bool moveCentering( rcsc::PlayerAgent * agent );
  
  bool doSelfPass( rcsc::PlayerAgent * agent,
                     const int max_dash_step );

    void doMiddleAreaKick( rcsc::PlayerAgent * agent );
    void doCrossAreaKick( rcsc::PlayerAgent * agent );
    void doShootAreaKick( rcsc::PlayerAgent * agent );

    void doGoToCrossPoint( rcsc::PlayerAgent * agent,
                           const rcsc::Vector2D & home_pos );

    bool doCheckCrossPoint( rcsc::PlayerAgent * agent );
    
    void doOffensiveMiddleKick( rcsc::PlayerAgent * agent );
     
    void doGoToPoint( rcsc::PlayerAgent * agent,
                      const rcsc::Vector2D & target_point,
                      const double & dist_thr,
                      const double & dash_power,
                      const double & dir_thr ); 

public:
    
    static rcsc::Vector2D p10AP;        //stores last position attack swarm
     
     static rcsc::Vector2D p10DP;        //stores last position defense swarm
     
     static rcsc::Vector2D p10VAP;       //stores last speed attack swarm
     
     static rcsc::Vector2D p10VDP;       //stores last speed defense swarm
     
     static rcsc::Vector2D p10PB;        //stores last personal best particle 10

     static rcsc::Vector2D p10PB9;       //stores last personal best particle 9
     
     static rcsc::Vector2D p10PB11;       //stores last personal best particle 11
     
     static rcsc::Vector2D p10LB;        //stores last local best from the neighborhood
};


#endif
