// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA, Hiroki Shimora

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

#include <rcsc/geom/line_2d.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_stop_dash.h>

#include "body_intercept2008.h"
#include "neck_chase_ball.h"

#include <rcsc/action/neck_turn_to_ball.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include "bhv_savior_chase_ball.h"
#include "body_go_to_point2008.h"
#include "bhv_savior.h"

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SaviorChaseBall::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_SaviorChaseBall" );

    const WorldModel & wm = agent->world();
    const ServerParam & param = ServerParam::i();

    ////////////////////////////////////////////////////////////////////////
    // get active interception catch point

    int self_reach_cycle = wm.interceptTable()->selfReachCycle();

    const Vector2D my_int_pos = wm.ball().inertiaPoint( self_reach_cycle );
    dlog.addText( Logger::TEAM,
                  __FILE__": execute. intercept point=(%.2f %.2f)",
                  my_int_pos.x, my_int_pos.y );

    const double max_ball_error = 0.5;

    if ( ( my_int_pos.x
           > param.ourPenaltyAreaLineX() + param.ballSize() * 2 - max_ball_error
           || ( my_int_pos.x
                > param.ourTeamGoalLineX() - max_ball_error
                && my_int_pos.absY()
                > param.penaltyAreaHalfWidth() + param.ballSize() * 2 - max_ball_error ) )
         && ( ! wm.existKickableTeammate() || wm.existKickableOpponent() ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": intercept point is forward,"
                      "do normal intercept" );

        if ( rcsc::Body_Intercept2008().execute( agent ) )
        {
            return true;
        }
    }


    double pen_thr = wm.ball().distFromSelf() * 0.1 + 1.0;
    if ( pen_thr < 1.0 ) pen_thr = 1.0;

    //----------------------------------------------------------
    const Line2D ball_line( wm.ball().pos(), wm.ball().vel().th() );
    const Line2D defend_line( Vector2D( wm.self().pos().x, -10.0 ),
                              Vector2D( wm.self().pos().x, 10.0 ) );

    if ( my_int_pos.x > - ServerParam::i().pitchHalfLength() - 1.0
         && my_int_pos.x < ServerParam::i().ourPenaltyAreaLineX() - pen_thr
         && my_int_pos.absY() < ServerParam::i().penaltyAreaHalfWidth() - pen_thr )
    {
        bool save_recovery = false;
        if ( ball_line.dist( wm.self().pos() ) < ServerParam::i().catchableArea() )
        {
            save_recovery = true;
        }
        dlog.addText( Logger::TEAM,
                      __FILE__": execute normal intercept" );
        agent->debugClient().addMessage( "Intercept%d", __LINE__ );
        Body_Intercept2008( save_recovery ).execute( agent );
        this->doNeckAction( agent );
        return true;
    }

    int opp_min_cyc = wm.interceptTable()->opponentReachCycle();

    Vector2D intersection = ball_line.intersection( defend_line );

    if ( ! intersection.isValid()
         || intersection.absY() > ServerParam::i().goalHalfWidth() + 3.0
         )
    {
        if ( ! wm.existKickableOpponent() )
        {
            if ( self_reach_cycle <= opp_min_cyc + 2
                 && my_int_pos.x > -ServerParam::i().pitchHalfLength() - 2.0
                 && my_int_pos.x < ServerParam::i().ourPenaltyAreaLineX() - pen_thr
                 && my_int_pos.absY() < ServerParam::i().penaltyAreaHalfWidth() - pen_thr
                 )
            {
                if ( Body_Intercept2008( false ).execute( agent ) )
                {
                    dlog.addText( Logger::TEAM,
                                  __FILE__": execute normal interception" );
                    agent->debugClient().addMessage( "Intercept%d", __LINE__ );
                    this->doNeckAction( agent );
                    return true;
                }
            }

            dlog.addText( Logger::TEAM,
                          __FILE__": execute. ball vel has same slope to my body??"
                          " myvel-ang=%f body=%f. go to ball direct",
                          wm.ball().vel().th().degree(),
                          wm.self().body().degree() );
            // ball vel angle is same to my body angle
            agent->debugClient().addMessage( "GoToCatch%d", __LINE__ );
            doGoToCatchPoint( agent, wm.ball().pos() );
            return true;
        }
    }

    //----------------------------------------------------------
    // body is already face to intersection
    // only dash to intersection

    // check catch point
    if ( intersection.absX() > ( ServerParam::i().pitchHalfLength())
         || intersection.x > ( ServerParam::i().ourPenaltyAreaLineX())
         || intersection.absY() > ( ServerParam::i().penaltyAreaHalfWidth())
         )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": execute intersection(%.2f, %.2f) over range.",
                      intersection.x, intersection.y );
        if ( Body_Intercept2008( false ).execute( agent ) )
        {
            agent->debugClient().addMessage( "Intercept%d", __LINE__ );
            this->doNeckAction( agent );
            return true;
        }
        else
        {
            return false;
        }
    }

    //----------------------------------------------------------
    // check already there
    const Vector2D my_inertia_final_pos
        = wm.self().pos()
        + wm.self().vel()
        / (1.0 - wm.self().playerType().playerDecay());
    double dist_thr = 0.2 + wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    // if already intersection point stop dash
    if ( my_inertia_final_pos.dist( intersection ) < dist_thr )
    {
        agent->debugClient().addMessage( "StopForChase" );
        Body_StopDash( false ).execute( agent );
        this->doNeckAction( agent );
        return true;
    }

    //----------------------------------------------------------
    // forward or backward

    dlog.addText( Logger::TEAM,
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

    agent->debugClient().addMessage( "GoToCatch%d", __LINE__ );
    doGoToCatchPoint( agent, intersection );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SaviorChaseBall::doGoToCatchPoint( PlayerAgent * agent,
                                       const Vector2D & target_point )
{
    const WorldModel & wm = agent->world();

#if 0
    Vector2D rel = target_point - wm.self().pos();
    rel.rotate( - wm.self().body() );
    AngleDeg rel_angle = rel.th();
    const double angle_buf
        = std::fabs( AngleDeg::atan2_deg( wm.self().catchableArea() * 0.9,
                                          rel.r() ) );

    dlog.addText( Logger::TEAM,
                  __FILE__": GoToCatchPoint. (%.1f, %.1f). angle_diff=%.1f. angle_buf=%.1f",
                  target_point.x, target_point.y,
                  rel_angle.degree(), angle_buf );

    agent->debugClient().setTarget( target_point );

    double dash_power;

    // forward dash
    if ( rel_angle.abs() < angle_buf )
    {
        dash_power = std::min( wm.self().stamina(),
                               ServerParam::i().maxPower() );
        dlog.addText( Logger::TEAM,
                      __FILE__": forward dash" );
        agent->debugClient().addMessage( "GoToCatch:Forward" );
        agent->doDash( dash_power );
    }
    // back dash
    else if ( rel_angle.abs() > 180.0 - angle_buf )
    {
        dash_power = std::min( wm.self().stamina(),
                               ServerParam::i().maxPower() * 2.0 );
        dlog.addText( Logger::TEAM,
                      __FILE__": back dash" );
        agent->debugClient().addMessage( "GoToCatch:Back" );
        agent->doDash( -0.5 * dash_power );
    }
    // forward dash turn
    else if ( rel_angle.abs() < 90.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": turn %.1f for forward dash",
                      rel_angle.degree() );
        agent->debugClient().addMessage( "GoToCatch:F-Turn" );
        agent->doTurn( rel_angle );
    }
    else
    {
        rel_angle -= 180.0;
        dlog.addText( Logger::TEAM,
                      __FILE__": turn %.1f for back dash",
                      rel_angle.degree() );
        agent->debugClient().addMessage( "GoToCatch:B-Turn" );
        agent->doTurn( rel_angle );
    }
#else
    {
        Vector2D rel = target_point - wm.self().pos();
        rel.rotate( - wm.self().body() );
        AngleDeg rel_angle = rel.th();
        const double angle_buf
            = std::fabs( AngleDeg::atan_deg( rel.r() ) );

        dlog.addText( Logger::TEAM,
                      __FILE__": GoToCatchPoint. (%.1f, %.1f). angle_diff=%.1f. angle_buf=%.1f",
                      target_point.x, target_point.y,
                      rel_angle.degree(), angle_buf );

        agent->debugClient().setTarget( target_point );
    }


    rcsc::Vector2D absolute_diff = target_point - wm.self().inertiaPoint( 1 );

    rcsc::Body_GoToPoint2008( target_point,
#if 0
                              1.8,
#elif 0
                              2.5,
#else
                              2.0,
#endif
                              rcsc::ServerParam::i().maxPower(),
                              true,
                              true ).execute( agent )
    || rcsc::Body_GoToPoint2008( target_point,
                                 1.0,
                                 rcsc::ServerParam::i().maxPower(),
                                 true,
                                 true ).execute( agent );
#endif

    this->doNeckAction( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SaviorChaseBall::doNeckAction( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();

    const int self_reach_cycle = wm.interceptTable()->selfReachCycle();
    const int opp_reach_cycle = wm.interceptTable()->opponentReachCycle();

    const Vector2D ball_pos = wm.ball().inertiaPoint( self_reach_cycle );

    if ( self_reach_cycle + 2 < opp_reach_cycle
         && ball_pos.x >= rcsc::ServerParam::i().ourPenaltyAreaLineX() )
    {
        agent->setNeckAction( new Neck_TurnToBallOrScan() );
    }
    else
    {
        agent->setNeckAction( new rcsc_ext::Neck_ChaseBall() );
    }
}
