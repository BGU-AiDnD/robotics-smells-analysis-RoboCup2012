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

#include "role_Lside_forward.h"

#include "strategy.h"

#include "bhv_cross.h"
#include "bhv_basic_defensive_kick.h"
#include "bhv_basic_offensive_kick.h"
#include "bhv_self_pass.h"

#include "body_kick_to_corner.h"
#include "body_kick_to_front_space.h"

#include "bhv_basic_tackle.h"

#include "bhv_attacker_offensive_move.h"
#include "bhv_basic_move.h"

#include "body_advance_ball_test.h"
#include "body_clear_ball2008.h"
#include "body_dribble2008.h"
#include "body_intercept2008.h"
#include "body_hold_ball2008.h"
#include "body_smart_kick.h"
#include "bhv_pass_test.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_goalie_or_scan.h>
#include <rcsc/action/neck_turn_to_player_or_scan.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>
#include <rcsc/action/view_wide.h>

#include <rcsc/formation/formation.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/circle_2d.h>
#include <rcsc/geom/rect_2d.h>
#include <rcsc/geom/sector_2d.h>
#include <rcsc/math_util.h>

#include <rcsc/geom/vector_2d.h>
#include <rcsc/geom/triangle_2d.h>
#include <rcsc/geom/line_2d.h>

#include "neck_offensive_intercept_neck.h"

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleLSideForward::execute( rcsc::PlayerAgent * agent )
{
    //////////////////////////////////////////////////////////////
    // play_on play
    bool kickable = agent->world().self().isKickable();
    if ( agent->world().existKickableTeammate()
         && agent->world().teammatesFromBall().front()->distFromBall()
         < agent->world().ball().distFromSelf() )
    {
        kickable = false;
    }

    if ( kickable )
    {
        // kick
        doKick( agent );
    }
    else
    {
        // positioning
        doMove( agent );
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleLSideForward::doKick( rcsc::PlayerAgent * agent )
{
    const double nearest_opp_dist
        = agent->world().getDistOpponentNearestToSelf( 5 );

    switch ( Strategy::get_ball_area( agent->world().ball().pos() ) ) {
    case Strategy::BA_CrossBlock:
    case Strategy::BA_Stopper:
        if ( ! rcsc::Bhv_PassTest().execute( agent ) )
        {
            if ( nearest_opp_dist < rcsc::ServerParam::i().tackleDist() )
            {
                Body_AdvanceBallTest().execute( agent );
                agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
            }
            else
            {
                Bhv_BasicDefensiveKick().execute( agent );
            }

            if ( agent->effector().queuedNextBallKickable() )
            {
                agent->setNeckAction( new rcsc::Neck_ScanField() );
            }
            else
            {
                agent->setNeckAction( new rcsc::Neck_TurnToBall() );
            }
        }
        break;
    case Strategy::BA_Danger:
        if ( nearest_opp_dist < rcsc::ServerParam::i().tackleDist() )
        {
            Body_ClearBall2008().execute( agent );
            agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
        }
        else
        {
            Bhv_BasicDefensiveKick().execute( agent );
        }
        break;
    case Strategy::BA_DribbleBlock:
    case Strategy::BA_DefMidField:
        Bhv_BasicOffensiveKick().execute( agent );
        break;
    case Strategy::BA_DribbleAttack:
        doDribbleAttackAreaKick( agent );
        break;
    case Strategy::BA_OffMidField:
        doMiddleAreaKick( agent );
        break;
    case Strategy::BA_Cross:
        doCrossAreaKick( agent );
        break;
    case Strategy::BA_ShootChance:
        doShootAreaKick( agent );
        break;
    default:
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": unknown ball area" );
        rcsc::Body_HoldBall2008().execute( agent );
        break;
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleLSideForward::doMove( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();
    rcsc::Vector2D home_pos = Strategy::i().getPosition( agent->world().self().unum() );
    if ( ! home_pos.isValid() ) home_pos.assign( 0.0, 0.0 );


    /*//tries to have clear sight of the ball

    if(wm.existKickableTeammate() && wm.self().pos().dist(wm.ball().pos()) < 20.0)      //if our team has ball possession
    {
        rcsc::Rect2D clearArea = rcsc::Rect2D(wm.self().pos(),wm.ball().pos());

        if(wm.existOpponentIn(clearArea,1,false)) {         //if there is an opponent
            rcsc::Triangle2D newArea[2] = {rcsc::Triangle2D(wm.ball().pos(),
                                                              rcsc::Vector2D(wm.offsideLineX(),wm.self().pos().y-3),
                                                              rcsc::Vector2D(wm.offsideLineX(),wm.self().pos().y-13)),
                                           rcsc::Triangle2D(wm.ball().pos(),
                                                              rcsc::Vector2D(wm.offsideLineX(),wm.self().pos().y+3),
                                                              rcsc::Vector2D(wm.offsideLineX(),wm.self().pos().y+13)) };

            rcsc::Rect2D upArea = rcsc::Rect2D(rcsc::Vector2D(wm.self().pos().x,wm.self().pos().y+10),wm.ball().pos());
            rcsc::Rect2D downArea = rcsc::Rect2D(rcsc::Vector2D(wm.self().pos().x,wm.self().pos().y-10),wm.ball().pos());

            //vars to doGoToPoint function
            rcsc::Vector2D target_point;
            double dist_thr = wm.ball().distFromSelf() * 0.1;
            dash_power = rcsc::ServerParam::i().maxPower()/3;

            //trying to find clear area
            if(!wm.existOpponentIn(newArea[0],1,false)) {
                target_point.x = wm.offsideLineX();
                target_point.y = wm.self().pos().y-13;
                doGoToPoint(agent, target_point, dist_thr, dash_power, 15.0 );
                agent->setNeckAction( new rcsc::Neck_TurnToBall() );
            }
            else if(!wm.existOpponentIn(newArea[0],1,false)) {
                target_point.x = wm.offsideLineX();
                target_point.y = wm.self().pos().y+13;
                doGoToPoint(agent, target_point, dist_thr, dash_power, 15.0 );
                agent->setNeckAction( new rcsc::Neck_TurnToBall() );
            }
            else if(!wm.existOpponentIn(upArea,1,false)) {
                target_point.x = wm.self().pos().x;
                target_point.y = wm.self().pos().y+13;
                doGoToPoint(agent, target_point, dist_thr, dash_power, 15.0 );
                agent->setNeckAction( new rcsc::Neck_TurnToBall() );
            }
            else if(!wm.existOpponentIn(downArea,1,false)) {
                target_point.x = wm.self().pos().x;
                target_point.y = wm.self().pos().y-13;
                doGoToPoint(agent, target_point, dist_thr, dash_power, 15.0 );
                agent->setNeckAction( new rcsc::Neck_TurnToBall() );
            }
            else {
                //if there's no solution, choose random position and move
                srand(time(NULL));
                int randFactor = rand()%2;
                if(randFactor){
                    target_point.x = wm.offsideLineX();
                    target_point.y = wm.self().pos().y + wm.theirPlayer(1)->pos().y;
                    doGoToPoint(agent, target_point, dist_thr, dash_power, 15.0 );
                    agent->setNeckAction( new rcsc::Neck_TurnToBall() );
                }
                else{
                    target_point.x = wm.offsideLineX();
                    target_point.y = wm.self().pos().y - wm.theirPlayer(1)->pos().y;
                    doGoToPoint(agent, target_point, dist_thr, dash_power, 15.0 );
                    agent->setNeckAction( new rcsc::Neck_TurnToBall() );
                }
            }


        }
    }*/

    switch ( Strategy::get_ball_area( agent->world() ) ) {
    case Strategy::BA_CrossBlock:
    case Strategy::BA_Stopper:
    case Strategy::BA_Danger:
        Bhv_BasicMove( home_pos ).execute( agent );
        break;
    case Strategy::BA_DribbleBlock:
    case Strategy::BA_DefMidField:
        Bhv_AttackerOffensiveMove( home_pos, true ).execute( agent );
        break;
    case Strategy::BA_DribbleAttack:
    case Strategy::BA_OffMidField:
        Bhv_AttackerOffensiveMove( home_pos, true ).execute( agent );
        break;
    case Strategy::BA_Cross: // x>39, y>17
    case Strategy::BA_ShootChance: // x>39, y<17
        doShootAreaMove( agent, home_pos );
        break;
    default:
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": unknown ball area" );
        Bhv_BasicMove( home_pos ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
        break;
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleLSideForward::doDribbleAttackAreaKick( rcsc::PlayerAgent* agent )
{
    // !!!check stamina!!!
    static bool S_recover_mode = false;

    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doDribbleAttackAreaKick" );

    const rcsc::WorldModel & wm = agent->world();

    if ( wm.self().stamina()
         < rcsc::ServerParam::i().recoverDecThrValue() + 200.0 )
    {
        S_recover_mode = true;
    }
    else if ( wm.self().stamina()
              > rcsc::ServerParam::i().staminaMax() * 0.6 )
    {
        S_recover_mode = false;
    }

    const rcsc::PlayerObject * nearest_opp = wm.getOpponentNearestToSelf( 10 );
    const double nearest_opp_dist = ( nearest_opp
                                      ? nearest_opp->distFromSelf()
                                      : 1000.0 );
    rcsc::Vector2D nearest_opp_pos( -1000.0, 0.0 );
    if ( nearest_opp ) nearest_opp_pos = nearest_opp->pos();

    const rcsc::Bhv_PassTest::PassRoute * pass
        = rcsc::Bhv_PassTest::get_best_pass( wm );

    if ( pass )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": exist pass point" );
        double opp_to_pass_dist = 100.0;
        wm.getOpponentNearestTo( pass->receive_point_, 10, &opp_to_pass_dist );

        if ( pass->receive_point_.x > 35.0
             && opp_to_pass_dist > 10.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": very challenging pass" );
            rcsc::Bhv_PassTest().execute( agent );
            return;
        }


        if ( nearest_opp
             && nearest_opp_pos.x > wm.self().pos().x - 1.0
             && nearest_opp->angleFromSelf().abs() < 90.0
             && nearest_opp_dist < 5.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": dribble attack. pass to avoid front opp" );
            rcsc::Bhv_PassTest().execute( agent );
            return;
        }

        if ( nearest_opp_pos.x > wm.self().pos().x - 0.5
             && nearest_opp_dist < 5.8
             && pass->receive_point_.x > wm.self().pos().x + 3.0
             )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": dribble attack. keep away & attack pass" );
            rcsc::Bhv_PassTest().execute( agent );
            return;
        }

        if ( nearest_opp_pos.x > wm.self().pos().x - 1.0
             && nearest_opp_dist < 5.0
             && ( pass->receive_point_.x > 20.0
                  || pass->receive_point_.x > wm.self().pos().x - 3.0 )
             )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": dribble attack(2). keep away & attack pass" );
            rcsc::Bhv_PassTest().execute( agent );
            return;
        }

        if ( S_recover_mode
             && ( pass->receive_point_.x > 30.0
                  || pass->receive_point_.x > wm.self().pos().x - 3.0 )
             )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": dribble attack. pass for stamina recover" );
            rcsc::Bhv_PassTest().execute( agent );
            return;
        }

        //if ( pass_point.x > wm.self().pos().x )
        if ( pass->receive_point_.x > wm.self().pos().x - 3.0
             && opp_to_pass_dist > 8.0 )
        {
            const rcsc::Sector2D sector( wm.self().pos(),
                                         0.5, 8.0,
                                         -30.0, 30.0 );
            // opponent check with goalie
            if ( wm.existOpponentIn( sector, 10, true ) )
            {
                rcsc::dlog.addText( rcsc::Logger::ROLE,
                                    __FILE__": dribble attack. forward pass to avoid opp" );
                rcsc::Bhv_PassTest().execute( agent );
                return;
            }
        }
    }

