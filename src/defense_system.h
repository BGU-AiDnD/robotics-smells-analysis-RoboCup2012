// -*-c++-*-

/*!
  \file defense_system.h
  \brief defense related utilities Header File
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

#ifndef GEAR_DEFENSE_SYSTEM_H
#define GEAR_DEFENSE_SYSTEM_H

#include <rcsc/geom/vector_2d.h>

namespace rcsc {
class WorldModel;
}

class DefenseSystem {
private:

public:

    static
    int predict_self_reach_cycle( const rcsc::WorldModel & wm,
                                  const rcsc::Vector2D & target_point,
                                  const double & dist_thr,
                                  const bool save_recovery,
                                  double * stamina );

};

#endif
