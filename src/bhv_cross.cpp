// -*-c++-*-

/*!
  \file bhv_cross.cpp
  \brief cross to center action
*/

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

#include "bhv_cross.h"

#include "body_smart_kick.h"

#include <rcsc/action/basic_actions.h>

#include <rcsc/player/debug_client.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/say_message_builder.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/line_2d.h>
#include <rcsc/soccer_math.h>
#include <rcsc/math_util.h>

//#define DEBUG

std::vector< Bhv_Cross::Target >
Bhv_Cross::S_cached_target;

/*-------------------------------------------------------------------*/
/*!
  execute action
*/
bool
Bhv_Cross::execute( rcsc::PlayerAgent * agent )
{
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Bhv_Cross " );

    int receiver_number = 0;
    rcsc::Vector2D target_point( 40.0, 0.0 );
    double first_speed = 2.0;

    int receiver_conf = 1000;

    if ( get_best_point( agent, NULL ) )
    {
        std::vector< Target >::iterator max_it
            = std::max_element( S_cached_target.begin(),
                                S_cached_target.end(),
                                TargetCmp() );
        receiver_number = max_it->receiver->unum();
        receiver_conf = max_it->receiver->posCount();
        target_point = max_it->target_point;
        first_speed = max_it->first_speed;
    }
    else
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": target not found " );
        if ( agent->world().self().pos().x > 35.0 )
        {
            target_point.assign( 42.0, 0.0 );
        }
        else if ( agent->world().self().pos().x > 30.0 )
        {
            target_point.assign( 36.0, 0.0 );
        }
        else
        {
            target_point.assign( 36.0, 10.0 );
            if ( agent->world().self().pos().y > 0.0 )
            {
                target_point.y *= -1.0;
            }
        }

        first_speed
            = rcsc::calc_first_term_geom_series_last
            ( 1.8,
              agent->world().self().pos().dist( target_point ),
              rcsc::ServerParam::i().ballDecay() );

        first_speed = std::min( first_speed,
                                rcsc::ServerParam::i().ballSpeedMax() );
    }


    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": cross to center (%.2f, %.2f)",
                        target_point.x, target_point.y );
    agent->debugClient().addMessage( "cross" );
    agent->debugClient().setTarget( target_point );
#ifdef USE_SMART_KICK
    rcsc::Body_SmartKick( target_point,
                          first_speed,
                          first_speed * 0.96,
                          3 ).execute( agent );
#else
    rcsc::Body_KickMultiStep( target_point,
                              first_speed,
                              false //true // enforce
                              ).execute( agent );
    if ( receiver_conf == 0 )
    {
        agent->setIntention
            ( new rcsc::IntentionKick( target_point,
                                       first_speed,
                                       2,
                                       true, // enforce
                                       agent->world().time() ) );
    }
#endif
    // register intention
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": register cross kick intention" );

    agent->setNeckAction( new rcsc::Neck_TurnToPoint( target_point ) );

    // register communication
    if ( agent->config().useCommunication()
         && 0 < receiver_number
         && receiver_number < 12 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": set communication" );
        agent->debugClient().addMessage( "SaypCross" );
        agent->addSayMessage( new rcsc::PassMessage( receiver_number,
                                                     target_point,
                                                     agent->effector().queuedNextBallPos(),
                                                     agent->effector().queuedNextBallVel() ) );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!
  static method
