#ifndef DEF_THE_FALLEN_DREAM_H
#define DEF_THE_FALLEN_DREAM_H

#include "CreatureAIImpl.h"

#define ATFDScriptName "instance_amirdrassil_the_fallen_dream"
#define DataHeader "FallenDream"

uint32 const EncounterCount = 5;

enum DataTypes
{
    // Encounters
    DATA_ROOT_GUARDIAN     				= 0,
    DATA_ASHEN_PRIESTESS   				= 1,
    DATA_FALLEN_FLAME_LORD 				= 2,
    DATA_QUEEN_FRANCESCA   				= 3,
    DATA_VOID_ENTITY       				= 4,

    // Misc
    DATA_INTRO_STATE
};

enum CreatureIds
{
    // Bosses
	BOSS_ROOT_GUARDIAN     				= 600000,
    BOSS_ASHEN_PRIESTESS   				= 600001,
    BOSS_FALLEN_FLAME_LORD 				= 600002,
    BOSS_QUEEN_FRANCESCA   				= 600003,
    BOSS_VOID_ENTITY       				= 600004,
};

enum SharedActions
{
    ACTION_START_INTRO 					= 0,
};

template <class AI, class T>
inline AI* GetFallenDreamAI(T* obj)
{
    return GetInstanceAI<AI>(obj, ATFDScriptName);
}

#define RegisterGetFallenDreamCreatureAI(ai_name) RegisterCreatureAIWithFactory(ai_name, GetFallenDreamAI)

#endif // DEF_THE_FALLEN_DREAM_H
