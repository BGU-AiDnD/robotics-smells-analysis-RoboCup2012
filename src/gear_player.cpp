// -*-c++-*-

/*!
  \file helios_player.cpp
  \brief concrete player agent Source File
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


#include "gear_player.h"

#include "strategy.h"
#include "soccer_role.h"

#include "bhv_goalie_free_kick.h"
#include "bhv_penalty_kick.h"
#include "bhv_pre_process.h"
#include "bhv_set_play.h"
#include "bhv_set_play_kick_in.h"
#include "bhv_set_play_indirect_free_kick.h"
#include "view_tactical.h"

#include "kick_table.h"

#include <rcsc/formation/formation.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/say_message_builder.h>
#include <rcsc/player/audio_sensor.h>
#include <rcsc/player/freeform_parser.h>


#include <rcsc/common/logger.h>
#include <rcsc/common/basic_client.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_param.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/common/say_message_parser.h>
#include <rcsc/common/free_message_parser.h>

#include <rcsc/param/param_map.h>
#include <rcsc/param/cmd_line_parser.h>

#include <boost/shared_ptr.hpp>

#include <sstream>

/*-------------------------------------------------------------------*/
/*!
  constructor
*/
GearPlayer::GearPlayer()
    : rcsc::PlayerAgent()
{
    typedef boost::shared_ptr< rcsc::SayMessageParser > SMP;

    boost::shared_ptr< rcsc::AudioMemory > audio_memory( new rcsc::AudioMemory );

    M_worldmodel.setAudioMemory( audio_memory );

    addSayMessageParser( SMP( new rcsc::BallMessageParser( audio_memory ) ) );
    addSayMessageParser( SMP( new rcsc::PassMessageParser( audio_memory ) ) );
    addSayMessageParser( SMP( new rcsc::InterceptMessageParser( audio_memory ) ) );
    addSayMessageParser( SMP( new rcsc::GoalieMessageParser( audio_memory ) ) );
    addSayMessageParser( SMP( new rcsc::OffsideLineMessageParser( audio_memory ) ) );
    addSayMessageParser( SMP( new rcsc::DefenseLineMessageParser( audio_memory ) ) );
    addSayMessageParser( SMP( new rcsc::WaitRequestMessageParser( audio_memory ) ) );
    addSayMessageParser( SMP( new rcsc::PassRequestMessageParser( audio_memory ) ) );
    addSayMessageParser( SMP( new rcsc::DribbleMessageParser( audio_memory ) ) );
    addSayMessageParser( SMP( new rcsc::BallGoalieMessageParser( audio_memory ) ) );
    addSayMessageParser( SMP( new rcsc::OnePlayerMessageParser( audio_memory ) ) );
    addSayMessageParser( SMP( new rcsc::SelfMessageParser( audio_memory ) ) );
    addSayMessageParser( SMP( new rcsc::BallPlayerMessageParser( audio_memory ) ) );

    typedef boost::shared_ptr< rcsc::FreeformParser > FP;

    setFreeformParser( FP( new rcsc::FreeformParser( M_worldmodel ) ) );
    
    M_last_cycle_changed_formation = 0;
}

/*-------------------------------------------------------------------*/
/*!
  destructor
*/
GearPlayer::~GearPlayer()
{

}

