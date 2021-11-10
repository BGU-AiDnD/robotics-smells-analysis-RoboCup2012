// -*-c++-*-

/*!
  \file bhv_savior.h
  \brief aggressive goalie behavior
*/

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA

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

#include "bhv_savior.h"

#include <rcsc/geom/angle_deg.h>
#include <rcsc/common/server_param.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/penalty_kick_state.h>
#include <rcsc/player/intercept_table.h>
#include "body_intercept2008.h"
#include "bhv_savior_chase_ball.h"
#include "bhv_pass_test.h"
#include <rcsc/action/body_stop_ball.h>
#include "body_shoot.h"
#include <rcsc/action/body_hold_ball.h>
#include "body_go_to_point2008.h"
#include <rcsc/action/body_turn_to_angle.h>
#include <rcsc/action/body_stop_dash.h>
#include "body_dribble2008.h"
#include <rcsc/action/bhv_scan_field.h>
#include <rcsc/action/neck_turn_to_ball.h>
#include "neck_chase_ball.h"
#include <rcsc/action/neck_turn_to_point.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>
#include <rcsc/action/view_synch.h>
#include "bhv_find_player.h"
#include "bhv_pass_kick_find_receiver.h"
#include "bhv_find_ball.h"
#include "body_hold_ball2008.h"

#include "world_model_ext.h"
#include <rcsc/player/say_message_builder.h>
#include <rcsc/common/logger.h>
#include <rcsc/math_util.h>
#include "bhv_deflecting_tackle.h"
#include "bhv_danger_area_tackle.h"
#include "bhv_basic_tackle.h"
#include "body_clear_ball2008.h"
#include "body_smart_kick.h"
#include "body_force_shoot.h"
#include "pass_course_table.h"
#include <algorithm>
#include <cmath>
#include <cstdio>


//
// all revert switch
//

// uncomment this, to revert all unstable behaviors
#if 0
# define OLD_STABLE
#endif



//
// each revert switch
//

// !!!!!!
// !!!!!!
// uncomment this not to do forward positioning
// new behavior is not stable, my be lose point by long shoot.
#if 1
# define OLD_AGGRESSIVE_POSITIONING
#endif
// !!!!!!
// !!!!!!


// uncomment this to disable shifting to shoot point
// this shifting is effective to opponent's fast passing near goal
// but has side effect, may weak far corner shooting at side 1 on 1.
#if 1
# define DISABLE_GOAL_LINE_POSITIONING_SHIFT_TO_SHOOT_POINT
#endif

// uncomment this to use old condition for decision which positioning
// mode, normal positioning or goal line positioning
#if 0
# define OLD_GOAL_LINE_POSITINING_CONDITION
#endif

// uncomment this, to revert behavior going back to goal from mid field
#if 0
# define DISABLE_FAST_BACK_HOME
#endif


// uncomment this to use old aggressive defensing at mid field
// new emergent advance is semi stable
#if 1
# define OLD_EMERGENT_ADVANCE
#endif

// uncomment this to use old ball chasing
// no need to uncomment this, newer is more stable
#if 0
# define OLD_CHASE_BALL
#endif

#ifdef OLD_STABLE
# define DISABLE_FAST_BACK_HOME
# define OLD_AGGRESSIVE_POSITIONING
# define OLD_EMERGENT_ADVANCE
# define DISABLE_GOAL_LINE_POSITIONING_SHIFT_TO_SHOOT_POINT
# define OLD_GOAL_LINE_POSITINING_CONDITION
//# define OLD_CHASE_BALL
#endif


#if 1
# define DO_EMERGENT_ADVANCE
#endif
#if 1
# define DO_GOAL_LINE_POSITIONONG
#endif
#if 1
# define FORCE_GOAL_LINE_POSITIONONG
#endif

#if 0
# define GOAL_LINE_POSITIONINIG_FACE_BODY
#endif

static const double EMERGENT_ADVANCE_DIR_DIFF_THRESHOLD = 3.0; //degree

bool
Bhv_Savior::execute( rcsc::PlayerAgent * agent )
{
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Bhv_Savior::execute()" );

    const rcsc::WorldModel & wm = agent->world();

    const double our_team_goal_line_x = rcsc::ServerParam::i().ourTeamGoalLineX();
    switch( wm.gameMode().type() )
    {
    case rcsc::GameMode::PlayOn:
    case rcsc::GameMode::KickIn_:
    case rcsc::GameMode::OffSide_:
    case rcsc::GameMode::CornerKick_:
        {
            return this->doPlayOnMove( agent );
        }

    case rcsc::GameMode::GoalKick_:
        {
            if ( wm.gameMode().side() == wm.self().side() )
            {
                const rcsc::Vector2D pos( our_team_goal_line_x, 0.0 );

                rcsc::Body_GoToPoint2008( pos, 1.0 ).execute( agent )
                    || rcsc::Body_StopDash( true ).execute( agent )
                    || rcsc::Body_TurnToAngle( 0.0 ).execute( agent );

                agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );

                return true;
            }
            else
            {
                return this->doPlayOnMove( agent );
            }
            break;
        }

    case rcsc::GameMode::BeforeKickOff:
    case rcsc::GameMode::AfterGoal_:
        {
            const rcsc::Vector2D pos( our_team_goal_line_x, 0.0 );

            if ( (pos - wm.self().pos()).r() > 1.0 )
            {
                agent->doMove( pos.x, pos.y );
            }
            else
            {
                rcsc::Bhv_ScanField().execute( agent );
            }
            agent->setNeckAction( new rcsc_ext::Neck_ChaseBall );

            return true;
        }

    case rcsc::GameMode::PenaltyTaken_:
        {
            const rcsc::PenaltyKickState * pen = wm.penaltyKickState();

            if ( pen->currentTakerSide() == wm.ourSide() )
            {
                return rcsc::Bhv_ScanField().execute( agent );
            }
            else
            {
                return this->doPlayOnMove
                             ( agent,
                               true,
                               (pen->onfieldSide() != wm.ourSide()) );
            }

            break;
        }

    default:
        {
            if ( wm.gameMode().type() == rcsc::GameMode::KickOff_
                 && wm.gameMode().side() != wm.ourSide() )
            {
                return rcsc::Bhv_ScanField().execute( agent );
            }
            else
            {
                return this->doPlayOnMove( agent );
            }
        }

        break;
    }

    return false;
}

