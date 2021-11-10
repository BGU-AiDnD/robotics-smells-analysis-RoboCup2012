// -*-c++-*-

/*!
  \file bhv_pass_kick_find_receiver.h
  \brief search the pass receiver player and perform pass kick if possible
*/

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

#ifndef GEAR_BHV_PASS_KICK_FIND_RECEIVER_H
#define GEAR_BHV_PASS_KICK_FIND_RECEIVER_H

#include <rcsc/player/soccer_action.h>

/*!
  \class Bhv_PassKickFindReceiver
  \brief search specified player and perform pass kick if possible
*/
class Bhv_PassKickFindReceiver
    : public rcsc::SoccerBehavior {
private:
    static const int COUNT_THRESHOLD;
public:
    /*!
      \brief constructor
     */
    Bhv_PassKickFindReceiver()
      { }

    /*!
      \brief do find player
      \param agent agent pointer to the agent itself
      \return true with action, false if target player already found
     */
    bool execute( rcsc::PlayerAgent * agent );

private:

    bool doFirstKick( rcsc::PlayerAgent * agent );
    void doFindReceiver( rcsc::PlayerAgent * agent );


};

#endif
