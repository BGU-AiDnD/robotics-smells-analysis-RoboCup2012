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

#ifndef ROLE_LCENTER_BACK_H
#define ROLE_LCENTER_BACK_H

#include "soccer_role.h"

namespace rcsc {
class Vector2D;
class PlayerAgent;
class Formation;
}

class RoleLCenterBack
    : public SoccerRole {
private:

public:

    RoleLCenterBack()
      { }

    ~RoleLCenterBack()
      {
          //std::cerr << "delete RoleLCenterBack" << std::endl;
      }

    virtual
    void execute( rcsc::PlayerAgent * agent );

    static
    std::string name()
      {
          return std::string( "LCenterBack" );
      }

    static
    SoccerRole * create()
      {
          return ( new RoleLCenterBack() );
      }

private:
    void doKick( rcsc::PlayerAgent * agent );
    void doMove( rcsc::PlayerAgent * agent );


    void doBasicMove( rcsc::PlayerAgent * agent,
                      const rcsc::Vector2D & home_pos );
    rcsc::Vector2D getBasicMoveTarget( rcsc::PlayerAgent * agent,
                                       const rcsc::Vector2D & home_pos,
                                       double * dist_thr );

    // unique action for each area

    void doCrossBlockAreaMove( rcsc::PlayerAgent * agent,
                               const rcsc::Vector2D & home_pos );

    void doStopperMove( rcsc::PlayerAgent * agent,
                        const rcsc::Vector2D & home_pos );

    void doDangerAreaMove( rcsc::PlayerAgent * agent,
                           const rcsc::Vector2D & home_pos );

    void doDefMidMove( rcsc::PlayerAgent * agent,
                       const rcsc::Vector2D & home_pos );

    rcsc::Vector2D getDefMidMoveTarget( rcsc::PlayerAgent * agent,
                                        const rcsc::Vector2D & home_pos );

    // support action

    void doMarkMove( rcsc::PlayerAgent * agent,
                     const rcsc::Vector2D & home_pos );

    bool doDangerGetBall( rcsc::PlayerAgent * agent,
                          const rcsc::Vector2D & home_pos );


    void doGoToPoint( rcsc::PlayerAgent * agent,
                      const rcsc::Vector2D & target_point,
                      const double & dist_thr,
                      const double & dash_power,
                      const double & dir_thr );

    double dash_power;

};

#endif

