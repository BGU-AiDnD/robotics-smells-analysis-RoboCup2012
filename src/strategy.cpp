// -*-c++-*-

/*!
  \file strategy.cpp
  \brief team strategh Source File
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
#include "config.h"
#endif

#include "strategy.h"

#include "role_goalie.h"
#include "role_side_back.h"
#include "role_center_back.h"
#include "role_defensive_half.h"
#include "role_center_half.h"
#include "role_offensive_half.h"
#include "role_side_half.h"
#include "role_side_forward.h"
#include "role_center_forward.h"
#include "role_attacker.h"
#include "role_side_attacker.h"
#include "role_Lside_back.h"
#include "role_Lcenter_back.h"
#include "role_Ldefensive_half.h"
#include "role_Lcenter_half.h"
#include "role_Loffensive_half.h"
#include "role_Lside_half.h"
#include "role_Lside_forward.h"
#include "role_Lcenter_forward.h"
#include "role_Lattacker.h"
#include "role_Lside_attacker.h"
#include "role_HCside_back.h"
#include "role_HCcenter_back.h"
#include "role_HCdefensive_half.h"
#include "role_HCcenter_half.h"
#include "role_HCoffensive_half.h"
#include "role_HCside_half.h"
#include "role_HCside_forward.h"
#include "role_HCcenter_forward.h"
#include "role_HCattacker.h"
#include "role_HCside_attacker.h"

#include "role_sample.h"
#include "soccer_role.h"

#include "mark_table.h"

#include <rcsc/formation/formation_dt.h>
#include <rcsc/formation/formation_static.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/world_model.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/param/cmd_line_parser.h>
#include <rcsc/param/param_map.h>
#include <rcsc/game_mode.h>

#include <set>
#include <fstream>
#include <iostream>
#include <cstdio>

const std::string Strategy::BEFORE_KICK_OFF_CONF = "before-kick-off.conf";
const std::string Strategy::GOAL_KICK_OPP_CONF = "goal-kick-opp.conf";
const std::string Strategy::GOAL_KICK_OUR_CONF = "goal-kick-our.conf";
const std::string Strategy::GOALIE_CATCH_OPP_CONF = "goalie-catch-opp.conf";
const std::string Strategy::GOALIE_CATCH_OUR_CONF = "goalie-catch-our.conf";
//const std::string Strategy::GOAL_KICK_OPP_FORMATION_CONF = "goal-kick-opp.conf";
//const std::string Strategy::GOAL_KICK_OUR_FORMATION_CONF = "goal-kick-our.conf";
//const std::string Strategy::GOALIE_CATCH_OPP_FORMATION_CONF = "goalie-catch-opp.conf";
//const std::string Strategy::GOALIE_CATCH_OUR_FORMATION_CONF = "goalie-catch-our.conf";

const std::string Strategy::NORMAL_FORMATION_CONF = "normal-formation.conf";
const std::string Strategy::DEFENSE_FORMATION_CONF = "defense-formation.conf";
const std::string Strategy::OFFENSE_FORMATION_CONF = "offense-formation.conf";
const std::string Strategy::GOALIE_FORMATION_CONF = "goalie-formation.conf";

const std::string Strategy::KICKIN_OUR_FORMATION_CONF = "kickin-our-formation.conf";
const std::string Strategy::SETPLAY_OUR_FORMATION_CONF = "setplay-our-formation.conf";
const std::string Strategy::SETPLAY_OPP_FORMATION_CONF = "setplay-opp-formation.conf";
const std::string Strategy::INDIRECT_FREEKICK_OUR_FORMATION_CONF = "indirect-freekick-our-formation.conf";
const std::string Strategy::INDIRECT_FREEKICK_OPP_FORMATION_CONF = "indirect-freekick-opp-formation.conf";

/*-------------------------------------------------------------------*/
/*!

*/
Strategy::Strategy()
    : M_current_situation( Normal_Situation )
    , M_position_types( 11, Position_Center )
    , M_positions( 11 )
    , M_mark_table( new MarkTable() )
    , M_opponent_offense_strategy( No_Strategy )
    , M_opponent_defense_strategy( No_Strategy )
    , M_savior( true )
{
    M_role_factory[RoleSample::name()] = &RoleSample::create;

#if 0
    M_role_factory[RoleGoalie::name()] = &RoleGoalie::create;
    M_role_factory[RoleCenterBack::name()] = &RoleSample::create;
    M_role_factory[RoleSideBack::name()] = &RoleSample::create;
    M_role_factory[RoleDefensiveHalf::name()] = &RoleSample::create;
    M_role_factory[RoleOffensiveHalf::name()] = &RoleSample::create;
    M_role_factory[RoleSideForward::name()] = &RoleSample::create;
    M_role_factory[RoleCenterForward::name()] = &RoleSample::create;
#else
    M_role_factory[RoleGoalie::name()] = &RoleGoalie::create;
    M_role_factory[RoleCenterBack::name()] = &RoleCenterBack::create;
    M_role_factory[RoleSideBack::name()] = &RoleSideBack::create;
    M_role_factory[RoleDefensiveHalf::name()] = &RoleDefensiveHalf::create;
    M_role_factory[RoleCenterHalf::name()] = &RoleCenterHalf::create;
    M_role_factory[RoleOffensiveHalf::name()] = &RoleOffensiveHalf::create;
    M_role_factory[RoleSideHalf::name()] = &RoleSideHalf::create;
    M_role_factory[RoleSideForward::name()] = &RoleSideForward::create;
    M_role_factory[RoleCenterForward::name()] = &RoleCenterForward::create;
    M_role_factory[RoleAttacker::name()] = &RoleAttacker::create;
    M_role_factory[RoleSideAttacker::name()] = &RoleSideAttacker::create;
    M_role_factory[RoleLCenterBack::name()] = &RoleLCenterBack::create;
    M_role_factory[RoleLSideBack::name()] = &RoleLSideBack::create;
    M_role_factory[RoleLDefensiveHalf::name()] = &RoleLDefensiveHalf::create;
    M_role_factory[RoleLCenterHalf::name()] = &RoleLCenterHalf::create;
    M_role_factory[RoleLOffensiveHalf::name()] = &RoleLOffensiveHalf::create;
    M_role_factory[RoleLSideHalf::name()] = &RoleLSideHalf::create;
    M_role_factory[RoleLSideForward::name()] = &RoleLSideForward::create;
    M_role_factory[RoleLCenterForward::name()] = &RoleLCenterForward::create;
    M_role_factory[RoleLAttacker::name()] = &RoleLAttacker::create;
    M_role_factory[RoleLSideAttacker::name()] = &RoleLSideAttacker::create;
    M_role_factory[RoleHCCenterBack::name()] = &RoleHCCenterBack::create;
    M_role_factory[RoleHCSideBack::name()] = &RoleHCSideBack::create;
    M_role_factory[RoleHCDefensiveHalf::name()] = &RoleHCDefensiveHalf::create;
    M_role_factory[RoleHCCenterHalf::name()] = &RoleHCCenterHalf::create;
    M_role_factory[RoleHCOffensiveHalf::name()] = &RoleHCOffensiveHalf::create;
    M_role_factory[RoleHCSideHalf::name()] = &RoleHCSideHalf::create;
    M_role_factory[RoleHCSideForward::name()] = &RoleHCSideForward::create;
    M_role_factory[RoleHCCenterForward::name()] = &RoleHCCenterForward::create;
    M_role_factory[RoleHCAttacker::name()] = &RoleHCAttacker::create;
    M_role_factory[RoleHCSideAttacker::name()] = &RoleHCSideAttacker::create;
#endif

    M_formation_factory[rcsc::FormationDT::name()] = &rcsc::FormationDT::create;
    M_formation_factory[rcsc::FormationStatic::name()] = &rcsc::FormationStatic::create;
    
    M_formation_type = "433_normal";
}

