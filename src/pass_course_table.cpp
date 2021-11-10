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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "pass_course_table.h"
#include "intercept_simulator.h"

#include "kick_table.h"

#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include "world_model_ext.h"

#include <boost/shared_ptr.hpp>

#include <cmath>
#include <cfloat>
#include <algorithm>
#include <sstream>

const double PassCourseTable::GOALIE_PASS_EVAL_THRESHOLD = 17.5;
const double PassCourseTable::BACK_PASS_EVAL_THRESHOLD = 17.5;
const double PassCourseTable::PASS_EVAL_THRESHOLD = 14.5;
const double PassCourseTable::CHANCE_PASS_EVAL_THRESHOLD = 14.5;
const double PassCourseTable::PASS_RECEIVER_PREDICT_STEP = 2.5;

#define DEBUG_PRINT 1

using rcsc::dlog;

//
// ConnectionTable
//
PassCourseTable::ConnectionTable::ConnectionTable()
    : player( MAX_PLAYER + 1, false )
    , goal( false )
    , push( false )
    , free_radius( 0.0 )
    , evaluation( 0.0 )
{

}


//
// PassCourseTable
//
PassCourseTable::PassCourseTable( bool inhibit_self_push )
    : M_inhibit_self_push( inhibit_self_push )
    , M_table_prepared( false )
    , M_table( MAX_PLAYER + 1 )
    , M_search_result_prepared( false )
    , M_result()
    , M_first_best_player( -1 )
{

}

PassCourseTable::~PassCourseTable()
{

}

void
PassCourseTable::prepare_table( const rcsc::WorldModel & world ) const
{
    if ( M_table_prepared )
    {
        return;
    }

    const rcsc::WorldModel & wm = world;

    for ( int from = 1; from <= MAX_PLAYER; ++from )
    {
        Player_Reference fp = wm.ourPlayer( from );

        M_table[from].goal = this->shoot_check( wm, fp );
        M_table[from].push = this->push_check( wm, fp );

        for ( int to = 1; to <= MAX_PLAYER; ++to )
        {
            M_table[from].player[to] = this->pass_check( wm, fp, wm.ourPlayer( to ) );
        }

        M_table[from].free_radius = this->free_radius_check( wm, fp );
    }

#if DEBUG_PRINT
    std::stringstream buf;
    buf << "\t";
    for ( int to = 1; to <= MAX_PLAYER; ++to )
    {
        buf << to;

        if ( to != MAX_PLAYER )
        {
            buf << ' ';
        }
        else
        {
            buf << std::endl;
            dlog.flush();
            buf.str( "" );
        }
    }

    for ( int from = 1; from <= MAX_PLAYER; ++from )
    {
        buf << from << "\t";

        for ( int to = 1; to <= MAX_PLAYER; ++to )
        {
            if ( M_table[from].player[to] >= 1.0 )
            {
                buf << '*';
            }
            else
            {
                buf << '.';
            }

            if ( to != MAX_PLAYER )
            {
                buf << ' ';
            }
        }

        if ( M_table[from].goal )
        {
            buf << " goal";
        }

        if ( M_table[from].push )
        {
            buf << " push";
        }

        buf << std::endl;
        dlog.flush();
        buf.str( "" );
    }

    dlog.flush();
#endif // end DEBUG_PRINT

    for ( int from = 1; from <= MAX_PLAYER; ++from )
    {
        M_table[from].evaluation = this->static_evaluation( wm, from );

#if DEBUG_PRINT
        buf << "table[" << from << "].evaluation = "
            << M_table[from].evaluation << std::endl;
        dlog.flush();
        buf.str( "" );
#endif
    }

    M_table_prepared = true;
}


const std::vector< PassCourseTable::ConnectionTable > &
PassCourseTable::get_table( const rcsc::WorldModel & world ) const
{
    this->prepare_table( world );

    return M_table;
}