#if 0
    for ( int dash_step = 16; dash_step >= 6; dash_step -= 2 )
    {
        if ( doSelfPass( agent, dash_step ) )
        {
            return;
        }
    }
#else
    if ( Bhv_SelfPass().execute( agent ) )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doDribbleAttackAreaKick SelfPass" );
        return;
    }
#endif

    if ( doStraightDribble( agent ) )
    {
        return;
    }

    // recover mode
    if ( S_recover_mode )
    {
        if ( nearest_opp_dist > 5.0 )
        {
            rcsc::Body_HoldBall2008( true, rcsc::Vector2D(36.0, wm.self().pos().y * 0.9 )//0.0)
                                     ).execute( agent );
            agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
            return;
        }
        else if ( rcsc::Bhv_PassTest().execute( agent ) )
        {
            return;
        }
        else
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": dribble attack. exhaust kick to corner" );
            Body_KickToCorner( ( wm.self().pos().y < 0.0 )
                               ).execute( agent );
            agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
            return;
        }
        return;
    }

    if ( nearest_opp_dist < rcsc::ServerParam::i().defaultPlayerSize() * 2.0 + 0.2
         || wm.existKickableOpponent() )
    {
        if ( rcsc::Bhv_PassTest().execute( agent ) )
        {
            return;
        }

        if ( wm.self().pos().x < 25.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": dribble attack. kick to near side" );
            Body_KickToCorner( ( wm.self().pos().y < 0.0 )
                               ).execute( agent );
        }
        else if ( wm.self().pos().x < 35.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": dribble attack. kick to space for keep away" );
            Body_KickToFrontSpace().execute( agent );
        }
        else
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": dribble attack. opponent very near. cross to center." );
            Bhv_Cross().execute( agent );
            return;
        }
        agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
        return;
    }


    if ( nearest_opp
         && wm.self().pos().x > 20.0
         && nearest_opp->distFromSelf() < 1.0
         && nearest_opp->pos().x > wm.self().pos().x
         && std::fabs( nearest_opp->pos().y - wm.self().pos().y ) < 0.9 )
    {
        Body_KickToCorner( ( wm.self().pos().y < 0.0 )
                           ).execute( agent );

        agent->setNeckAction( new rcsc::Neck_ScanField() );
        agent->debugClient().addMessage( "SF:BaseKickAvoidOpp" );
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": dribble attack. kick to near side to avoid opponent" );
        return;
    }

    // wing dribble
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doDribbleAttackAreaKick. dribble attack. normal dribble" );
    doSideForwardDribble( agent );
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
RoleLSideForward::doStraightDribble( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();

    // dribble straight
    if ( wm.self().pos().x > 36.0 )
    {
        return false;
    }

    if ( wm.self().pos().absY() > rcsc::ServerParam::i().pitchHalfWidth() - 2.0 )
    {
        if ( wm.self().pos().y < 0.0 && wm.self().body().degree() < 1.5 )
        {
            return false;
        }
        if ( wm.self().pos().y > 0.0 && wm.self().body().degree() > -1.5 )
        {
            return false;
        }
    }

    const rcsc::PlayerObject * nearest_opp = wm.getOpponentNearestToSelf( 10 );
    if ( nearest_opp
         && nearest_opp->pos().x > wm.self().pos().x + 0.5
         && std::fabs( nearest_opp->pos().y -  wm.self().pos().y ) < 2.0
         && nearest_opp->distFromSelf() < 2.0
         )
    {
        return false;
    }

    const rcsc::Rect2D target_rect
        = rcsc::Rect2D::from_center( wm.ball().pos().x + 5.5,
                                     wm.ball().pos().y,
                                     10.0, 9.0 );
    if ( wm.existOpponentIn( target_rect, 10, false ) )
    {
        return false;
    }

    const rcsc::Rect2D safety_rect
        = rcsc::Rect2D::from_center( wm.ball().pos().x + 6.5,
                                     wm.ball().pos().y,
                                     13.0, 13.0 );
    //int dash_count = 3;
    int dash_count = 10;
    if ( wm.existOpponentIn( safety_rect, 10, false ) )
    {
        //dash_count = 1;
        dash_count = 8;
    }

    rcsc::Vector2D drib_target( 47.0, wm.self().pos().y );
    if ( wm.self().body().abs() < 20.0 )
    {
        drib_target
            = wm.self().pos()
            + rcsc::Vector2D::polar2vector( 10.0, wm.self().body() );
    }

    agent->debugClient().addMessage( "StraightDrib" );
    agent->debugClient().addRectangle( target_rect );

    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": dribble straight to (%.1f %.1f) dash_count = %d",
                        drib_target.x, drib_target.y,
                        dash_count );
    rcsc::Body_Dribble2008( drib_target,
                            1.0,
                            dash_power,
                            dash_count,
                            false // no dodge
                            ).execute( agent );

    const rcsc::PlayerObject * opponent = static_cast< rcsc::PlayerObject * >( 0 );

    const rcsc::PlayerPtrCont::const_iterator end = wm.opponentsFromSelf().end();
    for ( rcsc::PlayerPtrCont::const_iterator it = wm.opponentsFromSelf().begin();
          it != end;
          ++it )
    {
        if ( (*it)->distFromSelf() > 10.0 ) break;
        if ( (*it)->posCount() > 5 ) continue;
        if ( (*it)->posCount() == 0 ) continue;
        if ( (*it)->angleFromSelf().abs() > 90.0 ) continue;
        if ( wm.dirCount( (*it)->angleFromSelf() ) < (*it)->posCount() ) continue;

        // TODO: check the next visible range

        opponent = *it;
        break;
    }

    if ( opponent )
    {
        agent->debugClient().addMessage( "LookOpp" );
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": dribble straight look opp %d (%.1f %.1f)",
                            opponent->unum(),
                            opponent->pos().x, opponent->pos().y );
        agent->setNeckAction( new rcsc::Neck_TurnToPoint( opponent->pos() ) );
    }
    else
    {
        agent->setNeckAction( new rcsc::Neck_ScanField() );
    }
    return true;

}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleLSideForward::doSideForwardDribble( rcsc::PlayerAgent * agent )
{
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doSideForwardDribble" );

    const rcsc::Vector2D drib_target = getDribbleTarget( agent );
    rcsc::AngleDeg target_angle = ( drib_target - agent->world().self().pos() ).th();

    // decide dash count & looking point
    int dash_count = getDribbleAttackDashStep( agent, target_angle );

    bool dodge = true;
    if ( agent->world().self().pos().x > 43.0
         && agent->world().self().pos().absY() < 20.0 )
    {
        dodge = false;
    }

    agent->debugClient().setTarget( drib_target );
    agent->debugClient().addMessage( "DribAtt%d", dash_count );

    rcsc::Body_Dribble2008( drib_target,
                            1.0,
                            dash_power,
                            dash_count,
                            dodge
                            ).execute( agent );

    if ( agent->world().dirCount( target_angle ) > 1 )
    {
        target_angle
            = ( drib_target - agent->effector().queuedNextMyPos() ).th();
        rcsc::AngleDeg next_body = agent->effector().queuedNextMyBody();
        if ( ( target_angle - next_body ).abs() < 90.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": SB Dribble LookDribTarget" );
            agent->debugClient().addMessage( "LookDribTarget" );
            agent->setNeckAction( new rcsc::Neck_TurnToPoint( drib_target ) );
            return;
        }
    }

    agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
}


