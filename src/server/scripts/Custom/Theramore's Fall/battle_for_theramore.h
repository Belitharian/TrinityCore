#ifndef BATTLE_FOR_THERAMORE_H_
#define BATTLE_FOR_THERAMORE_H_

#include "CreatureAIImpl.h"
#include "Position.h"

#define BFTScriptName "scenario_battle_for_theramore"
#define DataHeader    "BFT"

#define TERVOSH_PATH_01         6
#define TERVOSH_PATH_02         10
#define TERVOSH_PATH_03         16
#define KALECGOS_PATH_01        17
#define KINNDY_PATH_01          16
#define KINNDY_PATH_02          10
#define OFFICER_PATH_01         10
#define HEDRIC_PATH_01          7

#define PERITH_LOCATION         3
#define ARCHMAGES_LOCATION      6
#define ACTORS_RELOCATION       10

#define FIRE_LOCATION           32

enum class BFTPhases
{
    FindJaina,
    TheCouncil,
    Waiting,
    TheUnknownTauren,
    Evacuation,
    ALittleHelp,
    Preparation,
};

enum BFTData
{
    // NPCs
    DATA_JAINA_PROUDMOORE               = 1,
    DATA_ARCHMAGE_TERVOSH,
    DATA_KINNDY_SPARKSHINE,
    DATA_KALECGOS,
    DATA_PAINED,
    DATA_PERITH_STORMHOOVE,
    DATA_THERAMORE_OFFICER,
    DATA_HEDRIC_EVENCANE,
    DATA_RHONIN,
    DATA_VEREESA_WINDRUNNER,
    DATA_THALEN_SONGWEAVER,
    DATA_TARI_COGG,
    DATA_AMARA_LEESON,
    DATA_THADER_WINDERMERE,

    DATA_SCENARIO_PHASE,

    // GameObjects
    DATA_PORTAL_TO_STORMWIND,
    DATA_PORTAL_TO_DALARAN,
    DATA_MYSTIC_BARRIER_01,
    DATA_MYSTIC_BARRIER_02,
};

enum BFTCreatures
{
    NPC_JAINA_PROUDMOORE                = 64560,
    NPC_RHONIN                          = 64564,
    NPC_KALECGOS                        = 64565,
    NPC_HEDRIC_EVENCANE                 = 58840,
    NPC_KNIGHT_OF_THERAMORE             = 59654,
    NPC_INVISIBLE_STALKER               = 32780,
    NPC_THERAMORE_OFFICER               = 58913,
    NPC_THERAMORE_FOOTMAN               = 58612,
    NPC_THERAMORE_FAITHFUL              = 59595,
    NPC_THERAMORE_ARCANIST              = 59596,
    NPC_THERAMORE_CITIZEN_MALE          = 143773,
    NPC_THERAMORE_CITIZEN_FEMALE        = 143776,

    NPC_ARCHMAGE_TERVOSH 	            = 500000,
    NPC_KINNDY_SPARKSHINE 	            = 500001,
    NPC_PAINED 	                        = 500002,
    NPC_PERITH_STORMHOOVE               = 500003,
    NPC_ALLIANCE_PEASANT                = 500004,
    NPC_VEREESA_WINDRUNNER              = 500006,
    NPC_TARI_COGG                       = 500007,
    NPC_AMARA_LEESON                    = 500008,
    NPC_THADER_WINDERMERE               = 500009,
    NPC_THALEN_SONGWEAVER               = 500010,

    NPC_EVENT_THERAMORE_TRAINING        = 550000,
    NPC_EVENT_THERAMORE_FAITHFUL        = 550001,
};

enum BFTMisc
{
    // Spells
    SPELL_COSMETIC_LARGE_FIRE           = 277763,
    SPELL_COSMETIC_FIRE_LIGHT           = 320348,
    SPELL_CAMERA_SHAKE_VOLCANO          = 246439,


    // GameObjects
    GOB_MYSTIC_BARRIER                  = 323860,
    GOB_PORTAL_TO_STORMWIND             = 353823,
    GOB_PORTAL_TO_DALARAN               = 323842,

    // Criteria Trees
    CRITERIA_TREE_EVACUATION            = 1000009,
    CRITERIA_TREE_A_LITTLE_HELP         = 1000011,

    // Sounds
    SOUND_FEARFUL_CROWD                 = 15003,

    // Events
    EVENT_FIND_JAINA                    = 65800,
    EVENT_THE_COUNCIL                   = 65801,
    EVENT_WAITING                       = 65802,
    EVENT_THE_UNKNOWN_TAUREN            = 65803,
    EVENT_A_LITTLE_HELP                 = 65804,
    EVENT_SPEAK_THADER                  = 65805,
    EVENT_SPEAK_RHONIN                  = 65806,
};

