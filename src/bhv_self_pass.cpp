// -*-c++-*-

/*!
  \file bhv_self_pass.cpp
  \brief pass to self action
*/

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "bhv_self_pass.h"

#include "body_smart_kick.h"
#include "body_intercept2008.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/soccer_intention.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/ray_2d.h>
#include <rcsc/soccer_math.h>
#include <rcsc/math_util.h>
#include <rcsc/timer.h>

#include "neck_offensive_intercept_neck.h"

//#define DEBUG

/*-------------------------------------------------------------------*/
class IntentionSelfPass
    : public rcsc::SoccerIntention {
private:

    const rcsc::Vector2D M_target_point;
    int M_step;
    int M_count;

public:

    IntentionSelfPass( const rcsc::Vector2D & target_point,
                       const int step )
        : M_target_point( target_point )
        , M_step( step )
        , M_count( 0 )
      { }

    bool finished( const rcsc::PlayerAgent * agent );

    bool execute( rcsc::PlayerAgent * agent );

};

/*-------------------------------------------------------------------*/
/*!

*/
bool
IntentionSelfPass::finished( const rcsc::PlayerAgent * agent )
{
    if ( M_step <= 0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": finished() empty queue" );
        return true;
    }

    if ( M_count >= 1
         && agent->world().self().isKickable() )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": finished() kickable" );
        return true;
    }

    if ( agent->world().existKickableOpponent()
         || agent->world().existKickableTeammate() )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": finished() exist kickable player" );
        return true;
    }

    if ( agent->world().ball().pos().x < agent->world().self().pos().x )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": finished() ball is back." );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
IntentionSelfPass::execute( rcsc::PlayerAgent * agent )
{
    --M_step;
    ++M_count;

    if ( M_count <= 3 )
    {
        if ( rcsc::Body_GoToPoint( M_target_point, 0.3,
                                   rcsc::ServerParam::i().maxPower()
                                   ).execute( agent ) )
        {
            agent->setNeckAction( new rcsc::Neck_ScanField() );

            agent->debugClient().addMessage( "I_SelfPassGoTo" );
            agent->debugClient().setTarget( M_target_point );
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": intention. go to (%.1f %1.f) ",
                                M_target_point.x, M_target_point.y );
            return true;
        }
    }
    else
    {
        rcsc::Body_Intercept2008().execute( agent );

#if 0
        int self_min = agent->world().interceptTable()->selfReachCycle();
        int opp_min = agent->world().interceptTable()->opponentReachCycle();
        if ( self_min == 4 && opp_min >= 2 )
        {
            agent->setViewAction( new rcsc::View_Wide() );
        }
        else if ( self_min == 3 && opp_min >= 2 )
        {
            agent->setViewAction( new rcsc::View_Normal() );
        }
        agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
#else
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
#endif

        agent->debugClient().addMessage( "I_SelfPassIntercept" );
        agent->debugClient().setTarget( M_target_point );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": intention. intercept (%.1f %1.f) ",
                            M_target_point.x, M_target_point.y );
            return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_SelfPass::execute( rcsc::PlayerAgent * agent )
{
    if ( ! agent->world().self().isKickable() )
    {
        std::cerr << __FILE__ << ": " << __LINE__
                  << " not ball kickable!"
                  << std::endl;
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__":  not kickable" );
        return false;
    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Bhv_SelfPass" );

    const rcsc::WorldModel & wm = agent->world();

    rcsc::MSecTimer timer;

    const rcsc::Ray2D body_ray( wm.self().pos(), wm.self().body() );
    const double goal_area_x
        = rcsc::ServerParam::i().pitchHalfLength()
        - rcsc::ServerParam::i().goalAreaLength() * 0.5;
    const rcsc::Rect2D target_area = rcsc::Rect2D::from_center( goal_area_x,
                                                                0.0,
                                                                rcsc::ServerParam::i().goalAreaLength(),
                                                                30.0 );

    if ( wm.self().body().abs() < 15.0
         || target_area.intersection( body_ray, NULL, NULL ) > 0 )
    {
        agent->debugClient().addRectangle( target_area );
        if ( doKickDashes( agent ) )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": self_pass_elapsed(1)=%.3f [ms]",
                                timer.elapsedReal() );
            return true;
        }
    }