/*-------------------------------------------------------------------*/
/*!

*/
bool
RoleLSideForward::doSelfPass( rcsc::PlayerAgent * agent,
                             const int max_dash_step )
{
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doSelfPass" );

    const rcsc::WorldModel & wm = agent->world();

    if ( wm.self().pos().x < 0.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": in our area" );
        return false;
    }

    if ( wm.self().body().abs() > 10.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": body angle is invalid" );
        return false;
    }

    rcsc::Vector2D receive_pos( rcsc::Vector2D::INVALIDATED );

    //const double dash_power = dash_power;
    //const int max_dash_step = 16;

    int dash_step = max_dash_step;
    {
        double my_effort = wm.self().effort();
        double my_recovery = wm.self().recovery();
        // next cycles my stamina
        double my_stamina
            = wm.self().stamina()
            + ( wm.self().playerType().staminaIncMax()
                * wm.self().recovery() );

        rcsc::Vector2D my_pos = wm.self().pos();
        rcsc::Vector2D my_vel = wm.self().vel();
        rcsc::AngleDeg accel_angle = wm.self().body();

        my_pos += my_vel;

        for ( int i = 0; i < max_dash_step; ++i )
        {
            if ( my_stamina
                 < rcsc::ServerParam::i().recoverDecThrValue() + 400.0 )
            {
                dash_step = i;
                break;
            }

            double available_stamina
                =  std::max( 0.0,
                             my_stamina
                             - rcsc::ServerParam::i().recoverDecThrValue()
                             - 300.0 );
            double consumed_stamina = dash_power;
            consumed_stamina = std::min( available_stamina,
                                         consumed_stamina );
            double used_power = consumed_stamina;
            double max_accel_mag = ( std::fabs( used_power )
                                     * wm.self().playerType().dashPowerRate()
                                     * my_effort );
            double accel_mag = max_accel_mag;
            if ( wm.self().playerType().normalizeAccel( my_vel,
                                                        accel_angle,
                                                        &accel_mag ) )
            {
                used_power *= accel_mag / max_accel_mag;
            }

            rcsc::Vector2D dash_accel
                = rcsc::Vector2D::polar2vector( std::fabs( used_power )
                                                * my_effort
                                                * wm.self().playerType().dashPowerRate(),
                                                accel_angle );
            my_vel += dash_accel;
            my_pos += my_vel;

            my_vel *= wm.self().playerType().playerDecay();

            wm.self().playerType().getOneStepStaminaComsumption();
        }

        if ( dash_step <= 5 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": too short dash step %d",
                                dash_step );
            return false;
        }

        receive_pos = my_pos;
    }

    if ( receive_pos.x > rcsc::ServerParam::i().pitchHalfLength() - 5.0
         || receive_pos.absY() > rcsc::ServerParam::i().pitchHalfWidth() - 2.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": selfPass receive pos(%.1f %.1f) is near goal line",
                            receive_pos.x, receive_pos.y );
        return false;
    }

    const rcsc::PlayerPtrCont::const_iterator end = wm.opponentsFromSelf().end();
    for ( rcsc::PlayerPtrCont::const_iterator it = wm.opponentsFromSelf().begin();
          it != end;
          ++it )
    {
        if ( (*it)->posCount() >= 15 ) continue;

        if ( (*it)->pos().dist( receive_pos )
             < rcsc::ServerParam::i().defaultPlayerSpeedMax()
             * ( dash_step + std::min( 10, (*it)->posCount() ) )
             + 1.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": selfPass: opponent can reach faster then self."
                                " step=%d (%.1f %.1f)",
                                dash_step, (*it)->pos().x, (*it)->pos().y );
            return false;
        }
    }

    double first_speed = rcsc::calc_first_term_geom_series( wm.ball().pos().dist( receive_pos ),
                                                            rcsc::ServerParam::i().ballDecay(),
                                                            dash_step + 1 + 1 ); // kick + buffer

    rcsc::AngleDeg target_angle = ( receive_pos - wm.ball().pos() ).th();
    rcsc::Vector2D max_vel
        = rcsc::Body_KickOneStep::get_max_possible_vel( target_angle,
                                                        wm.self().kickRate(),
                                                        wm.ball().vel() );
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": selfPass first speed = %.2f",
                        first_speed );
    if ( max_vel.r() < first_speed )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": selfPass cannot kick by one step. max_speed= %.2f",
                            max_vel.r() );
        return false;
    }

    rcsc::Body_KickOneStep( receive_pos,
                            first_speed ).execute( agent );
    agent->setViewAction( new rcsc::View_Wide );
    agent->setNeckAction( new rcsc::Neck_TurnToBall );

    agent->debugClient().addMessage( "SelfPass" );
    agent->debugClient().setTarget( receive_pos );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
