// -*-c++-*-

/*!
  \file bhv_pass_test.h
  \brief advanced pass planning & behavior.
*/

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA, Hiroki SHIMORA

 This code is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifndef RCSC_ACTION_BHV_PASS_TEST_H
#define RCSC_ACTION_BHV_PASS_TEST_H

#include <rcsc/player/soccer_action.h>
#include <rcsc/geom/vector_2d.h>

#include <functional>
#include <vector>

namespace rcsc {

class WorldModel;
class AbstractPlayerObject;
class PlayerObject;

/*!
  \class Bhv_PassTest
  \brief advanced pass planning & behavior.
 */
class Bhv_PassTest
    : public SoccerBehavior {
public:
    enum PassType {
        DIRECT  = 1,
        LEAD    = 2,
        THROUGH = 3,
        RECURSIVE = 4
    };

    /*!
      \struct PassRoute
      \brief pass route information object, that contains type,
      receiver info, receive point and ball first speed.
     */
    struct PassRoute {
        PassType type_;
        const AbstractPlayerObject * receiver_;
        Vector2D receive_point_;
        double first_speed_;
        bool one_step_kick_;
        double score_;

        PassRoute( PassType type,
                   const AbstractPlayerObject * receiver,
                   const Vector2D & point,
                   const double & speed,
                   const bool one_step_kick )
            : type_( type )
            , receiver_( receiver )
            , receive_point_( point )
            , first_speed_( speed )
            , one_step_kick_( one_step_kick )
            , score_( 0.0 )
          { }
    };

    class PassRouteScoreComp
        : public std::binary_function< PassRoute, PassRoute, bool > {
    public:
        result_type operator()( const first_argument_type & lhs,
                                const second_argument_type & rhs ) const
          {
              return lhs.score_ < rhs.score_;
          }
    };

    class PassRouteEvaluator {
    public:
        /*!
          \brief destructor
        */
        virtual ~PassRouteEvaluator()
          {

          }

        /*!
          \brief evaluate a pass route
          \param route route to evaluate
          \return evaluation of route
        */
        virtual double operator() ( const PassRoute & route ) = 0;
    };

private:

    //! cached calculated pass data
    static std::vector< PassRoute > S_cached_pass_route;


public:
    /*!
      \brief accessible from global.
    */
    Bhv_PassTest()
      { }

    /*!
      \brief execute action
      \param agent pointer to the agent itself
      \return true if action is performed
    */
    bool execute( PlayerAgent * agent );

    /*!
      \brief calculate best pass route
      \param world consr rerefence to the WorldModel
      \return pointer to the pass route variable. if no route, NULL is returned.
    */
    static
    const
    PassRoute * get_best_pass( const WorldModel & world );


    static
    const
    std::vector< PassRoute > & get_pass_route_cont( const WorldModel & world )
      {
          get_best_pass( world );
          return S_cached_pass_route;
      }

private:
    /*!
      \brief execute pass using pass course table
      \param agent pointer to the agent itself
      \return true if action is performed
    */
    static
    bool doRecursiveSearchPass( PlayerAgent * agent );

private:
    static
    void create_routes( const WorldModel & world );

    static
    void create_direct_pass( const WorldModel & world,
                             const PlayerObject * teammates );
    static
    void create_lead_pass( const WorldModel & world,
                           const PlayerObject * teammates );
    static
    void create_through_pass( const WorldModel & world,
                              const PlayerObject * teammates );
    static
    void create_recursive_pass( const WorldModel & world );

    static
    bool verify_direct_pass( const WorldModel & world,
                             const PlayerObject * receiver,
                             const Vector2D & target_point,
                             const double & target_dist,
                             const AngleDeg & target_angle,
                             const double & first_speed );
    static
    bool verify_through_pass( const WorldModel & world,
                              const PlayerObject * receiver,
                              const Vector2D & receiver_pos,
                              const Vector2D & target_point,
                              const double & target_dist,
                              const AngleDeg & target_angle,
                              const double & first_speed,
                              const double & reach_step,
                              const bool is_through_pass );

    static
    void evaluate_routes( const WorldModel & world );

    static
    bool can_kick_by_one_step( const WorldModel & world,
                               const double & first_speed,
                               const AngleDeg & target_angle );

};

}

#endif
