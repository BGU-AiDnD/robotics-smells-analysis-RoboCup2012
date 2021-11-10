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

#include "bhv_breakaway_through_pass.h"

#include "body_smart_kick.h"
#include "body_hold_ball2008.h"

#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/body_stop_ball.h>

#include <rcsc/action/neck_turn_to_player_or_scan.h>
#include <rcsc/action/neck_scan_field.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/say_message_builder.h>

#include <rcsc/common/audio_memory.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/ray_2d.h>
#include <rcsc/math_util.h>

using namespace rcsc;

const double Bhv_BreakawayThroughPass::END_SPEED = 1.3;

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_BreakawayThroughPass::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.audioMemory().passRequestTime().cycle() <= wm.time().cycle() - 2 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": too old request time (%ld)",
                      wm.audioMemory().passRequestTime().cycle() );
        return false;
    }

    double first_speed = 0.0;
    AngleDeg target_angle;
    Vector2D target_point;

    if ( ! search( agent, &first_speed, &target_angle, &target_point ) )
    {
        return false;
    }

    agent->debugClient().addMessage( "BreakawayPass" );
    agent->debugClient().setTarget( target_point );
#ifdef USE_SMART_KICK
    Body_SmartKick( target_point,
                    first_speed,
                    first_speed * 0.96,
                    3 ).execute( agent );
#else
    Body_KickMultiStep( target_point,
                        first_speed,
                        false
                        ).execute( agent ); // not enforce
    agent->setIntention
        ( new IntentionKick( target_point,
                             first_speed,
                             3, // max kick step
                             false, // not enforce
                             wm.time() ) );