/*-------------------------------------------------------------------*/
/*!

*/
Strategy &
Strategy::instance()
{
    static Strategy s_instance;
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
Strategy::init( rcsc::CmdLineParser & cmd_parser )
{
    rcsc::ParamMap param_map( "HELIOS extentions" );
    param_map.add()
        ( "savior", "", &M_savior, "Savior is used as the goalie player." );


    //
    //
    //

    cmd_parser.parse( param_map );
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Strategy::read( const std::string & config_dir )
{
    static bool s_initialized = false;

    if ( s_initialized )
    {
        std::cerr << __FILE__ << ' ' << __LINE__ << ": already initialized."
                  << std::endl;
        return false;
    }

    std::string configpath = config_dir;
    if ( ! configpath.empty()
         && configpath[ configpath.length() - 1 ] != '/' )
    {
        configpath += '/';
    }

    // before kick off
    M_before_kick_off_formation = readFormation( configpath + M_formation_type + '/' + BEFORE_KICK_OFF_CONF );
    if ( ! M_before_kick_off_formation )
    {
        std::cerr << "Failed to read before_kick_off formation" << std::endl;
        return false;
    }

    ///////////////////////////////////////////////////////////
    M_normal_formation = readFormation( configpath + M_formation_type + '/' + NORMAL_FORMATION_CONF);
    if ( ! M_normal_formation )
    {
        std::cerr << "Failed to read normal formation" << std::endl;
        return false;
    }
    
    ///////////////////////////////////////////////////////////
    M_defense_formation = readFormation( configpath + M_formation_type + '/' + DEFENSE_FORMATION_CONF);
    if ( ! M_defense_formation )
    {
        std::cerr << "Failed to read defense formation" << std::endl;
        return false;
    }
    
    ///////////////////////////////////////////////////////////
    M_offense_formation = readFormation( configpath + M_formation_type + '/' + OFFENSE_FORMATION_CONF);
    if ( ! M_offense_formation )
    {
        std::cerr << "Failed to read offense formation" << std::endl;
        return false;
    }

    //////////////////////////////////////////////

    M_goalie_formation = readFormation( configpath + M_formation_type + '/' + GOALIE_FORMATION_CONF );
    if ( ! M_goalie_formation )
    {
        std::cerr << "Failed to read goalie formation" << std::endl;
        //return false;
    }

    M_goal_kick_opp_formation = readFormation( configpath + M_formation_type + '/' + GOAL_KICK_OPP_CONF );
    if ( ! M_goal_kick_opp_formation )
    {
        return false;
    }

    M_goal_kick_our_formation = readFormation( configpath + M_formation_type + '/' + GOAL_KICK_OUR_CONF );
    if ( ! M_goal_kick_our_formation )
    {
        return false;
    }

    M_goalie_catch_opp_formation = readFormation( configpath + M_formation_type + '/' + GOALIE_CATCH_OPP_CONF );
    if ( ! M_goalie_catch_opp_formation )
    {
        return false;
    }

    M_goalie_catch_our_formation = readFormation( configpath + M_formation_type + '/' + GOALIE_CATCH_OUR_CONF );
    if ( ! M_goalie_catch_our_formation )
    {
        return false;
    }

    M_kickin_our_formation = readFormation( configpath + M_formation_type + '/' + KICKIN_OUR_FORMATION_CONF );
    if ( ! M_kickin_our_formation )
    {
        std::cerr << "Failed to read kickin our formation" << std::endl;
        return false;
    }

    M_setplay_opp_formation = readFormation( configpath + M_formation_type + '/' + SETPLAY_OPP_FORMATION_CONF );
    if ( ! M_setplay_opp_formation )
    {
        std::cerr << "Failed to read setplay opp formation" << std::endl;
        return false;
    }

    M_setplay_our_formation = readFormation( configpath + M_formation_type + '/' + SETPLAY_OUR_FORMATION_CONF );
    if ( ! M_setplay_our_formation )
    {
        std::cerr << "Failed to read setplay our formation" << std::endl;
        return false;
    }

    M_indirect_freekick_opp_formation = readFormation( configpath + M_formation_type + '/' + INDIRECT_FREEKICK_OPP_FORMATION_CONF );
    if ( ! M_indirect_freekick_opp_formation )
    {
        std::cerr << "Failed to read indirect freekick opp formation" << std::endl;
        return false;
    }

    M_indirect_freekick_our_formation = readFormation( configpath + M_formation_type + '/' + INDIRECT_FREEKICK_OUR_FORMATION_CONF );
    if ( ! M_indirect_freekick_our_formation )
    {
        std::cerr << "Failed to read indirect freekick our formation" << std::endl;
        return false;
    }

    //s_initialized = true;
    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
boost::shared_ptr< rcsc::Formation >
Strategy::readFormation( const std::string & filepath )
{
    std::ifstream fin( filepath.c_str() );
    if ( ! fin.is_open() )
    {
        std::cerr << __FILE__ << ": " << __LINE__
                  << " ***ERROR*** failed to open file [" << filepath << "]"
                  << std::endl;
        return boost::shared_ptr< rcsc::Formation >
            ( static_cast< rcsc::Formation * >( 0 ) );
    }


    std::string temp, type;
    fin >> temp >> type; // read training method type name
    fin.seekg( 0 );

    boost::shared_ptr< rcsc::Formation > ptr = createFormation( type );

    if ( ! ptr )
    {
        std::cerr << __FILE__ << ": " << __LINE__
                  << " ***ERROR*** failed to create formation [" << filepath << "]"
                  << std::endl;
        return boost::shared_ptr< rcsc::Formation >
            ( static_cast< rcsc::Formation * >( 0 ) );
    }

    if ( ! ptr->read( fin ) )
    {
        std::cerr << __FILE__ << ": " << __LINE__
                  << " ***ERROR*** failed to read formation [" << filepath << "]"
                  << std::endl;
        return boost::shared_ptr< rcsc::Formation >
            ( static_cast< rcsc::Formation * >( 0 ) );
    }

    for ( int unum = 1; unum <= 11; ++unum )
    {
        if ( M_role_factory.find( ptr->getRoleName( unum ) ) == M_role_factory.end() )
        {
            std::cerr << __FILE__ << ": " << __LINE__
                      << " ***ERROR*** Unsupported role name ["
                      << ptr->getRoleName( unum ) << "] is appered in ["
                      << filepath << "]" << std::endl;
            return boost::shared_ptr< rcsc::Formation >
                ( static_cast< rcsc::Formation * >( 0 ) );
        }
    }

    return ptr;
}

/*-------------------------------------------------------------------*/
/*!

*/
boost::shared_ptr< rcsc::Formation >
Strategy::createFormation( const std::string & type_name ) const
{
    std::map< std::string, FormationCreator >::const_iterator factory
        = M_formation_factory.find( type_name );

    if ( factory == M_formation_factory.end() )
    {
        std::cerr << __FILE__ << ": " << __LINE__
                  << " ***ERROR*** No such a formation name ["
                  << type_name << "]"
                  << std::endl;
        return boost::shared_ptr< rcsc::Formation >
            ( static_cast< rcsc::Formation * >( 0 ) );
    }

    boost::shared_ptr< rcsc::Formation > f( factory->second() );
    return f;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
Strategy::update( const rcsc::WorldModel & wm )
{
    updateSituation( wm );
    updatePosition( wm );
    analyzeOpponentStrategy( wm );

    M_mark_table->update( wm );
}

/*-------------------------------------------------------------------*/
/*!

*/
boost::shared_ptr< SoccerRole >
Strategy::createRole( const int number,
                      const rcsc::WorldModel & wm ) const
{
    if ( number < 1 || 11 < number )
    {
        std::cerr << __FILE__ << ": " << __LINE__
                  << " ***ERROR*** Invalid player number " << number
                  << std::endl;
        return boost::shared_ptr< SoccerRole >( static_cast< SoccerRole * >( 0 ) );
    }

    boost:: shared_ptr< const rcsc::Formation > formation = getFormation( wm );
    if ( ! formation )
    {
        std::cerr << __FILE__ << ": " << __LINE__
                  << " ***ERROR*** failed to create role. Null formation" << std::endl;
        return boost::shared_ptr< SoccerRole >( static_cast< SoccerRole * >( 0 ) );
    }

    const std::string role_name = formation->getRoleName( number );

    // get role factory of this role name
    std::map< std::string, RoleCreator >::const_iterator factory = M_role_factory.find( role_name );
    if ( factory == M_role_factory.end() )
    {
        std::cerr << __FILE__ << ": " << __LINE__
                  << " ***ERROR*** No such a role name ["
                  << role_name << "]"
                  << std::endl;
        return boost::shared_ptr< SoccerRole >( static_cast< SoccerRole * >( 0 ) );
    }

    boost::shared_ptr< SoccerRole > role_ptr( factory->second() );
    return role_ptr;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
Strategy::updateSituation( const rcsc::WorldModel & wm )
{
    M_current_situation = Normal_Situation;

    if ( wm.gameMode().type() != rcsc::GameMode::PlayOn )
    {
        if ( wm.gameMode().isPenaltyKickMode() )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": Situation PenaltyKick" );
            M_current_situation = PenaltyKick_Situation;
        }
        else if ( wm.gameMode().isPenaltyKickMode() )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": Situation OurSetPlay" );
            M_current_situation = OurSetPlay_Situation;
        }
        else
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": Situation OppSetPlay" );
            M_current_situation = OppSetPlay_Situation;
        }
        return;
    }

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();
    int our_min = std::min( self_min, mate_min );

    if ( opp_min <= our_min - 2 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": Situation Defense" );
        M_current_situation = Defense_Situation;
        return;
    }

    if ( our_min <= opp_min - 2 )
    {
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": Situation Offense" );
        M_current_situation = Offense_Situation;
        return;
    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": Situation Normal" );
}