PassCourseTable::CourseSelection
PassCourseTable::get_first_selection( const rcsc::WorldModel & world ) const
{
    const rcsc::WorldModel & wm = world;

    this->prepare_result( world, RECURSIVE_LEVEL, wm.self().unum() );

    CourseSelection c;
    c.player_number = wm.self().unum();

    if ( M_table[wm.self().unum()].goal == true )
    {
        c.type = CourseSelection::Shoot;

        return c;
    }

    c.player_number = M_first_best_player;

    if ( M_first_best_player == wm.self().unum() )
    {
        if ( M_table[wm.self().unum()].push == true )
        {
            c.type = CourseSelection::Push;
        }
        else
        {
            c.type = CourseSelection::Hold;
        }

        return c;
    }
    else
    {
        const rcsc::AbstractPlayerObject * target = wm.ourPlayer( M_first_best_player );

        if ( target
             && ( target->seenPosCount() <= 1
                  //|| target->posCount() <= 1
                  )
             )
        {
            c.type = CourseSelection::Pass;
            rcsc::dlog.addText( rcsc::Logger::PASS,
                                __FILE__": (get_first_selection) found pass target %d (%.1f %.1f) count=%d",
                                target->unum(),
                                target->pos().x, target->pos().y,
                                target->seenPosCount() );
#if 1
            rcsc::Vector2D target_point = target->pos() + target->vel();
            double target_ball_speed
                = get_recursive_pass_ball_speed( wm.ball().pos().dist( target_point ) );
            rcsc::Vector2D one_step_vel
                = rcsc::KickTable::calc_max_velocity( ( target_point - wm.ball().pos() ).th(),
                                                      wm.self().kickRate(),
                                                      wm.ball().vel() );
            if ( one_step_vel.r() <= target_ball_speed * 0.96 )
            {
                const rcsc::Vector2D next_self_pos = wm.self().pos() + wm.self().vel();
                const rcsc::AngleDeg face_angle = ( target_point - next_self_pos ).th();

                const double view_half_width = rcsc::ViewWidth::width( rcsc::ViewWidth::NARROW ) * 0.5;
                const double neck_min = rcsc::ServerParam::i().minNeckAngle() - ( view_half_width - 10.0 );
                const double neck_max = rcsc::ServerParam::i().maxNeckAngle() + ( view_half_width - 10.0 );

                rcsc::dlog.addText( rcsc::Logger::PASS,
                                        __FILE__": (get_first_selection) cannot kick by 1 step." );

                double angle_diff = ( face_angle - wm.self().body() ).abs();
                if ( angle_diff < neck_min
                     || neck_max < angle_diff )
                {
                    rcsc::dlog.addText( rcsc::Logger::PASS,
                                        __FILE__": (get_first_selection) cannot look only by turn_neck. dir=%.1f diff=%.1f.",
                                        face_angle.degree(),
                                        angle_diff );
                    c.type = CourseSelection::FindPassReceiver;
                }
            }
#endif
        }
        else
        {
            c.type = CourseSelection::FindPassReceiver;
        }

        return c;
    }
}

PassCourseTable::CourseSelection
PassCourseTable::get_result( const rcsc::WorldModel & world ) const
{
    const rcsc::WorldModel & wm = world;

    this->prepare_result( world, RECURSIVE_LEVEL, wm.self().unum() );

#if DEBUG_PRINT
    for ( size_t i = 0; i < M_result.size(); ++i )
    {
        std::stringstream buf;

        buf << "result[" << i << "] = ";

        switch( M_result[i].type )
        {
        case CourseSelection::Hold:
            buf << "Hold";
            break;

        case CourseSelection::FindPassReceiver:
            buf << "FindPassReceiver " << M_result[i].player_number;
            break;

        case CourseSelection::Pass:
            buf << "Pass " << M_result[i].player_number;
            break;

        case CourseSelection::Push:
            buf << "Push";
            break;

        case CourseSelection::Shoot:
            buf << "Shoot";
            break;

        default:
            buf << "???";
            break;
        }

        buf << std::endl;

        dlog.flush();
    }
#endif

    return M_result[0];
}

std::vector<PassCourseTable::CourseSelection>
PassCourseTable::get_all_course_result( const rcsc::WorldModel & world ) const
{
    this->prepare_result( world,
                          RECURSIVE_LEVEL,
                          world.self().unum() );

    return M_result;
}

int
PassCourseTable::getNumberOfPassCources( const rcsc::WorldModel & world ) const
{
    this->prepare_result( world,
                          RECURSIVE_LEVEL,
                          world.self().unum() );

    int self_unum = world.self().unum();

    int n = 0;
    for ( int i = 1; i <= MAX_PLAYER; ++i )
    {
        if ( M_table[self_unum].player[i] >= PASS_EVAL_THRESHOLD )
        {
            ++n;
        }
    }

    return n;
}


