#ifndef BATTLE_FOR_THERAMORE_H_
#define BATTLE_FOR_THERAMORE_H_

#include "CreatureAIImpl.h"

#define BfTScriptName "scenario_battle_for_theramore"

enum BFTData
{

};

enum BFTCreatures
{
    NPC_JAINA_PROUDMOORE 	= 64560,
    NPC_ARCHMAGE_TERVOSH 	= 500000,
    NPC_KINNDY_SPARKSHINE 	= 500001,
    NPC_KALECGOS 			= 64565,
};

template <class AI, class T>
inline AI* GetBattleForTheramoreAI(T* obj)
{
    return GetInstanceAI<AI>(obj, BfTScriptName);
}

#endif // BATTLE_FOR_THERAMORE_H_