rcsc::Vector2D
RoleLSideForward::getDribbleTarget( rcsc::PlayerAgent * agent )
{
    const double base_x = 47.5; //50.0; // old 45.0
    const double second_x = 40.0;
    const rcsc::WorldModel & wm = agent->world();

    rcsc::Vector2D drib_target( rcsc::Vector2D::INVALIDATED );

    bool goalie_near = false;
    //--------------------------------------------------
    // goalie check
    const rcsc::PlayerObject * opp_goalie = agent->world().getOpponentGoalie();
    if ( opp_goalie
         && opp_goalie->pos().x > rcsc::ServerParam::i().theirPenaltyAreaLineX()
         && opp_goalie->pos().absY() < rcsc::ServerParam::i().penaltyAreaHalfWidth()
         && ( opp_goalie->distFromSelf()
              < rcsc::ServerParam::i().catchAreaLength()
              + rcsc::ServerParam::i().defaultPlayerSpeedMax() * 3.5 )
         )
    {
        goalie_near = true;
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": dribble. goalie close" );
    }

    //--------------------------------------------------
    // goalie is close
    if ( goalie_near
         && std::fabs( opp_goalie->pos().y - wm.self().pos().y ) < 3.0 )
    {
        drib_target.assign( base_x, 25.0 );
        if ( wm.self().pos().y < 0.0 ) drib_target.y *= -1.0;

        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": getDribbleTarget. goalie colse. normal target" );

        return drib_target;
    }

    // goalie is safety
    const rcsc::PlayerObject * nearest_opp = wm.getOpponentNearestToSelf( 6 );

    if ( nearest_opp
         && nearest_opp->distFromSelf() < 2.0
         && wm.self().body().abs() < 40.0 )
    {
        rcsc::Vector2D tmp_target
            = wm.self().pos()
            + rcsc::Vector2D::polar2vector( 5.0, wm.self().body() );
        if ( tmp_target.x < 50.0 )
        {
            rcsc::Sector2D body_sector( wm.self().pos(),
                                        0.5, 10.0,
                                        wm.self().body() - 30.0,
                                        wm.self().body() + 30.0 );
            if ( ! wm.existOpponentIn( body_sector, 10, true ) )
            {
                rcsc::dlog.addText( rcsc::Logger::ROLE,
                                    __FILE__": getDribbleTarget. opp near. dribble to my body dir" );
                return tmp_target;
            }
        }
    }


    // cross area
    if ( wm.self().pos().x > second_x )
    {
        rcsc::Vector2D rect_center( ( wm.self().pos().x + base_x ) * 0.5,
                                    wm.self().pos().y < 0.0
                                    ? wm.self().pos().y + 4.0
                                    : wm.self().pos().y - 4.0 );
        rcsc::Rect2D side_rect
            = rcsc::Rect2D::from_center( rect_center, 6.0, 8.0 );
        agent->debugClient().addRectangle( side_rect );

        if ( wm.countOpponentsIn( side_rect, 10, false ) <= 1 )
        {
            drib_target.assign( base_x, 12.0 );
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": getDribbleTarget. cut in (%.1f, %.1f)",
                                drib_target.x, drib_target.y );
        }
        else
        {
            drib_target.assign( second_x,
                                rcsc::min_max( 15.0,
                                               wm.self().pos().absY() - 1.0,
                                               25.0 ) );
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": getDribbleTarget. keep away (%.1f, %.1f)",
                                drib_target.x, drib_target.y );
        }

        if ( Strategy::i().getPositionType( wm.self().unum() ) == Position_Left )
        {
            drib_target.y *= -1.0;
        }


        if ( opp_goalie )
        {
            rcsc::Vector2D drib_rel = drib_target - wm.self().pos();
            rcsc::AngleDeg drib_angle = drib_rel.th();
            rcsc::Line2D drib_line( wm.self().pos(), drib_target );
            if ( ( opp_goalie->angleFromSelf() - drib_angle ).abs() < 90.0
                 && drib_line.dist( opp_goalie->pos() ) < 4.0
                 )
            {
                double drib_dist = std::max( 0.0, opp_goalie->distFromSelf() - 5.0 );
                drib_target
                    = wm.self().pos()
                    + drib_rel.setLengthVector( drib_dist );

                rcsc::dlog.addText( rcsc::Logger::ROLE,
                                    __FILE__": getDribbleTarget. attend goalie. dist=%.2f (%.1f, %.1f)",
                                    drib_dist,
                                    drib_target.x, drib_target.y );
            }
        }

        return drib_target;
    }

    if ( wm.self().pos().x > 36.0 )
    {
        if ( wm.self().pos().absY() < 5.0 )
        {
            drib_target.assign( base_x, 22.0 );
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": getDribbleTarget. keep away to (%.1f, %.1f)",
                                drib_target.x, drib_target.y );
        }
        else if ( wm.self().pos().absY() > 19.0 )
        {
            drib_target.assign( base_x, 12.0 );
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": getDribbleTarget. go their penalty area (%.1f, %.1f)",
                                drib_target.x, drib_target.y );
        }
        else
        {
            drib_target.assign( base_x, wm.self().pos().absY() );
            if ( drib_target.y > 30.0 ) drib_target.y = 30.0;
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": getDribbleTarget. go my Y (%.1f, %.1f)",
                                drib_target.x, drib_target.y );
        }

        if ( Strategy::i().getPositionType( wm.self().unum() ) == Position_Left )
        {
            drib_target.y *= -1.0;
        }

        return drib_target;
    }

    drib_target.assign( base_x, wm.self().pos().absY() );
    if ( drib_target.y > 30.0 ) drib_target.y = 30.0;

    if ( Strategy::i().getPositionType( wm.self().unum() ) == Position_Left )
    {
        drib_target.y *= -1.0;
    }

    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": getDribbleTarget. normal target (%.1f, %.1f)",
                        drib_target.x, drib_target.y );

    return drib_target;
}

/*-------------------------------------------------------------------*/
/*!

*/
int
RoleLSideForward::getDribbleAttackDashStep( const rcsc::PlayerAgent * agent,
                                           const rcsc::AngleDeg & target_angle )
{
    bool goalie_near = false;
    //--------------------------------------------------
    // goalie check
    const rcsc::PlayerObject * opp_goalie = agent->world().getOpponentGoalie();
    if ( opp_goalie
         && opp_goalie->pos().x > rcsc::ServerParam::i().theirPenaltyAreaLineX()
         && opp_goalie->pos().absY() < rcsc::ServerParam::i().penaltyAreaHalfWidth()
         && ( opp_goalie->distFromSelf()
              < rcsc::ServerParam::i().catchAreaLength()
              + rcsc::ServerParam::i().defaultPlayerSpeedMax() * 3.5 )
         )
    {
        goalie_near = true;
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": dribble. goalie close" );
    }

    const rcsc::Sector2D target_sector( agent->world().self().pos(),
                                        0.5, 10.0,
                                        target_angle - 30.0,
                                        target_angle + 30.0 );
    const int default_step = 10;
    double dash_dist = 4.0;
    try {
        dash_dist = agent->world().self().playerType()
            .dashDistanceTable().at( default_step );
        dash_dist += agent->world().self().playerType()
            .inertiaTravel( agent->world().self().vel(), default_step ).r();
    }
    catch ( ... )
    {
        std::cerr << __FILE__ << ": " << __LINE__
                  << " Exception caught" << std::endl;
    }

    rcsc::Vector2D next_point
        = rcsc::Vector2D::polar2vector( dash_dist, target_angle );
    next_point += agent->world().self().pos();
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": getDribbleAttackDashStep. my_dash_dsit= %.2f  point(%.1f %.1f)",
                        dash_dist, next_point.x, next_point.y  );

    const rcsc::PlayerPtrCont & opps = agent->world().opponentsFromSelf();
    const rcsc::PlayerPtrCont::const_iterator opps_end = opps.end();
    bool exist_opp = false;
    for ( rcsc::PlayerPtrCont::const_iterator it = opps.begin();
          it != opps_end;
          ++it )
    {
        if ( (*it)->posCount() >= 10 )
        {
            continue;
        }
        if ( ( (*it)->angleFromSelf() - target_angle ).abs() > 120.0 )
        {
            continue;
        }

        if ( (*it)->distFromSelf() > 10.0 )
        {
            break;
        }
        if ( target_sector.contains( (*it)->pos() ) )
        {
            exist_opp = true;
            break;
        }
    }

    if ( exist_opp )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": getDribbleAttackDashStep. exist opp" );
        return 6;
    }

    if ( agent->world().self().pos().x > 36.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": getDribbleAttackDashStep. x > 36" );
        return 8;
        //return 3;
        //return 2;
    }

    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": getDribbleAttackDashStep. default" );
    return 10;
    //return 3;
    //return 2;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleLSideForward::doMiddleAreaKick( rcsc::PlayerAgent * agent )
{
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doMiddleAreaKick" );

    const rcsc::WorldModel & wm = agent->world();

#if 0
    {
        for ( int dash_step = 16; dash_step >= 6; dash_step -= 2 )
        {
            if ( doSelfPass( agent, dash_step ) )
            {
                rcsc::dlog.addText( rcsc::Logger::ROLE,
                                    __FILE__": doMiddleAreaKick SelfPass" );
                return;
            }
        }
    }
#else
    if ( Bhv_SelfPass().execute( agent ) )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doMiddleAreaKick SelfPass" );
        return;
    }
