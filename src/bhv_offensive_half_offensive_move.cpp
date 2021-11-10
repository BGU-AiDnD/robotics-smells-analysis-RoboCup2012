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

#include "bhv_offensive_half_offensive_move.h"

#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_ball_and_player.h>

#include "body_intercept2008.h"

#include "bhv_basic_move.h"
#include "bhv_attacker_offensive_move.h"
#include "bhv_basic_tackle.h"
#include "bhv_get_ball.h"
#include "bhv_block_ball_owner.h"
#include "neck_check_ball_owner.h"
#include "neck_offensive_intercept_neck.h"

#include "strategy.h"

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_OffensiveHalfOffensiveMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_OffensiveHalfOffensiveMove" );

    const WorldModel & wm = agent->world();
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    //
    // attacker move
    //
    if ( home_pos.x > 30.0
         && home_pos.absY() < 7.0 )
    {
        agent->debugClient().addMessage( "OH:Off:Attacker" );
        Bhv_AttackerOffensiveMove( home_pos, false ).execute( agent );
        return true;
    }

    //
    // tackle
    //
    if ( Bhv_BasicTackle( 0.8, 90.0 ).execute( agent ) )
    {
        agent->debugClient().addMessage( "OH:Off:Tackle" );
        return true;
    }

    //
    // intercept
    //
    if ( doIntercept( agent ) )
    {
        return true;
    }

    //
    // get ball
    //
    if ( doGetBall( agent ) )
    {
        return true;
    }

#if 0
    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();
    const Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );
    const PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();

    //
    // block ball owner opponent
    //
    if ( fastest_opp
         && opp_min < self_min - 2
         && opp_min < mate_min
         && std::fabs( opp_trap_pos.y - home_pos.y ) < 10.0
         && opp_trap_pos.x < home_pos.x + 10.0
         && opp_trap_pos.dist( home_pos ) < 18.0 )
    {
        Vector2D top_left( -50.0, home_pos.y - 15.0 );
        Vector2D bottom_right( home_pos.x + 5.0, home_pos.y + 15.0 );
        dlog.addText( Logger::ROLE,
                      __FILE__": try block ball owner" );
        //if ( Bhv_BlockBallOwner( new Rect2D( top_left, bottom_right )
        //                         ).execute( agent ) )
        if ( Bhv_GetBall( Rect2D( top_left, bottom_right ) ).execute( agent ) )
        {
            agent->debugClient().addMessage( "OH:OffMove:Block" );
            return true;
        }
    }

    if ( self_min <= mate_min
         && ! wm.existKickableTeammate() )
    {
        agent->debugClient().addMessage( "OH:OffMove:Intercept" );
        Body_Intercept2008().execute( agent );
#if 0
        if ( self_min == 4 && opp_min >= 2 )
        {
            agent->setViewAction( new View_Wide() );
        }
        else if ( self_min == 3 && opp_min >= 2 )
        {
            agent->setViewAction( new View_Normal() );
        }
        agent->setNeckAction( new Neck_TurnToBall() );
#else
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
#endif
        return true;
    }

    if ( opp_min < mate_min
         && wm.ball().pos().dist( home_pos ) < 10.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": doOffensiveMove. ball owner is not our. press" );
        //agent->debugClient().addMessage( "OffPress(2)" );
        //Bhv_Press( home_pos ).execute( agent );
        Rect2D bounding_rect( Vector2D( -50.0, home_pos.y - 10.0 ),
                              Vector2D( home_pos.x + 5.0, home_pos.y + 10.0 ) );
        if ( Bhv_GetBall( bounding_rect ).execute( agent ) )
        {
            agent->debugClient().addMessage( "OH:OffMove:GetBall" );
            return true;
        }
    }
#endif

    dlog.addText( Logger::ROLE,
                  __FILE__": basic move" );
    agent->debugClient().addMessage( "OH:OffMove:Basic" );
    Bhv_BasicMove( home_pos ).execute( agent );

    return true;
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_OffensiveHalfOffensiveMove::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( ! wm.existKickableTeammate()
         && self_min <= mate_min + 1
         && self_min <= opp_min + 1 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept" );
        agent->debugClient().addMessage( "OH:Off:Intercept" );
        Body_Intercept2008().execute( agent );
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_OffensiveHalfOffensiveMove::doGetBall( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.4 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) no stamina" );
        return false;
    }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();
    const PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();
    const Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

    //
    // block ball owner opponent
    //
    if ( fastest_opp
         && opp_min < self_min // - 2
         && opp_min < mate_min
         && std::fabs( opp_trap_pos.y - home_pos.y ) < 10.0
         && opp_trap_pos.x < home_pos.x + 10.0
         && opp_trap_pos.dist( home_pos ) < 18.0 )
    {
        Vector2D top_left( -52.0, home_pos.y - 15.0 );
        Vector2D bottom_right( home_pos.x + 5.0, home_pos.y + 15.0 );
        dlog.addText( Logger::ROLE,
                      __FILE__": try block ball owner" );
        if ( Bhv_GetBall( Rect2D( top_left, bottom_right ) ).execute( agent ) )
        {
            agent->debugClient().addMessage( "OH:Off:GetBall" );
            return true;
        }

        if ( Bhv_BlockBallOwner( new Rect2D( top_left, bottom_right )
                                 ).execute( agent ) )
        {
            agent->debugClient().addMessage( "OH:Off:Block" );
            return true;
        }
    }

    return false;
}
