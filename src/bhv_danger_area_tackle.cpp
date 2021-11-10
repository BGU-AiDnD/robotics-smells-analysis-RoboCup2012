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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "bhv_danger_area_tackle.h"

#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/line_2d.h>
#include <rcsc/geom/ray_2d.h>

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_DangerAreaTackle::execute( rcsc::PlayerAgent * agent )
{
//     static rcsc::GameTime s_last_execute_time( 0, 0 );

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Bhv_DangerAreaTackle" );

    const rcsc::WorldModel & wm = agent->world();

//     if ( s_last_execute_time == wm.time() )
//     {
//         rcsc::dlog.addText( rcsc::Logger::TEAM,
//                             __FILE__": called several times" );
//         return false;
//     }
//     s_last_execute_time = wm.time();

    if ( clearGoal( agent ) )
    {
        return true;
    }

    if ( wm.self().tackleProbability() < M_min_probability )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": failed. low tackle_prob=%.2f < %.2f",
                            wm.self().tackleProbability(),
                            M_min_probability );
        return false;
    }

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    bool ball_shall_be_in_our_goal = false;


    //
    // check where the ball shall be gone without tackle
    //
#if 0
    ball_shall_be_in_our_goal = false;

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": ball_shall_be_in_our_goal unchecked" );
#else
    const double goal_half_width = rcsc::ServerParam::i().goalHalfWidth();

    const rcsc::Vector2D goal_center = rcsc::ServerParam::i().ourTeamGoalPos();
    const rcsc::Vector2D goal_left_post( goal_center.x, +goal_half_width );
    const rcsc::Vector2D goal_right_post( goal_center.x, -goal_half_width );
    bool is_shoot_ball = ( ( (goal_left_post - wm.ball().pos() ).th()
                             - wm.ball().vel().th() ).degree() < 0
                           && ( ( goal_right_post - wm.ball().pos() ).th()
                                - wm.ball().vel().th() ).degree() > 0 );

    const int self_reach_cycle = wm.interceptTable()->selfReachCycle();

    if ( is_shoot_ball
         && wm.ball().inertiaPoint( self_reach_cycle ).x
            <= rcsc::ServerParam::i().ourTeamGoalLineX() )
    {
        ball_shall_be_in_our_goal = true;
    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": ball_shall_be_in_our_goal = %s",
                        ( ball_shall_be_in_our_goal ? "true" : "false" ) );
#endif

    if ( wm.existKickableOpponent()
         || ball_shall_be_in_our_goal
         || ( opp_min < self_min
              && opp_min < mate_min ) )
    {

    }
    else
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": failed. not necessary." );
        return false;
    }

    //
    // v11 or older
    //

    if ( agent->config().version() < 12.0 )
    {
        return executeOld( agent );
    }
    else
    {
        return executeV12( agent );
    }

    return false;
}


