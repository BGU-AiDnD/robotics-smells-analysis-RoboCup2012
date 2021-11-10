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

#include "bhv_basic_offensive_kick.h"

#include "body_kick_to_corner.h"

#include "bhv_pass_test.h"
#include "body_advance_ball_test.h"
#include "body_clear_ball2008.h"
#include "body_dribble2008.h"
#include "body_hold_ball2008.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/sector_2d.h>

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_BasicOffensiveKick::execute( rcsc::PlayerAgent * agent )
{
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Bhv_BasicOffensiveKick" );
    const rcsc::WorldModel & wm = agent->world();
    const rcsc::PlayerObject * nearest_opp = wm.getOpponentNearestToSelf( 5 );
    const double nearest_opp_dist = ( nearest_opp
                                      ? nearest_opp->distFromSelf()
                                      : 1000.0 );
    const rcsc::Vector2D nearest_opp_pos = ( nearest_opp
                                             ? nearest_opp->pos()
                                             : rcsc::Vector2D( -1000.0, 0.0 ) );

    bool exist_front_defender = false;
    {
        const rcsc::PlayerPtrCont::const_iterator end = wm.opponentsFromSelf().end();
        for ( rcsc::PlayerPtrCont::const_iterator o = wm.opponentsFromSelf().begin();
              o != end;
              ++o )
        {
            if ( (*o)->goalie() ) continue;
            if ( (*o)->isGhost() ) continue;
            if ( (*o)->posCount() > 10 ) continue;
            if ( (*o)->pos().x > wm.self().pos().x )
            {
                exist_front_defender = true;
                break;
            }
        }
    }

    const rcsc::Bhv_PassTest::PassRoute * pass
        = rcsc::Bhv_PassTest::get_best_pass( wm );

    if ( pass
         && ( exist_front_defender || wm.self().stamina() < 1400.0 )
         && ( pass->receive_point_.x > wm.self().pos().x //+ 5.0
              || ( nearest_opp_dist < 7.0 //4.0
                   && pass->receive_point_.x > wm.self().pos().x - 10.0 ) ) //- 1.0 ) )
         //&& ! wm.existOpponentIn( rcsc::Circle2D( pass_point, 6.0 ), 10, true )
         )

    {
        agent->debugClient().addMessage( "OffKickPass(1)" );
        rcsc::Bhv_PassTest().execute( agent );
        return true;
    }

    if ( pass
         && ( exist_front_defender || wm.self().stamina() < 1400.0 )
         && nearest_opp_dist < 7.0 )
    {
        agent->debugClient().addMessage( "OffKickPass(2)" );
        rcsc::Bhv_PassTest().execute( agent );
        return true;
    }

    if ( pass
         && nearest_opp_dist < 5.0
         && ( pass->receive_point_.x > wm.self().pos().x - 5.0
              || pass->receive_point_.x > 10.0 )
         )
    {
        agent->debugClient().addMessage( "OffKickPass(3)" );
        rcsc::Bhv_PassTest().execute( agent );
    }

    //----------------------------------------------

    if ( doDribbleAvoid( agent, nearest_opp ) )
    {
        return true;
    }

    //----------------------------------------------

    //rcsc::Vector2D drib_target( 50.0, wm.self().pos().absY() );
    rcsc::Vector2D drib_target( 47.0, wm.self().pos().absY() ); // 2007.06.28
    //if ( drib_target.y < 20.0 ) drib_target.y = 20.0;
    //if ( drib_target.y > 29.0 ) drib_target.y = 27.0;
    if ( drib_target.y > 20.0 ) drib_target.y = 20.0;
    else if ( drib_target.y > 10.0 ) drib_target.y = 10.0;
    if ( wm.self().pos().x > 25.0 ) drib_target.y = 5.0;

    if ( wm.self().pos().y < 0.0 ) drib_target.y *= -1.0;

    const rcsc::AngleDeg drib_angle = ( drib_target - wm.self().pos() ).th();

    // opponent is behind of me
    if ( nearest_opp_pos.x < wm.self().pos().x
         || ( nearest_opp_dist > 2.0
              && nearest_opp_pos.x < wm.self().pos().x + 1.0 )
         )
    {
        // check opponents
        // 15m, +-30 degree
        const rcsc::Sector2D sector( wm.self().pos(),
                                     0.5, 15.0,
                                     drib_angle - 30.0,
                                     drib_angle + 30.0 );
        // opponent check with goalie
        if ( ! wm.existOpponentIn( sector, 10, true ) )
        {
            const int max_dash_step
                = wm.self().playerType().cyclesToReachDistance( wm.self().pos().dist( drib_target ) );
            if ( wm.self().pos().x > 35.0 )
            {
                drib_target.y *= ( 10.0 / drib_target.absY() );
            }

            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": fast dribble to (%.1f, %.1f) max_step=%d",
                                drib_target.x, drib_target.y,
                                max_dash_step );
            agent->debugClient().addMessage( "OffKickDrib(2)" );
            rcsc::Body_Dribble2008( drib_target,
                                    1.0,
                                    rcsc::ServerParam::i().maxPower(),
                                    std::min( 5, max_dash_step )
                                    ).execute( agent );
        }
        else
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": slow dribble to (%.1f, %.1f)",
                                drib_target.x, drib_target.y );
            agent->debugClient().addMessage( "OffKickDrib(3)" );
            rcsc::Body_Dribble2008( drib_target,
                                    1.0,
                                    rcsc::ServerParam::i().maxPower(),
                                    2
                                    ).execute( agent );

        }
        agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
        return true;
    }

    // opp is far from me
    if ( nearest_opp_dist > 5.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": opp far. dribble to (%.1f, %.1f)",
                            drib_target.x, drib_target.y );
        agent->debugClient().addMessage( "OffKickDrib(4)" );
        rcsc::Body_Dribble2008( drib_target,
                                1.0,
                                rcsc::ServerParam::i().maxPower() * 0.6,
                                1
                                ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
        return true;
    }

    // opp is close

    // can pass
    if ( rcsc::Bhv_PassTest().execute( agent ) )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": OffKickPass(3)" );
        agent->debugClient().addMessage( "OffKickPass(3)" );
        return true;
    }

    if ( nearest_opp_dist > 2.5 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": hold dribble" );
        agent->debugClient().addMessage( "OffKickDrib(5)" );
        rcsc::Body_Dribble2008( drib_target,
                                1.0,
                                rcsc::ServerParam::i().maxPower(),
                                1
                                ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
        return true;
    }

    if ( wm.self().pos().x > 20.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": hold" );
        agent->debugClient().addMessage( "OffKickHold" );
        rcsc::Body_HoldBall2008( true, rcsc::Vector2D( 45.0, 0.0 ) ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
    }
    else if ( wm.self().pos().x > wm.offsideLineX() - 10.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": kick near side" );
        agent->debugClient().addMessage( "OffKickToCorner" );
        Body_KickToCorner( (wm.self().pos().y < 0.0) ).execute( agent);
        agent->setNeckAction( new rcsc::Neck_ScanField() );
    }
    else
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": advance" );
        agent->debugClient().addMessage( "OffKickAdvance" );
        Body_AdvanceBallTest().execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_BasicOffensiveKick::doDribbleAvoid( rcsc::PlayerAgent * agent,
                                        const rcsc::PlayerObject * nearest_opp )
{
    if ( ! nearest_opp )
    {
        return false;
    }

    if ( nearest_opp->distFromSelf() > 5.0 )
    {
        return false;
    }

    const rcsc::WorldModel & wm = agent->world();

    if ( wm.self().body().abs() > 70.0 )
    {
        return false;
    }

    // dribble to my body dir

    const rcsc::Vector2D body_dir_drib_target
        = wm.self().pos()
        + rcsc::Vector2D::polar2vector( 5.0, wm.self().body() );

    if ( body_dir_drib_target.x > rcsc::ServerParam::i().pitchHalfLength() - 1.0
         || body_dir_drib_target.absY() > rcsc::ServerParam::i().pitchHalfWidth() - 1.0  )
    {
        return false;
    }

    int max_dir_count = 0;
    wm.dirRangeCount( wm.self().body(), 20.0, &max_dir_count, NULL, NULL );
    if ( max_dir_count >= 4 )
    {
        return false;
    }

    // check opponents
    // 10m, +-30 degree
    const rcsc::Sector2D sector( wm.self().pos(),
                                 0.5, 10.0,
                                 wm.self().body() - 30.0,
                                 wm.self().body() + 30.0 );
    // opponent check with goalie
    if ( wm.existOpponentIn( sector, 10, true ) )
    {
        return false;
    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": doDribbleAvoid: dribble to my body dir" );
    agent->debugClient().addMessage( "OffKickDribAvoid" );

    rcsc::Body_Dribble2008( body_dir_drib_target,
                            1.0,
                            rcsc::ServerParam::i().maxPower(),
                            2
                            ).execute( agent );

    agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
    return true;
}