*/
bool
Bhv_Cross::get_best_point( const rcsc::PlayerAgent * agent,
                           rcsc::Vector2D * target_point )
{
    search( agent );

    if ( S_cached_target.empty() )
    {
        return false;
    }

    if ( target_point )
    {
        std::vector< Target >::iterator max_it
            = std::max_element( S_cached_target.begin(),
                                S_cached_target.end(),
                                TargetCmp() );
        *target_point = max_it->target_point;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!
  static method
*/
void
Bhv_Cross::search( const rcsc::PlayerAgent * agent )
{
    static rcsc::GameTime s_last_calc_time( -1, 0 );

    if ( s_last_calc_time == agent->world().time() )
    {
        return;
    }

    s_last_calc_time = agent->world().time();

    const rcsc::Rect2D cross_area
        ( rcsc::Vector2D( rcsc::ServerParam::i().theirPenaltyAreaLineX() - 3.0,
                          - rcsc::ServerParam::i().penaltyAreaHalfWidth() + 5.0 ),
          rcsc::Size2D( rcsc::ServerParam::i().penaltyAreaLength() + 3.0,
                        rcsc::ServerParam::i().penaltyAreaWidth() - 10.0 ) );
    const double offside_x = agent->world().offsideLineX();

    S_cached_target.clear();

    rcsc::dlog.addText( rcsc::Logger::CROSS,
                        __FILE__": search. start" );

    const rcsc::PlayerPtrCont::const_iterator mates_end
        = agent->world().teammatesFromSelf().end();
    for ( rcsc::PlayerPtrCont::const_iterator it
              = agent->world().teammatesFromSelf().begin();
          it != mates_end;
          ++it )
    {
        rcsc::dlog.addText( rcsc::Logger::CROSS,
                            "--> teammate %d (%.1f, %.1f)",
                            (*it)->unum(), (*it)->pos().x, (*it)->pos().y );
        if ( (*it)->posCount() > 5 ) continue;
        if ( (*it)->isTackling() ) continue;
        if ( (*it)->pos().x > offside_x + 1.0 ) continue;

        if ( (*it)->distFromSelf() > 30.0 ) break;

        if ( ! cross_area.contains( (*it)->pos() ) )
        {
#ifdef DEBUG
            rcsc::dlog.addText( rcsc::Logger::CROSS,
                                "__ out of bounds" );
#endif
            continue;
        }

        if ( create_close( agent, *it ) )
        {
#ifdef DEBUG
            rcsc::dlog.addText( rcsc::Logger::CROSS,
                                " __ Found close target " );
#endif
            break;
        }

        create_target( agent, *it );
    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": search. end. candidate size = %d",
                        S_cached_target.size() );

}

/*-------------------------------------------------------------------*/
/*!
  static method
*/
bool
Bhv_Cross::create_close( const rcsc::PlayerAgent * agent,
                         const rcsc::PlayerObject * receiver )
{
    if ( receiver->distFromSelf() < 2.0
         || 14.0 < receiver->distFromSelf() )
    {
        return false;
    }

    if ( receiver->pos().x < 40.0
         || receiver->pos().absY() > 15.0 )
    {
        return false;
    }

    if ( receiver->pos().absY() > 7.0
         && receiver->pos().y * agent->world().self().pos().y > 0.0 // same Y side
         && receiver->pos().absY() > agent->world().self().pos().absY() )
    {
        return false;
    }


    if ( agent->world().self().pos().x < 30.0
         || agent->world().self().pos().absY() > 25.0 )
    {
        return false;
    }

    const rcsc::PlayerPtrCont & opps = agent->world().opponentsFromSelf();
    const rcsc::PlayerPtrCont::const_iterator opps_end = opps.end();

    rcsc::Vector2D target_point = receiver->pos();
    target_point.y += receiver->vel().y * 2.0;
    target_point.x += 1.8;
    //target_point.x += 3.0;

    for ( int i = 0; i < 3; i++ )
    {
        target_point.x -= 0.6;
        //target_point.x -= 1.0;
#ifdef DEBUG
        rcsc::dlog.addText( rcsc::Logger::CROSS,
                            " ____ close: check target(%.1f %.1f)",
                            target_point.x, target_point.y );
#endif
        const rcsc::Line2D cross_line( agent->world().ball().pos(),
                                       target_point );
        const double target_dist = agent->world().ball().pos().dist( target_point );

        bool success = true;

        for ( rcsc::PlayerPtrCont::const_iterator it = opps.begin();
              it != opps_end;
              ++it )
        {
            if ( (*it)->posCount() > 5 ) continue;

            if ( (*it)->goalie() )
            {
                if ( (*it)->distFromBall() > target_dist + 3.0 )
                {
                    continue;
                }

                if ( cross_line.dist( (*it)->pos() + (*it)->vel() )
                     < rcsc::ServerParam::i().catchAreaLength() + 1.5 )
                {
                    success = false;
#ifdef DEBUG
                    rcsc::dlog.addText( rcsc::Logger::CROSS,
                                        " ______ close: exist goalie on cross line" );
#endif
                    break;
                }
            }
            else
            {
                if ( (*it)->distFromSelf() > target_dist + 2.0 )
                {
                    continue;
                }

                if ( cross_line.dist( (*it)->pos() + (*it)->vel() )
                     < rcsc::ServerParam::i().defaultKickableArea() + 0.5 )
                {
                    success = false;
#ifdef DEBUG
                    rcsc::dlog.addText( rcsc::Logger::CROSS,
                                        " ______ close: exist opponent %d(%.1f %.1f) on cross line",
                                        (*it)->unum(), (*it)->pos().x, (*it)->pos().y );
#endif
                    break;
                }
            }
        }

        if ( success )
        {
            const double dash_x = std::max( std::fabs( target_point.x - receiver->pos().x )
                                            - receiver->vel().x
                                            - receiver->playerTypePtr()->kickableArea() * 0.5,
                                            0.0 );
            const double dash_step = receiver->playerTypePtr()->cyclesToReachDistance( dash_x );
            const double target_dist = agent->world().ball().pos().dist( target_point );

            double end_speed = 1.8;
            double first_speed = 1000.0;
            double ball_steps_to_target = 1.0;
            do
            {
                first_speed
                    = rcsc::calc_first_term_geom_series_last
                    ( end_speed,
                      target_dist,
                      rcsc::ServerParam::i().ballDecay() );
                if ( first_speed > rcsc::ServerParam::i().ballSpeedMax() )
                {
                    end_speed -= 0.075;
                    continue;
                }

                ball_steps_to_target
                    = rcsc::calc_length_geom_series( first_speed,
                                                     target_dist,
                                                     rcsc::ServerParam::i().ballDecay() );
                if ( dash_step < ball_steps_to_target )
                {
                    break;
                }

                end_speed -= 0.05;
            }
            while ( end_speed > 1.5 );

            if ( first_speed > rcsc::ServerParam::i().ballSpeedMax()
                 || dash_step > ball_steps_to_target )
            {
                rcsc::dlog.addText( rcsc::Logger::CROSS,
                                    " ______ close: cannot kick the ball by desired speed" );
                return false;
            }

            rcsc::dlog.addText( rcsc::Logger::CROSS,
                                "Score=1000. found close. unum=%d(%.2f %.2f) speed=%.1f",
                                receiver->unum(),
                                target_point.x, target_point.y,
                                first_speed );

            S_cached_target.push_back( Target( receiver,
                                               target_point,
                                               first_speed,
                                               1000.0 ) ); // high score
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!
  static method
*/
void
Bhv_Cross::create_target( const rcsc::PlayerAgent * agent,
                          const rcsc::PlayerObject * receiver )
{
    //static const double receiver_dash_speed = 1.0;
    static const double blocker_dash_speed = 1.0;

    const rcsc::PlayerPtrCont & opps = agent->world().opponentsFromSelf();
    const rcsc::PlayerPtrCont::const_iterator opps_end = opps.end();

    const double y_diff2
        = rcsc::square( receiver->pos().y - agent->world().ball().pos().y );
    //const double max_x = rcsc::ServerParam::i().pitchHalfLength() - 2.0;
    const double max_x = std::min( receiver->pos().x + 4.0,
                                   rcsc::ServerParam::i().pitchHalfLength() - 2.0 );

    const rcsc::PlayerType * player_type = agent->world().ourPlayerType( receiver->unum() );

    double dash_step_buf = 0.0;
    if ( receiver->velCount() <= 1
         && receiver->vel().x > 0.3 )
    {
        dash_step_buf = -1.0;
    }
    else if ( receiver->bodyCount() <= 1
              && receiver->body().abs() < 15.0 )
    {
        dash_step_buf = 0.0;
    }
    else
    {
        dash_step_buf = 2.0;
    }

    for ( double receive_x = std::max( receiver->pos().x - 2.0,
                                       rcsc::ServerParam::i().theirPenaltyAreaLineX() );
          receive_x < max_x;
          receive_x += 0.5 )
    {
        double dash_x = std::fabs( receive_x - receiver->pos().x );

        const double dash_step = player_type->cyclesToReachDistance( dash_x - 0.5 );

        double target_dist
            = std::sqrt( std::pow( receive_x - agent->world().ball().pos().x , 2.0 )
                         + y_diff2 );

        //----------------------------------------------------
        double end_speed = 1.8;
        double first_speed = 100.0;
        double ball_steps_to_target = 100.0;

        while ( end_speed > 1.5 )
        {
            first_speed
                = rcsc::calc_first_term_geom_series_last
                ( end_speed,
                  target_dist,
                  rcsc::ServerParam::i().ballDecay() );

            if ( first_speed > rcsc::ServerParam::i().ballSpeedMax() )
            {
                end_speed -= 0.1;
                continue;
            }

            ball_steps_to_target
                = rcsc::calc_length_geom_series( first_speed,
                                                 target_dist ,
                                                 rcsc::ServerParam::i().ballDecay() );

            if ( ball_steps_to_target < dash_step + dash_step_buf )
            {
                end_speed -= 0.075;
                continue;
            }

            break;
        }

        if ( first_speed > rcsc::ServerParam::i().ballSpeedMax()
             || ball_steps_to_target < dash_step )
        {
            rcsc::dlog.addText( rcsc::Logger::CROSS,
                                " ____ calc %d(%.1f %.1f) Failed. speed=%.1f, ball_step=%d, dash_step=%d",
                                receiver->unum(),
                                receive_x, receiver->pos().y,
                                first_speed,
                                ball_steps_to_target, dash_step );
            continue;
        }

        //----------------------------------------------------
        const rcsc::Vector2D target_point( receive_x, receiver->pos().y );
        const rcsc::AngleDeg target_angle
            = ( target_point - agent->world().ball().pos() ).th();
        const rcsc::AngleDeg minus_target_angle = -target_angle;
        const rcsc::Vector2D first_vel
            = rcsc::Vector2D::polar2vector( first_speed, target_angle );
        const double next_speed = first_speed * rcsc::ServerParam::i().ballDecay();

        bool success = true;
        double opp_min_step = 100.0;

#ifdef DEBUG
        rcsc::dlog.addText( rcsc::Logger::CROSS,
                            " ____ calc %d(%.1f %.1f) dash_x=%.1f angle=%.1f  speed=%.1f",
                            receiver->unum(),
                            target_point.x, target_point.y,
                            dash_x,
                            target_angle.degree(),
                            first_speed );
#endif

        for ( rcsc::PlayerPtrCont::const_iterator it = opps.begin();
              it != opps_end;
              ++it )
        {
            double opp_to_target_dist = (*it)->pos().dist( target_point );
            if ( (*it)->goalie() )
            {
                if ( opp_to_target_dist - 3.0 < dash_x )
                {
                    rcsc::dlog.addText( rcsc::Logger::CROSS,
                                        " ______opp to target = %.1f", opp_to_target_dist );
#ifdef DEBUG
                    rcsc::dlog.addText( rcsc::Logger::CROSS,
                                        " ______exist goalie at target point" );
#endif
                    success = false;
                    break;
                }
            }
            else if ( (*it)->pos().dist( target_point ) < dash_x - 3.0 )
            {
#ifdef DEBUG
                rcsc::dlog.addText( rcsc::Logger::CROSS,
                                    " ______exist closer opp %d(%.1f, %.1f)",
                                    (*it)->unum(),
                                    (*it)->pos().x, (*it)->pos().y );
#endif
                success = false;
                break;
            }

            rcsc::Vector2D ball_to_opp = (*it)->pos();
            ball_to_opp -= agent->world().ball().pos();
            ball_to_opp -= first_vel;
            ball_to_opp.rotate( minus_target_angle );

            if ( 0.0 < ball_to_opp.x && ball_to_opp.x < target_dist )
            {
                const double virtual_dash
                    = blocker_dash_speed * std::min( 3, (*it)->posCount() );
                double opp2line_dist = ball_to_opp.absY();
                opp2line_dist -= virtual_dash;
                if ( (*it)->goalie() )
                {
                    opp2line_dist -= rcsc::ServerParam::i().catchAreaLength();
                }
                else
                {
                    opp2line_dist -= (*it)->playerTypePtr()->kickableArea();
                }

                if ( opp2line_dist < 0.0 )
                {
#ifdef DEBUG
                    rcsc::dlog.addText( rcsc::Logger::CROSS,
                                        " ______ opp %d (%.1f %.1f) already on cross line. line_dist=%.1f",
                                        (*it)->unum(), (*it)->pos().x, (*it)->pos().y,
                                        opp2line_dist );
#endif
                    success = false;
                    break;
                }

                const double ball_steps_to_project
                    = rcsc::calc_length_geom_series( next_speed,
                                                     ball_to_opp.x,
                                                     rcsc::ServerParam::i().ballDecay() );

                //double opp_step = opp2line_dist / blocker_dash_speed;
                double opp_step = (*it)->playerTypePtr()->cyclesToReachDistance( opp2line_dist );
                if ( ball_steps_to_project < 0.0
                     || opp_step < ball_steps_to_project )
                {
#ifdef DEBUG
                    rcsc::dlog.addText( rcsc::Logger::CROSS,
                                        "______ opp%d(%.1f %.1f) can reach."
                                        " cycle=%.1f. ball_cycle=%.1f",
                                        (*it)->unum(),
                                        (*it)->pos().x, (*it)->pos().y,
                                        opp_step, ball_steps_to_project );
#endif
                    success = false;
                    break;
                }

#ifdef DEBUG
                rcsc::dlog.addText( rcsc::Logger::CROSS,
                                    "______ ok opp%d(%.1f %.1f)."
                                    " cycle=%.1f. ball_cycle=%.1f",
                                    (*it)->unum(),
                                    (*it)->pos().x, (*it)->pos().y,
                                    opp_step, ball_steps_to_project );
#endif

                if ( opp_step < opp_min_step )
                {
                    opp_min_step = opp_step;
                }
            }
        }

        //----------------------------------------------------
        // evaluation & push back to the cached vector
        if ( success )
        {
            double tmp = 0.0;
            //-----------------------------------------------------------
            //tmp += opp_min_step * 8.0;

            //-----------------------------------------------------------
            tmp -= dash_x;
            //tmp -= ( 47.0 - target_point.x ) * 2.0;

            //-----------------------------------------------------------
            // negative criteria
            double recv_x_diff = std::fabs( receiver->pos().x
                                            - agent->world().ball().pos().x );
            tmp -= rcsc::min_max( -10.0, recv_x_diff * 2.0, 10.0 );

            if ( target_point.x > 40.0
                 && target_point.absY() < 7.0 )
            {
                tmp += 500.0;
            }

            //-----------------------------------------------------------
            //#ifdef DEBUG
            rcsc::dlog.addText( rcsc::Logger::CROSS,
                                "Score=%5.2f. unum=%d(%.1f, %.1f), point(%.2f, %.2f) speed=%.2f",
                                tmp,
                                receiver->unum(), receiver->pos().x, receiver->pos().y,
                                target_point.x, target_point.y, first_speed );
            //#endif
            S_cached_target.push_back( Target( receiver,
                                               target_point,
                                               first_speed,
                                               tmp ) );
        }
    }

}
