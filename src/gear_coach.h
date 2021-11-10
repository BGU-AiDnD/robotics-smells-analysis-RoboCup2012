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

#ifndef GEAR_COACH_H
#define GEAR_COACH_H

#include <vector>
#include <map>
#include <set>

#include <rcsc/types.h>

#include <rcsc/coach/coach_agent.h>

namespace rcsc {
class PlayerType;
}

class GearCoach
    : public rcsc::CoachAgent {
private:
    typedef std::vector< const rcsc::PlayerType * > PlayerTypePtrCont;

    int M_substitute_count; // substitution count after kick-off
    std::vector< int > M_available_player_type_id;
    std::map< int, int > M_assigned_player_type_id;

    int M_opponent_player_types[11];
    
    double M_teammate_recovery[11];

    rcsc::TeamGraphic M_team_graphic;

public:

    GearCoach();

    virtual
    ~GearCoach();

private:

    /*!
      \brief substitute player.

      This methos should be overrided in the derived class
    */
    void updateRecoveryInfo();
    
    void doSubstitute();

    void doFirstSubstitute();
    
    void doSubstituteTiredPlayers();

    void substituteTo( const int unum,
                       const int type );

    int getFastestType( PlayerTypePtrCont & candidates );

    void sayPlayerTypes();
    
    bool changeFormation (std::string f);

    void sendTeamGraphic();

    void printVars (); // funcao aux para imprimir vars

    rcsc::Vector2D ballPos; //guarda a posição da bola (as vars x e y são publicas)
    
    void swarmUpdatePosition ();
    
    void swarmAttack ();
    
    void swarmDefense ();

protected:

    /*!
      You can override this method.
      But you must call PlayerAgent::doInit() in this method.
    */
    virtual
    bool initImpl( rcsc::CmdLineParser & cmd_parser );

    //! main decision
    virtual
    void actionImpl();
    
private:
  int ball_possession_our;
  int ball_possession_opp;
  int last_counted_cycle_possession;
  std::string current_formation;
  std::string fuzzy_formation;
  std::string scoring_formation;
  std::list<std::string> blacklisted_formations;
  bool need_to_change_formation;

  void chooseNewFormation ();
  void countBallPossession ();
  void decideFormation ();
  void decideBehavior (int time, double attack, double defense);
  void decidePosition ();
  bool isOurTeamLosing ();
  bool isOurTeamScoring();
  //void fuzzy( int time, int averagePosition);
  
  //void openFile(int FT, int t, int x);

  void fuzzyFormationChooser(int t, int x);		//formation chooser while fuzzy isn't ready
  void ballPositionCounter();
  void ballPositionOverTime();
  void gameTime();

  //function that retrieves data for psychofuzzy. Data are: game time,
  //number of fouls, ball possession, number of correct passes, numbers of interceptions,
  //unsuccessful attacks, unsuccessful defenses, ball position over time.

  void psychoFuzzy();

  //print the data calculated at psychoFuzzy

  void printPsycho();

  //psychofuzzy tables

  int  pf1[100];
  int  pf2[100];
  int  pf3[100];
  int  pf4[100];
  int  pf5[100];
  int  pf6[100];
  int  pf7[100];
  int  pf8[100];
  int  pf9[100];
  int  pf10[100];
  int  pf11[100];
  int  pf12[100];
  int  pf13[100];
  int  pf14[100];
  int  pf15[100];
  int  pf16[100];
  int  pf17[100];
  int  pf18[100];
  int  pf19[100];
  int  pf20[100];
  int  pf21[100];
  int  pf22[100];
  int  pf23[100];
  int  pf24[100];

  //formations variables

  bool F442;
  bool F433;
  bool F4231;
  bool Flight;
  bool Fnormal;
  bool Fhardcore;

  //psychoFuzzy vars
  double attackpct;
  double defensepct;

  int foulsCounter;
  double ourBall;
  double theirBall;
  bool foul;
  long int currentCycle;
  long int lastCycle;
  bool weHaveBall;
  bool weHaveKicked;
  bool theyHaveBall;
  bool theyHaveKicked;
  double correctPasses;
  double incorrectPasses;
  bool isAttacking;
  double badAttacks;
  bool isDefending;
  double goodDefenses;
  int ourScore;
  int theirScore;
  double pfBallPos;

  //formation fuzzy vars
  
  bool ballTooClose;				//ball position entries for fuzzy
  bool ballClose;
  bool ballDistant;
  
  int ballCounterTooClose;			
  int ballCounterClose;
  int ballCounterDistant;
  double ballPosition;
  double averageBallPosition;
  int timeCounter;
  
  bool gameTimeBegin;				//game time entries for fuzzy
  bool gameTimeMiddle;
  bool gameTimeEnd;
  
  int goalsDifference;
  bool teamSufferedGoal;

  int last_cycle;
      
};

#endif
