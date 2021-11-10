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

#include "bhv_mark_pass_line.h"

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/bhv_scan_field.h>
#include <rcsc/action/bhv_go_to_point_look_ball.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_player_or_scan.h>
#include <rcsc/action/view_synch.h>

#include "body_intercept2008.h"

#include "bhv_basic_tackle.h"
#include "bhv_basic_move.h"
#include "bhv_defender_basic_block_move.h"

#include "strategy.h"
#include "mark_table.h"

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_MarkPassLine::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM | Logger::MARK,
                  __FILE__": Bhv_MarkPassLine" );

    const WorldModel & wm = agent->world();

    if ( wm.ball().pos().x < 10.0 )
    {
        dlog.addText( Logger::TEAM | Logger::MARK,
                      __FILE__": ball is in defensive area" );
        return false;
    }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    //
    // get mark target player
    //
    const AbstractPlayerObject * mark_target = getMarkTarget( wm );

    if ( ! mark_target )
    {
        dlog.addText( Logger::TEAM | Logger::MARK,
                      __FILE__": no mark target" );
        return false;
    }

    const int opp_min = wm.interceptTable()->opponentReachCycle();
    const AbstractPlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();

    if ( mark_target == fastest_opp )
    {
        dlog.addText( Logger::TEAM | Logger::MARK,
                      __FILE__": mark target is same as the fastest opponent." );
        return false;
    }

    //
    // check mark target accuracy
    //
    if ( mark_target->isGhost() )
    {
        if ( wm.ball().distFromSelf() < 20.0
             && ( wm.existKickableOpponent()
                  || ( opp_min <= 3
                       && fastest_opp->distFromBall() < 3.0 )
                  )
             )
        {
            dlog.addText( Logger::TEAM | Logger::MARK,
                          __FILE__": exist ball access opponent. cancel to find the mark target" );
            return false;
        }

        if ( mark_target->ghostCount() >= 2 )
        {
            dlog.addText( Logger::TEAM | Logger::MARK,
                          __FILE__": could not find the ghost" );
            return false;
        }

        agent->debugClient().addMessage( "FindMarkTarget" );
        dlog.addText( Logger::TEAM | Logger::MARK,
                      __FILE__": ghost opponent. direct look" );
        //Bhv_ScanField().execute( agent );
        Bhv_BodyNeckToPoint( mark_target->pos() ).execute( agent );
        return true;
    }

    if ( mark_target->posCount() >= 5 )
    {
        agent->debugClient().addMessage( "CheckMarkTarget" );
        dlog.addText( Logger::TEAM | Logger::MARK,
                      __FILE__": check mark target" );
        return Bhv_NeckBodyToPoint( mark_target->pos() ).execute( agent );
    }

    //
    // get the move position
    //
    Vector2D target_point = getMarkPosition( wm, mark_target );

    if ( ! target_point.isValid() )
    {
        dlog.addText( Logger::TEAM | Logger::MARK,
                      __FILE__": illegal mark target point" );
        return false;
    }

    //
    // go to the move point
    //
    double dash_power = Strategy::get_defender_dash_power( wm, target_point );
    double dist_thr = mark_target->pos().dist( target_point ) * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( Body_GoToPoint( target_point, dist_thr, dash_power ).execute( agent ) )
    {
        agent->debugClient().addMessage( "MarkPass:Go%.0f", dash_power );
    }
    else
    {
        AngleDeg body_angle = ( wm.ball().pos().y < wm.self().pos().y
                                ? -90.0
                                : 90.0 );
        Body_TurnToAngle( body_angle ).execute( agent );
        agent->debugClient().addMessage( "MarkPass:Turn%.0f", body_angle.degree() );
    }

    if ( mark_target->posCount() >= 3 )
    {
#if 0
        agent->setNeckAction( new Neck_TurnToPlayerOrScan( mark_target, 3 ) );
#else
        Vector2D face_point = mark_target->pos() + mark_target->vel();
        Vector2D next_self_pos = wm.self().pos() + wm.self().vel();
        AngleDeg neck_angle = ( face_point - next_self_pos ).th();
        neck_angle -= wm.self().body();

        int see_cycle = agent->effector().queuedNextSeeCycles();
        if ( see_cycle > 1 )
        {
            agent->setViewAction( new View_Synch() );
        }

        double view_half_width = agent->effector().queuedNextViewWidth().width();
        double neck_min = ServerParam::i().minNeckAngle() - ( view_half_width - 20.0 );
        double neck_max = ServerParam::i().maxNeckAngle() + ( view_half_width - 20.0 );

        if ( neck_min < neck_angle.degree()
             && neck_angle.degree() < neck_max )
        {
            neck_angle = ServerParam::i().normalizeNeckAngle( neck_angle.degree() );
            agent->setNeckAction( new Neck_TurnToRelative( neck_angle ) );
            agent->debugClient().addMessage( "CheckMarkTargetNeck" );
        }
        else
        {
            agent->debugClient().addMessage( "CheckMarkTargetBody" );
            agent->debugClient().setTarget( face_point );
            Bhv_NeckBodyToPoint( face_point ).execute( agent );
        }
#endif
    }
    else
    {
        int count_thr = -1;
        switch ( agent->effector().queuedNextViewWidth().type() ) {
        case ViewWidth::NARROW:
            count_thr = 1;
            break;
        case ViewWidth::NORMAL:
            count_thr = 2;
            break;
        case ViewWidth::WIDE:
            count_thr = 3;
            break;
        default:
            break;
        }

        agent->setNeckAction( new Neck_TurnToBallOrScan( count_thr ) );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
const
AbstractPlayerObject *
Bhv_MarkPassLine::getMarkTarget( const rcsc::WorldModel & wm )
{
#if 0
    return Strategy::i().markTable().getTargetOf( wm.self().unum() );
#else
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    dlog.addText( Logger::TEAM | Logger::MARK,
                  __FILE__": position = %s",
                  ( position_type == Position_Left
                    ? "left"
                    : position_type == Position_Right
                    ? "right"
                    : "center" ) );

    const PlayerObject * nearest_attacker = static_cast< const PlayerObject * >( 0 );
    const PlayerObject * outside_attacker = static_cast< const PlayerObject * >( 0 );
    double min_dist2 = 100000.0;

    const PlayerPtrCont::const_iterator end = wm.opponentsFromSelf().end();
    for ( PlayerPtrCont::const_iterator it = wm.opponentsFromSelf().begin();
          it != end;
          ++it )
    {
        bool same_side = false;
        if ( position_type == Position_Center
             || ( position_type == Position_Left
                  && (*it)->pos().y < 0.0 )
             || ( position_type == Position_Right
                  && (*it)->pos().y > 0.0 )
             )
        {
            same_side = true;
        }

        dlog.addText( Logger::TEAM | Logger::MARK,
                      __FILE__": check player %d (%.1f %.1f)",
                      (*it)->unum(),
                      (*it)->pos().x, (*it)->pos().y );

        if ( same_side
             && (*it)->pos().x < wm.ourDefenseLineX() + 17.0
             && (*it)->pos().absY() > home_pos.absY() - 5.0 )
        {
            outside_attacker = *it;
            dlog.addText( Logger::TEAM | Logger::MARK,
                          __FILE__": found outside attacker %d (%.1f %.1f)",
                          outside_attacker->unum(),
                          outside_attacker->pos().x, outside_attacker->pos().y );
        }

        if ( (*it)->pos().x > wm.ourDefenseLineX() + 10.0 )
        {
            if ( (*it)->pos().x > home_pos.x ) continue;
        }
        if ( (*it)->pos().x > home_pos.x + 10.0 ) continue;
        if ( (*it)->pos().x < home_pos.x - 20.0 ) continue;
        if ( std::fabs( (*it)->pos().y - home_pos.y ) > 20.0 ) continue;

        double d2 = (*it)->pos().dist( home_pos );
        if ( d2 < min_dist2 )
        {
            min_dist2 = d2;
            nearest_attacker = *it;
        }
    }

    if ( outside_attacker
         && nearest_attacker != outside_attacker )
    {
        dlog.addText( Logger::TEAM | Logger::MARK,
                      __FILE__": exist outside attacker %d (%.1f %.1f)",
                      outside_attacker->unum(),
                      outside_attacker->pos().x, outside_attacker->pos().y );
        return static_cast< PlayerObject * >( 0 );
    }

    if ( nearest_attacker )
    {
        dlog.addText( Logger::TEAM | Logger::MARK,
                      __FILE__": exist nearest attacker %d (%.1f %.1f)",
                      nearest_attacker->unum(),
                      nearest_attacker->pos().x, nearest_attacker->pos().y );

        return nearest_attacker;
    }

    return static_cast< PlayerObject * >( 0 );
#endif
}

/*-------------------------------------------------------------------*/
/*!

*/
Vector2D
Bhv_MarkPassLine::getMarkPosition( const rcsc::WorldModel & wm,
                                   const AbstractPlayerObject * target_player )
{
    if ( ! target_player )
    {
        return Vector2D::INVALIDATED;
    }

    Vector2D target_point = Vector2D::INVALIDATED;

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    if ( home_pos.x - target_player->pos().x > 5.0 )
    {
        target_point
            = target_player->pos()
            + ( wm.ball().pos() - target_player->pos() ).setLengthVector( 1.0 );

        dlog.addText( Logger::TEAM | Logger::MARK,
                      __FILE__": block pass line front (%.1f %.1f)",
                      target_point.x, target_point.y );
    }
    else if ( wm.ball().pos().x - target_player->pos().x > 15.0 )
    {
        target_point
            = target_player->pos()
            + ( wm.ball().pos() - target_player->pos() ).setLengthVector( 1.0 );
        dlog.addText( Logger::TEAM | Logger::MARK,
                      __FILE__": block pass line (%.1f %.1f)",
                      target_point.x, target_point.y );
    }
    else
    {
        const Vector2D goal_pos( -50.0, 0.0 );

        target_point
            = target_player->pos()
            + ( goal_pos - target_player->pos() ).setLengthVector( 1.0 );
        dlog.addText( Logger::TEAM | Logger::MARK,
                      __FILE__": block through pass (%.1f %.1f)",
                      target_point.x, target_point.y );
    }

    return target_point;
}
