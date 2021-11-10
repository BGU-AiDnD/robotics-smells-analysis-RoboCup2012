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
#include "config.h"
#endif

#include "gear_coach.h"

#include <rcsc/coach/coach_command.h>
#include <rcsc/coach/coach_config.h>
#include <rcsc/common/basic_client.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/player_param.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_type.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/common/say_message_parser.h>
#include <rcsc/param/param_map.h>
#include <rcsc/param/cmd_line_parser.h>

#include <rcsc/player/world_model.h>
#include <rcsc/geom/vector_2d.h>

#include "swarm.h"
#include <cmath>
#include <ctime>
#include <cstdlib>

#include <cstdio>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <functional>

#include <fstream>
#include <string>

//#include "team_logo.xpm"

/////////////////////////////////////////////////////////

struct TypeStaminaIncComp
    : public std::binary_function< const rcsc::PlayerType *,
                                   const rcsc::PlayerType *,
                                   bool > {

    result_type operator()( first_argument_type lhs,
                            second_argument_type rhs ) const
      {
          return lhs->staminaIncMax() < rhs->staminaIncMax();
      }

};

/////////////////////////////////////////////////////////

struct RealSpeedMaxCmp
    : public std::binary_function< const rcsc::PlayerType *,
                                   const rcsc::PlayerType *,
                                   bool > {

    result_type operator()( first_argument_type lhs,
                            second_argument_type rhs ) const
      {
          if ( std::fabs( lhs->realSpeedMax() - rhs->realSpeedMax() ) < 0.005 )
          {
              return lhs->cyclesToReachMaxSpeed() < rhs->cyclesToReachMaxSpeed();
          }

          return lhs->realSpeedMax() > rhs->realSpeedMax();
      }

};

/////////////////////////////////////////////////////////

struct MaxSpeedReachStepCmp
    : public std::binary_function< const rcsc::PlayerType *,
                                   const rcsc::PlayerType *,
                                   bool > {

    result_type operator()( first_argument_type lhs,
                            second_argument_type rhs ) const
      {
          return lhs->cyclesToReachMaxSpeed() < rhs->cyclesToReachMaxSpeed();
      }

};

/*-------------------------------------------------------------------*/
/*!

*/

GearCoach::GearCoach()
    : rcsc::CoachAgent()
    , M_substitute_count( 0 )
{
    //
    // register audio memory & say message parsers
    //
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
    addSayMessageParser( SMP( new rcsc::BallPlayerMessageParser( audio_memory ) ) );
//     addSayMessageParser( SMP( new rcsc::FreeMessageParser< 9 >( audio_memory ) ) );
//     addSayMessageParser( SMP( new rcsc::FreeMessageParser< 8 >( audio_memory ) ) );
//     addSayMessageParser( SMP( new rcsc::FreeMessageParser< 7 >( audio_memory ) ) );
//     addSayMessageParser( SMP( new rcsc::FreeMessageParser< 6 >( audio_memory ) ) );
//     addSayMessageParser( SMP( new rcsc::FreeMessageParser< 5 >( audio_memory ) ) );
//     addSayMessageParser( SMP( new rcsc::FreeMessageParser< 4 >( audio_memory ) ) );
//     addSayMessageParser( SMP( new rcsc::FreeMessageParser< 3 >( audio_memory ) ) );
//     addSayMessageParser( SMP( new rcsc::FreeMessageParser< 2 >( audio_memory ) ) );
//     addSayMessageParser( SMP( new rcsc::FreeMessageParser< 1 >( audio_memory ) ) );

    //
    //
    //

    for ( int i = 0; i < 11; ++i )
    {
        // init map values
        M_assigned_player_type_id[i] = rcsc::Hetero_Default;
    }

    for ( int i = 0; i < 11; ++i )
    {
        M_opponent_player_types[i] = rcsc::Hetero_Default;
    }
    
    for ( int i = 0; i < 11; ++i )
    {
        M_teammate_recovery[i] = rcsc::ServerParam::i().recoverInit();
    }
    
    ball_possession_our = 0;
    ball_possession_opp = 0;
    last_counted_cycle_possession = 0;
    current_formation = "433_normal";
    need_to_change_formation = false;
    
    ballCounterTooClose = 0;
    ballCounterClose = 0;
    ballCounterDistant = 0;    
    ballPosition = 0;
    
    ballTooClose = false;
    ballClose = false;
    ballDistant = false;
    
    gameTimeBegin = false;
    gameTimeMiddle = false;
    gameTimeEnd = false;
    
    goalsDifference = 0;
    
    teamSufferedGoal = false;

    //psycho vars

    foulsCounter = 0;
    ourBall = 0;
    theirBall = 0;
    foul = false;
    currentCycle = 0;
    lastCycle = 0;
    weHaveBall = false ;
    weHaveKicked = false ;
    theyHaveBall = false ;
    theyHaveKicked = false;
    correctPasses = 0;
    incorrectPasses = 0;
    isAttacking = false;
    badAttacks = 0;
    isDefending = false;
    goodDefenses = 0;
    pfBallPos = 0.0;

    F442 = false;
    F433 = true;
    F4231 = false;
    Flight = false;
    Fnormal = true;
    Fhardcore = false;

    attackpct = 0;
    defensepct = 0;

}
/*-------------------------------------------------------------------*/
/*!

*/
GearCoach::~GearCoach()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
GearCoach::initImpl( rcsc::CmdLineParser & cmd_parser )
{
    //rcsc::ParamMap my_params;
    // cmd_parser.parse( my_params );

    if ( ! rcsc::CoachAgent::initImpl( cmd_parser ) )
    {
        //my_params.printHelp( std::cout );
        M_client->setServerAlive( false );
        return false;
    }

    if ( cmd_parser.failed() )
    {
        std::cerr << "coach: ***WARNING*** detected invalid options: ";
        cmd_parser.print( std::cerr );
        std::cerr << std::endl;
    }

    //////////////////////////////////////////////////////////////////
    // Add your code here.
    //////////////////////////////////////////////////////////////////

    /*if ( config().useTeamGraphic() )
    {
        if ( config().teamGraphicFile().empty() )
        {
            M_team_graphic.createXpmTiles( team_logo_xpm );
        }
        else
        {
            M_team_graphic.readXpmFile( config().teamGraphicFile().c_str() );
        }
    }*/

    return true;
}

