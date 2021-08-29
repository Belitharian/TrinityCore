#ifndef BATTLE_FOR_THERAMORE_H_
#define BATTLE_FOR_THERAMORE_H_

#include "CreatureAIImpl.h"
#include "Position.h"

#define BFTScriptName "scenario_battle_for_theramore"
#define DataHeader    "BFT"

#define TERVOSH_PATH_01     6
#define TERVOSH_PATH_02     10

#define KALECGOS_PATH_01    17

#define KINNDY_PATH_01      16

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

enum BFTEvents
{
    EVENT_FIND_JAINA                    = 65800,
    EVENT_LOCALIZE_THE_FOCUSING_IRIS    = 65801,
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

Position const TervoshPath02[TERVOSH_PATH_02] =
{
    { -3748.30f, -4439.27f, 30.55f, 0.78f },
    { -3746.77f, -4436.25f, 30.55f, 1.40f },
    { -3747.73f, -4433.83f, 30.55f, 2.15f },
    { -3751.01f, -4432.27f, 30.68f, 2.99f },
    { -3753.79f, -4432.02f, 31.87f, 3.16f },
    { -3756.99f, -4432.37f, 33.02f, 3.39f },
    { -3759.40f, -4433.32f, 33.82f, 3.63f },
    { -3761.81f, -4435.31f, 34.96f, 4.01f },
    { -3764.31f, -4439.69f, 35.21f, 0.31f },
    { -3760.45f, -4442.08f, 35.21f, 2.08f }
};

Position const KinndyPath01[KINNDY_PATH_01] =
{
    { -3747.85f, -4440.07f, 30.55f, 1.36f },
    { -3747.79f, -4437.71f, 30.55f, 0.33f },
    { -3745.78f, -4436.35f, 30.55f, 5.46f },
    { -3743.54f, -4437.23f, 30.55f, 5.03f },
    { -3742.10f, -4441.03f, 30.55f, 5.12f },
    { -3741.28f, -4444.33f, 30.55f, 4.78f },
    { -3741.65f, -4447.89f, 31.58f, 4.40f },
    { -3742.86f, -4450.98f, 32.74f, 4.24f },
    { -3744.70f, -4453.56f, 33.82f, 3.91f },
    { -3747.31f, -4455.54f, 35.00f, 3.64f },
    { -3749.82f, -4456.48f, 36.02f, 3.37f },
    { -3753.10f, -4456.95f, 37.19f, 3.15f },
    { -3755.77f, -4456.66f, 37.99f, 2.87f },
    { -3758.91f, -4455.53f, 37.99f, 2.70f },
    { -3762.38f, -4452.92f, 37.99f, 2.33f },
    { -3762.73f, -4450.42f, 37.99f, 4.62f }
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

Position const KinndyPoint01    = { -3748.06f, -4442.12f, 30.55f, 1.24f };
Position const JainaPoint01     = { -3751.32f, -4438.13f, 30.55f, 0.40f };

template <class AI, class T>
inline AI* GetBattleForTheramoreAI(T* obj)
{
    return GetInstanceAI<AI>(obj, BFTScriptName);
}

#endif // BATTLE_FOR_THERAMORE_H_
