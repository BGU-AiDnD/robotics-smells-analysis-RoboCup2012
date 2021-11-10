// -*-c++-*-

/*!
  \file body_clear_ball.cpp
  \brief kick the ball to escape from a dangerous situation
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

#include "body_clear_ball_test.h"

#include "body_smart_kick.h"

#include <rcsc/action/body_kick_one_step.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/line_2d.h>

namespace rcsc {

const double Body_ClearBallTest::S_SEARCH_ANGLE = 8.0;

GameTime Body_ClearBallTest::S_last_calc_time( 0, 0 );
AngleDeg Body_ClearBallTest::S_cached_best_angle = 0.0;

/*-------------------------------------------------------------------*/
/*!
  execute action
*/
bool
Body_ClearBallTest::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Body_ClearBall" );

    if ( ! agent->world().self().isKickable() )
    {
        std::cerr << __FILE__ << ": " << __LINE__
                  << " not ball kickable!"
                  << std::endl;
        dlog.addText( Logger::ACTION,
                      __FILE__":  not kickable" );
        return false;
    }

    if ( S_last_calc_time != agent->world().time() )
    {
        dlog.addText( Logger::CLEAR,
                      __FILE__": update clear angle" );

        double lower_angle, upper_angle;

        if ( agent->world().self().pos().y > ServerParam::i().goalHalfWidth() - 1.0 )
        {
            lower_angle = 0.0;
            upper_angle = 90.0;
        }
        else if ( agent->world().self().pos().y < -ServerParam::i().goalHalfWidth() + 1.0 )
        {
            lower_angle = -90.0;
            upper_angle = 0.0;
        }
        else
        {
            lower_angle = -60.0;
            upper_angle = 60.0;
        }

        S_cached_best_angle = get_best_angle( agent,
                                              lower_angle,
                                              upper_angle,
                                              ! agent->world().self().goalie() );
        S_last_calc_time = agent->world().time();
    }


    const Vector2D target_point
        = agent->world().self().pos()
        + Vector2D::polar2vector(30.0, S_cached_best_angle);

    dlog.addText( Logger::TEAM,
                  __FILE__": clear angle = %.1f",
                  S_cached_best_angle.degree() );

    agent->debugClient().setTarget( target_point );
    agent->debugClient().addLine( agent->world().ball().pos(), target_point );

    if ( agent->world().self().goalie()
         || agent->world().gameMode().type() == GameMode::GoalKick_ )
    {
        Body_KickOneStep( target_point,
                          ServerParam::i().ballSpeedMax()
                          ).execute( agent );
        agent->debugClient().addMessage( "Clear" );
        dlog.addText( Logger::TEAM,
                      __FILE__": goalie or goal_kick register clear kick. one step." );
    }
    else
    {
        Vector2D one_step_vel
            = Body_KickOneStep::get_max_possible_vel
            ( ( target_point - agent->world().ball().pos() ).th(),
              agent->world().self().kickRate(),
              agent->world().ball().vel() );

        if ( one_step_vel.r() > 2.0 )
        {
            Body_KickOneStep( target_point,
                              ServerParam::i().ballSpeedMax()
                              ).execute( agent );
            agent->debugClient().addMessage( "Clear1K" );
            return true;
        }

        agent->debugClient().addMessage( "Clear2K" );
#ifdef USE_SMART_KICK
        Body_SmartKick( target_point,
                        ServerParam::i().ballSpeedMax(),
                        ServerParam::i().ballSpeedMax() * 0.9,
                        2 ).execute( agent );
#else
        Body_KickTwoStep( target_point,
                          ServerParam::i().ballSpeedMax(),
                          false //true // enforce
                          ).execute( agent );
        agent->setIntention
            ( new IntentionKick( target_point,
                                 ServerParam::i().ballSpeedMax(),
                                 1,
                                 true, // enforde
                                 agent->world().time() ) );
#endif
        dlog.addText( Logger::TEAM,
                      __FILE__": register clear kick intention" );
    }
    return true;
}

/*-------------------------------------------------------------------*/
/*!
  lower < uppeer
*/
AngleDeg
Body_ClearBallTest::get_best_angle( const PlayerAgent * agent,
                                    const double & lower_angle,
                                    const double & upper_angle,
                                    const bool clear_mode )
{
    const double angle_range = upper_angle - lower_angle;
    // defalut angle step is 8.0
    const int ANGLE_DIVS
        = static_cast< int >( rint( angle_range / 8.0 ) ) + 1;
    const double angle_step = angle_range / ( ANGLE_DIVS - 1 );

    AngleDeg best_angle = 0.0;
    double best_score = 0.0;

    AngleDeg angle = lower_angle;

    for ( int i = 0;
          i < ANGLE_DIVS;
          ++i, angle += angle_step )
    {
        double score = calc_score( agent, angle );

        if ( clear_mode )
        {
            score *= ( 0.5
                       * ( AngleDeg::sin_deg( 1.5 * angle.abs() + 45.0 )
                           + 1.0) );
        }

        score *= std::pow( 0.95,
                           std::max( 0, agent->world().dirCount( angle ) - 3 ) );

        if ( score > best_score )
        {
            best_angle = angle;
            best_score = score;
        }

        dlog.addText( Logger::CLEAR,
                      "Body_ClearBall.get_best_angle. search_angle=%.0f, score=%f",
                      angle.degree(), score );
    }

    return best_angle;
}

/*-------------------------------------------------------------------*/
/*!

*/
double
Body_ClearBallTest::calc_score( const PlayerAgent * agent,
                                const AngleDeg & target_angle )
{
    static const double kickable_area = 1.2;

    double score = 1000.0;

    const Line2D angle_line( agent->world().self().pos(),
                             target_angle );

    const AngleDeg target_left_angle = target_angle - 30.0;
    const AngleDeg target_right_angle = target_angle + 30.0;

    const PlayerPtrCont::const_iterator end
        = agent->world().opponentsFromSelf().end();
    for ( PlayerPtrCont::const_iterator
              it = agent->world().opponentsFromSelf().begin();
          it != end;
          ++it )
    {
        if ( (*it)->distFromBall() > 40.0 ) continue;

        if ( (*it)->angleFromSelf().isWithin( target_left_angle,
                                              target_right_angle ) )
        {
            Vector2D project_point = angle_line.projection( (*it)->pos() );
            double width = std::max( 0.0,
                                     angle_line.dist( (*it)->pos() ) - kickable_area );
            double dist = agent->world().self().pos().dist( project_point );
            score *= width / dist;
        }
    }

    return score;
}

}