bool
Bhv_Savior::doPlayOnMove( rcsc::PlayerAgent * agent,
                          bool penalty_kick_mode,
                          bool reverse_x )
{
    const rcsc::WorldModel & wm = agent->world();
    const rcsc::ServerParam & param = rcsc::ServerParam::i();

    //
    // set some parameters for thinking
    //
    const int teammate_reach_cycle = wm.interceptTable()->teammateReachCycle();
    const int opponent_reach_cycle = wm.interceptTable()->opponentReachCycle();
    const int self_reach_cycle = wm.interceptTable()->selfReachCycle();
    int predict_step = std::min( opponent_reach_cycle,
                                 std::min( teammate_reach_cycle,
                                           self_reach_cycle ) );

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": self reach cycle = %d",
                        self_reach_cycle );
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": teammate reach cycle = %d",
                        teammate_reach_cycle );
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": opponent reach cycle = %d",
                        opponent_reach_cycle );
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": ball pos = [%f, %f]",
                        wm.ball().pos().x, wm.ball().pos().y );
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": ball dist = %f",
                        wm.ball().distFromSelf() );
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": tackle probability = %f",
                        wm.self().tackleProbability() );


    //
    // set default neck action
    //
    if ( wm.ball().seenPosCount() == 0
         && predict_step >= 10
         && ! penalty_kick_mode )
    {
        agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
    }
    else
    {
        agent->setNeckAction( new rcsc_ext::Neck_ChaseBall() );
    }


    //
    // if catchable, do catch
    //
    const double max_ball_error = 0.5;

    bool ball_cachable_if_reach
          = ( wm.self().goalie()
              && ( wm.gameMode().type() == rcsc::GameMode::PlayOn
                   || wm.gameMode().type() == rcsc::GameMode::PenaltyTaken_ )
              && wm.time().cycle()
                 >= agent->effector().getCatchTime().cycle()
                    + rcsc::ServerParam::i().catchBanCycle()
              && wm.ball().pos().x < param.ourPenaltyAreaLineX()
                 + param.ballSize() * 2
                 - max_ball_error
              && wm.ball().pos().absY() < param.penaltyAreaHalfWidth()
                 + param.ballSize() * 2
              - max_ball_error
              && ( ! wm.existKickableTeammate() || wm.existKickableOpponent() ) );


    // if catchable situation
    if ( ball_cachable_if_reach < wm.ball().distFromSelf() - 0.1 )
    {
        // if near opponent is not exist, catch the ball,
        // otherwise don't catch it.
        //
        // (this check is for avoiding back pass foul)
        if ( penalty_kick_mode
             || ( wm.ball().pos().x <= 40.0
                  || ! wm.getPlayerCont
                  ( new rcsc::AndPlayerPredicate
                    ( new rcsc::OpponentOrUnknownPlayerPredicate( wm ),
                      new rcsc::PointNearPlayerPredicate( wm.self().pos(),
                                                          5.0 ) ) )
                  .empty() ) )
        {
            return agent->doCatch();
        }
    }


    //
    // if kickable, do kick
    //
    if ( wm.self().isKickable() )
    {
        return this->doKick( agent );
    }


    agent->debugClient().addMessage( "ball dist=%f", wm.ball().distFromSelf() );


    //
    // set parameters
    //
    const double goal_half_width = rcsc::ServerParam::i().goalHalfWidth();

    const rcsc::Vector2D goal_center = translateXVec( reverse_x,
                                                      rcsc::ServerParam::i().ourTeamGoalPos() );
    const rcsc::Vector2D goal_left_post( translateX( reverse_x,
                                                     goal_center.x ),
                                         translateX( reverse_x,
                                                     +goal_half_width ) );
    const rcsc::Vector2D goal_right_post( translateX( reverse_x,
                                                      goal_center.x ),
                                          translateX( reverse_x,
                                                      -goal_half_width ) );

    bool is_shoot_ball = ( ( (goal_left_post - wm.ball().pos() ).th()
                             - wm.ball().vel().th() ).degree() < 0
                           && ( ( goal_right_post - wm.ball().pos() ).th()
                                - wm.ball().vel().th() ).degree() > 0 );

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": is_shoot_ball = %s",
                        ( is_shoot_ball ? "true" : "false" ) );

    if ( is_shoot_ball )
    {
        agent->debugClient().addLine( wm.self().pos()
                                      + rcsc::Vector2D( -2.0, -2.0 ),
                                      wm.self().pos()
                                      + rcsc::Vector2D( -2.0, +2.0 ) );
    }


    //
    // tackle
    //
    double tackle_prob_threshold = 0.8;
    bool despair_mode = false;

    if ( is_shoot_ball
         && wm.ball().inertiaPoint( self_reach_cycle ).x
            <= rcsc::ServerParam::i().ourTeamGoalLineX() )
    {
        despair_mode = true;

        agent->debugClient().addLine( rcsc::Vector2D
                                      ( param.ourTeamGoalLineX() - 2.0,
                                        - param.goalHalfWidth() ),
                                      rcsc::Vector2D
                                      ( param.ourTeamGoalLineX() - 2.0,
                                        + param.goalHalfWidth() ) );

        tackle_prob_threshold = rcsc::EPS;

        bool next_step_goal = ( wm.ball().inertiaPoint( 1 ).x
                                > rcsc::ServerParam::i().ourTeamGoalLineX()
                                  + param.ballRand() );

        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": next step, ball is in goal [%f, %f]",
                            wm.ball().inertiaPoint( 1 ).x,
                            wm.ball().inertiaPoint( 1 ).y );

        if ( next_step_goal )
        {
            double next_tackle_prob_forward
                   = getSelfNextTackleProbabilityWithDash
                       ( wm, + param.maxPower() );
            double next_tackle_prob_backword
                   = getSelfNextTackleProbabilityWithDash
                       ( wm, - param.maxPower() );

            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": next_tackle_prob_forward = %f",
                                next_tackle_prob_forward );
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": next_tackle_prob_backword = %f",
                                next_tackle_prob_backword );

            if ( next_tackle_prob_forward > wm.self().tackleProbability() )
            {
                rcsc::dlog.addText( rcsc::Logger::TEAM,
                                    __FILE__": dash forward expect next tackle" );

                agent->doDash( + param.maxPower() );

                return true;
            }
            else if ( next_tackle_prob_backword > wm.self().tackleProbability() )
            {
                rcsc::dlog.addText( rcsc::Logger::TEAM,
                                    __FILE__": dash backward expect next tackle" );
                agent->doDash( - param.maxPower() );

                return true;
            }
        }
    }

    if ( despair_mode )
    {
        if ( rcsc::Bhv_DeflectingTackle( tackle_prob_threshold, true ).execute( agent ) )
        {
            agent->debugClient().addMessage( "Savior:DeflectingTackle(%f)",
                                             wm.self().tackleProbability() );
            return true;
        }
    }

    if ( Bhv_DangerAreaTackle( tackle_prob_threshold ).execute( agent )
         || Bhv_BasicTackle( tackle_prob_threshold, 160.0 ).execute( agent ) )
    {
        agent->debugClient().addMessage( "Savior:Tackle(%f)",
                                         wm.self().tackleProbability() );
        return true;
    }


    const rcsc::Rect2D shrinked_penalty_area
        ( rcsc::Vector2D( param.ourTeamGoalLineX(),
                          - (param.penaltyAreaHalfWidth() - 1.0) ),
          rcsc::Size2D( param.penaltyAreaLength() - 1.0,
                        (param.penaltyAreaHalfWidth() - 1.0) * 2.0 ) );


    //
    // chase ball
    //
#if 0
    if ( is_shoot_ball
         && wm.ball().inertiaPoint( self_reach_cycle ).x
            <= rcsc::ServerParam::i().ourTeamGoalLineX() )
    {
        agent->debugClient().addLine( wm.self().pos()
                                      + rcsc::Vector2D( +2.0, -2.0 ),
                                      wm.self().pos()
                                      + rcsc::Vector2D( +2.0, +2.0 ) );

        if ( ! wm.existKickableOpponent() )
        {
            agent->debugClient().addMessage( "Savior:ShotChase" );
            if ( this->doChaseBall( agent ) )
            {
                return true;
            }
        }
    }
#endif

#if 0
    if ( self_reach_cycle <= teammate_reach_cycle + 5
         && ! wm.existKickableTeammate()
         && self_reach_cycle + 1 < opponent_reach_cycle
         && wm.gameMode().type() == rcsc::GameMode::PlayOn )
#else
    if ( (self_reach_cycle <= teammate_reach_cycle + 5
          || (shrinked_penalty_area.contains
              ( wm.ball().inertiaPoint( self_reach_cycle ) )
              && self_reach_cycle <= teammate_reach_cycle ) )
         && ! wm.existKickableTeammate()
         && self_reach_cycle + 1 < opponent_reach_cycle
         && wm.gameMode().type() == rcsc::GameMode::PlayOn )
#endif
    {
        agent->debugClient().addMessage( "Savior:Chase" );

        if ( this->doChaseBall( agent ) )
        {
            return true;
        }
    }

    if ( penalty_kick_mode
         && ( self_reach_cycle + 1 < opponent_reach_cycle
              || is_shoot_ball ) )
    {
        agent->debugClient().addMessage( "Savior:ChasePenaltyKickMode" );

        if ( rcsc::Body_Intercept2008().execute( agent ) )
        {
            agent->setNeckAction( new rcsc_ext::Neck_ChaseBall() );

            return true;
        }
    }


    //
    // check ball
    //
    long ball_premise_accuracy = 2;
    bool brief_ball_check = false;

    if ( wm.self().pos().x >= -30.0 )
    {
        ball_premise_accuracy = 6;

#if ! defined( DISABLE_FAST_BACK_HOME )
        brief_ball_check = true;
#endif
    }

    if ( wm.ball().seenPosCount() > ball_premise_accuracy
         || ( brief_ball_check
              && wm.ball().posCount() > ball_premise_accuracy ) )
    {
        agent->debugClient().addMessage( "Savior:FindBall" );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__ ": find ball" );
        return rcsc_ext::Bhv_FindBall().execute( agent );
    }


    //
    // set predicted ball pos
    //
    rcsc::Vector2D ball_pos = getBoundPredictBallPos( wm, predict_step, 0.5 );


    //
    // positioning
    //
    bool in_region;
    bool dangerous;
    bool aggressive;
    bool emergent_advance;
    bool goal_line_positioning;

    rcsc::Vector2D best_position = this->getBasicPosition
                                         ( agent,
                                           penalty_kick_mode,
                                           despair_mode,
                                           reverse_x,
                                           &in_region,
                                           &dangerous,
                                           &aggressive,
                                           &emergent_advance,
                                           &goal_line_positioning );


    if ( emergent_advance )
    {
        rcsc::Vector2D hat_base_vec = ( best_position - wm.self().pos() ).normalizedVector();

        // draw arrow
        agent->debugClient().addLine( wm.self().pos() + hat_base_vec * 3.0,
                                      wm.self().pos() + hat_base_vec * 3.0
                                      + rcsc::Vector2D::polar2vector
                                        ( 1.5, hat_base_vec.th() + 150.0 ) );
        agent->debugClient().addLine( wm.self().pos() + hat_base_vec * 3.0,
                                      wm.self().pos() + hat_base_vec * 3.0
                                      + rcsc::Vector2D::polar2vector
                                        ( 1.5, hat_base_vec.th() - 150.0 ) );
        agent->debugClient().addLine( wm.self().pos() + hat_base_vec * 4.0,
                                      wm.self().pos() + hat_base_vec * 4.0
                                      + rcsc::Vector2D::polar2vector
                                        ( 1.5, hat_base_vec.th() + 150.0 ) );
        agent->debugClient().addLine( wm.self().pos() + hat_base_vec * 4.0,
                                      wm.self().pos() + hat_base_vec * 4.0
                                      + rcsc::Vector2D::polar2vector
                                        ( 1.5, hat_base_vec.th() - 150.0 ) );
    }

    if ( goal_line_positioning )
    {
        agent->debugClient().addLine( rcsc::Vector2D
                                      ( param.ourTeamGoalLineX() - 1.0,
                                        - param.goalHalfWidth() ),
                                      rcsc::Vector2D
                                      ( param.ourTeamGoalLineX() - 1.0,
                                        + param.goalHalfWidth() ) );
    }


