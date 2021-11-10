// -*-c++-*-

/*!
  \file bhv_find_player.cpp
  \brief search specified player by body and neck
*/

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA, Hidehisa AKIYAMA

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

#include "bhv_find_player.h"

#include <rcsc/action/bhv_scan_field.h>
#include <rcsc/action/body_turn_to_point.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_player_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/abstract_player_object.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/math_util.h>

#include <vector>
#include <algorithm>
#include <cmath>

namespace rcsc_ext {

/*-------------------------------------------------------------------*/
/*!

*/
Bhv_FindPlayer::Bhv_FindPlayer( const rcsc::AbstractPlayerObject * target_player,
                                int count_threshold )
    : M_target_player( target_player )
    , M_count_threshold( count_threshold )
{

}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_FindPlayer::execute( rcsc::PlayerAgent * agent )
{
    rcsc::dlog.addText( rcsc::Logger::ACTION,
                        __FILE__": Bhv_FindPlayer" );

    if ( ! M_target_player )
    {
        return false;
    }

    rcsc::dlog.addText( rcsc::Logger::ACTION,
                        __FILE__": target player side=%d unum=%d (%.1f %.1f) threshold=%d",
                        M_target_player->side(),
                        M_target_player->unum(),
                        M_target_player->pos().x, M_target_player->pos().y,
                        M_count_threshold );


    if ( M_target_player->seenPosCount() <= M_count_threshold
         && M_target_player->posCount() <= M_count_threshold )
    {
        rcsc::dlog.addText( rcsc::Logger::ACTION,
                            __FILE__": already found" );

        return false;
    }

    const rcsc::WorldModel & wm = agent->world();

    if ( M_target_player->isGhost() )
    {
        //return rcsc::Bhv_ScanField().execute( agent );
        rcsc::Vector2D face_point( rcsc::ServerParam::i().pitchHalfLength() - 7.0,
                                   wm.self().pos().y * 0.9 );
        face_point.x = face_point.x * 0.7 + wm.self().pos().x * 0.3;

        rcsc::Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );

        rcsc::dlog.addText( rcsc::Logger::ACTION,
                            __FILE__": target player is ghost." );
        agent->debugClient().addMessage( "FindPlayer:Ghost" );
        return true;
    }

    //
    // search the best angle
    //
    const rcsc::Vector2D next_self_pos = wm.self().pos() + wm.self().vel();
    const rcsc::Vector2D player_pos = M_target_player->pos() + M_target_player->vel();

    //
    // create candidate body target points
    //

    std::vector< rcsc::Vector2D > body_points;
    body_points.reserve( 16 );

    const double max_x = rcsc::ServerParam::i().pitchHalfLength() - 7.0;
    const double y_step = player_pos.absY() / 5.0;
    const double max_y = player_pos.absY() + y_step - 0.001;
    const double y_sign = rcsc::sign( player_pos.y );

    // current body dir
    body_points.push_back( next_self_pos + rcsc::Vector2D::polar2vector( 10.0, wm.self().body() ) );

    // on the static x line
    for ( double y = 0.0; y < max_y; y += y_step )
    {
        body_points.push_back( rcsc::Vector2D( max_x, y * y_sign ) );
    }

    // on the static y line
    if ( next_self_pos.x < max_x )
    {
        for ( double x_rate = 0.9; x_rate >= 0.0; x_rate -= 0.1 )
        {
            double x = std::min( max_x,
                                 max_x * x_rate + next_self_pos.x * ( 1.0 - x_rate ) );
            body_points.push_back( rcsc::Vector2D( x, player_pos.y ) );
        }
    }
    else
    {
        body_points.push_back( rcsc::Vector2D( max_x, player_pos.y ) );
    }

    //
    // evaluate candidate points
    //

    const double next_view_half_width = agent->effector().queuedNextViewWidth().width() * 0.5;
    const double max_turn = wm.self().playerType().effectiveTurn( rcsc::ServerParam::i().maxMoment(),
                                                                  wm.self().vel().r() );
    const rcsc::AngleDeg player_angle = ( player_pos - next_self_pos ).th();
    const double neck_min = rcsc::ServerParam::i().minNeckAngle() - next_view_half_width + 10.0;
    const double neck_max = rcsc::ServerParam::i().maxNeckAngle() + next_view_half_width - 10.0;

    rcsc::Vector2D best_point = rcsc::Vector2D::INVALIDATED;

    const std::vector< rcsc::Vector2D >::const_iterator p_end = body_points.end();
    for ( std::vector< rcsc::Vector2D >::const_iterator p = body_points.begin();
          p != p_end;
          ++p )
    {
        rcsc::AngleDeg target_body_angle = ( *p - next_self_pos ).th();
        double turn_moment_abs = ( target_body_angle - wm.self().body() ).abs();

        rcsc::dlog.addText( rcsc::Logger::ACTION,
                            "____ body_point=(%.1f %.1f) angle=%.1f moment=%.1f",
                            p->x, p->y,
                            target_body_angle.degree(),
                            turn_moment_abs );

        if ( turn_moment_abs > max_turn )
        {
            rcsc::dlog.addText( rcsc::Logger::ACTION,
                                "____ xxxx cannot turn by 1 step" );
            continue;
        }

        double angle_diff = ( player_angle - target_body_angle ).abs();
        if ( neck_min < angle_diff
             && angle_diff < neck_max )
        {
            best_point = *p;

            rcsc::dlog.addText( rcsc::Logger::ACTION,
                                "____ oooo can turn and look" );
            break;
        }

        rcsc::dlog.addText( rcsc::Logger::ACTION,
                                "____ xxxx cannot look" );
    }

    if ( ! best_point.isValid() )
    {
        best_point.assign( max_x * 0.7 + wm.self().pos().x * 0.3,
                           wm.self().pos().y * 0.9 );
    }


    rcsc::dlog.addText( rcsc::Logger::ACTION,
                        __FILE__": turn to (%.1f %.1f)",
                        best_point.x, best_point.y );
    rcsc::Body_TurnToPoint( best_point ).execute( agent );
    agent->debugClient().addLine( next_self_pos, best_point );

    agent->setNeckAction( new rcsc::Neck_TurnToPlayerOrScan( M_target_player, M_count_threshold ) );
    return true;
}

}