enum BFTTalks
{
    SAY_REUNION_1         = 0,
    SAY_REUNION_2         = 0,
    SAY_REUNION_3         = 1,
    SAY_REUNION_4         = 1,
    SAY_REUNION_5         = 2,
    SAY_REUNION_6         = 0,
    SAY_REUNION_7         = 0,
    SAY_REUNION_8         = 1,
    SAY_REUNION_9         = 1,
    SAY_REUNION_9_BIS     = 2,
    SAY_REUNION_10        = 3,
    SAY_REUNION_11        = 2,
    SAY_REUNION_12        = 4,
    SAY_REUNION_13        = 3,
    SAY_REUNION_14        = 3,
    SAY_REUNION_15        = 5,
    SAY_REUNION_16        = 4,
    SAY_REUNION_17        = 5,

    SAY_WARN_1            = 0,
    SAY_WARN_2            = 6,
    SAY_WARN_3            = 1,
    SAY_WARN_4            = 2,
    SAY_WARN_5            = 7,
    SAY_WARN_6            = 3,
    SAY_WARN_7            = 4,
    SAY_WARN_8            = 8,
    SAY_WARN_9            = 9,
    SAY_WARN_10           = 10,
    SAY_WARN_11           = 0,
    SAY_WARN_12           = 11,
    SAY_WARN_13           = 5,
    SAY_WARN_14           = 0,
    SAY_WARN_15           = 12,
    SAY_WARN_16           = 1,
    SAY_WARN_17           = 2,
    SAY_WARN_18           = 3,
    SAY_WARN_19           = 13,
    SAY_WARN_20           = 4,
    SAY_WARN_21           = 14,
    SAY_WARN_22           = 5,
    SAY_WARN_23           = 6,
    SAY_WARN_24           = 15,
    SAY_WARN_25           = 7,
    SAY_WARN_26           = 16,
    SAY_WARN_27           = 17,
    SAY_WARN_28           = 18,
    SAY_WARN_29           = 8,
    SAY_WARN_30           = 9,
    SAY_WARN_31           = 19,
    SAY_WARN_32           = 10,
    SAY_WARN_33           = 20,
    SAY_WARN_34           = 11,
    SAY_WARN_35           = 21,
    SAY_WARN_36           = 6,
    SAY_WARN_37           = 22,

    SAY_PRE_BATTLE_1      = 0,
    SAY_PRE_BATTLE_2      = 23,
    SAY_PRE_BATTLE_3      = 1,
    SAY_PRE_BATTLE_4      = 24,
    SAY_PRE_BATTLE_5      = 0,
    SAY_PRE_BATTLE_6      = 25,
    SAY_PRE_BATTLE_7      = 26,
    SAY_PRE_BATTLE_8      = 0,
    SAY_PRE_BATTLE_9      = 27,
    SAY_PRE_BATTLE_10     = 28,
    SAY_PRE_BATTLE_11     = 1,
    SAY_PRE_BATTLE_12     = 29,
    SAY_PRE_BATTLE_13     = 0,
    SAY_PRE_BATTLE_14     = 30,
    SAY_PRE_BATTLE_15     = 31,
};

struct Location
{
    uint32 dataId;
    Position const position;
    Position const destination;
};

Location const perithLocation[PERITH_LOCATION] =
{
    { DATA_THERAMORE_OFFICER,    { -3733.33f, -4422.51f, 30.51f, 3.92f }, { -3746.61f, -4435.87f, 30.55f, 3.17f } },
    { DATA_PAINED,               { -3734.16f, -4425.18f, 30.55f, 3.92f }, { -3746.69f, -4437.91f, 30.55f, 3.57f } },
    { DATA_PERITH_STORMHOOVE,    { -3731.74f, -4422.76f, 30.49f, 3.92f }, { -3744.75f, -4435.91f, 30.55f, 3.49f } }
};

Location const archmagesLocation[ARCHMAGES_LOCATION] =
{
    { DATA_RHONIN,		         { 0.f, 0.f, 0.f, 0.f }, { -3718.51f, -4542.53f, 25.82f, 3.59f } },
    { DATA_VEREESA_WINDRUNNER,	 { 0.f, 0.f, 0.f, 0.f }, { -3716.33f, -4543.03f, 25.82f, 3.59f } },
    { DATA_THALEN_SONGWEAVER,    { 0.f, 0.f, 0.f, 0.f }, { -3715.66f, -4544.08f, 25.82f, 3.59f } },
    { DATA_TARI_COGG,		     { 0.f, 0.f, 0.f, 0.f }, { -3717.86f, -4539.88f, 25.82f, 3.59f } },
    { DATA_AMARA_LEESON,         { 0.f, 0.f, 0.f, 0.f }, { -3716.01f, -4540.03f, 25.82f, 3.59f } },
    { DATA_THADER_WINDERMERE,	 { 0.f, 0.f, 0.f, 0.f }, { -3717.01f, -4538.31f, 25.82f, 3.59f } }
};

