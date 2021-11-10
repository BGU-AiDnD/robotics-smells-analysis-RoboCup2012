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

#ifndef GEAR_ROLE_DEFENSIVE_HALF_H
#define GEAR_ROLE_DEFENSIVE_HALF_H

#include "soccer_role.h"

namespace rcsc {
class Vector2D;
class PlayerAgent;
class Formation;
}

class RoleDefensiveHalf
    : public SoccerRole {
private:

public:

    RoleDefensiveHalf()
      { }

    ~RoleDefensiveHalf()
      {
          //std::cerr << "delete RoleDefensiveHalf" << std::endl;
      }

    virtual
    void execute( rcsc::PlayerAgent * agent );


    static
    std::string name()
      {
          return std::string( "DefensiveHalf" );
      }

    static
    SoccerRole * create()
      {
          return ( new RoleDefensiveHalf() );
      }

private:
    void doKick( rcsc::PlayerAgent * agent );
    void doMove( rcsc::PlayerAgent * agent );

    void doCrossBlockMove( rcsc::PlayerAgent * agent,
                           const rcsc::Vector2D & home_pos );
    void doOffensiveMove( rcsc::PlayerAgent * agent,
                          const rcsc::Vector2D & home_pos );
    void doDefensiveMove( rcsc::PlayerAgent * agent,
                          const rcsc::Vector2D & home_pos );
    void doDribbleBlockMove( rcsc::PlayerAgent * agent,
                             const rcsc::Vector2D & home_pos);

    void doCrossAreaMove( rcsc::PlayerAgent * agent,
                          const rcsc::Vector2D & home_pos);
};


#endif