/*-------------------------------------------------------------------*/
/*!

*/
bool
GearPlayer::initImpl( rcsc::CmdLineParser & cmd_parser )
{
    if ( ! rcsc::PlayerAgent::initImpl( cmd_parser ) )
    {
        M_client->setServerAlive( false );
        return false;
    }

    // read additional options
    Strategy::instance().init( cmd_parser );

    if ( cmd_parser.failed() )
    {
        std::cerr << "***WARNING*** detected invalid options: ";
        cmd_parser.print( std::cerr );
        std::cerr << std::endl;
    }

#if 1
    if ( config().teamName().compare( 0, 9, "Warthog2D" ) != 0 )
    {
        std::cerr << "***ERROR*** Illegal team name ["
                  << config().teamName() << "]"
                  << std::endl;
        M_client->setServerAlive( false );
        return false;
    }
#endif

    if ( config().goalie() )
    {
        std::cerr << config().teamName() << ": "
                  << "Savior mode " << ( Strategy::i().savior() ? "on" : "off" )
                  << std::endl;
    }

    if ( ! Strategy::instance().read( config().configDir() ) )
    {
        std::cerr << "***ERROR*** Failed to read team strategy." << std::endl;
        M_client->setServerAlive( false );
        return false;
    }

    if ( rcsc::KickTable::instance().read( config().configDir() + "/kick-table" ) )
    {
        std::cerr << "Loaded the kick table: ["
                  << config().configDir() << "/kick-table]"
                  << std::endl;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!
  main decision
  This is virtual in super.
*/
void
GearPlayer::actionImpl()
{
    // set defense line
    debugClient().addLine( rcsc::Vector2D( world().ourDefenseLineX(),
                                           world().self().pos().y - 2.0 ),
                           rcsc::Vector2D( world().ourDefenseLineX(),
                                           world().self().pos().y + 2.0 ) );
    // set offside line
    debugClient().addLine( rcsc::Vector2D( world().offsideLineX(),
                                           world().self().pos().y - 15.0 ),
                           rcsc::Vector2D( world().offsideLineX(),
                                           world().self().pos().y + 15.0 ) );
    
    // check for formation change order from coach
    if ( hearFormation () )
      std::cout << "Player "
		<< world().self().unum()
		<< " was ordered to change formation."
		<< std::endl;


    //
    // update strategy (formation, ...)
    //
    Strategy::instance().update( world() );

    //////////////////////////////////////////////////////////////
    // check tackle expires
    // check self position accuracy
    // ball search
    // check queued intention
    // check simultaneous kick
    if ( Bhv_PreProcess().execute( this ) )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": preprocess done" );
        return;
    }

    //////////////////////////////////////////////////////////////
    // create current role
    boost::shared_ptr< SoccerRole > role_ptr
        = Strategy::i().createRole( world().self().unum(),  world() );

    if ( ! role_ptr )
    {
        std::cerr << config().teamName() << ": "
                  << world().self().unum()
                  << " Error. Role is not registered.\nExit ..."
                  << std::endl;
        M_client->setServerAlive( false );
        return;
    }

    //////////////////////////////////////////////////////////////
    // play_on mode
    if ( world().gameMode().type() == rcsc::GameMode::PlayOn  )
    {
        role_ptr->execute( this );
        return;
    }

    rcsc::Vector2D home_pos = Strategy::i().getPosition( world().self().unum() );
    if ( ! home_pos.isValid() )
    {
        std::cerr << config().teamName() << ": "
                  << world().self().unum()
                  << " ***ERROR*** illegal home position."
                  << std::endl;
        home_pos.assign( 0.0, 0.0 );
    }

    debugClient().addLine( rcsc::Vector2D( home_pos.x, home_pos.y - 0.25 ),
                           rcsc::Vector2D( home_pos.x, home_pos.y + 0.25 ) );
    debugClient().addLine( rcsc::Vector2D( home_pos.x - 0.25, home_pos.y ),
                           rcsc::Vector2D( home_pos.x + 0.25, home_pos.y ) );
    debugClient().addCircle( home_pos, 0.5 );

    //////////////////////////////////////////////////////////////
    // kick_in or corner_kick
    if ( ( world().gameMode().type() == rcsc::GameMode::KickIn_
           || world().gameMode().type() == rcsc::GameMode::CornerKick_ )
         && world().ourSide() == world().gameMode().side() )
    {
        if ( world().self().goalie() )
        {
            Bhv_GoalieFreeKick().execute( this );
        }
        else
        {
            Bhv_SetPlayKickIn( home_pos ).execute( this );
        }
        return;
    }

    //////////////////////////////////////////////////////////////
    if ( world().gameMode().type() == rcsc::GameMode::BackPass_
         || world().gameMode().type() == rcsc::GameMode::IndFreeKick_ )
    {
        Bhv_SetPlayIndirectFreeKick( home_pos ).execute( this );
        return;
    }

    //////////////////////////////////////////////////////////////
    // penalty kick mode
    if ( world().gameMode().isPenaltyKickMode() )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": penalty kick" );
        Bhv_PenaltyKick().execute( this );
        return;
    }

    //////////////////////////////////////////////////////////////
    // goalie free kick mode
    if ( world().self().goalie() )
    {
        Bhv_GoalieFreeKick().execute( this );
        return;
    }

    //////////////////////////////////////////////////////////////
    // other set play mode

    Bhv_SetPlay( home_pos ).execute( this );
}