#if 0
    //
    // attack to near ball
    //
    if ( ! dangerous
         && ball_cachable_if_reach
         && wm.ball().distFromSelf() < 5.0 )
    {
        agent->debugClient().addMessage( "Savior:Attack" );
        return this->doChaseBall( agent );
    }
#endif


    agent->debugClient().addLine( wm.self().pos(), best_position );


    double max_position_error = 0.70;
    double dash_power = param.maxPower();

    if ( aggressive )
    {
        if ( wm.ball().distFromSelf() >= 30.0 )
        {
            max_position_error = 2.0;
        }
    }


    double diff = (best_position - wm.self().pos()).r();

    if ( wm.ball().distFromSelf() >= 30.0
         && wm.ball().pos().x >= -20.0
         && diff < 5.0
         && ! penalty_kick_mode )
    {
        dash_power = param.maxPower() * 0.7;

        agent->debugClient().addLine
            ( wm.self().pos() + rcsc::Vector2D( -1.0, +3.0 ),
              wm.self().pos() + rcsc::Vector2D( +1.0, +3.0 ) );

        agent->debugClient().addLine
            ( wm.self().pos() + rcsc::Vector2D( -1.0, -3.0 ),
              wm.self().pos() + rcsc::Vector2D( +1.0, -3.0 ) );
    }

    if ( emergent_advance )
    {
        if ( diff >= 10.0 )
        {
            // for speedy movement
            max_position_error = 1.9;
        }
        else if ( diff >= 5.0 )
        {
            // for speedy movement
            max_position_error = 1.0;
        }

        agent->debugClient().addCircle( wm.self().pos(), 3.0 );
    }
    else if ( goal_line_positioning )
    {
        if ( ( best_position - wm.self().pos() ).x > 0.3 )
        {
            max_position_error = 2.0;
        }
        else
        {
            max_position_error = 1.0;
        }
    }
    else if ( dangerous )
    {
        if ( diff >= 10.0 )
        {
            // for speedy movement
            max_position_error = 1.9;
        }
        else if ( diff >= 5.0 )
        {
            // for speedy movement
            max_position_error = 1.0;
        }
    }
    else
    {
        if ( diff >= 10.0 )
        {
            // for speedy movement
            max_position_error = 1.5;
        }
        else if ( diff >= 5.0 )
        {
            // for speedy movement
            max_position_error = 1.0;
        }
    }

    if ( despair_mode )
    {
        if ( max_position_error < 1.5 )
        {
            max_position_error = 1.5;

            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": Savior: despair_mode, "
                                "position error changed to %f",
                                max_position_error );
        }
    }


    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Savior: positioning target = [%f, %f], "
                        "max_position_error = %f, dash_power = %f",
                        best_position.x, best_position.y,
                        max_position_error, dash_power );

    bool force_back_dash = false;

    if ( rcsc::Body_GoToPoint2008( best_position,
                                   max_position_error,
                                   dash_power,
                                   true,
                                   force_back_dash,
                                   despair_mode ).execute( agent ) )
    {
        agent->debugClient().addMessage( "Savior:Positioning" );
        agent->debugClient().setTarget( best_position );
        agent->debugClient().addCircle( best_position, max_position_error );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__ ": go to (%.2f %.2f) error=%.3f  dash_power=%.1f  force_back=%d",
                            best_position.x, best_position.y,
                            max_position_error,
                            dash_power,
                            static_cast< int >( force_back_dash ) );
        return true;
    }


    //
    // emergency stop
    //
    if ( opponent_reach_cycle <= 1
         && (wm.ball().pos() - wm.self().pos()).r() < 10.0
         && wm.self().vel().r() >= 0.05 )
    {
        if ( rcsc::Body_StopDash( true ).execute( agent ) )
        {
            agent->debugClient().addMessage( "Savior:EmergemcyStop" );
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__ ": emergency stop" );
            return true;
        }
    }


    //
    // go to position with minimum error
    //
    const rcsc::PlayerObject * firstAccessOpponent;
    firstAccessOpponent = wm.interceptTable()->fastestOpponent();

    if ( (firstAccessOpponent
          && ( firstAccessOpponent->pos() - wm.self().pos() ).r() >= 5.0)
         && opponent_reach_cycle >= 3 )
    {
        if ( rcsc::Body_GoToPoint2008( best_position,
                                       0.3,
                                       dash_power,
                                       true,
                                       false,
                                       despair_mode ).execute( agent ) )
        {
            agent->debugClient().addMessage( "Savior:TunePositioning" );
            agent->debugClient().setTarget( best_position );
            agent->debugClient().addCircle( best_position, 0.3 );
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__ ": go to position with minimum error. target=(%.2f %.2f) dash_power=%.1f",
                                best_position.x, best_position.y,
                                dash_power );
            return true;
        }
    }


    //
    // stop
    //
    if ( wm.self().vel().r() >= 0.01 )
    {
        if ( rcsc::Body_StopDash( true ).execute( agent ) )
        {
            agent->debugClient().addMessage( "Savior:Stop" );
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__ ": stop. line=%d", __LINE__ );
            return true;
        }
    }


    //
    // turn body angle against ball
    //
    const rcsc::Vector2D final_body_pos = wm.self().inertiaFinalPoint();

    rcsc::AngleDeg target_body_angle = 0.0;

    if ( ((ball_pos - final_body_pos).r() <= 10.0
          && (wm.existKickableOpponent() || wm.existKickableTeammate()))
         || goal_line_positioning )
    {
        if ( goal_line_positioning
#ifdef GOAL_LINE_POSITIONINIG_FACE_BODY
             && ( opponent_reach_cycle <= 1
                  && final_body_pos.absY() >= param.goalHalfWidth() + 0.5 )
#endif
           )
        {
            target_body_angle = rcsc::sign( final_body_pos.y ) * 90.0;
        }
        else
        {
            rcsc::AngleDeg diff_body_angle = 0.0;

            if ( final_body_pos.y > 0.0 )
            {
                diff_body_angle = + 90.0;
            }
            else
            {
                diff_body_angle = - 90.0;
            }

            if ( translateXIsLessEqualsTo( reverse_x, final_body_pos.x, -45.0 ) )
            {
                diff_body_angle *= -1.0;
            }

            target_body_angle = (ball_pos - final_body_pos).th()
                                + diff_body_angle;
        }
    }

    target_body_angle = this->translateTheta( reverse_x, target_body_angle );

    if ( rcsc::Body_TurnToAngle( target_body_angle ).execute( agent ) )
    {
        agent->debugClient().addMessage( "Savior:TurnTo%.0f",
                                         target_body_angle.degree() );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__ ": turn to angle %.1f",
                            target_body_angle.degree() );
        return true;
    }

    agent->debugClient().addMessage( "Savior:NoAction" );
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__ ": no action" );
    return true;
}


