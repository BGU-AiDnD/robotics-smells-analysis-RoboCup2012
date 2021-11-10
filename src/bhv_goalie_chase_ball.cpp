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

#include "bhv_goalie_chase_ball.h"

#include "bhv_goalie_basic_move.h"
#include "bhv_danger_area_tackle.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_stop_dash.h>
#include <rcsc/action/body_intercept2009.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/line_2d.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!
  execute action
*/
bool
Bhv_GoalieChaseBall::execute( rcsc::PlayerAgent * agent )
{
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Bhv_GoalieChaseBall" );
    //----------------------------------------------------------
    // It is necessary to deal with the situation
    //  which opponent keeps the ball or dribbles.

    /*
      const double opp_to_ball_dist
      = ( agent->world().getOpponentsFromBall().empty()
      ? 1000.0
      : agent->world().getOpponentsFromBall().front()->distFromBall() );
      if ( opp_to_ball_dist < rcsc::ServerParam::i().defaultKickableArea() + 0.2 )
      {
      rcsc::Vector2D target_point
      = agent->world().getOpponentsFromBall().front()->pos();
      target_point += agent->world().getOpponentsFromBall().front()->vel();
      doGoToCatchPoint(act, target_point);
      return true;
      }
    */


    //////////////////////////////////////////////////////////////////
    // tackle
    if ( Bhv_DangerAreaTackle().execute( agent ) )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": tackle" );
        return true;
    }

    const rcsc::WorldModel & wm = agent->world();

    ////////////////////////////////////////////////////////////////////////
    // get active interception catch point

    rcsc::Vector2D my_int_pos = wm.ball().inertiaPoint( wm.interceptTable()->selfReachCycle() );
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": execute. intercept point=(%.2f %.2f)",
                        my_int_pos.x, my_int_pos.y );

    double pen_thr = wm.ball().distFromSelf() * 0.1 + 1.0;
    if ( pen_thr < 1.0 ) pen_thr = 1.0;

    //----------------------------------------------------------
    const rcsc::Line2D ball_line( wm.ball().pos(), wm.ball().vel().th() );
    const rcsc::Line2D defend_line( rcsc::Vector2D( wm.self().pos().x, -10.0 ),
                                    rcsc::Vector2D( wm.self().pos().x, 10.0 ) );

    if ( my_int_pos.x > - rcsc::ServerParam::i().pitchHalfLength() - 1.0
         && my_int_pos.x < rcsc::ServerParam::i().ourPenaltyAreaLineX() - pen_thr
         && my_int_pos.absY() < rcsc::ServerParam::i().penaltyAreaHalfWidth() - pen_thr )
    {
        bool save_recovery = false;
        if ( ball_line.dist( wm.self().pos() ) < rcsc::ServerParam::i().catchableArea() )
        {
            save_recovery = true;
        }
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": execute normal intercept" );
        agent->debugClient().addMessage( "Intercept(0)" );
        rcsc::Body_Intercept2009( save_recovery ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_TurnToBall() );
        return true;
    }

    int self_goalie_min = wm.interceptTable()->selfReachCycle();
    int opp_min_cyc = wm.interceptTable()->opponentReachCycle();

    rcsc::Vector2D intersection = ball_line.intersection( defend_line );
    if ( ! intersection.isValid()
         || intersection.absY() > rcsc::ServerParam::i().goalHalfWidth() + 3.0
         )
    {
        if ( ! wm.existKickableOpponent() )
        {
            if ( self_goalie_min <= opp_min_cyc + 2
                 && my_int_pos.x > -rcsc::ServerParam::i().pitchHalfLength() - 2.0
                 && my_int_pos.x < rcsc::ServerParam::i().ourPenaltyAreaLineX() - pen_thr
                 && my_int_pos.absY() < rcsc::ServerParam::i().penaltyAreaHalfWidth() - pen_thr
                 )
            {
                if ( rcsc::Body_Intercept2009( false ).execute( agent ) )
                {
                    rcsc::dlog.addText( rcsc::Logger::TEAM,
                                        __FILE__": execute normal interception" );
                    agent->debugClient().addMessage( "Intercept(1)" );
                    agent->setNeckAction( new rcsc::Neck_TurnToBall() );
                    return true;
                }
            }

            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": execute. ball vel has same slope to my body??"
                                " myvel-ang=%f body=%f. go to ball direct",
                                wm.ball().vel().th().degree(),
                                wm.self().body().degree() );
            // ball vel angle is same to my body angle
            agent->debugClient().addMessage( "GoToCatch(1)" );
            doGoToCatchPoint( agent, wm.ball().pos() );
            return true;
        }
    }

    //----------------------------------------------------------
    // body is already face to intersection
    // only dash to intersection

    // check catch point
    if ( intersection.absX() > ( rcsc::ServerParam::i().pitchHalfLength())
         || intersection.x > ( rcsc::ServerParam::i().ourPenaltyAreaLineX())
         || intersection.absY() > ( rcsc::ServerParam::i().penaltyAreaHalfWidth())
         )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": execute intersection(%.2f, %.2f) over range.",
                            intersection.x, intersection.y );
        if ( rcsc::Body_Intercept2009( false ).execute( agent ) )
        {
            agent->debugClient().addMessage( "Intercept(2)" );
            agent->setNeckAction( new rcsc::Neck_TurnToBall() );
            return true;
        }
        else
        {
            return Bhv_GoalieBasicMove().execute( agent );
        }
    }

    //----------------------------------------------------------
    // check already there
    const rcsc::Vector2D my_inertia_final_pos
        = wm.self().pos()
        + wm.self().vel()
        / (1.0 - wm.self().playerType().playerDecay());
    double dist_thr = 0.2 + wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    // if already intersection point stop dash
    if ( my_inertia_final_pos.dist( intersection ) < dist_thr )
    {
        agent->debugClient().addMessage( "StopForChase" );
        rcsc::Body_StopDash( false ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_TurnToBall() );
        return true;
    }

    //----------------------------------------------------------
    // forward or backward

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": slide move. point=(%.1f, %.1f)",
                        intersection.x, intersection.y );
    if ( wm.ball().pos().x > -35.0 )
    {
        if ( wm.ball().pos().y * intersection.y < 0.0 ) // opposite side
        {
            intersection.y = 0.0;
        }
        else
        {
            intersection.y *= 0.5;
        }
    }

    agent->debugClient().addMessage( "GoToCatch(2)" );
    doGoToCatchPoint( agent, intersection );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