/*-------------------------------------------------------------------*/
/*!

*/
void
GearPlayer::handleServerParam()
{
    rcsc::KickTable::instance().createTables();
#if 0
    if ( config().playerNumber() == 1 )
    {
        rcsc::KickTable::instance().write( config().configDir() + "/kick-table" );
    }
#endif
}

/*-------------------------------------------------------------------*/
/*!

*/
void
GearPlayer::handlePlayerParam()
{

}

/*-------------------------------------------------------------------*/
/*!

*/
void
GearPlayer::handlePlayerType()
{

}

/*-------------------------------------------------------------------*/
/*!
  communication decision.
  virtual.method
*/
void
GearPlayer::communicationImpl()
{
    if ( ! config().useCommunication()
         || world().gameMode().type() == rcsc::GameMode::BeforeKickOff
         || world().gameMode().type() == rcsc::GameMode::AfterGoal_
         || world().gameMode().isPenaltyKickMode() )
    {
        return;
    }

    sayBall();
    sayDefenseLine();
    sayGoalie();
    sayIntercept();
    sayOffsideLine();
    saySelf();

    attentiontoSomeone();
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
GearPlayer::sayBall()
{
    // ball Info: seen at current
    const int len = effector().getSayMessageLength();
    if ( static_cast< int >( len + rcsc::BallMessage::slength() )
         > rcsc::ServerParam::i().playerSayMsgSize() )
    {
        return false;
    }

    if ( world().ball().posCount() > 0
         || world().ball().velCount() > 0 )
    {
        return false;
    }

    const rcsc::PlayerObject * ball_nearest_teammate = NULL;
    const rcsc::PlayerObject * second_ball_nearest_teammate = NULL;

    const rcsc::PlayerPtrCont::const_iterator t_end = world().teammatesFromBall().end();
    for ( rcsc::PlayerPtrCont::const_iterator t = world().teammatesFromBall().begin();
          t != t_end;
          ++t )
    {
        if ( (*t)->isGhost() || (*t)->posCount() >= 10 ) continue;

        if ( ! ball_nearest_teammate )
        {
            ball_nearest_teammate = *t;
        }
        else if ( ! second_ball_nearest_teammate )
        {
            second_ball_nearest_teammate = *t;
            break;
        }
    }

    int mate_min = world().interceptTable()->teammateReachCycle();
    int opp_min = world().interceptTable()->opponentReachCycle();

    bool send_ball = false;
    if ( world().self().isKickable()  // I am kickable
         || ( ! ball_nearest_teammate
              || ( ball_nearest_teammate->distFromBall()
                   > world().ball().distFromSelf() - 3.0 ) // I am nearest to ball
              || ( ball_nearest_teammate->distFromBall() < 6.0
                   && ball_nearest_teammate->distFromBall() > 1.0
                   && opp_min <= mate_min + 1 ) )
         || ( second_ball_nearest_teammate
              && ( ball_nearest_teammate->distFromBall()  // nearest to ball teammate is
                   > rcsc::ServerParam::i().visibleDistance() - 0.5 ) // over vis dist
              && ( second_ball_nearest_teammate->distFromBall()
                   > world().ball().distFromSelf() - 5.0 ) ) ) // I am second
    {
        send_ball = true;
    }

    if ( send_ball
         && static_cast< int >( len + rcsc::BallGoalieMessage::slength() )
         <= rcsc::ServerParam::i().playerSayMsgSize()
         && world().ball().pos().x > 34.0
         && world().ball().pos().absY() < 20.0 )
    {
        const rcsc::PlayerObject * opp_goalie = world().getOpponentGoalie();
        if ( opp_goalie
             && opp_goalie->posCount() == 0
             && opp_goalie->bodyCount() == 0
             && opp_goalie->unum() != rcsc::Unum_Unknown
             && opp_goalie->distFromSelf() < 25.0
             && opp_goalie->pos().x > 52.5 - 16.0
             && opp_goalie->pos().x < 52.5
             && opp_goalie->pos().absY() < 20.0 )
        {
            addSayMessage( new rcsc::BallGoalieMessage( effector().queuedNextBallPos(),
                                                        effector().queuedNextBallVel(),
                                                        opp_goalie->pos(),
                                                        opp_goalie->body() ) );
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": say ball/goalie status" );
            debugClient().addMessage( "SayG" );
            return true;
        }
    }

    if ( send_ball )
    {
        if ( world().self().isKickable()
             && effector().queuedNextBallKickable() )
        {
            // set ball velocity to 0.
            addSayMessage( new rcsc::BallMessage( effector().queuedNextBallPos(),
                                                  rcsc::Vector2D( 0.0, 0.0 ) ) );
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": say ball status. next cycle kickable." );
            debugClient().addMessage( "Say_ball-K" );
        }
        else
        {
            addSayMessage( new rcsc::BallMessage( effector().queuedNextBallPos(),
                                                  effector().queuedNextBallVel() ) );
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": say ball status" );
            debugClient().addMessage( "Say_ball" );
        }
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
GearPlayer::sayGoalie()
{
    // goalie info: ball is in chance area
    int len = effector().getSayMessageLength();
    if ( static_cast< int >( len + rcsc::GoalieMessage::slength() )
         > rcsc::ServerParam::i().playerSayMsgSize() )
    {
        return false;
    }

    if ( world().ball().pos().x < 34.0
         || world().ball().pos().absY() > 20.0 )
    {
        return false;
    }

    const rcsc::PlayerObject * opp_goalie = world().getOpponentGoalie();
    if ( opp_goalie
         && opp_goalie->posCount() == 0
         && opp_goalie->bodyCount() == 0
         && opp_goalie->unum() != rcsc::Unum_Unknown
         && opp_goalie->distFromSelf() < 25.0 )
    {
        addSayMessage( new rcsc::GoalieMessage( opp_goalie->unum(),
                                                opp_goalie->pos(),
                                                opp_goalie->body() ) );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": say goalie info: %d pos=(%.1f %.1f) body=%.1f",
                            opp_goalie->unum(),
                            opp_goalie->pos().x,
                            opp_goalie->pos().y,
                            opp_goalie->body().degree() );
        debugClient().addMessage( "Sayg" );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
GearPlayer::sayIntercept()
{
    // intercept info
    const int len = effector().getSayMessageLength();
    if ( static_cast< int >( len + rcsc::InterceptMessage::slength() )
         > rcsc::ServerParam::i().playerSayMsgSize() )
    {
        return false;
    }

    if ( world().ball().posCount() > 0
         || world().ball().velCount() > 0 )
    {
        return false;
    }

    int self_min = world().interceptTable()->selfReachCycle();
    int mate_min = world().interceptTable()->teammateReachCycle();
    int opp_min = world().interceptTable()->opponentReachCycle();

    if ( world().self().isKickable() )
    {
        double next_dist =  effector().queuedNextMyPos().dist( effector().queuedNextBallPos() );
        if ( next_dist > 10 )
        {
            self_min = 10000;
        }
        else
        {
            self_min = 0;
        }
    }

    if ( self_min <= mate_min
         && self_min <= opp_min
         && self_min <= 10 )
    {
        addSayMessage( new rcsc::InterceptMessage( true,
                                                   world().self().unum(),
                                                   self_min ) );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": say self intercept info %d",
                            self_min );
        debugClient().addMessage( "Sayi_self" );
        return true;
    }
//     else
//     {
//         {
//             const rcsc::PlayerObject * mate = world().interceptTable()->fastestTeammate();

//             if ( mate
//                  && mate->unum() != rcsc::Unum_Unknown
//                  && mate->posCount() <= 0
//                  && mate->distFromSelf() <= 15.0
//                  && mate_min <= 10
//                  && mate_min <= self_min + 1
//                  && mate_min <= opp_min + 1 )
//             {
//                 addSayMessage( new rcsc::InterceptMessage( true,
//                                                            mate->unum(),
//                                                            mate_min ) );
//                 rcsc::dlog.addText( rcsc::Logger::TEAM,
//                                     __FILE__": say teammate intercept info %d",
//                                     mate_min );
//                 debugClient().addMessage( "Sayi_our" );
//                 return true;
//             }
//         }
//         {
//             const rcsc::PlayerObject * opp = world().interceptTable()->fastestOpponent();
//             if ( opp
//                  && opp->unum() != rcsc::Unum_Unknown
//                  && opp->posCount() <= 0
//                  && opp->distFromSelf() <= 15.0
//                  && opp_min <= 10
//                  && opp_min <= self_min + 1
//                  && opp_min <= mate_min + 1 )
//             {
//                 addSayMessage( new rcsc::InterceptMessage( false,
//                                                            opp->unum(),
//                                                            opp_min ) );
//                 rcsc::dlog.addText( rcsc::Logger::TEAM,
//                                     __FILE__": say opponent intercept info %d",
//                                     opp_min );
//                 debugClient().addMessage( "Sayi_opp" );
//                 return true;
//             }
//         }
//     }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
GearPlayer::sayOffsideLine()
{
    const int len = effector().getSayMessageLength();
    if ( static_cast< int >( len + rcsc::OffsideLineMessage::slength() )
         > rcsc::ServerParam::i().playerSayMsgSize() )
    {
        return false;
    }

    if ( world().offsideLineCount() <= 1
         && world().opponentsFromSelf().size() >= 8
         && 0.0 < world().ball().pos().x
         && world().ball().pos().x < 37.0
         && world().ball().pos().x > world().offsideLineX() - 20.0
         && std::fabs( world().self().pos().x - world().offsideLineX() ) < 20.0
         )
    {
        addSayMessage( new rcsc::OffsideLineMessage( world().offsideLineX() ) );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": say offside line %.1f",
                            world().offsideLineX() );
        debugClient().addMessage( "Sayo" );
        return true;
    }

    return false;
}