/*-------------------------------------------------------------------*/
/*!

*/
void
Strategy::updatePosition( const rcsc::WorldModel & wm )
{
    static rcsc::GameTime s_update_time( 0, 0 );
    if ( s_update_time == wm.time() )
    {
        return;
    }

    boost:: shared_ptr< const rcsc::Formation > f = getFormation( wm );
    if ( ! f )
    {
        std::cerr << wm.time()
                  << " ***ERROR*** could not get the current formation" << std::endl;
        return;
    }

    int ball_step = 0;
    if ( wm.gameMode().type() == rcsc::GameMode::PlayOn
         || wm.gameMode().type() == rcsc::GameMode::GoalKick_ )
    {
        ball_step = std::min( 1000, wm.interceptTable()->teammateReachCycle() );
        ball_step = std::min( ball_step, wm.interceptTable()->opponentReachCycle() );
        ball_step = std::min( ball_step, wm.interceptTable()->selfReachCycle() );
    }

    rcsc::Vector2D ball_pos = wm.ball().inertiaPoint( ball_step );

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": HOME POSITION: ball pos=(%.1f %.1f) step=%d",
                        ball_pos.x, ball_pos.y,
                        ball_step );

    M_positions.clear();
    f->getPositions( ball_pos, M_positions );

    if ( rcsc::ServerParam::i().useOffside() )
    {
        double max_x = wm.offsideLineX();
        int mate_step = wm.interceptTable()->teammateReachCycle();
        if ( mate_step < 50 )
        {
            rcsc::Vector2D trap_pos = wm.ball().inertiaPoint( mate_step );
            if ( trap_pos.x > max_x ) max_x = trap_pos.x;
        }

        for ( int unum = 1; unum <= 11; ++unum )
        {
            if ( M_positions[unum-1].x > max_x - 1.0 )
            {
                rcsc::dlog.addText( rcsc::Logger::TEAM,
                                    "____ %d offside. home_pos_x %.2f -> %.2f",
                                    unum,
                                    M_positions[unum-1].x, max_x - 1.0 );
                M_positions[unum-1].x = max_x - 1.0;
            }
        }
    }

    M_position_types.clear();
    for ( int unum = 1; unum <= 11; ++unum )
    {
        PositionType type = Position_Center;
        if ( f->isSideType( unum ) )
        {
            type = Position_Left;
        }
        else if ( f->isSymmetryType( unum ) )
        {
            type = Position_Right;
        }

        M_position_types.push_back( type );

        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            "__ %d home pos (%.2f %.2f) type=%d",
                            unum,
                            M_positions[unum-1].x, M_positions[unum-1].y,
                            type );
        rcsc::dlog.addCircle( rcsc::Logger::TEAM,
                              M_positions[unum-1], 0.5,
                              "#000000" );
    }

    s_update_time = wm.time();
}

