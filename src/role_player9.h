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

#ifndef GEAR_ROLE_PLAYER9_H
#define GEAR_ROLE_PLAYER9_H

#include "soccer_role.h"

namespace rcsc {
class AngleDeg;
class Vector2D;
class PlayerAgent;
class Formation;
}

class RolePlayer9
    : public SoccerRole {
private:

    static int S_count_shot_area_hold;

public:

    RolePlayer9()
      { }

    ~RolePlayer9()
      {
          //std::cerr << "delete RolePlayer9" << std::endl;
      }

    virtual
    void execute( rcsc::PlayerAgent * agent );

    static
    std::string name()
      {
          return std::string( "Player9" );
      }

    static
    SoccerRole * create()
      {
          return ( new RolePlayer9() );
      }

    //swarm
      
    void swarmUpdatePosition(rcsc::PlayerAgent * agent);  
    
    void swarmAttack(rcsc::PlayerAgent * agent);
    
    void swarmDefense(rcsc::PlayerAgent * agent);
    
private:
    void doKick( rcsc::PlayerAgent * agent );
    void doMove( rcsc::PlayerAgent * agent );

    void doDribbleAttackKick( rcsc::PlayerAgent * agent );
    void doShootChanceKick(  rcsc::PlayerAgent * agent );

    void doOffensiveMove( rcsc::PlayerAgent * agent,
                          const rcsc::Vector2D & home_pos);

    void doDefensiveMove( rcsc::PlayerAgent * agent,
                          const rcsc::Vector2D & home_pos);

    void doShootAreaMove( rcsc::PlayerAgent * agent,
                          const rcsc::Vector2D & home_pos );
    bool doShootAreaIntercept( rcsc::PlayerAgent * agent,
                               const rcsc::Vector2D & home_pos );
                               
    int countKickableCycleAtSamePoint(rcsc::PlayerAgent * agent);  
    
    void doShootAreaKick( rcsc::PlayerAgent * agent );
    
    bool doCheckCrossPoint( rcsc::PlayerAgent * agent );
    
    void doOffensiveMiddleKick( rcsc::PlayerAgent * agent );
    
    void doCrossAreaKick( rcsc::PlayerAgent * agent );
    
    void doDribbleAttackAreaKick( rcsc::PlayerAgent * agent );
    
    bool doStraightDribble( rcsc::PlayerAgent * agent );

    void doSideForwardDribble( rcsc::PlayerAgent * agent );
    
    rcsc::Vector2D getDribbleTarget( rcsc::PlayerAgent * agent );
    
    int getDribbleAttackDashStep( const rcsc::PlayerAgent * agent,
                                  const rcsc::AngleDeg & target_angle );
                                  
    bool kickCentering( rcsc::PlayerAgent * agent );       
    
    void doMiddleAreaKick( rcsc::PlayerAgent * agent ); 
    
    void doGoToPoint( rcsc::PlayerAgent * agent,
                      const rcsc::Vector2D & target_point,
                      const double & dist_thr,
                      const double & dash_power,
                      const double & dir_thr );                          

public:

    static rcsc::Vector2D p9AP;        //stores last position attack swarm
     
     static rcsc::Vector2D p9DP;        //stores last position defense swarm
     
     static rcsc::Vector2D p9VAP;       //stores last speed attack swarm
     
     static rcsc::Vector2D p9VDP;       //stores last speed defense swarm
     
     static rcsc::Vector2D p9PB;        //stores last personal best particle 9

     static rcsc::Vector2D p9PB5;       //stores last personal best particle 5
     
     static rcsc::Vector2D p9PB6;       //stores last personal best particle 6
     
     static rcsc::Vector2D p9PB7;       //stores last personal best particle 7
     
     static rcsc::Vector2D p9PB8;       //stores last personal best particle 8

     static rcsc::Vector2D p9PB10;       //stores last personal best particle 10
     
     static rcsc::Vector2D p9LB;        //stores last local best from the neighborhood 
                                    
};

#endif
