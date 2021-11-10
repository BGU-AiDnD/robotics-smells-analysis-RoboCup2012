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

#include "bhv_set_play_kick_off.h"

#include "bhv_go_to_static_ball.h"
#include "bhv_set_play.h"
#include "bhv_prepare_set_play_kick.h"

#include "body_smart_kick.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/neck_scan_field.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/math_util.h>

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_SetPlayKickOff::execute( rcsc::PlayerAgent * agent )
{
    static int S_scan_count = -5;

    const rcsc::WorldModel & wm = agent->world();

    ///////////////////////////////////////////////////////
    // no kicker -> not the nearest player to ball
    if ( ! wm.teammatesFromBall().empty()
         && ( wm.teammatesFromBall().front()->distFromBall()
              < wm.ball().distFromSelf() ) )
    {
        rcsc::Vector2D target_pos = M_home_pos;
        target_pos.x = std::min( -0.5, target_pos.x );

        double dash_power
            = Bhv_SetPlay::get_set_play_dash_power( agent );
        double dist_thr = wm.ball().distFromSelf() * 0.07;
        if ( dist_thr < 1.0 ) dist_thr = 1.0;

        if ( ! rcsc::Body_GoToPoint( target_pos,
                                     dist_thr,
                                     dash_power
                                     ).execute( agent ) )
        {
            // already there
            rcsc::Body_TurnToBall().execute( agent );
        }
        agent->setNeckAction( new rcsc::Neck_ScanField() );
    }
    ///////////////////////////////////////////////////////
    else
    {
        // kickaer

        // go to ball
        if ( ! Bhv_GoToStaticBall( 0.0 ).execute( agent ) )
        {

            // already ball point

            if ( wm.self().body().abs() < 175.0 )
            {
                rcsc::Body_TurnToAngle( 180.0 ).execute( agent );
                agent->setNeckAction( new rcsc::Neck_ScanField() );
                return true;
            }

            if ( S_scan_count < 0 )
            {
                S_scan_count++;
                rcsc::Body_TurnToAngle( 180.0 ).execute( agent );
                agent->setNeckAction( new rcsc::Neck_ScanField() );
                return true;
            }

            if ( S_scan_count < 10
                 && wm.teammatesFromSelf().empty() )
            {
                S_scan_count++;
                rcsc::Body_TurnToAngle( 180.0 ).execute( agent );
                agent->setNeckAction( new rcsc::Neck_ScanField() );
                return true;
            }

            S_scan_count = -5;


            rcsc::Vector2D target_point;
            double ball_speed
                = rcsc::ServerParam::i().maxPower()
                * wm.self().kickRate() - 1.0e-5;

            // teammate not found
            if ( wm.teammatesFromSelf().empty() )
            {
                target_point.assign( rcsc::ServerParam::i().pitchHalfLength(),
                                     static_cast< double >
                                     ( -1 + 2 * wm.time().cycle() % 2 )
                                     * 0.8 * rcsc::ServerParam::i().goalHalfWidth() );
                rcsc::dlog.addText( rcsc::Logger::TEAM,
                                    __FILE__": no teammate. target=(%.1f %.1f)",
                                    target_point.x, target_point.y );
            }
            else
            {
                const rcsc::PlayerObject * teammate = wm.teammatesFromSelf().front();
                double dist = teammate->distFromSelf();
                // too far
                if ( dist > 35.0 )
                {
                    target_point.assign( rcsc::ServerParam::i().pitchHalfLength(),
                                         static_cast< double >
                                         ( -1 + 2 * wm.time().cycle() % 2 )
                                         * 0.8 * rcsc::ServerParam::i().goalHalfWidth() );
                    rcsc::dlog.addText( rcsc::Logger::TEAM,
                                        __FILE__": nearest teammate is too far. target=(%.1f %.1f)",
                                        target_point.x, target_point.y );
                }
                else
                {
                    target_point = teammate->pos();
                    ball_speed = std::min( ball_speed,
                                           rcsc::calc_first_term_geom_series_last( 1.8,
                                                                                   dist,
                                                                                   rcsc::ServerParam::i().ballDecay() ) );
                    rcsc::dlog.addText( rcsc::Logger::TEAM,
                                        __FILE__": nearest teammate %d target=(%.1f %.1f) speed=%.3f",
                                        teammate->unum(),
                                        target_point.x, target_point.y,
                                        ball_speed );
                }
            }

            rcsc::Vector2D ball_vel = rcsc::Vector2D::polar2vector( ball_speed,
                                                                    ( target_point - wm.ball().pos() ).th() );
            rcsc::Vector2D ball_next = wm.ball().pos() + ball_next;
            while ( wm.self().pos().dist( ball_next ) < wm.self().playerType().kickableArea() + 0.2 )
            {
                ball_vel.setLength( ball_speed + 0.1 );
                ball_speed += 0.1;
                ball_next = wm.ball().pos() + ball_vel;

                rcsc::dlog.addText( rcsc::Logger::TEAM,
                                        __FILE__": kickable in next cycle. adjust ball speed to %.3f",
                                        ball_speed );
            }

            ball_speed = std::min( ball_speed,
                                   rcsc::ServerParam::i().maxPower() * wm.self().kickRate() );

            // enforce one step kick
            rcsc::Body_SmartKick( target_point,
                                  ball_speed,
                                  ball_speed * 0.96,
                                  1 ).execute( agent );
            agent->setNeckAction( new rcsc::Neck_ScanField() );
            agent->debugClient().setTarget( target_point );
        }
    }

    return true;
}