bool
Bhv_Savior::doKick( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();
    const rcsc::ServerParam & param = rcsc::ServerParam::i();

    //
    // if exist near opponent, clear the ball
    //
    if ( wm.existKickableOpponent()
         || wm.interceptTable()->opponentReachCycle() < 4 )
    {
        Body_ClearBall2008().execute( agent );
        agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );

        agent->debugClient().addMessage( "clear ball" );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            "opponent near or kickable, clear ball" );
        return true;
    }


    const double clear_y_threshold = param.penaltyAreaHalfWidth() - 5.0;

    //
    // do pass
    //
    const PassCourseTable & table = PassCourseTable::instance
                                    ( wm, ! wm.teammates().empty() );

    PassCourseTable::CourseSelection result = table.get_first_selection( wm );

    const rcsc::Vector2D goal_pos = rcsc::ServerParam::i().theirTeamGoalPos();
    agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );

    switch( result.type )
    {
    case PassCourseTable::CourseSelection::Shoot:
        if ( Body_ForceShoot().execute( agent ) )
        {
            agent->debugClient().addMessage( "Savior:Shoot" );
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": doKick() force shoot" );
            return true;
        }

        break;

    case PassCourseTable::CourseSelection::Push:
        {
            rcsc::Vector2D dribble_target;

            if ( wm.self().pos().x > + 35.0 )
            {
                dribble_target = rcsc_ext::ServerParam_opponentTeamFarGoalPostPos
                                 ( wm.self().pos() );
            }
            else
            {
                dribble_target = wm.self().pos() + rcsc::Vector2D( 10.0, 0.0 );
            }

            agent->debugClient().addMessage( "Savior:Dribble" );
            agent->debugClient().setTarget( dribble_target );
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": doKick() dribble to (%.1f %.1f)",
                                dribble_target.x, dribble_target.y );
            rcsc::Body_Dribble2008( dribble_target,
                                    1.0,
                                    param.maxPower(),
                                    3 ).execute( agent );
            return true;
            break;
        }

    case PassCourseTable::CourseSelection::FindPassReceiver:
    case PassCourseTable::CourseSelection::Pass:
        {
            const rcsc::AbstractPlayerObject * target = wm.ourPlayer( result.player_number );
            rcsc::Vector2D target_point = target->pos() + target->vel();

            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": doKick() self pos = [%f, %f], "
                                "pass target = [%f, %f]",
                                wm.self().pos().x, wm.self().pos().y,
                                target_point.x, target_point.y );

            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": doKick(): side? = %s",
                                ( (wm.self().pos().absY() > clear_y_threshold)?
                                  "true" : "false" ) );

            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": doKick() same side = %s,"
                                " target y abs = %f, self y abs = %f,"
                                " self y abs - 5.0 = %f,"
                                " %f > %f = %s",
                                (target_point.y * wm.self().pos().y > 0.0?
                                 "true": "false"),
                                std::fabs( target_point.y ),
                                std::fabs( wm.self().pos().y ),
                                std::fabs( wm.self().pos().y ) - 5.0,
                                std::fabs( target_point.y ),
                                std::fabs( wm.self().pos().y ) - 5.0,
                                ( ( std::fabs( target_point.y )
                                    > (std::fabs( wm.self().pos().y ) - 5.0) )
                                  ? "true": "false") );


            //
            // if side of field, clear the ball
            //
            if ( wm.self().pos().absY() > clear_y_threshold
                 && ! (target_point.y * wm.self().pos().y > 0.0
                       && std::fabs( target_point.y ) > (std::fabs( wm.self().pos().y ) + 5.0)
                       && target_point.x > wm.self().pos().x + 5.0 ) )
            {
                rcsc::dlog.addText( rcsc::Logger::TEAM,
                                    __FILE__": doKick() self pos y %f is grater than %f,"
                                    "pass target pos y = %f, clear ball",
                                    wm.self().pos().absY(),
                                    clear_y_threshold,
                                    target_point.y );
                agent->debugClient().addMessage( "Savior:ForceClear" );

                Body_ClearBall2008().execute( agent );
                agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
                return true;
            }
            else
            {
                rcsc::dlog.addText( rcsc::Logger::TEAM,
                                    __FILE__": doKick() not force clear" );
            }


            //
            // find player
            //
            if ( result.type == PassCourseTable::CourseSelection::FindPassReceiver
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
                    agent->debugClient().addMessage( "Savior:FindReceiver" );
                    rcsc::dlog.addText( rcsc::Logger::TEAM,
                                        __FILE__":doKick()  find receiver." );
                    return true;
                }
                else
                {
                    rcsc::Body_HoldBall2008( true,
                                             target_point,
                                             target_point ).execute( agent );
                    agent->setNeckAction( new rcsc::Neck_TurnToPoint( target_point ) );
                    agent->debugClient().addMessage( "Savior:FindHold" );
                    rcsc::dlog.addText( rcsc::Logger::TEAM,
                                        __FILE__": doKick() hold ball." );
                    return true;
                }
            }

            //
            // pass kick
            //
            table.setDebugPassRoute( agent );

            rcsc::Body_SmartKick( target_point,
                                  param.maxPower(),
                                  param.maxPower() * 0.96,
                                  3 ).execute( agent );
            agent->setNeckAction( new rcsc::Neck_TurnToPoint( target_point ) );

            agent->debugClient().addMessage( "Savior:Pass" );
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": doKick() pass to %d (%.1f %.1f).",
                                target->unum(),
                                target->pos().x, target->pos().y );
            agent->debugClient().setTarget( target->unum() );

            if ( agent->config().useCommunication()
                 && result.player_number != rcsc::Unum_Unknown )
            {
                rcsc::dlog.addText( rcsc::Logger::TEAM,
                                    __FILE__
                                    ": execute() set pass communication." );
                agent->debugClient().addMessage( "Savior:Say_Pass" );

                rcsc::Vector2D target_buf = target_point - agent->world().self().pos();
                target_buf.setLength( 1.0 );

                agent->addSayMessage( new rcsc::PassMessage( result.player_number,
                                                             target_point + target_buf,
                                                             agent->effector().queuedNextBallPos(),
                                                             agent->effector().queuedNextBallVel() ) );
            }
            return true;
            break;
        }

    case PassCourseTable::CourseSelection::Hold:
        Body_ClearBall2008().execute( agent );
        agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );

        agent->debugClient().addMessage( "Savior:ClearBall" );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": doKick(): hold returned, clear ball" );
        return true;
        break;
    }

    return false;
}


// XXX: move this code to other place

/*!
  \class ContainsPlayerPredicate
  \brief check if target player is in region
*/
template<typename T>
class ContainsPlayerPredicate
    : public rcsc::PlayerPredicate
{
private:
    //! geomotry to check
    T M_geom;

public:
    /*!
      \brief construct with the geometry object
      \param geometry object for checking
    */
    ContainsPlayerPredicate( const T & geom )
        : M_geom( geom )
      {
      }

    /*!
      \brief predicate function
      \param p const reference to the target player object
      \return true if target player is in the region
    */
    bool operator()( const rcsc::AbstractPlayerObject & p ) const
      {
          return M_geom.contains( p.pos() );
      }
};

rcsc::Vector2D
Bhv_Savior::getBasicPosition( rcsc::PlayerAgent * agent,
                              bool penalty_kick_mode,
                              bool despair_mode,
                              bool reverse_x,
                              bool * in_region,
                              bool * dangerous,
                              bool * aggressive,
                              bool * emergent_advance,
                              bool * goal_line_positioning ) const
{
    if ( in_region )
    {
        *in_region = false;
    }

    if ( dangerous )
    {
        *dangerous = false;
    }

    if ( aggressive )
    {
        *aggressive = false;
    }

    if ( emergent_advance )
    {
        *emergent_advance = false;
    }

    if ( goal_line_positioning )
    {
        *goal_line_positioning = false;
    }

    const rcsc::WorldModel & wm = agent->world();
    const rcsc::ServerParam & param = rcsc::ServerParam::i();

    static const double AGGRESSIVE_POSITIONING_STAMINA_RATE_THRESHOLD = 0.85;


    const int teammate_reach_cycle = wm.interceptTable()->teammateReachCycle();
    const int opponent_reach_cycle = wm.interceptTable()->opponentReachCycle();
    const int self_reach_cycle = wm.interceptTable()->selfReachCycle();
    int predict_step = std::min( opponent_reach_cycle,
                                 std::min( teammate_reach_cycle,
                                           self_reach_cycle ) );

    const rcsc::Vector2D self_pos = wm.self().pos();
    const rcsc::Vector2D ball_pos = getBoundPredictBallPos( wm, predict_step, 0.5 );

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": (getBasicPosition) ball predict pos = (%f,%f)",
                        ball_pos.x, ball_pos.y );

    rcsc::Vector2D positioning_base_pos = ball_pos;

#if 0
    const rcsc::AbstractPlayerObject * holder = wm.interceptTable()->fastestOpponent();

    if ( holder )
    {
        double ball_rate = 3.0 / 4.0;

        positioning_base_pos = ball_pos * ball_rate
            + holder->pos() * (1.0 - ball_rate);
    }
