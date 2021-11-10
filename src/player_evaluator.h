// -*-c++-*-

/*!
  \file player_evaluator.h
  \brief player evaluator classes
*/

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA

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


#ifndef GEAR_PLAYER_EVALUATOR_H
#define GEAR_PLAYER_EVALUATOR_H

#include <rcsc/player/abstract_player_object.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/common/server_param.h>
#include <cmath>

/*!
  \class PlayerEvaluator
  \brief abstract player evaluator function object class
 */
class PlayerEvaluator {
protected:
    /*!
      \brief protected constructor
     */
    PlayerEvaluator()
      { }
public:
    /*!
      \brief virtual destructor
     */
    virtual
    ~PlayerEvaluator()
      { }

    /*!
      \brief evaluation function
      \param p const reference to the target player object
      \return the evaluated value
     */
    virtual
    double operator()( const rcsc::AbstractPlayerObject & p ) const = 0;
};

/*!
  \class AbsYDiffPlayerEvaluator
  \brief evaluation by y-coodinate difference
 */
class AbsYDiffPlayerEvaluator
    : public PlayerEvaluator {
private:
    //! base point
    const rcsc::Vector2D M_point;

    // not used
    AbsYDiffPlayerEvaluator();
public:
    /*!
      \brief construct with base point
      \param point base point
     */
    AbsYDiffPlayerEvaluator( const rcsc::Vector2D & point )
        : M_point( point )
      { }

    /*!
      \brief evaluation function
      \param p const reference to the target player
      \return evaluation value (absolute y-coodinate difference)
     */
    double operator()( const rcsc::AbstractPlayerObject & p ) const
      {
          return std::fabs( p.pos().y - M_point.y );
      }
};

/*!
  \class AbsAngleDiffPlayerEvaluator
  \brief evaluation by absolute angle difference
 */
class AbsAngleDiffPlayerEvaluator
    : public PlayerEvaluator {
private:
    //! base point
    const rcsc::Vector2D M_base_point;
    //! compared angle
    const rcsc::AngleDeg M_base_angle;

    // not used
    AbsAngleDiffPlayerEvaluator();
public:
    /*!
      \brief construct with base point & angle
      \param base_point base point
      \param base_angle compared angle
     */
    AbsAngleDiffPlayerEvaluator( const rcsc::Vector2D & base_point,
                                 const rcsc::AngleDeg & base_angle )
        : M_base_point( base_point )
        , M_base_angle( base_angle )
      { }

    /*!
      \brief evaluation function
      \param p const reference to the target player
      \return evaluation value (absolute angle difference)
     */
    double operator()( const rcsc::AbstractPlayerObject & p ) const
      {
          return ( ( p.pos() - M_base_point ).th() - M_base_angle ).abs();
      }
};

/*!
  \class XPosPlayerEvaluator
  \brief evaluation by x-coordinate value
 */
class XPosPlayerEvaluator
    : public PlayerEvaluator {
public:
    /*!
      \brief evaluation function
      \param p const reference to the target player
      \return evaluation value (x-coordinate value)
     */
    double operator()( const rcsc::AbstractPlayerObject & p ) const
      {
          return p.pos().x;
      }
};

/*!
  \class DistFromPosPlayerEvaluator
  \brief evaluation by distance from position
 */
class DistFromPosPlayerEvaluator
    : public PlayerEvaluator {
private:
    //! base point
    const rcsc::Vector2D M_base_point;
public:
    /*!
      \brief construct with base point
      \param base_point base point
     */
    DistFromPosPlayerEvaluator( const rcsc::Vector2D & base_point )
        : M_base_point( base_point )
      { }

public:
    /*!
      \brief evaluation function
      \param p const reference to the target player
      \return evaluation value (x-coordinate value)
     */
    double operator()( const rcsc::AbstractPlayerObject & p ) const
      {
          return (p.pos() - M_base_point).r();
      }
};

/*!
  \class BallControllableDistancePlayerEvaluator
  \brief evaluation by ball controllable area
 */
class BallControllableDistancePlayerEvaluator
    : public PlayerEvaluator {
public:
    /*!
      \brief evaluation function
      \param p const reference to the target player
      \return evaluation value (controllable dinstance)
     */
    double operator()( const rcsc::AbstractPlayerObject & p ) const
    {
        if ( p.goalie()
             && p.pos().absX() > rcsc::ServerParam::i().pitchHalfLength() - rcsc::ServerParam::i().penaltyAreaLength()
             && p.pos().absY() < rcsc::ServerParam::i().penaltyAreaHalfWidth() )
        {
            // XXX: check if ball is in penalty area or not
            return rcsc::ServerParam::i().catchableArea();
        }

        return ( p.playerTypePtr()
                 ? p.playerTypePtr()->kickableArea()
                 : rcsc::ServerParam::i().defaultKickableArea() );
    }
};

#endif