#if 0
    //for ( double r = 0.0; r < 1.2; r += 0.1 )
    for ( double r = 0.9; r >= 0.0; r -= 0.1 )
    {
        rcsc::Vector2D target_point( 42.0, wm.self().pos().y * r );
        if ( doKickTurnDashes( agent, target_point ) )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": self_pass_elapsed(2)=%.3f [ms]",
                                timer.elapsedReal() );
            return true;
        }
    }
#else
    std::vector< double > target_y;
    for ( double y = 0.0;
          y < wm.self().pos().absY() && y < 30.0;
          y += 5.0 )
    {
        target_y.push_back( y );
    }
    target_y.push_back( wm.self().pos().absY() );

    for ( std::vector< double >::iterator y = target_y.begin();
          y != target_y.end();
          ++y )
    {
        rcsc::Vector2D target_point( 42.0, *y );
        if ( wm.self().pos().y < 0.0 ) target_point.y *= -1.0;
        //if ( std::fabs( target_point.y - wm.self().pos().y ) > 16.0 ) continue;
        if ( ( target_point - wm.self().pos() ).th().abs() > 40.0 ) continue;

        if ( doKickTurnDashes( agent, target_point ) )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": self_pass_elapsed(2)=%.3f [ms]",
                                timer.elapsedReal() );
            return true;
        }
    }
