#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "role_player1.h"

#include "bhv_savior.h"

#include "strategy.h"

#include "bhv_goalie_basic_move.h"
#include "bhv_goalie_chase_ball.h"
#include "bhv_goalie_free_kick.h"

#include "body_clear_ball2008.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/neck_scan_field.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/world_model.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/rect_2d.h>

/*-------------------------------------------------------------------*/
/*!

*/
void
RolePlayer1::execute( rcsc::PlayerAgent* agent )
{
    if ( Strategy::i().savior() )
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

*/
void
RolePlayer1::doKick( rcsc::PlayerAgent * agent )
{
    Body_ClearBall2008().execute( agent );
    agent->setNeckAction( new rcsc::Neck_ScanField() );
}

/*-------------------------------------------------------------------*/
/*!

*/
void
RolePlayer1::doMove( rcsc::PlayerAgent * agent )
{
    if ( Bhv_GoalieChaseBall::is_ball_chase_situation( agent ) )
    {
        Bhv_GoalieChaseBall().execute( agent );
    }
    else
    {
        Bhv_GoalieBasicMove().execute( agent );
    }
}