#endif


    rcsc::Rect2D penalty_area;
    rcsc::Rect2D conservative_area;
    rcsc::Vector2D goal_pos;

    if ( reverse_x )
    {
        penalty_area.assign( rcsc::Vector2D( param.theirPenaltyAreaLineX(),
                                             - param.penaltyAreaHalfWidth() ),
                             rcsc::Size2D( param.penaltyAreaLength(),
                                           param.penaltyAreaWidth() ) );

#if defined( OLD_GOAL_LINE_POSITINING_CONDITION )
        conservative_area.assign( rcsc::Vector2D( param.theirPenaltyAreaLineX() - 15.0,
                                                  - param.penaltyAreaHalfWidth() ),
                                  rcsc::Size2D( param.penaltyAreaLength() + 15.0,
                                                param.penaltyAreaWidth() ) );
#else
        conservative_area.assign( rcsc::Vector2D( param.theirPenaltyAreaLineX() - 15.0,
                                                  - param.pitchHalfWidth() ),
                                  rcsc::Size2D( param.penaltyAreaLength() + 15.0,
                                                param.pitchWidth() ) );
#endif

        goal_pos = rcsc::ServerParam::i().theirTeamGoalPos();
    }
    else
    {
        penalty_area.assign( rcsc::Vector2D( param.ourTeamGoalLineX(),
                                             - param.penaltyAreaHalfWidth() ),
                             rcsc::Size2D( param.penaltyAreaLength(),
                                           param.penaltyAreaWidth() ) );

#if defined( OLD_GOAL_LINE_POSITINING_CONDITION )
        conservative_area.assign( rcsc::Vector2D( param.ourTeamGoalLineX(),
                                                  - param.penaltyAreaHalfWidth() ),
                                  rcsc::Size2D( param.penaltyAreaLength() + 15.0,
                                                param.penaltyAreaWidth() ) );
#else
        conservative_area.assign( rcsc::Vector2D( param.ourTeamGoalLineX(),
                                                  - param.pitchHalfWidth() ),
                                  rcsc::Size2D( param.penaltyAreaLength() + 15.0,
                                                param.pitchWidth() ) );
#endif


        goal_pos = rcsc::ServerParam::i().ourTeamGoalPos();
    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": (getBasicPosition) number of teammates in conservative area is %d",
                        wm.getPlayerCont( new rcsc::AndPlayerPredicate
                                          ( new rcsc::TeammatePlayerPredicate( wm ),
                                            new rcsc::ContainsPlayerPredicate< rcsc::Rect2D >
                                            ( conservative_area ) ) ).size() );

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": (getBasicPosition) ball in area = %d",
                        (int)(conservative_area.contains( wm.ball().pos() )) );

#if 0
# if 0
    double base_dist = 22.0;
# elif 0
    double base_dist = 20.0;
#else
    double base_dist = 16.0;
# endif
#else
    double base_dist = 14.0;
#endif

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": (getBasicPosition) basic base dist = %f",
                        base_dist );

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": (getBasicPosition) defenseLineX = %f",
                        wm.ourDefenseLineX() );

    bool goal_line_positioning_flag = false;

#if defined( OLD_GOAL_LINE_POSITINING_CONDITION )
    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": (getBasicPosition) ball dir from goal = %f",
                        getDirFromGoal( ball_pos ).degree() );

#if 1
    bool is_side_angle = ( getDirFromGoal( ball_pos ).abs() > 30.0 );
# elif 0
    bool is_side_angle = ( getDirFromGoal( ball_pos ).abs() > 35.0 );
# elif 1
    bool is_side_angle = ( getDirFromGoal( ball_pos ).abs() > 40.0 );
#else
    bool is_side_angle = ( getDirFromGoal( ball_pos ).abs() > 45.0 );
#endif

#ifdef FORCE_GOAL_LINE_POSITIONONG
    if ( ((! is_side_angle && wm.defenseLineX() < -15.0)
          || (wm.defenseLineX() < -15.0 && wm.defenseLineX() > -40.0))
         && ! penalty_kick_mode )
    {
        goal_line_positioning_flag = true;

        if ( goal_line_positioning )
        {
            *goal_line_positioning = goal_line_positioning_flag;
        }

        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": (getBasicPosition) "
                            "force goal line positioning" );
    }
    else
#endif
    // many teammates are in penalty area, do back positioning
    if ( ! is_side_angle
         && penalty_area.contains( wm.ball().pos() )
         && wm.getPlayerCont
         ( new rcsc::AndPlayerPredicate
           ( new rcsc::TeammatePlayerPredicate( wm ),
             new ContainsPlayerPredicate< rcsc::Rect2D >
             ( penalty_area ) ) ).size() >= 3 )
    {
        base_dist = std::min( base_dist,
                              std::max( wm.defenseLineX()
                                        - param.ourPenaltyAreaLineX() - 2.0,
                                        2.0 ) );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": (getBasicPosition) back positioning, "
                            "base dist = %f", base_dist );

# if 0
        if ( base_dist < 10.0
             && std::fabs( ball_pos.y ) < param.goalHalfWidth() )
# elif 0
        if ( base_dist < 12.0
             && std::fabs( ball_pos.y ) < param.goalHalfWidth() + 5.0 )
# else
        if ( std::fabs( ball_pos.y ) < param.goalHalfWidth() + 5.0 )
# endif
        {
            goal_line_positioning_flag = true;

            if ( goal_line_positioning )
            {
                *goal_line_positioning = goal_line_positioning_flag;
            }
        }
    }
#else
    if ( isGoalLinePositioningSituation( wm, ball_pos, penalty_kick_mode ) )
    {
            goal_line_positioning_flag = true;

            if ( goal_line_positioning )
            {
                *goal_line_positioning = goal_line_positioning_flag;
            }
    }
#endif

    // if ball is not in penalty area and teammates are in penalty area
    // (e.g. our clear ball succeded), do back positioning
    if ( ! conservative_area.contains( wm.ball().pos() )
         && wm.getPlayerCont
               ( new rcsc::AndPlayerPredicate
                 ( new rcsc::TeammatePlayerPredicate( wm ),
                   new rcsc::ContainsPlayerPredicate<rcsc::Rect2D>
                       ( conservative_area ) ) ).size() >= 2 )
    {
#ifdef DO_GOAL_LINE_POSITIONONG
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": goal line positioning" );

        goal_line_positioning_flag = true;

        if ( goal_line_positioning )
        {
            *goal_line_positioning = goal_line_positioning_flag;
        }
#else
        base_dist = 5.0;
#endif
    }

    // aggressive positioning
#if defined( OLD_AGGRESSIVE_POSITIONING )
    const double additional_forward_positioning_max = 18.0;
#else
    const double additional_forward_positioning_max = 25.0;
#endif

    const double emergent_advance_dist = (goal_pos - ball_pos).r() - 3.0;

    bool emergent_advance_mode = false;
#if defined( DO_EMERGENT_ADVANCE )
    const rcsc::AbstractPlayerObject * fastestOpponent = wm.interceptTable()->fastestOpponent();
    // if opponent player will be have the ball at backward of our
    // defense line, do 1-on-1 aggressive defense
#if defined( OLD_EMERGENT_ADVANCE )
    if ( ball_pos.x <= -20.0
         && ball_pos.x >= param.ourPenaltyAreaLineX() + 5.0
         && opponent_reach_cycle < teammate_reach_cycle
         && ball_pos.x <= wm.ourDefenseLineX() + 5.0
         && (getDirFromGoal( ball_pos )
             - getDirFromGoal( wm.self().inertiaPoint(1) )).abs()
            < EMERGENT_ADVANCE_DIR_DIFF_THRESHOLD
         && canShootFrom( wm, ball_pos, 20 )
         && ! wm.existKickableOpponent()
         && ! penalty_kick_mode
         && fastestOpponent
         && wm.getPlayerCont
               ( new rcsc::AndPlayerPredicate
                 ( new rcsc::TeammatePlayerPredicate( wm ),
                   new rcsc::XCoordinateBackwardPlayerPredicate
                       ( fastestOpponent->pos().x ),
                   new rcsc::YCoordinatePlusPlayerPredicate
                       ( fastestOpponent->pos().y - 1.0 ),
                   new rcsc::YCoordinateMinusPlayerPredicate
                       ( fastestOpponent->pos().y + 1.0 ) ) ).empty()
         && emergent_advance_dist < 30.0 )
#else
    if ( ball_pos.x >= param.ourPenaltyAreaLineX() + 5.0
         && opponent_reach_cycle < teammate_reach_cycle
         && ball_pos.x <= wm.defenseLineX() + 5.0
         && (getDirFromGoal( ball_pos )
             - getDirFromGoal( wm.self().inertiaPoint(1) )).abs()
            < EMERGENT_ADVANCE_DIR_DIFF_THRESHOLD
         && canShootFrom( wm, ball_pos, 20 )
         && ! penalty_kick_mode
         && fastestOpponent
         && wm.getPlayerCont
               ( new rcsc::AndPlayerPredicate
                 ( new rcsc::TeammatePlayerPredicate( wm ),
                   new rcsc::XCoordinateBackwardPlayerPredicate
                       ( fastestOpponent->pos().x ),
                   new rcsc::YCoordinatePlusPlayerPredicate
                       ( fastestOpponent->pos().y - 1.0 ),
                   new rcsc::YCoordinateMinusPlayerPredicate
                       ( fastestOpponent->pos().y + 1.0 ) ) ).empty()
         && getDirFromGoal( ball_pos ).abs() < 20.0 )
