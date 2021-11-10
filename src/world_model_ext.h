#ifndef LIBRCSC_EXT_WORLD_MODEL_EXT_H
#define LIBRCSC_EXT_WORLD_MODEL_EXT_H

#include <rcsc/player/world_model.h>
#include <rcsc/common/server_param.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/abstract_player_object.h>
#include <rcsc/player/player_predicate.h>
#include "player_evaluator.h"

#include <rcsc/math_util.h>

#include <boost/shared_ptr.hpp>

#include <cfloat>

namespace rcsc_ext {

static
double getDistPlayerNearestToPoint( const rcsc::WorldModel &,
                                    const rcsc::Vector2D & point,
                                    const rcsc::PlayerCont & cont,
                                    int validOpponentThreshold = -1 )
{
    static const double MAX_DIST = 65535.0;

    double min_dist2 = MAX_DIST;

    const rcsc::PlayerCont::const_iterator end = cont.end();
    for ( rcsc::PlayerCont::const_iterator it = cont.begin();
          it != end;
          ++it )
    {
        if ( (*it).isGhost() )
        {
            continue;
        }

        if ( validOpponentThreshold != -1 )
        {
            if ( (*it).posCount() > validOpponentThreshold )
            {
                continue;
            }
        }

        double dist2 = ( (*it).pos() - point ).r2();

        if ( dist2 < min_dist2 )
        {
            min_dist2 = dist2;
        }
    }

    return std::sqrt( min_dist2 );
}

static
inline
double getDistOpponentNearestToPoint( const rcsc::WorldModel & wm,
                                      const rcsc::Vector2D & point,
                                      int validOpponentThreshold = -1 )
{
    return( getDistPlayerNearestToPoint( wm, point, wm.opponents(),
                                         validOpponentThreshold ) );
}

static
inline
double getDistTeammateNearestToPoint( const rcsc::WorldModel & wm,
                                      const rcsc::Vector2D & point,
                                      int validOpponentThreshold = -1 )
{
    return getDistPlayerNearestToPoint( wm, point, wm.teammates(),
                                        validOpponentThreshold );
}


static
inline
rcsc::Vector2D getBallPredictPoint( const rcsc::WorldModel & wm )
{
    const int ball_predict_cycle = std::min( wm.interceptTable() -> teammateReachCycle(),
                                             wm.interceptTable() -> opponentReachCycle());

    return wm.ball().inertiaPoint( ball_predict_cycle );
}


static
inline
rcsc::Vector2D
ServerParam_ourTeamNearGoalPostPos( const rcsc::Vector2D & point )
{
    return rcsc::Vector2D( -rcsc::ServerParam::i().pitchHalfLength(),
                           +rcsc::sign( point.y ) * rcsc::ServerParam::i().goalHalfWidth() );
}

static
inline
rcsc::Vector2D
ServerParam_ourTeamFarGoalPostPos( const rcsc::Vector2D & point )
{
    return rcsc::Vector2D( -rcsc::ServerParam::i().pitchHalfLength(),
                           -rcsc::sign( point.y ) * rcsc::ServerParam::i().goalHalfWidth() );
}

static
inline
rcsc::Vector2D
ServerParam_opponentTeamNearGoalPostPos( const rcsc::Vector2D & point )
{
    return rcsc::Vector2D( +rcsc::ServerParam::i().pitchHalfLength(),
                           +rcsc::sign( point.y ) * rcsc::ServerParam::i().goalHalfWidth() );
}

static
inline
rcsc::Vector2D
ServerParam_opponentTeamFarGoalPostPos( const rcsc::Vector2D & point )
{
    return rcsc::Vector2D( +rcsc::ServerParam::i().pitchHalfLength(),
                           -rcsc::sign( point.y ) * rcsc::ServerParam::i().goalHalfWidth() );
}

static
inline
double
AbstractPlayerCont_getMinimumEvaluation( const rcsc::AbstractPlayerCont & container,
                                         const PlayerEvaluator * evaluator )
{
    boost::shared_ptr<const PlayerEvaluator> evaluator_ptr( evaluator );

    double min_value = +DBL_MAX;

    const rcsc::AbstractPlayerCont::const_iterator p_end = container.end();
    for ( rcsc::AbstractPlayerCont::const_iterator it = container.begin();
          it != p_end;
          ++it )
    {
        double value = (*evaluator_ptr)( **it );

        if ( value < min_value )
        {
            min_value = value;
        }
    }

    return min_value;
}

static
inline
const
rcsc::AbstractPlayerObject *
AbstractPlayerCont_getMinimumEvaluationPlayer( rcsc::AbstractPlayerCont & container,
                                               const PlayerEvaluator * evaluator,
                                               double * evaluated_value = static_cast< double * >( 0 ) )
{
    boost::shared_ptr<const PlayerEvaluator> evaluator_ptr( evaluator );

    double min_value = +DBL_MAX;
    const rcsc::AbstractPlayerObject * pl = static_cast< rcsc::AbstractPlayerObject * >( 0 );

    rcsc::AbstractPlayerCont::const_iterator p_end = container.end();
    for ( rcsc::AbstractPlayerCont::const_iterator it = container.begin();
          it != p_end;
          ++it )
    {
        double value = (*evaluator_ptr)( **it );

        if ( value < min_value )
        {
            pl = *it;
            min_value = value;
        }
    }

    if ( evaluated_value )
    {
        *evaluated_value = min_value;
    }

    return pl;
}

static
inline
double
AbstractPlayerCont_getMaximumEvaluation( rcsc::AbstractPlayerCont & container,
                                         const PlayerEvaluator * evaluator )
{
    boost::shared_ptr< const PlayerEvaluator > evaluator_ptr( evaluator );

    double max_value = -DBL_MAX;

    rcsc::AbstractPlayerCont::const_iterator p_end = container.end();
    for ( rcsc::AbstractPlayerCont::const_iterator it = container.begin();
          it != p_end;
          ++it )
    {
        double value = (*evaluator_ptr)( **it );

        if ( value > max_value )
        {
            max_value = value;
        }
    }

    return max_value;
}

static
inline
const
rcsc::AbstractPlayerObject *
AbstractPlayerCont_getMaximumEvaluationPlayer( rcsc::AbstractPlayerCont & container,
                                               const PlayerEvaluator * evaluator,
                                               double * evaluated_value = static_cast< double * >( 0 ) )
{
    boost::shared_ptr< const PlayerEvaluator > evaluator_ptr( evaluator );

    double max_value = -DBL_MAX;
    const rcsc::AbstractPlayerObject * pl = static_cast< rcsc::AbstractPlayerObject * >( 0 );

    const rcsc::AbstractPlayerCont::const_iterator p_end = container.end();
    for ( rcsc::AbstractPlayerCont::const_iterator it = container.begin();
          it != p_end;
          ++it )
    {
        double value = (*evaluator_ptr)( **it );

        if ( value > max_value )
        {
            pl = *it;
            max_value = value;
        }
    }

    if ( evaluated_value )
    {
        *evaluated_value = max_value;
    }

    return pl;
}

static
inline
double
WorldModel_forwardLineX( const rcsc::WorldModel & wm,
                         int validOpponentThreshold = -1 )
{
    if ( validOpponentThreshold == -1 )
    {
        validOpponentThreshold = 20;
    }

    rcsc::AbstractPlayerCont our_team_set;
    our_team_set = wm.getPlayerCont( new rcsc::AndPlayerPredicate
                                     ( new rcsc::TeammateOrSelfPlayerPredicate( wm ),
                                       new rcsc::CoordinateAccuratePlayerPredicate
                                       ( validOpponentThreshold ),
                                       new rcsc::XCoordinateBackwardPlayerPredicate
                                       ( wm.offsideLineX() ) ) );

    double forward_line_x = AbstractPlayerCont_getMaximumEvaluation( our_team_set,
                                                                     new XPosPlayerEvaluator );

    return forward_line_x;
}

}

#endif
