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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "bhv_find_ball.h"
#include <rcsc/player/player_agent.h>
#include <rcsc/player/player_object.h>
#include <rcsc/action/body_turn_to_point.h>
#include <rcsc/action/neck_turn_to_ball.h>

namespace rcsc_ext {

/*-------------------------------------------------------------------*/
/*!

*/
Bhv_FindBall::Bhv_FindBall()
{
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_FindBall::execute( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();

    if ( wm.ball().seenPosCount() == 0
      && wm.ball().posCount() == 0 )
    {
        return false;
    }

    rcsc::Vector2D point = wm.ball().inertiaPoint( 1 );

    // XXX: if already checked intertia point, should search around!!

    agent->setNeckAction( new rcsc::Neck_TurnToBall() );
    rcsc::Body_TurnToPoint( point ).execute( agent );

    return true;
}

}
