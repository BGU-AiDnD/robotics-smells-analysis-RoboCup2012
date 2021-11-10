// -*-c++-*-

/*!
  \file shoot_table2008.cpp
  \brief shoot plan search and holder class
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

#include "shoot_table2008.h"

#include "kick_table.h"
#include "intercept_simulator.h"

#include <rcsc/player/player_agent.h>
#include <rcsc/player/interception.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/math_util.h>
#include <rcsc/timer.h>

#define DEBUG

namespace rcsc {

/*-------------------------------------------------------------------*/
/*!

 */
void
ShootTable2008::search( const PlayerAgent * agent )
{
    static GameTime s_time( 0, 0 );

    /////////////////////////////////////////////////////////////////////
    const WorldModel & wm = agent->world();

    if ( s_time == wm.time() )
    {
        return;
    }

    s_time = wm.time();
    M_shots.clear();

    /////////////////////////////////////////////////////////////////////
    static const Rect2D
        shootable_area( Vector2D( ServerParam::i().theirPenaltyAreaLineX() - 5.0, // left
                                  -ServerParam::i().penaltyAreaHalfWidth() ), // top
                        Size2D( ServerParam::i().penaltyAreaLength() + 5.0, // length
                                ServerParam::i().penaltyAreaWidth() ) ); // width

    if ( ! wm.self().isKickable()
         || ! shootable_area.contains( wm.ball().pos() ) )
    {
        return;
    }

    MSecTimer timer;

    Vector2D goal_l( ServerParam::i().pitchHalfLength(),
                     -ServerParam::i().goalHalfWidth() );
    Vector2D goal_r( ServerParam::i().pitchHalfLength(),
                     ServerParam::i().goalHalfWidth() );

    goal_l.y += std::min( 1.5,
                          0.6 + goal_l.dist( wm.ball().pos() ) * 0.042 );
    goal_r.y -= std::min( 1.5,
                          0.6 + goal_r.dist( wm.ball().pos() ) * 0.042 );

    if ( wm.self().pos().x > ServerParam::i().pitchHalfLength() - 1.0
         && wm.self().pos().absY() < ServerParam::i().goalHalfWidth() )
    {
        goal_l.x = wm.self().pos().x + 1.5;
        goal_r.x = wm.self().pos().x + 1.5;
    }

    const int DIST_DIVS = 15;
    const double dist_step = std::fabs( goal_l.y - goal_r.y ) / ( DIST_DIVS - 1 );

    dlog.addText( Logger::SHOOT,
                  "Shoot search range=(%.1f %.1f)-(%.1f %.1f) dist_step=%.1f",
                  goal_l.x, goal_l.y, goal_r.x, goal_r.y, dist_step );

    const PlayerObject * goalie = agent->world().getOpponentGoalie();

    Vector2D shot_point = goal_l;

    for ( int i = 0;
          i < DIST_DIVS;
          ++i, shot_point.y += dist_step )
    {
        calculateShotPoint( wm, shot_point, goalie );
    }

    dlog.addText( Logger::SHOOT,
                  "Shoot found %d shots. search_shoot_elapsed=%.3f [ms]",
                  (int)M_shots.size(),
                  timer.elapsedReal() );

}

/*-------------------------------------------------------------------*/
/*!

 */
