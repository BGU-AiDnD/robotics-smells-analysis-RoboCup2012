// -*-c++-*-

/*!
  \file bhv_cross.h
  \brief cross to center action
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

#ifndef GEAR_BHV_CROSS_H
#define GEAR_BHV_CROSS_H

#include <rcsc/player/soccer_action.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_time.h>

#include <vector>
#include <functional>

namespace rcsc {
class PlayerObject;
}

class Bhv_Cross
    : public rcsc::SoccerBehavior {
public:

    struct Target {
        const rcsc::PlayerObject * receiver;
        rcsc::Vector2D target_point;
        double first_speed;
        double score;

        Target( const rcsc::PlayerObject * recv,
                const rcsc::Vector2D & pos,
                const double & speed,
                const double & scr )
            : receiver( recv ),
              target_point( pos ),
              first_speed( speed ),
              score( scr )
          { }
    };

    class TargetCmp
        : public std::binary_function< Target, Target, bool > {
    public:
        result_type operator()( const first_argument_type & lhs,
                                const second_argument_type & rhs ) const
          {
              return lhs.score < rhs.score;
          }
    };

private:

    static std::vector< Target > S_cached_target;

public:

    bool execute( rcsc::PlayerAgent * agent );


    static
    bool get_best_point( const rcsc::PlayerAgent * agent,
                         rcsc::Vector2D * target_point );
private:
    static
    void search( const rcsc::PlayerAgent * agent );

    static
    bool create_close( const rcsc::PlayerAgent * agent,
                       const rcsc::PlayerObject * receiver );
    static
    void create_target( const rcsc::PlayerAgent * agent,
                        const rcsc::PlayerObject * receiver );

};

#endif