#endif

    const rcsc::PlayerObject * nearest_opp = wm.getOpponentNearestToSelf( 5 );

    const double nearest_opp_dist = ( nearest_opp
                                      ? nearest_opp->distFromSelf()
                                      : 1000.0 );
    const rcsc::Vector2D nearest_opp_pos = ( nearest_opp
                                             ? nearest_opp->pos()
                                             : rcsc::Vector2D( -1000.0, 0.0 ) );

    const rcsc::Bhv_PassTest::PassRoute * pass
        = rcsc::Bhv_PassTest::get_best_pass( wm );

    if ( pass
         && pass->receive_point_.x > wm.self().pos().x + 1.5 )
    {
        double opp_dist = 200.0;
        wm.getOpponentNearestTo( pass->receive_point_, 10, &opp_dist );
        if ( opp_dist > 5.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": first pass" );
            agent->debugClient().addMessage( "MidPass(1)" );
            rcsc::Bhv_PassTest().execute( agent );
            return;
        }
    }

    // passable & opp near
    if ( nearest_opp_dist < 3.0 )
    {
        if ( rcsc::Bhv_PassTest().execute( agent ) )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": pass" );
            agent->debugClient().addMessage( "MidPass(2)" );
            return;
        }
    }

    rcsc::Vector2D drib_target;

    if ( nearest_opp_dist >
         rcsc::ServerParam::i().tackleDist() + rcsc::ServerParam::i().defaultPlayerSpeedMax() )
    {
        // dribble to my body dir
        rcsc::AngleDeg ang_l, ang_r;
        if ( wm.self().pos().y > 0.0 )
        {
            ang_l = ( rcsc::Vector2D( 52.5, -7.0 ) - wm.self().pos() ).th();
            ang_r = ( rcsc::Vector2D( 52.5, 38.0 ) - wm.self().pos() ).th();
        }
        else
        {
            ang_l = ( rcsc::Vector2D( 52.5, -38.0 ) - wm.self().pos() ).th();
            ang_r = ( rcsc::Vector2D( 52.5, 7.0 ) - wm.self().pos() ).th();
        }

        if ( wm.self().body().isWithin( ang_l, ang_r ) )
        {
            drib_target
                = wm.self().pos()
                + rcsc::Vector2D::polar2vector( 5.0, wm.self().body() );

            int max_dir_count = 0;
            wm.dirRangeCount( wm.self().body(), 20.0, &max_dir_count, NULL, NULL );

            if ( drib_target.absX() < rcsc::ServerParam::i().pitchHalfLength() - 1.0
                 && drib_target.absY() < rcsc::ServerParam::i().pitchHalfWidth() - 1.0
                 && max_dir_count <= 5 )
            {
                const rcsc::Sector2D sector(wm.self().pos(), 0.5, 10.0,
                                            wm.self().body() - 30.0,
                                            wm.self().body() + 30.0);
                // oponent check with goalie
                if ( ! wm.existOpponentIn( sector, 10, true ) )
                {
                    rcsc::dlog.addText( rcsc::Logger::ROLE,
                                        __FILE__": dribble to may body dir" );
                    agent->debugClient().addMessage( "MidDribToBody(1)" );
                    rcsc::Body_Dribble2008( drib_target,
                                            1.0,
                                            dash_power,
                                            10 //2
                                            ).execute( agent );
                    agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
                    return;
                }
            }
        }

    }

    drib_target.x = 50.0;
    drib_target.y = ( wm.self().pos().y > 0.0
                      ? 7.0
                      : -7.0 );
    const rcsc::AngleDeg target_angle = (drib_target - wm.self().pos()).th();
    const rcsc::Sector2D sector( wm.self().pos(),
                                 0.5, 15.0,
                                 target_angle - 30.0,
                                 target_angle + 30.0 );
    // opp's is behind of me
    if ( nearest_opp_pos.x < wm.self().pos().x + 1.0
         && nearest_opp_dist > 2.0 )
    {
        // oponent check with goalie
        if ( ! wm.existOpponentIn( sector, 10, true ) )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": opp behind. dribble 3 dashes" );
            agent->debugClient().addMessage( "MidDrib3D(1)" );
            rcsc::Body_Dribble2008( drib_target,
                                    1.0,
                                    dash_power,
                                    10 //3
                                    ).execute( agent );
        }
        else
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": opp behind. dribble 1 dash" );
            agent->debugClient().addMessage( "MidDrib1D(1)" );
            rcsc::Body_Dribble2008( drib_target,
                                    1.0,
                                    dash_power,
                                    10 //1
                                    ).execute( agent );
        }
        agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
        return;
    }


    // opp is far from me
    if ( nearest_opp_dist > 8.0 )
    {
        // oponent check with goalie
        if ( ! wm.existOpponentIn( sector, 10, true ) )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": opp far. dribble 3 dashes" );
            agent->debugClient().addMessage( "MidDrib3D(2)" );
            rcsc::Body_Dribble2008( drib_target,
                                    1.0,
                                    dash_power,
                                    10 //3
                                    ).execute( agent );
        }
        else
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": opp far. dribble 1 dash" );
            agent->debugClient().addMessage( "MidDrib1D(2)" );
            rcsc::Body_Dribble2008( drib_target,
                                    1.0,
                                    dash_power,
                                    10 //1
                                    ).execute( agent );
        }
        agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
        return;
    }

    // opponent is close

    if ( nearest_opp_dist > 5.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": opp middle close. dribble" );
        agent->debugClient().addMessage( "MidDribSlow" );
        rcsc::Body_Dribble2008( drib_target,
                                1.0,
                                dash_power,
                                10 //1
                                ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
        return;
    }

    // exist close opponent
    // pass
    if ( rcsc::Bhv_PassTest().execute( agent ) )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": opp close. pass" );
        agent->debugClient().addMessage( "MidPass" );
        return;
    }

    // dribble to my body dir
    if ( nearest_opp_dist > rcsc::ServerParam::i().tackleDist()
         || nearest_opp_pos.x < wm.self().pos().x + 1.0 )
    {
        // dribble to my body dir
        if ( wm.self().body().abs() < 100.0 )
        {
            const rcsc::Vector2D body_dir_drib_target
                = wm.self().pos()
                + rcsc::Vector2D::polar2vector(5.0, wm.self().body());
            int max_dir_count = 0;
            wm.dirRangeCount( wm.self().body(), 20.90, &max_dir_count, NULL, NULL );

            if ( max_dir_count <= 5
                 && body_dir_drib_target.x < rcsc::ServerParam::i().pitchHalfLength() - 1.0
                 && body_dir_drib_target.absY() < rcsc::ServerParam::i().pitchHalfWidth() - 1.0
                 )
            {
                // check opponents
                // 10m, +-30 degree
                const rcsc::Sector2D body_dir_sector( wm.self().pos(),
                                                      0.5, 10.0,
                                                      wm.self().body() - 30.0,
                                                      wm.self().body() + 30.0);
                // oponent check with goalie
                if ( ! wm.existOpponentIn( sector, 10, true ) )
                {
                    rcsc::dlog.addText( rcsc::Logger::ROLE,
                                        __FILE__": opp close. dribble to my body" );
                    agent->debugClient().addMessage( "MidDribToBody(2)" );
                    rcsc::Body_Dribble2008( body_dir_drib_target,
                                            1.0,
                                            dash_power,
                                            10 //2
                                            ).execute( agent );
                    agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
                    return;
                }
            }
        }
    }

    if ( wm.self().pos().x > wm.offsideLineX() - 5.0 )
    {
        // kick to corner
        rcsc::Vector2D corner_pos( rcsc::ServerParam::i().pitchHalfLength() - 8.0,
                                   rcsc::ServerParam::i().pitchHalfWidth() - 8.0 );
        if ( wm.self().pos().y < 0.0 )
        {
            corner_pos.y *= -1.0;
        }

        // near side
        if ( wm.self().pos().x < 25.0
             || wm.self().pos().absY() > 18.0 )
        {
            const rcsc::Sector2D front_sector( wm.self().pos(),
                                               0.5, 10.0,
                                               -30.0, 30.0 );
            // oponent check with goalie
            if ( ! wm.existOpponentIn( front_sector, 10, true ) )
            {
                rcsc::dlog.addText( rcsc::Logger::ROLE,
                                    __FILE__": kick to corner" );
                agent->debugClient().addMessage( "MidToCorner(1)" );
                Body_KickToCorner( (wm.self().pos().y < 0.0) ).execute( agent );
                agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
                return;
            }
        }

        const rcsc::AngleDeg corner_angle = (corner_pos - wm.self().pos()).th();
        const rcsc::Sector2D corner_sector( wm.self().pos(),
                                            0.5, 10.0,
                                            corner_angle - 15.0,
                                            corner_angle + 15.0 );
        // oponent check with goalie
        if ( ! wm.existOpponentIn( corner_sector, 10, true ) )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": kick to near side" );
            agent->debugClient().addMessage( "MidToCorner(2)" );
            Body_KickToCorner( ( wm.self().pos().y < 0.0 )
                               ).execute( agent );
            agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
            return;
        }
    }

    agent->debugClient().addMessage( "MidHold" );
    rcsc::Body_HoldBall2008().execute( agent );
    agent->setNeckAction( new rcsc::Neck_ScanField() );
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleLSideForward::doCrossAreaKick( rcsc::PlayerAgent* agent )
{
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doCrossAreaKick" );

    const rcsc::WorldModel& wm = agent->world();

    //-------------------------------------------------------
    // !!!check stamina!!!
    static bool S_recover_mode = false;
    if ( wm.self().stamina()
         < rcsc::ServerParam::i().recoverDecThrValue() + 200.0 )
    {
        S_recover_mode = true;
    }
    else if ( wm.self().stamina()
              > rcsc::ServerParam::i().staminaMax() * 0.6 )
    {
        S_recover_mode = false;
    }


    const rcsc::Vector2D next_self_pos = wm.self().pos() + wm.self().vel();

    //-------------------------------------------------------
    // check opponent fielder & goalie distance
    const rcsc::PlayerObject * nearest_opp = wm.getOpponentNearestToSelf( 10 );
    const double opp_dist
        = ( nearest_opp
            ? nearest_opp->distFromSelf()
            : 1000.0 );
//         = ( wm.opponentsFromSelf().empty()
//             ? 1000.0
//             : wm.opponentsFromSelf().front()->distFromSelf() );
    const rcsc::Vector2D opp_pos
        = ( nearest_opp
            ? nearest_opp->pos() + nearest_opp->vel()
            : rcsc::Vector2D( -1000.0, 0.0 ) );

    const rcsc::PlayerObject * opp_goalie = wm.getOpponentGoalie();
    bool goalie_near = false;
    bool goalie_verynear = false;
    if ( opp_goalie )
    {
        if ( opp_goalie->pos().x > rcsc::ServerParam::i().theirPenaltyAreaLineX()
             && opp_goalie->pos().absY() < rcsc::ServerParam::i().penaltyAreaHalfWidth() )
        {
            if ( opp_goalie->distFromBall()
                 < ( rcsc::ServerParam::i().catchAreaLength()
                     + rcsc::ServerParam::i().defaultPlayerSpeedMax() * 4.0 ) )
            {
                goalie_near = true;
            }
            if ( opp_goalie->distFromBall()
                 < ( rcsc::ServerParam::i().catchAreaLength()
                     + rcsc::ServerParam::i().defaultPlayerSpeedMax() * 2.0 ) )
            {
                goalie_verynear = true;
            }
        }
    }

    //-------------------------------------------------------
    if ( goalie_verynear )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": goalie very near. keep away from goalie " );
        // kick to the near side corner
        agent->debugClient().addMessage( "XAreaGKNear" );
        Body_KickToCorner( (wm.self().pos().y < 0.0)
                           ).execute( agent );
        agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
        return;
    }

    //-------------------------------------------------------

    const rcsc::Bhv_PassTest::PassRoute * pass
        = rcsc::Bhv_PassTest::get_best_pass( wm );

    bool receiver_free
        = ( pass
            && ! wm.existOpponentIn( rcsc::Circle2D( pass->receiver_->pos(), 5.0 ),
                                     10,
                                     true ) );

    if ( pass
         && pass->receive_point_.x > 37.0
         && receiver_free
         && pass->receive_point_.absY() < 12.0
         && ! wm.existOpponentIn( rcsc::Circle2D( pass->receive_point_, 6.0 ),
                                  10,
                                  true )
         && ( ! opp_goalie
              || opp_goalie->pos().dist( pass->receive_point_ ) > 6.0 )
         )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": CrossAreaKick. very chance pass." );
        agent->debugClient().addMessage( "XAreaPassChance1" );
        rcsc::Bhv_PassTest().execute( agent );
        return;
    }

    if ( pass )
    {
        rcsc::Rect2D shoot_rect = rcsc::Rect2D::from_center( 52.5 - 7.0,
                                                             0.0,
                                                             14.0,
                                                             32.0 );
        if ( shoot_rect.contains( pass->receive_point_ )
             && receiver_free
             && wm.countOpponentsIn( shoot_rect, 10, false ) <= 2
             && ! wm.existOpponentIn( rcsc::Circle2D( pass->receive_point_, 5.0 ),
                                      10
                                      , true ) )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": CrossAreaKick. very chance pass(2)." );
            agent->debugClient().addMessage( "XAreaPassChance2" );
            rcsc::Bhv_PassTest().execute( agent );
            return;
        }
    }

    if ( pass
         && pass->receive_point_.x > 39.0
         && pass->receive_point_.absY() < 7.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": can cross pass." );
        agent->debugClient().addMessage( "XAreaPass1" );
        rcsc::Bhv_PassTest().execute( agent );
        return;
    }

    // very side attack
    if ( pass
         && wm.self().pos().absY() > 27.0
         && opp_dist < 4.0
         && pass->receive_point_.x > 26.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": very side attack.  cross" );
        agent->debugClient().addMessage( "XAreaPass2" );
        rcsc::Bhv_PassTest().execute( agent );
        return;
    }

    //-------------------------------------------------------
    // cross to center
    // exist teammate at cross area
    if ( opp_dist < 3.0
         && Bhv_Cross::get_best_point( agent, NULL ) )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": teammate in cross area. cross to center" );
        agent->debugClient().addMessage( "XAreaCross1" );
        Bhv_Cross().execute( agent );
        return;
    }