/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_DangerAreaTackle::clearGoal( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();

    if ( wm.self().tackleProbability() <= 0.0 )
    {
        return false;
    }

    const rcsc::ServerParam & param = rcsc::ServerParam::i();

    const int self_min = wm.interceptTable()->selfReachCycle();

    const rcsc::Vector2D self_trap_pos = wm.ball().inertiaPoint( self_min );
    if ( self_trap_pos.x > - param.pitchHalfLength() + 0.5 )
    {
        return false;
    }

    //
    // cannot intercept the ball in the field
    //

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": my trap pos(%.1f %.1f) < pitch.x",
                        self_trap_pos.x, self_trap_pos.y );

    const rcsc::Ray2D ball_ray( wm.ball().pos(), wm.ball().vel().th() );
    const rcsc::Line2D goal_line( rcsc::Vector2D( - param.pitchHalfLength(), 10.0 ),
                                  rcsc::Vector2D( - param.pitchHalfLength(), -10.0 ) );
    const rcsc::Vector2D intersect =  ball_ray.intersection( goal_line );
    if ( ! intersect.isValid()
         || intersect.absY() > param.goalHalfWidth() + 0.5 )
    {
        return false;
    }

    //
    // ball is moving to our goal
    //

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": ball is moving to our goal" );


    if ( agent->config().version() < 12.0 )
    {
        double tackle_power = ( wm.self().body().abs() > 90.0
                                ? param.maxTacklePower()
                                : - param.maxBackTacklePower() );

        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": clear goal" );
        agent->debugClient().addMessage( "tackleClearOld%.0f", tackle_power );
        agent->doTackle( tackle_power );
        agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
        return true;
    }

    //
    // search best angle
    //

    const rcsc::Line2D line_c( rcsc::Vector2D( -param.pitchHalfLength(), 0.0 ),
                               rcsc::Vector2D( 0.0, 0.0 ) );
    const rcsc::Line2D line_l( rcsc::Vector2D( -param.pitchHalfLength(), -param.goalHalfWidth() ),
                               rcsc::Vector2D( 0.0, -param.goalHalfWidth() ) );
    const rcsc::Line2D line_r( rcsc::Vector2D( -param.pitchHalfLength(), -param.goalHalfWidth() ),
                               rcsc::Vector2D( 0.0, -param.goalHalfWidth() ) );

    const rcsc::AngleDeg ball_rel_angle
        = wm.ball().angleFromSelf() - wm.self().body();
    const double tackle_rate
        = ( param.tacklePowerRate()
            * ( 1.0 - 0.5 * ( ball_rel_angle.abs() / 180.0 ) ) );

    rcsc::AngleDeg best_angle = 0.0;
    double max_speed = -1.0;

    for ( double a = -180.0; a < 180.0; a += 10.0 )
    {
        rcsc::AngleDeg target_rel_angle = a - wm.self().body().degree();

        double eff_power = param.maxBackTacklePower()
            + ( ( param.maxTacklePower() - param.maxBackTacklePower() )
                * ( 1.0 - target_rel_angle.abs() / 180.0 ) );
        eff_power *= tackle_rate;

        rcsc::Vector2D vel = wm.ball().vel()
            + rcsc::Vector2D::polar2vector( eff_power, rcsc::AngleDeg( a ) );
        rcsc::AngleDeg vel_angle = vel.th();

        if ( vel_angle.abs() > 80.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": clearGoal() angle=%.1f. vel_angle=%.1f is dangerouns",
                                a,
                                vel_angle.degree() );
            continue;
        }


        double speed = vel.r();

        int n_intersects = 0;
        if ( ball_ray.intersection( line_c ).isValid() ) ++n_intersects;
        if ( ball_ray.intersection( line_l ).isValid() ) ++n_intersects;
        if ( ball_ray.intersection( line_r ).isValid() ) ++n_intersects;

        if ( n_intersects == 3 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                "__ angle=%.1f vel=(%.1f %.1f)"
                                " 3 intersects with v_lines. angle is dangerous.",
                                a, vel.x, vel.y );
            speed -= 2.0;
        }
        else if ( n_intersects == 2
                  && wm.ball().pos().absY() > 3.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                "__ executeV12() angle=%.1f vel=(%.1f %.1f)"
                                " 2 intersects with v_lines. angle is dangerous.",
                                a, vel.x, vel.y );
            speed -= 2.0;
        }

        if ( speed > max_speed )
        {
            max_speed = speed;
            best_angle = target_rel_angle + wm.self().body();
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": clearGoal() update. angle=%.1f vel_angle=%.1f speed=%.2f",
                                a,
                                vel_angle.degree(),
                                speed );
        }
        else
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": clearGoal() no_update. angle=%.1f vel_angle=%.1f speed=%.2f",
                                a,
                                vel_angle.degree(),
                                speed );
        }
    }

    //
    // never accelerate the ball
    //

    if ( max_speed < 1.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": failed clearGoal. max_speed=%.3f", max_speed );
        return false;
    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": clear goal" );
    agent->debugClient().addMessage( "tackleClear%.0f", best_angle.degree() );
    agent->doTackle( ( best_angle - wm.self().body() ).degree() );
    agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );

    return true;
}