void
PassCourseTable::prepare_result( const rcsc::WorldModel & world,
                                 int recursive_level,
                                 int from ) const
{
    if ( M_search_result_prepared )
    {
        return;
    }

    this->prepare_table( world );

    M_search_result_prepared = true;


    M_result.clear();
    M_result.push_back( CourseSelection() );

    M_result[0].recursive_level = 0;
    M_result[0].player_number = from;
    M_first_best_player = from;

    if ( M_table[from].goal )
    {
        M_result[0].type = CourseSelection::Shoot;
    }
    else if ( M_table[from].push )
    {
        M_result[0].type = CourseSelection::Push;
    }
    else
    {
        M_result[0].type = CourseSelection::Hold;
    }

    if ( recursive_level == 0 )
    {
        return;
    }


    int max_index = from;
    double max_eval = M_table[from].evaluation;
    std::vector< CourseSelection > max_eval_course_list;

    for ( int i = 1; i <= MAX_PLAYER; ++ i )
    {
#if DEBUG_PRINT
        {
            std::stringstream buf;
            buf << "table[" << from << "][" << i << "]" << std::endl;
            dlog.flush();
        }
#endif

        if ( i == from
             || M_table[from].player[i] < 1.0 )
        {
            continue;
        }

#if DEBUG_PRINT
        {
            std::stringstream buf;
            buf << "top level: searching pass to " << i << std::endl;
            dlog.flush();
        }
#endif


        CourseSelection pass_test;
        pass_test.type = CourseSelection::Pass;
        pass_test.player_number = i;
        pass_test.recursive_level = RECURSIVE_LEVEL - recursive_level;

        std::vector< CourseSelection > search_result;
        search_result.push_back( pass_test );

        double ev = recursive_search( world, recursive_level - 1, i, &search_result );

#if DEBUG_PRINT
        {
            std::stringstream buf;
            buf << "evaluation = " << ev << std::endl;
            dlog.flush();
        }
#endif

        ev = this->evaluateRoute( search_result, world );

        if ( ev > max_eval )
        {
#if DEBUG_PRINT
            dlog.flush();
#endif

            max_index = i;
            max_eval = ev;
            max_eval_course_list = search_result;
        }
    }

    if ( max_index != from )
    {
        M_first_best_player = max_index;
        M_result = max_eval_course_list;
    }

#if 0
    if ( M_table[from].push
         && M_result[0].type != CourseSelection::Shoot
         && M_result.size() <= 3 )
    {
        M_result[0].type = CourseSelection::Push;
        M_first_best_player = from;
    }
#endif


#if DEBUG_PRINT
    for ( size_t i = 0; i < M_result.size(); ++ i )
    {
        std::stringstream buf;
        buf << "result[" << i << "] = ";

        switch( M_result[i].type )
        {
        case CourseSelection::Hold:
            buf << "Hold";
            break;

        case CourseSelection::FindPassReceiver:
            buf << "Find_Pass_Receiver " << M_result[i].player_number;
            break;

        case CourseSelection::Pass:
            buf << "Pass " << M_result[i].player_number;
            break;

        case CourseSelection::Push:
            buf << "Push";
            break;

        case CourseSelection::Shoot:
            buf << "Shoot";
            break;

        default:
            buf << "???";
            break;
        }

        buf << ", recursive_level = "
            << M_result[i].recursive_level << std::endl;

        dlog.flush();
    }
#endif
}


double
PassCourseTable::recursive_search( const rcsc::WorldModel & world,
                                   int recursive_level,
                                   int from,
                                   std::vector< CourseSelection > * course_list ) const
{
    CourseSelection r;
    r.recursive_level = RECURSIVE_LEVEL - recursive_level;


    if ( M_table[from].goal )
    {
        r.type = CourseSelection::Shoot;
        r.player_number = from;

        course_list->push_back( r );

        return M_table[from].evaluation;
    }


    r.player_number = from;

    if ( M_table[from].push )
    {
        r.type = CourseSelection::Push;
    }
    else
    {
        r.type = CourseSelection::Hold;
    }


    if ( recursive_level == 0 )
    {
        course_list->push_back( r );

        return M_table[from].evaluation;
    }


    int max_index = from;
    double max_eval = M_table[from].evaluation;
    std::vector< CourseSelection > max_eval_course_list;

    std::vector< CourseSelection > test_course_list = *course_list;


    for ( int i = 1; i <= MAX_PLAYER; ++i )
    {
        if ( i == from
             || M_table[from].player[i] < 1.0 )
        {
            continue;
        }

        bool already_passed = false;

        for ( size_t h = 0; h < course_list->size(); ++h )
        {
            if ( (i == (*course_list)[h].player_number
                  || i == world.self().unum() )
                 && (*course_list)[h].type == CourseSelection::Pass )
            {
                already_passed = true;
                break;
            }
        }

        if ( already_passed )
        {
            continue;
        }


        CourseSelection pass_test;
        pass_test.type = CourseSelection::Pass;
        pass_test.player_number = i;
        pass_test.recursive_level = RECURSIVE_LEVEL - recursive_level;

        test_course_list.resize( RECURSIVE_LEVEL - recursive_level );
        test_course_list.push_back( pass_test );

        double ev = recursive_search( world, recursive_level - 1, i,
                                      &test_course_list );

        ev = this->evaluateRoute( test_course_list, world );

        if ( ev > max_eval )
        {
            max_index = i;
            max_eval = ev;
            max_eval_course_list = test_course_list;
        }
    }


    if ( max_index != from )
    {
        *course_list = max_eval_course_list;
    }
    else
    {
        course_list->push_back( r );
    }

    return max_eval;
}




