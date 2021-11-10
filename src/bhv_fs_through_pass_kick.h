
////////////////////////////////////////////////////////////////////

#ifndef BHV_FS_THROUGH_PASS_KICK_H
#define BHV_FS_THROUGH_PASS_KICK_H

#include <rcsc/player/soccer_action.h>
#include <rcsc/player/world_model.h>

class Bhv_FSThroughPassKick
: public rcsc::SoccerBehavior {
  
 private:

  bool get_through_pass( const rcsc::WorldModel & world,
			 rcsc::Vector2D * target_point,
			 double * first_speed,
			 int * receiver_unum );

  bool verifyPassCource(const rcsc::WorldModel & world
			, const rcsc::Vector2D & target_point 
			, double * first_speed 
			);

 public:
  
  bool execute( rcsc::PlayerAgent * agent );
  
};

#endif
