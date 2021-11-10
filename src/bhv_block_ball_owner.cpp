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

#include "bhv_block_ball_owner.h"

#include <rcsc/soccer_math.h>
#include <rcsc/geom/segment_2d.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_turn_to_angle.h>
#include <rcsc/action/neck_turn_to_ball_and_player.h>

#include "body_intercept2008.h"
#include "bhv_basic_tackle.h"
#include "bhv_danger_area_tackle.h"
#include "neck_check_ball_owner.h"

#include "strategy.h"

using namespace rcsc;

int Bhv_BlockBallOwner::S_wait_count = 0;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_BlockBallOwner::execute( PlayerAgent * agent )
{
    if ( ! M_bounding_region )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": Bhv_BlockBallOwner no region" );
        return false;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_BlockBallOwner rect=(%.1f %.1f)(%.1f %.1f)",
                  M_bounding_region->left(), M_bounding_region->top(),
                  M_bounding_region->right(), M_bounding_region->bottom() );

    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    //
    // get block point
    //
    const PlayerObject * opponent = wm.interceptTable()->fastestOpponent();

    Vector2D block_point = getBlockPoint( wm, opponent );

    if ( ! block_point.isValid() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": no block point" );
        return false;
    }

    if ( ! M_bounding_region->contains( block_point ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": danger area tackle" );
        return false;
    }

    //
    // tackle
    //
    if ( wm.ball().pos().x < -36.0
         &&  wm.ball().pos().absY() < 16.0
         && Bhv_DangerAreaTackle( 0.85 ).execute( agent ) )
    {
        agent->debugClient().addMessage( "Block:DangerTackle" );
        dlog.addText( Logger::ROLE,
                      __FILE__": danger area tackle" );
        return true;
    }

    if ( Bhv_BasicTackle( 0.85, 60.0 ).execute( agent ) )
    {
        agent->debugClient().addMessage( "Block:Tackle" );
        dlog.addText( Logger::ROLE,
                      __FILE__": basic tackle" );
        return true;
    }

    //
    // action
    //
    if ( ! doBodyAction( agent, opponent, block_point ) )
    {
        return false;
    }

    agent->setNeckAction( new Neck_CheckBallOwner() );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_BlockBallOwner::doBodyAction( PlayerAgent * agent,
                                  const PlayerObject * opponent,
                                  const Vector2D & block_point )
{
    const WorldModel & wm = agent->world();

    if ( opponentIsBlocked( wm, opponent, block_point ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": opponent is blocked. get the ball." );
        agent->debugClient().addMessage( "Blocked:GetBall" );
        Body_Intercept2008( true ).execute( agent );
        return true;
    }

    double dist_thr = 1.0;

    agent->debugClient().addMessage( "Block" );
    agent->debugClient().setTarget( block_point );
    agent->debugClient().addCircle( block_point, dist_thr );

    double dash_power = wm.self().getSafetyDashPower( ServerParam::i().maxPower() );

    if ( Body_GoToPoint( block_point,
                         dist_thr,
                         dash_power,
                         1, // cycle
                         false, // no back dash
                         true, // save recovery
                         40.0 // dir_thr
                         ).execute( agent ) )
    {
        S_wait_count = 0;
        agent->debugClient().addMessage( "GoTo%.0f",
                                         dash_power );
        dlog.addText( Logger::TEAM,
                      __FILE__": GoToPoint (%.1f %.1f) power=%.1f",
                      block_point.x, block_point.y,
                      dash_power );
        return true;
    }

    ++S_wait_count;

    dlog.addText( Logger::TEAM,
                  __FILE__": wait_count=%d",
                  S_wait_count );
    if ( S_wait_count >= 10 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": waited. attack to the ball" );
        agent->debugClient().addMessage( "WaitOver:Intercept" );
        Body_Intercept2008( true ).execute( agent );
        return true;
    }

    AngleDeg body_angle = wm.ball().angleFromSelf() + 90.0;
    if ( body_angle.abs() < 90.0 )
    {
        body_angle += 180.0;
    }

    agent->debugClient().addMessage( "TurnTo%.0f",
                                     body_angle.degree() );
    dlog.addText( Logger::TEAM,
                  __FILE__": turn to anble=%.1f",
                  body_angle.degree() );

    Body_TurnToAngle( body_angle ).execute( agent );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_BlockBallOwner::opponentIsBlocked( const WorldModel & wm,
                                       const PlayerObject * opponent,
                                       const Vector2D & block_point )
{
    if ( ! opponent
         || ! block_point.isValid() )
    {
        return false;
    }

    const double dist_thr2 = square( 1.0 );

    //const Vector2D opp_pos = opponent->pos() + opponent->vel();
    const Vector2D block_pos = opponent->pos() + opponent->vel();

    const PlayerPtrCont::const_iterator end = wm.teammatesFromBall().end();
    for ( PlayerPtrCont::const_iterator it = wm.teammatesFromBall().begin();
          it != end;
          ++it )
    {
        if ( (*it)->isGhost() ) continue;
        if ( (*it)->posCount() >= 5 ) continue;

        if ( ( (*it)->pos() + (*it)->vel() ).dist2( block_pos ) < dist_thr2 )
        {
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_BlockBallOwner::getBlockPoint( const WorldModel & wm,
                                   const PlayerObject * opponent )
{
    if ( ! opponent )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": execute(). no opponent" );
        return Vector2D::INVALIDATED;
    }

    //
    // estimate opponent info
    //
    Vector2D opp_pos = opponent->pos() + opponent->vel();
    Vector2D opp_target( -45.0, opp_pos.y * 0.9 );

    dlog.addText( Logger::TEAM,
                  __FILE__": getBlockPoint() opp_pos=(%.1f %.1f) opp_target=(%.1f %.1f)",
                  opp_pos.x, opp_pos.y,
                  opp_target.x, opp_target.y );

    AngleDeg opp_body;
    if ( opponent->bodyCount() <= 3
         && opp_body.abs() > 150.0 )

    {
        opp_body = opponent->body();
        dlog.addText( Logger::TEAM,
                      __FILE__": getBlockPoint() set opponent body %.0f",
                      opp_body.degree() );
    }
    else
    {
        opp_body = ( opp_target - opp_pos ).th();
        dlog.addText( Logger::TEAM,
                      __FILE__": getBlockPoint() set opp target point(%.1f %.1f) as opp body %.0f",
                      opp_target.x, opp_target.y,
                      opp_body.degree() );

    }

    //
    // check intersection with current body line
    //
    const Vector2D opp_unit_vec = Vector2D::polar2vector( 1.0, opp_body );

    opp_target = opp_pos + opp_unit_vec * 10.0;

    dlog.addText( Logger::TEAM,
                  __FILE__": getBlockPoint() set opp_target=(%.1f %.1f)",
                  opp_target.x, opp_target.y );

    Segment2D target_segment( opp_pos + opp_unit_vec * 1.0,
                              opp_pos + opp_unit_vec * 6.0 );

    Vector2D my_pos = wm.self().pos() + wm.self().vel();
    Segment2D my_move_segment( my_pos,
                               my_pos + Vector2D::polar2vector( 100.0, wm.self().body() ) );
    Vector2D intersection = my_move_segment.intersection( target_segment, true );

    //
    // set block point
    //
    Vector2D block_point( - 50.0, 0.0 );

    if ( intersection.isValid() )
    {
        block_point = intersection;
        dlog.addText( Logger::TEAM,
                      __FILE__": getBlockPoint() intersection" );
    }
    else
    {
        block_point = opp_pos + opp_unit_vec * 4.0;
        dlog.addText( Logger::TEAM,
                      __FILE__": getBlockPoint() no intersection" );
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": getBlockPoint() opponent=%d (%,2f, %,2f) block_point=(%.1f %.1f)",
                  opponent->unum(),
                  opponent->pos().x, opponent->pos().y,
                  block_point.x, block_point.y );

    return block_point;
}
