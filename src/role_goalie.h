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

#ifndef AGENT2D_ROLE_GOALIE_H
#define AGENT2D_ROLE_GOALIE_H

#include <rcsc/game_time.h>

#include "soccer_role.h"

class RoleGoalie
    : public SoccerRole {
private:
    rcsc::GameTime M_last_goalie_kick_time;

public:
    RoleGoalie()
        : M_last_goalie_kick_time( 0, 0 )
      { }

    ~RoleGoalie()
      {
          //std::cerr << "delete RoleGoalie" << std::endl;
      }

    virtual
    void execute( rcsc::PlayerAgent * agent );

    static
    std::string name()
      {
          return std::string( "Goalie" );
      }

    static
    SoccerRole * create()
      {
          return new RoleGoalie();
      }

private:
    void doKick( rcsc::PlayerAgent * agent );
    void doMove( rcsc::PlayerAgent * agent );
};


#endif
