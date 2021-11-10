// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hossein Mobasher.

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
#ifndef BHV_SHOOT2009_H
#define BHV_SHOOT2009_H

#include <rcsc/geom/vector_2d.h>
#include <rcsc/player/soccer_action.h>

using namespace rcsc;

class Bhv_Shoot2009
      : SoccerBehavior {
private:
    Vector2D M_shoot_point;
    bool     M_shoot_check;

public:
    /*!
	constructor for this class.
	do not any work.
    */
    Bhv_Shoot2009()
    : M_shoot_point( Vector2D::INVALIDATED )
    , M_shoot_check( false )
    {

    }

    /*!
	executable shoot.
    */
    bool execute( PlayerAgent * agent );

private:

    /*!
	return best shoot point.
	if return true means that prob of goal is above 95 %.
    */
    bool get_best_shoot( PlayerAgent * agent , Vector2D& point_ );

};

#endif
