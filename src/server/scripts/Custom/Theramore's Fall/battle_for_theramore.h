#ifndef BATTLE_FOR_THERAMORE_H_
#define BATTLE_FOR_THERAMORE_H_

#include "CreatureAIImpl.h"
#include "Position.h"

#define BFTScriptName "scenario_battle_for_theramore"
#define DataHeader    "BFT"

#define TERVOSH_PATH_01     6
#define KALECGOS_PATH_01    17

enum BFTData
{
    // NPCs
    DATA_JAINA_PROUDMOORE,
    DATA_ARCHMAGE_TERVOSH,
    DATA_KINNDY_SPARKSHINE,
    DATA_KALECGOS,

    // GameObjects
    DATA_PORTAL_TO_STORMWIND
};

enum BFTCreatures
{
    NPC_JAINA_PROUDMOORE 	= 64560,
    NPC_ARCHMAGE_TERVOSH 	= 500000,
    NPC_KINNDY_SPARKSHINE 	= 500001,
    NPC_KALECGOS 			= 64565,
    NPC_INVISIBLE_STALKER   = 32780
};

enum BFTGameObjets
{
    GOB_PORTAL_TO_STORMWIND = 353823
};

Position const TervoshPath01[TERVOSH_PATH_01] =
{
    { -3757.20f, -4446.58f, 30.55f, 1.42f },
    { -3756.53f, -4443.35f, 30.55f, 1.25f },
    { -3755.68f, -4441.48f, 30.55f, 0.97f },
    { -3753.32f, -4440.05f, 30.55f, 0.01f },
    { -3751.34f, -4440.64f, 30.55f, 6.14f },
    { -3749.14f, -4440.17f, 30.55f, 0.56f }
};

Position const KalecgosPath01[KALECGOS_PATH_01] =
{
    { -3746.33f, -4437.25f, 30.55f, 0.79f },
    { -3740.72f, -4431.76f, 30.55f, 0.76f },
    { -3736.23f, -4427.42f, 30.55f, 0.76f },
    { -3731.04f, -4422.38f, 30.47f, 0.78f },
    { -3726.24f, -4417.50f, 27.83f, 0.78f },
    { -3722.28f, -4413.71f, 25.91f, 0.71f },
    { -3718.76f, -4410.87f, 24.44f, 0.66f },
    { -3715.83f, -4406.72f, 22.91f, 1.24f },
    { -3713.90f, -4401.45f, 20.97f, 1.14f },
    { -3712.16f, -4397.84f, 19.47f, 1.08f },
    { -3709.95f, -4394.53f, 18.17f, 0.87f },
    { -3707.16f, -4392.07f, 17.14f, 0.52f },
    { -3703.63f, -4391.01f, 16.00f, 0.04f },
    { -3699.53f, -4391.18f, 14.48f, 6.16f },
    { -3694.77f, -4391.74f, 12.79f, 6.24f },
    { -3690.79f, -4391.58f, 11.52f, 0.11f },
    { -3685.71f, -4390.25f, 10.67f, 0.45f }
};

Position const KinndyPoint01 = { -3748.06f, -4442.12f, 30.55f, 1.24f };

template <class AI, class T>
inline AI* GetBattleForTheramoreAI(T* obj)
{
    return GetInstanceAI<AI>(obj, BFTScriptName);
}

#endif // BATTLE_FOR_THERAMORE_H_