#endif

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": self_pass_elapsed(3)=%.3f [ms]",
                        timer.elapsedReal() );

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SelfPass::doKickDashes( rcsc::PlayerAgent * agent )
{
    static std::vector< rcsc::Vector2D > self_state;

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        "__ selfPass doKickDashes" );

    const rcsc::WorldModel & wm = agent->world();

    createSelfCache( agent, 0, 18,
                     wm.self().body(),
                     self_state ); // no_turn, max_dash=18

    int n_dash = self_state.size();

    if ( n_dash < 6 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": too short dash step %d",
                            n_dash );
        return false;
    }

    for ( ; n_dash >= 6; n_dash -= 2 )
    {
        const rcsc::Vector2D receive_pos = self_state[n_dash - 1];

        if ( ! canKick( wm, 0, n_dash, receive_pos ) )
        {
            continue;
        }

        if ( ! checkOpponent( agent, 0, n_dash, receive_pos ) )
        {
            continue;
        }

        if ( doKick( agent, 0, n_dash, receive_pos ) )
        {
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SelfPass::doKickTurnDashes( rcsc::PlayerAgent * agent,
                                const rcsc::Vector2D & target_point )
{
    static std::vector< rcsc::Vector2D > self_state;

    const rcsc::WorldModel & wm = agent->world();

    const rcsc::Vector2D my_pos = wm.self().inertiaFinalPoint();
    const rcsc::AngleDeg target_angle = ( target_point - my_pos ).th();

    //
    // check the required turn step
    //
    const double angle_diff = ( target_angle - wm.self().body() ).abs();
    if ( angle_diff > 120.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            "__ selfPass doKickTurnDashes target_point=(%.1f %.1f)"
                            " too big angle_diff=%.1f",
                            target_point.x, target_point.y,
                            angle_diff );
        return false;
    }

    {
        double turn_margin = 180.0;
        double target_dist = my_pos.dist( target_point );
        turn_margin = std::max( turn_margin, 15.0 );

        if ( turn_margin > angle_diff )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                "__ selfPass doKickTurnDashes target_point=(%.1f %.1f)"
                                " too small angle_diff=%.1f < turn_margin=%.1f",
                                target_point.x, target_point.y,
                                angle_diff, turn_margin );
            return false;
        }
    }

    if ( angle_diff
         > wm.self().playerType().effectiveTurn( rcsc::ServerParam::i().maxMoment(),
                                                 wm.self().vel().r() ) )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            "__ selfPass doKickTurnDashes target_point=(%.1f %.1f)"
                            " cannot turn by one step."
                            " angle_diff=%.1f",
                            target_point.x, target_point.y,
                            angle_diff );
        return false;
    }

    createSelfCache( agent,
                     1, 12, // turn=1, max_dash=12
                     target_angle,
                     self_state );

    int n_dash = self_state.size() - 1;

    if ( n_dash < 6 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": too short dash step %d",
                            n_dash );
        return false;
    }

    for ( ; n_dash >= 6; n_dash -= 2 )
    {
        const rcsc::Vector2D receive_pos = self_state[n_dash - 1];

        if ( ! canKick( wm, 1, n_dash, receive_pos ) )
        {
            continue;
        }

        if ( ! checkOpponent( agent, 1, n_dash, receive_pos ) )
        {
            continue;
        }

        if ( doKick( agent, 1, n_dash, receive_pos ) )
        {
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SelfPass::createSelfCache( rcsc::PlayerAgent * agent,
                               const int n_turn,
                               const int n_dash,
                               const rcsc::AngleDeg & accel_angle,
                               std::vector< rcsc::Vector2D > & self_state )
{
    self_state.clear();
    self_state.reserve( n_turn + n_dash );

    const rcsc::WorldModel & wm = agent->world();
    const rcsc::PlayerType & self_type = wm.self().playerType();

    const double dash_power = rcsc::ServerParam::i().maxPower();
    const double stamina_thr = rcsc::ServerParam::i().recoverDecThrValue() + 350.0;

    double my_stamina = ( wm.self().stamina()
                          + ( self_type.staminaIncMax()
                              * wm.self().recovery() ) );
    double my_effort = wm.self().effort();
    double my_recovery = wm.self().recovery();

    rcsc::Vector2D my_pos = wm.self().pos();
    rcsc::Vector2D my_vel = wm.self().vel();

    my_pos += my_vel;
    my_vel *= self_type.playerDecay();

    for ( int i = 0; i < n_turn; ++i )
    {
        my_pos += my_vel;
        my_vel *= self_type.playerDecay();
        self_state.push_back( my_pos );
    }

    wm.self().playerType().getOneStepStaminaComsumption();

    for ( int i = 0; i < n_dash; ++i )
    {
        if ( my_stamina < stamina_thr )
        {
            break;
        }

        double available_stamina =  std::max( 0.0, my_stamina - stamina_thr );
        double consumed_stamina = dash_power;
        consumed_stamina = std::min( available_stamina,
                                     consumed_stamina );
        double used_power = consumed_stamina;
        double max_accel_mag = ( std::fabs( used_power )
                                 * self_type.dashPowerRate()
                                 * my_effort );
        double accel_mag = max_accel_mag;
        if ( self_type.normalizeAccel( my_vel,
                                       accel_angle,
                                       &accel_mag ) )
        {
            used_power *= accel_mag / max_accel_mag;
        }

        rcsc::Vector2D dash_accel
            = rcsc::Vector2D::polar2vector( std::fabs( used_power )
                                            * my_effort
                                            * self_type.dashPowerRate(),
                                            accel_angle );
        my_vel += dash_accel;
        my_pos += my_vel;

        if ( my_pos.x > rcsc::ServerParam::i().pitchHalfLength() - 3.5 )
        {
            break;
        }

        const rcsc::AngleDeg target_angle = ( my_pos - wm.ball().pos() ).th();

        if ( my_pos.absY() > rcsc::ServerParam::i().pitchHalfWidth() - 3.5
             &&  ( ( my_pos.y > 0.0 && target_angle.degree() > 0.0 )
                   || ( my_pos.y < 0.0 && target_angle.degree() < 0.0 )
                   )
             )
        {
            break;
        }

        my_vel *= self_type.playerDecay();

        self_type.getOneStepStaminaComsumption();
        self_state.push_back( my_pos );
    }

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SelfPass::checkOpponent( rcsc::PlayerAgent * agent,
                             const int n_turn,
                             const int n_dash,
                             const rcsc::Vector2D & receive_pos )
{
    const rcsc::WorldModel & wm = agent->world();

    const double max_moment = rcsc::ServerParam::i().maxMoment();
    const rcsc::PlayerPtrCont::const_iterator o_end = wm.opponentsFromSelf().end();

    for ( rcsc::PlayerPtrCont::const_iterator o = wm.opponentsFromSelf().begin();
          o != o_end;
          ++o )
    {
        const rcsc::PlayerType * player_type = (*o)->playerTypePtr();

        bool goalie = false;
        double control_area = player_type->kickableArea();
        if ( (*o)->goalie()
             && receive_pos.x > rcsc::ServerParam::i().theirPenaltyAreaLineX()
             && receive_pos.absY() < rcsc::ServerParam::i().penaltyAreaHalfWidth() )
        {
            goalie = true;
            control_area = rcsc::ServerParam::i().catchableArea();
        }

        const rcsc::Vector2D & opos = ( (*o)->seenPosCount() <= (*o)->posCount()
                                        ? (*o)->seenPos()
                                        : (*o)->pos() );
        const int vel_count = std::min( (*o)->seenVelCount(), (*o)->velCount() );
        const rcsc::Vector2D & ovel = ( (*o)->seenVelCount() <= (*o)->velCount()
                                        ? (*o)->seenVel()
                                        : (*o)->vel() );

        rcsc::Vector2D opp_pos = ( vel_count <= 1
                                   ? rcsc::inertia_n_step_point( opos, ovel,
                                                                 n_turn + n_dash + 1,
                                                                 player_type->playerDecay() )
                                   : opos + ovel );
        rcsc::Vector2D opp_to_target = receive_pos - opp_pos;

        double opp_to_target_dist = opp_to_target.r();

        int opp_turn_step = 0;
        if ( (*o)->bodyCount() <= 5
             || vel_count <= 5 )
        {
            double angle_diff = ( (*o)->bodyCount() <= 1
                                  ? ( opp_to_target.th() - (*o)->body() ).abs()
                                  : ( opp_to_target.th() - ovel.th() ).abs() );

            double turn_margin = 180.0;
            if ( control_area < opp_to_target_dist )
            {
                turn_margin = rcsc::AngleDeg::asin_deg( control_area / opp_to_target_dist );
            }
            turn_margin = std::max( turn_margin, 15.0 );

            double opp_speed = ovel.r();
            while ( angle_diff > turn_margin )
            {
                angle_diff -= player_type->effectiveTurn( max_moment, opp_speed );
                opp_speed *= (*o)->playerTypePtr()->playerDecay();
                ++opp_turn_step;
            }
        }

        opp_to_target_dist -= control_area;
        opp_to_target_dist -= 0.2;
        opp_to_target_dist -= (*o)->distFromSelf() * 0.01;

        if ( opp_to_target_dist < 0.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                "____ selfPass n_turn=%d n_dash=%d"
                                " opponent %d(%.1f %.1f) is already at receive point"
                                " (%.1f %.1f)",
                                n_turn,
                                n_dash,
                                (*o)->unum(),
                                (*o)->pos().x, (*o)->pos().y,
                                receive_pos.x, receive_pos.y );
            return false;
        }

        int opp_reach_step = player_type->cyclesToReachDistance( opp_to_target_dist );
        opp_reach_step += opp_turn_step;
        opp_reach_step -= rcsc::bound( 0, (*o)->posCount(), 5 );

        if ( opp_reach_step <= n_turn + n_dash )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                "____ selfPass n_turn=%d n_dash=%d"
                                " opponent %d (%.1f %.1f) can reach faster then self."
                                " target=(%.1f %.1f) opp_step=%d opp_turn=%d",
                                n_turn,
                                n_dash,
                                (*o)->unum(),
                                (*o)->pos().x, (*o)->pos().y,
                                receive_pos.x, receive_pos.y,
                                opp_reach_step,
                                opp_turn_step );
            return false;
        }
#ifdef DEBUG
        else
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                "______ selfPass n_turn=%d n_dash=%d"
                                " opponent %d (%.1f %.1f) ok."
                                " target=(%.1f %.1f) opp_step=%d opp_turn=%d",
                                n_turn,
                                n_dash,
                                (*o)->unum(),
                                (*o)->pos().x, (*o)->pos().y,
                                receive_pos.x, receive_pos.y,
                                opp_reach_step,
                                opp_turn_step );
        }
