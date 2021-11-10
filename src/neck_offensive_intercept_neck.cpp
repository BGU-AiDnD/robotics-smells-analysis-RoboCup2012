// -*-c++-*-

/*!
  \file neck_offensive_intercept_neck.cpp
  \brief neck action for an offensive intercept situation
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

#include "neck_offensive_intercept_neck.h"

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/action/neck_turn_to_point.h>
#include <rcsc/action/neck_turn_to_ball_and_player.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_goalie_or_scan.h>
#include <rcsc/action/view_wide.h>
#include <rcsc/action/view_normal.h>
#include <rcsc/action/view_synch.h>

#include "neck_default_intercept_neck.h"

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Neck_OffensiveInterceptNeck::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.existKickableOpponent() )
    {
        const PlayerObject * opp = wm.interceptTable()->fastestOpponent();
        if ( opp )
        {
            Neck_TurnToBallAndPlayer( opp ).execute( agent );
            dlog.addText( Logger::TEAM,
                          __FILE__": Neck_OffensiveInterceptNeck. ball and player" );
            return true;
        }
    }

    const int self_min = wm.interceptTable()->selfReachCycle();
    //const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    bool ball_safety = true;
    bool ball_in_visible = false;

    const Vector2D next_self_pos = agent->effector().queuedNextMyPos();
    const Vector2D next_ball_pos = agent->effector().queuedNextBallPos();
    if ( next_self_pos.dist( next_ball_pos ) < ServerParam::i().visibleDistance() - 0.5 )
    {
        ball_in_visible = true;

        if ( opp_min <= 1 )
        {
            ball_safety = false;
            ball_in_visible = false;
        }
        else
        {
            const PlayerObject * nearest_opp = wm.getOpponentNearestTo( next_ball_pos,
                                                                        10,
                                                                        NULL );
            if ( nearest_opp
                 && next_ball_pos.dist( nearest_opp->pos() + nearest_opp->vel() )
                 < nearest_opp->playerTypePtr()->kickableArea() + 0.3 )
            {
                ball_safety = false;
                ball_in_visible = false;
            }
        }
    }

#ifdef NO_USE_NECK_DEFAULT_INTERCEPT_NECK
    if ( self_min == 4 && opp_min >= 2 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": Neck_OffensiveInterceptNeck. default view: wide" );
        View_Wide().execute( agent );
    }
    else if ( self_min == 3 && opp_min >= 2 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": Neck_OffensiveInterceptNeck. default view: normal (1)" );
        View_Normal().execute( agent );
    }
    else if ( self_min > 10 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": Neck_OffensiveInterceptNeck. default view: normal (2)" );
        View_Normal().execute( agent );
    }

    if ( next_ball_pos.x > 34.0
         && next_ball_pos.absY() < 17.0
         && ( ball_in_visible
              || ( wm.ball().posCount() <= 1
                   && ball_safety )
              )
         )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": Neck_OffensiveInterceptNeck. goalie or scan" );
        Neck_TurnToGoalieOrScan().execute( agent );
    }
    else if ( ball_in_visible )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": Neck_OffensiveInterceptNeck. low conf teammate" );
        Neck_TurnToLowConfTeammate().execute( agent );
    }
    else
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": Neck_OffensiveInterceptNeck. ball or scan" );
        Neck_TurnToBallOrScan().execute( agent );
    }
#else
    ViewAction * view = static_cast< ViewAction * >( 0 );
    if ( self_min == 4 && opp_min >= 2 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": Neck_OffensiveInterceptNeck. default view: wide" );
        view = new View_Wide();
    }
    else if ( self_min == 3 && opp_min >= 2 )
    {
        view = new View_Normal();
        dlog.addText( Logger::TEAM,
                      __FILE__": Neck_OffensiveInterceptNeck. default view: normal (1)" );
    }
    else if ( self_min > 10 )
    {
        view = new View_Normal();
        dlog.addText( Logger::TEAM,
                      __FILE__": Neck_OffensiveInterceptNeck. default view: normal (2)" );
    }

    if ( next_ball_pos.x > 34.0
         && next_ball_pos.absY() < 17.0
         && ( ball_in_visible
              || ( wm.ball().posCount() <= 1
                   && ball_safety )
              )
         )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": Neck_OffensiveInterceptNeck. default neck: goalie or scan" );
        Neck_DefaultInterceptNeck( view,
                                   new Neck_TurnToGoalieOrScan() )
            .execute( agent );

    }
    else if ( ball_in_visible )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": Neck_OffensiveInterceptNeck. default neck: low conf teammate" );
        Neck_DefaultInterceptNeck( view,
                                   new Neck_TurnToLowConfTeammate() )
            .execute( agent );
    }
    else
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": Neck_OffensiveInterceptNeck. default neck: ball or scan" );
        Neck_DefaultInterceptNeck( view,
                                   new Neck_TurnToBallOrScan() )
            .execute( agent );
    }
#endif

    return true;
}