Location const actorsRelocation[ACTORS_RELOCATION] =
{
    { DATA_JAINA_PROUDMOORE,     { 0.f, 0.f, 0.f, 0.f }, { -3658.39f, -4372.87f,  9.35f, 0.69f } },
    { DATA_KINNDY_SPARKSHINE,    { 0.f, 0.f, 0.f, 0.f }, { -3666.15f, -4519.95f, 10.03f, 2.44f } },
    { DATA_ARCHMAGE_TERVOSH,     { 0.f, 0.f, 0.f, 0.f }, { -3808.72f, -4541.01f, 10.68f, 3.09f } },
    { DATA_HEDRIC_EVENCANE,      { 0.f, 0.f, 0.f, 0.f }, { -3661.38f, -4376.67f,  9.35f, 0.69f } },
    { DATA_RHONIN,               { 0.f, 0.f, 0.f, 0.f }, { -3677.44f, -4521.55f, 10.21f, 0.50f } },
    { DATA_VEREESA_WINDRUNNER,   { 0.f, 0.f, 0.f, 0.f }, { -3833.79f, -4545.92f,  9.22f, 0.75f } },
    { DATA_THALEN_SONGWEAVER,    { 0.f, 0.f, 0.f, 0.f }, { -3652.05f, -4365.66f,  9.53f, 0.69f } },
    { DATA_TARI_COGG,            { 0.f, 0.f, 0.f, 0.f }, { -3786.23f, -4263.26f,  7.03f, 1.56f } },
    { DATA_AMARA_LEESON,         { 0.f, 0.f, 0.f, 0.f }, { -3649.58f, -4369.21f,  9.57f, 0.69f } },
    { DATA_THADER_WINDERMERE,    { 0.f, 0.f, 0.f, 0.f }, { -3778.58f, -4263.91f,  6.99f, 1.56f } }
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

Position const TervoshPath03[TERVOSH_PATH_03] =
{
    { -3761.81f, -4441.01f, 35.21f, 2.67f },
    { -3762.99f, -4440.28f, 35.21f, 2.37f },
    { -3763.56f, -4437.21f, 35.21f, 1.33f },
    { -3761.65f, -4434.85f, 34.83f, 0.61f },
    { -3759.32f, -4433.47f, 33.82f, 0.45f },
    { -3756.29f, -4432.44f, 32.74f, 0.16f },
    { -3752.55f, -4432.24f, 31.40f, 6.17f },
    { -3749.87f, -4433.35f, 30.55f, 5.68f },
    { -3747.98f, -4435.74f, 30.55f, 5.05f },
    { -3747.66f, -4438.79f, 30.55f, 4.53f },
    { -3748.67f, -4441.65f, 30.55f, 4.46f },
    { -3748.51f, -4444.42f, 30.55f, 4.94f },
    { -3748.51f, -4447.08f, 30.55f, 4.31f },
    { -3750.97f, -4448.87f, 30.55f, 3.45f },
    { -3753.54f, -4449.37f, 30.55f, 3.18f },
    { -3757.23f, -4449.74f, 30.55f, 4.07f }
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

Position const KinndyPath02[KINNDY_PATH_02] =
{
    { -3761.76f, -4453.74f, 37.99f, 5.44f },
    { -3759.69f, -4455.62f, 37.99f, 5.65f },
    { -3756.62f, -4457.10f, 37.99f, 6.08f },
    { -3752.35f, -4457.36f, 36.91f, 0.10f },
    { -3747.91f, -4456.21f, 35.28f, 0.51f },
    { -3744.90f, -4453.93f, 33.98f, 0.79f },
    { -3742.48f, -4450.88f, 32.63f, 1.10f },
    { -3741.36f, -4446.71f, 31.06f, 1.51f },
    { -3742.50f, -4441.13f, 30.55f, 1.95f },
    { -3745.87f, -4443.12f, 30.55f, 3.83f }
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

Position const OfficerPath01[OFFICER_PATH_01] =
{
    { -3748.30f, -4436.64f, 30.55f, 3.89f },
    { -3749.04f, -4438.16f, 30.55f, 4.76f },
    { -3747.74f, -4440.53f, 30.55f, 5.74f },
    { -3745.61f, -4440.45f, 30.55f, 0.57f },
    { -3744.42f, -4437.99f, 30.55f, 1.44f },
    { -3743.96f, -4435.54f, 30.55f, 1.20f },
    { -3742.57f, -4433.53f, 30.55f, 0.78f },
    { -3740.16f, -4431.23f, 30.55f, 0.77f },
    { -3729.92f, -4421.16f, 30.43f, 0.76f },
    { -3723.88f, -4415.31f, 26.56f, 0.76f }
};

Position const HedricPath01[HEDRIC_PATH_01]
{
    { -3717.79f, -4522.24f, 25.82f, 5.16f },
    { -3714.91f, -4528.24f, 25.82f, 5.16f },
    { -3713.09f, -4532.02f, 25.82f, 5.16f },
    { -3711.22f, -4535.91f, 25.82f, 5.16f },
    { -3710.03f, -4538.38f, 25.82f, 5.16f },
    { -3712.85f, -4539.80f, 25.82f, 3.60f },
    { -3716.85f, -4541.81f, 25.82f, 3.60f }
};

Position const FireLocation[FIRE_LOCATION]
{
    { -3678.40f, -4371.85f, 11.68f, 2.03f },
    { -3667.64f, -4363.02f, 10.83f, 1.19f },
    { -3651.10f, -4380.16f, 10.88f, 2.42f },
    { -3677.23f, -4398.54f, 10.88f, 0.00f },
    { -3649.54f, -4408.82f, 10.50f, 0.00f },
    { -3620.09f, -4439.64f, 13.71f, 5.01f },
    { -3605.37f, -4456.89f, 32.25f, 4.97f },
    { -3707.90f, -4522.16f, 37.54f, 0.32f },
    { -3693.52f, -4541.62f, 12.30f, 0.89f },
    { -3640.06f, -4517.35f, 9.462f, 2.54f },
    { -3662.97f, -4483.28f, 10.67f, 1.46f },
    { -3709.40f, -4500.12f, 12.53f, 3.76f },
    { -3790.05f, -4526.94f, 11.03f, 3.79f },
    { -3794.72f, -4538.27f, 10.89f, 1.98f },
    { -3836.88f, -4554.55f, 9.221f, 1.72f },
    { -3876.13f, -4540.68f, 11.20f, 2.08f },
    { -3708.28f, -4468.44f, 30.39f, 3.34f },
    { -3702.75f, -4419.57f, 44.48f, 1.29f },
    { -3739.75f, -4400.20f, 25.44f, 0.00f },
    { -3743.15f, -4423.85f, 27.96f, 4.65f },
    { -3780.53f, -4376.54f, 16.14f, 2.84f },
    { -3781.71f, -4328.35f, 10.04f, 1.32f },
    { -3750.42f, -4317.50f, 9.403f, 0.00f },
    { -3701.99f, -4325.82f, 11.38f, 0.00f },
    { -3702.83f, -4340.57f, 11.41f, 4.71f },
    { -3800.27f, -4278.82f, 26.05f, 6.27f },
    { -3793.82f, -4365.30f, 27.49f, 4.97f },
    { -3835.34f, -4392.50f, 11.02f, 3.50f },
    { -3858.21f, -4435.02f, 10.52f, 2.29f },
    { -3834.65f, -4441.25f, 24.46f, 0.00f },
    { -3824.27f, -4452.03f, 12.81f, 3.02f },
    { -3793.71f, -4471.57f, 14.37f, 0.00f }
};

Position const KinndyPoint01    = { -3748.06f, -4442.12f, 30.55f, 1.24f };
Position const KinndyPoint02    = { -3725.93f, -4543.47f, 25.82f, 0.11f };
Position const JainaPoint01     = { -3751.32f, -4438.13f, 30.55f, 0.40f };
Position const JainaPoint02     = { -3731.47f, -4547.05f, 27.11f, 0.25f };
Position const PainedPoint01    = { -3747.93f, -4442.05f, 30.54f, 1.54f };
Position const OfficerPoint01   = { -3748.43f, -4432.99f, 30.54f, 4.66f };
Position const QuillPoint01     = { -3751.32f, -4438.13f, 31.26f, 3.33f };
Position const TervoshPoint01   = { -3720.83f, -4551.10f, 25.82f, 1.35f };
Position const KalecgosPoint01  = { -3730.39f, -4550.39f, 27.11f, 0.54f };
Position const PortalPoint01    = { -3712.42f, -4539.62f, 25.82f, 3.59f };
Position const HedricPoint01    = { -3717.79f, -4522.24f, 25.82f, 5.16f };
Position const HedricPoint02    = { -3725.24f, -4540.07f, 25.82f, 5.98f };

template <class AI, class T>
inline AI* GetBattleForTheramoreAI(T* obj)
{
    return GetInstanceAI<AI>(obj, BFTScriptName);
}

#endif // BATTLE_FOR_THERAMORE_H_
