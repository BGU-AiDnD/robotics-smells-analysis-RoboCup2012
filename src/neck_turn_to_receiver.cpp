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

#include "neck_turn_to_receiver.h"

#include "bhv_pass_test.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>

#include <rcsc/player/player_agent.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

/*-------------------------------------------------------------------*/
/*!

*/
bool
Neck_TurnToReceiver::execute( rcsc::PlayerAgent * agent )
{
    if ( agent->effector().queuedNextBallKickable() )
    {
        if ( executeImpl( agent ) )
        {

        }
        else if ( agent->world().self().pos().x > 35.0
                  || agent->world().self().pos().absY() > 20.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": Neck_TurnToReceiver. next kickable."
                                " attack or side area. scan field" );
            rcsc::Neck_ScanField().execute( agent );;
        }
        else
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": Neck_TurnToReceiver. next kickable. look low conf teammate" );
            rcsc::Neck_TurnToLowConfTeammate().execute( agent );
        }
    }
    else
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": Neck_TurnToReceiver. look ball" );
        rcsc::Neck_TurnToBall().execute( agent );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Neck_TurnToReceiver::executeImpl( rcsc::PlayerAgent * agent )
{
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Neck_TurnToReceiver" );

    const rcsc::WorldModel & wm = agent->world();
    if ( ! wm.self().isKickable() )
    {
        return false;
    }

    const rcsc::Bhv_PassTest::PassRoute * pass = rcsc::Bhv_PassTest::get_best_pass( wm );
    if ( ! pass )
    {
        return false;
    }

#if 0
    if ( pass->receiver_->posCount() == 0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": Neck_TurnToReceiver. current seen." );
        return false;
    }

    rcsc::Vector2D face_point = pass->receiver_->pos();
    if ( pass->receiver_->distFromBall() > 5.0
         && pass->receiver_->pos().x < 35.0 )
    {
        face_point.x += 3.0;
    }

    const rcsc::Vector2D next_pos = agent->effector().queuedNextMyPos();
    const rcsc::AngleDeg next_body = agent->effector().queuedNextMyBody();
    const double next_view_width = agent->effector().queuedNextViewWidth().width() * 0.5;

    rcsc::AngleDeg rel_angle = ( face_point - next_pos ).th() - next_body;

    if ( rel_angle.abs() > rcsc::ServerParam::i().maxNeckAngle() + next_view_width - 5.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": Neck_TurnToReceiver.cannot face to (%.1f %.1f)",
                            face_point.x, face_point.y );
        return false;
    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Neck_TurnToReceiver.(%.1f %.1f)",
                        face_point.x, face_point.y );
    rcsc::Neck_TurnToPoint( face_point ).execute( agent );;
    return true;

#else

    if ( ( pass->type_ == rcsc::Bhv_PassTest::DIRECT
           || pass->type_ == rcsc::Bhv_PassTest::LEAD )
         && pass->receiver_->posCount() == 0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": Neck_TurnToReceiver. current seen." );
        return false;
    }

    rcsc::Vector2D receiver_pos = pass->receiver_->pos() + pass->receiver_->vel();
    if ( pass->receiver_->distFromBall() > 5.0
         && pass->receiver_->pos().x < 35.0 )
    {
        receiver_pos.x += 3.0;
    }

    rcsc::Vector2D face_point = ( receiver_pos + pass->receive_point_ ) * 0.5;

    if ( pass->receiver_->posCount() == 0 )
    {
        face_point = pass->receive_point_;
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": face to the receiver point." );
    }

    const rcsc::Vector2D next_pos = agent->effector().queuedNextMyPos();
    const rcsc::AngleDeg next_body = agent->effector().queuedNextMyBody();
    const double next_view_width = agent->effector().queuedNextViewWidth().width() * 0.5;

    rcsc::AngleDeg rel_angle = ( face_point - next_pos ).th() - next_body;

    if ( rel_angle.abs() > rcsc::ServerParam::i().maxNeckAngle() + next_view_width - 5.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": cannot face to (%.1f %.1f)",
                            face_point.x, face_point.y );
        return false;
    }

    rcsc::Neck_TurnToPoint( face_point ).execute( agent );;

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Neck_TurnToReceiver.(%.1f %.1f)",
                        face_point.x, face_point.y );
    agent->debugClient().addMessage( "NeckToReceiver" );
    return true;
#endif
}
