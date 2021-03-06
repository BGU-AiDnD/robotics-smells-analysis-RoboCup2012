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

#ifndef TOKYOTECH_BHV_MARK_PASS_LINE_H
#define TOKYOTECH_BHV_MARK_PASS_LINE_H

#include <rcsc/geom/vector_2d.h>
#include <rcsc/player/soccer_action.h>

namespace rcsc {
class AbstractPlayerObject;
class WorldModel;
}

// added 2008-07-04

class Bhv_MarkPassLine
    : public rcsc::SoccerBehavior {
public:
    Bhv_MarkPassLine()
      { }

    bool execute( rcsc::PlayerAgent * agent );

private:

    const
    rcsc::AbstractPlayerObject * getMarkTarget( const rcsc::WorldModel & wm );

    rcsc::Vector2D  getMarkPosition( const rcsc::WorldModel & wm,
                                     const rcsc::AbstractPlayerObject * target_player );

};

#define USE_BHV_MARK_PASS_LINE

#endif