#endif
    const AbstractPlayerObject * receiver = static_cast< AbstractPlayerObject * >( 0 );
    int receiver_unum = Unum_Unknown;
    if ( wm.audioMemory().passRequestTime() == wm.time()
         && ! wm.audioMemory().passRequest().empty() )
    {
        receiver_unum = wm.audioMemory().passRequest().front().sender_;
        receiver = wm.ourPlayer( receiver_unum );
    }

    if ( receiver )
    {
        agent->setNeckAction( new Neck_TurnToPlayerOrScan( receiver ) );
    }
    else
    {
        agent->setNeckAction( new Neck_ScanField() );
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": execute() register pass intention" );

    if ( agent->config().useCommunication()
         && receiver_unum != Unum_Unknown )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": execute() set pass communication." );

        agent->debugClient().addMessage( "SaypBreak" );
        agent->addSayMessage( new PassMessage( receiver_unum,
                                               target_point,
                                               agent->effector().queuedNextBallPos(),
                                               agent->effector().queuedNextBallVel() ) );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_BreakawayThroughPass::search( const PlayerAgent * agent,
                                  double * first_speed,
                                  AngleDeg * target_angle,
                                  Vector2D * target_point )
{
    const WorldModel & wm = agent->world();

    const AbstractPlayerObject * receiver = static_cast< AbstractPlayerObject * >( 0 );
    int receiver_unum = Unum_Unknown;
    Vector2D receive_point = Vector2D::INVALIDATED;

    if ( wm.audioMemory().passRequestTime() == wm.time()
         && ! wm.audioMemory().passRequest().empty() )
    {
        receiver_unum = wm.audioMemory().passRequest().front().sender_;
        receiver = wm.ourPlayer( receiver_unum );
        receive_point = wm.audioMemory().passRequest().front().pos_;
    }

    if ( ! receiver )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": receiver %d is not seen",
                      receiver_unum );
        return false;
    }

    if ( ! receive_point.isValid() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": receiver = %d, but illegal receive point",
                      receiver->unum() );
        return false;
    }

    if ( receiver->pos().x > wm.offsideLineX() + 1.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": receiver %d is over offside line (%.1f %.1f)",
                      receiver->unum(),
                      receiver->pos().x, receiver->pos().y );
        return false;
    }

    if ( receiver->pos().x < wm.self().pos().x - 2.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": receiver %d is back. (%.1f %.1f)",
                      receiver->unum(),
                      receiver->pos().x, receiver->pos().y );
        return false;
    }

    if ( receiver->pos().x + 1.1 * receiver->posCount()
         < wm.offsideLineX() - 2.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": receiver %d is too back (%.1f %.1f)",
                      receiver->unum(),
                      receiver->pos().x, receiver->pos().y );
        // TODO: set intention
        return false;
    }

    const Vector2D dash_point = receive_point;

    // create opponent container sorted by angle

    std::vector< AngleDeg > opponent_angles;
    opponent_angles.reserve( wm.opponentsFromBall().size() + 2 );

    {
        const AngleDeg default_angle = ( dash_point - wm.ball().pos() ).th();

        opponent_angles.push_back( AngleDeg( -179.9999 ) );

        const PlayerPtrCont::const_iterator end = wm.opponentsFromBall().end();
        for ( PlayerPtrCont::const_iterator o = wm.opponentsFromBall().begin();
              o != end;
              ++o )
        {
            if ( (*o)->isGhost() || (*o)->posCount() >= 15 ) continue;

            if ( (*o)->goalie() && (*o)->pos().x > 37.0 )
            {
                continue;
            }

            if ( (*o)->pos().x > wm.offsideLineX() + 3.0 )
            {
                continue;
            }

            AngleDeg angle_from_ball = ( (*o)->pos() - wm.ball().pos() ).th();

            if ( ( angle_from_ball - default_angle ).abs() > 90.0 )
            {
                continue;
            }

            opponent_angles.push_back( angle_from_ball );
        }
    }

    // if no opponent, just kick to the default target point
    if ( opponent_angles.size() == 1 )
    {
        double speed_max = ( receiver->playerTypePtr()
                             ? receiver->playerTypePtr()->realSpeedMax()
                             : ServerParam::i().defaultRealSpeedMax() );
        double dash_steps
            = receiver->pos().dist( dash_point) / speed_max
            - receiver->posCount();
        if ( dash_steps < 0.999 ) dash_steps = 0.999;
        int idash_steps = static_cast< int >( std::ceil( dash_steps ) );
        double target_dist = wm.ball().pos().dist( dash_point );

        double tmp_speed = 3.0;
        for ( int loop = 0; loop < 10; ++loop )
        {
            tmp_speed
                = calc_first_term_geom_series( target_dist,
                                               ServerParam::i().ballDecay(),
                                               idash_steps );
            double end_speed = tmp_speed
                * std::pow( ServerParam::i().ballDecay(), idash_steps );
            if ( tmp_speed < ServerParam::i().ballSpeedMax()
                 && end_speed < 1.5 )
            {
                break;
            }
            ++idash_steps;
        }

        *first_speed = tmp_speed;
        *target_angle = ( dash_point - wm.self().pos() ).th();
        *target_point = dash_point;
        return true;
    }

    // sort by angle
    std::sort( opponent_angles.begin(), opponent_angles.end(),
               AngleDeg::DegreeCmp() );
    // added extra angles
    opponent_angles[0] = opponent_angles[1] - 60.0;
    opponent_angles.push_back( opponent_angles.back() + 60.0 );

    const std::vector< AngleDeg >::const_iterator o_end = opponent_angles.end();
    for ( std::vector< AngleDeg >::const_iterator o = opponent_angles.begin();
          o != o_end;
          ++o )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": sorted opponent angle = %.1f",
                      o->degree() );
    }

    AngleDeg best_angle;
    double best_first_speed = -1.0;
    Vector2D best_target_point;

    if ( calcPenLineTarget( wm,
                            receiver, dash_point,
                            opponent_angles,
                            &best_angle,
                            &best_first_speed,
                            &best_target_point ) )
    {
        *target_angle = best_angle;
        *first_speed = best_first_speed;
        *target_point = best_target_point;
        return true;
    }

    double widest_angle = -1.0;

    for ( std::vector< AngleDeg >::const_iterator o = opponent_angles.begin();
          o != o_end;
          ++o )
    {
        if ( o + 1 == o_end ) break;

        AngleDeg angle;
        double speed;
        Vector2D point;
        double angle_width = getAngleWidth( wm,
                                            receiver, dash_point,
                                            *o, *(o + 1),
                                            &angle,
                                            &speed,
                                            &point );
        if ( angle_width > 0.0
             && speed > 2.1 ) //&& speed > 2.5 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": found. angle=%.1f speed=%.1f angle_width=%.1f",
                          angle.degree(), speed, angle_width );

            if ( angle_width > widest_angle )
            {
                widest_angle = angle_width;
                best_angle = angle;
                best_first_speed = speed;
                best_target_point = point;
            }
        }
    }

    if ( widest_angle < 0.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": no solution. widest angle=%.1f",
                      widest_angle );
        return false;
    }

    if ( best_first_speed < 2.1 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": too slow first speed=%.1f",
                      best_first_speed );
        return false;
    }

    *target_angle = best_angle;
    *first_speed = best_first_speed;
    *target_point = best_target_point;

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
double
Bhv_BreakawayThroughPass::getAngleWidth( const WorldModel & wm,
                                         const AbstractPlayerObject * receiver,
                                         const Vector2D & dash_point,
                                         const AngleDeg & angle_left,
                                         const AngleDeg & angle_right,
                                         AngleDeg * target_angle,
                                         double * first_speed,
                                         Vector2D * target_point )
{
    const double min_angle_width = 50.0;

    double angle_width = ( angle_left - angle_right ).abs();
    if ( angle_width < min_angle_width )
    {
        return -1.0;
    }

    AngleDeg tmp_angle = AngleDeg::bisect( angle_left, angle_right );

    dlog.addText( Logger::TEAM,
                  __FILE__": get angle width left=%.1f right=%.1f -> target=%.1f"
                  " angle_width=%.1f",
                  angle_left.degree(),
                  angle_right.degree(),
                  tmp_angle.degree(),
                  angle_width );

    if ( tmp_angle.abs() > 60.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": invalid angle. pass_angle=%.1f",
                      tmp_angle.degree() );
        return -1.0;
    }

    Ray2D pass_ray( wm.ball().pos(), tmp_angle );
    Ray2D dash_ray( receiver->pos(), dash_point );

    Vector2D intersection = pass_ray.intersection( dash_ray );

    if ( ! intersection.isValid() )
    {
        // no intersection
        dlog.addText( Logger::TEAM,
                      __FILE__": no intersection. pass_angle=%.1f",
                      tmp_angle.degree() );
        return -1.0;
    }

    const PlayerObject * goalie = wm.getOpponentGoalie();

    if ( intersection.x > 49.0
//          || ( std::fabs( dash_point.y - wm.ball().pos().y ) > 5.0
//               && intersection.x > 36.0
//               && intersection.absY() < 18.0 )
         || ( goalie
              && std::fabs( dash_point.y - wm.ball().pos().y ) > 5.0
              && intersection.x > 36.0
              && intersection.absY() < 20.0
              && goalie->pos().dist( intersection ) < receiver->pos().dist( intersection ) - 3.0
              )
         )
    {
        // maybe goalie catch the ball
        dlog.addText( Logger::TEAM,
                      __FILE__": over pitch or goalie may catch."
                      " angle=%.1f pos=(%.1f %.1f)",
                      tmp_angle.degree(),
                      intersection.x, intersection.y );
        return -1.0;
    }

    double target_dist = wm.ball().pos().dist( dash_point );

    Vector2D one_step_vel
        = Body_KickOneStep::get_max_possible_vel( tmp_angle,
                                                  wm.self().kickRate(),
                                                  wm.ball().vel() );
    double one_step_speed = one_step_vel.r();

    double ball_first_speed
        = calc_first_term_geom_series_last( END_SPEED,
                                            target_dist,
                                            ServerParam::i().ballDecay() );

    if ( ball_first_speed > ServerParam::i().ballSpeedMax() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": route found. over ball speed max %.2f",
                      ball_first_speed );
        return -1.0;
    }

    if ( ball_first_speed * 0.9 < one_step_speed )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": changed to one step kick. required=%.1f one_step=%.1f",
                      ball_first_speed, one_step_speed );

        ball_first_speed = std::min( one_step_speed, ball_first_speed );
    }

    double speed_max = ( receiver->playerTypePtr()
                         ? receiver->playerTypePtr()->realSpeedMax()
                         : ServerParam::i().defaultRealSpeedMax() );
    double dash_steps
        = receiver->pos().dist( dash_point) / speed_max
        - receiver->posCount();
    if ( dash_steps < 0.999 ) dash_steps = 0.999;

   double ball_steps_to_target
        = calc_length_geom_series( ball_first_speed,
                                   target_dist,
                                   ServerParam::i().ballDecay() );
   if ( dash_steps > ball_steps_to_target )
   {
       dlog.addText( Logger::TEAM,
                      __FILE__": getAngleWidth. receiver cannot reach the target point."
                     " dash_step=%.1f ball_steps=%.1f",
                     dash_steps, ball_steps_to_target );
        return -1.0;
   }

    *target_angle = tmp_angle;
    *first_speed = one_step_speed;
    *target_point = intersection;

    dlog.addText( Logger::TEAM,
                  __FILE__": candidate angle=%.1f speed=%.1f pos(%.1f %.1f)"
                  " angle_width=%.1f",
                  tmp_angle.degree(), *first_speed,
                  intersection.x, intersection.y,
                  angle_width );

    return angle_width;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_BreakawayThroughPass::calcPenLineTarget( const WorldModel & wm,
                                             const AbstractPlayerObject * receiver,
                                             const Vector2D & dash_point,
                                             const std::vector< AngleDeg > & opponent_angles,
                                             AngleDeg * target_angle,
                                             double * first_speed,
                                             Vector2D * target_point )
{
    if ( //dash_point.x > 40.0 ||
        receiver->pos().x > 30.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": calcPenLineTarget. receivr x is too front" );
        return false;
    }

    Line2D dash_line( receiver->pos(), dash_point );

    //Vector2D pen_line_target( 35.0, dash_line.getY( 35.0 ) );
    Vector2D pen_line_target( 32.0, dash_line.getY( 32.0 ) );

    if ( pen_line_target.y == Line2D::ERROR_VALUE )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": calcPenLineTarget. no intersection with pen line" );
        return false;
    }

    if ( pen_line_target.absY() > 20.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": calcPenLineTarget. big y_value = %.1f",
                      pen_line_target.y );
        return false;
    }

    AngleDeg tmp_angle = ( pen_line_target - wm.ball().pos() ).th();

    if ( tmp_angle.abs() > 60.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": calcPenLineTarget. invalid angle. pass_angle=%.1f",
                      tmp_angle.degree() );
        return false;
    }

    double ball_first_speed
        = calc_first_term_geom_series_last( 1.0, //END_SPEED,
                                            wm.self().pos().dist( pen_line_target ),
                                            ServerParam::i().ballDecay() );
    Vector2D ball_first_vel = Vector2D::polar2vector( ball_first_speed,
                                                      tmp_angle );

    double ball_steps_to_target
        = calc_length_geom_series( ball_first_speed,
                                   wm.ball().pos().dist( pen_line_target ),
                                   ServerParam::i().ballDecay() );

    double dash_dist = receiver->pos().dist( pen_line_target );
    double speed_max = ( receiver->playerTypePtr()
                         ? receiver->playerTypePtr()->realSpeedMax()
                         : ServerParam::i().defaultRealSpeedMax() );
    double dash_steps = dash_dist / speed_max;

    dlog.addText( Logger::TEAM,
                  __FILE__": calcPenLineTarget. ball_steps=%.1f dash_steps=%.1f",
                  ball_steps_to_target, dash_steps );

    if ( dash_steps > ball_steps_to_target )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": calcPenLineTarget. receiver cannot reach the target point" );
        return false;
    }


    if ( opponent_angles.size() > 2 )
    {
        if ( tmp_angle.isLeftOf( opponent_angles[1] ) )
        {
            double angle_width = ( opponent_angles[1] - tmp_angle ).abs();
            if ( angle_width > 25.0 )
            {
                dlog.addText( Logger::TEAM,
                              __FILE__": calcPenLineTarget. found left over."
                              " pass_angle=%.1f  angle_width_half=%.1f",
                              tmp_angle.degree(), angle_width );

                *target_angle = tmp_angle;
                *first_speed = ball_first_speed;
                *target_point = pen_line_target;

                return true;
            }
        }

        if ( tmp_angle.isRightOf( opponent_angles[opponent_angles.size() - 2] ) )
        {
            double angle_width = ( opponent_angles[opponent_angles.size() - 2] - tmp_angle ).abs();
            if ( angle_width > 25.0 )
            {
                dlog.addText( Logger::TEAM,
                              __FILE__": calcPenLineTarget. found right over."
                              " pass_angle=%.1f  angle_width_half=%.1f",
                              tmp_angle.degree(), angle_width );

                *target_angle = tmp_angle;
                *first_speed = ball_first_speed;
                *target_point = pen_line_target;

                return true;
            }
        }
    }


    const std::vector< AngleDeg >::const_iterator o_end = opponent_angles.end();
    for ( std::vector< AngleDeg >::const_iterator o = opponent_angles.begin();
          o != o_end;
          ++o )
    {
        if ( o + 1 == o_end ) break;

        if ( tmp_angle.isRightOf( *o )
             && tmp_angle.isLeftOf( *(o + 1) ) )
        {
            double angle_width = std::min( ( *o - tmp_angle ).abs(),
                                           ( *(o + 1) - tmp_angle ).abs() );
            if ( angle_width > 25.0 )
            {
                dlog.addText( Logger::TEAM,
                              __FILE__": calcPenLineTarget. found"
                              " pass_angle=%.1f  angle_width_half=%.1f",
                              tmp_angle.degree(), angle_width );

                *target_angle = tmp_angle;
                *first_speed = ball_first_speed;
                *target_point = pen_line_target;

                return true;
            }
        }
    }

    return false;
}
