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

#ifndef GEAR_ROLE_HCSIDE_BACK_H
#define GEAR_ROLE_HCSIDE_BACK_H

#include "soccer_role.h"

namespace rcsc {
class Vector2D;
class PlayerAgent;
class Formation;
}

class RoleHCSideBack
    : public SoccerRole {
private:

public:

    RoleHCSideBack()
      { }

    ~RoleHCSideBack()
      {
          //std::cerr << "delete RoleHCSideBack" << std::endl;
      }

    virtual
    void execute( rcsc::PlayerAgent * agent );

    static
    std::string name()
      {
          return std::string( "HCSideBack" );
      }

    static
    SoccerRole * create()
      {
          return ( new RoleHCSideBack() );
      }


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

    double dash_power;
};


#endif
