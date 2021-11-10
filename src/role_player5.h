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

#ifndef GEAR_ROLE_PLAYER5_H
#define GEAR_ROLE_PLAYER5_H

#include "soccer_role.h"

namespace rcsc {
class Vector2D;
class PlayerAgent;
class Formation;
}

class RolePlayer5
    : public SoccerRole {
private:

public:

    RolePlayer5()
      { }

    ~RolePlayer5()
      {
          //std::cerr << "delete RolePlayer5" << std::endl;
      }

    virtual
    void execute( rcsc::PlayerAgent * agent );

    static
    std::string name()
      {
          return std::string( "Player5" );
      }

    static
    SoccerRole * create()
      {
          return ( new RolePlayer5() );
      }

    //swarm
      
    void swarmUpdatePosition(rcsc::PlayerAgent * agent);  
    
    void swarmAttack(rcsc::PlayerAgent * agent);
    
    void swarmDefense(rcsc::PlayerAgent * agent);
    
private:
    void doKick( rcsc::PlayerAgent * agent );
    void doMove( rcsc::PlayerAgent * agent );

    void doBasicMove( rcsc::PlayerAgent * agent,
                      const rcsc::Vector2D & home_pos );
    rcsc::Vector2D getBasicMoveTarget( rcsc::PlayerAgent * agent,
                                       const rcsc::Vector2D & home_pos );

    bool doBlockPassLine( rcsc::PlayerAgent * agent,
                          const rcsc::Vector2D & home_pos );

    void doCrossBlockAreaMove( rcsc::PlayerAgent * agent,
                               const rcsc::Vector2D & home_pos );
    void doDangerAreaMove( rcsc::PlayerAgent * agent,
                           const rcsc::Vector2D & home_pos );
    void doDribbleBlockAreaMove( rcsc::PlayerAgent * agent,
                                 const rcsc::Vector2D & home_pos );
    void doDefMidAreaMove( rcsc::PlayerAgent * agent,
                           const rcsc::Vector2D & home_pos );

    void doBlockSideAttacker( rcsc::PlayerAgent * agent,
                              const rcsc::Vector2D & home_pos );
                              
    void doGoToPoint( rcsc::PlayerAgent * agent,
                      const rcsc::Vector2D & target_point,
                      const double & dist_thr,
                      const double & dash_power,
                      const double & dir_thr );

public:
    
    static rcsc::Vector2D p5AP;        //stores last position attack swarm
     
     static rcsc::Vector2D p5DP;        //stores last position defense swarm
     
     static rcsc::Vector2D p5VAP;       //stores last speed attack swarm
     
     static rcsc::Vector2D p5VDP;       //stores last speed defense swarm
     
     static rcsc::Vector2D p5PB;        //stores last personal best particle 5
     
     static rcsc::Vector2D p5PB2;       //stores last personal best particle 2
     
     static rcsc::Vector2D p5PB3;       //stores last personal best particle 3
     
     static rcsc::Vector2D p5PB4;       //stores last personal best particle 4

     static rcsc::Vector2D p5PB6;       //stores last personal best particle 6
     
     static rcsc::Vector2D p5LB;        //stores last local best from the neighborhood 1
};


#endif
