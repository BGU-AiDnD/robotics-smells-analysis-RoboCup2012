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

#ifndef TOKYOTECH_BODY_KICK_TO_CORNER_H
#define TOKYOTECH_BODY_KICK_TO_CORNER_H

#include <rcsc/player/soccer_action.h>


class Body_KickToCorner
    : public rcsc::BodyAction {
private:
    const bool M_to_left;

public:
    // if left == ( world.self().pos().y < 0.0 )
    //   kick to the near side corner
    explicit
    Body_KickToCorner( const bool left )
        : M_to_left( left )
      { }

    bool execute( rcsc::PlayerAgent * agent );

};

#endif
