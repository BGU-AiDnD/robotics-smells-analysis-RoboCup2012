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
/*
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

//#include "role_goalie.h"
#include "role_FuzzyGoalie.h"

#include "bhv_savior.h"

#include "strategy.h"

#include "bhv_goalie_basic_move.h"
#include "bhv_goalie_chase_ball.h"
#include "bhv_goalie_free_kick.h"

#include "body_clear_ball2008.h"
#include "ultimaTentativa1.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/obsolete/intention_kick2007.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/world_model.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/rect_2d.h>
#include <rcsc/coach/global_object.h>

//fuzzy behavior
//#include "ultimaTentativa1.c"
//#include "ultimaTentativa1.h"


/*-------------------------------------------------------------------*/
/*!


void
RoleFuzzyGoalie::execute( rcsc::PlayerAgent* agent )
{
    /*if ( Strategy::i().savior() )
    {
        Bhv_Savior().execute( agent );
        return;
    }

    static const
        rcsc::Rect2D our_penalty( rcsc::Vector2D( -rcsc::ServerParam::i().pitchHalfLength(),
                                                  -rcsc::ServerParam::i().penaltyAreaHalfWidth() + 1.0 ),
                                  rcsc::Size2D( rcsc::ServerParam::i().penaltyAreaLength() - 1.0,
                                                rcsc::ServerParam::i().penaltyAreaWidth() - 2.0 ) );

    //////////////////////////////////////////////////////////////
    // play_on play

    // catchable
    if ( agent->world().time().cycle() > M_last_goalie_kick_time.cycle() + 5
         && agent->world().ball().distFromSelf() < agent->world().self().catchableArea() - 0.05
         && our_penalty.contains( agent->world().ball().pos() ) )
    {
        rcsc::dlog.addText( rcsc::Logger::ROLE,
                            __FILE__": catchable. ball dist=%.1f, my_catchable=%.1f",
                            agent->world().ball().distFromSelf(),
                            agent->world().self().catchableArea() );
        agent->doCatch();
        agent->setNeckAction( new rcsc::Neck_TurnToBall() );
    }
    else if ( agent->world().self().isKickable() )
    {
        doKick( agent );
    }
    else
    {
        doMove( agent );
    }
}

/*-------------------------------------------------------------------*/
/*!


void
RoleFuzzyGoalie::doKick( rcsc::PlayerAgent * agent )
{
    Body_ClearBall2008().execute( agent );
    agent->setNeckAction( new rcsc::Neck_ScanField() );
}

/*-------------------------------------------------------------------*/
/*!


void
RoleFuzzyGoalie::doMove( rcsc::PlayerAgent * agent )
{
    /*if ( Bhv_GoalieChaseBall::is_ball_chase_situation( agent ) )
    {
        Bhv_GoalieChaseBall().execute( agent );
    }
    else
    {
        Bhv_GoalieBasicMove().execute( agent );
    }*/
    
    /*const rcsc::WorldModel & wm = agent->world();
    //three fuzzy regions
    rcsc::Rect2D Area[3] = {rcsc::Rect2D( rcsc::Vector2D(-52.5,35.0), rcsc::Vector2D(-12.5,-35.0) ),
                            rcsc::Rect2D( rcsc::Vector2D(-20.0,35.0), rcsc::Vector2D( 20.0,-35.0) ),
                            rcsc::Rect2D( rcsc::Vector2D( 12.5,35.0), rcsc::Vector2D( 52.5,-35.0) ),};
    rcsc::Rect2D currentArea;
    int numPlayers;

    if ( wm.self().pos().x < -12.5 )
        currentArea = Area[0];
    else if ( wm.self().pos().x > 12.5 )
        currentArea = Area[2];
    else currentArea = Area[1];

    numPlayers = int(wm.countOpponentsIn(currentArea, 0, true)); 

    //MembershipFunction possibilidadeChute;
    FuzzyNumber possibilidadeChute;
    FuzzyNumber areaAtacante;
    FuzzyNumber areaGoleiro;
    FuzzyNumber numJogadores;
    //MembershipFunction numJogadores;
    FuzzyNumber respostaGoleiro;

    //MembershipFunction currentPlayerArea;
    FuzzyNumber currentPlayerArea;
    MembershipFunction currentGoalieArea;

    const rcsc::WorldModel & wm = agent->world();

    TP_possChute ps;
    TP_areaJogador aj;
    TP_areaHori ah;
    TP_posXgoleiro posx;
    TP_qtJogadores qtj;

    if (!wm.existKickableOpponent()) {
        possibilidadeChute = ps.baixa;
    }
    else {
        int oppPlayers, ourPlayers;
        //three fuzzy regions
        rcsc::Rect2D playerArea[4] = {rcsc::Rect2D( rcsc::Vector2D(-52.5,35.0), rcsc::Vector2D(-12.5,-35.0) ),
                                      rcsc::Rect2D( rcsc::Vector2D(-20.0,35.0), rcsc::Vector2D( 20.0,-35.0) ),
                                      rcsc::Rect2D( rcsc::Vector2D( 12.5,35.0), rcsc::Vector2D( 35.0,-35.0) ),
                                      rcsc::Rect2D( rcsc::Vector2D( 35.0,35.0), rcsc::Vector2D( 52.5,-35.0) ),};
        rcsc::Rect2D auxPlayerArea;

        const rcsc::PlayerObject * ballOwner = wm.getOpponentNearestToBall(0,true);
        
        if ( ballOwner->pos().x < -12.5 ) {
            //currentPlayerArea = aj.longe;
            auxPlayerArea = playerArea[0];
        }
        else if ( ballOwner->pos().x > 35 ) {
            //currentPlayerArea = aj.mtPerto;
            auxPlayerArea = playerArea[3];
        }
        else if ( ballOwner->pos().x < 35 && ballOwner->pos().x > 12 ) {
            //currentPlayerArea = aj.perto;
            auxPlayerArea = playerArea[2];
        }
        else {
            //currentPlayerArea = aj.medio;
            auxPlayerArea = playerArea[1];
        }



        //determines number of players
        oppPlayers = int(wm.countOpponentsIn(auxPlayerArea, 0, true));
        ourPlayers = int(wm.countTeammatesIn(auxPlayerArea, 0, true));

        if (ourPlayers - oppPlayers > 2)
            numJogadores = qtj.vantagem;
        else if (ourPlayers - oppPlayers < 2 && ourPlayers - oppPlayers > -1)
            numJogadores = qtj.empate;
        else if (ourPlayers - oppPlayers < -1 && ourPlayers - oppPlayers > -4)
            numJogadores = qtj.desvantagem;
        else numJogadores = qtj.largaDes;
    }

    FuzzyNumber poss;
    //RL_definePoss( currentPlayerArea, numJogadores, poss );

    //defines goalie area
    rcsc::Rect2D goalieArea[3] = {rcsc::Rect2D( rcsc::Vector2D(-52.5,35.0), rcsc::Vector2D(-40.0,-35.0) ),
                                  rcsc::Rect2D( rcsc::Vector2D(-40.0,35.0), rcsc::Vector2D(-20.0,-35.0) ),
                                  rcsc::Rect2D( rcsc::Vector2D(-20.0,35.0), rcsc::Vector2D(-10.0,-35.0) ),};
    rcsc::Rect2D auxGoalieArea;

    if ( wm.self().pos().x < -40.0 ) {
            currentGoalieArea = ah.esquerda;
            auxGoalieArea = goalieArea[0];
        }
        else if ( wm.self().pos().x > -20 ) {
            currentGoalieArea = ah.direita;
            auxGoalieArea = goalieArea[2];
        }
        else {
            currentGoalieArea = ah.meio;
            auxGoalieArea = goalieArea[1];
        }

    FuzzyNumber goalieAnswer;

    //ultimaTentativa1InferenceEngine(poss,currentPlayerArea,currentGoalieArea,
                                    //numJogadores,goalieAnswer,poss);
     
}
*/