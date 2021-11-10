// -*-c++-*-

/*!
  \file body_pass.cpp
  \brief advanced pass planning & behavior.
*/

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA, Hiroki SHIMORA

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

#include "bhv_pass_test.h"

#include "pass_course_table.h"
#include "neck_turn_to_receiver.h"

#include "kick_table.h"

#include "body_smart_kick.h"
#include "body_hold_ball2008.h"

#include "bhv_find_player.h"
#include "bhv_pass_kick_find_receiver.h"
#include "body_force_shoot.h"
#include "body_dribble2008.h"
#include "neck_chase_ball.h"
#include "world_model_ext.h"

#include <rcsc/action/body_stop_ball.h>
#include <rcsc/action/neck_turn_to_point.h>

#include <rcsc/player/interception.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/say_message_builder.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/geom/rect_2d.h>
#include <rcsc/geom/sector_2d.h>
#include <rcsc/soccer_math.h>
#include <rcsc/math_util.h>
#include <rcsc/timer.h>

#define DEBUG
#define DEBUG_CLIENT_DEBUG

#define DO_CHECK_BEFORE_PASS

namespace rcsc {

namespace {

int g_pass_counter = 0;

}


std::vector< Bhv_PassTest::PassRoute > Bhv_PassTest::S_cached_pass_route;

/*-------------------------------------------------------------------*/
/*!
  execute action
*/
bool
Bhv_PassTest::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ACTION | Logger::PASS,
                  __FILE__": Bhv_PassTest. execute()" );

    if ( ! agent->world().self().isKickable() )
    {
        std::cerr << __FILE__ << ": " << __LINE__
                  << " not ball kickable!"
                  << std::endl;
        dlog.addText( Logger::ACTION | Logger::PASS,
                      __FILE__":  not kickable" );
        return false;
    }

    const PassRoute * pass = get_best_pass( agent->world() );

#if 0
    bool do_pass_course_table = true;

    if ( ! pass )
    {
        do_pass_course_table = true;
    }
    else if ( pass->type_ == THROUGH )
    {
        do_pass_course_table = false;
    }

    if ( do_pass_course_table )
    {
        if ( doRecursiveSearchPass( agent ) )
        {
            return true;
        }
    }
#endif


#ifdef DEBUG_CLIENT_DEBUG
    const std::vector< PassRoute > & pass_candidates
        = get_pass_route_cont( agent->world() );

    std::vector< PassRoute >::const_iterator end = pass_candidates.end();
    std::vector< PassRoute >::const_iterator it;

    for( it = pass_candidates.begin(); it != end; ++it )
    {
        agent->debugClient().addLine( agent->world().self().pos(),
                                      (*it).receive_point_ );
    }
