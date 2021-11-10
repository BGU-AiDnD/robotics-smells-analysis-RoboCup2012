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

#ifndef GEAR_ROLE_HCATTACKER_H
#define GEAR_ROLE_HCATTACKER_H

#include "soccer_role.h"

namespace rcsc {
class Vector2D;
class PlayerAgent;
class Formation;
}

class RoleHCAttacker
    : public SoccerRole {
private:

    static int S_count_shot_area_hold;
	
public:

    RoleHCAttacker()
      { }

    ~RoleHCAttacker()
      {
          //std::cerr << "delete RoleHCAttacker" << std::endl;
      }

    virtual
    void execute( rcsc::PlayerAgent * agent );

    static
    std::string name()
      {
          return std::string( "HCAttacker" );
      }

    static
    SoccerRole * create()
      {
          return ( new RoleHCAttacker() );
      }
     
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

    double dash_power;

    double back_dash_power;
};


#endif