#endif
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": (getBasicPosition) emergent advance mode" );
        emergent_advance_mode = true;

        if ( emergent_advance )
        {
            *emergent_advance = true;
        }

        agent->debugClient().addMessage( "EmergentAdvance" );
    }
#endif


    if ( penalty_kick_mode )
    {
        base_dist = (goal_pos - ball_pos).r() - 3.0;

        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": (getBasicPosition) penalty kick mode, "
                            "new base dist = %d", base_dist );
    }
    else if ( emergent_advance_mode )
    {
        base_dist = emergent_advance_dist;
    }
#if defined( OLD_AGGRESSIVE_POSITIONING )
    else if ( wm.ourDefenseLineX() >= - additional_forward_positioning_max
              && wm.self().stamina()
                 >= rcsc::ServerParam::i().staminaMax()
                    * AGGRESSIVE_POSITIONING_STAMINA_RATE_THRESHOLD )
#else
    else if ( wm.defenseLineX() >= -20.0
              && wm.self().stamina()
                 >= rcsc::ServerParam::i().staminaMax()
                    * AGGRESSIVE_POSITIONING_STAMINA_RATE_THRESHOLD )
#endif
    {
        base_dist += std::min( wm.ourDefenseLineX(), 0.0 )
                     + additional_forward_positioning_max;

        agent->debugClient().addMessage( "AggressivePositioniong" );

        if ( aggressive )
        {
            *aggressive = true;
        }
    }
    else
    {
        if ( aggressive )
        {
            *aggressive = false;
        }
    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__ ": (getBasicPosition) base dist = %f", base_dist );

    if ( in_region )
    {
        *in_region = true;
    }

    if ( dangerous )
    {
        *dangerous = false;
    }

    if ( emergent_advance )
    {
        *emergent_advance = emergent_advance_mode;
    }

    if ( ! reverse_x )
    {
        return getBasicPositionFromBall( agent,
                                         positioning_base_pos,
                                         base_dist,
                                         wm.self().pos(),
                                         penalty_kick_mode,
                                         despair_mode,
                                         in_region,
                                         dangerous,
                                         emergent_advance_mode,
                                         goal_line_positioning );
    }
    else
    {
        rcsc::Vector2D reversed_ball_pos( - positioning_base_pos.x,
                                          positioning_base_pos.y );

        rcsc::Vector2D self_pos( - wm.self().pos().x, wm.self().pos().y );

        rcsc::Vector2D result_pos = getBasicPositionFromBall
                                    ( agent,
                                      reversed_ball_pos,
                                      base_dist,
                                      self_pos,
                                      penalty_kick_mode,
                                      despair_mode,
                                      in_region,
                                      dangerous,
                                      emergent_advance_mode,
                                      goal_line_positioning );

        return rcsc::Vector2D( - result_pos.x, result_pos.y );
    }
}

rcsc::Vector2D
Bhv_Savior::getBasicPositionFromBall( rcsc::PlayerAgent * agent,
                                      const rcsc::Vector2D & ball_pos,
                                      double base_dist,
                                      const rcsc::Vector2D & self_pos,
                                      bool no_danger_angle,
                                      bool despair_mode,
                                      bool * in_region,
                                      bool * dangerous,
                                      bool emergent_advance,
                                      bool * goal_line_positioning ) const
{
    static const double GOAL_LINE_POSITIONINIG_GOAL_X_DIST = 1.5;
    static const double MINIMUM_GOAL_X_DIST = GOAL_LINE_POSITIONINIG_GOAL_X_DIST;

    const rcsc::WorldModel & wm = agent->world();

    const double goal_line_x = rcsc::ServerParam::i().ourTeamGoalLineX();
    const double goal_half_width = rcsc::ServerParam::i().goalHalfWidth();

    const double alpha = std::atan2( goal_half_width, base_dist );

    const double min_x = (goal_line_x + MINIMUM_GOAL_X_DIST);

    const rcsc::Vector2D goal_center = rcsc::ServerParam::i().ourTeamGoalPos();
    const rcsc::Vector2D goal_left_post( goal_center.x, +goal_half_width );
    const rcsc::Vector2D goal_right_post( goal_center.x, -goal_half_width );

    rcsc::Line2D line_1( ball_pos, goal_left_post );
    rcsc::Line2D line_2( ball_pos, goal_right_post );

    rcsc::AngleDeg line_dir = rcsc::AngleDeg::normalize_angle
                              ( rcsc::AngleDeg::normalize_angle
                                ( (goal_left_post - ball_pos).th().degree()
                                    + (goal_right_post - ball_pos).th().degree() )
                                  / 2.0 );

    rcsc::Line2D line_m( ball_pos, line_dir );

    rcsc::Line2D goal_line( goal_left_post, goal_right_post );

    if ( ! emergent_advance
         && ( goal_line_positioning != static_cast<bool *>(0)
              && *goal_line_positioning ) )
    {
        return this->getGoalLinePositioningTarget
                     ( agent, wm, GOAL_LINE_POSITIONINIG_GOAL_X_DIST,
                       ball_pos, despair_mode );
    }


    rcsc::Vector2D p = goal_line.intersection( line_m );

    if ( ! p.isValid() )
    {
        if ( ball_pos.x > 0.0 )
        {
            return rcsc::Vector2D( min_x, goal_left_post.y );
        }
        else if ( ball_pos.x < 0.0 )
        {
            return rcsc::Vector2D( min_x, goal_right_post.y );
        }
        else
        {
            return rcsc::Vector2D( min_x, goal_center.y );
        }
    }

    if ( in_region != static_cast<bool *>(0) )
    {
        *in_region = false;
    }

    // angle -> dist
    double dist_from_goal = (line_1.dist(p) + line_2.dist(p)) / 2.0
        / std::sin(alpha);

    if ( dist_from_goal <= goal_half_width )
    {
        dist_from_goal = goal_half_width;
    }

    if ( (ball_pos - p).r() + 1.5 < dist_from_goal )
    {
        dist_from_goal = (ball_pos - p).r() + 1.5;
    }

    rcsc::AngleDeg current_position_line_dir = (self_pos - p).th();

    rcsc::AngleDeg position_error = line_dir - current_position_line_dir;

    const double danger_angle = 21.0;

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": position angle error = %f",
                        position_error.abs() );

    agent->debugClient().addMessage( "angleError = %f", position_error.abs() );

    if ( ! no_danger_angle
         && position_error.abs() > danger_angle
         && (ball_pos - goal_center).r() < 50.0
         && (p - self_pos).r() > 5.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": position angle error too big,"
                            " dangerous" );

        dist_from_goal *= (1.0 - (position_error.abs() - danger_angle)
                           / (180.0 - danger_angle) ) / 3.0;

        if ( dist_from_goal < goal_half_width - 1.0 )
        {
            dist_from_goal = goal_half_width - 1.0;
        }

        if ( dangerous != static_cast<bool *>(0) )
        {
            *dangerous = true;
        }

#if defined( OLD_GOAL_LINE_POSITINING_CONDITION )
        if ( self_pos.x < -45.0
             && ( ball_pos - self_pos ).r() < 20.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                "change to goal line positioning" );

            if ( goal_line_positioning != static_cast<bool *>(0) )
            {
                *goal_line_positioning = true;
            }

            return this->getGoalLinePositioningTarget
                         ( agent, wm, GOAL_LINE_POSITIONINIG_GOAL_X_DIST,
                           ball_pos, despair_mode );
        }
#endif
    }
#if 0
    else if ( ! no_danger_angle
              && position_error.abs() > 5.0
              && (p - self_pos).r() > 5.0 )
#else
    else if ( position_error.abs() > 5.0
              && (p - self_pos).r() > 5.0 )
#endif
    {
        double current_position_dist = (p - self_pos).r();

        if ( dist_from_goal < current_position_dist )
        {
            dist_from_goal = current_position_dist;
        }

        dist_from_goal = (p - self_pos).r();

        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": position angle error big,"
                            " positioning has changed to projection point, "
                            "new dist_from_goal = %f", dist_from_goal );

        if ( dangerous != static_cast<bool *>(0) )
        {
            *dangerous = true;
        }

#if defined( OLD_GOAL_LINE_POSITINING_CONDITION )
        if ( self_pos.x < -45.0
             && ( ball_pos - self_pos ).r() < 20.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                "change to goal line positioning" );

            if ( goal_line_positioning != static_cast<bool *>(0) )
            {
                *goal_line_positioning = true;
            }

            return this->getGoalLinePositioningTarget
                         ( agent, wm, GOAL_LINE_POSITIONINIG_GOAL_X_DIST,
                           ball_pos, despair_mode );
        }