#if 1
    //-------------------------------------------------------
    // dribble cource is blocked
    if ( pass
         && receiver_free
         && wm.self().pos().x > 40.0
         && wm.self().pos().absY() > 20.0 )
    {
        rcsc::Vector2D rect_center( 47.0,
                                    ( wm.self().pos().absY() + 9.0 ) * 0.5 );
        if ( wm.self().pos().y < 0.0 ) rect_center.y *= -1.0;

        rcsc::Rect2D side_rect
            = rcsc::Rect2D::from_center( rect_center,
                                         10.0,
                                         wm.self().pos().absY() - 9.0 );
        agent->debugClient().addRectangle( side_rect );
        if ( wm.countOpponentsIn( side_rect, 10, false ) >= 3 ) // 2 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": CrossAreaKick. dribble cource blocked." );
            agent->debugClient().addMessage( "XAreaPassBlocked" );
            rcsc::Bhv_PassTest().execute( agent );
            return;
        }
    }
#endif

    //-------------------------------------------------------
    if ( S_recover_mode
         && ! goalie_near
         && opp_dist > 4.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": recovering" );
        agent->debugClient().addMessage( "XAreaHold" );
        rcsc::Body_HoldBall2008().execute( agent );
        agent->setNeckAction( new rcsc::Neck_TurnToLowConfTeammate() );
        return;
    }

    //-------------------------------------------------------
    // keep dribble
    if ( ! goalie_near
         // && opp_dist > rcsc::ServerParam::i().tackleDist() + 0.5
         // 2008-07-19
         && opp_dist > rcsc::ServerParam::i().tackleDist() + 1.0
//          && ( ( opp_dist > rcsc::ServerParam::i().tackleDist() + 0.5
//                 && opp_pos.x < next_self_pos.x - 0.5 )
//               || opp_dist > rcsc::ServerParam::i().tackleDist() + 1.5 )
         )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": goalie not near and not tackle opp. wing dribble. opp_dist=%.2f",
                            opp_dist );
        agent->debugClient().addMessage( "XAreaDrib1" );
        doSideForwardDribble( agent );
        return;
    }
    //-------------------------------------------------------
    // forcely pass
    if ( pass )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": enforce pass" );
        agent->debugClient().addMessage( "XAreaPass3" );
        rcsc::Bhv_PassTest().execute( agent );
        return;
    }

    //-------------------------------------------------------
    // not exist opp on my body dir
    rcsc::Sector2D body_sector( wm.self().pos(),
                                0.5, 8.0,
                                wm.self().body() - 25.0,
                                wm.self().body() + 25.0 );
    // oponent check with goalie
    if ( ! wm.existOpponentIn( body_sector, 10, true ) )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": exsit opp in my body dir. wing dribble" );
        agent->debugClient().addMessage( "XAreaDrib2" );
        doSideForwardDribble( agent );
        return;
    }

    //-------------------------------------------------------
    // forcely cross
    if ( S_recover_mode && opp_dist < 2.5 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": enforce cross" );
        agent->debugClient().addMessage( "XAreaCross2" );
        Bhv_Cross().execute( agent );
        return;
    }

    doSideForwardDribble( agent );
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleLSideForward::doShootAreaKick( rcsc::PlayerAgent * agent )
{
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doShootAreaKick" );

    const rcsc::WorldModel & wm = agent->world();
    const rcsc::PlayerObject * opp_goalie = wm.getOpponentGoalie();
    //--------------------------------------------------------//
    // get pass info

    const rcsc::Bhv_PassTest::PassRoute * pass
        = rcsc::Bhv_PassTest::get_best_pass( wm );

    if ( pass
         && pass->receive_point_.x > 44.0
         && pass->receive_point_.absY() < rcsc::ServerParam::i().goalHalfWidth() + 3.0
         && ( ! opp_goalie
              || opp_goalie->pos().dist( pass->receive_point_ ) > 6.0 )
         )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": ShootAreaKick Pass (%.1f, %.1f)",
                            pass->receive_point_.x,
                            pass->receive_point_.y );
        agent->debugClient().addMessage( "ShootAreaPass" );
        rcsc::Bhv_PassTest().execute( agent );
        return;
    }

    //--------------------------------------------------------//
    // get cross info
    rcsc::Vector2D cross_point;
    const bool can_cross
        = Bhv_Cross::get_best_point( agent, &cross_point );

    if ( can_cross )
    {
        double opp_to_cross_dist = 100.0;
        const rcsc::PlayerObject * opp_to_cross = wm.getOpponentNearestTo( cross_point,
                                                                           6,
                                                                           &opp_to_cross_dist );
        if ( cross_point.x > 43.0
             && opp_to_cross_dist > 3.0
             && ( ! opp_goalie
                  || opp_goalie->pos().dist( cross_point ) > 5.0 )
             && cross_point.absY() < 10.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": ShootAreaKick Cross. very chance area (%.1f, %.1f)",
                                cross_point.x, cross_point.y );
            agent->debugClient().addMessage( "ShootAreaCross(1)" );
            Bhv_Cross().execute( agent );
            return;
        }
