// -*-c++-*-

/*!
  \file strategy.h
  \brief team strategh Header File
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

#ifndef GEAR_STRATEGY_H
#define GEAR_STRATEGY_H

#include <rcsc/formation/formation.h>
#include <rcsc/geom/vector_2d.h>

#include <boost/scoped_ptr.hpp>

#include <string>
#include <map>
#include <vector>

namespace rcsc {
class CmdLineParser;
class WorldModel;
class Formation;
}

class SoccerRole;
class MarkTable;

enum PositionType {
    Position_Left = -1,
    Position_Center = 0,
    Position_Right = 1,
};

enum SituationType {
    Normal_Situation,
    Offense_Situation,
    Defense_Situation,
    OurSetPlay_Situation,
    OppSetPlay_Situation,
    PenaltyKick_Situation,
};

enum StrategyType {
    FastPassShoot_Strategy, // Brainstormers type
    ManMark_Strategy, // Brainstormers,AmoiensisNQ type
    Normal_Strategy,
    No_Strategy,
};

class Strategy {
public:
    static const std::string BEFORE_KICK_OFF_CONF;
    static const std::string GOAL_KICK_OPP_CONF;
    static const std::string GOAL_KICK_OUR_CONF;
    static const std::string GOALIE_CATCH_OPP_CONF;
    static const std::string GOALIE_CATCH_OUR_CONF;
    //static const std::string GOAL_KICK_OPP_FORMATION_CONF;
    //static const std::string GOAL_KICK_OUR_FORMATION_CONF;
    //static const std::string GOALIE_CATCH_OPP_FORMATION_CONF;
    //static const std::string GOALIE_CATCH_OUR_FORMATION_CONF;

    static const std::string NORMAL_FORMATION_CONF;
    static const std::string DEFENSE_FORMATION_CONF;
    static const std::string OFFENSE_FORMATION_CONF;
    static const std::string GOALIE_FORMATION_CONF;

    static const std::string KICKIN_OUR_FORMATION_CONF;
    static const std::string SETPLAY_OUR_FORMATION_CONF;
    static const std::string SETPLAY_OPP_FORMATION_CONF;
    static const std::string INDIRECT_FREEKICK_OUR_FORMATION_CONF;
    static const std::string INDIRECT_FREEKICK_OPP_FORMATION_CONF;

    enum BallArea {
        BA_CrossBlock, BA_DribbleBlock, BA_DribbleAttack, BA_Cross,
        BA_Stopper,    BA_DefMidField,  BA_OffMidField,   BA_ShootChance,
        BA_Danger,

        BA_None
    };

private:
    typedef SoccerRole * (*RoleCreator)();
    //typedef rcsc::Formation * (*FormationCreator)();
    
    //typedef SoccerRole * (*RoleCreator)( boost::shared_ptr< const rcsc::Formation > );
    typedef boost::shared_ptr<rcsc::Formation>  (*FormationCreator)();

    //
    // formations
    //

    std::string M_formation_type;

    std::map< std::string, RoleCreator > M_role_factory;
    std::map< std::string, FormationCreator > M_formation_factory;

    boost::shared_ptr< rcsc::Formation > M_before_kick_off_formation;

    boost::shared_ptr< rcsc::Formation > M_normal_formation;
    boost::shared_ptr< rcsc::Formation > M_defense_formation;
    boost::shared_ptr< rcsc::Formation > M_offense_formation;

    boost::shared_ptr< rcsc::Formation > M_goalie_formation;

    boost::shared_ptr< rcsc::Formation > M_setplay_our_formation;
    boost::shared_ptr< rcsc::Formation > M_setplay_opp_formation;
    boost::shared_ptr< rcsc::Formation > M_kickin_our_formation;
    boost::shared_ptr< rcsc::Formation > M_goal_kick_opp_formation;
    boost::shared_ptr< rcsc::Formation > M_goal_kick_our_formation;
    boost::shared_ptr< rcsc::Formation > M_goalie_catch_opp_formation;
    boost::shared_ptr< rcsc::Formation > M_goalie_catch_our_formation;
    boost::shared_ptr< rcsc::Formation > M_indirect_freekick_our_formation;
    boost::shared_ptr< rcsc::Formation > M_indirect_freekick_opp_formation;

    // situation typ
    SituationType M_current_situation;

    // current home positions
    //boost::shared_ptr< rcsc::Formation > M_current_formation;
    std::vector< PositionType > M_position_types;
    std::vector< rcsc::Vector2D > M_positions;

    //
    // mark
    //
    boost::scoped_ptr< MarkTable > M_mark_table;

    //
    // strategy
    //
    StrategyType M_opponent_offense_strategy;
    StrategyType M_opponent_defense_strategy;

    //
    // options
    //
    bool M_savior;


    // private for singleton
    Strategy();

    // not used
    Strategy( const Strategy & );
    const Strategy & operator=( const Strategy & );
public:

    static
    Strategy & instance();

    static
    const
    Strategy & i()
      {
          return instance();
      }

    //
    // initialization
    //

    void init( rcsc::CmdLineParser & cmd_parser );
    bool read( const std::string & config_dir );

    //
    // update
    //

    void update( const rcsc::WorldModel & wm );

    //
    // accessor to the current information
    //
    boost::shared_ptr< SoccerRole > createRole( const int number,
                                                const rcsc::WorldModel & wm ) const;
    PositionType getPositionType( const int number ) const;
    rcsc::Vector2D getPosition( const int number ) const;

    //
    // mark table
    //
    const
    MarkTable & markTable() const
      {
          return *M_mark_table;
      }

    //
    // strategy analysis
    //

    StrategyType opponentOffenseStrategy() const
      {
          return M_opponent_defense_strategy;
      }
    StrategyType opponentDefenseStrategy() const
      {
          return M_opponent_defense_strategy;
      }


    //
    // global options
    //

    bool savior() const
      {
          return M_savior;
      }

private:
    void updateSituation( const rcsc::WorldModel & wm );
    // update the current position table
    void updatePosition( const rcsc::WorldModel & wm );
    void analyzeOpponentStrategy( const rcsc::WorldModel & wm );

    boost::shared_ptr< rcsc::Formation > readFormation( const std::string & filepath );
    boost::shared_ptr< rcsc::Formation > createFormation( const std::string & type_name ) const;

    boost:: shared_ptr< rcsc::Formation > getFormation( const rcsc::WorldModel & wm ) const;

public:

    static
    BallArea get_ball_area( const rcsc::WorldModel & wm );
    static
    BallArea get_ball_area( const rcsc::Vector2D & ball_pos );

    static
    double get_defender_dash_power( const rcsc::WorldModel & wm,
                                    const rcsc::Vector2D & home_pos );
    static
    double get_defense_line_x( const rcsc::WorldModel & wm,
                               const rcsc::Vector2D & home_pos );

    static
    double get_set_play_dash_power( const rcsc::WorldModel & wm );
    
    bool
    SetFormationType (std::string f);

    std::string
    GetFormationType () const;

};

#endif