double
PassCourseTable::pass_check( const rcsc::WorldModel & wm,
                             const Player_Reference & from,
                             const Player_Reference & to ) const
{
    if ( ! from || ! to )
    {
        return false;
    }

    if ( ( ( wm.gameMode().type() == rcsc::GameMode::KickOff_
             && wm.gameMode().side() == wm.ourSide() )
           || ( wm.gameMode().type() == rcsc::GameMode::FreeKick_
                && wm.gameMode().side() == wm.ourSide() ) )
         && from->unum() == to->unum() )
    {
        return 0.0;
    }

    const long VALID_TEAMMATE_ACCURACY = 8;
    const long VALID_OPPONENT_ACCURACY = 20;

    if ( from->isGhost()
         || to->isGhost()
         || from->posCount() > VALID_TEAMMATE_ACCURACY
         || to->posCount() > VALID_TEAMMATE_ACCURACY )
    {
        return 0.0;
    }


#if DEBUG_PRINT
    {
        std::stringstream buf;
        buf << "checking pass " << from->unum()
            << " to " << to->unum() << ": ";
        dlog.flush();
    }
#endif

    rcsc::Vector2D to_pos = to->pos() + to->vel() * PASS_RECEIVER_PREDICT_STEP;

    if ( ( to_pos - from->pos() ).r() <= 4.0 )
    {
#if DEBUG_PRINT
        dlog.flush();
#endif
        return 0.0;
    }

    if ( ( to_pos - from->pos() ).r() >= 35.0 )
    {
#if DEBUG_PRINT
        dlog.flush();
#endif
        return 0.0;
    }

    if ( to_pos.x >= wm.offsideLineX() )
    {
#if DEBUG_PRINT
        dlog.flush();
#endif
        return 0.0;
    }

    if ( to_pos.x <= rcsc::ServerParam::i().ourPenaltyAreaLineX()
         && to_pos.absY() <= rcsc::ServerParam::i().penaltyAreaHalfWidth() )
    {
#if DEBUG_PRINT
        dlog.flush();
#endif
        return 0.0;
    }

    if ( to_pos.absX() >= rcsc::ServerParam::i().pitchHalfLength()
         || to_pos.absY() >= rcsc::ServerParam::i().pitchHalfWidth() )
    {
#if DEBUG_PRINT
        dlog.flush();
#endif
        return 0.0;
    }

    if ( to->goalie()
         && to_pos.x < rcsc::ServerParam::i().ourPenaltyAreaLineX() + 1.0
         && to_pos.absY() < rcsc::ServerParam::i().penaltyAreaHalfWidth() + 1.0 )
    {
#if DEBUG_PRINT
        dlog.flush();
#endif
        return 0.0;
    }

    if ( to->goalie()
         && to_pos.x > wm.ourDefenseLineX() - 3.0 )
    {
#if DEBUG_PRINT
        dlog.flush();
#endif
        return 0.0;
    }


    rcsc::Vector2D from_pos = from->pos();

    if ( from->isSelf() )
    {
        from_pos = wm.ball().pos();
    }

    rcsc::AngleDeg test_dir = ( to_pos - from_pos ).th();

    double pass_course_cone = + 360.0;


    const double OVER_TEAMMATE_IGNORE_DISTANCE = ( to->pos().x >= +25.0
                                                   ? 2.0
                                                   : 5.0 );

    const rcsc::PlayerCont::const_iterator o_end = wm.opponents().end();
    for ( rcsc::PlayerCont::const_iterator opp = wm.opponents().begin();
          opp != o_end;
          ++opp )
    {
        if ( (*opp).posCount() > VALID_OPPONENT_ACCURACY )
        {
            continue;
        }

        if ( ( (*opp).pos() - from_pos ).r()
             > ( to_pos - from_pos ).r() + OVER_TEAMMATE_IGNORE_DISTANCE )
        {
            continue;
        }

        double angle_diff = ( ( (*opp).pos() - from_pos ).th() - test_dir ).abs();

        if ( from->isSelf() )
        {
            double kickable_dist = to->playerTypePtr()->playerSize()
                + to->playerTypePtr()->kickableMargin();
            double hide_radian = std::asin( std::min( kickable_dist / ( (*opp).pos() - from_pos ).r(),
                                                      1.0 ) );
            angle_diff = std::max( angle_diff
                                   - rcsc::AngleDeg::rad2deg( hide_radian ),
                                   0.0 );
        }

        if ( pass_course_cone > angle_diff )
        {
            pass_course_cone = angle_diff;
        }
    }


    double eval_threshold = PASS_EVAL_THRESHOLD;

    if ( to->pos().x - 2.0 <= from->pos().x )
    {
        eval_threshold = BACK_PASS_EVAL_THRESHOLD;
    }

    if ( from->pos().x >= +25.0
         && to->pos().x >= +25.0 )
    {
        eval_threshold = CHANCE_PASS_EVAL_THRESHOLD;
    }

    if ( from->goalie() )
    {
        eval_threshold = GOALIE_PASS_EVAL_THRESHOLD;
    }

    if ( from->isSelf()
         && from->distFromBall() <= 3.0 )
    {
#if 1
        if ( pass_course_cone > eval_threshold )
        {
            return pass_course_cone;
        }
        else
        {
# if DEBUG_PRINT
            std::stringstream buf;
            buf << "pass course too narrow: "
                << pass_course_cone << std::endl;
            dlog.flush();
# endif
            return 0.0;
        }
#else
        long self_access_step;
        long teammate_access_step;
        long opponent_access_step;
        long unknown_access_step;
        int first_access_teammate;
        int first_access_opponent;

        Simulator sim;

        rcsc::Vector2D vec( to->pos() - wm.self().pos() );
        vec.setLength( 2.5 );

        sim.simulate( wm,
                      wm.getPlayerCont
                      ( new rcsc::CoordinateAccuratePlayerPredicate( 20 ) ),
                      wm.ball().pos(),
                      vec,
                      2 /* ball_velocity_change_step */,
                      3 /* self_penalty_step */,
                      3 /* self_penalty_step */,
                      0 /* opponent_penalty_step */,
                      &self_access_step,
                      &teammate_access_step,
                      &opponent_access_step,
                      &unknown_access_step,
                      &first_access_teammate,
                      &first_access_opponent );

        long our_team_access_step = teammate_access_step;
        long opp_team_access_step = opponent_access_step;

        if ( self_access_step < our_team_access_step )
        {
            our_team_access_step = self_access_step;
        }

        if ( unknown_access_step < opp_team_access_step )
        {
            opp_team_access_step = unknown_access_step;
        }

        if ( our_team_access_step < opp_team_access_step
             || pass_course_cone > eval_threshold )
        {
            return pass_course_cone;
        }
        else
        {
# if DEBUG_PRINT
            std::stringstream buf;
            buf << "pass course too narrow: " << pass_course_cone << std::endl;
            dlog.print( buf.str().c_str() );
# endif
            return 0.0;
        }
#endif
    }
    else
    {
        if ( pass_course_cone > eval_threshold )
        {
            std::stringstream buf;
            buf << "ok " << pass_course_cone << std::endl;
            dlog.flush();

            return pass_course_cone;
        }
        else
        {
#if DEBUG_PRINT
            std::stringstream buf;
            buf << "pass course too narrow: "
                << pass_course_cone << std::endl;
            dlog.flush();
#endif
            return 0.0;
        }
    }
}