#if 1
        if ( ( ! opp_to_cross
               || opp_to_cross_dist > 5.0 )
             && cross_point.x > 45.0
             && ( cross_point.absY() < 7.0
                  || cross_point.y * wm.ball().pos().y < 0.0 ) // opposite side
             )
        {


            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": ShootAreaKick Cross. big space (%.1f, %.1f)",
                                cross_point.x, cross_point.y );
            agent->debugClient().addMessage( "ShootAreaCross(2)" );
            Bhv_Cross().execute( agent );
            return;
        }

        rcsc::Rect2D front_rect( rcsc::Vector2D( cross_point.x,
                                                 cross_point.y - 3.0 ),
                                 rcsc::Size2D( 4.0, 6.0 ) );
        if ( cross_point.x > 44.0
             && opp_to_cross_dist > 3.0
             && ( ! opp_goalie
                  || opp_goalie->pos().dist( cross_point ) > 5.0 )
             && ( cross_point.absY() < 7.0
                  || cross_point.y * wm.ball().pos().y < 0.0 ) // opposite side
             && ! wm.existOpponentIn( front_rect, 6, true ) )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": ShootAreaKick Cross. exist shoot cource (%.1f, %.1f)",
                                cross_point.x, cross_point.y );
            agent->debugClient().addMessage( "ShootAreaCross(3)" );
            Bhv_Cross().execute( agent );
            return;
        }
#endif
    }

    const rcsc::PlayerObject * nearest_opp = wm.getOpponentNearestToSelf( 10 );
    const rcsc::Vector2D nearest_opp_pos = ( nearest_opp
                                             ? nearest_opp->pos()
                                             : rcsc::Vector2D( -1000.0, 0.0 ) );
    const double nearest_opp_dist = ( nearest_opp
                                      ? nearest_opp->distFromSelf()
                                      : 1000.0 );
    //--------------------------------------------------------//
    // enforce cross
    if ( can_cross
         && nearest_opp_dist < 2.5
         && cross_point.x > 38.0
         && cross_point.absY() < 10.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": ShootAreaKick enforce Cross. (%.1f %.1f)",
                            cross_point.x, cross_point.y );
        agent->debugClient().addMessage( "EnforceCross(1)" );
        Bhv_Cross().execute( agent );
        return;
    }

    if ( nearest_opp_dist < 2.0
         && wm.self().pos().x > 44.0
         && wm.self().pos().absY() < 15.0 )
    {
        rcsc::Vector2D enforce_cross_point( 47.0, 0.0 );
        rcsc::Rect2D shoot_area
            = rcsc::Rect2D::from_center( enforce_cross_point,
                                         5.0, 5.0 );
        if ( wm.existTeammateIn( shoot_area, 5, true ) )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": Enforce Cross" );
            agent->debugClient().addMessage( "EnforceCross" );
            agent->debugClient().setTarget( enforce_cross_point );

            rcsc::Body_SmartKick( enforce_cross_point,
                                  rcsc::ServerParam::i().ballSpeedMax(),
                                  rcsc::ServerParam::i().ballSpeedMax() * 0.95,
                                  3 ).execute( agent );
            agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan() );
            return;
        }
    }

    //--------------------------------------------------------//
    // try dribble
    rcsc::Vector2D cut_in_point( 50.0, rcsc::ServerParam::i().goalHalfWidth() );
    if ( wm.self().pos().y < 0.0 )
    {
        cut_in_point.y *= -1.0;
    }

    // check opponent near to dribble target

    if ( wm.self().pos().x > 45.0
         && wm.self().pos().absY() < 15.0 )
    {
        //if ( nearest_opp->distFromSelf() > 2.0 )
        if ( 2.0 < nearest_opp->distFromSelf()
             && nearest_opp->distFromSelf() < 3.0 )
        {
            agent->debugClient().addMessage( "ShotHold1" );

            rcsc::Vector2D face_point( 50.0, 0.0 );
            rcsc::Body_HoldBall2008( true, face_point ).execute( agent );
            agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan() );
            return;
        }
    }

    //------------------------------------------------------------//
    // check dribble chance
    if ( nearest_opp_dist > 1.6
         && wm.self().pos().x > 38.0
         && wm.self().pos().absY() < 14.0 )
    {
//         const rcsc::PlayerObject * opp_goalie = wm.getOpponentGoalie();
//         bool goalie_front = false;

//         if ( opp_goalie
//              && opp_goalie->pos().x < 46.0
//              && opp_goalie->pos().x < wm.self().pos().x + 7.0
//              && std::fabs( opp_goalie->pos().y - wm.self().pos().y ) < 1.6 )
//         {
//             goalie_front = true;
//         }

//         if ( ! goalie_front )
        {
            rcsc::Vector2D drib_target( 47.0, wm.self().pos().y );
            rcsc::Rect2D target_rect( rcsc::Vector2D( wm.ball().pos().x + 0.5,
                                                      wm.ball().pos().y - 1.4 ),
                                      rcsc::Size2D( 2.5, 2.8 ) );
            int dash_step = 3;
            if ( wm.self().pos().x > drib_target.x - 1.0 )
            {
                drib_target.y = ( wm.self().pos().y > 0.0
                                  ? 7.0
                                  : -7.0 );
                double y_diff = drib_target.y - wm.self().pos().y;
                target_rect = rcsc::Rect2D::from_center( drib_target.x,
                                                         wm.self().pos().y + 1.4 * y_diff / std::fabs( y_diff ),
                                                         2.5,
                                                         2.5 );
                dash_step = 1;
            }

            //if ( ! wm.existOpponentIn( target_rect, 10, true ) ) // with goalie
            if ( ! wm.existOpponentIn( target_rect, 10, false ) ) // without goalie
            {
                agent->debugClient().addMessage( "ShotAreaDrib" );
                agent->debugClient().addRectangle( target_rect );
                agent->debugClient().setTarget( drib_target );

                rcsc::Body_Dribble2008( drib_target,
                                        1.0,
                                        dash_power,
                                        dash_step,
                                        false // no dodge
                                        ).execute( agent );
                agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan() );
                return;
            }
        }
    }

    if ( 3.0 < nearest_opp->distFromSelf() )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": ShootAreaKick opp not near. cut in dribble" );
        doSideForwardDribble( agent );
        return;
    }

    if ( can_cross )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": ShootAreaKick  cross" );
        agent->debugClient().addMessage( "ShootAreaCross2" );
        Bhv_Cross().execute( agent );
        return;
    }

    // pass
    if ( pass
         && pass->receive_point_.x > 30.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": ShootAreaKick pass" );
        agent->debugClient().addMessage( "ShootAreaPass2" );
        rcsc::Bhv_PassTest().execute( agent );
        return;
    }


    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": ShootAreaKick Hold" );
    agent->debugClient().addMessage( "ShotHold2" );
    rcsc::Body_HoldBall2008().execute( agent );
    agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan() );
}

/*-------------------------------------------------------------------*/
/*!

*/
double
RoleLSideForward::getShootAreaMoveDashPower( rcsc::PlayerAgent * agent,
                                            const rcsc::Vector2D & target_point )
{
    const rcsc::WorldModel & wm = agent->world();

    static bool s_recover = false;

   // double dash_power = dash_power;

    // recover check
    // check X buffer & stamina
    if ( wm.self().stamina() < rcsc::ServerParam::i().effortDecThrValue() + 500.0 )
    {
        if ( wm.self().pos().x > 30.0 )
        {
            s_recover = true;
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": recover on" );
        }
    }
    else if ( wm.self().stamina() > rcsc::ServerParam::i().effortIncThrValue() )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": recover off" );
        s_recover = false;
    }

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( opp_min < self_min - 2
         && opp_min < mate_min - 5 )
    {
      //  dash_power = std::max( dash_power * 0.2,
        //                       wm.self().playerType().staminaIncMax() * 0.8 );
    }
    else if ( wm.ball().pos().x > wm.self().pos().x )
    {
        // keep max power
     //   dash_power = dash_power;
    }
    else if ( wm.ball().pos().x > 41.0
              && wm.ball().pos().absY() < rcsc::ServerParam::i().goalHalfWidth() + 5.0 )
    {
        // keep max power
       // dash_power = dash_power;
    }
    else if ( s_recover )
    {
        //dash_power = wm.self().playerType().staminaIncMax() * 0.6;
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": recovering" );
    }
    else if ( wm.interceptTable()->teammateReachCycle() <= 1
              && wm.ball().pos().x > 33.0
              && wm.ball().pos().absY() < 7.0
              && wm.ball().pos().x < wm.self().pos().x
              && wm.self().pos().x < wm.offsideLineX()
              && wm.self().pos().absY() < 9.0
              && std::fabs( wm.ball().pos().y - wm.self().pos().y ) < 3.5
              && std::fabs( target_point.y - wm.self().pos().y ) > 5.0 )
    {
       // dash_power = wm.self().playerType()
         //   .getDashPowerToKeepSpeed( 0.3, wm.self().effort() );
        //dash_power = std::min( dash_power * 0.75,
             //                  dash_power );
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": slow for cross. power=%.1f",
                            dash_power );
    }
    else
    {
       // dash_power = dash_power * 0.75;
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": ball is far" );
    }

    return dash_power;
}

