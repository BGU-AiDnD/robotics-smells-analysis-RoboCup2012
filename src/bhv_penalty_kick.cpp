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

#include "bhv_penalty_kick.h"

#include "bhv_savior.h"

#include  "strategy.h"

#include "bhv_goalie_chase_ball.h"
#include "bhv_goalie_basic_move.h"
#include "bhv_go_to_static_ball.h"

#include "body_clear_ball2008.h"
#include "body_dribble2008.h"
#include "body_intercept2008.h"
#include "body_smart_kick.h"

#include "neck_chase_ball.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/bhv_go_to_point_look_ball.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/body_stop_dash.h>
#include <rcsc/action/body_stop_ball.h>
#include <rcsc/action/neck_scan_field.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/penalty_kick_state.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/rect_2d.h>
#include <rcsc/geom/ray_2d.h>
#include <rcsc/soccer_math.h>
#include <rcsc/math_util.h>

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_PenaltyKick::isKickTaker( rcsc::PlayerAgent* agent )
{
    const rcsc::PenaltyKickState * state = agent->world().penaltyKickState();
    int taker_unum
        = 11 - ( ( state->ourTakerCounter() - 1 ) % 11 );
    return taker_unum == agent->world().self().unum();
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_PenaltyKick::execute( rcsc::PlayerAgent * agent )
{
    const rcsc::PenaltyKickState * state = agent->world().penaltyKickState();
    switch ( agent->world().gameMode().type() ) {
    case rcsc::GameMode::PenaltySetup_:
        if ( state->currentTakerSide() == agent->world().ourSide() )
        {
            if ( isKickTaker( agent ) )
            {
                return doKickerSetup( agent );
            }
        }
        // their kick phase
        else
        {
            if ( agent->world().self().goalie() )
            {
                return doGoalieSetup( agent );
            }
        }
        break;
    case rcsc::GameMode::PenaltyReady_:
        if ( state->currentTakerSide() == agent->world().ourSide() )
        {
            if ( isKickTaker( agent ) )
            {
                return doKickerReady( agent );
            }
        }
        // their kick phase
        else
        {
            if ( agent->world().self().goalie() )
            {
                return doGoalieSetup( agent );
            }
        }
        break;
    case rcsc::GameMode::PenaltyTaken_:
        if ( state->currentTakerSide() == agent->world().ourSide() )
        {
            if ( isKickTaker( agent ) )
            {
                return doKicker( agent );
            }
        }
        // their kick phase
        else
        {
            if ( agent->world().self().goalie() )
            {
                return doGoalie( agent );
            }
        }
        break;
    case rcsc::GameMode::PenaltyScore_:
    case rcsc::GameMode::PenaltyMiss_:
        if ( state->currentTakerSide() == agent->world().ourSide() )
        {
            if ( agent->world().self().goalie() )
            {
                return doGoalieSetup( agent );
            }
        }
        break;
    case rcsc::GameMode::PenaltyOnfield_:
    case rcsc::GameMode::PenaltyFoul_:
        break;
    default:
        // nothing to do.
        std::cerr << "Is current playmode not a Penalty Shootout???" << std::endl;
        return false;
    }


    if ( agent->world().self().goalie() )
    {
        return doGoalieWait( agent );
    }
    else
    {
        return doKickerWait( agent );
    }

    // never reach here
    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_PenaltyKick::doKickerWait( rcsc::PlayerAgent* agent )
{
    //int myid = agent->world().self().unum() - 1;
    //rcsc::Vector2D wait_pos(1, 6.5, 15.0 * myid);
    //rcsc::Vector2D wait_pos(1, 5.5, 90.0 + (180.0 / 12.0) * agent->world().self().unum());
    double dist_step = ( 9.0 + 9.0 ) / 12;
    rcsc::Vector2D wait_pos( -2.0, -9.8 + dist_step * agent->world().self().unum() );

    // already there
    if ( agent->world().self().pos().dist( wait_pos ) < 0.7 )
    {
        rcsc::Bhv_NeckBodyToBall().execute( agent );
    }
    else
    {
        // no dodge
        rcsc::Body_GoToPoint( wait_pos,
                              0.7,
                              rcsc::ServerParam::i().maxPower()
                              ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_TurnToRelative( 0.0 ) );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_PenaltyKick::doKickerSetup( rcsc::PlayerAgent* agent )
{
    const rcsc::PenaltyKickState * state = agent->world().penaltyKickState();
    const double onfield_flag = ( state->onfieldSide() == agent->world().ourSide()
                                  ? -1.0
                                  : 1.0 );
    const rcsc::Vector2D goal_c(rcsc::ServerParam::i().pitchHalfLength() * onfield_flag, 0.0);
    const rcsc::PlayerObject* opp_goalie = agent->world().getOpponentGoalie();
    rcsc::AngleDeg place_angle = 0.0;
    if ( onfield_flag < 0.0 )
    {
        place_angle += 180.0;
    }

    // ball is close enoughly.
    if ( ! Bhv_GoToStaticBall( place_angle ).execute( agent ) )
    {
        rcsc::Vector2D face_point( rcsc::ServerParam::i().pitchHalfLength(),
                                   0.0 );
        face_point.x *= onfield_flag;

        rcsc::Body_TurnToPoint( face_point ).execute( agent );
        if ( opp_goalie )
        {
            agent->setNeckAction( new rcsc::Neck_TurnToPoint( opp_goalie->pos() ) );
        }
        else
        {
            agent->setNeckAction( new rcsc::Neck_TurnToPoint( goal_c ) );
        }
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_PenaltyKick::doKickerReady( rcsc::PlayerAgent* agent )
{
    const rcsc::PenaltyKickState * state = agent->world().penaltyKickState();
    // stamina recovering...
    if ( agent->world().self().stamina() < rcsc::ServerParam::i().staminaMax() - 10.0
         && ( agent->world().time().cycle()
              - state->time().cycle()
              > rcsc::ServerParam::i().penReadyWait() - 3 )
         )
    {
        return doKickerSetup( agent );
    }

    if ( ! agent->world().self().isKickable() )
    {
        return doKickerSetup( agent );
    }

    return doKicker( agent );
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_PenaltyKick::doKicker( rcsc::PlayerAgent* agent )
{
    /////////////////////////////////////////////////
    // case: server does NOT allow multiple kicks
    if ( ! rcsc::ServerParam::i().penAllowMultKicks() )
    {
        return doOneKickShoot( agent );
    }

    /////////////////////////////////////////////////
    // case: server allows multiple kicks

    // get ball
    if ( ! agent->world().self().isKickable() )
    {
        if ( ! rcsc::Body_Intercept2008().execute( agent ) )
        {
            rcsc::Body_GoToPoint( agent->world().ball().pos(),
                                  0.4,
                                  rcsc::ServerParam::i().maxPower()
                                  ).execute( agent );
        }

        if ( agent->world().ball().posCount() > 0 )
        {
            agent->setNeckAction( new rcsc::Neck_TurnToBall() );
        }
        else
        {
            const rcsc::PlayerObject* opp_goalie = agent->world().getOpponentGoalie();
            if ( opp_goalie )
            {
                agent->setNeckAction( new rcsc::Neck_TurnToPoint( opp_goalie->pos() ) );
            }
            else
            {
                agent->setNeckAction( new rcsc::Neck_ScanField() );
            }
        }

        return true;
    }

    // kick decision
    if ( doShoot( agent ) )
    {
        return true;
    }

    return doDribble( agent );
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_PenaltyKick::doOneKickShoot( rcsc::PlayerAgent* agent )
{
    const rcsc::PenaltyKickState * state = agent->world().penaltyKickState();
    const double ball_speed = agent->world().ball().vel().r();
    // ball is moveng --> kick has taken.
    if ( ! rcsc::ServerParam::i().penAllowMultKicks()
         && ball_speed > 0.3 )
    {
        return false;
    }

    // go to the ball side
    if ( ! agent->world().self().isKickable() )
    {
        rcsc::Body_GoToPoint( agent->world().ball().pos(),
                              0.4,
                              rcsc::ServerParam::i().maxPower()
                              ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_TurnToBall() );
        return true;
    }

    // turn to the ball to get the maximal kick rate.
    if ( ( agent->world().ball().angleFromSelf() - agent->world().self().body() ).abs()
         > 3.0 )
    {
        rcsc::Body_TurnToBall().execute( agent );
        const rcsc::PlayerObject* opp_goalie = agent->world().getOpponentGoalie();
        if ( opp_goalie )
        {
            agent->setNeckAction( new rcsc::Neck_TurnToPoint( opp_goalie->pos() ) );
        }
        else
        {
            rcsc::Vector2D goal_c(rcsc::ServerParam::i().pitchHalfLength(), 0.0);
            if ( state->onfieldSide()
                 == agent->world().ourSide() )
            {
                goal_c.x *= -1.0;
            }
            agent->setNeckAction( new rcsc::Neck_TurnToPoint( goal_c ) );
        }
        return true;
    }

    // decide shot target point
    rcsc::Vector2D shot_point(rcsc::ServerParam::i().pitchHalfLength(), 0.0);
    if ( state->onfieldSide()
         == agent->world().ourSide() )
    {
        shot_point *= -1.0;
    }

    const rcsc::PlayerObject* opp_goalie = agent->world().getOpponentGoalie();
    if ( opp_goalie )
    {
        shot_point.y = rcsc::ServerParam::i().goalHalfWidth() - 1.0;
        if ( opp_goalie->pos().absY() > 0.5 )
        {
            if ( opp_goalie->pos().y > 0.0 )
            {
                shot_point.y *= -1.0;
            }
        }
        else if ( opp_goalie->bodyCount() < 2 )
        {
            if ( opp_goalie->body().degree() > 0.0 )
            {
                shot_point.y *= -1.0;
            }
        }
    }

    // enforce one step kick
    rcsc::Body_KickOneStep( shot_point,
                            rcsc::ServerParam::i().ballSpeedMax()
                            ).execute( agent );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_PenaltyKick::doShoot( rcsc::PlayerAgent* agent )
{
    const rcsc::PenaltyKickState * state = agent->world().penaltyKickState();

    if ( agent->world().time().cycle()
         - state->time().cycle()
         > rcsc::ServerParam::i().penTakenWait() - 25 )
    {
        return doOneKickShoot( agent );
    }


    rcsc::Vector2D shot_point;
    double shot_speed;

    if ( getShootTarget( agent, &shot_point, &shot_speed ) )
    {
#ifdef USE_SMART_KICK
        rcsc::Body_SmartKick( shot_point,
                              shot_speed,
                              shot_speed * 0.96,
                              3 ).execute( agent );
#else
        rcsc::Body_KickMultiStep( shot_point, shot_speed ).execute( agent );
#endif
        agent->setNeckAction( new rcsc::Neck_TurnToPoint( shot_point ) );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_PenaltyKick::getShootTarget( const rcsc::PlayerAgent* agent,
                                 rcsc::Vector2D* point, double* first_speed )
{
    const rcsc::PenaltyKickState * state = agent->world().penaltyKickState();

    // check if I am in shootable area.
    if ( agent->world().self().pos().absX() < rcsc::ServerParam::i().pitchHalfLength() - 20.0
         || agent->world().self().pos().absY() > rcsc::ServerParam::i().penaltyAreaHalfWidth() )
    {
        return false;
    }

    const rcsc::PlayerObject* opp_goalie = agent->world().getOpponentGoalie();

    // goalie is not found.
    if ( ! opp_goalie )
    {
        rcsc::Vector2D shot_c(rcsc::ServerParam::i().pitchHalfLength(), 0.0);
        if ( state->onfieldSide()
             == agent->world().ourSide() )
        {
            shot_c.x *= -1.0;
        }
        if ( point ) *point = shot_c;
        if ( first_speed ) *first_speed = rcsc::ServerParam::i().ballSpeedMax();

        return true;
    }

    int best_l_or_r = 0;
    double best_speed = rcsc::ServerParam::i().ballSpeedMax() + 1.0;


    double post_buf = 1.0
        + std::min( 1.0,
                    ( rcsc::ServerParam::i().pitchHalfLength()
                      - agent->world().self().pos().absX() )
                    * 0.1 );

    // consder only 2 angle
    rcsc::Vector2D shot_l( rcsc::ServerParam::i().pitchHalfLength(),
                           -rcsc::ServerParam::i().goalHalfWidth() + post_buf );
    rcsc::Vector2D shot_r( rcsc::ServerParam::i().pitchHalfLength(),
                           rcsc::ServerParam::i().goalHalfWidth() - post_buf );
    if ( state->onfieldSide() == agent->world().ourSide() )
    {
        shot_l *= -1.0;
        shot_r *= -1.0;
    }

    const rcsc::AngleDeg angle_l = ( shot_l - agent->world().ball().pos() ).th();
    const rcsc::AngleDeg angle_r = ( shot_r - agent->world().ball().pos() ).th();

    // !!! Magic Number !!!
    const double goalie_max_speed = 1.0;
    // default player speed max * conf decay
    const double goalie_dist_buf
        = goalie_max_speed * std::min( 5, opp_goalie->posCount() )
        + rcsc::ServerParam::i().catchAreaLength()
        + 0.2;

    const rcsc::Vector2D goalie_next_pos = opp_goalie->pos() + opp_goalie->vel();

    for ( int i = 0; i < 2; i++ )
    {
        const rcsc::Vector2D& target = ( i == 0 ? shot_l : shot_r );
        const rcsc::AngleDeg& angle = ( i == 0 ? angle_l : angle_r );

        double dist2goal = agent->world().ball().pos().dist( target );

        // at first, set minimal speed to reach the goal line
        double tmp_first_speed
            =  ( dist2goal + 5.0 ) * ( 1.0 - rcsc::ServerParam::i().ballDecay() );
        tmp_first_speed = std::max( 1.2, tmp_first_speed );
        bool not_over_max = true;
        while ( not_over_max )
        {
            if ( tmp_first_speed > rcsc::ServerParam::i().ballSpeedMax() )
            {
                not_over_max = false;
                tmp_first_speed = rcsc::ServerParam::i().ballSpeedMax();
            }

            rcsc::Vector2D ball_pos = agent->world().ball().pos();
            rcsc::Vector2D ball_vel
                = rcsc::Vector2D::polar2vector( tmp_first_speed, angle );
            ball_pos += ball_vel;
            ball_vel *= rcsc::ServerParam::i().ballDecay();

            bool goalie_can_reach = false;

            // goalie move at first step is ignored (cycle is set to ZERO),
            // because goalie must look the ball velocity before chasing action.
            double cycle = 0.0;
            while ( ball_pos.absX() < rcsc::ServerParam::i().pitchHalfLength() )
            {
                if ( goalie_next_pos.dist( ball_pos )
                     < goalie_max_speed * cycle + goalie_dist_buf )
                {
                    rcsc::dlog.addText( rcsc::Logger::TEAM,
                                        __FILE__": shoot. goalie can reach. cycle=%.0f"
                                        " target=(%.1f, %.1f) speed=%.1f",
                                        cycle + 1.0, target.x, target.y, tmp_first_speed );
                    goalie_can_reach = true;
                    break;
                }

                ball_pos += ball_vel;
                ball_vel *= rcsc::ServerParam::i().ballDecay();
                cycle += 1.0;
            }

            if ( ! goalie_can_reach )
            {
                rcsc::dlog.addText( rcsc::Logger::TEAM,
                                    __FILE__": shoot. goalie never reach. target=(%.1f, %.1f) speed=%.1f",
                                    target.x, target.y,
                                    tmp_first_speed );
                if ( tmp_first_speed < best_speed )
                {
                    best_l_or_r = i;
                    best_speed = tmp_first_speed;
                }
                break; // end of this angle
            }
            tmp_first_speed += 0.4;
        }
    }


    if ( best_speed <= rcsc::ServerParam::i().ballSpeedMax() )
    {
        if ( point )
        {
            *point = ( best_l_or_r == 0 ? shot_l : shot_r );
        }
        if ( first_speed )
        {
            *first_speed = best_speed;
        }

        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!
  dribble to the shootable point
*/
bool
Bhv_PenaltyKick::doDribble( rcsc::PlayerAgent* agent )
{
    static bool S_target_reversed = false;

    const rcsc::PenaltyKickState * state = agent->world().penaltyKickState();

    const double onfield_flag
        = ( state->onfieldSide() == agent->world().ourSide()
            ? -1.0 : 1.0 );
    const rcsc::Vector2D goal_c(rcsc::ServerParam::i().pitchHalfLength() * onfield_flag, 0.0);

    const double penalty_abs_x = rcsc::ServerParam::i().theirPenaltyAreaLineX();

    const rcsc::PlayerObject* opp_goalie = agent->world().getOpponentGoalie();
    const double goalie_max_speed = 1.0;

    const double my_abs_x = agent->world().self().pos().absX();

    const double goalie_dist = ( opp_goalie
                                 ? ( opp_goalie->pos().dist(agent->world().self().pos())
                                     - goalie_max_speed * std::min(5, opp_goalie->posCount()) )
                                 : 200.0 );
    const double goalie_abs_x = ( opp_goalie
                                  ? opp_goalie->pos().absX()
                                  : 200.0 );

    /////////////////////////////////////////////////
    // dribble parametors

    const double base_target_abs_y = rcsc::ServerParam::i().goalHalfWidth() + 4.0;
    rcsc::Vector2D drib_target = goal_c;
    double drib_power = rcsc::ServerParam::i().maxPower();
    int drib_dashes = 6;

    /////////////////////////////////////////////////

    // it's too far to the goal.
    // dribble to the shootable area
    if ( my_abs_x < penalty_abs_x - 3.0
         && goalie_dist > 10.0 )
    {
        //drib_power *= 0.6;
    }
    else
    {
        if ( goalie_abs_x > my_abs_x )
        {
            if ( goalie_dist < 4.0 )
            {
                S_target_reversed = ! S_target_reversed;
            }

            if ( S_target_reversed )
            {
                if ( agent->world().self().pos().y < -base_target_abs_y + 2.0 )
                {
                    S_target_reversed = false;
                    drib_target.y = base_target_abs_y;
                    rcsc::dlog.addText( rcsc::Logger::TEAM,
                                        __FILE__": dribble(1). target=(%.1f, %.1f)",
                                        drib_target.x, drib_target.y );
                }
                else
                {
                    drib_target.y = -base_target_abs_y;
                    rcsc::dlog.addText( rcsc::Logger::TEAM,
                                        __FILE__": dribble(2). target=(%.1f, %.1f)",
                                        drib_target.x, drib_target.y );
                }
            }
            else // == if ( ! S_target_reversed )
            {
                if ( agent->world().self().pos().y > base_target_abs_y - 2.0 )
                {
                    S_target_reversed = true;
                    drib_target.y = -base_target_abs_y;
                    rcsc::dlog.addText( rcsc::Logger::TEAM,
                                        __FILE__": dribble(3). target=(%.1f, %.1f)",
                                        drib_target.x, drib_target.y );
                }
                else
                {
                    drib_target.y = base_target_abs_y;
                    rcsc::dlog.addText( rcsc::Logger::TEAM,
                                        __FILE__": dribble(4). target=(%.1f, %.1f)",
                                        drib_target.x, drib_target.y );
                }
            }

            drib_target.x = goalie_abs_x + 1.0;
            drib_target.x = rcsc::min_max( penalty_abs_x - 2.0,
                                           drib_target.x,
                                           rcsc::ServerParam::i().pitchHalfLength() - 4.0 );
            drib_target.x *= onfield_flag;

            double dashes = ( agent->world().self().pos().dist( drib_target )
                              / rcsc::ServerParam::i().defaultPlayerSpeedMax() );
            drib_dashes = static_cast<int>(floor(dashes));
            drib_dashes = rcsc::min_max( 1, drib_dashes, 6 );
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": dribble. target=(%.1f, %.1f) dashes=%d",
                                drib_target.x, drib_target.y, drib_dashes );
        }
    }


    if ( opp_goalie && goalie_dist < 3.0 )
    {
        rcsc::AngleDeg drib_angle = ( drib_target - agent->world().self().pos() ).th();
        rcsc::AngleDeg goalie_angle = ( opp_goalie->pos() - agent->world().self().pos() ).th();
        drib_dashes = 6;
        if ( (drib_angle - goalie_angle).abs() < 80.0 )
        {
            drib_target = agent->world().self().pos();
            drib_target += rcsc::Vector2D::polar2vector( 10.0, goalie_angle + 180.0 );
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": dribble. avoid goalie. target=(%.1f, %.1f)",
                                drib_target.x, drib_target.y );
        }
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": dribble. goalie near. dashes=%d",
                            drib_dashes );
    }

    rcsc::Vector2D target_rel = drib_target - agent->world().self().pos();
    double buf = 2.0;
    if ( drib_target.absX() < penalty_abs_x )
    {
        buf += 2.0;
    }
    if ( target_rel.absX() < 5.0
         && ( opp_goalie == NULL
              || opp_goalie->pos().dist( drib_target ) > target_rel.r() - buf )
         )
    {
        if ( ( target_rel.th() - agent->world().self().body() ).abs() < 5.0 )
        {
            double first_speed
                = rcsc::calc_first_term_geom_series_last
                ( 0.5,
                  target_rel.r(),
                  rcsc::ServerParam::i().ballDecay() );

            first_speed = std::min( first_speed, rcsc::ServerParam::i().ballSpeedMax() );
#ifdef USE_SMART_KICK
            rcsc::Body_SmartKick( drib_target,
                                  first_speed,
                                  first_speed * 0.96,
                                  3 ).execute( agent );
#else
            rcsc::Body_KickMultiStep( drib_target, first_speed ).execute( agent );
#endif
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": kick. to=(%.1f, %.1f) first_speed=%.1f",
                                drib_target.x, drib_target.y, first_speed );
        }
        else
        {
            rcsc::Body_StopBall().execute( agent );
        }
    }
    else
    {
        bool dodge_mode = true;
        if ( opp_goalie == NULL
             || ( ( opp_goalie->pos() - agent->world().self().pos() ).th()
                  - ( drib_target - agent->world().self().pos() ).th() ).abs() > 45.0 )
        {
            dodge_mode = false;
        }

        rcsc::Body_Dribble2008( drib_target,
                                2.0,
                                drib_power,
                                drib_dashes,
                                dodge_mode
                                ).execute( agent );
    }

    if ( opp_goalie )
    {
        agent->setNeckAction( new rcsc::Neck_TurnToPoint( opp_goalie->pos() ) );
    }
    else
    {
        agent->setNeckAction( new rcsc::Neck_ScanField() );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_PenaltyKick::doGoalieWait( rcsc::PlayerAgent* agent )
{
    const rcsc::PenaltyKickState * state = agent->world().penaltyKickState();

    rcsc::Vector2D wait_pos( - rcsc::ServerParam::i().pitchHalfLength() - 2.0, 25.0 );

    if ( state->onfieldSide() != agent->world().ourSide() )
    {
        wait_pos.x *= -1.0;
    }
    if ( agent->world().ourSide() == rcsc::LEFT )
    {
        wait_pos.y = -25.0;
    }

    rcsc::Vector2D face_point( wait_pos.x * 0.5, 0.0 );
    if ( agent->world().self().pos().dist2( wait_pos ) < 1.0 )
    {
        rcsc::Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_TurnToBall() );
    }
    else
    {
        rcsc::Body_GoToPoint( wait_pos,
                              0.5,
                              rcsc::ServerParam::i().maxPower()
                              ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_TurnToPoint( face_point ) );
    }
    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_PenaltyKick::doGoalieSetup( rcsc::PlayerAgent* agent )
{
    const rcsc::PenaltyKickState * state = agent->world().penaltyKickState();

    rcsc::Vector2D move_point;
    if ( Strategy::i().savior() )
    {
        move_point.assign( rcsc::ServerParam::i().ourTeamGoalLineX()
                           + rcsc::ServerParam::i().penMaxGoalieDistX() - 0.1,
                           0.0 );
    }
    else
    {
        move_point.assign( -50.0, 0.0 );
    }
    move_point.x = std::min( - rcsc::ServerParam::i().pitchHalfLength()
                             + rcsc::ServerParam::i().penMaxGoalieDistX()
                             - 0.5,
                             move_point.x );
    if ( state->onfieldSide() != agent->world().ourSide() )
    {
        move_point.x *= -1.0;
    }

    if ( rcsc::Body_GoToPoint( move_point,
                               0.5,
                               rcsc::ServerParam::i().maxPower()
                               ).execute( agent ) )
    {
        agent->setNeckAction( new rcsc::Neck_TurnToBall() );
        return true;
    }

    // already there
    if ( Strategy::i().savior() )
    {
        if ( std::fabs( agent->world().self().body().abs() ) > 2.0 )
        {
            rcsc::Vector2D face_point( 0.0, 0.0 );
            rcsc::Body_TurnToPoint( face_point ).execute( agent );
        }

        agent->setNeckAction( new rcsc_ext::Neck_ChaseBall() );
    }
    else
    {
        if ( std::fabs( 90.0 - agent->world().self().body().abs() ) > 2.0 )
        {
            rcsc::Vector2D face_point( agent->world().self().pos().x, 100.0 );
            if ( agent->world().self().body().degree() < 0.0 )
            {
                face_point.y = -100.0;
            }
            rcsc::Body_TurnToPoint( face_point ).execute( agent );
        }

        rcsc::Vector2D neck_point( 0.0, 0.0 );
        agent->setNeckAction( new rcsc::Neck_TurnToPoint( neck_point ) );
    }
    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_PenaltyKick::doGoalie( rcsc::PlayerAgent* agent )
{
    const rcsc::PenaltyKickState * state = agent->world().penaltyKickState();

    ///////////////////////////////////////////////
    // check if catchabale
    rcsc::Rect2D our_penalty( rcsc::Vector2D( -rcsc::ServerParam::i().pitchHalfLength(),
                                              -rcsc::ServerParam::i().penaltyAreaHalfWidth() + 1.0 ),
                              rcsc::Size2D( rcsc::ServerParam::i().penaltyAreaLength() - 1.0,
                                            rcsc::ServerParam::i().penaltyAreaWidth() - 2.0 ) );
    if ( state->onfieldSide() != agent->world().ourSide() )
    {
        our_penalty.assign( rcsc::Vector2D( rcsc::ServerParam::i().theirPenaltyAreaLineX() + 1.0,
                                            -rcsc::ServerParam::i().penaltyAreaHalfWidth() + 1.0 ),
                            rcsc::Size2D( rcsc::ServerParam::i().penaltyAreaLength() - 1.0,
                                          rcsc::ServerParam::i().penaltyAreaWidth() - 2.0 ) );
    }

    if ( our_penalty.contains( agent->world().ball().pos() ) )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": goalie try to catch" );
        return agent->doCatch();
    }

    if ( agent->world().self().isKickable() )
    {
        Body_ClearBall2008().execute( agent );
        agent->setNeckAction( new rcsc::Neck_TurnToBall() );
        return true;
    }


    ///////////////////////////////////////////////
    // if taker can only one kick, goalie should stay the front of goal.
    if ( ! rcsc::ServerParam::i().penAllowMultKicks() )
    {
        // kick has not taken.
        if ( agent->world().ball().vel().r2() < 0.01
             && agent->world().ball().pos().absX() < ( rcsc::ServerParam::i().pitchHalfLength()
                                                       - rcsc::ServerParam::i().penDistX() - 1.0 )
             )
        {
            return doGoalieSetup( agent );
        }
        if ( agent->world().ball().vel().r2() > 0.01 )
        {
            return doGoalieSlideChase( agent );
        }
    }

    ///////////////////////////////////////////////
    //
    if ( Strategy::i().savior() )
    {
        return Bhv_Savior().execute( agent );
    }
    else
    {
        return doGoalieBasicMove( agent );
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_PenaltyKick::doGoalieBasicMove( rcsc::PlayerAgent* agent )
{
    const rcsc::PenaltyKickState * state = agent->world().penaltyKickState();

    rcsc::Rect2D our_penalty( rcsc::Vector2D( -rcsc::ServerParam::i().pitchHalfLength(),
                                              -rcsc::ServerParam::i().penaltyAreaHalfWidth() + 1.0 ),
                              rcsc::Size2D( rcsc::ServerParam::i().penaltyAreaLength() - 1.0,
                                            rcsc::ServerParam::i().penaltyAreaWidth() - 2.0 ) );
    if ( state->onfieldSide() != agent->world().ourSide() )
    {
        our_penalty.assign( rcsc::Vector2D( rcsc::ServerParam::i().theirPenaltyAreaLineX() + 1.0,
                                            -rcsc::ServerParam::i().penaltyAreaHalfWidth() + 1.0 ),
                            rcsc::Size2D( rcsc::ServerParam::i().penaltyAreaLength() - 1.0,
                                          rcsc::ServerParam::i().penaltyAreaWidth() - 2.0 ) );
    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": goalieBasicMove. " );

    ////////////////////////////////////////////////////////////////////////
    // get active interception catch point
    const int self_min = agent->world().interceptTable()->selfReachCycle();
    rcsc::Vector2D move_pos = agent->world().ball().inertiaPoint( self_min );

    if ( our_penalty.contains( move_pos ) )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": goalieBasicMove. exist intercept point " );
        agent->debugClient().addMessage( "ExistIntPoint" );
        if ( agent->world().interceptTable()->opponentReachCycle()
             < agent->world().interceptTable()-> selfReachCycle()
             || agent->world().interceptTable()-> selfReachCycle() <=4 )
        {
            if ( rcsc::Body_Intercept2008( false ).execute( agent ) )
            {
                agent->debugClient().addMessage( "Intercept" );
                rcsc::dlog.addText( rcsc::Logger::TEAM,
                                    __FILE__": goalieBasicMove. do intercept " );
                agent->setNeckAction( new rcsc::Neck_TurnToBall() );
                return true;
            }
        }
    }


    rcsc::Vector2D my_pos = agent->world().self().pos();
    rcsc::Vector2D ball_pos;
    if ( agent->world().existKickableOpponent() )
    {
        ball_pos = agent->world().opponentsFromBall().front()->pos();
        ball_pos += agent->world().opponentsFromBall().front()->vel();
    }
    else
    {
        ball_pos = rcsc::inertia_n_step_point( agent->world().ball().pos(),
                                               agent->world().ball().vel(),
                                               3,
                                               rcsc::ServerParam::i().ballDecay() );
    }

    if ( state->onfieldSide() != agent->world().ourSide() )
    {
        my_pos *= -1.0;
        ball_pos *= -1.0;
    }

    move_pos = getGoalieMovePos(ball_pos, my_pos);

    if ( state->onfieldSide() != agent->world().ourSide() )
    {
        move_pos *= -1.0;
    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": goalie basic move to (%.1f, %.1f)",
                        move_pos.x, move_pos.y );
    agent->debugClient().setTarget( move_pos );
    agent->debugClient().addMessage( "BasicMove" );

    if ( ! rcsc::Body_GoToPoint( move_pos,
                                 0.5,
                                 rcsc::ServerParam::i().maxPower()
                                 ).execute( agent ) )
    {
        // already there
        rcsc::AngleDeg face_angle = agent->world().ball().angleFromSelf();
        if ( agent->world().ball().angleFromSelf().isLeftOf( agent->world().self().body() ) )
        {
            face_angle += 90.0;
        }
        else
        {
            face_angle -= 90.0;
        }
        rcsc::Body_TurnToAngle( face_angle ).execute( agent );
    }
    agent->setNeckAction( new rcsc::Neck_TurnToBall() );

    return true;
}

/*-------------------------------------------------------------------*/
/*!
  ball_pos & my_pos is set to self localization oriented.
  if ( onfiled_side != our_side ), these coordinates must be reversed.
*/
rcsc::Vector2D
Bhv_PenaltyKick::getGoalieMovePos( const rcsc::Vector2D & ball_pos,
                                   const rcsc::Vector2D & my_pos )
{
    const double min_x = ( -rcsc::ServerParam::i().pitchHalfLength()
                           + rcsc::ServerParam::i().catchAreaLength() * 0.9 );

    if ( ball_pos.x < -49.0 )
    {
        if ( ball_pos.absY() < rcsc::ServerParam::i().goalHalfWidth() )
        {
            return rcsc::Vector2D( min_x, ball_pos.y );
        }
        else
        {
            return rcsc::Vector2D( min_x,
                                   ( rcsc::sign( ball_pos.y )
                                     * rcsc::ServerParam::i().goalHalfWidth() ) );
        }
    }

    rcsc::Vector2D goal_l( -rcsc::ServerParam::i().pitchHalfLength(),
                           -rcsc::ServerParam::i().goalHalfWidth() );
    rcsc::Vector2D goal_r( -rcsc::ServerParam::i().pitchHalfLength(),
                           rcsc::ServerParam::i().goalHalfWidth() );

    rcsc::AngleDeg ball2post_angle_l = ( goal_l - ball_pos ).th();
    rcsc::AngleDeg ball2post_angle_r = ( goal_r - ball_pos ).th();

    // NOTE: post_angle_r < post_angle_l
    rcsc::AngleDeg line_dir = rcsc::AngleDeg::bisect( ball2post_angle_r,
                                                      ball2post_angle_l );

    rcsc::Line2D line_mid( ball_pos, line_dir );
    rcsc::Line2D goal_line( goal_l, goal_r );

    rcsc::Vector2D intersection = goal_line.intersection( line_mid );
    if ( intersection.isValid() )
    {
        rcsc::Line2D line_l( ball_pos, goal_l );
        rcsc::Line2D line_r( ball_pos, goal_r );

        rcsc::AngleDeg alpha = rcsc::AngleDeg::atan2_deg( rcsc::ServerParam::i().goalHalfWidth(),
                                                          rcsc::ServerParam::i().penaltyAreaLength() - 2.5 );
        double	dist_from_goal
            = ( ( line_l.dist( intersection ) + line_r.dist( intersection ) ) * 0.5 )
            / alpha.sin();

        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": goalie move. intersection=(%.1f, %.1f) dist_from_goal=%.1f",
                            intersection.x, intersection.y, dist_from_goal );
        if ( dist_from_goal <= rcsc::ServerParam::i().goalHalfWidth() )
        {
            dist_from_goal = rcsc::ServerParam::i().goalHalfWidth();
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": goalie move. outer of goal. dist_from_goal=%.1f",
                                dist_from_goal );
        }

        if ( ( ball_pos - intersection ).r() + 1.5 < dist_from_goal )
        {
            dist_from_goal = ( ball_pos - intersection ).r() + 1.5;
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": goalie move. near than ball. dist_from_goal=%.1f",
                                dist_from_goal );
        }

        rcsc::AngleDeg position_error = line_dir - (intersection - my_pos).th();

        const double danger_angle = 21.0;
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": goalie move position_error_angle=%.1f",
                            position_error.degree() );
        if ( position_error.abs() > danger_angle )
        {
            dist_from_goal *= ( ( 1.0 - ((position_error.abs() - danger_angle)
                                         / (180.0 - danger_angle)) )
                                * 0.5 );
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": goalie move. error is big. dist_from_goal=%.1f",
                                dist_from_goal );
        }

        rcsc::Vector2D result = intersection;
        rcsc::Vector2D add_vec = ball_pos - intersection;
        add_vec.setLength( dist_from_goal );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": goalie move. intersection=(%.1f, %.1f) add_vec=(%.1f, %.1f)%.2f",
                            intersection.x, intersection.y,
                            add_vec.x, add_vec.y,
                            add_vec.r() );
        result += add_vec;
        if ( result.x < min_x )
        {
            result.x = min_x;
        }
        return result;
    }
    else
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": goalie move. shot line has no intersection with goal line" );

        if ( ball_pos.x > 0.0 )
        {
            return rcsc::Vector2D(min_x , goal_l.y);
        }
        else if ( ball_pos.x < 0.0 )
        {
            return rcsc::Vector2D(min_x , goal_r.y);
        }
        else
        {
            return rcsc::Vector2D(min_x , 0.0);
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_PenaltyKick::doGoalieChaseBall( rcsc::PlayerAgent * )
{


    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_PenaltyKick::doGoalieSlideChase( rcsc::PlayerAgent* agent )
{
    const rcsc::WorldModel & wm = agent->world();

    if ( std::fabs( 90.0 - wm.self().body().abs() ) > 2.0 )
    {
        rcsc::Vector2D face_point( wm.self().pos().x, 100.0);
        if ( wm.self().body().degree() < 0.0 )
        {
            face_point.y = -100.0;
        }
        rcsc::Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_TurnToBall() );
        return true;
    }

    rcsc::Ray2D ball_ray( wm.ball().pos(), wm.ball().vel().th() );
    rcsc::Line2D ball_line( ball_ray.origin(), ball_ray.dir() );
    rcsc::Line2D my_line( wm.self().pos(), wm.self().body() );


    rcsc::Vector2D intersection = my_line.intersection( ball_line );
    if ( ! intersection.isValid()
         || ! ball_ray.inRightDir( intersection ) )
    {
        rcsc::Body_Intercept2008( false ).execute( agent ); // goalie mode
        agent->setNeckAction( new rcsc::Neck_TurnToBall() );
        return true;
    }

    if ( agent->world().self().pos().dist( intersection )
         < rcsc::ServerParam::i().catchAreaLength() * 0.7 )
    {
        rcsc::Body_StopDash( false ).execute( agent ); // not save recovery
        agent->setNeckAction( new rcsc::Neck_TurnToBall() );
        return true;
    }

    rcsc::AngleDeg angle = ( intersection - agent->world().self().pos() ).th();
    double dash_power = rcsc::ServerParam::i().maxPower();
    if ( ( angle - agent->world().self().body() ).abs() > 90.0 )
    {
        dash_power *= -1.0;
    }
    agent->doDash( dash_power );
    agent->setNeckAction( new rcsc::Neck_TurnToBall() );
    return true;
}