double
PassCourseTable::free_radius_check( const rcsc::WorldModel & wm,
                                    const Player_Reference & pl ) const
{
    if ( ! pl )
    {
        return 0.0;
    }

    const long VALID_TEAMMATE_ACCURACY = 20;
    const long VALID_OPPONENT_ACCURACY = 20;

    if ( pl->posCount() > VALID_TEAMMATE_ACCURACY
         || pl->isGhost() )
    {
        return 0.0;
    }


    double r_min = rcsc::ServerParam::i().pitchLength()
                   + rcsc::ServerParam::i().pitchWidth();

    const rcsc::PlayerCont::const_iterator o_end = wm.opponents().end();
    for ( rcsc::PlayerCont::const_iterator opp = wm.opponents().begin();
          opp != o_end;
          ++opp )
    {
        if ( (*opp).posCount() < VALID_OPPONENT_ACCURACY )
        {
            continue;
        }

        double r = ( (*opp).pos() - pl->pos() ).r();

        if ( r < r_min )
        {
            r_min = r;
        }
    }

    return r_min;
}

#if 0
double
PassCourseTable::free_forward_width_check( const rcsc::WorldModel & wm,
                                           const rcsc::Vector2D & point ) const
{
    const long VALID_PLAYER_ACCURACY = 50;

    Player_Set pl_set = wm.player_set
                           ( new And_Player_Predicate
                             ( new Opponent_Or_Unknown_Player_Predicate(),
                               new Coordinate_Accurate_Player_Predicate
                               ( VALID_PLAYER_ACCURACY ),
                               new X_Coordinate_Backward_Player_Predicate
                               ( wm.opponent_team().defence_line() + 0.01 ),
                               new X_Coordinate_Forward_Player_Predicate
                               ( point.x ) ) );

    double free_width = 20.0;

    Player_Set::iterator o_end != pl_set.end();
    for ( Player_Set::iterator opp = pl_set.begin();
          opp != o_end;
          ++opp )
    {
        double w = std::fabs( opp->pos().y - point.y );

        if ( w < free_width )
        {
            free_width = w;
        }
    }

    return free_width;
}
#endif

bool
PassCourseTable::shoot_check( const rcsc::WorldModel & wm,
                              const Player_Reference & from ) const
{
    const long VALID_TEAMMATE_ACCURACY = 20;
    const long VALID_OPPONENT_ACCURACY = 20;
    const long VALID_FAR_OPPONENT_ACCURACY = 40;

    if ( ! from )
    {
        return false;
    }

    if ( from->posCount() > VALID_TEAMMATE_ACCURACY
         || from->isGhost() )
    {
        return false;
    }

    if ( (from->pos() - rcsc::ServerParam::i().theirTeamGoalPos()).r() > 27.5 )
    {
        return false;
    }


    long acc = VALID_OPPONENT_ACCURACY;

    if ( from->pos().x <= 30.0 )
    {
        acc = VALID_FAR_OPPONENT_ACCURACY;
    }

#if 0
    return (from->pos() - rcsc::ServerParam::i().theirTeamGoalPos()).r()
            < 20.0;
#else
    return PassCourseTable::canShootFrom( wm,
                                          from->pos(),
                                          VALID_FAR_OPPONENT_ACCURACY );
#endif
}


