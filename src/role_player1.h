/////////////////////////////////////////////////////////////////////

#ifndef GEAR_ROLE_PLAYER1_H
#define GEAR_ROLE_PLAYER1_H

#include "soccer_role.h"

#include <rcsc/game_time.h>

class RolePlayer1
    : public SoccerRole {
private:
    rcsc::GameTime M_last_goalie_kick_time;

public:
    RolePlayer1()
        : M_last_goalie_kick_time( 0, 0 )
      { }

    ~RolePlayer1()
      {
          //std::cerr << "delete RolePlayer1" << std::endl;
      }

    virtual
    void execute( rcsc::PlayerAgent * agent );

    static
    std::string name()
      {
          return std::string( "Player1" );
      }

    static
    SoccerRole * create()
      {
          return new RolePlayer1();
      }

private:
    void doKick( rcsc::PlayerAgent * agent );
    void doMove( rcsc::PlayerAgent * agent );
};

#endif
