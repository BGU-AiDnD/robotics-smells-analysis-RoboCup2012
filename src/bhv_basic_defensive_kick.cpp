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

#include "bhv_basic_defensive_kick.h"

#include "body_advance_ball_test.h"
#include "body_clear_ball2008.h"
#include "bhv_pass_test.h"
#include "body_dribble2008.h"
#include "body_hold_ball2008.h"

#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/basic_actions.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_BasicDefensiveKick::execute( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();

    const rcsc::PlayerObject * nearest_opp = wm.getOpponentNearestToSelf( 7 );
    const double nearest_opp_dist = ( nearest_opp
                                      ? nearest_opp->distFromSelf()
                                      : 1000.0 );
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        "Bhv_BasicDefensiveKick. opp dist=%.2f",
                        nearest_opp_dist );

    const rcsc::Bhv_PassTest::PassRoute * pass
        = rcsc::Bhv_PassTest::get_best_pass( wm );

    if ( pass )
    {
        rcsc::Vector2D pass_point = pass->receive_point_;
        if ( pass_point.x > wm.self().pos().x + 20.0 )
        {
            if ( rcsc::Bhv_PassTest().execute( agent ) )
            {
                rcsc::dlog.addText( rcsc::Logger::TEAM,
                                    __FILE__": pass to front (%.2f %.2f )",
                                    pass_point.x, pass_point.y );
                agent->debugClient().addMessage( "DefPass1" );
                return true;
            }
        }

        if ( pass_point.x > wm.self().pos().x + 7.0 )
        {
            double opp_dist = 1000.0;
            const rcsc::PlayerObject * opp
                = wm.getOpponentNearestTo( pass_point,
                                           10,
                                           &opp_dist );
            if ( opp && opp_dist > 7.0 )
            {
                rcsc::dlog.addText( rcsc::Logger::TEAM,
                                    __FILE__": pass to front (%.2f %.2f)",
                                    pass_point.x, pass_point.y );
                agent->debugClient().addMessage( "DefPass2" );
                rcsc::Bhv_PassTest().execute( agent );
                return true;
            }
        }
    }

    if ( nearest_opp_dist > 12.0 )
    {
        rcsc::Vector2D target( 36.0, 0.0 );
        rcsc::Body_Dribble2008( target,
                                1.0,
                                rcsc::ServerParam::i().maxPower() * 0.7,
                                1,
                                false // never avoid
                                ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );

        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": dribble(1) to (%.2f %.2f)",
                            target.x, target.y );
        agent->debugClient().addMessage( "DefDrib1" );
        return true;
    }

    // safety area
    if ( nearest_opp_dist > 8.0
         && wm.self().pos().x > -30.0 )
    {
        rcsc::Vector2D target( 36.0, 0.0 );
        rcsc::Body_Dribble2008( target,
                                1.0,
                                rcsc::ServerParam::i().maxPower() * 0.7,
                                1,
                                false // never avoid
                                ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );

        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": dribble(2) to (%.2f, %.2f)",
                            target.x, target.y );
        agent->debugClient().addMessage( "DefDrib2" );
        return true;
    }

    // can pass
#if 1
   if ( rcsc::Bhv_PassTest().execute( agent ) )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": normal pass" );
        agent->debugClient().addMessage( "DefPassDefault" );
        return true;
    }
#else
   // 2008-07-08 shimora
   if ( pass )
   {
       rcsc::Vector2D pass_point = pass->receive_point_;
       if ( pass_point.x > wm.self().pos().x + 5.0 )
       {
           if ( rcsc::Bhv_PassTest().execute( agent ) )
           {
               rcsc::dlog.addText( rcsc::Logger::TEAM,
                                   __FILE__": normal pass" );
               agent->debugClient().addMessage( "DefPassDefault" );
               return true;
           }
       }
   }
#endif

    // dribble
    if ( nearest_opp_dist > 12.0
         && wm.self().pos().absY() > 20.0 )
    {
        rcsc::AngleDeg ang_l, ang_r;
        if ( wm.self().pos().y < 0.0 )
        {
            ang_l = ( rcsc::Vector2D(0.0, 0.0) - wm.self().pos() ).th();
            ang_r = 80.0;
        }
        else
        {
            ang_l = -80.0;
            ang_r = ( rcsc::Vector2D(0.0, 0.0) - wm.self().pos() ).th();
        }

        if ( ang_l.isLeftOf( wm.self().body() )
             && wm.self().body().isLeftOf( ang_r ) )
        {
            rcsc::Vector2D drib_target
                = wm.self().pos()
                + rcsc::Vector2D::polar2vector( 2.0, wm.self().body() );
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": dribble to body dir" );
            agent->debugClient().addMessage( "DefDrib3" );
            rcsc::Body_Dribble2008( drib_target,
                                    1.0,
                                    rcsc::ServerParam::i().maxPower(),
                                    1,
                                    false // never avoid
                                    ).execute( agent );
            agent->setNeckAction( new rcsc::Neck_ScanField() );
            return true;
        }

        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": opp too far. dribble to their side" );
        agent->debugClient().addMessage( "DefDrib4" );
        rcsc::Vector2D target( 36.0, 0.0 );
        rcsc::Body_Dribble2008( target,
                                1.0,
                                rcsc::ServerParam::i().maxPower(),
                                1,
                                false // never avoid
                                ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
        return true;
    }

    if ( nearest_opp_dist > 5.0
         || ( nearest_opp_dist > 3.0
              && nearest_opp
              && nearest_opp->pos().x < wm.ball().pos().x - 0.5 )
         )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": opp far. dribble to their side" );
        agent->debugClient().addMessage( "DefDrib5" );
        rcsc::Vector2D target( 36.0, wm.self().pos().y * 1.1 );
        if ( target.y > 30.0 ) target.y = 30.0;
        if ( target.y < -30.0 ) target.y = -30.0;
        rcsc::Body_Dribble2008( target,
                                1.0,
                                rcsc::ServerParam::i().maxPower() * 0.8,
                                1,
                                false // never avoid
                                ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
        return true;
    }

#if 1
    if ( nearest_opp_dist > 4.0 )
    {
        agent->debugClient().addMessage( "DefHold" );

        rcsc::Vector2D face_point( 100.0, 0.0 );
        rcsc::Body_HoldBall2008( true, face_point ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
        return true;
    }

    if ( wm.self().pos().x < -35.0
         && wm.self().pos().absY() < 16.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": Bhv_BasicDefensiveKick. clear" );
        agent->debugClient().addMessage( "DefClear" );
        Body_ClearBall2008().execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
        return true;
    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Bhv_BasicDefensiveKick. advance" );
    agent->debugClient().addMessage( "DefAdvance" );
    Body_AdvanceBallTest().execute( agent );
    agent->setNeckAction( new rcsc::Neck_ScanField() );
#else
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Bhv_BasicDefensiveKick. pass final" );
    agent->debugClient().addMessage( "DefPassFinal" );

    rcsc::Bhv_PassTest().execute( agent );
#endif

    return true;
}