/*-------------------------------------------------------------------*/
void
GearCoach::printVars ()
{
	ballPos = world().ball().pos();

	std::cout<<"...Variaveis..."<<std::endl;

	if(world().ourSide() == rcsc::LEFT)
	{
		std::cout<<"GearSim: "<<world().gameMode().scoreLeft() <<std::endl;
		std::cout<<"Opp: "<<world().gameMode().scoreRight() <<std::endl;

	}else{
		std::cout<<"GearSim: "<<world().gameMode().scoreRight() <<std::endl;
		std::cout<<"Opp: "<<world().gameMode().scoreLeft() <<std::endl;
	}

	std::cout<<"Ciclo: "<<world().time().cycle() <<std::endl;

	std::cout<<"Posicao X Bola: "<<ballPos.x<<std::endl;
	std::cout<<"\n "<<std::endl;
}
/*-------------------------------------------------------------------*/
/*!

*/
void
GearCoach::actionImpl()
{
    if ( world().time().cycle() == 0
         && config().useTeamGraphic()
         && M_team_graphic.tiles().size() != teamGraphicOKSet().size() )
    {
        sendTeamGraphic();
    }
    doSubstitute();
    sayPlayerTypes();
    //printVars ();
    //countBallPossession ();
    ballPositionCounter();
    lastCycle = world().time().cycle();

    //if(!world().gameMode().isServerCycleStoppedMode() || lastCycle != currentCycle) {          //avoid duplicated data)
    if(lastCycle > 0)
        psychoFuzzy();

    if ((badAttacks + ourScore) == 0)
        attackpct = 50;
    else
        attackpct = 100*(ourScore/(1 + badAttacks + ourScore) );
    if ((theirScore + goodDefenses) == 0)
        defensepct = 50;
    else 
        defensepct = 100*(goodDefenses/(1 + theirScore + goodDefenses) );
    //std::cout << "attack% = " << attackpct << std::endl << "defense% = " << defensepct << std::endl;

    if ( world().time().cycle() % 400 == 0 &&
         world().time().cycle() > 0 &&
         world().time().cycle() < 6000 &&
         world().gameMode().type() != rcsc::GameMode::BeforeKickOff )
    {
        decidePosition();
        decideBehavior (world().time().cycle(), attackpct, defensepct);
        decideFormation ();
    }

    if(need_to_change_formation == true)
    {
        if (changeFormation (fuzzy_formation) )
        {
            //std::cout << "Warthog2D coach: new formation : " << fuzzy_formation << std::endl;
            current_formation = fuzzy_formation;
        }
        else
        {
		  //std::cout << "Warthog2D coach: still changing formation : " << fuzzy_formation << std::endl;
            need_to_change_formation = true;
        }
    }

    //else std::cout << std::endl << "jogo parado" << std::endl;

    //if(world().gameMode().type() == rcsc::GameMode::FreeKickFault_)
        //std::cout << std::endl << "falta" << std::endl << std::endl << std::endl;
    
    //if ( world().time().cycle() % 50 == 0 && world().time().cycle() > 0 )
        //printPsycho();



     
//     if ( world().time().cycle() > 0 )
//     {
//         M_client->setServerAlive( false );
//     }
}


void
GearCoach::updateRecoveryInfo()
{
    const int half_time = rcsc::ServerParam::i().halfTime() * 10;
    const int normal_time = half_time * rcsc::ServerParam::i().nrNormalHalfs();

    if ( world().time().cycle() < normal_time
         && world().gameMode().type() == rcsc::GameMode::BeforeKickOff )
    {
        for ( int i = 0; i < 11; ++i )
        {
            M_teammate_recovery[i] = rcsc::ServerParam::i().recoverInit();
        }

        return;
    }

    if ( world().audioMemory().recoveryTime() != world().time() )
    {
        return;
    }

#if 0
    std::cerr << world().time() << ": heared recovery:\n";
    for ( std::vector< AudioMemory::Recovery >::const_iterator it = world().audioMemory().recovery().begin();
          it != world().audioMemory().recovery().end();
          ++it )
    {
        double recovery
            = it->rate_ * ( ServerParam::i().recoverInit() - ServerParam::i().recoverMin() )
            + ServerParam::i().recoverMin();

            std::cerr << "  sender=" << it->sender_
                      << " rate=" << it->rate_
                      << " value=" << recovery
                      << '\n';

    }
    std::cerr << std::flush;
#endif
    const std::vector< rcsc::AudioMemory::Recovery >::const_iterator end = world().audioMemory().recovery().end();
    for ( std::vector< rcsc::AudioMemory::Recovery >::const_iterator it = world().audioMemory().recovery().begin();
          it != end;
          ++it )
    {
        if ( 1 <= it->sender_
             && it->sender_ <= 11 )
        {
            double value
                = it->rate_ * ( rcsc::ServerParam::i().recoverInit() - rcsc::ServerParam::i().recoverMin() )
                + rcsc::ServerParam::i().recoverMin();
            // std::cerr << "coach: " << world().time() << " heared recovery: sender=" << it->sender_
            //           << " value=" << value << std::endl;
            M_teammate_recovery[ it->sender_ - 1 ] = value;
        }
    }
}