/*-------------------------------------------------------------------*/
/*!

*/
void
Strategy::analyzeOpponentStrategy( const rcsc::WorldModel & wm )
{
    if ( M_opponent_defense_strategy == No_Strategy
         && ! wm.opponentTeamName().empty() )
    {
        M_opponent_offense_strategy = Normal_Strategy;
        M_opponent_defense_strategy = Normal_Strategy;

        // TODO
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
PositionType
Strategy::getPositionType( const int number ) const
{
    if ( number < 1 || 11 < number )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": Illegal number : " << number
                  << std::endl;
        return Position_Center;
    }
    return M_position_types[number - 1];
}

/*-------------------------------------------------------------------*/
/*!

*/
rcsc::Vector2D
Strategy::getPosition( const int number ) const
{
    if ( number < 1 || 11 < number )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": Illegal number : " << number
                  << std::endl;
        return rcsc::Vector2D::INVALIDATED;
    }
    return M_positions[number - 1];
}

/*-------------------------------------------------------------------*/
/*!

*/
boost:: shared_ptr< rcsc::Formation >
Strategy::getFormation( const rcsc::WorldModel & wm ) const
{
    if ( wm.gameMode().type() == rcsc::GameMode::PlayOn )
    {
        if ( wm.self().goalie()
             && M_goalie_formation )
        {
            return M_goalie_formation;
        }

        switch ( M_current_situation ) {
        case Defense_Situation:
            return M_defense_formation;
        case Offense_Situation:
            return M_offense_formation;
        default:
            break;
        }
        return M_normal_formation;
    }

    if ( wm.gameMode().type() == rcsc::GameMode::KickIn_
         || wm.gameMode().type() == rcsc::GameMode::CornerKick_ )
    {
        if ( wm.ourSide() == wm.gameMode().side() )
        {
            // our kick-in or corner-kick
            return M_kickin_our_formation;
        }
        else
        {
            return M_setplay_opp_formation;
        }
    }

    if ( ( wm.gameMode().type() == rcsc::GameMode::BackPass_
           && wm.gameMode().side() == wm.theirSide() )
         || ( wm.gameMode().type() == rcsc::GameMode::IndFreeKick_
              && wm.gameMode().side() == wm.ourSide() ) )
    {
        return M_indirect_freekick_our_formation;
    }

    if ( ( wm.gameMode().type() == rcsc::GameMode::BackPass_
           && wm.gameMode().side() == wm.ourSide() )
         || ( wm.gameMode().type() == rcsc::GameMode::IndFreeKick_
              && wm.gameMode().side() == wm.theirSide() ) )
    {
        return M_indirect_freekick_opp_formation;
    }

    if ( wm.gameMode().type() == rcsc::GameMode::GoalKick_ )
    {
        if ( wm.gameMode().side() == wm.ourSide() )
        {
            return M_goal_kick_our_formation;
        }
        else
        {
            return M_goal_kick_opp_formation;
        }
    }

    if ( wm.gameMode().type() == rcsc::GameMode::GoalieCatch_ )
    {
        if ( wm.gameMode().side() == wm.ourSide() )
        {
            return M_goalie_catch_our_formation;
        }
        else
        {
            return M_goalie_catch_opp_formation;
        }
    }

    if ( wm.gameMode().type() == rcsc::GameMode::BeforeKickOff
         || wm.gameMode().type() == rcsc::GameMode::AfterGoal_ )
    {
        return M_before_kick_off_formation;
    }

    if ( wm.gameMode().isOurSetPlay( wm.ourSide() ) )
    {
        return M_setplay_our_formation;
    }

    if ( wm.gameMode().type() != rcsc::GameMode::PlayOn )
    {
        return M_setplay_opp_formation;
    }

    if ( wm.self().goalie()
         && M_goalie_formation )
    {
        return M_goalie_formation;
    }


    switch ( M_current_situation ) {
    case Defense_Situation:
        return M_defense_formation;
    case Offense_Situation:
        return M_offense_formation;
    default:
        break;
    }

    return M_normal_formation;
}

