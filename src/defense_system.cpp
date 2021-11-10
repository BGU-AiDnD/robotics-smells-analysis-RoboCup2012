// -*-c++-*-

/*!
  \file defense_system.cpp
  \brief defense related utilities Source File
*/

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
#include "config.h"
#endif

#include "defense_system.h"

#include <rcsc/common/server_param.h>
#include <rcsc/common/player_type.h>
#include <rcsc/player/world_model.h>


using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
int
DefenseSystem::predict_self_reach_cycle( const WorldModel & wm,
                                         const Vector2D & target_point,
                                         const double & dist_thr,
                                         const bool save_recovery,
                                         double * stamina )
{
    const ServerParam & param = ServerParam::i();
    const PlayerType & self_type = wm.self().playerType();
    const double max_moment = param.maxMoment();
    const double recover_dec_thr = param.staminaMax() * param.recoverDecThr();

    const double first_my_speed = wm.self().vel().r();

    for ( int cycle = 0; cycle < 25; ++cycle )
    {
        Vector2D my_pos = wm.self().inertiaPoint( cycle );
        double target_dist = ( target_point - my_pos ).r();
        if ( target_dist - dist_thr > self_type.realSpeedMax() * cycle )
        {
            continue;
        }

        int n_turn = 0;
        int n_dash = 0;

        AngleDeg target_angle = ( target_point - my_pos ).th();
        double my_speed = first_my_speed;

        //
        // turn
        //
        double angle_diff = ( target_angle - wm.self().body() ).abs();
        double turn_margin = 180.0;
        if ( dist_thr < target_dist )
        {
            turn_margin = std::max( 15.0,
                                    AngleDeg::asin_deg( dist_thr / target_dist ) );
        }

        while ( angle_diff > turn_margin )
        {
            angle_diff -= self_type.effectiveTurn( max_moment, my_speed );
            my_speed *= self_type.playerDecay();
            ++n_turn;
        }

        double my_stamina = wm.self().stamina();
        double my_effort = wm.self().effort();
        double my_recovery = wm.self().recovery();

        AngleDeg dash_angle = wm.self().body();
        if ( n_turn > 0 )
        {
            angle_diff = std::max( 0.0, angle_diff );
            dash_angle = target_angle;
            if ( ( target_angle - wm.self().body() ).degree() > 0.0 )
            {
                dash_angle += angle_diff;
            }
            else
            {
                dash_angle += angle_diff;
            }

            self_type.getOneStepStaminaComsumption();
        }

        //
        // dash
        //

        Vector2D vel = wm.self().vel() * std::pow( self_type.playerDecay(), n_turn );
        while ( n_turn + n_dash < cycle
                && target_dist > dist_thr )
        {
            double dash_power = std::min( param.maxPower(), my_stamina );
            if ( save_recovery
                 && my_stamina - dash_power < recover_dec_thr )
            {
                dash_power = std::max( 0.0, my_stamina - recover_dec_thr );
            }

            Vector2D accel = Vector2D::polar2vector( dash_power
                                                     * self_type.dashPowerRate()
                                                     * my_effort,
                                                     dash_angle );
            vel += accel;

            double speed = vel.r();
            if ( speed > self_type.playerSpeedMax() )
            {
                vel *= self_type.playerSpeedMax() / speed;
            }

            my_pos += vel;
            vel *= self_type.playerDecay();

            self_type.getOneStepStaminaComsumption();

            target_dist = my_pos.dist( target_point );
            ++n_dash;
        }

        if ( target_dist <= dist_thr )
        {
            if ( stamina )
            {
                *stamina = my_stamina;
            }
            return n_turn + n_dash;
        }
    }

    return 1000;
}
