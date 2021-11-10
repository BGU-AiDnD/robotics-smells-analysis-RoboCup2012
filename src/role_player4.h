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

#ifndef GEAR_ROLE_PLAYER4_H
#define GEAR_ROLE_PLAYER4_H

#include "soccer_role.h"

namespace rcsc {
class Vector2D;
class PlayerAgent;
class Formation;
}

class RolePlayer4
    : public SoccerRole {
private:

public:

    RolePlayer4()
      { }

    ~RolePlayer4()
      {
          //std::cerr << "delete RolePlayer4" << std::endl;
      }

    virtual
    void execute( rcsc::PlayerAgent * agent );

    static
    std::string name()
      {
          return std::string( "Player4" );
      }

    static
    SoccerRole * create()
      {
          return ( new RolePlayer4() );
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
    static rcsc::Vector2D p4AP;        //stores last position attack swarm
     
     static rcsc::Vector2D p4DP;        //stores last position defense swarm
     
     static rcsc::Vector2D p4VAP;       //stores last speed attack swarm
     
     static rcsc::Vector2D p4VDP;       //stores last speed defense swarm
     
     static rcsc::Vector2D p4PB;        //stores last personal best particle 4
     
     static rcsc::Vector2D p4PB2;       //stores last personal best particle 2
     
     static rcsc::Vector2D p4PB3;       //stores last personal best particle 3
     
     static rcsc::Vector2D p4PB5;       //stores last personal best particle 5

     static rcsc::Vector2D p4PB6;       //stores last personal best particle 6
     
     static rcsc::Vector2D p4LB;        //stores last local best from the neighborhood                                          
};


#endif
