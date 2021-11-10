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

#include "bhv_set_play.h"

#include "bhv_set_play_free_kick.h"
#include "bhv_set_play_goal_kick.h"
#include "bhv_set_play_kick_off.h"
#include "bhv_set_play_kick_in.h"
#include "bhv_their_goal_kick_move.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/bhv_before_kick_off.h>
#include <rcsc/action/bhv_scan_field.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/line_2d.h>

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_SetPlay::execute( rcsc::PlayerAgent * agent )
{
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Bhv_SetPlay" );

    if ( ! agent->world().ball().posValid() )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": invalid ball pos" );
        return rcsc::Bhv_ScanField().execute( agent );
    }

    switch ( agent->world().gameMode().type() ) {
    case rcsc::GameMode::KickOff_:
        if ( agent->world().gameMode().side() == agent->world().ourSide() )
        {
            return Bhv_SetPlayKickOff( M_home_pos ).execute( agent );
        }
        else
        {
            rcsc::Vector2D target_point = M_home_pos;
            if ( target_point.x > -0.5 )
            {
                target_point.x = -0.5;
            }
            doBasicTheirSetPlayMove( agent, target_point );
        }
        break;
    case rcsc::GameMode::KickIn_:
        if ( agent->world().gameMode().side() == agent->world().ourSide() )
        {
            return Bhv_SetPlayKickIn( M_home_pos ).execute( agent );
        }
        else
        {
            doBasicTheirSetPlayMove( agent, M_home_pos );
        }
        break;
    case rcsc::GameMode::GoalKick_:
        if ( agent->world().gameMode().side() == agent->world().ourSide() )
        {
            return Bhv_SetPlayGoalKick( M_home_pos ).execute( agent );
        }
        else
        {
            return Bhv_TheirGoalKickMove( M_home_pos ).execute( agent );
        }
        break;
#if 0
    case rcsc::GameMode::FreeKick_:
    case rcsc::GameMode::CornerKick_:
    case rcsc::GameMode::GoalieCatch_: // after catch
    case rcsc::GameMode::Offside_:
    case rcsc::GameMode::FreeKickFault_:
    case rcsc::GameMode::BackPass_:
    case rcsc::GameMode::CatchFault_:
    case rcsc::GameMode::IndFreeKick_:
#endif
    default:
        break;
    }


    if ( agent->world().gameMode().isOurSetPlay( agent->world().ourSide() ) )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": our set play" );
        return Bhv_SetPlayFreeKick( M_home_pos ).execute( agent );
    }
    else
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": their set play. myhome=(%.1f, %.1f)",
                            M_home_pos.x, M_home_pos.y );
        doBasicTheirSetPlayMove( agent, M_home_pos );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
double
Bhv_SetPlay::get_set_play_dash_power( const rcsc::PlayerAgent * agent )
{
    if ( agent->world().gameMode().type() == rcsc::GameMode::PenaltySetup_ )
    {
        return agent->world().self()
            .getSafetyDashPower( rcsc::ServerParam::i().maxPower() );
    }

    double rate;
    if ( agent->world().self().stamina() > rcsc::ServerParam::i().staminaMax() * 0.8 )
    {
        rate = 1.5
            * agent->world().self().stamina()
            / rcsc::ServerParam::i().staminaMax();
    }
    else
    {
        rate = 0.9
            * ( agent->world().self().stamina()
                - rcsc::ServerParam::i().recoverDecThrValue() )
            / rcsc::ServerParam::i().staminaMax();
        rate = std::max( 0.0, rate );
    }

    return ( agent->world().self().playerType().staminaIncMax()
             * agent->world().self().recovery()
             * rate );
}

/*-----------------------------------------------------------------------------*
 * recursive function
 *
 *-----------------------------------------------------------------------------*/