bool
PassCourseTable::push_check( const rcsc::WorldModel & wm,
                             const Player_Reference & from ) const
{
    if ( ! from
         || (M_inhibit_self_push && from->isSelf()) )
    {
        return false;
    }

    if ( (wm.gameMode().type() == rcsc::GameMode::KickOff_
          && wm.gameMode().side() == wm.ourSide())
         || (wm.gameMode().type() == rcsc::GameMode::FreeKick_
             && wm.gameMode().side() == wm.ourSide())
         || (wm.gameMode().type() == rcsc::GameMode::KickIn_
             && wm.gameMode().side() == wm.ourSide()))
    {
        return false;
    }

    const long VALID_PLAYER_ACCURACY = 20;
    const long VALID_OPPONENT_ACCURACY = VALID_PLAYER_ACCURACY;
    const double FREE_WIDTH = 1.75;

    if ( ! from
         || from->posCount() > VALID_PLAYER_ACCURACY
         || from->isGhost() )
    {
        return false;
    }

#if DEBUG_PRINT
    {
        std::stringstream buf;
        buf << "checking push " << from->unum() << ": ";
        dlog.flush();
    }
#endif


    if ( from->pos().x > wm.offsideLineX() )
    {
#if DEBUG_PRINT
        dlog.flush();
#endif
        return false;
    }


    static const double WIDE_FREE_WIDTH = 10.0;

    if ( from->isSelf()
         || from->pos().x >= wm.offsideLineX() - 20.0 )
    {
        rcsc::AbstractPlayerCont opp_set;
        opp_set = wm.getPlayerCont( new rcsc::AndPlayerPredicate
                                    ( new rcsc::OpponentOrUnknownPlayerPredicate( wm ),
                                      new rcsc::CoordinateAccuratePlayerPredicate
                                      ( VALID_PLAYER_ACCURACY ),
                                      new rcsc::XCoordinateForwardPlayerPredicate
                                      ( from->pos().x ),
                                      new rcsc::XCoordinateBackwardPlayerPredicate
                                      ( wm.offsideLineX() + 1.0 ),
                                      new rcsc::YCoordinatePlusPlayerPredicate
                                      ( from->pos().y - WIDE_FREE_WIDTH ),
                                      new rcsc::YCoordinateMinusPlayerPredicate
                                      ( from->pos().y + WIDE_FREE_WIDTH ) ) );

        double free_length = rcsc_ext::AbstractPlayerCont_getMinimumEvaluation
                             ( opp_set,
                               new XPosPlayerEvaluator )
                             - from->pos().x;

        if ( free_length >= 12.5 )
        {
            dlog.flush();
            return true;
        }
    }


    if ( from->pos().x <= wm.ourDefenseLineX() + 10.0 )
    {
# if DEBUG_PRINT
        dlog.flush();
# endif
        return false;
    }

    if ( from->pos().x < rcsc_ext::WorldModel_forwardLineX( wm ) - 5.0 )
    {
# if DEBUG_PRINT
        std::stringstream buf;
        buf << "too far from our forward line: x = " << from->pos().x
            << ", forward line = " << rcsc_ext::WorldModel_forwardLineX( wm )
            << std::endl;
        dlog.flush();
# endif
        return false;
    }

    rcsc::PlayerCont::const_iterator o_end = wm.opponents().end();
    for ( rcsc::PlayerCont::const_iterator opp = wm.opponents().begin();
          opp != o_end;
          ++opp )
    {
        if ( (*opp).posCount() > VALID_OPPONENT_ACCURACY )
        {
            continue;
        }

        if ( (from->pos().x < (*opp).pos().x
              && (*opp).pos().x < from->pos().x + 5.0)
             && std::fabs( (*opp).pos().y - from->pos().y ) < FREE_WIDTH )
        {
# if DEBUG_PRINT
            dlog.flush();
# endif
            return false;
        }
    }

    return true;
}


