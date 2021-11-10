// -*-c++-*-

/*!
  \file bhv_find_ball.h
  \brief search ball by body and neck
*/

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA

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

#ifndef RCSC_EXT_ACTION_BHV_FIND_BALL_H
#define RCSC_EXT_ACTION_BHV_FIND_BALL_H

#include <rcsc/player/soccer_action.h>

namespace rcsc_ext {

/*!
  \class Bhv_FindBall
  \brief search ball with body and neck
 */
class Bhv_FindBall
    : public rcsc::SoccerBehavior {
public:
    /*!
      \brief constructor
     */
    Bhv_FindBall();

    /*!
      \brief do find ball
      \param agent agent pointer to the agent itself
      \return true with action, false if ball already found
     */
    bool execute( rcsc::PlayerAgent * agent );
};

}

#endif
