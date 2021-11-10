// -*-c++-*-

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

#ifndef PASS_COURSE_TABLE
#define PASS_COURSE_TABLE

#include <rcsc/player/player_agent.h>
#include <rcsc/player/soccer_action.h>

#include <boost/shared_ptr.hpp>
#include <vector>

class PassCourseTable {
public:
    struct CourseSelection {
    public:
        enum ActionType { Hold, FindPassReceiver, Pass, Push, Shoot };

    public:
        ActionType type;
        int player_number;
        int recursive_level;
    };


protected:
    static const int RECURSIVE_LEVEL = 4;

    typedef int State;
    static const int MAX_PLAYER = 11; // XXX: 11 -> ServerParam::i().maxPlayer()

    static const int PUSH_STATE = MAX_PLAYER + 1;
    static const int GOAL_STATE = MAX_PLAYER + 2;

    static const double GOALIE_PASS_EVAL_THRESHOLD;
    static const double BACK_PASS_EVAL_THRESHOLD;
    static const double PASS_EVAL_THRESHOLD;
    static const double CHANCE_PASS_EVAL_THRESHOLD;
    static const double PASS_RECEIVER_PREDICT_STEP;

    struct ConnectionTable {
    public:
        std::vector< double > player;
        bool goal;
        bool push;

        double free_radius;

    public:
        double evaluation;

    public:
        ConnectionTable();
    };

private:
    typedef const rcsc::AbstractPlayerObject * Player_Reference;

private:
    const bool M_inhibit_self_push;

    mutable bool M_table_prepared;
    mutable std::vector< ConnectionTable > M_table;

    mutable bool M_search_result_prepared;
    mutable std::vector< CourseSelection > M_result;
    mutable int M_first_best_player;

private:
    void prepare_table( const rcsc::WorldModel & world ) const;

    void prepare_result( const rcsc::WorldModel & world,
                         int recursive_level,
                         int from ) const;

    double recursive_search( const rcsc::WorldModel & world,
                             int recursive_level,
                             int from,
                             std::vector< CourseSelection > * result ) const;

protected:
    virtual double pass_check( const rcsc::WorldModel & wm,
                               const Player_Reference & from,
                               const Player_Reference & to ) const;

    virtual bool shoot_check( const rcsc::WorldModel & wm,
                              const Player_Reference & from ) const;

    virtual bool push_check( const rcsc::WorldModel & world,
                             const Player_Reference & from ) const;

    virtual double free_radius_check( const rcsc::WorldModel & world,
                                      const Player_Reference & pl ) const;

#if 0
    virtual double free_forward_width_check( const rcsc::WorldModel & world,
                                             const D2_Vector & point ) const;
#endif

    virtual double evaluateRoute( const std::vector< CourseSelection > & route,
                                  const rcsc::WorldModel & world ) const;

    virtual double static_evaluation( const rcsc::WorldModel & world,
                                      int player_number ) const;

public:
    PassCourseTable( bool inhibit_self_push = false );
    virtual ~PassCourseTable();

    virtual const std::vector< ConnectionTable > & get_table( const rcsc::WorldModel & world ) const;

    virtual CourseSelection get_first_selection( const rcsc::WorldModel & world ) const;

    virtual CourseSelection get_result( const rcsc::WorldModel & world ) const;

    virtual std::vector< CourseSelection > get_all_course_result( const rcsc::WorldModel & world ) const;

    int getNumberOfPassCources( const rcsc::WorldModel & world ) const;

    virtual void setDebugPassRoute( rcsc::PlayerAgent * agent ) const;

private:
    static rcsc::GameTime S_cache_time;
    static boost::shared_ptr< PassCourseTable > S_cache;

public:
    static const PassCourseTable & instance( const rcsc::WorldModel & wm,
                                             bool inhibit_self_push = false );

    static double get_recursive_pass_ball_speed( const double & distance );


public:
    static bool canShootFrom( const rcsc::WorldModel & wm,
                              const rcsc::Vector2D & pos,
                              long valid_opponent_threshold );

    static double getFreeAngleFromPos( const rcsc::WorldModel & wm,
                                       long valid_opponent_threshold,
                                       const rcsc::Vector2D & pos );

    static double getMinimumFreeAngle( const rcsc::WorldModel & wm,
                                       const rcsc::Vector2D & goal,
                                       long valid_opponent_threshold,
                                       const rcsc::Vector2D & pos );
};


#endif
