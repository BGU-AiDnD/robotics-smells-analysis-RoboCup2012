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

#include "bhv_offensive_half_defensive_move.h"

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
Bhv_OffensiveHalfDefensiveMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_OffensiveHalfDefensiveMove" );

    //
    // tackle
    //
    if ( Bhv_BasicTackle( 0.8, 90.0 ).execute( agent ) )
    {
        agent->debugClient().addMessage( "OH:Def:Tackle" );
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

    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    // intercept
    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( self_min < 16
         && opp_min >= self_min - 1 )
    {
        Vector2D self_trap_pos
            = inertia_n_step_point( wm.ball().pos(),
                                    wm.ball().vel(),
                                    self_min,
                                    ServerParam::i().ballDecay() );
        bool enough_stamina = true;
        double estimated_consume
            = wm.self().playerType().getOneStepStaminaComsumption()
            * self_min;
        if ( wm.self().stamina() - estimated_consume < ServerParam::i().recoverDecThrValue() )
        {
            enough_stamina = false;
        }

        if ( enough_stamina
             && opp_min < 3
             && ( home_pos.dist( self_trap_pos ) < 10.0
                  || ( home_pos.absY() < self_trap_pos.absY()
                       && home_pos.y * self_trap_pos.y > 0.0 ) // same side
                  || self_trap_pos.x < home_pos.x
                  )
             )
        {
            agent->debugClient().addMessage( "Intercept(1)" );
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
    }

    if ( self_min < 15
         && self_min < mate_min + 2
         && ! wm.existKickableTeammate() )
    {
        agent->debugClient().addMessage( "Intercept(2)" );
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
        agent->setNeckAction( new Neck_TurnToBallOrScan() );
#else
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
#endif
        return true;
    }

    Vector2D target_point = home_pos;
    //     if ( wm.existKickableTeammate()
    //          || ( mate_min < 2 && opp_min > 2 )
    //          )
    //     {
    //         target_point.x += 10.0;
    //     }
    // #if 1
    //     else if ( home_pos.y * wm.ball().pos().y > 0.0 ) // same side
    //     {
    //         target_point.x = wm.ball().pos().x + 1.0;
    //     }
    // #endif
    Bhv_BasicMove( target_point ).execute( agent );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_OffensiveHalfDefensiveMove::doIntercept( PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( ! wm.existKickableTeammate()
         && self_min <= mate_min
         && self_min <= opp_min + 1 )
    {
        agent->debugClient().addMessage( "OH:Def:Intercept" );
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept" );
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
Bhv_OffensiveHalfDefensiveMove::doGetBall( PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();

    if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.5 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) no stamina" );
        return false;
    }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    const Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );
    double dist_to_trap_pos = opp_trap_pos.dist( home_pos );

    if ( ! wm.existKickableTeammate()
         && opp_min < mate_min
         && opp_min < self_min
         && ( dist_to_trap_pos < 10.0
#if 1
              || ( position_type == Position_Left
                   && opp_trap_pos.y < home_pos.y
                   && opp_trap_pos.y > home_pos.y - 7.0
                   && opp_trap_pos.x < home_pos.x + 5.0 )
              || ( position_type == Position_Right
                   && opp_trap_pos.y > home_pos.y
                   && opp_trap_pos.y < home_pos.y + 7.0
                   && opp_trap_pos.x < home_pos.x + 5.0 )
#endif
              )
         )
    {

        if ( ! wm.existKickableTeammate()
             && self_min <= 4 )
        {
            agent->debugClient().addMessage( "OH:GetBall:Intercept" );
            dlog.addText( Logger::ROLE,
                          __FILE__": intercept" );
            Body_Intercept2008().execute( agent );
            agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
            return true;
        }

        Rect2D bounding_rect( Vector2D( home_pos.x - 10.0, home_pos.y - 15.0 ),
                              Vector2D( home_pos.x + 6.0, home_pos.y + 15.0 ) );
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) rect=(%.1f %.1f)(%.1f %.1f)",
                      bounding_rect.left(), bounding_rect.top(),
                      bounding_rect.right(), bounding_rect.bottom() );

        if ( opp_trap_pos.x > -36.0 )
        {
            if ( Bhv_GetBall( bounding_rect ).execute( agent ) )
            {
                agent->debugClient().addMessage( "OH:GetBall" );
                return true;
            }
        }

        if ( Bhv_BlockBallOwner( new Rect2D( bounding_rect )
                                 ).execute( agent ) )
        {
            agent->debugClient().addMessage( "OH:Def:Block" );
            return true;
        }
    }


    return false;
}
