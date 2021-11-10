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

#include "bhv_pre_process.h"

#include "strategy.h"

#include "bhv_breakaway_through_pass.h"
#include "view_tactical.h"

#include "body_intercept2008.h"
#include "bhv_shoot2008.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/bhv_before_kick_off.h>
#include <rcsc/action/bhv_emergency.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_kick_one_step.h>
//#include <rcsc/action/body_shoot.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include "neck_offensive_intercept_neck.h"

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/audio_memory.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_PreProcess::execute( rcsc::PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();

    //////////////////////////////////////////////////////////////
    // freezed by tackle effect
    if ( wm.self().isFrozen() )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": tackle wait. expires= %d",
                            wm.self().tackleExpires() );
        // face neck to ball
        agent->setViewAction( new View_Tactical() );
        agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
        return true;
    }

    //////////////////////////////////////////////////////////////
    // BeforeKickOff or AfterGoal. should jump to the initial position
    if ( wm.gameMode().type() == rcsc::GameMode::BeforeKickOff
         || wm.gameMode().type() == rcsc::GameMode::AfterGoal_ )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": before_kick_off" );
        rcsc::Vector2D move_point = Strategy::i().getPosition( wm.self().unum() );
        agent->setViewAction( new View_Tactical() );
        
        //---おまけモーション---
	static int last_our_points = 0;
	static int last_opp_points = 0;
	int current_our_points = 0;
	int current_opp_points = 0;
