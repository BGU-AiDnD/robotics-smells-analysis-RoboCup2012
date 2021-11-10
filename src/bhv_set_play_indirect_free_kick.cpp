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

#include "bhv_set_play_indirect_free_kick.h"

#include "bhv_set_play.h"
#include "bhv_prepare_set_play_kick.h"
#include "bhv_go_to_static_ball.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/body_kick_collide_with_ball.h>
#include "bhv_pass_test.h"
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/say_message_builder.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/circle_2d.h>
#include <rcsc/math_util.h>

bool Bhv_SetPlayIndirectFreeKick::S_kicker_canceled = false;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayIndirectFreeKick::execute( rcsc::PlayerAgent * agent )
{
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Bhv_SetPlayIndirectFreeKick" );
    //---------------------------------------------------
    if ( isKicker( agent ) )
    {
        doKick( agent );
    }
    else
    {
        doMove( agent );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayIndirectFreeKick::isKicker( const rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();

    if ( ( wm.gameMode().type() == rcsc::GameMode::BackPass_
           && wm.gameMode().side() == wm.ourSide() )
         || ( wm.gameMode().type() == rcsc::GameMode::IndFreeKick_
              && wm.gameMode().side() == wm.theirSide() )
         )
    {
        S_kicker_canceled = true;
        return false;
    }

    if ( wm.setplayCount() < 5 )
    {
        if ( wm.time().cycle() < wm.lastSetPlayStartTime().cycle() + 5 )
        {
            S_kicker_canceled = false;
        }
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": isKicker() wait period" );
        return false;
    }

    if ( S_kicker_canceled )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__":  isKicker() kicker canceled" );
        return false;
    }

    const rcsc::PlayerObject * nearest_mate = ( wm.teammatesFromBall().empty()
                                                ? static_cast< rcsc::PlayerObject * >( 0 )
                                                : wm.teammatesFromBall().front() );

    if ( ! nearest_mate )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__":  no nearest teammate" );
        return true;
    }

    if ( nearest_mate->distFromBall() < wm.ball().distFromSelf() * 0.85 )
    {
        return false;
    }

    if ( nearest_mate->distFromBall() < 3.0
         && wm.ball().distFromSelf() < 3.0
         && nearest_mate->distFromBall() < wm.ball().distFromSelf() )
    {
        S_kicker_canceled = true;

        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__":  isKicker() kicker canceled" );
        return false;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SetPlayIndirectFreeKick::doKick( rcsc::PlayerAgent * agent )
{
    static int s_scan_count = -5;

    // go to ball
    if ( Bhv_GoToStaticBall( 0.0 ).execute( agent ) )
    {
        return;
    }

    // already ball point

    const rcsc::WorldModel & wm = agent->world();

    const rcsc::Vector2D face_point( 50.0, 0.0 );
    const rcsc::AngleDeg face_angle = ( face_point - wm.self().pos() ).th();

    if ( wm.time().stopped() > 0 )
    {
        rcsc::Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
        return;
    }

    if ( ( face_angle - wm.self().body() ).abs() > 5.0 )
    {
        rcsc::Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
        return;
    }

    if ( s_scan_count < 0 )
    {
        ++s_scan_count;
        rcsc::Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
        return;
    }

    if ( s_scan_count < 10
         && wm.teammatesFromSelf().empty() )
    {
        ++s_scan_count;
        rcsc::Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
        return;
    }

    if ( wm.time().stopped() != 0 )
    {
        rcsc::Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
        return;
    }

    s_scan_count = -5;


    const double max_kick_speed = wm.self().kickRate() * rcsc::ServerParam::i().maxPower();

    rcsc::Vector2D target_point;
    double ball_speed = 0.0;

    const rcsc::Bhv_PassTest::PassRoute * pass
        = rcsc::Bhv_PassTest::get_best_pass( wm );
    if ( pass
         && pass->receive_point_.x > 35.0
         && pass->receive_point_.x > wm.self().pos().x - 1.0
         )
    {
        target_point = pass->receive_point_;
        ball_speed = std::min( pass->first_speed_, max_kick_speed );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__":  pass to (%.1f %.1f) speed=%.2f",
                            target_point.x, target_point.y,
                            ball_speed );
    }
    else if ( wm.teammatesFromSelf().empty()
              || wm.teammatesFromSelf().front()->distFromSelf() > 35.0
              || wm.teammatesFromSelf().front()->pos().x < -30.0 )
    {
        target_point
            = rcsc::Vector2D( rcsc::ServerParam::i().pitchHalfLength(),
                              static_cast< double >( -1 + 2 * wm.time().cycle() % 2 )
                              * ( rcsc::ServerParam::i().goalHalfWidth() - 0.8 ) );
        ball_speed = max_kick_speed;
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__":  kick to goal (%.1f %.1f) speed=%.2f",
                            target_point.x, target_point.y,
                            ball_speed );
    }
    else
    {
        double max_x = -100.0;
        const rcsc::PlayerObject * receiver = static_cast< const rcsc::PlayerObject * >( 0 );

        const rcsc::PlayerPtrCont::const_iterator end = wm.teammatesFromBall().end();
        for ( rcsc::PlayerPtrCont::const_iterator t = wm.teammatesFromBall().begin();
              t != end;
              ++t )
        {
            if ( (*t)->posCount() > 5 ) continue;
            if ( (*t)->distFromBall() > 20.0 ) continue;

            if ( (*t)->pos().x > max_x
                 && (*t)->pos().x < wm.offsideLineX() )
            {
                max_x = (*t)->pos().x;
                receiver = (*t);
            }
        }

        double target_dist = 10.0;
        if ( ! receiver )
        {
            target_dist = wm.teammatesFromSelf().front()->distFromSelf();
            target_point = wm.teammatesFromSelf().front()->pos();
        }
        else
        {
            target_dist = receiver->distFromSelf();
            target_point = receiver->pos();
            target_point.x += 0.6;
        }
        ball_speed = rcsc::calc_first_term_geom_series_last( 1.8, // end speed
                                                             target_dist,
                                                             rcsc::ServerParam::i().ballDecay() );
        ball_speed = std::min( ball_speed, max_kick_speed );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__":  pass to nearest teammate (%.1f %.1f) speed=%.2f",
                            target_point.x, target_point.y,
                            ball_speed );
    }

    agent->debugClient().setTarget( target_point );

    rcsc::Body_KickOneStep( target_point, ball_speed ).execute( agent );

    agent->setNeckAction( new rcsc::Neck_ScanField() );
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

    const double circle_r = world.gameMode().type() == rcsc::GameMode::BackPass_
        ? rcsc::ServerParam::i().goalAreaLength() + 0.5
        : rcsc::ServerParam::i().centerCircleR() + 0.5;

    if ( ( world.self().pos().x > -rcsc::ServerParam::i().pitchHalfLength()
           || world.self().pos().absY() > rcsc::ServerParam::i().goalHalfWidth()
           || world.ball().distFromSelf() < world.self().pos().dist( point ) )
         && ( rcsc::Line2D( world.self().pos(), point ).dist2( world.ball().pos() )
              < circle_r * circle_r )
         && ( ( world.ball().pos() - point ).th() - ( world.self().pos() - point ).th() ).abs()
         < 90.0
         && ( world.ball().angleFromSelf() - ( point - world.self().pos() ).th() ).abs()
         < 90.0
         )
    {
        rcsc::Vector2D new_point = world.ball().pos();
        rcsc::AngleDeg self2target = ( point - world.self().pos() ).th();
        if ( world.ball().angleFromSelf().isLeftOf( self2target ) )
        {
            new_point += rcsc::Vector2D::polar2vector( 10.5,
                                                       self2target + 90.0 );
        }
        else
        {
            new_point += rcsc::Vector2D::polar2vector( 10.5,
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

 */
void
Bhv_SetPlayIndirectFreeKick::doMove( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();

    const bool our_kick = ( ( wm.gameMode().type() == rcsc::GameMode::BackPass_
                              && wm.gameMode().side() == wm.theirSide() )
                            || ( wm.gameMode().type() == rcsc::GameMode::IndFreeKick_
                                 && wm.gameMode().side() == wm.ourSide() )
                            );

    rcsc::Vector2D target_point = M_home_pos;
    if ( our_kick )
    {
        target_point.x = std::min( wm.offsideLineX() - 1.0, target_point.x );

        double nearest_dist = 1000.0;
        const rcsc::PlayerObject * teammate = wm.getTeammateNearestTo( target_point, 10, &nearest_dist );
        if ( nearest_dist < 2.5 )
        {
            target_point += ( target_point - teammate->pos() ).setLengthVector( 2.5 );
            target_point.x = std::min( wm.offsideLineX() - 1.0, target_point.x );
        }
    }
    else
    {
        target_point = get_avoid_circle_point( wm, M_home_pos, 0 );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__":  their kick adjust target to (%.1f %.1f)->(%.1f %.1f) ",
                            M_home_pos.x, M_home_pos.y,
                            target_point.x, target_point.y );
    }

    double dash_power = wm.self().getSafetyDashPower( rcsc::ServerParam::i().maxPower() );

    double dist_thr = wm.ball().distFromSelf() * 0.07;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().addMessage( "IndFKMove" );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( ! rcsc::Body_GoToPoint( target_point,
                                 dist_thr,
                                 dash_power
                                 ).execute( agent ) )
    {
        // already there
        if ( our_kick )
        {
            rcsc::Vector2D turn_point( rcsc::ServerParam::i().pitchHalfLength(), 0.0 );
            rcsc::AngleDeg angle = ( turn_point - wm.self().pos() ).th();
            if ( angle.abs() > 100.0 )
            {
                turn_point = rcsc::Vector2D::polar2vector( 10.0, angle + 100.0 );
                if ( turn_point.x < 0.0 )
                {
                    turn_point.rotate( 180.0 );
                }
                turn_point += wm.self().pos();
            }

            rcsc::Body_TurnToPoint( turn_point ).execute( agent );
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__":  our kick. turn to (%.1f %.1f)",
                                turn_point.x, turn_point.y );
        }
        else
        {
            rcsc::Vector2D turn_point = rcsc::Vector2D::polar2vector( 10.0, wm.ball().angleFromSelf() + 90.0 );
            if ( turn_point.x < 0.0 )
            {
                turn_point.rotate( 180.0 );
            }
            turn_point += wm.self().pos();
            rcsc::Body_TurnToPoint( turn_point ).execute( agent );
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__":  their kick. turn to (%.1f %.1f)",
                                turn_point.x, turn_point.y );
        }
        S_kicker_canceled = false;
    }

    if ( our_kick
         && ! S_kicker_canceled
         && ( wm.self().pos().dist( target_point )
              > std::max( wm.ball().pos().dist( target_point ) * 0.2, dist_thr ) + 6.0 )
         )
    {
        agent->debugClient().addMessage( "Sayw" );
        agent->addSayMessage( new rcsc::WaitRequestMessage() );
    }

    if ( our_kick )
    {
        agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
    }
    else
    {
        agent->setNeckAction( new rcsc::Neck_TurnToBall() );
    }
}
