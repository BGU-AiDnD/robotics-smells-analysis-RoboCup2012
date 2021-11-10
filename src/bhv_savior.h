// -*-c++-*-

/*!
  \file bhv_savior.h
  \brief aggressive goalie behavior
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

#ifndef BHV_SAVIOR_H
#define BHV_SAVIOR_H

#include <rcsc/player/soccer_action.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/math_util.h>

namespace rcsc {
    class WorldModel;
}


class Bhv_Savior
    : public rcsc::SoccerBehavior {

private:
    static
    double translateX( bool reverse,
                       double x );

    static
    rcsc::Vector2D translateXVec( bool reverse,
                                  const rcsc::Vector2D & vec );

    static
    rcsc::AngleDeg translateTheta( bool reverse,
                                   const rcsc::AngleDeg & theta );

    static
    bool translateXIsLessEqualsTo( bool reverse,
                                   double x, double threshold );

private:
    static
    rcsc::Vector2D getBallFieldLineCrossPoint( const rcsc::Vector2D & ball_from,
                                               const rcsc::Vector2D & ball_to,
                                               const rcsc::Vector2D & p1,
                                               const rcsc::Vector2D & p2,
                                               double field_back_offset );

public:
    static
    rcsc::Vector2D getBoundPredictBallPos( const rcsc::WorldModel & wm,
                                           int predict_step,
                                           double field_back_offset = rcsc::EPS );

    static
    rcsc::AngleDeg getDirFromGoal( const rcsc::Vector2D & pos );

    static
    double getTackleProbability( const rcsc::Vector2D & body_relative_ball );

    static
    rcsc::Vector2D getSelfNextPosWithDash( const rcsc::WorldModel & wm,
                                           double dash_power );

    static
    double getSelfNextTackleProbabilityWithDash( const rcsc::WorldModel & wm,
                                                 double dash_power );


    static
    bool canShootFrom( const rcsc::WorldModel & wm,
                       const rcsc::Vector2D & pos,
                       long valid_teammate_threshold = 20 );

    static
    double getFreeAngleFromPos( const rcsc::WorldModel & wm,
                                long valid_teammate_threshold,
                                const rcsc::Vector2D & pos );

    static
    double getMinimumFreeAngle( const rcsc::WorldModel & wm,
                                const rcsc::Vector2D & goal,
                                long valid_teammate_threshold,
                                const rcsc::Vector2D & pos );

private:
    rcsc::Vector2D getBasicPosition( rcsc::PlayerAgent * agent,
                                     bool penalty_kick_mode,
                                     bool despair_mode,
                                     bool reverse_x,
                                     bool * in_region,
                                     bool * dangerous,
                                     bool * aggressive_positioning,
                                     bool * emergent_advance,
                                     bool * goal_line_positioning ) const;

    rcsc::Vector2D getBasicPositionFromBall
                     ( rcsc::PlayerAgent * agent,
                       const rcsc::Vector2D & ball_pos,
                       double base_dist,
                       const rcsc::Vector2D & self_pos,
                       bool no_danger_angle,
                       bool despair_mode,
                       bool * in_region,
                       bool * dangerous,
                       bool emergent_advance,
                       bool * goal_line_positioning ) const;

    rcsc::Vector2D getGoalLinePositioningTarget( rcsc::PlayerAgent * agent,
                                                 const rcsc::WorldModel & wm,
                                                 const double goal_x_dist,
                                                 const rcsc::Vector2D & ball_pos,
                                                 bool despair_mode ) const;


    static
    bool isGoalLinePositioningSituation( const rcsc::WorldModel & wm,
                                         const rcsc::Vector2D & ball_pos,
                                         bool penalty_kick_mode );

    bool doPlayOnMove( rcsc::PlayerAgent * agent,
                       bool penalty_kick_mode = false,
                       bool reverse_x = false );

    bool doKick( rcsc::PlayerAgent * agent );

    bool doChaseBall( rcsc::PlayerAgent * agent );

    bool doEmergentAdvance( rcsc::PlayerAgent * agent );

public:
    bool execute( rcsc::PlayerAgent * agent );
};

#endif