#endif


    // evaluation
    //   judge situation
    //   decide max kick step
    //

    if ( ! pass )
    {
        return false;
    }

    if ( pass->type_ == DIRECT )
    {
        agent->debugClient().addMessage( "direct pass" );
    }
    else if ( pass->type_ == LEAD )
    {
        agent->debugClient().addMessage( "lead pass" );
    }
    else if ( pass->type_ == THROUGH )
    {
        agent->debugClient().addMessage( "through pass" );
    }
    else if ( pass->type_ == RECURSIVE )
    {
        agent->debugClient().addMessage( "recursive pass" );
    }


    if ( pass->type_ == RECURSIVE )
    {
        doRecursiveSearchPass( agent );
        return true;
    }

    agent->debugClient().setTarget( pass->receive_point_ );

    Body_SmartKick( pass->receive_point_,
                    pass->first_speed_,
                    pass->first_speed_ * 0.96,
                    3 ).execute( agent );

    agent->setNeckAction( new Neck_TurnToReceiver );

    if ( agent->config().useCommunication()
         && pass->receiver_->unum() != Unum_Unknown )
    {
        dlog.addText( Logger::ACTION | Logger::PASS,
                      __FILE__": execute() set pass communication." );
        Vector2D target_buf( 0.0, 0.0 );
        if ( pass->type_ == DIRECT )
        {
            target_buf.assign( 0.0, 0.0 );
        }
        else if ( pass->type_ == LEAD )
        {
            target_buf.assign( 0.0, 0.0 );
        }
        else if ( pass->type_ == THROUGH )
        {
            target_buf = pass->receive_point_ - pass->receiver_->pos();
            target_buf.setLength( 1.0 );
        }

        agent->debugClient().addMessage( "Say:pass" );
        agent->addSayMessage( new PassMessage( pass->receiver_->unum(),
                                               pass->receive_point_ + target_buf,
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
Bhv_PassTest::doRecursiveSearchPass( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const PassCourseTable & table = PassCourseTable::instance( wm , ! wm.teammates().empty() );
    PassCourseTable::CourseSelection first_pass = table.get_first_selection( wm );
    PassCourseTable::CourseSelection pass_result = table.get_result( wm );

    switch( first_pass.type )
    {
    case PassCourseTable::CourseSelection::Shoot:
        if ( Body_ForceShoot().execute( agent ) )
        {
            dlog.addText( Logger::PASS,
                          __FILE__": PassCourseTable/shoot" );
            agent-> setNeckAction( new rcsc_ext::Neck_ChaseBall() );
            return true;
        }
        break;

    case PassCourseTable::CourseSelection::Push:
        {
            Vector2D dribble_target;

            if ( wm.self().pos().x > + 35.0 )
            {
                dribble_target = rcsc_ext::ServerParam_opponentTeamFarGoalPostPos
                    ( wm.self().pos() );
            }
            else
            {
                dribble_target = wm.self().pos() + Vector2D( 10.0, 0.0 );
            }

            Body_Dribble2008( dribble_target,
                              1.0,
                              ServerParam::i().maxPower(),
                              3 ).execute( agent );
            break;
        }

    case PassCourseTable::CourseSelection::FindPassReceiver:
    case PassCourseTable::CourseSelection::Pass:
        {
            table.setDebugPassRoute( agent );

            const AbstractPlayerObject * target_player = wm.ourPlayer( first_pass.player_number );
            Vector2D target_point = target_player->inertiaPoint( 1 );

            if ( first_pass.type == PassCourseTable::CourseSelection::FindPassReceiver
                 //&& wm.interceptTable()->opponentReachCycle() >= 3 )
                 && rcsc_ext::getDistPlayerNearestToPoint( wm,
                                                           wm.ball().pos() + wm.ball().vel(),
                                                           wm.opponents(),
                                                           10 ) > 4.0 )
            {
                agent->debugClient().addCircle( agent->world().self().pos(), 3.0 );
                agent->debugClient().addCircle( agent->world().self().pos(), 5.0 );
                agent->debugClient().addCircle( agent->world().self().pos(), 10.0 );

                if ( Bhv_PassKickFindReceiver().execute( agent ) )
                {
                    dlog.addText( Logger::ACTION,
                                  __FILE__": doRecursiveSearchPass() find receiver." );
                    return true;
                }
                else
                {
                    Body_HoldBall2008( true,
                                       target_point,
                                       target_point ).execute( agent );
                    agent->setNeckAction( new Neck_TurnToPoint( target_point ) );
                    agent->debugClient().addMessage( "RecursivePass:FindHold" );
                    dlog.addText( Logger::ACTION,
                                  __FILE__": (doRecursiveSearchPass) hold ball." );
                    return true;
                }
            }

            double target_ball_speed
                = PassCourseTable::get_recursive_pass_ball_speed( wm.ball().pos().dist( target_point ) );

            Body_SmartKick( target_point,
                            target_ball_speed,
                            target_ball_speed * 0.96,
                            3 ).execute( agent );
            agent->setNeckAction( new Neck_TurnToPoint( target_point ) );

            agent->debugClient().setTarget( target_player->unum() );

            if ( agent->config().useCommunication()
                 && first_pass.player_number != Unum_Unknown )
            {
                dlog.addText( Logger::ACTION,
                              __FILE__": (doRecursiveSearchPass) set pass communication." );
            }

            Vector2D target_buf = target_point - agent->world().self().pos();
            target_buf.setLength( 1.0 );

            agent->addSayMessage( new PassMessage
                                  ( first_pass.player_number,
                                    target_point + target_buf,
                                    agent->effector().queuedNextBallPos(),
                                    agent->effector().queuedNextBallVel() ) );

            dlog.addText( Logger::PASS,
                          __FILE__": (doRecursiveSearchPass) pass to "
                          "%d (%.1f %.1f) recv_pos=(%.1f %.1f)",
                          target_player->unum(),
                          target_player->pos().x, target_player->pos().y,
                          target_point.x, target_point.y );
            return true;
            break;
        }

    case PassCourseTable::CourseSelection::Hold:
        Body_HoldBall2008().execute( agent );

        agent->debugClient().addMessage( "hold" );
        dlog.addText( Logger::PASS,
                      __FILE__": hold" );
        return true;
        break;
    }

    return false;
}



/*-------------------------------------------------------------------*/
/*!
  static method
*/
const
Bhv_PassTest::PassRoute *
Bhv_PassTest::get_best_pass( const WorldModel & world )
{
    static GameTime s_last_calc_time( 0, 0 );
    static const PassRoute * s_best_pass = static_cast< const PassRoute * >( 0 );

    if ( s_last_calc_time == world.time() )
    {
        return s_best_pass;
    }

    s_last_calc_time = world.time();
    s_best_pass = static_cast< const PassRoute * >( 0 );

    MSecTimer timer;

    // create route
    create_routes( world );

    dlog.addText( Logger::PASS,
                  __FILE__": get_best_pass() elapsed %.3f [ms]",
                  timer.elapsedReal() );;

    if ( ! S_cached_pass_route.empty() )
    {
        std::vector< PassRoute >::iterator it
            = std::max_element( S_cached_pass_route.begin(),
                                S_cached_pass_route.end(),
                                PassRouteScoreComp() );
        s_best_pass = &(*it);
        dlog.addText( Logger::PASS,
                      __FILE__": get_best_pass() size=%d. target=(%.1f %.1f)"
                      " speed=%.3f  receiver=%d",
                      S_cached_pass_route.size(),
                      s_best_pass->receive_point_.x,
                      s_best_pass->receive_point_.y,
                      s_best_pass->first_speed_,
                      s_best_pass->receiver_->unum() );
    }

    return s_best_pass;
}

/*-------------------------------------------------------------------*/
/*!
  static method
*/
void
Bhv_PassTest::create_routes( const WorldModel & world )
{
    // reset old info
    S_cached_pass_route.clear();
    g_pass_counter = 0;

    // loop candidate teammates
    const PlayerPtrCont::const_iterator
        t_end = world.teammatesFromSelf().end();
    for ( PlayerPtrCont::const_iterator
              it = world.teammatesFromSelf().begin();
          it != t_end;
          ++it )
    {
        if ( (*it)->goalie() && (*it)->pos().x < -22.0 )
        {
            // goalie is rejected.
            continue;
        }
        if ( (*it)->posCount() > 3 )
        {
            // low confidence players are rejected.
            continue;
        }
        if ( (*it)->isTackling() )
        {
            // player is freezed
            continue;
        }
        if ( (*it)->pos().x > world.offsideLineX() )
        {
            // offside players are rejected.
#ifdef DEBUG
            dlog.addText( Logger::PASS,
                          " receiver %d is offside",
                          (*it)->unum() );
#endif
            continue;
        }
        if ( (*it)->pos().x < world.ball().pos().x - 25.0 )
        {
            // too back
            continue;
        }

        // create & verify each route
#if 1
        create_direct_pass( world, *it );
#endif
        create_lead_pass( world, *it );
        if ( world.self().pos().x > world.offsideLineX() - 20.0 )
        {
            create_through_pass( world, *it );
        }
    }

    // create recursice pass
    create_recursive_pass( world );

    ////////////////////////////////////////////////////////////////
    // evaluation
    evaluate_routes( world );
}

/*-------------------------------------------------------------------*/
/*!
  static method
*/
void
Bhv_PassTest::create_direct_pass( const WorldModel & world,
                                  const PlayerObject * receiver )
{
    static const double MAX_DIRECT_PASS_DIST
        = 0.8 * inertia_final_distance( ServerParam::i().ballSpeedMax(),
                                        ServerParam::i().ballDecay() );
#ifdef DEBUG
    dlog.addText( Logger::PASS,
                  "Create_direct_pass() to %d pos(%.1f %.1f) vel(5.2f %.2f)",
                  receiver->unum(),
                  receiver->pos().x, receiver->pos().y,
                  receiver->vel().x, receiver->vel().y );
#endif

    // out of pitch?
    if ( receiver->pos().absX() > ServerParam::i().pitchHalfLength() - 3.0
         || receiver->pos().absY() > ServerParam::i().pitchHalfWidth() - 3.0 )
    {
#ifdef DEBUG
        dlog.addText( Logger::PASS,
                      "__ out of pitch" );
#endif
        return;
    }

    /////////////////////////////////////////////////////////////////
    // too far
    if ( receiver->distFromSelf() > MAX_DIRECT_PASS_DIST )
    {
#ifdef DEBUG
        dlog.addText( Logger::PASS,
                      "__ over max distance %.2f > %.2f",
                      receiver->distFromSelf(),
                      MAX_DIRECT_PASS_DIST );
#endif
        return;
    }
    // too close
    if ( receiver->distFromSelf() < 6.0 )
    {
#ifdef DEBUG
        dlog.addText( Logger::PASS,
                      "__ too close. dist = %.2f  canceled",
                      receiver->distFromSelf() );
#endif
        return;
    }

    double opp_dist = 0.0;
    const PlayerObject * opp = world.getOpponentNearestTo( receiver->pos(),
                                                           10,
                                                           &opp_dist );

    /////////////////
    if ( receiver->pos().x < world.self().pos().x + 5.0 )
    {
        if ( ( ! opp || opp_dist > 12.0 )
             && receiver->posCount() <= 2
             && receiver->pos().x > world.self().pos().x - 10.0
             && receiver->pos().x > 40.0 )
        {
            // safety
        }
        else if ( receiver->angleFromSelf().abs() < 40.0
                  || ( receiver->pos().x > world.ourDefenseLineX() + 10.0
                       && receiver->pos().x > 0.0 )
                  )
        {
            // "DIRECT: not defender.";
        }
        else if ( receiver->angleFromSelf().abs() < 100.0
                  && receiver->posCount() <= 2
                  && receiver->pos().x > -20.0
                  && opp_dist > 6.0 )
        {
            // XXX
        }
        else if ( receiver->seenPosCount() <= 2
                  && receiver->pos().x > -30.0
                  && opp_dist > 12.0 )
        {
            // XXX
        }
        else
        {
            // "DIRECT: back.;
#ifdef DEBUG
            dlog.addText( Logger::PASS,
                          "__ back pass. DF Line=%.1f. canceled",
                          world.ourDefenseLineX() );
#endif
            return;
        }
    }

    // not safety area
    if ( receiver->pos().x < -20.0 )
    {
        if ( ( ! opp || opp_dist > 12.0 )
             && receiver->posCount() <= 2
             && receiver->pos().x > world.self().pos().x - 10.0
             && receiver->pos().x > 40.0 )
        {
            // safety
        }
        else if ( receiver->pos().x > world.self().pos().x + 13.0 )
        {
            // safety clear??
        }
        else if ( receiver->pos().x > world.self().pos().x + 5.0
                  && receiver->pos().absY() > 20.0
                  && fabs(receiver->pos().y - world.self().pos().y) < 20.0
                  )
        {
            // safety area
        }

        else
        {
            // dangerous
#ifdef DEBUG
            dlog.addText( Logger::PASS,
                          "__ receiver is in dangerous area. canceled" );
#endif
            return;
        }
    }

    /////////////////////////////////////////////////////////////////

    // set base target
    Vector2D base_player_pos = receiver->pos();
    if ( receiver->velCount() < 3 )
    {
        Vector2D fvel = receiver->vel();
        const PlayerType * type = world.ourPlayerType( receiver->unum() );
        //fvel /= ServerParam::i().defaultPlayerDecay();
        fvel /= type->playerDecay();
        fvel *= min_max( 1, receiver->velCount(), 2 );
        base_player_pos += fvel;
    }

    const Vector2D receiver_rel = base_player_pos - world.ball().pos();
    const double receiver_dist = receiver_rel.r();
    const AngleDeg receiver_angle = receiver_rel.th();

#ifdef DEBUG
    dlog.addText( Logger::PASS,
                  "__ receiver. predict pos(%.2f %.2f) rel(%.2f %.2f)"
                  "  dist=%.2f  angle=%.1f",
                  base_player_pos.x, base_player_pos.y,
                  receiver_rel.x, receiver_rel.y,
                  receiver_dist,
                  receiver_angle.degree() );

#endif

    double end_speed = 1.8; //2.0;
    //= ServerParam::i().ballSpeedMax()
    //* std::pow( ServerParam::i().ballDecay(), 2 );
    double first_speed = 100.0;
    do
    {
        first_speed
            = calc_first_term_geom_series_last
            ( end_speed,
              receiver_dist,
              ServerParam::i().ballDecay() );
        if ( first_speed < ServerParam::i().ballSpeedMax() )
        {
            break;
        }
        end_speed -= 0.075;
    }
    while ( end_speed > 0.8 );


    if ( first_speed > ServerParam::i().ballSpeedMax() )
    {
#ifdef DEBUG
        dlog.addText( Logger::PASS,
                      "__ ball first speed= %.3f.  too high. canceled",
                      first_speed );
#endif
        return;
    }

    // add strictly direct pass

    if ( verify_direct_pass( world,
                             receiver,
                             base_player_pos,
                             receiver_dist,
                             receiver_angle,
                             first_speed ) )
    {
        S_cached_pass_route
            .push_back( PassRoute( DIRECT,
                                   receiver,
                                   base_player_pos,
                                   first_speed,
                                   can_kick_by_one_step( world,
                                                         first_speed,
                                                         receiver_angle )
                                   )
                        );
        dlog.addText( Logger::PASS,
                      "ok__%d direct unum=%d pos=(%.1f %.1f). first_speed=%.2f end_speed=%.2f",
                      g_pass_counter,
                      receiver->unum(),
                      base_player_pos.x, base_player_pos.y,
                      first_speed, end_speed );
        dlog.addCircle( Logger::PASS,
                        base_player_pos, 0.2,
                        "#FF0000" );
    }
    else
    {
        dlog.addText( Logger::PASS,
                      "x__ direct unum=%d pos=(%.1f %.1f). first_speed=%.2f end_speed=%.2f",
                      receiver->unum(),
                      base_player_pos.x, base_player_pos.y,
                      first_speed, end_speed );
        dlog.addRect( Logger::PASS,
                      base_player_pos.x - 0.1, base_player_pos.y - 0.1, 0.2, 0.2,
                      "#0000FF" );
    }

    if ( receiver_dist < 10.0 )
    {
        return;
    }

    // add kickable edge points
    double kickable_angle_buf = 360.0 * ( ServerParam::i().defaultKickableArea()
                                          / (2.0 * M_PI * receiver_dist) );
    first_speed *= ServerParam::i().ballDecay();

    // right side
    Vector2D target_new = world.ball().pos();
    AngleDeg angle_new = receiver_angle;
    angle_new += kickable_angle_buf;
    target_new += Vector2D::polar2vector(receiver_dist, angle_new);

    if ( verify_direct_pass( world,
                             receiver,
                             target_new,
                             receiver_dist,
                             angle_new,
                             first_speed ) )
    {
        S_cached_pass_route
            .push_back( PassRoute( DIRECT,
                                   receiver,
                                   target_new,
                                   first_speed,
                                   can_kick_by_one_step( world,
                                                         first_speed,
                                                         angle_new )
                                   )
                        );
        dlog.addText( Logger::PASS,
                      "ok__%d direct unum=%d pos=(%.1f %.1f). first_speed=%.2f end_speed=%.2f",
                      g_pass_counter,
                      receiver->unum(),
                      target_new.x, target_new.y,
                      first_speed, end_speed );
        dlog.addCircle( Logger::PASS,
                        target_new, 0.2,
                        "#FF0000" );
    }
    else
    {
        dlog.addText( Logger::PASS,
                      "x__ direct unum=%d pos=(%.1f %.1f). first_speed=%.2f end_speed=%.2f",
                      receiver->unum(),
                      target_new.x, target_new.y,
                      first_speed, end_speed );
        dlog.addRect( Logger::PASS,
                      target_new.x - 0.1, target_new.y - 0.1, 0.2, 0.2,
                      "#0000FF" );
    }

    // left side
    target_new = world.ball().pos();
    angle_new = receiver_angle;
    angle_new -= kickable_angle_buf;
    target_new += Vector2D::polar2vector( receiver_dist, angle_new );

    if ( verify_direct_pass( world,
                             receiver,
                             target_new,
                             receiver_dist,
                             angle_new,
                             first_speed ) )
    {
        S_cached_pass_route
            .push_back( PassRoute( DIRECT,
                                   receiver,
                                   target_new,
                                   first_speed,
                                   can_kick_by_one_step( world,
                                                         first_speed,
                                                         angle_new )
                                   )
                        );
        dlog.addText( Logger::PASS,
                      "ok__%d direct unum=%d pos=(%.1f %.1f). first_speed=%.2f end_speed=%.2f",
                      g_pass_counter,
                      receiver->unum(),
                      target_new.x, target_new.y,
                      first_speed, end_speed );
        dlog.addCircle( Logger::PASS,
                        target_new, 0.2,
                        "#FF0000" );
    }
    else
    {
        dlog.addText( Logger::PASS,
                      "x__ direct unum=%d pos=(%.1f %.1f). first_speed=%.2f end_speed=%.2f",
                      receiver->unum(),
                      target_new.x, target_new.y,
                      first_speed, end_speed );
        dlog.addRect( Logger::PASS,
                      target_new.x - 0.1, target_new.y - 0.1, 0.2, 0.2,
                      "#0000FF" );
    }
}

/*-------------------------------------------------------------------*/
/*!
  static method
*/
void
Bhv_PassTest::create_lead_pass( const WorldModel & world,
                                const PlayerObject * receiver )
{
    static const double MAX_LEAD_PASS_DIST
        = 0.7 * inertia_final_distance( ServerParam::i().ballSpeedMax(),
                                        ServerParam::i().ballDecay() );
    static const
        Rect2D shrinked_pitch( Vector2D( -ServerParam::i().pitchHalfLength() + 3.0,
                                         -ServerParam::i().pitchHalfWidth() + 3.0 ),
                               Size2D( ServerParam::i().pitchLength() - 6.0,
                                       ServerParam::i().pitchWidth() - 6.0 ) );

    //static const double receiver_dash_speed = 0.9;
    //static const double receiver_dash_speed = 0.8;

#ifdef DEBUG
    dlog.addText( Logger::PASS,
                  "Create_lead_pass() to %d(%.1f %.1f)",
                  receiver->unum(),
                  receiver->pos().x, receiver->pos().y );
#endif

    /////////////////////////////////////////////////////////////////
    // too far
    if ( receiver->distFromSelf() > MAX_LEAD_PASS_DIST )
    {
#ifdef DEBUG
        dlog.addText( Logger::PASS,
                      "__ over max distance %.2f > %.2f",
                      receiver->distFromSelf(), MAX_LEAD_PASS_DIST );
#endif
        return;
    }
    // too close
    if ( receiver->distFromSelf() < 2.0 )
    {
#ifdef DEBUG
        dlog.addText( Logger::PASS,
                      "__ too close %.2f",
                      receiver->distFromSelf() );
#endif
        return;
    }

    const PlayerType * player_type = world.ourPlayerType( receiver->unum() );

    Vector2D receiver_pos = receiver->pos();
    if ( receiver->velCount() < 3 )
    {
        Vector2D fvel = receiver->vel();
        fvel /= player_type->playerDecay();
        fvel *= std::min( receiver->velCount() + 1, 2 );
        receiver_pos += fvel;
    }

#ifdef DEBUG
    dlog.addText( Logger::PASS,
                  "__ receiver predict pos(%.2f %.2f)",
                  receiver_pos.x, receiver_pos.y );
#endif

    if ( receiver_pos.x < world.self().pos().x - 15.0
         && receiver_pos.x < 15.0 )
    {
#ifdef DEBUG
        dlog.addText( Logger::PASS,
                      "__ receiver is back cancel" );
#endif
        return;
    }
    //
    if ( receiver_pos.x < -10.0
         && std::fabs( receiver_pos.y - world.self().pos().y ) > 20.0 )
    {
#ifdef DEBUG
        dlog.addText( Logger::PASS,
                      "__ receiver is in our field. or Y diff is big" );
#endif
        return;
    }

    const Vector2D receiver_rel = receiver_pos - world.ball().pos();
    const double receiver_dist = receiver_rel.r();
    const AngleDeg receiver_angle = receiver_rel.th();

    const double circum = 2.0 * receiver_dist * M_PI;
    const double angle_step_abs = std::max( 5.0, 360.0 * ( 2.0 / circum ));
    /// angle loop
    double total_add_angle_abs;
    int count;
    for ( total_add_angle_abs = angle_step_abs, count = 0;
          total_add_angle_abs < 16.0 && count < 5;
          total_add_angle_abs += angle_step_abs, ++count )
    {
        /////////////////////////////////////////////////////////////
        // estimate required first ball speed
        //const double dash_step
        //= ( (angle_step_abs / 360.0) * circum ) / receiver_dash_speed + 2.0;
        const double receiver_travel = (angle_step_abs / 360.0) * circum;
        const double dash_step
            = player_type->cyclesToReachDistance( receiver_travel );

        //double end_speed = player_type->realSpeedMax() + 0.1;
        double end_speed = player_type->realSpeedMax();
        //double end_speed = 1.2 - 0.11 * count; // Magic Number
        double first_speed = 100.0;
        double ball_steps_to_target = 100.0;
        do
        {
            first_speed
                = calc_first_term_geom_series_last
                ( end_speed,
                  receiver_dist,
                  ServerParam::i().ballDecay() );
            if ( first_speed < ServerParam::i().ballSpeedMax() )
            {
                break;
            }
            ball_steps_to_target
                = calc_length_geom_series( first_speed,
                                           receiver_dist,
                                           ServerParam::i().ballDecay() );
            if ( dash_step + 3.5 < ball_steps_to_target )
            {
                break;
            }
            end_speed -= 0.1;
        }
        while ( end_speed > 0.5 );


        if ( first_speed > ServerParam::i().ballSpeedMax() )
        {
            continue;
        }

        /////////////////////////////////////////////////////////////
        // angle plus minus loop
        for ( int i = 0; i < 2; i++ )
        {
            AngleDeg target_angle = receiver_angle;
            if ( i == 0 )
            {
                target_angle -= total_add_angle_abs;
            }
            else
            {
                target_angle += total_add_angle_abs;
            }

            // check dir confidence
            int max_count = 100, ave_count = 100;
            world.dirRangeCount( target_angle, 20.0,
                                 &max_count, NULL, &ave_count );
            if ( max_count > 9 || ave_count > 3 )
            {
                continue;
            }

            const Vector2D target_point
                = world.ball().pos()
                + Vector2D::polar2vector(receiver_dist, target_angle);

            /////////////////////////////////////////////////////////////////
            // ignore back pass
            if ( target_point.x < 0.0
                 && target_point.x <  world.self().pos().x )
            {
                continue;
            }

            if ( target_point.x < 0.0
                 && target_point.x < receiver_pos.x - 3.0 )
            {
                continue;
            }
            if ( target_point.x < receiver_pos.x - 6.0 )
            {
                continue;
            }
            // out of pitch
            if ( ! shrinked_pitch.contains( target_point ) )
            {
                continue;
            }
            // not safety area
            if ( target_point.x < -10.0 )
            {
                if ( target_point.x < world.ourDefenseLineX() + 10.0 )
                {
                    continue;
                }
                else if ( target_point.x > world.self().pos().x + 20.0
                          && fabs( target_point.y - world.self().pos().y ) < 20.0 )
                {
                    // safety clear ??
                }
                else if ( target_point.x > world.self().pos().x + 5.0 // forward than me
                          && std::fabs( target_point.y - world.self().pos().y ) < 20.0
                          ) // out side of me
                {
                    // safety area
                }
                else if ( target_point.x > world.ourDefenseLineX() + 20.0 )
                {
                    // safety area
                }
                else
                {
                    // dangerous
                    continue;
                }
            }
            /////////////////////////////////////////////////////////////////

            // add lead pass route
            // this methid is same as through pass verification method.
            if ( verify_through_pass( world,
                                      receiver,
                                      receiver_pos,
                                      target_point,
                                      receiver_dist,
                                      target_angle,
                                      first_speed,
                                      ball_steps_to_target,
                                      false ) )
            {
                S_cached_pass_route
                    .push_back( PassRoute( LEAD,
                                           receiver,
                                           target_point,
                                           first_speed,
                                           can_kick_by_one_step( world,
                                                                 first_speed,
                                                                 target_angle ) ) );
                dlog.addText( Logger::PASS,
                              "ok__%d lead to unum=%d pos=(%.1f %.1f) angle=%.1f first_speed=%.1f",
                              g_pass_counter,
                              receiver->unum(),
                              target_point.x, target_point.y,
                              target_angle.degree(),
                              first_speed );
                dlog.addCircle( Logger::PASS,
                                target_point, 0.2,
                                "#FF0000" );
            }
            else
            {
                dlog.addText( Logger::PASS,
                              "x__ lead to unum=%d pos=(%.1f %.1f) angle=%.1f first_speed=%.1f",
                              receiver->unum(),
                              target_point.x, target_point.y,
                              target_angle.degree(),
                              first_speed );
                dlog.addRect( Logger::PASS,
                              target_point.x - 0.1, target_point.y - 0.1, 0.2, 0.2,
                              "#0000FF" );
            }

        }
    }
}

/*-------------------------------------------------------------------*/
/*!
  static method
*/
void
Bhv_PassTest::create_through_pass(const WorldModel & world,
                                  const PlayerObject * receiver)
{
    static const double MAX_THROUGH_PASS_DIST
        = 0.9 * inertia_final_distance( ServerParam::i().ballSpeedMax(),
                                        ServerParam::i().ballDecay() );

    ////////////////////////////////////////////////////////////////
    static const
        Rect2D shrinked_pitch( Vector2D( -ServerParam::i().pitchHalfLength() + 5.0,
                                         -ServerParam::i().pitchHalfWidth() + 5.0 ),
                               Size2D( ServerParam::i().pitchLength() - 10.0,
                                       ServerParam::i().pitchWidth() - 10.0 ) );

    //static const double receiver_dash_speed = 1.0;
    //static const double receiver_dash_speed = 0.85;

    static const double min_first_speed = 1.8;

    static const double S_min_dash = 5.5; //8.0;
    static const double S_max_dash = 25.0;
    static const double S_dash_range = S_max_dash - S_min_dash;
    static const double S_dash_inc = 2.5;
    static const int S_dash_loop
        = static_cast< int >( std::ceil( S_dash_range / S_dash_inc ) ) + 1;

    static const AngleDeg S_min_angle = -20.0;
    static const AngleDeg S_max_angle = 20.0;
    static const double S_angle_range = ( S_min_angle - S_max_angle ).abs();
    static const double S_angle_inc = 8.0;
    static const int S_angle_loop
        = static_cast< int >( std::ceil( S_angle_range / S_angle_inc ) ) + 1;

#ifdef DEBUG
    dlog.addText( Logger::PASS,
                  "Create_through_pass() to %d(%.1f %.1f)",
                  receiver->unum(),
                  receiver->pos().x, receiver->pos().y );
#endif

    if ( receiver->angleFromSelf().abs() > 135.0 )
    {
#ifdef DEBUG
        dlog.addText( Logger::PASS,
                      "__ receiver angle is too back" );
#endif
        return;
    }

    const PlayerType * player_type = world.ourPlayerType( receiver->unum() );

    Vector2D receiver_pos = receiver->pos();
    if ( receiver->velCount() < 3 )
    {
        Vector2D fvel = receiver->vel();
        fvel /= player_type->playerDecay();
        fvel *= std::min( receiver->velCount() + 1, 2 );
        receiver_pos += fvel;
    }

#ifdef DEBUG
    dlog.addText( Logger::PASS,
                  "__ receiver predict pos(%.2f %.2f)",
                  receiver_pos.x, receiver_pos.y );
#endif

    if ( receiver_pos.x < world.self().pos().x - 10.0 )
    {
#ifdef DEBUG
        dlog.addText( Logger::PASS,
                      "__ receiver is back" );
#endif
        return;
    }
    if ( std::fabs( receiver_pos.y - world.self().pos().y ) > 35.0 )
    {
#ifdef DEBUG
        dlog.addText( Logger::PASS,
                      "__ receiver Y diff is big" );
#endif
        return;
    }
    if ( world.ourDefenseLineX() < 0.0
         && receiver_pos.x < world.ourDefenseLineX() - 15.0 )
    {
#ifdef DEBUG
        dlog.addText( Logger::PASS,
                      "__ receiver is near to defense line" );
#endif
        return;
    }
    if ( world.offsideLineX() < 30.0
         && receiver_pos.x < world.offsideLineX() - 15.0 )
    {
#ifdef DEBUG
        dlog.addText( Logger::PASS,
                      "__ receiver is far from offside line" );
#endif
        return;
    }


    const bool pass_request = ( world.audioMemory().passRequestTime().cycle()
                                > world.time().cycle() - 2
                                && ! world.audioMemory().passRequest().empty()
                                && world.audioMemory().passRequest().front().sender_
                                == receiver->unum() );

    // angle loop
    AngleDeg dash_angle = S_min_angle;
    for ( int i = 0; i < S_angle_loop; ++i, dash_angle += S_angle_inc )
    {
        const Vector2D base_dash = Vector2D::polar2vector( 1.0, dash_angle );

        // dash dist loop
        double dash_dist = S_min_dash;
        for ( int j = 0; j < S_dash_loop; ++j, dash_dist += S_dash_inc )
        {
            Vector2D target_point = base_dash;
            target_point *= dash_dist;
            target_point += receiver->pos();

            if ( ! shrinked_pitch.contains( target_point ) )
            {
                // out of pitch
                continue;
            }
#if 0
            if ( target_point.x < 35.0
                 && target_point.x < receiver->pos().x + 10.0 )
            {
                // no good run
                continue;
            }
#endif
            if ( target_point.x < world.self().pos().x + 3.0 )
            {
#ifdef DEBUG
                dlog.addText( Logger::PASS,
                              "cc__ through unum=%d pos=(%.1f %.1f) < self.x + 3",
                              receiver->unum(),
                              target_point.x, target_point.y );
#endif
                continue;
            }

            const Vector2D target_rel = target_point - world.ball().pos();
            const double target_dist = target_rel.r();
            const AngleDeg target_angle = target_rel.th();

            // check dir confidence
            int max_count = 100, ave_count = 100;
            world.dirRangeCount( target_angle, 20.0,
                                 &max_count, NULL, &ave_count );
            //if ( max_count > 9 || ave_count > 3 )
            if ( max_count > 9 || ave_count > 4 )
            {
#ifdef DEBUG
                dlog.addText( Logger::PASS,
                              "cc__ through unum=%d pos=(%.1f %.1f)  low dir accuracy max=%d ave=%d",
                              receiver->unum(),
                              target_point.x, target_point.y,
                              max_count, ave_count );
#endif
                continue;
            }

            if ( target_dist > MAX_THROUGH_PASS_DIST ) // dist range over
            {
#ifdef DEBUG
                dlog.addText( Logger::PASS,
                              "cc__ through unum=%d pos=(%.1f %.1f) over max dist %.1f",
                              receiver->unum(),
                              target_point.x, target_point.y,
                              target_dist );
#endif
                continue;
            }

            if ( target_dist < dash_dist ) // I am closer than receiver
            {
#ifdef DEBUG
                dlog.addText( Logger::PASS,
                              "cc__ through unum=%d pos=(%.1f %.1f) target_dist=%.1f < dash_dist=%.1f",
                              receiver->unum(),
                              target_point.x, target_point.y,
                              target_dist, dash_dist );
#endif
                continue;
            }

            const double dash_step = player_type->cyclesToReachDistance( dash_dist );
            double dash_step_buf = 0.0;

            if ( pass_request )
            {
                //dash_step_buf = -0.5 - 0.01 * dash_dist;
                dash_step_buf = -0.5;
                dlog.addText( Logger::PASS,
                              "__ through unum=%d pos=(%.1f %.1f) dash_step_buf=%.2f pass_request",
                              receiver->unum(),
                              target_point.x, target_point.y,
                              dash_step_buf );
            }
            else if ( receiver->velCount() == 0
                      && ( receiver->vel().th() - dash_angle ).abs() < 15.0
                      && receiver->vel().r() > 0.2 )
            {
                //dash_step_buf = -0.5 - 0.01 * dash_dist;
                //dash_step_buf = -0.5 - 0.005 * dash_dist;
                //dash_step_buf = -0.5 - 0.003 * dash_dist; // 2008-07-18
                dash_step_buf = -0.5 - 0.0035 * dash_dist; // 2008-07-18
                dlog.addText( Logger::PASS,
                              "__ through unum=%d pos=(%.1f %.1f) dash_step_buf=%.2f running",
                              receiver->unum(),
                              target_point.x, target_point.y,
                              dash_step_buf );
            }
            else if ( receiver->bodyCount() <= 1
                      && ( receiver->body() - dash_angle ).abs() < 15.0 )
            {
                //dash_step_buf = -0.01 * dash_dist;
                //dash_step_buf = -0.005 * dash_dist;
                //dash_step_buf = -0.003 * dash_dist; // 2008-07-18
                dash_step_buf = -0.0035 * dash_dist; // 2008-07-18
                dlog.addText( Logger::PASS,
                              "__ through unum=%d pos=(%.1f %.1f) dash_step_buf=%.2f body_facing",
                              receiver->unum(),
                              target_point.x, target_point.y,
                              dash_step_buf );
            }
            else if ( dash_dist > 15.0 )
            {
                //dash_step_buf = 0.5 - 0.01 * dash_dist;
                dash_step_buf = 0.5 - 0.005 * dash_dist; // 2008-07-18
                dlog.addText( Logger::PASS,
                              "____ through unum=%d pos=(%.1f %.1f) dash_step_buf=%.2f long_run",
                              receiver->unum(),
                              target_point.x, target_point.y,
                              dash_step_buf );
            }
            else
            {
                dash_step_buf = 2.0;
                dlog.addText( Logger::PASS,
                              "__ through unum=%d pos=(%.1f %.1f) dash_step_buf=%.2f default",
                              receiver->unum(),
                              target_point.x, target_point.y,
                              dash_step_buf );
            }

            dash_step_buf += bound( 0, receiver->posCount() - 1, 3 );

            //double end_speed = 0.81;//0.65
            double end_speed = 1.75; //1.5; //2.0;
            double first_speed = 100.0;
            double ball_steps_to_target = 100.0;
            do
            {
                first_speed
                    = calc_first_term_geom_series_last
                    ( end_speed,
                      target_dist,
                      ServerParam::i().ballDecay() );
                if ( first_speed > ServerParam::i().ballSpeedMax() )
                {
                    end_speed -= 0.1;
                    continue;
                }

                if ( first_speed < min_first_speed ) break;

                ball_steps_to_target
                    = calc_length_geom_series( first_speed,
                                               target_dist,
                                               ServerParam::i().ballDecay() );
                if ( dash_step + dash_step_buf < ball_steps_to_target )
                {
                    break;
                }

                end_speed -= 0.075;
            }
            while ( end_speed > 0.1 );

            if ( first_speed > ServerParam::i().ballSpeedMax()
                 || first_speed < min_first_speed
                 || dash_step + dash_step_buf > ball_steps_to_target )
            {
                dlog.addText( Logger::PASS,
                              "cc__ through unum=%d pos=(%.1f %.1f) first_speed=%.1f"
                              " end_speed=%.1f dash_step=%.1f ball_step=%.1f",
                              receiver->unum(),
                              target_point.x, target_point.y,
                              first_speed,
                              end_speed,
                              dash_step,
                              ball_steps_to_target );
                continue;
            }

            if ( verify_through_pass( world,
                                      receiver,
                                      receiver->pos(),
                                      target_point, target_dist, target_angle,
                                      first_speed,
                                      ball_steps_to_target,
                                      true ) )
            {
                S_cached_pass_route
                    .push_back( PassRoute( THROUGH,
                                           receiver,
                                           target_point,
                                           first_speed,
                                           can_kick_by_one_step( world,
                                                                 first_speed,
                                                                 target_angle )
                                           )
                                );
                dlog.addText( Logger::PASS,
                              "ok__%d through unum=%d pos=(%.1f %.1f) angle=%.1f first_speed=%.1f end_speed=%.1f dash_step=%.1f ball_step=%.1f",
                              g_pass_counter,
                              receiver->unum(),
                              target_point.x, target_point.y,
                              target_angle.degree(),
                              first_speed,
                              end_speed,
                              dash_step,
                              ball_steps_to_target );
                dlog.addCircle( Logger::PASS,
                                target_point, 0.2,
                                "#FF0000" );
            }
            else
            {
                dlog.addText( Logger::PASS,
                              "xx__ through unum=%d pos=(%.1f %.1f) angle=%.1f first_speed=%.1f end_speed=%.1f dash_step=%.1f ball_step=%.1f",
                              receiver->unum(),
                              target_point.x, target_point.y,
                              target_angle.degree(),
                              first_speed,
                              end_speed,
                              dash_step,
                              ball_steps_to_target );
                dlog.addRect( Logger::PASS,
                              target_point.x - 0.2, target_point.y - 0.2, 0.4, 0.4,
                              "#0000FF" );
            }
        }
    }
}

/*-------------------------------------------------------------------*/
/*!
  static method
*/
void
Bhv_PassTest::create_recursive_pass( const WorldModel & world )
{
#ifdef DEBUG
    dlog.addText( Logger::PASS,
                  "Create_recursive_pass()" );
#endif

    const PassCourseTable & table = PassCourseTable::instance( world , true );
    PassCourseTable::CourseSelection first_pass = table.get_first_selection( world );
    PassCourseTable::CourseSelection pass_result = table.get_result( world );

    const Vector2D ball_pos = world.ball().pos();

    Vector2D receive_point;
    const AbstractPlayerObject * receiver = static_cast< PlayerObject * >( 0 );
    Vector2D ball_first_vec;

    switch( pass_result.type )
    {
    case PassCourseTable::CourseSelection::Shoot:
        {
            dlog.addText( Logger::PASS,
                          __FILE__": create_recursive_pass/"
                          "result: shoot" );

            //
            // we set this parameters,
            // but this will be ignored by following switch sentense.
            // anyway, we set.
            //

            receiver = world.ourPlayer( world.self().unum() );
            receive_point = ServerParam::i().theirTeamGoalPos();
            break;
        }

    case PassCourseTable::CourseSelection::Push:
        {
            dlog.addText( Logger::PASS,
                          __FILE__": create_recursive_pass/"
                          "restlt: push by teammate %d",
                          pass_result.player_number );

            //
            // we set this parameters,
            // but this will be ignored by following switch sentense.
            // anyway, we set.
            //

            receiver = world.ourPlayer( pass_result.player_number );
            receive_point = receiver->pos();

            // add bonus
            receive_point += Vector2D( 5.0, 0.0 );

            if ( receive_point.x
                 > ServerParam::i().theirTeamGoalLineX() - 1.0 )
            {
                receive_point.assign( ServerParam::i()
                                      .theirTeamGoalLineX() - 1.0,
                                      receive_point.y );
            }

            break;
        }

    case PassCourseTable::CourseSelection::FindPassReceiver:
        {
            dlog.addText( Logger::PASS,
                          __FILE__": create_recursive_pass/"
                          "restlt: find teammate %d",
                          pass_result.player_number );

            receiver = world.ourPlayer( pass_result.player_number );
            receive_point = receiver->pos();
            break;
        }

    case PassCourseTable::CourseSelection::Pass:
        {
            dlog.addText( Logger::PASS,
                          __FILE__": create_recursive_pass/"
                          "result: pass to teammate %d",
                          pass_result.player_number );

            receiver = world.ourPlayer( pass_result.player_number );
            receive_point = receiver->pos();
            break;
        }

    case PassCourseTable::CourseSelection::Hold:
        {
            dlog.addText( Logger::PASS,
                          __FILE__": create_recursive_pass/"
                          "result: hold, ignored" );

            // not append to candidate list, return
            return;
            break;
        }
    }

    switch( first_pass.type )
    {
    case PassCourseTable::CourseSelection::Shoot:
        {
#if 0
            // assume one step kick to better evaluation
            ball_first_vec.setPolar( EPS,
                                     (ServerParam::i().theirTeamGoalPos()
                                      - ball_pos).th() );
#else
            dlog.addText( Logger::PASS,
                          __FILE__": create_recursive_pass/"
                          "first = shoot, ignored" );

            // not append to candidate list, return
            return;
#endif
            break;
        }

    case PassCourseTable::CourseSelection::Push:
        {
            dlog.addText( Logger::PASS,
                          __FILE__": create_recursive_pass/"
                          "first = push, ignored" );

            // not append to candidate list, return
            return;
            break;
        }

    case PassCourseTable::CourseSelection::FindPassReceiver:
    case PassCourseTable::CourseSelection::Pass:
        {
            const AbstractPlayerObject * first_receiver;
            first_receiver = world.ourPlayer( first_pass.player_number );

            const Vector2D diff = ( first_receiver->pos()
                                    - world.ball().pos() );

            ball_first_vec.setPolar( PassCourseTable::get_recursive_pass_ball_speed( diff.r() ),
                                     diff.th() );
            break;
        }

    case PassCourseTable::CourseSelection::Hold:
        {
            // not append to candidate list, return
            return;
            break;
        }
    }

    //
    // check if the find player action can be performed.
    //
    if ( first_pass.type == PassCourseTable::CourseSelection::FindPassReceiver )
    {
        double opp_dist = rcsc_ext::getDistPlayerNearestToPoint( world,
                                                                 world.ball().pos() + world.ball().vel(),
                                                                 world.opponents(),
                                                                 10 );
        if ( opp_dist <= 4.0 )
        {
            dlog.addText( Logger::PASS,
                          __FILE__": create_recursive_pass() exist near opponent. dist=%.1f",
                          opp_dist );

            //
            // can kick by 1 step or not
            //
            Vector2D one_step_vel = KickTable::calc_max_velocity( ( receive_point - world.ball().pos() ).th(),
                                                                  world.self().kickRate(),
                                                                  world.ball().vel() );

            //
            // check the next neck range
            //
            if ( one_step_vel.r() <= ball_first_vec.r() * 0.96 )
            {
                dlog.addText( Logger::PASS,
                              __FILE__": create_recursive_pass() cannot kick by 1 step" );

                const Vector2D next_self_pos = world.self().pos() + world.self().vel();
                const Vector2D player_pos = receiver->pos() + receiver->vel();
                const AngleDeg player_angle = ( player_pos - next_self_pos ).th();

                const double view_half_width = ViewWidth::width( ViewWidth::NARROW ) * 0.5;
                const double neck_min = ServerParam::i().minNeckAngle() - ( view_half_width - 10.0 );
                const double neck_max = ServerParam::i().maxNeckAngle() + ( view_half_width - 10.0 );

                double angle_diff = ( player_angle - world.self().body() ).abs();
                if ( angle_diff < neck_min
                     || neck_max < angle_diff )
                {
                    dlog.addText( Logger::PASS,
                                  __FILE__": create_recursive_pass()"
                                  " cannot look only with turn_neck. neck_min=%.1f angle_diff=%.1f neck_max=%.1f",
                                  neck_min,
                                  angle_diff,
                                  neck_max );
                    return;
                }

                dlog.addText( Logger::PASS,
                              __FILE__": create_recursive_pass()"
                              " can look only with turn_neck. neck_min=%.1f angle_diff=%.1f neck_max=%.1f",
                              neck_min,
                              angle_diff,
                              neck_max );
            }
            else
            {
                dlog.addText( Logger::PASS,
                              __FILE__": create_recursive_pass() can kick by 1 step" );
            }
        }
    }

    //
    // append recursive pass
    //
    S_cached_pass_route.push_back( PassRoute( RECURSIVE,
                                              receiver,
                                              receive_point,
                                              ball_first_vec.r(),
                                              can_kick_by_one_step
                                              ( world,
                                                ball_first_vec.r(),
                                                ball_first_vec.th() ) ) );
}


/*-------------------------------------------------------------------*/
/*!
  static method
*/
bool
Bhv_PassTest::verify_direct_pass( const WorldModel & world,
                                  const PlayerObject * /*receiver*/,
                                  const Vector2D & target_point,
                                  const double & target_dist,
                                  const AngleDeg & target_angle,
                                  const double & first_speed )
{
    const Vector2D first_vel = Vector2D::polar2vector( first_speed, target_angle );
    const AngleDeg minus_target_angle = -target_angle;
    const double next_speed = first_speed * ServerParam::i().ballDecay();

    const double ball_steps_to_target = calc_length_geom_series( first_speed,
                                                                 target_dist,
                                                                 ServerParam::i().ballDecay() );

#ifdef DEBUG
    dlog.addText( Logger::PASS,
                  "____ verify direct to(%.1f %.1f). first_speed=%.3f. angle=%.1f",
                  target_point.x, target_point.y,
                  first_speed, target_angle.degree() );
#endif

    const PlayerPtrCont::const_iterator o_end = world.opponentsFromSelf().end();
    for ( PlayerPtrCont::const_iterator it = world.opponentsFromSelf().begin();
          it != o_end;
          ++it )
    {
        if ( (*it)->posCount() > 15 ) continue;
        if ( (*it)->isGhost() && (*it)->posCount() >= 4 ) continue;

        if ( ( (*it)->angleFromSelf() - target_angle ).abs() > 100.0 )
        {
            continue;
        }

        if ( (*it)->pos().dist2( target_point ) < 2.0 * 2.0 ) //3.0 * 3.0 )
        {
#ifdef DEBUG
            dlog.addText( Logger::PASS,
                          "______ opp%d(%.1f %.1f) is already on target point(%.1f %.1f).",
                          (*it)->unum(),
                          (*it)->pos().x, (*it)->pos().y,
                          target_point.x, target_point.y );
#endif
            return false;
        }

        {
            double opp_target_dist = (*it)->pos().dist( target_point );
            opp_target_dist -= (*it)->playerTypePtr()->kickableArea();
            opp_target_dist -= 0.2;
            if ( (*it)->velCount() <= 1 )
            {
                Vector2D vel = (*it)->vel();
                vel.rotate( - ( target_point - (*it)->pos() ).th() );
                opp_target_dist -= vel.x;
            }

            int opp_target_cycle = (*it)->playerTypePtr()->cyclesToReachDistance( opp_target_dist );
            if ( opp_target_cycle < ball_steps_to_target - 2 )
            {
#ifdef DEBUG
                dlog.addText( Logger::PASS,
                              "______ opp%d(%.1f %.1f) can reach receive point. opp_cycle=%d ball_step=%.1f",
                              (*it)->unum(),
                              (*it)->pos().x, (*it)->pos().y,
                              opp_target_cycle, ball_steps_to_target );
#endif
                return false;
            }
        }

        Vector2D ball_to_opp = (*it)->pos();
        ball_to_opp -= world.ball().pos();
        ball_to_opp -= first_vel;
        ball_to_opp.rotate( minus_target_angle );

        if ( 0.0 < ball_to_opp.x && ball_to_opp.x < target_dist )
        {
#if 1
            ///
            /// 2008-04-29 akiyama new algorithm
            ///
            const double ball_steps_to_project
                = calc_length_geom_series( next_speed,
                                           ball_to_opp.x,
                                           ServerParam::i().ballDecay() );

            double opp2line_dist = ball_to_opp.absY();
            opp2line_dist -= ServerParam::i().defaultKickableArea();
            opp2line_dist -= 0.2;
            opp2line_dist -= (*it)->distFromSelf() * 0.02;

            if ( (*it)->velCount() <= 1 )
            {
                opp2line_dist -= ( (*it)->vel().rotatedVector( minus_target_angle ).x
                                   * ( 1.0 - std::pow( (*it)->playerTypePtr()->playerDecay(), ball_steps_to_project ) )
                                   / ( 1.0 - (*it)->playerTypePtr()->playerDecay() ) );
            }

            if ( opp2line_dist < 0.0 )
            {
#ifdef DEBUG
                dlog.addText( Logger::PASS,
                              "______ opp%d(%.1f %.1f) is already on pass line",
                              (*it)->unum(),
                              (*it)->pos().x, (*it)->pos().y );
#endif
                return false;
            }

            int opp_line_reach_cycle = (*it)->playerTypePtr()->cyclesToReachDistance( opp2line_dist );
            if ( (*it)->bodyCount() <= 1 )
            {
                if ( ( (*it)->body() - target_angle ).abs() > 105.0 )
                {
                    //opp_line_reach_cycle += 1;
                }
                else if ( ( (*it)->body() - target_angle ).abs() < 90.0
                          && ( (*it)->vel().th() - target_angle ).abs() < 90.0
                          && (*it)->vel().r() > 0.2 )
                {
                    opp_line_reach_cycle -= 1;
                }
            }
            else
            {
                opp_line_reach_cycle -= 1;
            }
            opp_line_reach_cycle -= bound( 0, (*it)->posCount() - 1, 5 );

            if ( ball_steps_to_project < 0.0
                 || opp_line_reach_cycle < ball_steps_to_project * 1.05 + 0.5 )
            {
#ifdef DEBUG
                dlog.addText( Logger::PASS,
                              "______ xx opp %d(%.1f %.1f) can reach pass line."
                              " project_x=%.1f opp_y=%.1f"
                              " opp_reach_cycle=%d"
                              " ball_reach_step_to_project=%.1f",
                              (*it)->unum(),
                              (*it)->pos().x, (*it)->pos().y,
                              ball_to_opp.x, ball_to_opp.y,
                              opp_line_reach_cycle,
                              ball_steps_to_project );
#endif
                return false;
            }
#ifdef DEBUG
            dlog.addText( Logger::PASS,
                          "______ ok opp %d(%.1f %.1f)"
                          " project_x=%.1f opp_y=%.1f"
                          " opp_reach_cycle=%d"
                          " ball_reach_step_to_project=%.1f",
                          (*it)->unum(),
                          (*it)->pos().x, (*it)->pos().y,
                          ball_to_opp.x, ball_to_opp.y,
                          opp_line_reach_cycle,
                          ball_steps_to_project );
#endif
            ///
            ///
            ///
#else
            ///
            /// old algorithm
            ///
            const double player_dash_speed = 1.0;
            const double virtual_dash
                = player_dash_speed * 0.8 * std::min( 5, (*it)->posCount() );

            double opp2line_dist = ball_to_opp.absY();
            opp2line_dist -= virtual_dash;
            opp2line_dist -= ServerParam::i().defaultKickableArea();
            opp2line_dist -= 0.1;

            if ( opp2line_dist < 0.0 )
            {
#ifdef DEBUG
                dlog.addText( Logger::PASS,
                              "______ opp%d(%.1f %.1f) can reach pass line. rejected. vdash=%.1f",
                              (*it)->unum(),
                              (*it)->pos().x, (*it)->pos().y,
                              virtual_dash );
#endif
                return false;
            }

            const double ball_steps_to_project
                = calc_length_geom_series( next_speed,
                                           ball_to_opp.x,
                                           ServerParam::i().ballDecay() );
            //= move_step_for_first(ball_to_opp.x,
            //next_speed,
            //ServerParam::i().ballDecay());
            if ( ball_steps_to_project < 0.0
                 || opp2line_dist / player_dash_speed < ball_steps_to_project )
            {
#ifdef DEBUG
                dlog.addText( Logger::PASS,
                              "______ opp%d(%.1f %.1f) can reach pass line."
                              " ball reach step to project= %.1f",
                              (*it)->unum(),
                              (*it)->pos().x, (*it)->pos().y,
                              ball_steps_to_project );
#endif
                return false;
            }
#ifdef DEBUG
            dlog.addText( Logger::PASS,
                          "______ opp%d(%.1f %.1f) cannot intercept.",
                          (*it)->unum(),
                          (*it)->pos().x, (*it)->pos().y );
#endif
            ///
            ///
            ///
#endif
        }
    }

#ifdef DEBUG
    ++g_pass_counter;
    //dlog.addText( Logger::PASS,
    //              "%d: __ Success!", g_pass_counter );
#endif
    return true;
}

/*-------------------------------------------------------------------*/
/*!
  static method
*/
bool
Bhv_PassTest::verify_through_pass( const WorldModel & world,
                                   const PlayerObject * receiver,
                                   const Vector2D & receiver_pos,
                                   const Vector2D & target_point,
                                   const double & target_dist,
                                   const AngleDeg & target_angle,
                                   const double & first_speed,
                                   const double & /*reach_step*/,
                                   const bool is_through_pass )
{
    static const double player_dash_speed = ( is_through_pass ? 0.9 : 1.0 );

    const Vector2D first_vel = Vector2D::polar2vector( first_speed, target_angle );
    const AngleDeg minus_target_angle = -target_angle;
    const double next_speed = first_speed * ServerParam::i().ballDecay();

    const double receiver_to_target = receiver_pos.dist( target_point );

    bool very_aggressive = false;
    if ( is_through_pass )
    {
        if ( target_point.x > 28.0
             && target_point.x > world.ball().pos().x + 20.0 )
        {
#ifdef DEBUG
            dlog.addText( Logger::PASS,
                          "____ aggressive(1)." );
#endif
            very_aggressive = true;
        }
        else if ( target_point.x > 28.0
                  && target_point.x > world.offsideLineX() + 15.0
                  && target_point.x > world.ball().pos().x + 15.0 )
        {
#ifdef DEBUG
            dlog.addText( Logger::PASS,
                          "____ aggressive(2)." );
#endif
            very_aggressive = true;
        }
        else if ( target_point.x > 38.0
                  && target_point.x > world.offsideLineX()
                  && target_point.absY() < 14.0 )
        {
#ifdef DEBUG
            dlog.addText( Logger::PASS,
                          "____ aggressive(3)." );
#endif
            very_aggressive = true;
        }
        else if ( target_point.x > receiver->pos().x + 10.0
                  && receiver->pos().x > world.offsideLineX() - 2.5 )
        {
#ifdef DEBUG
            dlog.addText( Logger::PASS,
                          "____ aggressive(4)." );
#endif
            very_aggressive = true;
        }
    }

    bool pass_request = false;
    if ( is_through_pass
         && target_point.x > receiver->pos().x + 8.0 )
    {
        if ( world.audioMemory().passRequestTime().cycle() >= world.time().cycle() - 1 )
        {
            for ( std::vector< AudioMemory::PassRequest >::const_iterator it = world.audioMemory().passRequest().begin();
                  it != world.audioMemory().passRequest().end();
                  ++it )
            {
                if ( it->sender_ == receiver->unum() )
                {
#ifdef DEBUG
                    dlog.addText( Logger::PASS,
                                  "____ pass request. time(%ld) sender=%d pos=(%.1f %.1f)",
                                  world.audioMemory().passRequestTime().cycle(),
                                  it->sender_,
                                  it->pos_.x, it->pos_.y );
#endif
                    pass_request = true;
                    very_aggressive = true;
                    break;
                }
            }
        }
    }

    const PlayerPtrCont::const_iterator o_end = world.opponentsFromSelf().end();
    for ( PlayerPtrCont::const_iterator it = world.opponentsFromSelf().begin();
          it != o_end;
          ++it )
    {
        if ( (*it)->posCount() > 15 ) continue;
        if ( (*it)->isGhost() && (*it)->posCount() > 10 ) continue;
        if ( (*it)->isTackling() ) continue;

        if ( ( (*it)->angleFromSelf() - target_angle ).abs() > 100.0 )
        {
            continue;
        }

        const double virtual_dash
            = player_dash_speed * bound( 0.0, (*it)->posCount() - 0.5, 2.0 );
        const double opp_to_target = (*it)->pos().dist( target_point );
        double dist_rate = 1.0;
        //double dist_rate = ( very_aggressive ? 0.8 : 1.0 );
        double dist_buf = ( is_through_pass
                            ? ( very_aggressive ? 2.5 : 1.0 )
                            : -2.0 );
        if ( pass_request ) dist_buf += 2.0;

        if ( (*it)->pos().x > target_point.x - 3.0 )
        {
            //dist_rate = 1.08;
            dist_rate = 1.05;
            dist_buf -= 2.0;
        }

        if ( (*it)->goalie()
#if 0
             && (*it)->pos().x > 45.0
             && (*it)->pos().absY() > 10.0
             && ( target_point.absY() > ServerParam::i().penaltyAreaWidth() - 3.0
                  || target_point.x < ServerParam::i().theirPenaltyAreaLineX() - 1.5 )
#else
             && (*it)->pos().x > target_point.x - 3.0
#endif
             )
        {
            //dist_rate = 1.025;
            //dist_buf -= 1.5;
            dist_buf -= 0.5;
            //if ( is_through_pass ) dist_buf -= 0.5;
            //if ( very_aggressive ) dist_buf -= 0.5;
        }

#ifdef DEBUG
        dlog.addText( Logger::PASS,
                      "______ opp%d(%.1f %.1f) dist_rate=%.2f, dist_buf=%.2f"
                      " opp_dist=%.2f self_dist=%.2f",
                      (*it)->unum(),
                      (*it)->pos().x, (*it)->pos().y,
                      dist_rate, dist_buf,
                      opp_to_target - virtual_dash,
                      receiver_to_target * dist_rate - dist_buf );
#endif

        if ( opp_to_target - virtual_dash
             < receiver_to_target * dist_rate - dist_buf )
        {
#ifdef DEBUG
            dlog.addText( Logger::PASS,
                          "______ opp%d(%.1f %.1f) is closer than receiver. %.2f < %.2f %s",
                          (*it)->unum(),
                          (*it)->pos().x, (*it)->pos().y,
                          opp_to_target, receiver_to_target,
                          very_aggressive ? "aggressive" : "" );
#endif
            return false;
        }

        Vector2D ball_to_opp = (*it)->pos() + (*it)->vel();
        ball_to_opp -= world.ball().pos();
        ball_to_opp -= first_vel;
        ball_to_opp.rotate( minus_target_angle );

        if ( 0.0 < ball_to_opp.x && ball_to_opp.x < target_dist )
        {
#if 1
            ///
            /// 2008-04-29 akiyama new algorithm
            ///
            const double ball_steps_to_project
                = calc_length_geom_series( next_speed,
                                           ball_to_opp.x,
                                           ServerParam::i().ballDecay() );

            double opp2line_dist = ball_to_opp.absY();
            opp2line_dist -= ServerParam::i().defaultKickableArea();
            opp2line_dist -= 0.2;
            opp2line_dist -= (*it)->distFromSelf() * 0.02;

            if ( (*it)->velCount() <= 1 )
            {
                opp2line_dist -= ( (*it)->vel().rotatedVector( minus_target_angle ).x
                                   * ( 1.0 - std::pow( (*it)->playerTypePtr()->playerDecay(), ball_steps_to_project ) )
                                   / ( 1.0 - (*it)->playerTypePtr()->playerDecay() ) );
            }

            if ( opp2line_dist < 0.0 )
            {
#ifdef DEBUG
                dlog.addText( Logger::PASS,
                              "______ opp%d(%.1f %.1f) is already on pass line.",
                              (*it)->unum(),
                              (*it)->pos().x, (*it)->pos().y );
#endif
                return false;
            }

            int opp_line_reach_cycle = (*it)->playerTypePtr()->cyclesToReachDistance( opp2line_dist );
#if 0
            if ( (*it)->bodyCount() <= 1 )
            {
                if ( ( (*it)->body() - target_angle ).abs() > 105.0 )
                {
                    //opp_line_reach_cycle += 1;
                }
                else if ( ( (*it)->body() - target_angle ).abs() < 90.0
                          && ( (*it)->vel().th() - target_angle ).abs() < 90.0
                          && (*it)->vel().r() > 0.2 )
                {
                    opp_line_reach_cycle -= 1;
                }
            }
            else
            {
                opp_line_reach_cycle -= 1;
            }
            opp_line_reach_cycle -= bound( 0, (*it)->posCount() - 1, 5 );
#else
            if ( (*it)->bodyCount() <= 3 )
            {
                if ( ( ( target_point - (*it)->pos() ).th() - (*it)->body() ).abs()
                     > 95.0 )
                {
                    opp_line_reach_cycle += 1;
                }
            }
            opp_line_reach_cycle -= bound( 0, (*it)->posCount(), 5 );
#endif

            if ( ball_steps_to_project < 0.0
                 || opp_line_reach_cycle < ball_steps_to_project * 1.02 + 0.5 ) // 20080718
                //|| opp_line_reach_cycle < ball_steps_to_project + 0.5 ) // 20080713
                //|| opp_line_reach_cycle < ball_steps_to_project * 1.05 + 0.5 ) // old
            {
#ifdef DEBUG
                dlog.addText( Logger::PASS,
                              "______ xx opp can reach pass line."
                              " project_x=%.1f opp_y=%.1f"
                              " opp_reach_cycle=%d"
                              " ball_reach_step_to_project=%.1f",
                              ball_to_opp.x, ball_to_opp.y,
                              opp_line_reach_cycle,
                              ball_steps_to_project );
#endif
                return false;
            }
#ifdef DEBUG
            dlog.addText( Logger::PASS,
                          "______ ok "
                          " project_x=%.1f opp_y=%.1f"
                          " opp_reach_cycle=%d"
                          " ball_reach_step_to_project=%.1f",
                          ball_to_opp.x, ball_to_opp.y,
                          opp_line_reach_cycle,
                          ball_steps_to_project );
#endif
            ///
            ///
            ///
#else
            ///
            /// old algorithm
            ///
            double opp2line_dist = ball_to_opp.absY();
            opp2line_dist -= ServerParam::i().defaultKickableArea();
            opp2line_dist -= virtual_dash;
            //opp2line_dist -= 0.1;
            if ( opp2line_dist < 0.0 )
            {
#ifdef DEBUG
                dlog.addText( Logger::PASS,
                              "______ opp%d(%.1f %.1f) is already on pass line.",
                              (*it)->unum(),
                              (*it)->pos().x, (*it)->pos().y );
#endif
                return false;
            }

            const double ball_steps_to_project
                = calc_length_geom_series( next_speed,
                                           ball_to_opp.x,
                                           ServerParam::i().ballDecay() );

            if ( ball_steps_to_project < 0.0
                 || opp2line_dist / player_dash_speed < ball_steps_to_project )
            {
#ifdef DEBUG
                dlog.addText( Logger::PASS,
                              "______ opp%d(%.1f %.1f) can reach pass line."
                              " project_x=%.1f opp_y=%.1f"
                              " ball_reach_step_to_project=%.1f",
                              (*it)->unum(),
                              (*it)->pos().x, (*it)->pos().y,
                              ball_to_opp.x, ball_to_opp.y,
                              ball_steps_to_project );
#endif
                return false;
            }
            ///
            ///
            ///
#endif
        }
    }

#ifdef DEBUG
    ++g_pass_counter;
    //dlog.addText( Logger::PASS,
    //              "%d: __ Success!", g_pass_counter );
#endif
    return true;
}

/*-------------------------------------------------------------------*/
/*!
  static method
*/
void
Bhv_PassTest::evaluate_routes( const WorldModel & world )
{
    const AngleDeg min_angle = -45.0;
    const AngleDeg max_angle = 45.0;
    const Vector2D goal_pos( 43.0, 0.0 );
    const double ball_speed_max = ServerParam::i().ballSpeedMax();
    const double ball_speed_thr = std::min( 2.5, ball_speed_max - 0.1 );

    bool opp_very_near = false;
    const PlayerObject * nearest_opp = world.getOpponentNearestToSelf( 0 );
    if ( nearest_opp
         && nearest_opp->distFromSelf() < 2.0 )
    {
        opp_very_near = true;
    }

    int counter = 1;
    const std::vector< PassRoute >::iterator end = S_cached_pass_route.end();
    for ( std::vector< PassRoute >::iterator it = S_cached_pass_route.begin();
          it != end;
          ++it, ++counter )
    {
        //-----------------------------------------------------------
        double opp_dist_rate = 1.0;
        {
            double opp_dist = 100.0;
            world.getOpponentNearestTo( it->receive_point_, 20, &opp_dist );
            //opp_dist_rate = std::pow( 0.99, std::max( 0.0, 30.0 - opp_dist ) );
            //opp_dist_rate = 1.0 - std::exp( - opp_dist*opp_dist / ( 2.0 * 20.0 ) );
            //opp_dist_rate = std::min( 1.0, std::sqrt( opp_dist * 0.1 ) );
            opp_dist_rate = std::min( 1.0, std::sqrt( opp_dist * 0.09 ) );
        }
        //-----------------------------------------------------------
        double goal_dist_rate = std::pow( 0.98,
                                          it->receive_point_.dist( goal_pos ) );

        //-----------------------------------------------------------
        double x_diff_rate = 1.0;
        {
            double x_diff = it->receive_point_.x - it->receiver_->pos().x;
            if ( x_diff < 0.0 )
            {
                x_diff_rate = std::pow( 0.98, -x_diff );
            }
        }
        //-----------------------------------------------------------
        double pos_conf_rate = std::pow( 0.98, it->receiver_->posCount() );
        //-----------------------------------------------------------
        double dir_conf_rate = 1.0;
        {
            AngleDeg pass_angle = ( it->receive_point_ - world.self().pos() ).th();
            int max_dir_count = 0;
            world.dirRangeCount( pass_angle, 20.0, &max_dir_count, NULL, NULL );

            dir_conf_rate = std::pow( 0.98, max_dir_count );
        }
        //-----------------------------------------------------------
        //double offense_rate
        //    = std::pow( 0.98,
        //                std::max( 5.0, std::fabs( it->receive_point_.y
        //                                          - world.ball().pos().y ) ) );
        //-----------------------------------------------------------
        it->score_ = 1000.0;
        if ( opp_very_near && it->one_step_kick_ )
        {
            it->score_ += 1000.0;
        }
        it->score_ *= opp_dist_rate;
        it->score_ *= goal_dist_rate;
        it->score_ *= x_diff_rate;
        it->score_ *= pos_conf_rate;
        it->score_ *= dir_conf_rate;

        if ( it->type_ == LEAD )
        {
            it->score_ *= 0.9;
        }

        if ( it->one_step_kick_ )
        {
            it->score_ *= 1.2;
        }
        else if ( it->first_speed_ > ball_speed_thr )
        {
            it->score_ *= 0.9;
        }

        dlog.addText( Logger::PASS,
                      "<<<PASS %d Score %6.2f -- to%d(%.1f %.1f) recv_pos(%.1f %.1f) type %d "
                      " speed=%.2f",
                      counter,
                      it->score_,
                      it->receiver_->unum(),
                      it->receiver_->pos().x, it->receiver_->pos().y,
                      it->receive_point_.x, it->receive_point_.y,
                      it->type_,
                      it->first_speed_ );
        dlog.addText( Logger::PASS,
                      "____Rate: opp_dist=%.2f"
                      " goal_dist=%.2f"
                      " x_diff=%.2f"
                      " pos_conf=%.2f"
                      " dir_conf=%.2f %s",
                      opp_dist_rate,
                      goal_dist_rate,
                      x_diff_rate,
                      pos_conf_rate,
                      dir_conf_rate,
                      ( it->one_step_kick_ ? "one_step" : "" ) );
    }

}

/*-------------------------------------------------------------------*/
/*!
  static method
*/
bool
Bhv_PassTest::can_kick_by_one_step( const WorldModel & world,
                                    const double & first_speed,
                                    const AngleDeg & target_angle )
{
    Vector2D required_accel = Vector2D::polar2vector( first_speed, target_angle );
    required_accel -= world.ball().vel();
    double accel_r = required_accel.r();
    return ( accel_r <= ServerParam::i().ballAccelMax()
             && world.self().kickRate() * ServerParam::i().maxPower() > accel_r );
}

}
