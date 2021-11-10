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

#include "bhv_keep_shoot_chance.h"

#include "bhv_cross.h"
#include "body_kick_to_corner.h"

#include "body_advance_ball_test.h"
#include "body_clear_ball_test.h"
#include "body_dribble2008.h"
#include "bhv_pass_test.h"
#include "body_hold_ball2008.h"

#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_goalie_or_scan.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>
#include <rcsc/action/basic_actions.h>


#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/sector_2d.h>

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_KeepShootChance::execute( rcsc::PlayerAgent * agent )
{
    static const double goalie_max_speed = 0.95;

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Bhv_KeepShootChance" );

    const rcsc::WorldModel & wm = agent->world();

    const rcsc::Bhv_PassTest::PassRoute * pass
        = rcsc::Bhv_PassTest::get_best_pass( wm );

    if ( pass )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__":  pass found (%.1f, %.1f)",
                            pass->receive_point_.x,
                            pass->receive_point_.y );
        if ( pass->receive_point_.x > 30.0
             && pass->receive_point_.absY() < rcsc::ServerParam::i().goalHalfWidth() + 4.0 )
        {
            agent->debugClient().addMessage( "KeepChancePass" );
            rcsc::Bhv_PassTest().execute( agent );
            return true;
        }
    }

    const rcsc::PlayerObject * opp_goalie = wm.getOpponentGoalie();
    rcsc::Vector2D goalie_pos( 100.0, 0.0 );
    double goalie_dist = 15.0;
    int goalie_conf = 0;

    if ( opp_goalie )
    {
        goalie_pos = opp_goalie->pos();
        goalie_dist = opp_goalie->distFromSelf();
        goalie_conf = std::min( 5, opp_goalie->posCount() );
    }

    // goalie is not close
    if ( goalie_dist + goalie_max_speed * goalie_conf > 6.5
         && goalie_pos.x - wm.self().pos().x > 6.5
         )
    {
        if ( wm.self().pos().x > 36.0 //42.0
             && Bhv_Cross::get_best_point( agent, NULL ) )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": cross" );
            agent->debugClient().addMessage( "KeepChanceCross" );
            Bhv_Cross().execute( agent );
            return true;
        }

#if 1
        if ( wm.opponentsFromSelf().empty()
             || wm.opponentsFromSelf().front()->distFromSelf()
             > rcsc::ServerParam::i().defaultPlayerSpeedMax()
             + rcsc::ServerParam::i().defaultKickableArea()
             + 0.8 )
        {
            rcsc::Vector2D drib_target( rcsc::ServerParam::i().pitchHalfLength(),
                                        rcsc::ServerParam::i().goalHalfWidth() + 2.0 );
            if ( wm.self().pos().y < 0.0 )
            {
                drib_target.y *= -1.0;
            }
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": can turn dribble to goal post" );
            agent->debugClient().addMessage( "KeepChanceDrib" );
            // no dodge
            rcsc::Body_Dribble2008( drib_target,
                                    1.0,
                                    rcsc::ServerParam::i().maxPower() * 0.5,
                                    1, // only dash
                                    false
                                    ).execute( agent );

            agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan() );
            return true;
        }
