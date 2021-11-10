// -*-c++-*-

/*!
  \file body_go_to_point2008.cpp
  \brief run to specified point
*/

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

#include "body_go_to_point2008.h"

#include <rcsc/player/player_agent.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/math_util.h>
#include <rcsc/action/body_turn_to_angle.h>


namespace rcsc {

rcsc::GameTime Body_GoToPoint2008::S_cache_time;
rcsc::Vector2D Body_GoToPoint2008::S_cache_target_point;
double Body_GoToPoint2008::S_cache_max_error = 0.0;
double Body_GoToPoint2008::S_cache_max_power = 0.0;
bool Body_GoToPoint2008::S_cache_use_back_dash = true;
bool Body_GoToPoint2008::S_cache_force_back_dash = false;
bool Body_GoToPoint2008::S_cache_emergency_mode = false;
int Body_GoToPoint2008::S_turn_count = 0;

static
double
s_required_dash_power_to_reach_next_step( const rcsc::PlayerType & type,
                                          double dist );

/*-------------------------------------------------------------------*/
/*!

*/
Body_GoToPoint2008::Body_GoToPoint2008
                      ( const rcsc::Vector2D & target_point,
                        double max_error,
                        double max_power,
                        bool use_back_dash,
                        bool force_back_dash,
                        bool emergency_mode )
    : M_target_point( target_point )
    , M_max_error( max_error )
    , M_max_power( max_power )
    , M_use_back_dash( use_back_dash )
    , M_force_back_dash( force_back_dash )
    , M_emergency_mode( emergency_mode )
{
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Body_GoToPoint2008::execute( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();

    //
    // update cache
    //
    if ( wm.time().cycle() > S_cache_time.cycle() + 1
         || (S_cache_target_point - M_target_point).r() > rcsc::EPS
         || std::fabs( S_cache_max_error - M_max_error ) > rcsc::EPS
         || S_cache_use_back_dash != M_use_back_dash
         || std::fabs( S_cache_max_power - M_max_error ) > rcsc::EPS
         || S_cache_force_back_dash != M_force_back_dash
         || S_cache_emergency_mode != M_emergency_mode )
    {
        S_turn_count = 0;
    }

    S_cache_time = wm.time();
    S_cache_target_point = M_target_point;
    S_cache_max_error = M_max_error;
    S_cache_use_back_dash = M_use_back_dash;
    S_cache_max_power = M_max_power;
    S_cache_force_back_dash = M_force_back_dash;
    S_cache_emergency_mode = M_emergency_mode;


    rcsc::dlog.addText( rcsc::Logger::ACTION,
                        __FILE__ ": target_point = [%f, %f]"
                        ", max_position_error = %f, emergency_mode = %s",
                        M_target_point.x, M_target_point.y,
                        M_max_error,
                        ( M_emergency_mode ? "true" : "false" ) );

    rcsc::dlog.addText( rcsc::Logger::ACTION,
                        __FILE__": self pos [%-3.4f, %-3.4f](%-3.4f)",
                        wm.self().pos().x,
                        wm.self().pos().y,
                        wm.self().body().degree() );


    // already there
    if ( (M_target_point - wm.self().pos()).r() <= M_max_error )
    {
        rcsc::dlog.addText( rcsc::Logger::ACTION,
                            __FILE__ ": no need to do" );

        return false;
    }


    rcsc::Vector2D next_self_pos = wm.self().inertiaPoint( 1 );

    rcsc::Vector2D vec = M_target_point - next_self_pos;

    rcsc::dlog.addText( rcsc::Logger::ACTION,
                        __FILE__": next self pos "
                        "[% -3.4f, %-3.4f](%-3.4f)",
                        next_self_pos.x,
                        next_self_pos.y,
                        wm.self().body().degree() );


    rcsc::AngleDeg angle_diff = vec.th() - wm.self().body();

    double v_r = vec.r();
    if ( v_r < 0.01 )
    {
        v_r = 0.01;
    }

    double turn_limit;
    {
        double a = std::fabs( M_max_error / v_r );

        if ( a > 1.0 )
        {
            a = 1.0;
        }

        turn_limit = rcsc::AngleDeg::rad2deg( std::asin( a ) );

        double minimum_turn_limit;

        if ( v_r <= 10.0 )
        {
            minimum_turn_limit = 0.0;
        }
        else if ( v_r < 30.0 )
        {
            minimum_turn_limit = (v_r - 10.0) * (5.0 / 20.0);
        }
        else
        {
            minimum_turn_limit = 5.0;
        }

        if ( turn_limit < minimum_turn_limit )
        {
            turn_limit = minimum_turn_limit;
        }
    }

    rcsc::dlog.addText( rcsc::Logger::ACTION,
                        __FILE__": angle_diff = %f, turn_limit = %f",
                        angle_diff.degree(), turn_limit );


    bool back_dash = false;
    //
    // Back Dash
    //
    {
        double back_dash_dist = 10.0;
        if ( M_emergency_mode )
        {
            back_dash_dist = 15.0;
        }


        bool back_dash_stamina_ok
             = (M_emergency_mode
                || (wm.self().stamina()
                    > rcsc::ServerParam::i().staminaMax() / 2.0
                    && wm.self().stamina()
                    > rcsc::ServerParam::i().recoverDecThrValue() + 500.0 ));

        rcsc::dlog.addText( rcsc::Logger::ACTION,
                            __FILE__": back dash stamina ok = %s",
                            ( back_dash_stamina_ok ? "true" : "false" ) );


        rcsc::AngleDeg back_angle_diff
            = rcsc::AngleDeg( 180.0 + angle_diff.degree() ).abs();

        rcsc::dlog.addText( rcsc::Logger::ACTION,
                            __FILE__": back angle_diff = %f",
                            back_angle_diff.degree() );
        rcsc::dlog.addText( rcsc::Logger::ACTION,
                            __FILE__": v_r = %f, back_dash_dist = %f",
                            v_r, back_dash_dist );

        if ( ((M_use_back_dash && v_r <= back_dash_dist) || M_force_back_dash)
             && rcsc::AngleDeg( 180.0 + angle_diff.degree() ).abs() <= turn_limit
             && back_dash_stamina_ok )
        {
            rcsc::dlog.addText( rcsc::Logger::ACTION,
                                __FILE__ ": back_dash" );

            // No Turn
            back_dash = true;
        }
    }


    //
    // Turn
    //
    if ( (! back_dash
          || angle_diff.abs() <= 90.0)
         && (S_turn_count <= 3
             && ! (S_turn_count >= 1
                   && angle_diff.abs() <= 30.0 )) )
    {
        if ( angle_diff.abs() > turn_limit )
        {
            rcsc::dlog.addText( rcsc::Logger::ACTION,
                                __FILE__ ": TURN" );

            S_turn_count ++;

            if ( rcsc::Body_TurnToAngle
                 ( (M_target_point - next_self_pos).th() ).execute( agent ) )
            {
                return true;
            }
        }
    }
    // back dash turn
    if ( back_dash
         && (S_turn_count <= 3
             && ! (S_turn_count >= 1
                   && (angle_diff + 180.0).abs() <= 30.0 )) )
    {
        if ( (angle_diff + 180.0).abs() > turn_limit )
        {
            rcsc::dlog.addText( rcsc::Logger::ACTION,
                                __FILE__ ": BACK_TURN" );

            agent->debugClient().addLine( wm.self().pos()
                                          + rcsc::Vector2D( -5.0, -5.0 ),
                                          wm.self().pos()
                                          + rcsc::Vector2D( +5.0, +5.0 ) );
            agent->debugClient().addLine( wm.self().pos()
                                          + rcsc::Vector2D( -5.0, +5.0 ),
                                          wm.self().pos()
                                          + rcsc::Vector2D( +5.0, -5.0 ) );

            S_turn_count ++;

            if ( rcsc::Body_TurnToAngle
                 ( (M_target_point - next_self_pos).th().degree() + 180.0 )
                 .execute( agent ) )
            {
                return true;
            }
        }
    }


    //
    // Dash
    //
    rcsc::dlog.addText( rcsc::Logger::ACTION,
                        __FILE__ ": DASH" );

    double next_turn_distance = v_r * (vec.th() - wm.self().body()).cos();

    double dist_power_ceil
           = s_required_dash_power_to_reach_next_step( wm.self().playerType(),
                                                       next_turn_distance );

    return agent->doDash( wm.self().getSafetyDashPower
                          ( std::min( M_max_power, dist_power_ceil ) ) );
}


static
double
s_required_dash_power_to_reach_next_step( const rcsc::PlayerType & type,
                                          double dist )
{
    const double needed_power = dist / type.dashPowerRate();
    const double max_power = rcsc::ServerParam::i().maxPower();
    const double min_power = rcsc::ServerParam::i().minPower();

    if ( needed_power >= max_power )
    {
        return max_power;
    }
    else if ( needed_power <= min_power )
    {
        return min_power;
    }
    else
    {
        return needed_power;
    }
}

}
