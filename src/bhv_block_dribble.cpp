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

#include "bhv_block_dribble.h"

#include "bhv_basic_tackle.h"

#include "body_intercept2008.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_player_or_scan.h>
#include <rcsc/action/neck_turn_to_ball.h>
#include <rcsc/action/neck_turn_to_ball_and_player.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/soccer_intention.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_type.h>
#include <rcsc/geom/line_2d.h>
#include <rcsc/geom/ray_2d.h>
#include <rcsc/math_util.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/

class IntentionBlockDribble
    : public SoccerIntention {
private:
    const Vector2D M_target_point;
    int M_step;
    const Vector2D M_start_point;
    const int M_opp_number;
    const AngleDeg M_opp_angle; //!< last seen opponent angle
    GameTime M_last_execute_time;

public:

    IntentionBlockDribble( const Vector2D & target_point,
                           const int step,
                           const Vector2D & start_point,
                           const int opp_number,
                           const AngleDeg & opp_angle,
                           const GameTime & start_time )
        : M_target_point( target_point )
        , M_step( step )
        , M_start_point( start_point )
        , M_opp_number( opp_number )
        , M_opp_angle( opp_angle )
        , M_last_execute_time( start_time )
      { }

    bool finished( const PlayerAgent * agent );

    bool execute( PlayerAgent * agent );

};

/*-------------------------------------------------------------------*/
/*!

*/
bool
IntentionBlockDribble::finished( const PlayerAgent * agent )
{
    if ( M_step == 0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished() empty queue" );
        return true;
    }

    const WorldModel & wm = agent->world();

    if ( M_last_execute_time.cycle() + 1 != wm.time().cycle() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished(). last execute time is illegal" );
        return true;
    }

    const PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();

    if ( ! fastest_opp )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished() no interceptor opponent" );
        return true;
    }

    if ( M_opp_number != fastest_opp->unum() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished() target opponent was changed from %d to %d",
                      M_opp_number, fastest_opp->unum() );
        return true;
    }

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( self_min <= opp_min - 2 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished() can get the ball faster than opponent" );
        return true;
    }

    const AngleDeg opp_angle = ( fastest_opp->bodyCount() <= 1
                                 ? fastest_opp->body()
                                 : 180.0 );

    if ( ( opp_angle - M_opp_angle ).abs() > 3.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished() opponent angle changed" );
        return true;
    }

    const Vector2D opp_reach_pos = wm.ball().inertiaPoint( opp_min );

    if ( wm.self().pos().x < opp_reach_pos.x - 0.5
         && std::fabs( wm.self().pos().y - opp_reach_pos.y ) < 0.5 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished() reached" );
        return true;
    }

    if ( wm.self().pos().x < opp_reach_pos.x - 0.5
         && ( M_start_point.y - opp_reach_pos.y ) * ( wm.self().pos().y - opp_reach_pos.y ) < 0.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished() dash over. start point=(%.1f %.1f) opp_reach_pos=(%.1f %.1f)",
                      M_start_point.x, M_start_point.y,
                      opp_reach_pos.x, opp_reach_pos.y );
        return true;
    }

    //     const Ray2D opp_ray( opp_reach_pos, opp_angle );

    //     const Vector2D my_inertia_pos
    //         = wm.self().playerType().inertiaFinalPoint( wm.self().pos(),
    //                                                     wm.self().vel() );

    //     const AngleDeg dash_angle = ( M_target_point - my_inertia_pos ).th();
    //     const Line2D my_line( my_inertia_pos, dash_angle );
    //     Vector2D intersection = opp_ray.intersection( my_line );

    //     if ( ! intersection )
    //     {
    //         dlog.addText( Logger::TEAM,
    //                       __FILE__" finished() no interesection with block target point" );
    //         return true;
    //     }

    //     double opp_dist = opp_reach_pos.dist( intersection );
    //     double my_dist = my_inertia_pos.dist( intersection );

    //     if ( my_dist >= opp_dist * 1.1 )
    //     {
    //         dlog.addText( Logger::TEAM,
    //                       __FILE__" finished() my_dist = %.2f opp_dist = %.2f ",
    //                       my_dist, opp_dist );
    //         return true;
    //     }

    return false;
}


