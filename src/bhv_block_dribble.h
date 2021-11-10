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

#ifndef GEAR_BHV_BLOCK_DRIBBLE_H
#define GEAR_BHV_BLOCK_DRIBBLE_H

#include <rcsc/player/soccer_action.h>

#include <rcsc/player/player_object.h>
#include <rcsc/geom/vector_2d.h>

class Bhv_BlockDribble
    : public rcsc::SoccerBehavior {
private:

public:
    Bhv_BlockDribble()
      { }

    bool execute( rcsc::PlayerAgent * agent );

private:

    rcsc::Vector2D getTargetPoint( const rcsc::PlayerAgent * agent );

};

#endif
