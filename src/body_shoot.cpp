// -*-c++-*-

/*!
  \file body_shoot.cpp
  \brief advanced shoot planning and behavior.
*/

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "body_shoot.h"

#include <rcsc/action/body_hold_ball.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/body_stop_ball.h>

#include <rcsc/player/interception.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/rect_2d.h>
#include <rcsc/soccer_math.h>
#include <rcsc/math_util.h>

namespace rcsc {

ShootTable2008 Body_Shoot::S_shoot_table;

/*-------------------------------------------------------------------*/
/*!

*/
bool
Body_Shoot::execute( PlayerAgent * agent )
{
    if ( ! agent->world().self().isKickable() )
    {
        std::cerr << __FILE__ << ": " << __LINE__
                  << " not ball kickable!"
                  << std::endl;
        dlog.addText( Logger::ACTION,
                      "%s:%d:  not kickable"
                      ,__FILE__, __LINE__ );
        return false;
    }

    const ShootTable2008::ShotCont & shots = S_shoot_table.getShots( agent );

    // update
    if ( shots.empty() )
    {
        dlog.addText( Logger::SHOOT,
                      "%s:%d: not found shoot route"
                      ,__FILE__, __LINE__ );
        return false;
    }


    ShootTable2008::ShotCont::const_iterator shot
        = std::min_element( shots.begin(),
                            shots.end(),
                            ShootTable2008::ScoreCmp() );

    if ( shot == shots.end() )
    {
        dlog.addText( Logger::SHOOT,
                      "%s:%d: not found best shot"
                      ,__FILE__, __LINE__ );
        return false;
    }

    // it is necessary to evaluate shoot courses

    Vector2D target_point = shot->point_;
    double first_speed = shot->speed_;

    agent->debugClient().addMessage( "Shoot" );
    agent->debugClient().setTarget( target_point );

    Vector2D one_step_vel
        = Body_KickOneStep::get_max_possible_vel( ( target_point - agent->world().ball().pos() ).th(),
                                                  agent->world().self().kickRate(),
                                                  agent->world().ball().vel() );
    double one_step_speed = one_step_vel.r();

    dlog.addText( Logger::SHOOT,
                  "%s:%d: shoot to (%.2f, %.2f) speed=%.2f one_kick_max_speed=%.2f"
                  ,__FILE__, __LINE__,
                  target_point.x, target_point.y,
                  first_speed,
                  one_step_speed );

    Body_KickOneStep( target_point,
                          ServerParam::i().ballSpeedMax()
                          ).execute( agent );
        agent->debugClient().addMessage( "Enforce1Step" );
        return true;

    bool enforce_kick = false;
    if ( agent->world().self().pos().x > 45.0
         && ( agent->world().self().pos().absY()
              < ServerParam::i().goalHalfWidth() + 3.0 )
         )
    {
        enforce_kick = true;
    }

    /*
    agent->debugClient().addLine( agent->world().ball().pos(), target_point );
    agent->setIntention
        ( new IntentionKick( target_point,
                             first_speed,
                             2,
                             enforce_kick,
                             agent->world().time() ) );
    */
    return true;
}

}