#endif

        {
            const rcsc::Vector2D r = p + (ball_pos - p).normalizedVector() * dist_from_goal;

            agent->debugClient().addLine( p, r );
            agent->debugClient().addLine( self_pos, r );
        }
    }
    else if ( ! no_danger_angle
              && position_error.abs() > 10.0 )
    {
        dist_from_goal = std::min( dist_from_goal, 14.0 );

        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": position angle error is big,"
                            "new dist_from_goal = %d", dist_from_goal );
    }


    rcsc::Vector2D result = p + (ball_pos - p).normalizedVector() * dist_from_goal;

    if ( result.x < min_x )
    {
        result.assign( min_x, result.y );
    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": positioning target = [%f,%f]",
                        result.x, result.y );

    return result;
}




rcsc::Vector2D
Bhv_Savior::getGoalLinePositioningTarget( rcsc::PlayerAgent * agent,
                                          const rcsc::WorldModel & wm,
                                          const double goal_x_dist,
                                          const rcsc::Vector2D & ball_pos,
                                          bool despair_mode ) const
{
    const double goal_line_x = rcsc::ServerParam::i().ourTeamGoalLineX();

    const double goal_half_width = rcsc::ServerParam::i().goalHalfWidth();
    const rcsc::Vector2D goal_center = rcsc::ServerParam::i().ourTeamGoalPos();
    const rcsc::Vector2D goal_left_post( goal_center.x, +goal_half_width );
    const rcsc::Vector2D goal_right_post( goal_center.x, -goal_half_width );

    rcsc::AngleDeg line_dir = getDirFromGoal( ball_pos );

    rcsc::Line2D line_m( ball_pos, line_dir );

    rcsc::Line2D goal_line( goal_left_post, goal_right_post );

    rcsc::Line2D target_line( goal_left_post
                              + rcsc::Vector2D( goal_x_dist, 0.0 ),
                              goal_right_post
                              + rcsc::Vector2D( goal_x_dist, 0.0 ) );

    if ( despair_mode )
    {
        double target_x = std::max( wm.self().inertiaPoint( 1 ).x,
                                    goal_line_x );

        rcsc::Line2D positioniong_line( rcsc::Vector2D( target_x, -1.0 ),
                                        rcsc::Vector2D( target_x, +1.0 ) );

        if ( wm.ball().vel().r() < rcsc::EPS )
        {
            return wm.ball().pos();
        }

        rcsc::Line2D ball_line( ball_pos, wm.ball().vel().th() );

        rcsc::Vector2D c = positioniong_line.intersection( ball_line );

        if ( c.isValid() )
        {
            return c;
        }
    }


    rcsc::Vector2D p = target_line.intersection( line_m );

    if ( ! p.isValid() )
    {
        double target_y;

        if ( ball_pos.absY() > rcsc::ServerParam::i().goalHalfWidth() )
        {
            target_y = rcsc::sign( ball_pos.y )
                * std::min( ball_pos.absY(),
                            rcsc::ServerParam::i().goalHalfWidth()
                            + 2.0 );
        }
        else
        {
            target_y = ball_pos.y * 0.8;
        }

        p = rcsc::Vector2D( goal_line_x + goal_x_dist, target_y );
    }


#if 1
    const int teammate_reach_cycle = wm.interceptTable()->teammateReachCycle();
    const int opponent_reach_cycle = wm.interceptTable()->opponentReachCycle();
    int predict_step = std::min( opponent_reach_cycle,
                                 teammate_reach_cycle );

    rcsc::Vector2D predict_ball_pos = wm.ball().inertiaPoint( predict_step );

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": predict_step = %d, "
                        "predict_ball_pos = [%f, %f]",
                        predict_step, predict_ball_pos.x, predict_ball_pos.y );

    bool can_shoot_from_predict_ball_pos = canShootFrom( wm, predict_ball_pos );

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": can_shoot_from_predict_ball_pos = %s",
                        ( can_shoot_from_predict_ball_pos? "true": "false" ) );

# if ! defined( DISABLE_GOAL_LINE_POSITIONING_SHIFT_TO_SHOOT_POINT )
    if ( can_shoot_from_predict_ball_pos )
    {
        rcsc::Vector2D old_p = p;

        //
        // shift position to shoot point
        //
        p += rcsc::Vector2D( 0.0, predict_ball_pos.y - p.y ) * 0.2;

        agent->debugClient().addCircle( old_p, 0.5 );

        agent->debugClient().addLine( p + rcsc::Vector2D( -1.5, -1.5 ),
                                      p + rcsc::Vector2D( +1.5, +1.5 ) );
        agent->debugClient().addLine( p + rcsc::Vector2D( -1.5, +1.5 ),
                                      p + rcsc::Vector2D( +1.5, -1.5 ) );

        agent->debugClient().addLine( old_p, p );

        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": new position = [%f, %f]",
                            p.x, p.y );
    }
# else
    static_cast< void >( agent );
# endif

#endif


    if ( p.absY() > rcsc::ServerParam::i().goalHalfWidth() + 2.0 )
    {
        p.assign( p.x,
                  rcsc::sign( p.y )
                  * ( rcsc::ServerParam::i().goalHalfWidth() + 2.0 ) );
    }

    return p;
}


/*-------------------------------------------------------------------*/
rcsc::AngleDeg
Bhv_Savior::getDirFromGoal( const rcsc::Vector2D & pos )
{
    const double goal_half_width = rcsc::ServerParam::i().goalHalfWidth();

    const rcsc::Vector2D goal_center = rcsc::ServerParam::i().ourTeamGoalPos();
    const rcsc::Vector2D goal_left_post( goal_center.x, +goal_half_width );
    const rcsc::Vector2D goal_right_post( goal_center.x, -goal_half_width );

    return rcsc::AngleDeg( ( (pos - goal_left_post).th().degree()
                             + (pos - goal_right_post).th().degree() ) / 2.0 );
}


/*-------------------------------------------------------------------*/
rcsc::Vector2D
Bhv_Savior::getBallFieldLineCrossPoint( const rcsc::Vector2D & ball_from,
                                        const rcsc::Vector2D & ball_to,
                                        const rcsc::Vector2D & p1,
                                        const rcsc::Vector2D & p2,
                                        double field_back_offset )
{
    const rcsc::Segment2D line( p1, p2 );
    const rcsc::Segment2D ball_segment( ball_from, ball_to );

    rcsc::Vector2D cross_point = ball_segment.intersection( line, true );

    if ( cross_point.isValid() )
    {
        if ( rcsc::Vector2D( ball_to - ball_from ).r() <= rcsc::EPS )
        {
            return cross_point;
        }

        return cross_point
                + rcsc::Vector2D::polar2vector
                  ( field_back_offset,
                    rcsc::Vector2D( ball_from - ball_to ).th() );
    }

    return ball_to;
}

/*-------------------------------------------------------------------*/
rcsc::Vector2D
Bhv_Savior::getBoundPredictBallPos( const rcsc::WorldModel & wm,
                                    int predict_step,
                                    double field_back_offset )
{
    rcsc::Vector2D current_pos = wm.ball().pos();
    rcsc::Vector2D predict_pos = wm.ball().inertiaPoint( predict_step );

    const double wid = rcsc::ServerParam::i().pitchHalfWidth();
    const double len = rcsc::ServerParam::i().pitchHalfLength();

    const rcsc::Vector2D corner_1( + len, + wid );
    const rcsc::Vector2D corner_2( + len, - wid );
    const rcsc::Vector2D corner_3( - len, - wid );
    const rcsc::Vector2D corner_4( - len, + wid );

    rcsc::Vector2D p0 = predict_pos;
    rcsc::Vector2D p1;
    rcsc::Vector2D p2;
    rcsc::Vector2D p3;
    rcsc::Vector2D p4;

    p1 = getBallFieldLineCrossPoint( current_pos, p0, corner_1, corner_2, field_back_offset );
    p2 = getBallFieldLineCrossPoint( current_pos, p1, corner_2, corner_3, field_back_offset );
    p3 = getBallFieldLineCrossPoint( current_pos, p2, corner_3, corner_4, field_back_offset );
    p4 = getBallFieldLineCrossPoint( current_pos, p3, corner_4, corner_1, field_back_offset );

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": getBoundPredictBallPos "
                        "result = [%f, %f]",
                        p4.x, p4.y );

    return p4;
}


