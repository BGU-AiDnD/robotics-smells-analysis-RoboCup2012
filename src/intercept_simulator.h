// -*-c++-*-

/*!
  \file intercept_simulator.h
  \brief simulate situation when self player kick the ball
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

#ifndef GEAR_INTERCEPT_SIMULATOR_H
#define GEAR_INTERCEPT_SIMULATOR_H

#include <rcsc/player/world_model.h>
#include <rcsc/player/abstract_player_object.h>

/*!
  \class InterceptSimulator
  \brief simulate situation when I kick ball
 */
class InterceptSimulator {
private:
    /*!
      \brief get time count to reach inertia ball point
      \param wm const reference to the WorldMedel instance
      \param ball_pos assumed coordinate of the ball
             after ball_velocity_change_step
      \param ball_vel assumed velocity of the ball
             after ball_velocity_change_step
      \param ball_vel_change_step step count
             to change velocity and position
      \param pl player who chase ball
      \return time count to reach inertia ball point
     */
    long getAccessStepToBallPredictPoint( const rcsc::WorldModel & wm,
                                          const rcsc::Vector2D & ball_pos,
                                          const rcsc::Vector2D & ball_vel,
                                          const long ball_vel_change_step,
                                          const rcsc::AbstractPlayerObject & pl,
                                          const long step ) const;

    /*!
      \brief get time count to reach ball
      \param wm const reference to the WorldMedel instance
      \param ball_pos assumed coordinate of the ball
             after ball_velocity_change_step
      \param ball_vel assumed velocity of the ball
             after ball_velocity_change_step
      \param ball_vel_change_step step count
             to change velocity and position
      \param penalty_step pause steps before chasing a ball
      \param max_step threshold steps to check. if this value is negative, ...
      \return time count to reach ball
     */
    long playerBallAccessStep( const rcsc::WorldModel & wm,
                               const rcsc::Vector2D & ball_pos,
                               const rcsc::Vector2D & ball_vel,
                               const long ball_vel_change_step,
                               const long penalty_step,
                               const rcsc::AbstractPlayerObject & pl,
                               const long max_step ) const;

protected:
    /*!
      \brief get penalty cycles of teammate
      \param pos_count cycle count of teammate from last observation
      \return penalty cycles of teammate
     */
    virtual
    long getTeammatePenaltyTime( long pos_count ) const;

    /*!
      \brief get penalty cycles of opponent
      \param pos_count cycle count of opponent from last observation
      \return penalty cycles of opponent
     */
    virtual
    long getOpponentPenaltyTime( long pos_count ) const;

public:
    /*!
      \brief constructor
     */
    InterceptSimulator();

    /*!
      \brief destructor
     */
    virtual
    ~InterceptSimulator();

    /*!
      \brief simulate situation when I kick ball
      \param wm const reference to the WorldMedel instance
      \param players players for checking
      \param ball_pos coordinate of ball after ball_velocity_change_step
      \param ball_vel velocity of ball after ball_velocity_change_step
      \param ball_velocity_change_step step count
             to change velocity and position
      \param self_penalty_step assumed pause steps of self
      \param teammate_penalty_step assumed pause steps of teammate
      \param opponent_penalty_step assumed pause steps of opponent
      \param self_access_step calculated result of self access step
      \param teammate_access_step calculated steps of fastest teammate
      \param teammate_access_step calculated steps of fastest opponent
      \param teammate_access_step calculated steps of fastest unknown player
      \param first_access_teammate uniform number of first access teammate
      \param first_access_opponent uniform number of first access opponent
      \param first_access_teammate calculated first access teammate
      \param max_step threshold steps to check
      \param acc_level threshold of valid position count
     */
    virtual
    void simulate( const rcsc::WorldModel & wm,
                   const rcsc::AbstractPlayerCont & players,
                   const rcsc::Vector2D & ball_pos,
                   const rcsc::Vector2D & ball_vel,
                   const long ball_vel_change_step,
                   const long self_penalty_step,
                   const long teammate_penalty_step,
                   const long opponent_or_unknown_penalty_step,
                   long * self_access_step,
                   long * teammate_access_step,
                   long * opponent_access_step,
                   long * unknown_access_step,
                   int * first_access_teammate,
                   int * first_access_opponent,
                   long max_step = -1 ) const;
};

#endif