/*-------------------------------------------------------------------*/
/*!

*/
Strategy::BallArea
Strategy::get_ball_area( const rcsc::WorldModel & wm )
{
    int ball_step = 1000;
    ball_step = std::min( ball_step, wm.interceptTable()->teammateReachCycle() );
    ball_step = std::min( ball_step, wm.interceptTable()->opponentReachCycle() );
    ball_step = std::min( ball_step, wm.interceptTable()->selfReachCycle() );

    return get_ball_area( wm.ball().inertiaPoint( ball_step ) );
}

/*-------------------------------------------------------------------*/
/*!

*/
Strategy::BallArea
Strategy::get_ball_area( const rcsc::Vector2D & ball_pos )
{
    if ( ball_pos.x > 36.0 )
    {
        if ( ball_pos.absY() > 17.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": get_ball_area: Cross" );
            return BA_Cross;
        }
        else
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": get_ball_area: ShootChance" );
            return BA_ShootChance;
        }
    }
    else if ( ball_pos.x > -1.0 )
    {
        if ( ball_pos.absY() > 17.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": get_ball_area: DribbleAttack" );
            return BA_DribbleAttack;
        }
        else
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": get_ball_area: OffMidField" );
            return BA_OffMidField;
        }
    }
    else if ( ball_pos.x > -30.0 )
    {
        if ( ball_pos.absY() > 17.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": get_ball_area: DribbleBlock" );
            return BA_DribbleBlock;
        }
        else
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": get_ball_area: DefMidField" );
            return BA_DefMidField;
        }
    }
    else if ( ball_pos.x > -36.5 )
    {
        if ( ball_pos.absY() > 17.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": get_ball_area: CrossBlock" );
            return BA_CrossBlock;
        }
        else
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": get_ball_area: Stopper" );
            return BA_Stopper;
        }
    }
    else
    {
        if ( ball_pos.absY() > 17.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": get_ball_area: CrossBlock" );
            return BA_CrossBlock;
        }
        else
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": get_ball_area: Danger" );
            return BA_Danger;
        }
    }

    rcsc::dlog.addText( rcsc::Logger::TEAM,
                        __FILE__": get_ball_area: unknown area" );
    return BA_None;
}