/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_DangerAreaTackle::executeOld( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();

    if ( wm.self().pos().absY() > rcsc::ServerParam::i().goalHalfWidth() + 5.0 )
    {
        // out of goal
        double tackle_power = rcsc::ServerParam::i().maxTacklePower();
        if ( wm.self().body().abs() < 10.0 )
        {

        }
        else if ( wm.self().body().abs() > 170.0 )
        {
            tackle_power = - rcsc::ServerParam::i().maxBackTacklePower();
            if ( tackle_power >= -1.0 )
            {
                return false;
            }
        }
        else if ( wm.self().body().degree() * wm.self().pos().y < 0.0 )
        {
            tackle_power = - rcsc::ServerParam::i().maxBackTacklePower();
            if ( tackle_power >= -1.0 )
            {
                return false;
            }
        }

        if ( std::fabs( tackle_power ) < 1.0 )
        {
            return false;
        }

        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": out of goal width" );
        agent->debugClient().addMessage( "tackle(1)" );
        agent->doTackle( tackle_power );
        agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
        return true;
    }
    else
    {
        // within goal width
        int power_sign = 0;
        double abs_body = wm.self().body().abs();

        if ( abs_body < 70.0 ) power_sign = 1;
        if ( abs_body > 110.0 ) power_sign = -1;
        if ( power_sign == 0 )
        {
            power_sign = ( wm.self().body().degree() > 0.0
                           ? 1
                           : -1 );
            if ( wm.ball().pos().y < 0.0 )
            {
                power_sign *= -1;
            }
        }

        double tackle_power = ( power_sign >= 0
                                ? rcsc::ServerParam::i().maxTacklePower()
                                : - rcsc::ServerParam::i().maxBackTacklePower() );
        if ( std::fabs( tackle_power ) < 1.0 )
        {
            return false;
        }

        if ( power_sign != 0 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": power_sign=%d",
                                power_sign );
            agent->debugClient().addMessage( "tackle(%d)", power_sign );
            agent->doTackle( tackle_power );
            agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_DangerAreaTackle::executeV12( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();
    const rcsc::ServerParam & param = rcsc::ServerParam::i();
    const rcsc::PlayerPtrCont::const_iterator o_end = wm.opponentsFromBall().end();

    const rcsc::Vector2D goal( - param.pitchHalfLength(), 0.0 );
    const rcsc::Vector2D virtual_accel = ( wm.existKickableOpponent()
                                           ? ( goal - wm.ball().pos() ).setLengthVector( 1.0 )
                                           : rcsc::Vector2D( 0.0, 0.0 ) );
    const rcsc::Line2D goal_line( rcsc::Vector2D( - param.pitchHalfLength(), 10.0 ),
                                  rcsc::Vector2D( - param.pitchHalfLength(), -10.0 ) );
    const rcsc::Line2D line_c( rcsc::Vector2D( -param.pitchHalfLength(), 0.0 ),
                               rcsc::Vector2D( 0.0, 0.0 ) );
    const rcsc::Line2D line_l( rcsc::Vector2D( -param.pitchHalfLength(), -param.goalHalfWidth() ),
                               rcsc::Vector2D( 0.0, -param.goalHalfWidth() ) );
    const rcsc::Line2D line_r( rcsc::Vector2D( -param.pitchHalfLength(), -param.goalHalfWidth() ),
                               rcsc::Vector2D( 0.0, -param.goalHalfWidth() ) );

    const rcsc::AngleDeg ball_rel_angle
        = wm.ball().angleFromSelf() - wm.self().body();
    const double tackle_rate
        = ( param.tacklePowerRate()
            * ( 1.0 - 0.5 * ( ball_rel_angle.abs() / 180.0 ) ) );

    rcsc::AngleDeg best_angle = 0.0;
    double max_speed = -1.0;

    for ( double a = -180.0; a < 180.0; a += 10.0 )
    {
        rcsc::AngleDeg rel_angle = a - wm.self().body().degree();

        double eff_power = param.maxBackTacklePower()
            + ( ( param.maxTacklePower() - param.maxBackTacklePower() )
                * ( 1.0 - rel_angle.abs() / 180.0 ) );
        eff_power *= tackle_rate;

        rcsc::Vector2D vel = ( wm.ball().vel()
                               + rcsc::Vector2D::polar2vector( eff_power, rcsc::AngleDeg( a ) ) );
        vel += virtual_accel;

        const rcsc::Ray2D ball_ray( wm.ball().pos(), vel.th() );
        const rcsc::Vector2D intersect =  ball_ray.intersection( goal_line );
        if ( intersect.isValid()
             && intersect.absY() < param.goalHalfWidth() + 5.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": executeV12() angle=%.1f vel=(%.1f %.1f)"
                                " intersect is dangerous.",
                                a, vel.x, vel.y );
            continue;
        }

        const rcsc::Vector2D ball_next = wm.ball().pos() + vel;

        bool maybe_opponent_get_ball = false;
        for ( rcsc::PlayerPtrCont::const_iterator o = wm.opponentsFromBall().begin();
              o != o_end;
              ++o )
        {
            if ( (*o)->posCount() > 10 ) continue;
            if ( (*o)->isGhost() ) continue;
            if ( (*o)->isTackling() ) continue;
            if ( (*o)->distFromBall() > 6.0 ) break;;

            rcsc::Vector2D opp_pos = (*o)->pos() + (*o)->vel();
            if ( opp_pos.dist( ball_next ) < 1.0 )
            {
                maybe_opponent_get_ball = true;
                break;
            }
        }

        if ( maybe_opponent_get_ball )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": executeV12() angle=%.1f vel=(%.1f %.1f)"
                                " maybe opponent get ball.",
                                a, vel.x, vel.y );
            continue;
        }

        double speed = vel.r();


        int n_intersects = 0;
        if ( ball_ray.intersection( line_c ).isValid() ) ++n_intersects;
        if ( ball_ray.intersection( line_l ).isValid() ) ++n_intersects;
        if ( ball_ray.intersection( line_r ).isValid() ) ++n_intersects;

        if ( n_intersects == 3 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                "__ angle=%.1f vel=(%.1f %.1f)"
                                " 3 intersects with v_lines. angle is dangerous.",
                                a, vel.x, vel.y );
            speed -=2.0;
        }
        else if ( n_intersects == 2
                  && wm.ball().pos().absY() > 3.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                "__ angle=%.1f vel=(%.1f %.1f)"
                                " 2 intersects with v_lines. angle is dangerous.",
                                a, vel.x, vel.y );
            speed -= 2.0;
        }

        if ( speed > max_speed )
        {
            max_speed = speed;
            best_angle = a;
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": executeV12() angle=%.1f vel=(%.1f %.1f)%.2f"
                                " update.",
                                a, vel.x, vel.y, speed );
        }
        else
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": executeV12() angle=%.1f vel=(%.1f %.1f)%.2f"
                                " no update.",
                                a, vel.x, vel.y, speed );
        }
    }

    //
    // never accelerate the ball
    //

    if ( max_speed < 1.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": failed executeV12 max_speed=%.3f" );
        return false;
    }

    //
    // execute tackle
    //

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": danger area tackle angle=%.0f", best_angle.degree() );
    agent->debugClient().addMessage( "DanAreaTackle%.0f", best_angle.degree() );
    agent->doTackle( ( best_angle - wm.self().body() ).degree() );
    agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );

    return true;

}