/*-------------------------------------------------------------------*/
double
Bhv_Savior::getTackleProbability( const rcsc::Vector2D & body_relative_ball )
{
    const rcsc::ServerParam &param = rcsc::ServerParam::i();

    double tackle_length;

    if ( body_relative_ball.x > 0.0 )
    {
        if ( rcsc::ServerParam::i().tackleDist() > rcsc::EPS )
        {
            tackle_length = param.tackleDist();
        }
        else
        {
            return 0.0;
        }
    }
    else
    {
        if ( param.tackleBackDist() > rcsc::EPS )
        {
            tackle_length = param.tackleBackDist();
        }
        else
        {
            return 0.0;
        }
    }

    if ( param.tackleWidth() < rcsc::EPS )
    {
        return 0.0;
    }


    double prob = 1.0;

    // virtial dist penalty
    prob -= std::pow( body_relative_ball.absX() / tackle_length,
                      param.tackleExponent() );

    // horizontal dist penalty
    prob -= std::pow( body_relative_ball.absY() / param.tackleWidth(),
                      param.tackleExponent() );

    return std::min( 0.0, prob );
}

/*-------------------------------------------------------------------*/
rcsc::Vector2D
Bhv_Savior::getSelfNextPosWithDash( const rcsc::WorldModel & wm,
                                    double dash_power )
{
    return wm.self().inertiaPoint( 1 )
           + rcsc::Vector2D::polar2vector
             ( dash_power * wm.self().playerType().dashPowerRate(),
               wm.self().body() );
}

double
Bhv_Savior::getSelfNextTackleProbabilityWithDash( const rcsc::WorldModel & wm,
                                                  double dash_power )
{
    return getTackleProbability( ( wm.ball().inertiaPoint(1)
                                   - getSelfNextPosWithDash( wm, dash_power ) )
                                 .rotatedVector( - wm.self().body() ) );
}

/*-------------------------------------------------------------------*/
double
Bhv_Savior::translateX( bool reverse, double x )
{
    if ( reverse )
    {
        return -x;
    }
    else
    {
        return x;
    }
}

rcsc::Vector2D
Bhv_Savior::translateXVec( bool reverse, const rcsc::Vector2D & vec )
{
    if ( reverse )
    {
        return rcsc::Vector2D( - vec.x, vec.y );
    }
    else
    {
        return vec;
    }
}

rcsc::AngleDeg
Bhv_Savior::translateTheta( bool reverse, const rcsc::AngleDeg & theta )
{
    if ( reverse )
    {
        return theta + 180.0;
    }
    else
    {
        return theta;
    }
}

bool
Bhv_Savior::translateXIsLessEqualsTo( bool reverse,
                                      double x, double threshold )
{
    if ( reverse )
    {
        return (-x >= threshold);
    }
    else
    {
        return (x <= threshold);
    }
}


bool
Bhv_Savior::canShootFrom( const rcsc::WorldModel & wm,
                          const rcsc::Vector2D & pos,
                          long valid_teammate_threshold )
{
    static const double SHOOTABLE_THRESHOLD = 12.0;

    return getFreeAngleFromPos( wm, valid_teammate_threshold, pos )
            >= SHOOTABLE_THRESHOLD;
}

double
Bhv_Savior::getFreeAngleFromPos( const rcsc::WorldModel & wm,
                                 long valid_teammate_threshold,
                                 const rcsc::Vector2D & pos )
{
    const double goal_half_width = rcsc::ServerParam::i().goalHalfWidth();
    const double field_half_length = rcsc::ServerParam::i().pitchHalfLength();

    const rcsc::Vector2D goal_center = rcsc::ServerParam::i().ourTeamGoalPos();
    const rcsc::Vector2D goal_left( goal_center.x, +goal_half_width );
    const rcsc::Vector2D goal_right( goal_center.x, -goal_half_width );

    const rcsc::Vector2D goal_center_left( field_half_length,
                                           (+ goal_half_width - 1.5) / 2.0 );

    const rcsc::Vector2D goal_center_right( field_half_length,
                                            (- goal_half_width + 1.5) / 2.0 );


    double center_angle = getMinimumFreeAngle( wm,
                                               goal_center,
                                               valid_teammate_threshold,
                                               pos );

    double left_angle   = getMinimumFreeAngle( wm,
                                               goal_left,
                                               valid_teammate_threshold,
                                               pos );

    double right_angle  = getMinimumFreeAngle( wm,
                                               goal_right,
                                               valid_teammate_threshold,
                                               pos );

    double center_left_angle  = getMinimumFreeAngle
                                           ( wm,
                                             goal_center_left,
                                             valid_teammate_threshold,
                                             pos );

    double center_right_angle = getMinimumFreeAngle
                                           ( wm,
                                             goal_center_right,
                                             valid_teammate_threshold,
                                             pos );

    return std::max( center_angle,
                     std::max( left_angle,
                               std::max( right_angle,
                                         std::max( center_left_angle,
                                                   center_right_angle ) ) ) );
}


double
Bhv_Savior::getMinimumFreeAngle( const rcsc::WorldModel & wm,
                                 const rcsc::Vector2D & goal,
                                 long valid_teammate_threshold,
                                 const rcsc::Vector2D & pos )
{
    rcsc::AngleDeg test_dir = (goal - pos).th();

    double shoot_course_cone = +360.0;

    rcsc::AbstractPlayerCont team_set;
    team_set = wm.getPlayerCont( new rcsc::AndPlayerPredicate
                                ( new rcsc::TeammatePlayerPredicate
                                      ( wm ),
                                  new rcsc::CoordinateAccuratePlayerPredicate
                                      ( valid_teammate_threshold ) ) );

    rcsc::AbstractPlayerCont::const_iterator t_end = team_set.end();
    for ( rcsc::AbstractPlayerCont::const_iterator t = team_set.begin();
          t != t_end;
          ++t )
    {
        double controllable_dist;

        if ( (*t)->goalie() )
        {
            controllable_dist = rcsc::ServerParam::i().catchAreaLength();
        }
        else
        {
            controllable_dist = (*t)->playerTypePtr()->kickableArea();
        }

        rcsc::Vector2D relative_player = (*t)->pos() - pos;

        double hide_angle_radian = ( std::asin
                                     ( std::min( controllable_dist
                                                 / relative_player.r(),
                                                 1.0 ) ) );
        double angle_diff = std::max( (relative_player.th() - test_dir).abs()
                                      - hide_angle_radian / M_PI * 180,
                                      0.0 );

        if ( shoot_course_cone > angle_diff )
        {
            shoot_course_cone = angle_diff;
        }
    }

    return shoot_course_cone;
}


/*-------------------------------------------------------------------*/
bool
Bhv_Savior::doChaseBall( rcsc::PlayerAgent * agent )
{
#if defined( OLD_CHASE_BALL )
    return Bhv_SaviorChaseBall().execute( agent );
#else
    const int self_reach_cycle = agent->world().interceptTable()->selfReachCycle();
    rcsc::Vector2D intercept_point = agent->world().ball().inertiaPoint( self_reach_cycle );

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": (doChaseBall): "
                        "intercept point = [%f, %f]",
                        intercept_point.x, intercept_point.y );

    if ( intercept_point.x > rcsc::ServerParam::i().ourTeamGoalLineX() - 1.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": (doChaseBall): normal intercept" );

        if ( rcsc::Body_Intercept2008().execute( agent ) )
        {
            agent->setNeckAction( new rcsc_ext::Neck_ChaseBall() );

            return true;
        }
    }

    return false;
#endif
}


bool
Bhv_Savior::isGoalLinePositioningSituation( const rcsc::WorldModel & wm,
                                            const rcsc::Vector2D & ball_pos,
                                            bool penalty_kick_mode )
{
    if ( penalty_kick_mode )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": (isGoalLinePositioningSituation): "
                            "penalty_kick_mode, return false" );
        return false;
    }

    if ( wm.ourDefenseLineX() >= -15.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": (isGoalLinePositioningSituation): "
                            "defense line(%f) too forward, "
                            "no need to goal line positioning",
                            wm.ourDefenseLineX() );
        return false;
    }

    rcsc::AngleDeg ball_dir = getDirFromGoal( ball_pos );

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": (isGoalLinePositioningSituation) "
                        "ball direction from goal = %f",
                        ball_dir.degree() );

#if 0
# if 1
    bool is_side_angle = ( ball_dir.abs() > 30.0 );
# elif 0
    bool is_side_angle = ( ball_dir.abs() > 35.0 );
# elif 1
    bool is_side_angle = ( ball_dir.abs() > 40.0 );
# else
    bool is_side_angle = ( ball_dir.abs() > 45.0 );
# endif
#else
    bool is_side_angle = ( ball_dir.abs() > 50.0 );
#endif

    if ( is_side_angle )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": (isGoalLinePositioningSituation) "
                            "side angle, not goal line positioning" );

        return false;
    }

    if ( wm.ourDefenseLineX() >= -40.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": (isGoalLinePositioningSituation): "
                            "defense line(%f) too back, "
                            "return true",
                            wm.ourDefenseLineX() );
        return true;
    }


    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": (isGoalLinePositioningSituation) "
                        "return false" );
    return true;
}