/*-------------------------------------------------------------------*/
/*!

*/
bool
GearPlayer::sayDefenseLine()
{
    const int len = effector().getSayMessageLength();
    if ( static_cast< int >( len + rcsc::DefenseLineMessage::slength() )
         > rcsc::ServerParam::i().playerSayMsgSize() )
    {
        return false;
    }

    if ( std::fabs( world().self().pos().x - world().ourDefenseLineX() ) > 40.0 )
    {
        return false;
    }

    int opp_min = world().interceptTable()->opponentReachCycle();

    rcsc::Vector2D opp_trap_pos = world().ball().inertiaPoint( opp_min );

    if ( world().self().goalie()
         && -40.0 < opp_trap_pos.x
         && opp_trap_pos.x > 10.0 )
    {
        addSayMessage( new rcsc::DefenseLineMessage( world().ourDefenseLineX() ) );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": say defense line %.1f",
                            world().ourDefenseLineX() );
        debugClient().addMessage( "Sayd" );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
GearPlayer::saySelf()
{
    const int len = effector().getSayMessageLength();
    if ( static_cast< int >( len + rcsc::SelfMessage::slength() )
         > rcsc::ServerParam::i().playerSayMsgSize() )
    {
        return false;
    }

    if ( std::fabs( world().self().pos().x - world().ourDefenseLineX() ) > 40.0 )
    {
        return false;
    }

    bool send_self = false;

    if ( len > 0 )
    {
        send_self = true;
    }

    if ( ! send_self
        && world().time().cycle() % 3 == world().self().unum() % 3 )
    {
        const rcsc::PlayerObject * ball_nearest_teammate = NULL;
        const rcsc::PlayerObject * second_ball_nearest_teammate = NULL;

        const rcsc::PlayerPtrCont::const_iterator t_end = world().teammatesFromBall().end();
        for ( rcsc::PlayerPtrCont::const_iterator t = world().teammatesFromBall().begin();
              t != t_end;
              ++t )
        {
            if ( (*t)->isGhost() || (*t)->posCount() >= 10 ) continue;

            if ( ! ball_nearest_teammate )
            {
                ball_nearest_teammate = *t;
            }
            else if ( ! second_ball_nearest_teammate )
            {
                second_ball_nearest_teammate = *t;
                break;
            }
        }

        if ( ball_nearest_teammate
             && ball_nearest_teammate->distFromBall() < world().ball().distFromSelf()
             && ( ! second_ball_nearest_teammate
                  || second_ball_nearest_teammate->distFromBall() > world().ball().distFromSelf() )
             )
        {
            send_self = true;
        }
    }

    if ( send_self )
    {
//         addSayMessage( new rcsc::OnePlayerMessage( world().self().unum(),
//                                                    effector().queuedNextMyPos(),
//                                                    effector().queuedNextMyBody() ) );
        addSayMessage( new rcsc::SelfMessage( effector().queuedNextMyPos(),
                                              effector().queuedNextMyBody(),
                                              world().self().stamina() ) );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": say self status" );
        debugClient().addMessage( "Say_self" );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
GearPlayer::attentiontoSomeone()
{
    if ( world().self().pos().x > world().offsideLineX() - 15.0
         && world().interceptTable()->selfReachCycle() <= 3 )
    {
        std::vector< const rcsc::PlayerObject * > candidates;
        const rcsc::PlayerPtrCont::const_iterator end = world().teammatesFromSelf().end();
        for ( rcsc::PlayerPtrCont::const_iterator p = world().teammatesFromSelf().begin();
              p != end;
              ++p )
        {
            if ( (*p)->goalie() ) continue;
            if ( (*p)->unum() == rcsc::Unum_Unknown ) continue;
            if ( (*p)->pos().x > world().offsideLineX() + 1.0 ) continue;

            if ( (*p)->distFromSelf() > 20.0 ) break;

            candidates.push_back( *p );
        }

        const rcsc::PlayerObject * target_teammate = NULL;
        double max_x = -100.0;
        for ( std::vector< const rcsc::PlayerObject * >::const_iterator p = candidates.begin();
              p != candidates.end();
              ++p )
        {
            if ( (*p)->pos().x > max_x )
            {
                max_x = (*p)->pos().x;
                target_teammate = *p;
            }
        }

        if ( target_teammate )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": attentionto most front teammate",
                                world().offsideLineX() );
            debugClient().addMessage( "AttFrontMate" );
            doAttentionto( world().ourSide(), target_teammate->unum() );
            return;
        }

        // maybe ball owner
        if ( world().self().attentiontoUnum() > 0 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": attentionto off. maybe ball owner",
                                world().offsideLineX() );
            debugClient().addMessage( "AttOffBOwner" );
            doAttentiontoOff();
        }
        return;
    }

    {
        const rcsc::PlayerObject * fastest_teammate = world().interceptTable()->fastestTeammate();

        int self_min = world().interceptTable()->selfReachCycle();
        int mate_min = world().interceptTable()->teammateReachCycle();
        int opp_min = world().interceptTable()->opponentReachCycle();

        if ( ! fastest_teammate )
        {
            if ( world().self().attentiontoUnum() > 0 )
            {
                rcsc::dlog.addText( rcsc::Logger::TEAM,
                                    __FILE__": attentionto off. no fastest teammate",
                                    world().offsideLineX() );
                debugClient().addMessage( "AttOffNoMate" );
                doAttentiontoOff();
            }
            return;
        }

        if ( mate_min < self_min
             && mate_min <= opp_min
             && fastest_teammate->unum() != rcsc::Unum_Unknown )
        {
            // set attention to ball nearest teammate
            debugClient().addMessage( "AttBallOwner" );
            doAttentionto( world().ourSide(), fastest_teammate->unum() );
            return;
        }
    }

    {
        const rcsc::PlayerObject * nearest_teammate = world().getTeammateNearestToBall( 5 );
        if ( nearest_teammate
             && nearest_teammate->distFromBall() < 20.0
             && nearest_teammate->unum() != rcsc::Unum_Unknown )
        {
            debugClient().addMessage( "AttBallNearest" );
            doAttentionto( world().ourSide(), nearest_teammate->unum() );
            return;
        }
    }

    debugClient().addMessage( "AttOff" );
    doAttentiontoOff();
}

bool
GearPlayer::hearFormation ()
{
  int freeform_arrival_cycle = audioSensor().freeformMessageTime().cycle();
  if (M_last_cycle_changed_formation != freeform_arrival_cycle )
    {
      const char * freeform_msg = audioSensor().freeformMessage().c_str();
      std::stringstream freeform_stream ( freeform_msg );
      char msg_type_char;
      char formation_type_cstr[128];
      freeform_stream >> msg_type_char >> formation_type_cstr;
    
      std::string formation_type ( formation_type_cstr );
      if ( msg_type_char == 'f' )
	{
	  if ( !Strategy::instance ().SetFormationType (formation_type) )
	    {
	      std::cout << "***ERROR*** Invalid formation type "
			<< formation_type
			<< std::endl;
	      return false;
	    }
	  else if ( ! Strategy::instance().read( config().configDir() ) ) 
	    {
	      std::cout << "***ERROR*** Failed to read team strategy." << std::endl;
	      return false;
	    }
	}
      else
	return false;
    }
  else
    {
      return false;
    }

  M_last_cycle_changed_formation = freeform_arrival_cycle;

  return true;
}