/*-------------------------------------------------------------------*/
/*!

*/
double
Strategy::get_defender_dash_power( const rcsc::WorldModel & wm,
                                   const rcsc::Vector2D & home_pos )
{
    static bool S_recover_mode = false;

    if ( wm.self().stamina() < rcsc::ServerParam::i().staminaMax() * 0.5 )
    {
        S_recover_mode = true;
    }
    else if ( wm.self().stamina()
              > rcsc::ServerParam::i().staminaMax() * 0.85 )
    {
        S_recover_mode = false;
    }

    const double ball_xdiff
        //= wm.ball().pos().x - home_pos.x;
        = wm.ball().pos().x - wm.self().pos().x;

    const rcsc::PlayerType & mytype = wm.self().playerType();
    const double my_inc = mytype.staminaIncMax() * wm.self().recovery();

    double dash_power;
    if ( S_recover_mode )
    {
        if ( wm.ourDefenseLineX() > wm.self().pos().x )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": get_dash_power. correct DF line & recover" );
            dash_power = my_inc;
        }
        else if ( ball_xdiff < 5.0 )
        {
            dash_power = rcsc::ServerParam::i().maxPower();
        }
        else if ( ball_xdiff < 10.0 )
        {
            dash_power = rcsc::ServerParam::i().maxPower();
            dash_power *= 0.7;
            //dash_power
            //    = mytype.getDashPowerToKeepSpeed( 0.7, wm.self().effort() );
        }
        else if ( ball_xdiff < 20.0 )
        {
            dash_power = std::max( 0.0, my_inc - 10.0 );
        }
        else // >= 20.0
        {
            dash_power = std::max( 0.0, my_inc - 20.0 );
        }

        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": get_dash_power. recover mode dash_power= %.1f",
                            dash_power );

        return dash_power;
    }

    // normal case

