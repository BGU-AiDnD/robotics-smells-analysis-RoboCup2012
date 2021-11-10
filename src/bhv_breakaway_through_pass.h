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

#ifndef GEAR_BHV_BREAKAWAY_THROUGH_PATH_H
#define GEAR_BHV_BREAKAWAY_THROUGH_PATH_H

#include <rcsc/player/soccer_action.h>

#include <vector>

namespace rcsc {
class AngleDeg;
class Vector2D;
class WorldModel;
class AbstractPlayerObject;
}

class Bhv_BreakawayThroughPass
    : public rcsc::SoccerBehavior {
private:
    static const double END_SPEED;

public:

    bool execute( rcsc::PlayerAgent * agent );

private:

    bool search( const rcsc::PlayerAgent * agent,
                 double * first_speed,
                 rcsc::AngleDeg * target_angle,
                 rcsc::Vector2D * target_point );

    double getAngleWidth( const rcsc::WorldModel & wm,
                          const rcsc::AbstractPlayerObject * receiver,
                          const rcsc::Vector2D & dash_point,
                          const rcsc::AngleDeg & angle_left,
                          const rcsc::AngleDeg & angle_right,
                          rcsc::AngleDeg * target_angle,
                          double * first_speed,
                          rcsc::Vector2D * target_point );

    bool calcPenLineTarget( const rcsc::WorldModel & wm,
                            const rcsc::AbstractPlayerObject * receiver,
                            const rcsc::Vector2D & dash_point,
                            const std::vector< rcsc::AngleDeg > & opponent_angles,
                            rcsc::AngleDeg * target_angle,
                            double * first_speed,
                            rcsc::Vector2D * target_point );

};

#endif