/*-------------------------------------------------------------------*/
/*!

*/
bool
IntentionBlockDribble::execute( PlayerAgent * agent )
{
    const PlayerObject * fastest_opp = agent->world().interceptTable()->fastestOpponent();

    if ( ! fastest_opp )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": intention. no interceptor opponent" );
        return false;
    }

    M_last_execute_time = agent->world().time();
    --M_step;

    if ( Body_GoToPoint( M_target_point, 0.3,
                         ServerParam::i().maxPower()
                         ).execute( agent ) )
    {
        // go to
        agent->debugClient().addMessage( "I_BlockDribGoTo" );
        agent->debugClient().setTarget( M_target_point );
        dlog.addText( Logger::TEAM,
                      __FILE__": intention. go to (%.1f %1.f) ",
                      M_target_point.x, M_target_point.y );
    }
    else
    {
        // intercept ??
        if ( Body_Intercept2008().execute( agent ) )
        {
            agent->debugClient().addMessage( "I_BlockDribGo%d", M_step );
            dlog.addText( Logger::TEAM,
                          __FILE__": intention. try intercept " );
        }
        else
        {
            Body_TurnToPoint( fastest_opp->pos() + fastest_opp->vel()
                              ).execute( agent );

            agent->debugClient().addMessage( "I_BlockDribTurn" );
            dlog.addText( Logger::TEAM,
                          __FILE__": intention. turn to opp pos " );
        }
    }

    if ( agent->world().ball().posCount() <= 1 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": intention. turn_neck to player " );
        agent->setNeckAction( new Neck_TurnToPlayerOrScan( fastest_opp, -1 ) );
    }
    else
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": intention. turn_neck to ball " );
        agent->setNeckAction( new Neck_TurnToBall() );
    }

    return true;
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_BlockDribble::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    // tackle
    if ( Bhv_BasicTackle( 0.8, 90.0 ).execute( agent ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": execute(). tackle" );
        return true;
    }

    const int opp_min = wm.interceptTable()->opponentReachCycle();
    const PlayerObject * opp = wm.interceptTable()->fastestOpponent();

    if ( ! opp )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": execute(). no fastest opponent" );
        return false;
    }

    const PlayerType & self_type = wm.self().playerType();
    const double control_area = self_type.kickableArea() - 0.3;
    const double max_moment = ServerParam::i().maxMoment();// * ( 1.0 - ServerParam::i().playerRand() );

    const double first_my_speed = wm.self().vel().r();
    const Vector2D first_ball_pos = wm.ball().inertiaPoint( opp_min );

    Vector2D opp_target( -44.0, 0.0 );
    if ( first_ball_pos.x > -25.0
         && first_ball_pos.absY() > 20.0 )
    {
        opp_target.y = ( first_ball_pos.y > 0.0
                         ? 20.0
                         : -20.0 );
    }

    const Vector2D first_ball_vel
        = ( opp_target - first_ball_pos ).setLengthVector( opp->playerTypePtr()->realSpeedMax() );


    dlog.addText( Logger::TEAM,
                  __FILE__": execute(). virtual intercept. opp_min=%d bpos=(%.1f %.1f) bvel=(%.1f %.1f)",
                  opp_min,
                  first_ball_pos.x, first_ball_pos.y,
                  first_ball_vel.x, first_ball_vel.y );

    Vector2D target_point = Vector2D::INVALIDATED;
    int total_step = -1;

    for ( int cycle = 1; cycle <= 20; ++cycle )
    {
        int n_turn = 0;
        int n_dash = 0;
        AngleDeg dash_angle = wm.self().body();

        Vector2D ball_pos = inertia_n_step_point( first_ball_pos,
                                                  first_ball_vel,
                                                  cycle,
                                                  ServerParam::i().ballDecay() );
        Vector2D my_pos = wm.self().inertiaPoint( cycle + opp_min );
        Vector2D target_rel = ball_pos - my_pos;
        double target_dist = target_rel.r();
        AngleDeg target_angle = target_rel.th();

        double stamina = wm.self().stamina();
        double effort = wm.self().effort();
        double recovery = wm.self().recovery();

        // turn
        double angle_diff = ( target_angle - wm.self().body() ).abs();
        double turn_margin = 180.0;
        if ( control_area < target_dist )
        {
            turn_margin = AngleDeg::asin_deg( control_area / target_dist );
        }
        turn_margin = std::max( turn_margin, 15.0 );
        double my_speed = first_my_speed;
        while ( angle_diff > turn_margin )
        {
            angle_diff -= self_type.effectiveTurn( max_moment, my_speed );
            my_speed *= self_type.playerDecay();
            ++n_turn;
        }

        if ( n_turn > cycle + opp_min )
        {
            continue;
        }

        if ( n_turn > 0 )
        {
            angle_diff = std::max( 0.0, angle_diff );
            dash_angle = target_angle;
            if ( ( target_angle - wm.self().body() ).degree() > 0.0 )
            {
                dash_angle += angle_diff;
            }
            else
            {
                dash_angle += angle_diff;
            }

            self_type.getOneStepStaminaComsumption();
        }

        // dash
        n_dash = self_type.cyclesToReachDistance( target_dist );
        self_type.getOneStepStaminaComsumption();
        if ( stamina < ServerParam::i().recoverDecThrValue() + 300.0 )
        {
            continue;
        }

        if ( n_turn + n_dash <= cycle + opp_min )
        {
            target_point = ball_pos;
            total_step = n_turn + n_dash;
            break;
        }
    }

    if ( ! target_point.isValid() )
    {
        return false;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": execute(). virtual intercept. pos=(%.1f %.1f) total_step=%d",
                  target_point.x, target_point.y,
                  total_step );

    if ( ! Body_GoToPoint( target_point, 0.5,
                           ServerParam::i().maxPower()
                           ).execute( agent ) )
    {
        return false;
    }
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addMessage( "BlockDribVirtualIntercept" );
    agent->setNeckAction( new rcsc::Neck_TurnToBall() );
    return true;
}
