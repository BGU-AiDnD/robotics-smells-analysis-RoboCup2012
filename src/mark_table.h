// -*-c++-*-

/*!
  \file mark_table.h
  \brief mark target table Header File
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

#ifndef GEAR_MARK_TABLE_H
#define GEAR_MARK_TABLE_H

#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_time.h>

#include <vector>

namespace rcsc {
class WorldModel;
class AbstractPlayerObject;
}

class MarkTable {
public:

    struct Marker {
        const rcsc::AbstractPlayerObject * self_; //!< pointer to the marker teammate
        const rcsc::AbstractPlayerObject * target_; //!< pointer to the target opponent
        rcsc::Vector2D pos_; //!< base position for the marker teammate
        double dist2_; //!< distance from pos to target_->pos()

        Marker( const rcsc::AbstractPlayerObject * self,
                const rcsc::Vector2D & pos )
            : self_( self )
            , target_( static_cast< const rcsc::AbstractPlayerObject * >( 0 ) )
            , pos_( pos )
            , dist2_( 1000000.0 )
          { }

        Marker( const rcsc::AbstractPlayerObject * self,
                const rcsc::AbstractPlayerObject * target,
                const rcsc::Vector2D & pos,
                const double & dist2 )
            : self_( self )
            , target_( target )
            , pos_( pos )
            , dist2_( dist2 )
          { }

    private:
        // not used
        Marker();
    };

    struct Target {
        const rcsc::AbstractPlayerObject * self_; //!< pointer to the target opponent
        std::vector< Marker > markers_; //!< candidate markers

        Target()
          { }

        Target( const rcsc::AbstractPlayerObject * self )
            : self_( self )
          { }
    };

    struct Assignment {
        std::vector< const Marker * > markers_; //!< marker set
        double score_; //!< evaluated score

        Assignment()
            : score_( 0.0 )
          { }
    };

    struct Pair {
        const rcsc::AbstractPlayerObject * marker_; //!< pointer to the marker teammate
        const rcsc::AbstractPlayerObject * target_; //!< pointer to the target opponent

        Pair( const rcsc::AbstractPlayerObject * marker,
              const rcsc::AbstractPlayerObject * target )
            : marker_( marker )
            , target_( target )
          { }

    private:
        // not used
        Pair();
    };

    struct UnumPair {
        int marker_;
        int target_;

        UnumPair( const int marker,
                  const int target )
            : marker_( marker )
            , target_( target )
          { }

    private:
        // not used
        UnumPair();
    };

private:

    // pair of the pointer to the mark target opponent and the marker teammate
    std::vector< Pair > M_pairs;
    std::vector< UnumPair > M_unum_pairs;

    // not used
    MarkTable( const MarkTable & );
    const MarkTable & operator=( const MarkTable & );
public:

    MarkTable();

    //
    // getter methods.
    //

    const
    rcsc::AbstractPlayerObject * getTargetOf( const int marker_unum ) const;
    const
    rcsc::AbstractPlayerObject * getTargetOf( const rcsc::AbstractPlayerObject * marker ) const;

    const
    rcsc::AbstractPlayerObject * getMarkerOf( const int target_unum ) const;
    const
    rcsc::AbstractPlayerObject * getMarkerOf( const rcsc::AbstractPlayerObject * target ) const;

    const
    Pair * getPairOfMarker( const int unum ) const;
    const
    Pair * getPairOfTarget( const int unum ) const;

    const
    UnumPair * getUnumPairOfMarker( const int unum ) const;
    const
    UnumPair * getUnumPairOfTarget( const int unum ) const;


    /*!
      \brief update for the current state
     */
    void update( const rcsc::WorldModel & wm );

private:

    void createMarkTargets( const rcsc::WorldModel & wm,
                            std::vector< Target > & target_opponents );
    void createCombination( std::vector< Target * >::const_iterator first,
                            std::vector< Target * >::const_iterator last,
                            std::vector< const Marker * > & combination_stack,
                            std::vector< Assignment > & assignmentss );
    void evaluate( std::vector< Assignment > & assignmentss );
};

#endif
