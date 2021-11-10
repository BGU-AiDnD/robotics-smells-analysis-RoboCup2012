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

#include "body_kick_to_front_space.h"

#include "body_smart_kick.h"

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/rect_2d.h>
#include <rcsc/math_util.h>

/*-------------------------------------------------------------------*/
/*!

*/
bool
Body_KickToFrontSpace::execute( rcsc::PlayerAgent * agent )
{
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": rcsc::Body_KickToFrontSpace" );

    const double space_width = 15.0;
    const int space_w_divs = 10;

    double first_speed = 1.8;
    rcsc::Vector2D center = agent->world().self().pos();
    center.x = std::min( center.x + 14.0, rcsc::ServerParam::i().pitchHalfLength() - 3.0 );
    center.y = rcsc::min_max( -rcsc::ServerParam::i().pitchHalfWidth() + 7.0,
                              center.y,
                              rcsc::ServerParam::i().pitchHalfWidth() - 7.0 );

    rcsc::Rect2D space( rcsc::Vector2D( center.x - 0.5, center.y - space_width * 0.5 ),
                        rcsc::Size2D( 1.0, space_width ) );
    /*
      Line2D my_body_line(Ray2D(agent->world().self().pos(),
      agent->world().self().body()));
      rcsc::Vector2D attract_point;
      if ( ! my_body_line.getIntersection(space.leftEdge(), attract_point) )
      {
      attract_point = center;
      }
    */
    rcsc::Vector2D target_point = center;

    double min_congestion = 10000.0;

    const rcsc::PlayerPtrCont & opps = agent->world().opponentsFromSelf();
    const rcsc::PlayerPtrCont::const_iterator opps_end = opps.end();
    const double y_mesh = space_width / static_cast< double >( space_w_divs );
    double addy = space.top() + y_mesh * 0.5;
    for ( int i = 0; i < space_w_divs; i++ )
    {
        rcsc::Vector2D point( center.x, addy );
        if ( point.x > rcsc::ServerParam::i().pitchHalfLength() - 3.0
             || std::fabs( addy ) > rcsc::ServerParam::i().pitchHalfWidth() - 3.0 )
        {
            continue;
        }
        double congestion = 0.0;
        for ( rcsc::PlayerPtrCont::const_iterator it = opps.begin();
              it != opps_end;
              ++it )
        {
            congestion += 1.0 / (*it)->pos().dist2( point );
        }

        if ( congestion < min_congestion )
        {
            min_congestion = congestion;
            target_point = point;
        }

        addy += y_mesh;
    }


    if ( target_point.x > 35.0 )
    {
        first_speed = 1.3;
    }

#ifdef USE_SMART_KICK
    rcsc::Body_SmartKick( target_point,
                          first_speed,
                          first_speed * 0.96,
                          2 ).execute( agent );
#else
    rcsc::Body_KickTwoStep( target_point,
                            first_speed,
                            true
                            ).execute( agent ); // enforce
#endif

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": target=(%.1f, %.1f)",
                        target_point.x, target_point.y );
    agent->debugClient().addMessage( "kick to front space" );
    agent->debugClient().setTarget( target_point );

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": register kick to front space intention" );
    return true;
}