/*-------------------------------------------------------------------*/
/*!

*/
rcsc::Vector2D
RoleLSideForward::getShootAreaMoveTarget( rcsc::PlayerAgent * agent,
                                         const rcsc::Vector2D & home_pos )
{
    const rcsc::WorldModel & wm = agent->world();

    rcsc::Vector2D target_point = home_pos;

    int mate_min = wm.interceptTable()->teammateReachCycle();
    rcsc::Vector2D ball_pos = ( mate_min < 10
                                ? wm.ball().inertiaPoint( mate_min )
                                : wm.ball().pos() );
    bool opposite_side = false;
    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    if ( ( ball_pos.y > 0.0
           && position_type == Position_Left )
         || ( ball_pos.y <= 0.0
              && position_type == Position_Right )
         || ( ball_pos.y * home_pos.y < 0.0
              && position_type == Position_Center )
         )
    {
        opposite_side = true;
    }

    if ( opposite_side
         && ball_pos.x > 40.0 // very chance
         && 6.0 < ball_pos.absY()
         && ball_pos.absY() < 15.0 )
    {
        rcsc::Circle2D goal_area( rcsc::Vector2D( 47.0, 0.0 ),
                                  7.0 );

        if ( ! wm.existTeammateIn( goal_area, 10, true ) )
        {
            agent->debugClient().addMessage( "GoToCenterCross" );

            target_point.x = 47.0;

            if ( wm.self().pos().x > wm.offsideLineX() - 0.5
                 && target_point.x > wm.offsideLineX() - 0.5 )
            {
                target_point.x = wm.offsideLineX() - 0.5;
            }

            if ( ball_pos.absY() < 9.0 )
            {
                target_point.y = 0.0;
            }
            else
            {
                target_point.y = ( ball_pos.y > 0.0
                                   ? 1.0 : -1.0 );
                                   //? 2.4 : -2.4 );
            }

            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": ShootAreaMove. center cross point" );
        }
        else
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": ShootAreaMove. exist teammate in cross point" );
        }
    }

    // consider near opponent
    {
        double opp_dist = 200.0;
        const rcsc::PlayerObject * nearest_opp
            = wm.getOpponentNearestTo( target_point, 10, &opp_dist );

        if ( nearest_opp
             && opp_dist < 2.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::ROLE,
                                __FILE__": ShootAreaMove. Point Blocked. old target=(%.2f, %.2f)",
                                target_point.x, target_point.y );
            agent->debugClient().addMessage( "AvoidOpp" );
            //if ( nearest_opp->pos().x + 3.0 < wm.offsideLineX() - 1.0 )
            if ( nearest_opp->pos().x + 2.0 < wm.offsideLineX() - 1.0  )
            {
                //target_point.x = nearest_opp->pos().x + 3.0;
                target_point.x = nearest_opp->pos().x + 2.0;
            }
            else
            {
                target_point.x = nearest_opp->pos().x - 2.0;
#if 1
                if ( std::fabs( wm.self().pos().y - target_point.y ) < 1.0 )
                {
                    target_point.x = nearest_opp->pos().x - 3.0;
                }
#endif
            }
        }

    }

    // consider goalie
    if ( target_point.x > 45.0 )
    {
        const rcsc::PlayerObject * opp_goalie = wm.getOpponentGoalie();
        if ( opp_goalie
             && opp_goalie->distFromSelf() < wm.ball().distFromSelf() )
        {
            rcsc::Line2D cross_line( ball_pos, target_point );
            if (  cross_line.dist( opp_goalie->pos() ) < 3.0 )
            {
                agent->debugClient().addMessage( "AvoidGK" );
                agent->debugClient().addLine( ball_pos, target_point );
                rcsc::dlog.addText( rcsc::Logger::ROLE,
                                    __FILE__": ShootAreaMove. Goalie is on cross line. old target=(%.2f %.2f)",
                                    target_point.x, target_point.y );

                rcsc::Line2D move_line( wm.self().pos(), target_point );
                target_point.x -= 2.0;
            }
        }
    }

    return target_point;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RoleLSideForward::doShootAreaMove( rcsc::PlayerAgent* agent,
                                  const rcsc::Vector2D& home_pos )
{
    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doShootAreaMove" );

    const rcsc::WorldModel & wm = agent->world();

    if ( Bhv_BasicTackle( 0.8, 90.0 ).execute( agent ) )
    {
        return;
    }

    //------------------------------------------------------

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doShootAreaMove. intercept cycle. self=%d, mate=%d, opp=%d",
                        self_min, mate_min, opp_min );
    if ( ( ! wm.existKickableTeammate() && self_min < 4 )
         || self_min < mate_min
         || ( self_min <= mate_min && self_min > 5 )
         )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doShootAreaMove. get ball" );
        agent->debugClient().addMessage( "SFW:GetBall(1)" );

        rcsc::Body_Intercept2008().execute( agent );
#if 0
        if ( self_min == 4 && opp_min >= 2 )
        {
            agent->setViewAction( new rcsc::View_Wide() );
        }
        else if ( self_min == 3 && opp_min >= 2 )
        {
            agent->setViewAction( new rcsc::View_Normal() );
        }

        if ( wm.self().pos().x > 30.0 )
        {
            agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan() );
        }
        else
        {
            agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
        }
#else
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
#endif
        return;
    }

    //------------------------------------------------------
    if ( self_min < 20
         && self_min < mate_min
         && self_min < opp_min + 20
         && wm.ball().pos().absY() > 19.0
         //&& wm.interceptTable()->isOurTeamBallPossessor() )
         )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doShootAreaMove. get ball(2)" );
        agent->debugClient().addMessage( "SFW:GetBall(2)" );

        rcsc::Body_Intercept2008().execute( agent );
#if 0
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
        return;
    }

    //------------------------------------------------------
    // decide move target point & dash power
    const rcsc::Vector2D target_point = getShootAreaMoveTarget( agent, home_pos );
   // const double dash_power = getShootAreaMoveDashPower( agent, target_point );
    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    rcsc::dlog.addText( rcsc::Logger::ROLE,
                        __FILE__": doShootAreaMove. go to cross point (%.2f, %.2f)",
                        target_point.x, target_point.y );

    agent->debugClient().addMessage( "ShootMoveP%.0f", dash_power );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );


    if ( wm.self().pos().x > target_point.x + dist_thr*0.5
         && std::fabs( wm.self().pos().x - target_point.x ) < 3.0
         && wm.self().body().abs() < 10.0 )
    {
        agent->debugClient().addMessage( "Back" );
       // double back_dash_power
         //   = wm.self().getSafetyDashPower( -dash_power );
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doShootAreaMove. Back Move" );
        agent->doDash( -dash_power );
        agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
        return;
    }

    if ( ! rcsc::Body_GoToPoint( target_point, dist_thr, dash_power,
                                 5, // cycle
                                 false, // no back dash
                                 true, // save recovery
                                 20.0 // dir thr
                                 ).execute( agent ) )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doShootAreaMove. already target point. turn to front" );
        rcsc::Body_TurnToAngle( 0.0 ).execute( agent );
    }

    if ( wm.self().pos().x > 35.0 )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doShootAreaMove. Neck to Goalie or Scan" );
        agent->setNeckAction( new rcsc::Neck_TurnToGoalieOrScan() );
    }
    else
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": doShootAreaMove. Neck to Ball or Scan" );
        agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
    }
}

void
RoleLSideForward::doGoToPoint( rcsc::PlayerAgent * agent,
                             const rcsc::Vector2D & target_point,
                             const double & dist_thr,
                             const double & dash_power,
                             const double & dir_thr )
{

    if ( rcsc::Body_GoToPoint( target_point,
                         dist_thr,
                         dash_power,
                         100, // cycle
                         false, // no back
                         true, // stamina save
                         dir_thr
                         ).execute( agent ) )
    {
        return;
    }

    const rcsc::WorldModel & wm = agent->world();

    rcsc::AngleDeg body_angle;
    if ( wm.ball().pos().x < -30.0 )
    {
        body_angle = wm.ball().angleFromSelf() + 90.0;
        if ( wm.ball().pos().x < -45.0 )
        {
            if (  body_angle.degree() < 0.0 )
            {
                body_angle += 180.0;
            }
        }
        else if ( body_angle.degree() > 0.0 )
        {
            body_angle += 180.0;
        }
    }
    else // if ( std::fabs( wm.self().pos().y - wm.ball().pos().y ) > 4.0 )
    {
        //body_angle = wm.ball().angleFromSelf() + ( 90.0 + 20.0 );
        body_angle = wm.ball().angleFromSelf() + 90.0;
        if ( wm.ball().pos().x > wm.self().pos().x + 15.0 )
        {
            if ( body_angle.abs() > 90.0 )
            {
                body_angle += 180.0;
            }
        }
        else
        {
            if ( body_angle.abs() < 90.0 )
            {
                body_angle += 180.0;
            }
        }
    }
    /*
      else
      {
      body_angle = ( wm.ball().pos().y < wm.self().pos().y
      ? -90.0
      : 90.0 );
      }
    */

    rcsc::Body_TurnToAngle( body_angle ).execute( agent );

}