#endif
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SelfPass::canKick( const rcsc::WorldModel & wm,
                       const int n_turn,
                       const int n_dash,
                       const rcsc::Vector2D & receive_pos )
{
    const rcsc::AngleDeg target_angle = ( receive_pos - wm.ball().pos() ).th();

    //
    // check kick possibility
    //
    double first_speed = rcsc::calc_first_term_geom_series( wm.ball().pos().dist( receive_pos ),
                                                            rcsc::ServerParam::i().ballDecay(),
                                                            1 + 0 + n_dash );
    rcsc::Vector2D max_vel = rcsc::KickTable::calc_max_velocity( target_angle,
                                                                 wm.self().kickRate(),
                                                                 wm.ball().vel() );
    if ( max_vel.r() < first_speed )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            "____ selfPass canKick n_turn=%d n_dash=%d cannot kick by one step."
                            " first_speed=%.2f > max_speed=%.2f",
                            n_turn,
                            n_dash,
                            first_speed,
                            max_vel.r() );
        return false;
    }

    //
    // check collision
    //
    const rcsc::Vector2D my_next = wm.self().pos() + wm.self().vel();
    const rcsc::Vector2D ball_next = wm.ball().pos()
        + ( receive_pos - wm.ball().pos() ).setLengthVector( first_speed );

    if ( my_next.dist( ball_next ) < ( wm.self().playerType().playerSize()
                                       + rcsc::ServerParam::i().ballSize() + 0.1 ) )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            "____ selfPass n_turn=%d n_dash=%d maybe collision. first_speed=%.2f",
                            n_turn,
                            n_dash,
                            first_speed );
        return false;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SelfPass::doKick( rcsc::PlayerAgent * agent,
                      const int n_turn,
                      const int n_dash,
                      const rcsc::Vector2D & receive_pos )
{
    const rcsc::WorldModel & wm = agent->world();

    double first_speed = rcsc::calc_first_term_geom_series( wm.ball().pos().dist( receive_pos ),
                                                            rcsc::ServerParam::i().ballDecay(),
                                                            1 + n_turn + n_dash );

//     rcsc::AngleDeg target_angle = ( receive_pos - wm.ball().pos() ).th();
//     rcsc::Vector2D max_vel = rcsc::KickTable::calc_max_velocity( target_angle,
//                                                                  wm.self().kickRate(),
//                                                                  wm.ball().vel() );
//     if ( max_vel.r() < first_speed )
//     {
//         rcsc::dlog.addText( rcsc::Logger::TEAM,
//                             "__ selfPass cannot kick by one step. first_speed=%.2f > max_speed=%.2f",
//                             first_speed,
//                             max_vel.r() );
//         return false;
//     }

//     rcsc::Vector2D ball_next = wm.ball().pos()
//         + ( receive_pos - wm.ball().pos() ).setLengthVector( first_speed );
//     rcsc::Vector2D my_next = wm.self().pos() + wm.self().vel();
//     if ( my_next.dist( ball_next ) < ( wm.self().playerType().playerSize()
//                                        + rcsc::ServerParam::i().ballSize() + 0.1 ) )
//     {
//         rcsc::dlog.addText( rcsc::Logger::TEAM,
//                             "__ selfPass maybe collision. first_speed=%.2f",
//                             first_speed );
//         return false;
//     }

    if ( rcsc::Body_SmartKick( receive_pos,
                               first_speed,
                               first_speed * 0.99,
                               1 ).execute( agent ) )
    {
        agent->setViewAction( new rcsc::View_Wide() );
        agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan( 2 ) );

        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            "__ selfPass n_turn=%d n_dash=%d receive_pos=(%.1f %.1f) first_speed=%.2f",
                            n_turn,
                            n_dash,
                            receive_pos.x, receive_pos.y,
                            first_speed );

        agent->debugClient().addMessage( "SelfPass%d+%d", n_turn, n_dash );
        agent->debugClient().setTarget( receive_pos );

        agent->setIntention( new IntentionSelfPass( receive_pos, n_turn + n_dash ) );

        return true;
    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        "__ selfPass failed smart kick. n_turn=%d n_dash=%d"
                        " receive_pos=(%.1f %.1f) first_speed=%.2f",
                        n_turn,
                        n_dash,
                        receive_pos.x, receive_pos.y,
                        first_speed );

    return false;
}