double
PassCourseTable::static_evaluation( const rcsc::WorldModel & wm,
                                    int player_number ) const
{
    const rcsc::AbstractPlayerObject * teammate = wm.ourPlayer( player_number );

    //
    // if teammate is invalid, return bad evaluation
    //
    if ( ! teammate )
    {
        return - DBL_MAX / 2.0;
    }

    //
    // enforce pass when our kick off
    //
    if ( ( wm.gameMode().type() == rcsc::GameMode::KickOff_
           && wm.gameMode().side() == wm.ourSide() )
         && player_number == wm.self().unum() )
    {
        return - DBL_MAX / 2.0;
    }


    //
    // set basic point
    //
    double point = teammate->pos().x;


    //
    // add bonus for goal, push
    //
    if ( M_table[player_number].goal == true )
    {
        point += 1000.0;

        // XXX
#if 0
        Angle shoot_width = Act_Shoot::free_angle_with_ball_coordinate
                            ( wm,
                              Act_Shoot::DEFAULT_VALID_OPPONENT_ACCURACY,
                              teammate->pos() );

        point += std::max( shoot_width.degree(), 20.0 ) * 10.0;
#endif

        if ( player_number == wm.self().unum() )
        {
            point += 500.0;
        }
    }
    else if ( M_table[player_number].push == true )
    {
        point += 500.0 + ( rcsc::ServerParam::i().pitchHalfWidth()
                           - teammate->pos().absX() ) * 2.0;

        if ( player_number == wm.self().unum() )
        {
            point += 500.0;
        }
    }

    //
    // add distance bonus when near goal
    //
    double goal_dist = ( teammate->pos()
                         - rcsc::ServerParam::i().theirTeamGoalPos() ).r();

    if ( goal_dist <= 30.0 )
    {
        point += (30.0 - goal_dist) * 5.0;
    }
    point -= goal_dist;


    //
    // add free radius bonus
    //
#if 1
    point += std::min( M_table[player_number].free_radius, 10.0 );
#else
    // XXX: need more tune
    point += std::min( M_table[player_number].free_radius, 10.0 ) * 10.0;
#endif


    //
    // add pass count bonus
    //
    int pass_count = 0;
    for ( int i = 1; i <= MAX_PLAYER; ++i )
    {
        if ( M_table[player_number].player[i]
             && i != wm.self().unum() )
        {
            ++pass_count;
        }
    }
    point += std::min( pass_count, 3 ) * 3.0;

    return point;
}


double
PassCourseTable::evaluateRoute( const std::vector< CourseSelection > & route,
                                const rcsc::WorldModel & world ) const
{
    this->prepare_table( world );

    assert( ! route.empty() );

    double base_eval = M_table[(*(route.rbegin())).player_number].evaluation;

    double eval = base_eval;

    int holder = world.self().unum();

    const std::vector< CourseSelection >::const_iterator end = route.end();
    std::vector< CourseSelection >::const_iterator r;
    for ( r = route.begin(); r != end; ++r )
    {
        double radius = M_table[ holder ].free_radius;

        if ( (*r).type == CourseSelection::Pass )
        {
            int pass_target = (*r).player_number;

            eval *= std::min( radius, 10.0 ) * 0.1;
            eval *= 0.95;

            holder = pass_target;
        }
        else
        {
            if ( (*r).player_number == world.self().unum() )
            {
                eval *= std::min( radius, 10.0 ) * 0.1;
            }

            break;
        }
    }

    return eval;
}


void
PassCourseTable::setDebugPassRoute( rcsc::PlayerAgent * agent ) const
{
    const rcsc::WorldModel & wm = agent->world();

    this->prepare_result( wm,
                          RECURSIVE_LEVEL,
                          wm.self().unum() );

    CourseSelection first = get_first_selection( wm );

    if ( first.type != CourseSelection::Pass )
    {
        return;
    }

    agent->debugClient().addMessage( "pass" );
    agent->debugClient().setTarget( M_first_best_player );

    int ball_holder = M_first_best_player;
    for ( size_t i = 1;
          i < M_result.size();
          ++i )
    {
        if ( M_result[i].type == CourseSelection::Pass
             || M_result[i].type == CourseSelection::FindPassReceiver )
        {
            int next_ball_holder = M_result[i].player_number;

            agent->debugClient().addLine( wm.ourPlayer( ball_holder )->pos(),
                                          wm.ourPlayer( next_ball_holder )->pos() );
            ball_holder = next_ball_holder;
        }
        else if ( M_result[i].type == CourseSelection::Shoot )
        {
            agent->debugClient().addLine
                ( wm.ourPlayer( ball_holder )->pos(),
                  rcsc::Vector2D( rcsc::ServerParam::i().pitchHalfLength(),
                                  0.0 ) );
        }
    }
}


double
PassCourseTable::get_recursive_pass_ball_speed( const double & distance )
{
#if 0
    return std::min( 2.5,
                     std::min( rcsc::calc_first_term_geom_series_last
                               ( 1.8,
                                 distance,
                                 rcsc::ServerParam::i().ballDecay() ),
                               rcsc::ServerParam::i().ballSpeedMax() ) );
#elif 0
    if ( distance >= 30.0 )
    {
        return 3.0;
    }
    else if ( distance >= 20.0 )
    {
        return 2.7;
    }
    else if ( distance >= 14.0 )
    {
        return 2.4;
    }
    else if ( distance >= 8.0 )
    {
        return 2.0;
    }
    else if ( distance >= 5.0 )
    {
        return 1.6;
    }
    else if ( distance >= 3.0 )
    {
        return 1.3;
    }
    else
    {
        return 1.0;
    }
#else
    if ( distance >= 20.0 )
    {
        return 3.0;
    }
    else if ( distance >= 8.0 )
    {
        return 2.5;
    }
    else if ( distance >= 5.0 )
    {
        return 1.8;
    }
    else
    {
        return 1.5;
    }
#endif
}