void
ShootTable2008::calculateShotPoint( const WorldModel & wm,
                                    const Vector2D & shot_point,
                                    const PlayerObject * goalie )
{
    Vector2D shot_rel = shot_point - wm.ball().pos();
    AngleDeg shot_angle = shot_rel.th();

    int goalie_count = 1000;
    if ( goalie )
    {
        goalie_count = goalie->posCount();
    }

    if ( 5 < goalie_count
         && goalie_count < 30
         && wm.dirCount( shot_angle ) > 3 )
    {
        return;
    }

    double shot_dist = shot_rel.r();

    Vector2D one_step_vel
        = KickTable::calc_max_velocity( shot_angle,
                                        wm.self().kickRate(),
                                        wm.ball().vel() );
    double max_one_step_speed = one_step_vel.r();

    double shot_first_speed
        = ( shot_dist + 5.0 ) * ( 1.0 - ServerParam::i().ballDecay() );
    shot_first_speed = std::max( max_one_step_speed, shot_first_speed );
    shot_first_speed = std::max( 1.5, shot_first_speed );

    // gaussian function, distribution = goal half width
    //double y_rate = std::exp( - std::pow( shot_point.y, 2.0 )
    //                          / ( 2.0 * ServerParam::i().goalHalfWidth() * 3.0 ) );
    double y_dist = std::max( 0.0, shot_point.absY() - 4.0 );
    double y_rate = std::exp( - std::pow( y_dist, 2.0 )
                              / ( 2.0 * ServerParam::i().goalHalfWidth() ) );

    bool over_max = false;
    while ( ! over_max )
    {
        if ( shot_first_speed > ServerParam::i().ballSpeedMax() - 0.001 )
        {
            over_max = true;
            shot_first_speed = ServerParam::i().ballSpeedMax();
        }

        Shot shot( shot_point, shot_first_speed, shot_angle );
        shot.score_ = 0;

        if ( canScore( wm, &shot ) )
        {
            shot.score_ += 100;
            bool one_step = ( shot_first_speed <= max_one_step_speed );
            if ( one_step )
            {   // one step kick
                shot.score_ += 100;
            }

            double goalie_rate = -1.0;
            if ( shot.goalie_never_reach_ )
            {
                shot.score_ += 100;
            }

            if ( goalie )
            {
                AngleDeg goalie_angle = ( goalie->pos() - wm.ball().pos() ).th();
                double angle_diff = ( shot.angle_ - goalie_angle ).abs();
                goalie_rate = 1.0 - std::exp( - std::pow( angle_diff * 0.1, 2.0 )
                                              / ( 2.0 * 90.0 * 0.1 ) );
                shot.score_ = static_cast< int >( shot.score_ * goalie_rate );
#ifdef DEBUG
                dlog.addText( Logger::SHOOT,
                              "--- apply goalie rate. angle_diff=%.1f rate=%.2f",
                              angle_diff, goalie_rate );
#endif
            }

            shot.score_ = static_cast< int >( shot.score_ * y_rate );

            dlog.addText( Logger::SHOOT,
                          "<<< Shoot score=%d pos(%.1f %.1f)"
                          " angle=%.1f speed=%.1f"
                          " y_rate=%.2f g_rate=%.2f"
                          " one_step=%d GK_never_reach=%d",
                          shot.score_,
                          shot_point.x, shot_point.y,
                          shot_angle.degree(),
                          shot_first_speed,
                          y_rate, goalie_rate,
                          ( one_step ? 1 : 0 ),
                          ( shot.goalie_never_reach_ ? 1 : 0 ) );

            M_shots.push_back( shot );
        }
#ifdef DEBUG
        else
        {
            dlog.addText( Logger::SHOOT,
                          ">>> Shoot failed to(%.1f %.1f)"
                          " angle=%.1f speed=%.1f",
                          shot_point.x, shot_point.y,
                          shot_angle.degree(),
                          shot_first_speed );
        }
#endif
        shot_first_speed += 0.5;
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
ShootTable2008::canScore( const WorldModel & wm,
                          Shot * shot )
{
    static const double opp_x_thr = ServerParam::i().theirPenaltyAreaLineX() - 5.0;
    static const double opp_y_thr = ServerParam::i().penaltyAreaHalfWidth();
#if 0
    static const double player_max_speed
        = ServerParam::i().defaultPlayerSpeedMax();
    static const double player_control_area
        = ServerParam::i().defaultKickableArea();
#endif
    // estimate required ball travel step
    const double ball_reach_step
        = calc_length_geom_series( shot->speed_,
                                   wm.ball().pos().dist( shot->point_ ),
                                   ServerParam::i().ballDecay() );

    if ( ball_reach_step < 1.0 )
    {
        shot->score_ += 100;
        return true;
    }

    dlog.addText( Logger::SHOOT,
                  "canScore() shot to (%.1f %.1f) vel=(%.2f %.2f)%.3f angle=%.1f",
                  shot->point_.x, shot->point_.y,
                  shot->vel_.x, shot->vel_.y,
                  shot->speed_,
                  shot->angle_.degree() );

    const int ball_reach_step_i = static_cast< int >( std::ceil( ball_reach_step ) );

    // estimate opponent interception

    const PlayerPtrCont::const_iterator end = wm.opponentsFromSelf().end();
    for ( PlayerPtrCont::const_iterator it = wm.opponentsFromSelf().begin();
          it != end;
          ++it )
    {
        // outside of penalty
        if ( (*it)->pos().x < opp_x_thr ) continue;
        if ( (*it)->pos().absY() > opp_y_thr ) continue;
        if ( (*it)->isTackling() ) continue;

        // behind of shoot course
        if ( ( shot->angle_ - (*it)->angleFromSelf() ).abs() > 90.0 )
        {
            continue;
        }

        if ( (*it)->goalie() )
        {
            if ( maybeGoalieCatch( wm, *it, shot ) )
            {
                return false;
            }
        }
        else
        {
            if ( (*it)->posCount() > 10
                 || ( (*it)->isGhost() && (*it)->posCount() > 5 ) )
            {
                continue;
            }
#if 1
            int cycle = predictOpponentReachStep( wm,
                                                  shot->point_,
                                                  *it,
                                                  wm.ball().pos(),
                                                  shot->vel_,
                                                  ball_reach_step_i );
            dlog.addText( Logger::SHOOT,
                          "__ opp %d(%.1f %.1f) reach step=%d, ball_step=%d",
                          (*it)->unum(),
                          (*it)->pos().x, (*it)->pos().y,
                          cycle,
                          ball_reach_step_i );
            if ( cycle == 1
                 || cycle < ball_reach_step_i - 1 )
            {
                return false;
            }
#else
            double cycle = ( std::ceil( util.getReachCycle( (*it)->pos(),
                                                            &((*it)->vel()),
                                                            NULL,
                                                            std::min( 2, (*it)->posCount() ),
                                                            player_control_area,
                                                            player_max_speed ) ) );

            if ( cycle < ball_reach_step )
            {
#ifdef DEBUG
                dlog.addText( Logger::SHOOT,
                              "__ opp %d(%.1f %.1f) can reach. reach_step=%.2f < ball_step=%.2f",
                              (*it)->unum(),
                              (*it)->pos().x, (*it)->pos().y,
                              cycle,
                              ball_reach_step );
#endif
                return false;
            }
#endif
        }
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
ShootTable2008::maybeGoalieCatch( const WorldModel & wm,
                                  const PlayerObject * goalie,
                                  Shot * shot )
{
    static const
        Rect2D penalty_area( Vector2D( ServerParam::i().theirPenaltyAreaLineX(), // left
                                       -ServerParam::i().penaltyAreaHalfWidth() ), // top
                             Size2D( ServerParam::i().penaltyAreaLength(), // length
                                     ServerParam::i().penaltyAreaWidth() ) ); // width
    static const double catchable_area = ServerParam::i().catchableArea();

    const ServerParam & param = ServerParam::i();

    const double dash_accel_mag = ( param.maxPower()
                                    * param.defaultDashPowerRate()
                                    * param.defaultEffortMax() );
    const double seen_dist_noise = goalie->distFromSelf() * 0.05;

    int min_cycle = 1;
    {
        Line2D shot_line( wm.ball().pos(), shot->point_ );
        double goalie_line_dist = shot_line.dist( goalie->pos() );
        goalie_line_dist -= catchable_area;
        goalie_line_dist -= seen_dist_noise;
        min_cycle = static_cast< int >
            ( std::ceil( goalie_line_dist / param.defaultRealSpeedMax() ) ) ;
        min_cycle -= std::min( 5, goalie->posCount() );
        min_cycle = std::max( 1, min_cycle );
    }
    Vector2D ball_pos = inertia_n_step_point( wm.ball().pos(),
                                              shot->vel_,
                                              min_cycle,
                                              param.ballDecay() );
    Vector2D ball_vel = ( shot->vel_
                          * std::pow( param.ballDecay(), min_cycle ) );


    int cycle = min_cycle;
    while ( ball_pos.x < param.pitchHalfLength() + 0.085
            && cycle <= 50 )
    {
        // estimate the required turn angle
        Vector2D goalie_pos = goalie->inertiaPoint( cycle );
//             = inertia_n_step_point( goalie->pos(),
//                                     goalie->vel(),
//                                     cycle,
//                                     param.defaultPlayerDecay() );
        Vector2D ball_relative = ball_pos - goalie_pos;
        double ball_dist = ball_relative.r() - seen_dist_noise;

        if ( ball_dist < catchable_area )
        {
            return true;
        }

        if ( ball_dist < catchable_area + 1.0 )
        {
            shot->goalie_never_reach_ = false;
        }

        AngleDeg ball_angle = ball_relative.th();
        AngleDeg goalie_body = ( goalie->bodyCount() <= 5
                                 ? goalie->body()
                                 : ball_angle );

        int n_turn = 0;
        double angle_diff = ( ball_angle - goalie_body ).abs();
        if ( angle_diff > 90.0 )
        {
            angle_diff = 180.0 - angle_diff; // back dash
            goalie_body -= 180.0;
        }

        double turn_margin
            = std::max( AngleDeg::asin_deg( catchable_area / ball_dist ),
                        15.0 );

#ifdef DEBUG
        dlog.addText( Logger::SHOOT,
                      "__ goalie. cycle=%d ball_pos(%.1f %.1f) g_body=%.0f"
                      " b_angle=%.1f angle_diff=%.1f turn_margin=%.1f",
                      cycle,
                      ball_pos.x, ball_pos.y,
                      goalie_body.degree(),
                      ball_angle.degree(),
                      angle_diff, turn_margin );
#endif

        Vector2D goalie_vel = goalie->vel();

        while ( angle_diff > turn_margin )
        {
            double max_turn
                = effective_turn( 180.0,
                                  goalie_vel.r(),
                                  param.defaultInertiaMoment() );
            angle_diff -= max_turn;
            goalie_vel *= param.defaultPlayerDecay();
            ++n_turn;
        }

        // simulate dash
        goalie_pos
            = inertia_n_step_point( goalie->pos(),
                                    goalie->vel(),
                                    n_turn,
                                    param.defaultPlayerDecay() );

        const Vector2D dash_accel = Vector2D::polar2vector( dash_accel_mag,
                                                            ball_angle );
        const int max_dash = ( cycle - 1 - n_turn
                               + std::min( 5, goalie->posCount() ) );
        double goalie_travel = 0.0;
        for ( int i = 0; i < max_dash; ++i )
        {
            goalie_vel += dash_accel;
            goalie_pos += goalie_vel;
            goalie_travel += goalie_vel.r();
            goalie_vel *= param.defaultPlayerDecay();

            double d = goalie_pos.dist( ball_pos ) - seen_dist_noise;
#if 0
            dlog.addText( Logger::SHOOT,
                          "__ goalie. cycle=%d ball(%.1f %.1f) angle=%.0f"
                          " goalie pos(%.1f %.1f) travel=%.1f dist=%.1f turn=%d dash=%d",
                          cycle,
                          ball_pos.x, ball_pos.y,
                          ball_angle.degree(),
                          goalie_pos.x, goalie_pos.y,
                          goalie_travel,
                          d,
                          n_turn, i + 1 );
#endif
            if ( d < catchable_area + 1.0 + ( goalie_travel * 0.04 ) )
            {
                shot->goalie_never_reach_ = false;
            }
        }

        // check distance
        if ( goalie->pos().dist( goalie_pos ) * 1.05
             > goalie->pos().dist( ball_pos )
             - seen_dist_noise
             - catchable_area )
        {
#ifdef DEBUG
            dlog.addText( Logger::SHOOT,
                          "__ goalie can reach. move_dist=%.3f ball_dist=%.3f",
                          goalie->pos().dist( goalie_pos ) * 1.05,
                          goalie->pos().dist( ball_pos ) - seen_dist_noise - catchable_area );
#endif
            return true;
        }

        // update ball position & velocity
        ++cycle;
        ball_pos += ball_vel;
        ball_vel *= param.ballDecay();
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
ShootTable2008::predictOpponentReachStep( const WorldModel &,
                                          const Vector2D & target_point,
                                          const PlayerObject * opponent,
                                          const Vector2D & first_ball_pos,
                                          const Vector2D & first_ball_vel,
                                          const int max_step )
{
    const ServerParam & param = ServerParam::i();
    const PlayerType * player_type = opponent->playerTypePtr();
    const double control_area = player_type->kickableArea();

    int min_cycle = 1;
    {
        Line2D shot_line( first_ball_pos, target_point );
        double line_dist = shot_line.dist( opponent->pos() );
        line_dist -= control_area;
        min_cycle = static_cast< int >
            ( std::ceil( line_dist / player_type->realSpeedMax() ) ) ;
        min_cycle -= std::min( 5, opponent->posCount() );
        min_cycle = std::max( 1, min_cycle );
    }

    Vector2D ball_pos = inertia_n_step_point( first_ball_pos,
                                              first_ball_vel,
                                              min_cycle,
                                              param.ballDecay() );
    Vector2D ball_vel = first_ball_vel * std::pow( param.ballDecay(), min_cycle );

    int cycle = min_cycle;

    while ( cycle <= max_step )
    {
        Vector2D opp_pos = opponent->inertiaPoint( cycle );
        Vector2D opp_to_ball = ball_pos - opp_pos;
        double opp_to_ball_dist = opp_to_ball.r();

        int n_turn = 0;
        if ( opponent->bodyCount() <= 1
             || opponent->velCount() <= 1 )
        {
            double angle_diff =  ( opponent->bodyCount() <= 1
                                  ? ( opp_to_ball.th() - opponent->body() ).abs()
                                  : ( opp_to_ball.th() - opponent->vel().th() ).abs() );

            double turn_margin = 180.0;
            if ( control_area < opp_to_ball_dist )
            {
                turn_margin = AngleDeg::asin_deg( control_area / opp_to_ball_dist );
            }
            turn_margin = std::max( turn_margin, 12.0 );

            double opp_speed = opponent->vel().r();
            while ( angle_diff > turn_margin )
            {
                angle_diff -= player_type->effectiveTurn( param.maxMoment(), opp_speed );
                opp_speed *= player_type->playerDecay();
                ++n_turn;
            }
        }

        opp_to_ball_dist -= control_area;
        opp_to_ball_dist -= opponent->distFromSelf() * 0.03;

        if ( opp_to_ball_dist < 0.0 )
        {
            return cycle;
        }

        int n_step = player_type->cyclesToReachDistance( opp_to_ball_dist );
        n_step += n_turn;
        n_step -= bound( 0, opponent->posCount() - 1, 2 );

        if ( n_step <= cycle )
        {
            return cycle;
        }

        // update ball position & velocity
        ++cycle;
        ball_pos += ball_vel;
        ball_vel *= param.ballDecay();
    }

    return cycle;
}

}