namespace {

rcsc::Vector2D
get_avoid_circle_point( const rcsc::WorldModel & world,
                        const rcsc::Vector2D & point,
                        int depth )
{
    if ( depth > 5 )
    {
        return point;
    }

    if ( world.ball().distFromSelf() < world.self().pos().dist( point )
         && ( ( world.ball().pos() - point ).th()
              - ( world.self().pos() - point ).th() ).abs() < 90.0
         && ( world.ball().angleFromSelf()
              - ( point - world.self().pos() ).th() ).abs() < 90.0
         && ( rcsc::Line2D( world.self().pos(), point).dist2( world.ball().pos() )
              < 10.0 * 10.0 )
         )
    {
        rcsc::Vector2D new_point = world.ball().pos();
        rcsc::AngleDeg self2target = ( point - world.self().pos() ).th();
        if ( world.ball().angleFromSelf().isLeftOf( self2target ) )
        {
            new_point += rcsc::Vector2D::polar2vector( 11.5,
                                                       self2target + 90.0 );
        }
        else
        {
            new_point += rcsc::Vector2D::polar2vector( 11.5,
                                                       self2target - 90.0 );
        }
        // recursive
        return get_avoid_circle_point( world, new_point, depth + 1 );
    }

    return point;
}

} // end noname namespace

/*-------------------------------------------------------------------*/
/*!
  execute action
*/
void
Bhv_SetPlay::doBasicTheirSetPlayMove( rcsc::PlayerAgent * agent,
                                      const rcsc::Vector2D & target_point )
{
    double dash_power = Bhv_SetPlay::get_set_play_dash_power( agent );
    rcsc::Vector2D new_target = target_point;

    rcsc::Vector2D ball_to_target = target_point - agent->world().ball().pos();
    double target2ball_dist = ball_to_target.r();
    if ( target2ball_dist < 11.0 )
    {
        if ( agent->world().ball().pos().x < rcsc::ServerParam::i().ourPenaltyAreaLineX() + 1.0 )
        {
            // ball is in the behind of penalty area line
            double ydiff = std::sqrt( 11.0 * 11.0
                                      - ball_to_target.x * ball_to_target.x );
            if ( agent->world().ball().pos().y < 0.0 )
            {
                new_target.y = agent->world().ball().pos().y + ydiff;
            }
            else
            {
                new_target.y = agent->world().ball().pos().y - ydiff;
            }
        }
        else
        {
            new_target.x
                = agent->world().ball().pos().x
                - std::sqrt( 11.0 * 11.0
                             - ball_to_target.y * ball_to_target.y );
            if ( new_target.x < -45.0 )
            {
                new_target = agent->world().ball().pos();
                new_target += ball_to_target.setLengthVector( 11.0 );
                target2ball_dist = 11.0;
            }
        }
    }

    if ( agent->world().self().pos().absY() > rcsc::ServerParam::i().pitchHalfWidth()
         && agent->world().self().pos().x < agent->world().ball().pos().x + 11.0
         && agent->world().ball().pos().x < agent->world().self().pos().x )
    {
        // subtarget may be out of area.
        // at first, player should back to safety area
        new_target = agent->world().ball().pos();
        new_target.x += 12.0;
        new_target.y *= 0.9;
    }
    else
    {
        // recursive search
        new_target = get_avoid_circle_point( agent->world(), new_target, 0 );
    }

    double dist_thr = agent->world().ball().distFromSelf() * 0.1;
    if ( dist_thr < 0.7 ) dist_thr = 0.7;

    agent->debugClient().setTarget( new_target );
    agent->debugClient().addCircle( new_target, dist_thr );

    if ( ! rcsc::Body_GoToPoint( new_target,
                                 dist_thr,
                                 dash_power
                                 ).execute( agent ) )
    {
        // already there
        rcsc::AngleDeg body_angle = agent->world().ball().angleFromSelf();
        if ( body_angle.degree() < 0.0 ) body_angle -= 90.0;
        else body_angle += 90.0;

        rcsc::Body_TurnToAngle( body_angle ).execute( agent );
    }

    agent->setNeckAction( new rcsc::Neck_TurnToBall() );
}
