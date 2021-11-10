
////////////////////////////////////////////////////////////////////

#ifndef BHV_FS_CLEAR_BALL_H
#define BHV_FS_CLEAR_BALL_H

#include <rcsc/player/soccer_action.h>

class Bhv_FSClearBall
    : public rcsc::SoccerBehavior {

private:

public:
  
  bool execute( rcsc::PlayerAgent * agent );
  bool v2( rcsc::PlayerAgent * agent );
  bool v1( rcsc::PlayerAgent * agent );

};

#endif