#endif

        // dribble to my body dir
        if ( wm.self().pos().x < rcsc::ServerParam::i().pitchHalfLength() - 5.0
             && wm.self().pos().absY() < rcsc::ServerParam::i().goalHalfWidth() + 10.0
             && wm.self().body().abs() < 50.0 )
        {
            rcsc::Vector2D drib_target = wm.self().pos();
            drib_target += rcsc::Vector2D::polar2vector(5.0, wm.self().body());
            rcsc::Sector2D sector( wm.self().pos(),
                                   0.5, 3.0,
                                   wm.self().body() - 30.0,
                                   wm.self().body() + 30.0 );
            // opponent check with goalie
            if ( ! wm.existOpponentIn( sector, 10, true ) )
            {
                rcsc::dlog.addText( rcsc::Logger::TEAM,
                                    __FILE__": dribble to my body" );
                agent->debugClient().addMessage( "KeepChanceDribToBody" );
                // no dodge
                rcsc::Body_Dribble2008( drib_target,
                                        1.0,
                                        rcsc::ServerParam::i().maxPower(),
                                        2,
                                        false
                                        ).execute( agent );
                agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan() );
                return true;
            }
        }

        // dribble to goal line
        rcsc::Sector2D front_sector( wm.self().pos(),
                                     0.5, 6.0,
                                     -30.0, 30.0 );
        // opponent check with goalie
        if ( ! wm.existOpponentIn( front_sector, 10, true ) )
        {
            rcsc::Vector2D drib_target( //wm.self().pos().x,
                                       rcsc::ServerParam::i().pitchHalfLength(),
                                       rcsc::ServerParam::i().goalHalfWidth() + 2.0 );
            if ( wm.self().pos().y < 0.0 )
            {
                drib_target.y *= -1.0;
            }
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": dribble to goal post" );
            agent->debugClient().addMessage( "KeepChanceDribToGoal" );
            // no dodge
            rcsc::Body_Dribble2008( drib_target,
                                    1.0,
                                    rcsc::ServerParam::i().maxPower(),
                                    2,
                                    false
                                    ).execute( agent );
            agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan() );
            return true;
        }

        if ( Bhv_Cross::get_best_point( agent, NULL ) )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": cross" );
            agent->debugClient().addMessage( "KeepChanceCross(2)" );
            Bhv_Cross().execute( agent );
            return true;
        }

        // kick to far side
        if ( wm.self().pos().x > 40.0
             && wm.self().pos().absY() < 15.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": goal near. kick to far side" );
            // to far side
            agent->debugClient().addMessage( "KeepChanceToFar(1)" );
            Body_KickToCorner( (wm.self().pos().y > 0.0)
                               ).execute( agent );
            agent->setNeckAction( new rcsc::Neck_ScanField() );
            return true;
        }

        // pass
        if ( rcsc::Bhv_PassTest().execute( agent ) )
        {
            agent->debugClient().addMessage( "KeepChancePass(2)" );
            return true;
        }
    }

    if ( Bhv_Cross::get_best_point( agent, NULL ) )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": cross" );
        agent->debugClient().addMessage( "KeepChanceCross(3)" );
        Bhv_Cross().execute( agent );
        return true;
    }

    // kick to far side
    if ( wm.self().pos().x > 40.0
         && wm.self().pos().absY() < 15.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": goal near. kick to far side" );
        // to far side
        agent->debugClient().addMessage( "KeepChanceToFar(2)" );
        Body_KickToCorner( (wm.self().pos().y > 0.0) ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
        return true;
    }

    // keep
    const rcsc::PlayerObject * nearest_opp = wm.getOpponentNearestToSelf( 7 );
    const double nearest_opp_dist = ( nearest_opp
                                      ? nearest_opp->distFromSelf()
                                      : 1000.0 );
    if ( nearest_opp_dist > 5.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": keep" );
        agent->debugClient().addMessage( "KeepChanceDribCenter" );
        rcsc::Vector2D drib_target( 45.0, 0.0 );
        rcsc::Body_Dribble2008( drib_target,
                                1.0,
                                rcsc::ServerParam::i().maxPower() * 0.6,
                                1
                                ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
        return true;
    }

    if ( rcsc::Bhv_PassTest().execute( agent ) )
    {
        agent->debugClient().addMessage( "KeepChancePass(3)" );
        return true;
    }

    // cross far side
    if ( wm.self().pos().absY() < 10.0 )
    {
        agent->debugClient().addMessage( "ChanceHold" );
        rcsc::Body_HoldBall2008( true, rcsc::Vector2D( 45.0, 0.0 ) ).execute( agent );
//         rcsc::dlog.addText( rcsc::Logger::TEAM,
//                             __FILE__": enforce far side" );
//         // to far side
//         agent->debugClient().addMessage( "KeepChanceToFar(2)" );
//         Body_KickToCorner( (wm.self().pos().y > 0.0) ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_ScanField() );
        return true;
    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": cross far side" );

    // to far side
    agent->debugClient().addMessage( "KeepChanceToFar(3)" );
    Body_KickToCorner( (wm.self().pos().y > 0.0) ).execute( agent );
    agent->setNeckAction( new rcsc::Neck_ScanField() );
    return true;
}