/*-------------------------------------------------------------------*/
/*!

*/
void
GearCoach::doSubstitute()
/*{
    static bool S_first_substituted = false;

    if ( config().useHetero()
         && ! S_first_substituted
         && world().time().cycle() == 0
         && world().time().stopped() > 10 )
    {
        doFirstSubstitute();
        S_first_substituted = true;
    }
}*/
{
    static bool S_first_substituted = false;

    if ( ! S_first_substituted
         && world().time().cycle() == 0
         && world().time().stopped() > 10 )
    {
        doFirstSubstitute();
        S_first_substituted = true;

        return;
    }

    if ( world().time().cycle() > 0
         && world().gameMode().type() != rcsc::GameMode::PlayOn
         && ! world().gameMode().isPenaltyKickMode() )
    {
        doSubstituteTiredPlayers();

        return;
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
GearCoach::doFirstSubstitute()
{
    PlayerTypePtrCont candidates;

    std::fprintf( stderr,
                  "id speed step inc  power  stam"
                  //" decay"
                  //" moment"
                  //" dprate"
                  "  karea"
                  //"  krand"
                  //" effmax effmin"
                  "\n" );

    for ( rcsc::PlayerTypeSet::PlayerTypeMap::const_iterator it
              = rcsc::PlayerTypeSet::i().playerTypeMap().begin();
          it != rcsc::PlayerTypeSet::i().playerTypeMap().end();
          ++it )
    {
        if ( it->second.id() == rcsc::Hetero_Default )
        {
            if ( rcsc::PlayerParam::i().allowMultDefaultType() )
            {
                for ( int i = 0; i <= rcsc::MAX_PLAYER; ++i )
                {
                    M_available_player_type_id.push_back( rcsc::Hetero_Default );
                    candidates.push_back( &(it->second) );
                }
            }
        }
        else
        {
            for ( int i = 0; i < rcsc::PlayerParam::i().ptMax(); ++i )
            {
                M_available_player_type_id.push_back( it->second.id() );
                candidates.push_back( &(it->second) );
            }
        }
        std::fprintf( stderr,
                      " %d %.3f  %2d  %.1f %5.1f %5.1f"
                      //" %.3f"
                      //"  %4.1f"
                      //"  %.5f"
                      "  %.3f"
                      //"  %.2f"
                      //"  %.3f  %.3f"
                      "\n",
                      it->second.id(),
                      it->second.realSpeedMax(),
                      it->second.cyclesToReachMaxSpeed(),
                      it->second.staminaIncMax(),
                      it->second.getDashPowerToKeepMaxSpeed(),
                      it->second.getOneStepStaminaComsumption(),
                      //it->second.playerDecay(),
                      //it->second.inertiaMoment(),
                      //it->second.dashPowerRate(),
                      it->second.kickableArea()
                      //it->second.kickRand(),
                      //it->second.effortMax(), it->second.effortMin()
                      );
    }


//     std::cerr << "total available types = ";
//     for ( PlayerTypePtrCont::iterator it = candidates.begin();
//           it != candidates.end();
//           ++it )
//     {
//         std::cerr << (*it)->id() << ' ';
//     }
//     std::cerr << std::endl;

    std::vector< int > unum_order;
    unum_order.reserve( 10 );

    unum_order.push_back( 11 );
    unum_order.push_back( 2 );
    unum_order.push_back( 3 );
    unum_order.push_back( 9 );
    unum_order.push_back( 10 );
    unum_order.push_back( 6 );
    unum_order.push_back( 4 );
    unum_order.push_back( 5 );
    unum_order.push_back( 7 );
    unum_order.push_back( 8 );

    for ( std::vector< int >::iterator unum = unum_order.begin();
          unum != unum_order.end();
          ++unum )
    {
        int type = getFastestType( candidates );
        if ( type != rcsc::Hetero_Unknown )
        {
            substituteTo( *unum, type );
        }
    }
}


void
GearCoach::doSubstituteTiredPlayers()
{
    if ( M_substitute_count >= rcsc::PlayerParam::i().subsMax() )
    {
        // over the maximum substitution
        return;
    }

    const rcsc::ServerParam & SP = rcsc::ServerParam::i();

    //
    // check game time
    //
    const int half_time = SP.halfTime() * 10;
    const int normal_time = half_time * SP.nrNormalHalfs();

    if ( //world().time().cycle() < normal_time - 500
         //|| world().time().cycle() <= half_time + 1
         //|| world().gameMode().type() == rcsc::GameMode::KickOff_ )
         world().time().cycle() <= 3000 )
    {
        return;
    }

//    std::cerr << "coach: try substitute. " << world().time() << std::endl;

    //
    // create candidate teamamte
    //
    std::vector< int > tired_teammate_unum;

    for ( int i = 0; i < 11; ++i )
    {
        if ( M_teammate_recovery[i] < rcsc::ServerParam::i().recoverInit() - 0.002 )
        {
            tired_teammate_unum.push_back( i + 1 );
        }
    }

    if ( tired_teammate_unum.empty() )
    {
  //      std::cerr << "coach: try substitute. " << world().time()
    //              << " no tired teammate."
      //            << std::endl;
        return;
    }

    //
    // create candidate player type
    //
    PlayerTypePtrCont candidates;

    for ( std::vector< int >::const_iterator id = M_available_player_type_id.begin();
          id !=  M_available_player_type_id.end();
          ++id )
    {
        const rcsc::PlayerType * ptype = rcsc::PlayerTypeSet::i().get( *id );
        if ( ! ptype )
        {
            std::cerr << config().teamName() << " coach "
                      << world().time()
                      << " : Could not get player type. id=" << *id << std::endl;
            continue;
        }

        candidates.push_back( ptype );
    }

    //
    // try substitution
    //
    for ( std::vector< int >::iterator unum = tired_teammate_unum.begin();
          unum != tired_teammate_unum.end();
          ++unum )
    {
        if ( M_substitute_count >= rcsc::PlayerParam::i().subsMax() )
        {
            // over the maximum substitution
            return;
        }

        int type = getFastestType( candidates );
        if ( type != rcsc::Hetero_Unknown )
        {
            substituteTo( *unum, type );
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GearCoach::substituteTo( const int unum,
                           const int type )
{
    if ( world().time().cycle() > 0
         && M_substitute_count >= rcsc::PlayerParam::i().subsMax() )
    {
        std::cerr << "***Warning*** "
                  << config().teamName() << " coach: over the substitution max."
                  << " cannot change the player " << unum
                  << " to type " << type
                  << std::endl;
        return;
    }

    std::vector< int >::iterator it = std::find( M_available_player_type_id.begin(),
                                                 M_available_player_type_id.end(),
                                                 type );
    if ( it == M_available_player_type_id.end() )
    {
        std::cerr << "***ERROR*** "
                  << config().teamName() << " coach: "
                  << " cannot change the player " << unum
                  << " to type " << type
                  << std::endl;
        return;
    }

    M_available_player_type_id.erase( it );
    M_assigned_player_type_id.insert( std::make_pair( unum, type ) );
    if ( world().time().cycle() > 0 )
    {
        ++M_substitute_count;
    }

    doChangePlayerType( unum, type );

    std::cout << config().teamName() << " coach: "
              << "change player " << unum
              << " to type " << type
              << std::endl;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
GearCoach::getFastestType( PlayerTypePtrCont & candidates )
{
    if ( candidates.empty() )
    {
        return rcsc::Hetero_Unknown;
    }

    // sort by max speed
    std::sort( candidates.begin(),
               candidates.end(),
               RealSpeedMaxCmp() );

//     std::cerr << "getFastestType candidate = ";
//     for ( PlayerTypePtrCont::iterator it = candidates.begin();
//           it != candidates.end();
//           ++it )
//     {
//         std::cerr << (*it)->id() << ' ';
//     }
//     std::cerr << std::endl;

    PlayerTypePtrCont::iterator best_type = candidates.begin();
    double max_speed = (*best_type)->realSpeedMax();
    int min_cycle = 100;
    for ( PlayerTypePtrCont::iterator it = candidates.begin();
          it != candidates.end();
          ++it )
    {
        if ( (*it)->realSpeedMax() < max_speed - 0.01 )
        {
            break;
        }

        if ( (*it)->cyclesToReachMaxSpeed() < min_cycle )
        {
            best_type = it;
            max_speed = (*best_type)->realSpeedMax();
            min_cycle = (*best_type)->cyclesToReachMaxSpeed();
            continue;
        }

        if ( (*it)->cyclesToReachMaxSpeed() == min_cycle )
        {
            if ( (*it)->getOneStepStaminaComsumption()
                 < (*best_type)->getOneStepStaminaComsumption())
            {
                best_type = it;
                max_speed = (*best_type)->realSpeedMax();
            }
        }
    }

    int id = (*best_type)->id();
    candidates.erase( best_type );
    return id;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
GearCoach::sayPlayerTypes()
{
    /*
      format:
      "(player_types (1 0) (2 1) (3 2) (4 3) (5 4) (6 5) (7 6) (8 -1) (9 0) (10 1) (11 2))"
      ->
      (say (freeform "(player_type ...)"))
    */

    static rcsc::GameTime s_last_send_time( 0, 0 );

    if ( ! config().useFreeform() )
    {
        return;
    }

    if ( ! world().canSendFreeform() )
    {
        return;
    }

    int analyzed_count = 0;

    for ( int unum = 1; unum <= 11; ++unum )
    {
        const int id = world().theirPlayerTypeId( unum );

        if ( id != M_opponent_player_types[unum - 1] )
        {
            M_opponent_player_types[unum - 1] = id;

            if ( id != rcsc::Hetero_Unknown )
            {
                ++analyzed_count;
            }
        }
    }

    if ( analyzed_count == 0 )
    {
        return;
    }

    std::string msg;
    msg.reserve( 128 );

    msg = "(player_types ";

    for ( int unum = 1; unum <= 11; ++unum )
    {
        char buf[8];
        snprintf( buf, 8, "(%d %d)",
                  unum, M_opponent_player_types[unum - 1] );
        msg += buf;
    }

    msg += ")";

    doSayFreeform( msg );

    s_last_send_time = world().time();

    std::cout << config().teamName()
              << " Coach: "
              << world().time()
              << " send freeform " << msg
              << std::endl;
}

/*-------------------------------------------------------------------*/


bool
GearCoach::changeFormation (std::string f)
{
  std::string msg;
  if ( f == "4231_normal" )
    msg = "f 4231_normal";
  else if ( f == "4231_light" )
    msg = "f 4231_light";
  else if ( f == "4231_hardcore" )
      msg = "f 4231_hardcore";
  else if ( f == "442_normal" )
    msg = "f 442_normal";
  else if ( f == "442_light" )
    msg = "f 442_light";
  else if ( f == "442_hardcore" )
    msg = "f 442_hardcore";
  else if ( f == "433_normal" )
    msg = "f 433_normal";
  else if ( f == "433_light" )
    msg = "f 433_light";
  else if ( f == "433_hardcore" )
    msg = "f 433_hardcore";
  else if ( f == "sweeper" )
    msg = "f sweeper";
  else
    return false;

  if ( world().canSendFreeform() )
    {
      doSayFreeform ( msg.c_str () );
      need_to_change_formation = false;
      return true;
    }
  else
  {
	  need_to_change_formation = true;
	  return false;
  }

}
/*!

*/
void
GearCoach::sendTeamGraphic()
{
    int count = 0;
    for ( rcsc::TeamGraphic::Map::const_reverse_iterator tile
              = M_team_graphic.tiles().rbegin();
          tile != M_team_graphic.tiles().rend();
          ++tile )
    {
        if ( teamGraphicOKSet().find( tile->first ) == teamGraphicOKSet().end() )
        {
            if ( ! doTeamGraphic( tile->first.first,
                                  tile->first.second,
                                  M_team_graphic ) )
            {
                break;
            }
            ++count;
        }
    }

    if ( count > 0 )
    {
        std::cout << config().teamName()
                  << " coach: "
                  << world().time()
                  << " send team_graphic " << count << " tiles"
                  << std::endl;
    }
}

void
GearCoach::chooseNewFormation ()
{
  blacklisted_formations.push_front (current_formation);
  //TODO - melhorar o método de seleção
  if (current_formation == "433_normal")
    {
      current_formation = "4231_normal";
    }
  else if (current_formation == "442_normal")
    {
      current_formation = "4231_normal";
    }
  else if (current_formation == "4231_normal")
      {
        current_formation = "433_normal";
      }
  else current_formation = "4231_normal";
}

void
GearCoach::countBallPossession ()
{
  static int last_cycle = world().time().cycle();
  const rcsc::GlobalPlayerObject * player_with_ball_possession
    = world().getPlayerNearestTo(world().ball().pos());
  if(last_cycle != world().time().cycle())
    {
      if(player_with_ball_possession != NULL)
	{
	  // checa se o jogador existe
	  if(player_with_ball_possession->side() == world().ourSide())
	    {
	      ball_possession_our++;
	    }
	  else if(player_with_ball_possession->side() == world().theirSide())
	    {
	      ball_possession_opp++;
	    }
	}
    }
}


void
GearCoach::decidePosition ()
{

  /*if ( !(world().time().cycle() % 600)
       && world().gameMode().type() != rcsc::GameMode::BeforeKickOff) // a cada 600 ciclos...
    {
      // zera as posses de bola
      ball_possession_our = 0;
      ball_possession_opp = 0;
      if ( ball_possession_our < ball_possession_opp ) // posse de bola do nosso time eh menor - trocar a formação
	{
	  chooseNewFormation ();
	  if(changeFormation (current_formation))
	    {
	      need_to_change_formation = false;
	      std::cout << "GEARSIM coach: changed formation instantly to "
			<< current_formation
			<< std::endl;
	    }
	  else
	    need_to_change_formation = true; //a formacao soh pode ser mudada com a bola parada
	}*/

    //if (isOurTeamLosing())		//changes formation if losing
    //{      //std::cout<<"teste1"<<std::endl;
    //  if (!(world().time().cycle() % 250)   && 				//conditions to change formation
//      	    world().gameMode().type() != rcsc::GameMode::BeforeKickOff)
      	//    world().time().cycle() > 0 	    && 
      	//    world().time().cycle() != 3000  &&
      	//    world().time().cycle() != 6000  &&
      	//    world().time().cycle() != 7000)
	//{	//std::cout<<"teste2"<<std::endl;
	 //   if (isOurTeamScoring())	;	//if current formation is scoring goals, even if losing, keep the formation.
	        //need_to_change_formation = false;
	        
	  //  else if ((scoring_formation == current_formation) && !(teamSufferedGoal));	//if current formation already scored, and didn't suffered goals
	    	//need_to_change_formation = false;
	   // else
	   // {
	    //    teamSufferedGoal = false;
	        //std::cout<<"teste4"<<std::endl;
	        //fuzzy( world().time().cycle(), averageBallPosition/timeCounter);
	        //fuzzy parameters : world().time().cycle() and averageBallPosition/timeCounter
	        
		
		
	    	/*if ( fuzzy_formation == current_formation && !need_to_change_formation)
	    	{
    				chooseNewFormation();
    				need_to_change_formation = true;
	  	    		changeFormation (current_formation);
	  	    		//std::cout << "GearSim coach: new formation : " << current_formation
	  			    //<< std::endl;
	    	}
	    	else
	    	{
	    	    need_to_change_formation = true;
		    changeFormation (fuzzy_formation);
		    //std::cout << "GearSim coach: new formation : " << fuzzy_formation
	  			//    << std::endl;
	  			    
	  	    current_formation = fuzzy_formation;  			    
    	    	}*/
    	//    }
//	}



//    }
//    else 
 //   {
    	//need_to_change_formation = false;
 //   	scoring_formation = "null";
  //  }
    
 //   if (need_to_change_formation )
  //  	  changeFormation (current_formation);
  
  
    fuzzyFormationChooser(world().time().cycle(), averageBallPosition/timeCounter);
}

/*!

*/

bool
GearCoach::isOurTeamLosing()
{

	int ourScore, opponentScore;

	if(world().ourSide() == rcsc::LEFT)
  	{
		ourScore = world().gameMode().scoreLeft();
		opponentScore = world().gameMode().scoreRight();
		
  	}else{
		ourScore = world().gameMode().scoreRight();
		opponentScore = world().gameMode().scoreLeft();
	}
	
	if (ourScore >= opponentScore)  return false;
			
	else return true;	

}
/*!

*/

bool
GearCoach::isOurTeamScoring()
{

	int ourScore, opponentScore, currentDifference;
	
	if(world().ourSide() == rcsc::LEFT)
  	{
		ourScore = world().gameMode().scoreLeft();
		opponentScore = world().gameMode().scoreRight();
		
  	}else{
		ourScore = world().gameMode().scoreRight();
		opponentScore = world().gameMode().scoreLeft();
	}
	
	currentDifference = ourScore - opponentScore;
	
	if (goalsDifference < currentDifference)
	{
		scoring_formation = current_formation;
		goalsDifference = currentDifference;
		return true;
	}
	else if (goalsDifference > currentDifference)
	{
		teamSufferedGoal = true;
		goalsDifference = currentDifference;
		return false;	
	}
	else return false;

}

void
GearCoach::fuzzyFormationChooser(int t, int x)
{
	int FT;

	switch (int(t/250))

	    {

	    case 1:
	       {int t_1[]={1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,1,1,1,3,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
	       FT = t_1[x];
	       break;}

	    case 2:
	       {int t_2[]={1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
	    	FT = t_2[x];
	       break;}

	    case 3:
	       {int t_3[]={1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
	       FT = t_3[x];
	       break;}

	    case 4:
	    	{int t_4[]={1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
	    	FT = t_4[x];
	       break;}

	    case 5:
			{int t_5[]={1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
	    	 FT = t_5[x];
	    	 break;}

	    case 6:
			{int t_6[]={1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
	    	 FT = t_6[x];
	    	 break;}

	    case 7:
			{int t_7[]={1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
	    	 FT = t_7[x];
	    	 break;}

	    case 8:
			{int t_8[]={1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
	    	 FT = t_8[x];
	    	 break;}

	    case 9:
			{int t_9[]={1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
	    	 FT = t_9[x];
	    	 break;}

	    case 10:
			{int t_10[]={1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
	    	 FT = t_10[x];
	    	 break;}

	    case 11:
			{int t_11[]={1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
	    	 FT = t_11[x];
	    	 break;}

	    case 12:
			{int t_12[]={1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
	    	 FT = t_12[x];
	    	 break;}

	    case 13:
			{int t_13[]={2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
	    	 FT = t_13[x];
	    	 break;}

	    case 14:
			{int t_14[]={2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
	    	 FT = t_14[x];
	    	 break;}

	    case 15:
			{int t_15[]={2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
	    	 FT = t_15[x];
	    	 break;}

	    case 16:
			{int t_16[]={2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
	    	 FT = t_16[x];
	    	 break;}

	    case 17:
			{int t_17[]={2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
	    	 FT = t_17[x];
	    	 break;}

	    case 18:
			{int t_18[]={2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
	    	 FT = t_18[x];
	    	 break;}

	    case 19:
			{int t_19[]={2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
	    	 FT = t_19[x];
	    	 break;}

	    case 20:
			{int t_20[]={2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
	    	 FT = t_20[x];
	    	 break;}

	    case 21:
			{int t_21[]={2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
	    	 FT = t_21[x];
	    	 break;}

	    case 22:
			{int t_22[]={2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
	    	 FT = t_22[x];
	    	 break;}

	    case 23:
			{int t_23[]={2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3};
	    	 FT = t_23[x];
	    	 break;}

	    case 24:
			{int t_24[]={2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3};
	    	 FT = t_24[x];
	    	 break;}
	    }

	if (FT==2)
        {
            //fuzzy_formation = "442_normal";
            //std::cout << "Warthog2D coach chooses formation 442 !" << std::endl;
            F442 = true;
            F433 = false;
            F4231 = false;
        }	
	if (FT==1)
        {
            //fuzzy_formation = "4231_normal";
            //std::cout << "Warthog2D coach chooses formation 4231 !" << std::endl;
            F442 = false;
            F433 = false;
            F4231 = true;
        }
	
	if (FT==3)
        {
            //fuzzy_formation = "433_normal";
            //std::cout << "Warthog2D coach chooses formation 433 !" << std::endl;
            F442 = false;
            F433 = true;
            F4231 = false;
        }
		
}
/*!

*/
void
GearCoach::ballPositionCounter()
{	
	//std::cout << last_cycle << std::endl;
	
    if (world().gameMode().type() != rcsc::GameMode::BeforeKickOff &&
        world().gameMode().type() != rcsc::GameMode::AfterGoal_)
    {
    	if(world().ourSide() == rcsc::LEFT)
  	{
		if (world().ball().pos().x < -17.5)
			ballCounterTooClose++;
		
		else if (world().ball().pos().x > 17.5)	
			ballCounterDistant++;
			
		else ballCounterClose++;	
	}
	else
	if(world().ourSide() == rcsc::RIGHT)
  	{
		if (world().ball().pos().x < -17.5)
			ballCounterDistant++;
		
		else if (world().ball().pos().x > 17.5)	
			ballCounterTooClose++;
			
		else ballCounterClose++;	
	}
	
	if ( world().time().cycle() != 0 && (last_cycle != world().time().cycle()) )
	{
		if(world().ourSide() == rcsc::LEFT)
			ballPosition = (52.5 + world().ball().pos().x);
		else  ballPosition = fabs((-52.5 + world().ball().pos().x));
    
    		last_cycle = world().time().cycle();
    		timeCounter++;
    		averageBallPosition += ballPosition;
    		//std::cout << ballPosition << "  -   "<< world().time().cycle() << "    -    " << averageBallPosition/timeCounter << std::endl;	
    	}
    }
}
/*!

*/
void
GearCoach::ballPositionOverTime()
{	
	if (ballCounterTooClose > ballCounterClose && ballCounterTooClose > ballCounterDistant)		//checking ball position over time
	{
		ballTooClose = true;
		ballClose = false;
		ballDistant = false;
	}
		
	else if (ballCounterClose > ballCounterTooClose && ballCounterClose > ballCounterDistant)
	{
		ballTooClose = false;
		ballClose = true;
		ballDistant = false;
	}
			
	else 
	{
		ballTooClose = false;
		ballClose = false;
		ballDistant = true;
	}	
}
/*!

*/
void
GearCoach::gameTime()
{	
	if (world().time().cycle() <  1300)
		gameTimeBegin = true;
	else if (world().time().cycle() >  4700)
	{
		gameTimeBegin = false;
		gameTimeMiddle = false;
		gameTimeEnd = true;
	}
	else 
	{
		gameTimeBegin = false;
		gameTimeMiddle = true;
		gameTimeEnd = false;	
	}
}
/*!

*/
void
GearCoach::psychoFuzzy()
{
    currentCycle == world().time().cycle();

    //getting game score
    if(world().ourSide() == rcsc::LEFT)
    {
        ourScore = world().gameMode().scoreLeft();
	    theirScore = world().gameMode().scoreRight();
    }
    else
    {
	ourScore = world().gameMode().scoreRight();
	theirScore = world().gameMode().scoreLeft();
    }

    //finding nearest player to ball
    int i;
    
    const rcsc::GlobalPlayerObject * player;
    
    //player = world().teammate(1);
    player = world().getPlayerNearestTo(world().ball().pos());
       
    //number of fouls

   /* if(world().gameMode().type() == rcsc::GameMode::FreeKickFault_)
    {
        if(player->side() == world().ourSide())
            foulsCounter++;
    }*/

    //ball possesion

    if(player->side() ==  world().ourSide()) ourBall++;

    else theirBall++;
  
    //correct passes && unsuccessful attacks and defenses

    if(!weHaveKicked && world().existKickablePlayer() &&
       player->side() ==  world().ourSide() )
        weHaveBall = true;

    if(weHaveBall && !world().existKickablePlayer())
        weHaveKicked = true;

    if(weHaveKicked && world().existKickablePlayer() )
    {
        if(player->side() ==  world().ourSide() )
            correctPasses++;
        else
        {
            incorrectPasses++;
            badAttacks++;
        }
        weHaveBall = false;
        weHaveKicked = false;
    }

    if(!theyHaveKicked && world().existKickablePlayer() &&
       player->side() ==  world().theirSide() )
        theyHaveBall = true;

    if(theyHaveBall && !world().existKickablePlayer())
        theyHaveKicked = true;

    if(theyHaveKicked && world().existKickablePlayer() )
    {
        if(player->side() ==  world().ourSide() )
            goodDefenses++;

        theyHaveBall = false;
        theyHaveKicked = false;
    }

    //ball position over time

    //pfBallPos += (world().ball().pos().x + 52.5);
}
/*!

*/
void
GearCoach::printPsycho()
{
    std::cout << "tempo de partida: " << world().time().cycle() << std::endl;
    std::cout << "faltas sofridas: " << foulsCounter << std::endl;
    std::cout << "% posse de bola: " << 100*(ourBall/(ourBall + theirBall)) << std::endl;
    if((correctPasses + incorrectPasses) == 0)
        std::cout << "nenhum passe feito!" << std::endl;
    else std::cout << "% passes certos: " << 100*(correctPasses/(correctPasses + incorrectPasses)) << std::endl;
    if((badAttacks + ourScore) == 0)
        std::cout << "% ataques mal-sucedidos: 0" << std::endl;
    else std::cout << "% ataques mal-sucedidos: " << 100*(badAttacks/(badAttacks + ourScore) )<< std::endl;
    if((theirScore + goodDefenses) == 0)
        std::cout << "% defesas bem-sucedidas: 0" << std::endl;
    else std::cout << "% defesas bem-sucedidas: " << 100*(goodDefenses/(theirScore + goodDefenses) )<< std::endl;
    std::cout << "posição média da bola: " << pfBallPos/250 << std::endl;

    //pfBallPos = 0.0;
}
/*!

*/
void
GearCoach::decideBehavior (int time, double attack, double defense)
{
    int defuzzy;
    int a,d,t;

    a = abs((attack-1)/10);
    d = ((defense-1)/10);
    t = world().time().cycle()/250;

    switch (t)
    {
        case 0:{
           int pf1[]={78,78,78,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      50,50,50,50,50,50,50,50,50,50,
                      50,50,50,50,50,50,50,50,50,50};

            defuzzy = pf1[a*10 + d];
            break;}
        case 1:{
           int pf1[]={78,78,78,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      50,50,50,50,50,50,50,50,50,50,
                      50,50,50,50,50,50,50,50,50,50};

            defuzzy = pf1[a*10 + d];
            break;}
        case 2: {
            int pf2[]={78,78,78,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      50,50,50,50,50,50,50,50,50,50,
                      50,50,50,50,50,50,50,50,50,50};
            
            defuzzy = pf2[a*10 + d];
            break; }
        case 3: {
            int pf3[]={78,78,78,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      50,50,50,50,50,50,50,50,50,50,
                      50,50,50,50,50,50,50,50,50,50};
            
            defuzzy = pf3[a*10 + d];
            break; }
        case 4: {
            int pf4[]={78,78,78,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      50,50,50,50,50,50,50,50,50,50,
                      50,50,50,50,50,50,50,50,50,50};

            defuzzy = pf4[a*10 + d];
            break; }
        case 5: {
            int pf5[]={78,78,78,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      50,50,50,50,50,50,50,50,50,50,
                      50,50,50,50,50,50,50,50,50,50};

            defuzzy = pf5[a*10 + d];
            break; }
        case 6: {
            int pf6[]={78,78,78,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      50,50,50,50,50,50,50,50,50,50,
                      50,50,50,50,50,50,50,50,50,50};

            defuzzy = pf6[a*10 + d];
            break; }
        case 7: {
            int pf7[]={78,78,78,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      50,50,50,50,50,50,50,50,50,50,
                      50,50,50,50,50,50,50,50,50,50};

            defuzzy = pf7[a*10 + d];
            break; }
        case 8: {
            int pf8[]={78,78,78,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      50,50,50,50,50,50,50,50,50,50,
                      50,50,50,50,50,50,50,50,50,50};

            defuzzy = pf8[a*10 + d];
            break; }
        case 9: {
            int pf9[]={78,78,78,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      50,50,50,50,50,50,50,50,50,50,
                      50,50,50,50,50,50,50,50,50,50};

            defuzzy = pf9[a*10 + d];
            break; }
        case 10: {
            int pf10[]={78,78,78,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      50,50,50,50,50,50,50,50,50,50,
                      50,50,50,50,50,50,50,50,50,50};

            defuzzy = pf10[a*10 + d];
            break; }
        case 11: {
            int pf11[]={78,78,78,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      50,50,50,50,50,50,50,50,50,50,
                      50,50,50,50,50,50,50,50,50,50};

            defuzzy = pf11[a*10 + d];
            break; }
        case 12: {
            int pf12[]={78,78,78,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      50,50,50,50,50,50,50,50,50,50,
                      50,50,50,50,50,50,50,50,50,50};

            defuzzy = pf12[a*10 + d];
            break; }
        case 13: {
            int pf13[]={78,78,78,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      50,50,50,50,50,50,50,50,50,50,
                      50,50,50,50,50,50,50,50,50,50};

            defuzzy = pf13[a*10 + d];
            break; }
        case 14: {
            int pf14[]={78,78,78,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      50,50,50,50,50,50,50,50,50,50,
                      50,50,50,50,50,50,50,50,50,50};

            defuzzy = pf14[a*10 + d];
            break; }
        case 15: {
            int pf15[]={78,78,78,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      50,50,50,50,50,50,50,50,50,50,
                      50,50,50,50,50,50,50,50,50,50};

            defuzzy = pf15[a*10 + d];
            break; }
        case 16: {
            int pf16[]={78,78,78,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      50,50,50,50,50,50,50,50,50,50,
                      50,50,50,50,50,50,50,50,50,50};

            defuzzy = pf16[a*10 + d];
            break; }
        case 17: {
            int pf17[]={78,78,78,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      50,50,50,50,50,50,50,50,50,50,
                      50,50,50,50,50,50,50,50,50,50};

            defuzzy = pf17[a*10 + d];
            break; }
        case 18: {
            int pf18[]={78,78,78,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      50,50,50,50,50,50,50,50,50,50,
                      50,50,50,50,50,50,50,50,50,50};

            defuzzy = pf18[a*10 + d];
            break; }
        case 19: {
            int pf19[]={78,78,78,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      50,50,50,50,50,50,50,50,50,50,
                      50,50,50,50,50,50,50,50,50,50};

            defuzzy = pf19[a*10 + d];
            break; }
        case 20: {
            int pf20[]={77,77,77,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      51,51,51,51,51,51,51,51,51,51,
                      50,50,50,50,50,50,50,50,50,50};

            defuzzy = pf20[a*10 + d];
            break; }
        case 21: {
            int pf21[]={77,77,77,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      51,51,51,51,51,51,51,51,51,51,
                      51,51,51,51,51,51,51,51,51,51,};

            defuzzy = pf21[a*10 + d];
            break; }
        case 22: {
            int pf22[]={77,77,77,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      51,51,51,51,51,51,51,51,51,51,
                      51,51,51,51,51,51,51,51,51,51,};

            defuzzy = pf22[a*10 + d];
            break; }
        case 23: {
            int pf23[]={77,77,77,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      51,51,51,51,51,51,51,51,51,51,
                      51,51,51,51,51,51,51,51,51,51,};

            defuzzy = pf23[a*10 + d];
            break; }
        default: {
            int pf24[]={77,77,77,77,77,76,76,75,75,74,
                      73,73,73,73,73,72,72,71,70,69,
                      69,69,69,69,69,69,68,67,67,66,
                      65,65,65,65,65,65,65,65,64,63,
                      61,61,61,61,61,61,61,61,61,61,
                      57,57,57,57,57,57,57,57,57,57,
                      53,53,53,53,53,53,53,53,53,53,
                      51,51,51,51,51,51,51,51,51,51,
                      51,51,51,51,51,51,51,51,51,51,
                      51,51,51,51,51,51,51,51,51,51,};

            defuzzy = pf24[a*10 + d];
            break; }
    }

    //std::cout << "defuzzy = " << defuzzy << std::endl;

    if (defuzzy <= 52)
    {
	  //std::cout << "Warthog2D coach chooses behavior light !" << std::endl;
        Flight =  true;
        Fnormal = false;
        Fhardcore = false;
    }
    else if (defuzzy >= 65)
    {
	  //std::cout << "Warthog2D coach chooses behavior hardcore !" << std::endl;
        Flight =  false;
        Fnormal = false;
        Fhardcore = true;
    }
    else
    {
	  //std::cout << "Warthog2D coach chooses behavior normal !" << std::endl;
        Flight =  false;
        Fnormal = true;
        Fhardcore = false;
    }
}
/*!

*/
void
GearCoach::decideFormation ()
{
    if(F442 && Flight) fuzzy_formation = "442_light";
    else if(F442 && Fnormal) fuzzy_formation = "442_normal";
    else if(F442 && Fhardcore) fuzzy_formation = "442_hardcore";
    else if(F433 && Flight) fuzzy_formation = "433_light";
    else if(F433 && Fnormal) fuzzy_formation = "433_normal";
    else if(F433 && Fhardcore) fuzzy_formation = "433_hardcore";
    else if(F4231 && Flight) fuzzy_formation = "4231_light";
    else if(F4231 && Fnormal) fuzzy_formation = "4231_normal";
    else if(F4231 && Fhardcore) fuzzy_formation = "4231_hardcore";
    //else fuzzy_formation = "sweeper";

    if(fuzzy_formation == current_formation)
    {
        need_to_change_formation = false;
        //std::cout << "WRSIM coach: maintain current formation ! ("<< current_formation << ")" << std::endl;
    }

    else
    {
        need_to_change_formation = true;
        if (changeFormation (fuzzy_formation) )
        {
            //std::cout << "WRSIM coach: new formation : " << fuzzy_formation << std::endl;
            current_formation = fuzzy_formation;
        }
        //else std::cout << "WRSIM coach: still changing formation : " << fuzzy_formation << std::endl;
    }
}