/*	
	if (wm.gameMode().type() == rcsc::GameMode::BeforeKickOff &&
				    wm.time().cycle() < 1 && agent->world().setplayCount()<=47 )	//start of game
	{	
		if(!agent->world().setplayCount()%12)							//draw letter G
	  	{
			int setted_move_points_x[]={0,0,0,0,0,0,0,0,0,0,0};
			int setted_move_points_y[]={0,0,0,0,0,0,0,0,0,0,0};
			int count = agent->world().setplayCount();
			int unum = agent->world().self().unum();
			int index = (count + unum)%11;
			move_point.x=setted_move_points_x[unum-1];
			move_point.y=setted_move_points_y[unum-1];
		}
		if(agent->world().setplayCount()<=11)							//draw letter G
	  	{
			int setted_move_points_x[]={-19,-15,-21,-24,-25,-24,-22,-19,-16,-15,-18};
			int setted_move_points_y[]={0,4,6,4,0,-4,-7,-8,-7,0,6};
			int count = agent->world().setplayCount();
			int unum = agent->world().self().unum();
			int index = (count + unum)%11;
			move_point.x=setted_move_points_x[unum-1];
			move_point.y=setted_move_points_y[unum-1];
		}
		else if(agent->world().setplayCount()>12 && agent->world().setplayCount()<=23)		//draw letter E
	  	{
			int setted_move_points_x[]={-19,-15,-24,-24,-24,-24,-24,-19,-15,-15,-19};
			int setted_move_points_y[]={0,7,7,3,0,-3,-7,-7,-7,0,7};
			int count = agent->world().setplayCount() - 12;
			int unum = agent->world().self().unum();
			int index = (count + unum)%11;
			move_point.x=setted_move_points_x[unum-1];
			move_point.y=setted_move_points_y[unum-1];
		}
		else if(agent->world().setplayCount()>24 && agent->world().setplayCount()<=35)		//draw letter A
	  	{
			int setted_move_points_x[]={-19,-16,-26,-20,-24,-16,-14,-18,-22,-12,-10};
			int setted_move_points_y[]={0,0,5,-7,0,-7,-4,-10,-4,0,5};
			int count = agent->world().setplayCount() - 21;
			int unum = agent->world().self().unum();
			int index = (count + unum)%11;
			move_point.x=setted_move_points_x[unum-1];
			move_point.y=setted_move_points_y[unum-1];
		}
		else if(agent->world().setplayCount()>36 && agent->world().setplayCount()<=47)		//draw letter R
	  	{
			int setted_move_points_x[]={-19,-16,-24,-24,-24,-24,-18,-16,-21,-19,-18};
			int setted_move_points_y[]={0,7,7,2,-2,-7,3,-4,1,-7,-1};
			int count = agent->world().setplayCount() - 32;
			int unum = agent->world().self().unum();
			int index = (count + unum)%11;
			move_point.x=setted_move_points_x[unum-1];
			move_point.y=setted_move_points_y[unum-1];
		}
	
		rcsc::Bhv_BeforeKickOff( move_point ).execute( agent );
	        return true;
	        
	}
	
	if(agent->world().isOurLeft()){
	  current_our_points = agent->world().gameMode().scoreLeft();
	  current_opp_points = agent->world().gameMode().scoreRight();
	}else{
	      current_our_points = agent->world().gameMode().scoreRight();
	      current_opp_points = agent->world().gameMode().scoreLeft();
	}
	if(agent->world().setplayCount()<40)
	  {
	    if(current_our_points > last_our_points)			//if our team scored
	      {
		int r = agent->world().setplayCount()%360;
		int Unum = agent->world().self().unum();
		int theta = (40 - agent->world().setplayCount());
		double radius = theta;
		move_point.x = -15 + radius*cos(theta);
		move_point.y = radius*sin(theta);	      }
	    else if(current_opp_points > last_opp_points)		//if our team suffered goal
	      {
		//敗北モーション
		int setted_move_points_x[]={
		  -20,-19,-18,-17,-16,-15,-14,-13,-12,-11,
		  -11,-12,-13,-14,-15,-16,-17,-18,-19,-20
		};
		int setted_move_points_y[]={
		  -5,-4,-3,-2,-1,0,1,2,3,4,
		  -5,-4,-3,-2,-1,0,1,2,3,4
		};
		int count = agent->world().setplayCount();
		int unum = agent->world().self().unum()*2;
		int index = (count + unum)%20;
		
		move_point.x=setted_move_points_x[index]*2;
		move_point.y=setted_move_points_y[index]*2;
	      }
	  }else{
	  last_our_points = current_our_points;
	  last_opp_points = current_opp_points;
	}
 	//-----------*/

        rcsc::Bhv_BeforeKickOff( move_point ).execute( agent );
        return true;
    }

    //////////////////////////////////////////////////////////////
    // my pos is unknown
    if ( ! wm.self().posValid() )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": invalid my pos" );
        // included change view
        rcsc::Bhv_Emergency().execute( agent );
        return true;
    }
    //////////////////////////////////////////////////////////////
    // ball search
    // included change view
    int count_thr = 5;
    if ( wm.self().goalie() ) count_thr = 10;
    if ( wm.ball().posCount() > count_thr )
        // || wm.ball().rposCount() > count_thr + 3 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": search ball" );
        agent->debugClient().addMessage( "SearchBall" );
        agent->setViewAction( new View_Tactical() );
        rcsc::Bhv_NeckBodyToBall().execute( agent );
        return true;
    }

    //////////////////////////////////////////////////////////////
    // default change view

    agent->setViewAction( new View_Tactical() );

    //////////////////////////////////////////////////////////////
    // check shoot chance
    if ( wm.gameMode().type() != rcsc::GameMode::BackPass_
         && wm.gameMode().type() != rcsc::GameMode::CatchFault_
         && wm.gameMode().type() != rcsc::GameMode::IndFreeKick_
         && wm.time().stopped() == 0
         && wm.self().isKickable()
         && rcsc::Bhv_Shoot2008().execute( agent ) )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": shooted" );
        // reset intention
        agent->setIntention( static_cast< rcsc::SoccerIntention * >( 0 ) );
        return true;
    }

    //////////////////////////////////////////////////////////////
    // check queued action
    if ( agent->doIntention() )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": do queued intention" );
        return true;
    }

    //////////////////////////////////////////////////////////////
    // check simultaneous kick
    if ( wm.gameMode().type() == rcsc::GameMode::PlayOn
         && ! wm.self().goalie()
         && wm.self().isKickable()
         && wm.existKickableOpponent() )
    {
        const rcsc::PlayerObject * kicker = wm.interceptTable()->fastestOpponent();
        if ( kicker
             && ! kicker->isTackling()
             && kicker->isKickable( 0.1 ) )
        {
            rcsc::Vector2D goal_pos( rcsc::ServerParam::i().pitchHalfLength(), 0.0 );

            if ( wm.self().pos().x > 36.0
                 && wm.self().pos().absY() > 10.0 )
            {
                goal_pos.x = 45.0;
                rcsc::dlog.addText( rcsc::Logger::TEAM,
                                    __FILE__": simultaneous kick cross type" );
            }
            else if ( 10.0 < wm.self().pos().x
                      && wm.self().pos().x < 33.0 )
            {
                goal_pos.x = 45.0;
                goal_pos.y = 23.0;
                if ( wm.ball().pos().y < 0.0 ) goal_pos.y *= -1.0;
            }

            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": simultaneous kick" );
            agent->debugClient().addMessage( "SimultaneousKick" );
            agent->debugClient().setTarget( goal_pos );

            rcsc::Body_KickOneStep( goal_pos,
                                    rcsc::ServerParam::i().ballSpeedMax(),
                                    true // force mode
                                    ).execute( agent );
            agent->setNeckAction( new rcsc::Neck_TurnToBall );
            return true;
        }
    }

    //////////////////////////////////////////////////////////////
    // check pass request
