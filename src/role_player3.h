/////////////////////////////////////////////////////////////////////

#ifndef GEAR_ROLE_PLAYER3_H
#define GEAR_ROLE_PLAYER3_H

#include "soccer_role.h"

namespace rcsc {
class Vector2D;
class PlayerAgent;
class Formation;
}

class RolePlayer3
    : public SoccerRole {
private:

public:

    RolePlayer3()
      { }

    ~RolePlayer3()
      {
          //std::cerr << "delete RolePlayer3" << std::endl;
      }

    virtual
    void execute( rcsc::PlayerAgent * agent );

    static
    std::string name()
      {
          return std::string( "Player3" );
      }

    static
    SoccerRole * create()
      {
          return ( new RolePlayer3() );
      }
      
    //swarm
      
    void swarmUpdatePosition(rcsc::PlayerAgent * agent);  
    
    void swarmAttack(rcsc::PlayerAgent * agent);
    
    void swarmDefense(rcsc::PlayerAgent * agent);      

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

public:
    static rcsc::Vector2D p3AP;        //stores last position attack swarm
     
    static rcsc::Vector2D p3DP;        //stores last position defense swarm
     
    static rcsc::Vector2D p3VAP;       //stores last speed attack swarm
     
    static rcsc::Vector2D p3VDP;       //stores last speed defense swarm
   
    static rcsc::Vector2D p3PB;        //stores last personal best particle 3
    
    static rcsc::Vector2D p3PB2;       //stores last personal best particle 2
    
    static rcsc::Vector2D p3PB4;       //stores last personal best particle 4
    
    static rcsc::Vector2D p3PB5;       //stores last personal best particle 5

    static rcsc::Vector2D p3PB6;       //stores last personal best particle 6
    
    static rcsc::Vector2D p3LB;        //stores last local best from the neighborhood                  

};

#endif
