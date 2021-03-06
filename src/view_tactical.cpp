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

#include "view_tactical.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/view_synch.h>

#include <rcsc/player/player_agent.h>

#include <rcsc/common/logger.h>

/*-------------------------------------------------------------------*/
/*!

*/
bool
View_Tactical::execute( rcsc::PlayerAgent * agent )
{
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": View_Tactical" );

    // when our free kick mode, view width is set to WIDE
    switch ( agent->world().gameMode().type() ) {
    case rcsc::GameMode::BeforeKickOff:
    case rcsc::GameMode::AfterGoal_:
    case rcsc::GameMode::PenaltySetup_:
    case rcsc::GameMode::PenaltyReady_:
    case rcsc::GameMode::PenaltyMiss_:
    case rcsc::GameMode::PenaltyScore_:
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": doNormalChange. wide view" );
        return rcsc::View_Wide().execute( agent );

    case rcsc::GameMode::PenaltyTaken_:
        return rcsc::View_Synch().execute( agent );

    case rcsc::GameMode::GoalieCatch_:
        // our goalie kick
        if ( agent->world().gameMode().side() == agent->world().ourSide() )
        {
            return doOurGoalieFreeKick( agent );
        }

    case rcsc::GameMode::BackPass_:
    case rcsc::GameMode::FreeKickFault_:
    case rcsc::GameMode::CatchFault_:
    case rcsc::GameMode::GoalKick_:
    case rcsc::GameMode::KickOff_:
    case rcsc::GameMode::KickIn_:
    case rcsc::GameMode::CornerKick_:
    case rcsc::GameMode::FreeKick_:
    case rcsc::GameMode::OffSide_:
    case rcsc::GameMode::IndFreeKick_:
    default:
        break;
    }

    return doDefault( agent );
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
View_Tactical::doDefault( rcsc::PlayerAgent * agent )
{
    if ( ! agent->world().ball().posValid() )
    {
        // for field scan
        return agent->doChangeView( rcsc::ViewWidth::WIDE );
    }

    const double ball_dist = agent->world().ball().distFromSelf();

    // ball dist is too far
    if ( ball_dist > 40.0 )
    {
        return agent->doChangeView( rcsc::ViewWidth::WIDE );
    }

    if ( ball_dist > 20.0 )
    {
        return agent->doChangeView( rcsc::ViewWidth::NORMAL );
    }

    if ( ball_dist > 10.0 )
    {
        rcsc::AngleDeg ball_angle = agent->effector().queuedNextAngleFromBody
            ( agent->effector().queuedNextBallPos() );
        if ( ball_angle.abs() > 120.0 )
        {
            return agent->doChangeView( rcsc::ViewWidth::WIDE );
        }
    }

    //const bool teammate_kickable = agent->world().existKickableTeammate();
    //const bool opponent_kickable = agent->world().existKickableOpponent();

    double teammate_ball_dist = 1000.0;
    double opponent_ball_dist = 1000.0;

    if ( ! agent->world().teammatesFromBall().empty() )
    {
        teammate_ball_dist
            = agent->world().teammatesFromBall().front()->distFromBall();
    }

    if ( ! agent->world().opponentsFromBall().empty() )
    {
        opponent_ball_dist
            = agent->world().opponentsFromBall().front()->distFromBall();
    }

    if ( teammate_ball_dist > 5.0
         && opponent_ball_dist > 5.0
         && ball_dist > 10.0 )
    {
        return agent->doChangeView( rcsc::ViewWidth::NORMAL );
    }

    return rcsc::View_Synch().execute( agent );

}

/*-------------------------------------------------------------------*/
/*!

*/
bool
View_Tactical::doOurGoalieFreeKick( rcsc::PlayerAgent * agent )
{
    if ( agent->world().self().goalie() )
    {
        return rcsc::View_Wide().execute( agent );
    }

    return doDefault( agent );
}