#if 0
    if ( wm.heardPassRequestTime() == wm.time()
         && wm.self().isKickable() )
    {
        if ( Bhv_BreakawayThroughPass().execute( agent ) )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": breakaway thorough pass" );
            return true;
        }
    }
#endif

    //////////////////////////////////////////////////////////////
    // check communication intention
    if ( wm.audioMemory().passTime() == wm.time()
         && ! wm.audioMemory().pass().empty()
         && ( wm.audioMemory().pass().front().receiver_
              == wm.self().unum() )
         )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": communication receive" );
        doReceiveMove( agent );

        return true;
    }

    return false;
}


/*-------------------------------------------------------------------*/
/*!

*/
void
Bhv_PreProcess::doReceiveMove( rcsc::PlayerAgent * agent )
{
    agent->debugClient().addMessage( "IntentionRecv" );

    const rcsc::WorldModel & wm = agent->world();
    int self_min = wm.interceptTable()->selfReachCycle();

    rcsc::Vector2D self_trap_pos = wm.ball().inertiaPoint( self_min );
    rcsc::Vector2D receive_pos = ( wm.audioMemory().pass().empty()
                                   ? self_trap_pos
                                   : wm.audioMemory().pass().front().receive_pos_ );

    if ( ( ! wm.existKickableTeammate()
           && wm.ball().posCount() <= 1
           && wm.ball().velCount() <= 1
           && self_min < 6
           && self_trap_pos.dist( receive_pos ) < 8.0 )
         || wm.audioMemory().pass().empty() )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": PreProcess. Receiver. intercept cycle=%d",
                            self_min );

        agent->debugClient().addMessage( "Intercept_1" );
        rcsc::Body_Intercept2008().execute( agent );

#if 0
        int opp_min = wm.interceptTable()->opponentReachCycle();
        if ( self_min == 4 && opp_min >= 3 )
        {
            agent->setViewAction( new rcsc::View_Wide() );
        }
        else if ( self_min == 3 && opp_min >= 2 )
        {
            agent->setViewAction( new rcsc::View_Normal() );
        }
        else if ( self_min > 10 )
        {
            agent->setViewAction( new rcsc::View_Normal() );
        }

        agent->setNeckAction( new rcsc::Neck_TurnToBall() );
#else
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
#endif
        return;
    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": PreProcess. Receiver. intercept cycle=%d. go to receive point",
                        self_min );
    bool back_mode = false;

    rcsc::Vector2D target_rel = receive_pos - wm.self().pos();
    rcsc::AngleDeg target_angle = target_rel.th();
    if ( target_rel.r() < 6.0
         && ( target_angle - wm.self().body() ).abs() > 100.0
         && wm.self().stamina() > rcsc::ServerParam::i().staminaMax() * 0.6 )
    {
        back_mode = true;
    }

    agent->debugClient().setTarget( receive_pos );
    agent->debugClient().addCircle( receive_pos, 0.5 );
    agent->debugClient().addMessage( "GoTo" );

    rcsc::Body_GoToPoint( receive_pos,
                          3.0,
                          rcsc::ServerParam::i().maxPower(),
                          100,
                          back_mode
                          ).execute( agent );
    agent->setNeckAction( new rcsc::Neck_TurnToBall() );
}