bool
PassCourseTable::canShootFrom( const rcsc::WorldModel & wm,
                               const rcsc::Vector2D & pos,
                               long valid_opponent_threshold )
{
    static const double SHOOTABLE_THRESHOLD = 12.0;

    return getFreeAngleFromPos( wm, valid_opponent_threshold, pos )
            >= SHOOTABLE_THRESHOLD;
}

double
PassCourseTable::getFreeAngleFromPos( const rcsc::WorldModel & wm,
                                      long valid_opponent_threshold,
                                      const rcsc::Vector2D & pos )
{
    const double goal_half_width = rcsc::ServerParam::i().goalHalfWidth();
    const double field_half_length = rcsc::ServerParam::i().pitchHalfLength();

    const rcsc::Vector2D goal_center = rcsc::ServerParam::i().theirTeamGoalPos();
    const rcsc::Vector2D goal_left( goal_center.x, +goal_half_width );
    const rcsc::Vector2D goal_right( goal_center.x, -goal_half_width );

    const rcsc::Vector2D goal_center_left( field_half_length,
                                           (+ goal_half_width - 1.5) / 2.0 );

    const rcsc::Vector2D goal_center_right( field_half_length,
                                            (- goal_half_width + 1.5) / 2.0 );


    double center_angle = getMinimumFreeAngle( wm,
                                           goal_center,
                                           valid_opponent_threshold,
                                           pos );

    double left_angle   = getMinimumFreeAngle( wm,
                                               goal_left,
                                               valid_opponent_threshold,
                                               pos );

    double right_angle  = getMinimumFreeAngle( wm,
                                               goal_right,
                                               valid_opponent_threshold,
                                               pos );

    double center_left_angle  = getMinimumFreeAngle
                                           ( wm,
                                             goal_center_left,
                                             valid_opponent_threshold,
                                             pos );

    double center_right_angle = getMinimumFreeAngle
                                           ( wm,
                                             goal_center_right,
                                             valid_opponent_threshold,
                                             pos );

    return std::max( center_angle,
                     std::max( left_angle,
                               std::max( right_angle,
                                         std::max( center_left_angle,
                                                   center_right_angle ) ) ) );
}


double
PassCourseTable::getMinimumFreeAngle( const rcsc::WorldModel & wm,
                                  const rcsc::Vector2D & goal,
                                  long valid_opponent_threshold,
                                  const rcsc::Vector2D & pos )
{
    rcsc::AngleDeg test_dir = (goal - pos).th();

    double shoot_course_cone = +360.0;

    rcsc::AbstractPlayerCont opp_set;
    opp_set = wm.getPlayerCont( new rcsc::AndPlayerPredicate
                                ( new rcsc::OpponentOrUnknownPlayerPredicate
                                      ( wm ),
                                  new rcsc::CoordinateAccuratePlayerPredicate
                                      ( valid_opponent_threshold ) ) );

    rcsc::AbstractPlayerCont::const_iterator o_end = opp_set.end();
    for ( rcsc::AbstractPlayerCont::const_iterator opp = opp_set.begin();
          opp != o_end;
          ++opp )
    {
        double controllable_dist;

        if ( (*opp)->goalie() )
        {
            controllable_dist = rcsc::ServerParam::i().catchAreaLength();
        }
        else
        {
            controllable_dist = (*opp)->playerTypePtr()->kickableArea();
        }

        rcsc::Vector2D relative_player = (*opp)->pos() - pos;

        double hide_angle_radian = ( std::asin
                                     ( std::min( controllable_dist
                                                 / relative_player.r(),
                                                 1.0 ) ) );
        double angle_diff = std::max( (relative_player.th() - test_dir).abs()
                                      - hide_angle_radian / M_PI * 180,
                                      0.0 );

        if ( shoot_course_cone > angle_diff )
        {
            shoot_course_cone = angle_diff;
        }
    }

    return shoot_course_cone;
}



rcsc::GameTime PassCourseTable::S_cache_time( -1, 0 );
boost::shared_ptr< PassCourseTable > PassCourseTable::S_cache;

const PassCourseTable &
PassCourseTable::instance( const rcsc::WorldModel & wm,
                           bool inhibit_self_push )
{
    if ( S_cache_time != wm.time()
         || ! S_cache
         || S_cache->M_inhibit_self_push != inhibit_self_push )
    {
        S_cache = boost::shared_ptr< PassCourseTable >
                  ( new PassCourseTable( inhibit_self_push ) );
        S_cache_time = wm.time();
    }

    return *S_cache;
}
