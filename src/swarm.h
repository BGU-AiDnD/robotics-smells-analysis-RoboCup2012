#ifndef SWARM_H
#define SWARM_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <rcsc/geom/vector_2d.h>

#include <iostream>

class Swarm
{
public:

    Swarm() {}
    
    ~Swarm() {}
    
    void updateAttack(int num, rcsc::Vector2D pos)
    {
        AP[num-2] = pos;
    }
    
    void updateDefense(int num, rcsc::Vector2D pos)
    {
        DP[num-2] = pos;
    }
    
    void updateAPB(int num, rcsc::Vector2D pos)
    {
        attackPB[num-2] = pos;
    }
    
    void updateDPB(int num, rcsc::Vector2D pos)
    {
        defensePB[num-2] = pos;
    }
    
    void updateALB(int num, rcsc::Vector2D pos)
    {
        attackLB[num] = pos;
    }
    
    void updateDLB(int num, rcsc::Vector2D pos)
    {
        defenseLB[num] = pos;
    }
    
    void updateAPBX(int num, double pos)
    {
        attackPB[num-2].x = pos;
    }
    
    void updateDPBX(int num, double pos)
    {
        defensePB[num-2].x = pos;
    }
    
    void updateALBX(int num, double pos)
    {
        attackLB[num].x = pos;
    }
    
    void updateDLBX(int num, double pos)
    {
        defenseLB[num].x = pos;
    }
    
    rcsc::Vector2D getAttackPosition(int num)
    {
        return AP[num-2];
    }
    
    rcsc::Vector2D getDefensePosition(int num)
    {
        return DP[num-2];
    }
    
    rcsc::Vector2D getAPB(int num)
    {
        return attackPB[num-2];
    }
    
    rcsc::Vector2D getDPB(int num)
    {
        return defensePB[num-2];
    }
    
    rcsc::Vector2D getALB(int num)
    {
        return attackLB[num];
    }
    
    rcsc::Vector2D getDLB(int num)
    {
        return defenseLB[num];
    }
    
    double getAPBX(int num)
    {
        return attackPB[num-2].x;
    }
    
    double getDPBX(int num)
    {
        return defensePB[num-2].x;
    }
    
    double getALBX(int num)
    {
        return attackLB[num].x;
    }
    
    double getDLBX(int num)
    {
        return defenseLB[num].x;
    }
    
//private:
    
    rcsc::Vector2D AP[10];    
    
    rcsc::Vector2D DP[10];
    
    rcsc::Vector2D attackPB[10];
    
    rcsc::Vector2D defensePB[10];
    
    rcsc::Vector2D attackLB[3];            //3 neighboorhoods swarm, 2-6, 5-9, 9-11.
    
    rcsc::Vector2D defenseLB[3];

};
#endif