#if 1
    // added 2006/06/11 03:34
    if ( wm.ball().pos().x > 0.0
         && wm.self().pos().x < home_pos.x
         && wm.self().pos().x > wm.ourDefenseLineX() - 0.05 ) // 20080712
    {
        double power_for_max_speed = mytype.getDashPowerToKeepMaxSpeed( wm.self().effort() );
        double defense_dash_dist = wm.self().pos().dist( rcsc::Vector2D( -48.0, 0.0 ) );
        int cycles_to_reach = mytype.cyclesToReachDistance( defense_dash_dist );
        int available_dash_cycles = mytype.getMaxDashCyclesSavingRecovery( power_for_max_speed,
                                                                           wm.self().stamina(),
                                                                           wm.self().recovery() );
        if ( available_dash_cycles < cycles_to_reach )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": get_dash_power. keep stamina for defense back dash,"
                                " power_for_max=%.1f"
                                " dash_dist=%.1f, reach_cycle=%d, dashable_cycle=%d",
                                power_for_max_speed,
                                defense_dash_dist, cycles_to_reach, available_dash_cycles );
            dash_power = std::max( 0.0, my_inc - 20.0 );
            return dash_power;
        }
    }
#endif

    if ( wm.self().pos().x < -30.0
         && wm.ourDefenseLineX() > wm.self().pos().x )
    {
        dash_power = rcsc::ServerParam::i().maxPower();
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": get_dash_power. correct dash power for the defense line. power=%.1f",
                            dash_power );
    }
    else if ( home_pos.x < wm.self().pos().x )
    {
        dash_power = rcsc::ServerParam::i().maxPower();
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": get_dash_power. max power to go to the behind home position. power=%.1f",
                            dash_power );
    }
    else if ( ball_xdiff > 20.0 )
    {
        dash_power = my_inc;
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": get_dash_power. correct dash power to save stamina(1). power=%.1f",
                            dash_power );
    }
    else if ( ball_xdiff > 10.0 )
    {
        dash_power = rcsc::ServerParam::i().maxPower();
        dash_power *= 0.6;
        //dash_power = mytype.getDashPowerToKeepSpeed( 0.6, wm.self().effort() );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": get_dash_power. correct dash power to save stamina(2). power=%.1f",
                            dash_power );
    }
    else if ( ball_xdiff > 5.0 )
    {
        dash_power = rcsc::ServerParam::i().maxPower();
        dash_power *= 0.85;
        //dash_power = mytype.getDashPowerToKeepSpeed( 0.85, wm.self().effort() );
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": get_dash_power. correct dash power to save stamina(3). power=%.1f",
                            dash_power );
    }
    else
    {
        dash_power = rcsc::ServerParam::i().maxPower();
        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": get_dash_power. max power. power=%.1f",
                            dash_power );
    }

    return dash_power;
}


