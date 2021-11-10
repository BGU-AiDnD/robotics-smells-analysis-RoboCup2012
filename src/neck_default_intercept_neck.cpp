// -*-c++-*-

/*!
  \file neck_default_intercept_neck.cpp
  \brief default neck action to use with intercept action
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

#include "neck_default_intercept_neck.h"

#include <rcsc/action/neck_turn_to_point.h>
#include <rcsc/action/neck_turn_to_ball_and_player.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/view_wide.h>
#include <rcsc/action/view_normal.h>
#include <rcsc/action/view_synch.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

#include "bhv_pass_test.h"
#include "neck_default_intercept_neck.h"

/*-------------------------------------------------------------------*/
/*!

*/
Neck_DefaultInterceptNeck::~Neck_DefaultInterceptNeck()
{
    if ( M_default_view_act )
    {
        delete M_default_view_act;
        M_default_view_act = static_cast< rcsc::ViewAction * >( 0 );
    }

    if ( M_default_neck_act )
    {
        delete M_default_neck_act;
        M_default_neck_act = static_cast< rcsc::NeckAction * >( 0 );
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Neck_DefaultInterceptNeck::execute( rcsc::PlayerAgent * agent )
{
//     const rcsc::WorldModel & wm = agent->world();

//     if ( wm.existKickableOpponent() )
//     {
//         const rcsc::PlayerObject * opp = wm.interceptTable()->fastestOpponent();
//         if ( opp )
//         {
//             rcsc::Neck_TurnToBallAndPlayer( opp ).execute( agent );
//             rcsc::dlog.addText( rcsc::Logger::TEAM,
//                                 __FILE__": Neck_DefaultInterceptNeck. ball and player" );
//             return true;
//         }
//     }

    if ( doTurnToReceiver( agent ) )
    {
        return true;
    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Neck_DefaultInterceptNeck. default action" );

    if ( M_default_view_act )
    {
        M_default_view_act->execute( agent );
        agent->debugClient().addMessage( "InterceptNeck:DefaultView" );
    }
//     else
//     {
//         int self_min = wm.interceptTable()->selfReachCycle();
//         int opp_min = wm.interceptTable()->opponentReachCycle();

//         if ( self_min == 4 && opp_min >= 2 )
//         {
//             rcsc::View_Wide().execute( agent );
//         }
//         else if ( self_min == 3 && opp_min >= 2 )
//         {
//             rcsc::View_Normal().execute( agent );
//         }
//     }

    if ( M_default_neck_act )
    {
        M_default_neck_act->execute( agent );
        agent->debugClient().addMessage( "InterceptNeck:DefaultNeck" );
    }
    else
    {
        rcsc::Neck_TurnToBallOrScan().execute( agent );
        agent->debugClient().addMessage( "InterceptNeck:BallOrScan" );
    }
    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Neck_DefaultInterceptNeck::doTurnToReceiver( rcsc::PlayerAgent * agent )
{
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": doTurnToReceiver()" );
    const rcsc::WorldModel & wm = agent->world();

    if ( wm.interceptTable()->selfReachCycle() >= 2 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": doTurnToReceiver() self reach cycle >= 2 " );
        return false;
    }

    const rcsc::Bhv_PassTest::PassRoute * pass = rcsc::Bhv_PassTest::get_best_pass( wm );
    if ( ! pass )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": doTurnToReceiver() no pass." );
        return false;
    }

#if 0
    // 2008-07-11
    if ( pass->type_ == rcsc::Bhv_PassTest::RECURSIVE
         //&& wm.self().pos().x > 0.0
         && wm.self().pos().x > wm.offsideLineX() - 15.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": doTurnToReceiver() offensive area. cancel turn_neck fo recursive pass" );
        return false;
    }
#endif
//     if ( pass->type_ != rcsc::Bhv_PassTest::RECURSIVE )
//     {
//         rcsc::dlog.addText( rcsc::Logger::TEAM,
//                             __FILE__": doTurnToReceiver() no recursive pass." );
//         return false;
//     }

    if ( pass->receiver_->posCount() <= 0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": doTurnToReceiver() receiver pos_count %d <= 1",
                            pass->receiver_->posCount() );
        return false;
    }

//     int rest_cycles = wm.interceptTable()->selfReachCycle();
//     if ( agent->effector().queuedNextBallKickable() )
//     {
//         rest_cycles = 1;
//     }

//     const rcsc::SeeState & see_state = agent->seeState();
//     rcsc::dlog.addText( rcsc::Logger::TEAM,
//                         __FILE__": rest cycles = %d", rest_cycles );
//     rcsc::dlog.addText( rcsc::Logger::TEAM,
//                         __FILE__": next see = %d", see_state.cyclesTillNextSee() );

//     int see_cycles = see_state.cyclesTillNextSee() + 1;

//     if ( rest_cycles == 4
//          && wm.ball().posCount() <= 1 )
//     {
//         //agent->setViewAction( new rcsc::View_Wide() );
//         rcsc::View_Wide().execute( agent );
//     }
//     else if ( rest_cycles < see_cycles )
//     {
//         //agent->setViewAction( new rcsc::View_Synch() );
//         rcsc::View_Synch().execute( agent );
//     }

    rcsc::View_Synch().execute( agent );

    rcsc::Vector2D face_point = pass->receiver_->pos();
    if ( pass->receiver_->distFromBall() > 5.0
         && pass->receiver_->pos().x < 35.0 )
    {
        face_point.x += 3.0;
    }

    const rcsc::Vector2D next_pos = agent->effector().queuedNextMyPos();
    const rcsc::AngleDeg next_body = agent->effector().queuedNextMyBody();
    const double next_view_half_width = agent->effector().queuedNextViewWidth().width() * 0.5;

    rcsc::AngleDeg rel_angle = ( face_point - next_pos ).th() - next_body;

    if ( rel_angle.abs() > rcsc::ServerParam::i().maxNeckAngle() + next_view_half_width - 5.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": Neck_DefaultInterceptNeck.cannot face to receiver %d (%.1f %.1f)",
                            pass->receiver_->unum(),
                            face_point.x, face_point.y );

        // TODO: check 2nd pass candidate
        return false;
    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Neck_DefaultInterceptNeck.(%.1f %.1f)",
                        face_point.x, face_point.y );
    rcsc::Neck_TurnToPoint( face_point ).execute( agent );

    agent->debugClient().setTarget( pass->receiver_->unum() );
    agent->debugClient().addMessage( "InterceptNeck:toReceiver" );
    agent->debugClient().addLine( wm.self().pos(), face_point );
    agent->debugClient().addCircle( face_point, 3.0 );
    return true;
}


/*-------------------------------------------------------------------*/
/*!

*/
rcsc::NeckAction *
Neck_DefaultInterceptNeck::clone() const
{
    return new Neck_DefaultInterceptNeck( ( M_default_view_act
                                            ? M_default_view_act->clone()
                                            : static_cast< rcsc::ViewAction * >( 0 ) ),
                                          ( M_default_neck_act
                                            ? M_default_neck_act->clone()
                                            : static_cast< rcsc::NeckAction * >( 0 ) ) );
}
