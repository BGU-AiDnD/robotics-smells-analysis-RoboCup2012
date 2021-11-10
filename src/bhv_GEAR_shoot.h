
////////////////////////////////////////////////////////////////////

#ifndef BHV_GEAR_SHOOT_H
#define BHV_GEAR_SHOOT_H

#include <rcsc/player/soccer_action.h>
#include <rcsc/geom/vector_2d.h>

class Bhv_GEARShoot
    : public rcsc::SoccerBehavior {

private:

	rcsc::Vector2D M_shoot_point;

public:
  
  bool execute( rcsc::PlayerAgent * agent );
  
  rcsc::Vector2D getIntersectionPoint( rcsc::Vector2D & goalie, const rcsc::Vector2D & player, rcsc::Vector2D & shootPoint); 

};

#endif