/*-------------------------------------------------------------------*/
/*!

*/
double
Strategy::get_defense_line_x( const rcsc::WorldModel & wm,
                              const rcsc::Vector2D & home_pos )
{
    double x = home_pos.x;

    if ( wm.ball().pos().x > -30.0
         && wm.self().pos().x > home_pos.x
         && wm.ball().pos().x > wm.self().pos().x + 3.0 )
    {
        if ( wm.ball().pos().x > 0.0 ) x = -3.5;
        else if ( wm.ball().pos().x > -8.0 ) x = -11.5;
        else if ( wm.ball().pos().x > -16.0 ) x = -19.5;
        else if ( wm.ball().pos().x > -24.0 ) x = -27.5;
        else if ( wm.ball().pos().x > -30.0 ) x = -33.5;
        else x = home_pos.x;

        rcsc::dlog.addText( rcsc::Logger::TEAM,
                            __FILE__": get_defense_line_x. x= %.1f",
                            x );

        if ( x > 0.0 ) x = 0.0;
        if ( x > home_pos.x + 5.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": get_defense_line_x. adjust to the home_pos(+) %.1f",
                                home_pos.x );
            x = home_pos.x + 5.0;
        }

        if ( x < home_pos.x - 2.0 )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": get_defense_line_x. adjust to the home_pos(-) %.1f",
                                home_pos.x );
            x = home_pos.x - 2.0;
        }

        if ( wm.ourDefenseLineX() - 1.0 > x )
        {
            rcsc::dlog.addText( rcsc::Logger::TEAM,
                                __FILE__": get_defense_line_x. adjust to the defense line %.1f",
                                wm.ourDefenseLineX() );
            x = wm.ourDefenseLineX() - 1.0;
        }
    }

    return x;
}

/*-------------------------------------------------------------------*/
/*!

*/
double
Strategy::get_set_play_dash_power( const rcsc::WorldModel & wm )
{
    if ( wm.gameMode().type() == rcsc::GameMode::PenaltySetup_ )
    {
        return wm.self().getSafetyDashPower( rcsc::ServerParam::i().maxPower() );
    }

    double rate = 1.0;
    if ( wm.self().stamina() > rcsc::ServerParam::i().staminaMax() * 0.8 )
    {
        rate = 1.5
            * wm.self().stamina()
            / rcsc::ServerParam::i().staminaMax();
    }
    else
    {
        rate = 0.9
            * ( wm.self().stamina()
                - rcsc::ServerParam::i().recoverDecThrValue() )
            / rcsc::ServerParam::i().staminaMax();
        rate = std::max( 0.0, rate );
    }

    return ( wm.self().playerType().staminaIncMax()
             * wm.self().recovery()
             * rate );
}

bool
Strategy::SetFormationType (std::string f)
{
  bool valid_formation = false;

  if ( f == "433_normal" )
    valid_formation = true;
  else  if ( f == "433_light" )
    valid_formation = true;
  else  if ( f == "433_hardcore" )
    valid_formation = true;
  else  if ( f == "442_normal" )
    valid_formation = true;
  else  if ( f == "442_light" )
    valid_formation = true;
  else  if ( f == "442_hardcore" )
    valid_formation = true;
  else  if ( f == "442_swarm" )
    valid_formation = true;  
  else  if ( f == "4231_normal" )
    valid_formation = true;
  else  if ( f == "4231_light" )
    valid_formation = true;
  else  if ( f == "4231_hardcore" )
    valid_formation = true;
  else  if ( f == "sweeper" )
    valid_formation = true;

  if ( !valid_formation )
    {
      std::cerr << "Tried to set an invalid formation: "
		<< f.c_str ()		
		<< std::endl;
      return false;
    }
      
  M_formation_type = f;
  return true;
}

std::string
Strategy::GetFormationType () const
{
  return M_formation_type;
}