Bhv_GoalieChaseBall::doGoToCatchPoint( rcsc::PlayerAgent * agent,
                                       const rcsc::Vector2D & target_point )
{
    const WorldModel & wm = agent->world();

    double dash_power;

    rcsc::Vector2D rel = target_point - wm.self().pos();
    rel.rotate( - wm.self().body() );
    rcsc::AngleDeg rel_angle = rel.th();
    const double angle_buf
        = std::fabs( rcsc::AngleDeg::atan_deg( rel.r() ) );

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": GoToCatchPoint. (%.1f, %.1f). angle_diff=%.1f. angle_buf=%.1f",
                        target_point.x, target_point.y,
                        rel_angle.degree(), angle_buf );

    agent->debugClient().setTarget( target_point );

    // forward dash
    if ( rel_angle.abs() < angle_buf )
    {
        dash_power = std::min( wm.self().stamina(),
                               rcsc::ServerParam::i().maxPower() );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": forward dash" );
        agent->debugClient().addMessage( "GoToCatch:Forward" );
        agent->doDash( dash_power );
    }
    // back dash
    else if ( rel_angle.abs() > 180.0 - angle_buf )
    {
        dash_power = std::min( wm.self().stamina(),
                               rcsc::ServerParam::i().maxPower() * 2.0 );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": back dash" );
        agent->debugClient().addMessage( "GoToCatch:Back" );
        agent->doDash( -0.5 * dash_power );
    }
    // forward dash turn
    else if ( rel_angle.abs() < 90.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": turn %.1f for forward dash",
                            rel_angle.degree() );
        agent->debugClient().addMessage( "GoToCatch:F-Turn" );
        agent->doTurn( rel_angle );
    }
    else
    {
        rel_angle -= 180.0;
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": turn %.1f for back dash",
                            rel_angle.degree() );
        agent->debugClient().addMessage( "GoToCatch:B-Turn" );
        agent->doTurn( rel_angle );
    }

    agent->setNeckAction( new rcsc::Neck_TurnToBall() );
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_GoalieChaseBall::is_ball_chase_situation( const rcsc::PlayerAgent* agent )
{
    const rcsc::WorldModel & wm = agent->world();
    if ( wm.gameMode().type() != rcsc::GameMode::PlayOn )
    {
        return false;
    }

    int self_min = wm.interceptTable()->selfReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    ////////////////////////////////////////////////////////////////////////
    // ball is in very dangerous area
    const rcsc::Vector2D ball_next_pos = wm.ball().pos() + wm.ball().vel();
    if ( ball_next_pos.x < -rcsc::ServerParam::i().pitchHalfLength() + 8.0
         && ball_next_pos.absY() < rcsc::ServerParam::i().goalHalfWidth() + 3.0 )
    {
        // exist kickable teammate
        // avoid back pass
        if ( wm.existKickableTeammate() )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": danger area. exist kickable teammate?" );
            return false;
        }
        else if ( wm.ball().distFromSelf() < 3.0
                  && self_min <= 3 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": danger area. ball is very near." );
            return true;
        }
        else if ( self_min > opp_min + 3
                  && opp_min < 7 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": danger area. opponent may get tha ball faster than me" );
            return false;
        }
        else
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": danger area. chase ball" );
            return true;
        }
    }

    ////////////////////////////////////////////////////////////////////////
    // check shoot moving
    if ( is_ball_shoot_moving( agent )
         && self_min < opp_min )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": shoot moving. chase ball" );
        return true;
    }

    ////////////////////////////////////////////////////////////////////////
    // get active interception catch point

    const rcsc::Vector2D my_int_pos = wm.ball().inertiaPoint( wm.interceptTable()->selfReachCycle() );

    double pen_thr = wm.ball().distFromSelf() * 0.1 + 1.0;
    if ( pen_thr < 1.0 ) pen_thr = 1.0;
    if ( my_int_pos.absY() > rcsc::ServerParam::i().penaltyAreaHalfWidth() - pen_thr
         || my_int_pos.x > rcsc::ServerParam::i().ourPenaltyAreaLineX() - pen_thr )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": intercept point is out of penalty" );
        return false;
    }

    ////////////////////////////////////////////////////////////////////////
    // Now, I can chase the ball
    // check the ball possessor

    if ( wm.existKickableTeammate()
         && ! wm.existKickableOpponent() )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": exist kickable player" );
        return false;
    }

    if ( opp_min <= self_min - 2 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": opponent reach the ball faster than me" );
        return false;
    }

    const double my_dist_to_catch = wm.self().pos().dist( my_int_pos );

    double opp_min_dist = 10000.0;
    wm.getOpponentNearestTo( my_int_pos, 30, &opp_min_dist );

    if ( opp_min_dist < my_dist_to_catch - 2.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": opponent is nearer than me. my_dist=%.2f  opp_dist=%.2f",
                            my_dist_to_catch, opp_min_dist );
        return false;
    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": exist interception point. try chase." );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_GoalieChaseBall::is_ball_shoot_moving( const rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel& wm = agent->world();

    if ( wm.ball().distFromSelf() > 30.0 )
    {
        return false;
    }
#if 1
    if ( wm.ball().pos().x > -34.5 )
    {
        return false;
    }
#endif

    // check opponent kicker
    if ( wm.existKickableOpponent() )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": check shoot moving. opponent kickable " );
        return false;
    }
    else if ( wm.existKickableTeammate() )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": check shoot moving. teammate kickable" );
        return false;
    }

    if ( wm.ball().vel().absX() < 0.1 )
    {
        if ( wm.ball().pos().x < -46.0
             && wm.ball().pos().absY() < rcsc::ServerParam::i().goalHalfWidth() + 2.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": check shoot moving. bvel.x(%f) is ZERO. but near to goal",
                                wm.ball().vel().x );
            return true;
        }
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": check shoot moving. bvel,x is small" );
        return false;
    }


    const rcsc::Line2D ball_line( wm.ball().pos(), wm.ball().vel().th() );
    const double intersection_y = ball_line.getY( -rcsc::ServerParam::i().pitchHalfLength() );

    if ( std::fabs( ball_line.getB() ) > 0.1
         && std::fabs( intersection_y ) < rcsc::ServerParam::i().goalHalfWidth() + 2.0 )
    {
        if ( wm.ball().pos().x < -40.0
             && wm.ball().pos().absY() < 15.0 )
        {
            const rcsc::Vector2D end_point
                = wm.ball().pos()
                + wm.ball().vel() / ( 1.0 - rcsc::ServerParam::i().ballDecay());
            if ( wm.ball().vel().r() > 0.5 // 1.0
                 && end_point.x < -rcsc::ServerParam::i().pitchHalfLength() + 2.0 )
            {
                rcsc::dlog.addText( rcsc::Logger::TEAM,
                                    __FILE__": shoot to Y(%.1f). ball_line a=%.1f, b=%.1f, c=%.1f",
                                    intersection_y,
                                    ball_line.getA(),
                                    ball_line.getB(),
                                    ball_line.getC() );
                return true;
            }
        }
    }


    return false;
}
