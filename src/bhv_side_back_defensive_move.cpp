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

#include "bhv_side_back_defensive_move.h"

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
#include "bhv_basic_tackle.h"
#include "bhv_get_ball.h"
#include "neck_check_ball_owner.h"
#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

#include "defense_system.h"
#include "mark_table.h"
#include "strategy.h"

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackDefensiveMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": Bhv_SideBackDefensiveMove" );

    //
    // tackle
    //
    if ( Bhv_BasicTackle( 0.85, 50.0 ).execute( agent ) )
    {
        agent->debugClient().addMessage( "SB:Def:Tackle" );
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
    // attack to the ball owner
    //
    if ( doAttackBallOwner( agent ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": attack to ball owner" );
        return true;
    }

    //
    // emergency move
    //
    if ( doEmergencyMove( agent ) )
    {
        return true;
    }

    doNormalMove( agent );
    return true;

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackDefensiveMove::doIntercept( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( ! wm.existKickableTeammate()
         && ! wm.existKickableOpponent()
         && self_min <= mate_min
         && self_min <= opp_min + 1 )
    {
        agent->debugClient().addMessage( "SB:Def:Intercept" );
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept" );
        Body_Intercept2008().execute( agent );
#ifdef NO_USE_NECK_DEFAULT_INTERCEPT_NECK
        agent->setNeckAction( new Neck_TurnToBallOrScan() );
#else
        if ( opp_min >= self_min + 3 )
        {
            agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        }
        else
        {
            agent->setNeckAction( new Neck_DefaultInterceptNeck
                                  ( new Neck_TurnToBallOrScan() ) );
        }
#endif
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackDefensiveMove::doAttackBallOwner( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();

    //const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    const AbstractPlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();

    if ( mate_min < opp_min )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": doAttackBallOwner() ball owner is teammate" );
        return false;
    }

    if ( ! fastest_opp )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": doAttackBallOwner() no opponent" );
        return false;
    }

    const Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    //
    // check mark target
    //

    const MarkTable & mark_table = Strategy::i().markTable();
    const AbstractPlayerObject * mark_target = mark_table.getTargetOf( wm.self().unum() );
    const AbstractPlayerObject * free_attacker = static_cast< AbstractPlayerObject * >( 0 );

    if ( mark_target )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": doAttackBallOwner() mark_target= %d (%.1f %.1f)",
                      mark_target->unum(),
                      mark_target->pos().x, mark_target->pos().y );

        //
        // find other free attacker
        //
        const AbstractPlayerCont::const_iterator o_end = wm.theirPlayers().end();
        for ( AbstractPlayerCont::const_iterator o = wm.theirPlayers().begin();
              o != o_end;
              ++o )
        {
            const AbstractPlayerObject * marker = mark_table.getMarkerOf( *o );
            if ( marker ) continue; // exist other marker
            if ( (*o)->pos().x > mark_target->pos().x + 10.0 ) continue; // no attacker
            if ( ( position_type == Position_Left
                   && (*o)->pos().y < mark_target->pos().y + 10.0 )
                 || ( position_type == Position_Right
                      && (*o)->pos().y > mark_target->pos().y - 10.0 )
                 || ( position_type == Position_Center
                      && std::fabs( (*o)->pos().y - mark_target->pos().y ) < 10.0 )
                 )
            {
                free_attacker = *o;
                break;
            }
        }
    }

    if ( free_attacker )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": doAttackBallOwner() exist free_ataccker= %d (%.1f %.1f)",
                      free_attacker->unum(),
                      free_attacker->pos().x, free_attacker->pos().y );
        //return false;
    }

    //
    //
    //

    const double min_y = ( position_type == Position_Right
                           ? 2.0
                           : -31.5 );
    const double max_y = ( position_type == Position_Left
                           ? -2.0
                           : +31.5 );

    if ( fastest_opp == mark_target )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": doAttackBallOwner() fastest_opp == mark_target, %d (%.1f %.1f) trap_pos=(%.1f %.1f)",
                      mark_target->unum(),
                      mark_target->pos().x, mark_target->pos().y,
                      opp_trap_pos.x, opp_trap_pos.y );
        Rect2D bounding_rect( Vector2D( -50.0, min_y ),
                              Vector2D( -5.0, max_y ) );
        if ( Bhv_GetBall( bounding_rect ).execute( agent ) )
        {
            agent->debugClient().addMessage( "SB:Def:GetBall(1)" );
            return true;
        }
    }

    //
    //
    //

    if ( ! mark_target )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": doAttackBallOwner() no mark target. ball_owner=%d (%.1f %.1f) trap_pos=(%.1f %.1f)",
                      fastest_opp->unum(),
                      fastest_opp->pos().x, fastest_opp->pos().y,
                      opp_trap_pos.x, opp_trap_pos.y );

        if ( opp_trap_pos.x < home_pos.x + 10.0
             && ( std::fabs( opp_trap_pos.y - home_pos.y ) < 10.0
                  || ( position_type == Position_Left
                       && opp_trap_pos.y < home_pos.y )
                  || ( position_type == Position_Right
                       && opp_trap_pos.y > home_pos.y ) )
             )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": doAttackBallOwner() no mark target. attack " );
            Rect2D bounding_rect( Vector2D( -50.0, min_y ),
                                  Vector2D( -5.0, max_y ) );
            if ( Bhv_GetBall( bounding_rect ).execute( agent ) )
            {
                agent->debugClient().addMessage( "SB:Def:GetBall(2)" );
                return true;
            }
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackDefensiveMove::doEmergencyMove( rcsc::PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();
    const PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();

    if ( mate_min + 4<= opp_min )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doEmergencyMove) ball owner is teammate" );
        return false;
    }

    if ( ! fastest_opp )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doEmergencyMove) no opponent" );
        return false;
    }

    const Vector2D next_opp_pos = fastest_opp->pos() + fastest_opp->vel();
    const Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

    const Vector2D next_self_pos = wm.self().pos() + wm.self().vel();
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    double stamina = 0.0;
    int self_step = DefenseSystem::predict_self_reach_cycle( wm, home_pos, 1.0, true, &stamina );

    dlog.addText( Logger::ROLE,
                  __FILE__": (doEmergencyMove) home=(%.1f %.1f) next_self=(%.1f %.1f) step=%d  stamina=%.1f",
                  home_pos.x, home_pos.y,
                  next_self_pos.x, next_self_pos.y,
                  self_step, stamina );
    dlog.addText( Logger::ROLE,
                  __FILE__": (doEmergencyMove) opp_step=%d opp_trap_pos=(%.1f %.1f) next_opp=(%.1f %.1f)",
                  opp_min,
                  opp_trap_pos.x, opp_trap_pos.y,
                  next_opp_pos.x, next_opp_pos.y );

    if ( opp_trap_pos.x < next_self_pos.x
         //&& home_pos.x < next_self_pos.x
         && opp_min < self_step
         && next_opp_pos.x < next_self_pos.x + 1.0
         && ( opp_trap_pos.absY() < 7.0
              || ( position_type == Position_Left && opp_trap_pos.y < 0.0 )
              || ( position_type == Position_Right && opp_trap_pos.y > 0.0 )
              )
         )
    {
        Vector2D target_point( -48.0, 7.0 );

        if ( opp_trap_pos.absY() > 23.0 )
        {
            target_point.y = 20.0;
        }
        else if ( opp_trap_pos.absY() > 16.0 )
        {
            target_point.y = 14.0;
        }
        else if ( opp_trap_pos.absY() < 7.0 )
        {
            target_point.y = 4.0;
        }

        if ( position_type == Position_Left )
        {
            target_point.y *= -1.0;
        }

        double dash_power = wm.self().getSafetyDashPower( ServerParam::i().maxPower() );
        double dist_thr = std::fabs( wm.ball().pos().x - wm.self().pos().x ) * 0.1;
        if ( dist_thr < 0.5 ) dist_thr = 0.5;

        dlog.addText( Logger::ROLE,
                      __FILE__": (doEmergencyMove) emergecy move. target=(%.1f %.1f) dist_thr=%f",
                      target_point.x, target_point.y,
                      dist_thr );
        agent->debugClient().addMessage( "SB:Def:Emergency" );
        agent->debugClient().setTarget( target_point );
        agent->debugClient().addCircle( target_point, dist_thr );

        doGoToPoint( agent, target_point, dist_thr, dash_power, 18.0 );
        agent->setNeckAction( new Neck_CheckBallOwner() );
        return true;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": (doEmergencyMove) no emergency" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SideBackDefensiveMove::doNormalMove( rcsc::PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    home_pos.x = Strategy::get_defense_line_x( wm, home_pos );
    Vector2D target_point = home_pos;

#if 0
    if ( wm.defenseLineX() < target_point.x - 1.0 )
    {
        double new_x = target_point.x * 0.55 + wm.defenseLineX() * 0.45;
        dlog.addText( Logger::ROLE,
                      __FILE__": (doNormalMove) correct target x old_x=%.1f  defense_line=%.1f  new_x=%.1f",
                      target_point.x,
                      wm.defenseLineX(),
                      new_x );
        target_point.x = new_x;
    }
#endif

    target_point.x += 0.7;

#if 1
    if ( wm.ourDefenseLineX() < target_point.x )
    {
        const PlayerPtrCont::const_iterator o_end = wm.opponentsFromSelf().end();
        for ( PlayerPtrCont::const_iterator o = wm.opponentsFromSelf().begin();
              o != o_end;
              ++o )
        {
            if ( (*o)->isGhost() ) continue;
            if ( (*o)->posCount() >= 10 ) continue;
            if ( wm.ourDefenseLineX() < (*o)->pos().x
                 && (*o)->pos().x < target_point.x
                 )
            {
                target_point.x = std::max( home_pos.x - 2.0, (*o)->pos().x - 0.5 );
            }
        }
    }
#endif

    double dash_power = Strategy::get_defender_dash_power( wm, target_point );
    double dist_thr = std::fabs( wm.ball().pos().x - wm.self().pos().x ) * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().addMessage( "SB:Def:Normal" );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    doGoToPoint( agent, target_point, dist_thr, dash_power, 12.0 );

    agent->setNeckAction( new Neck_CheckBallOwner() );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SideBackDefensiveMove::doGoToPoint( rcsc::PlayerAgent * agent,
                                        const rcsc::Vector2D & target_point,
                                        const double & dist_thr,
                                        const double & dash_power,
                                        const double & dir_thr )
{
    if ( Body_GoToPoint( target_point, dist_thr, dash_power,
                         1, false, true, dir_thr
                          ).execute( agent ) )
    {
        agent->debugClient().addMessage( "Go%.1f", dash_power );
        dlog.addText( Logger::ROLE,
                      __FILE__": GoToPoint (%.1f %.1f) dash_power=%.1f dist_thr=%.2f",
                      target_point.x, target_point.y,
                      dash_power,
                      dist_thr );
        return;
    }

    // already there
    // turn to somewhere

    const WorldModel & wm = agent->world();

    Vector2D ball_next = wm.ball().pos() + wm.ball().vel();
    Vector2D my_final = wm.self().inertiaFinalPoint();
    AngleDeg ball_angle = ( ball_next - my_final ).th();

    AngleDeg target_angle;
    if ( ball_next.x < -30.0 )
    {
        target_angle = ball_angle + 90.0;
        if ( ball_next.x < -45.0 )
        {
            if ( target_angle.degree() < 0.0 )
            {
                target_angle += 180.0;
            }
        }
        else if ( target_angle.degree() > 0.0 )
        {
            target_angle += 180.0;
        }
    }
    else
    {
        target_angle = ball_angle + 90.0;
        if ( ball_next.x > my_final.x + 15.0 )
        {
            if ( target_angle.abs() > 90.0 )
            {
                target_angle += 180.0;
            }
        }
        else
        {
            if ( target_angle.abs() < 90.0 )
            {
                target_angle += 180.0;
            }
        }
    }

    Body_TurnToAngle( target_angle ).execute( agent );

    agent->debugClient().addMessage( "TurnTo%.0f",
                                     target_angle.degree() );
    dlog.addText( Logger::ROLE,
                  __FILE__": TurnToAngle %.1f",
                  target_angle.degree() );
}
