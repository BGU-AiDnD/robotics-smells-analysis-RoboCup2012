
////////////////////////////////////////////////////////////////////

#ifndef BHV_FS_SHOOT_H
#define BHV_FS_SHOOT_H

#include <rcsc/player/soccer_action.h>

class Bhv_FSShoot
    : public rcsc::SoccerBehavior {

private:

public:
  
  bool execute( rcsc::PlayerAgent * agent );

};

#endif
