#ifndef BATTLE_FOR_THERAMORE_H_
#define BATTLE_FOR_THERAMORE_H_

#include "CreatureAIImpl.h"
#include "Position.h"
#include "Totem.h"
#include "TotemAI.h"
#include "Custom/AI/CustomAI.h"

#define BFTScriptName "scenario_battle_for_theramore"
#define DataHeader "BFT"

//#define CUSTOM_DEBUG

#define PERITH_LOCATION          3
#define ARCHMAGES_LOCATION       6
#define ARCHMAGES_RELOCATION     6
#define ACTORS_RELOCATION       10
#define BARRIERS_LOCATION        2

#define FIRE_LOCATION           32

#define HORDE_WAVES_COUNT       10
#define NUMBER_OF_WOUNDED       6
#define NUMBER_OF_CITIZENS      8
#define NUMBER_OF_TROOPS        5
#define NUMBER_OF_FIRES         8

enum class BFTPhases
{
	None,
	FindJaina,
	TheCouncil,
	Waiting,
	UnknownTauren,
	Evacuation,
	ALittleHelp,
	Preparation,
	Preparation_Rhonin,
	TheBattle,
	TheBattle_RetrieveJaina,
	TheBattle_Survive,
	HelpTheWounded,
	HelpTheWounded_Extinguish,
	WaitForAmara,
	WaitForAmara_JoinJaina,
	WaitForAmara_WaitAmara,
	RetrieveRhonin,
	RetrieveRhonin_JoinRhonin,
};

enum BFTData
{
	// NPCs
	DATA_JAINA_PROUDMOORE               = 1,
	DATA_ARCHMAGE_TERVOSH,
	DATA_KINNDY_SPARKSHINE,
	DATA_KALECGOS,
	DATA_KALECGOS_DRAGON,
	DATA_PAINED,
	DATA_PERITH_STORMHOOVE,
	DATA_KNIGHT_OF_THERAMORE,
	DATA_HEDRIC_EVENCANE,
	DATA_RHONIN,
	DATA_VEREESA_WINDRUNNER,
	DATA_THALEN_SONGWEAVER,
	DATA_TARI_COGG,
	DATA_AMARA_LEESON,
	DATA_THADER_WINDERMERE,
	DATA_ROKNAH_HAG,
	DATA_CAPTAIN_DROK,
	DATA_WAVE_CALLER_GRUHTA,
	DATA_ADMIRAL_AUBREY,

	DATA_SCENARIO_PHASE,
	DATA_WOUNDED_TROOPS,
	DATA_WAVE_GROUP_ID,

    // Kalecgos Events
    DATA_KALECGOS_CIRCLE_EVENT,
    DATA_KALECGOS_SPELL_EVENT,
    DATA_KALECGOS_CANCEL_EVENT,

	// GameObjects
	DATA_PORTAL_TO_STORMWIND,
	DATA_PORTAL_TO_DALARAN,
	DATA_PORTAL_TO_ORGRIMMAR,
	DATA_MYSTIC_BARRIER_01,
	DATA_MYSTIC_BARRIER_02,
	DATA_ENERGY_BARRIER,
    DATA_POWDER_BARREL,

	// Invokers
	DATA_WAVE_DOORS                     = 100,
	DATA_WAVE_CITADEL,
	DATA_WAVE_DOCKS,
	DATA_WAVE_WEST,
    DATA_WAVE_BOAT,
};

enum BFTCreatures
{
	NPC_JAINA_PROUDMOORE                = 64560,
	NPC_RHONIN                          = 64564,
	NPC_KALECGOS                        = 64565,
	NPC_KALECGOS_DRAGON                 = 500014,
	NPC_HEDRIC_EVENCANE                 = 58840,
	NPC_KNIGHT_OF_THERAMORE             = 59654,
	NPC_INVISIBLE_STALKER               = 32780,
	NPC_UNMANNED_TANK                   = 58788,
	NPC_CAPTAIN_DROK                    = 58870,
	NPC_THERAMORE_FAITHFUL              = 59595,
	NPC_THERAMORE_ARCANIST              = 59596,
	NPC_THERAMORE_OFFICER               = 58913,
	NPC_THERAMORE_MARKSMAN              = 59317,
	NPC_THERAMORE_FOOTMAN               = 58612,
	NPC_THERAMORE_CITIZEN_MALE          = 143773,
	NPC_THERAMORE_CITIZEN_FEMALE        = 143776,
	NPC_BISHOP_DELAVEY                  = 500022,
	NPC_ADMIRAL_AUBREY                  = 121953,

	NPC_ROKNAH_GRUNT                    = 64732,
	NPC_ROKNAH_LOA_SINGER               = 64733,
	NPC_ROKNAH_HAG                      = 64734,
	NPC_ROKNAH_FELCASTER                = 65507,
	NPC_WAVE_CALLER_GRUHTA              = 65510,

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
	NPC_THERAMORE_WOUNDED_TROOP         = 500012,
	NPC_THERAMORE_FIRE_CREDIT           = 500013
};

enum BFTMisc
{
	// Spells
	SPELL_TELEPORT_DUMMY                = 51347,
	SPELL_THERAMORE_EXPLOSION_SCENE     = 128446,
	SPELL_CHAT_BUBBLE                   = 140812,
	SPELL_SKYBOX_EFFECT                 = 148137,
    SPELL_FOR_THE_ALLIANCE              = 185265,
    SPELL_ARCANE_FX                     = 200065,
	SPELL_CAMERA_SHAKE_VOLCANO          = 246439,
	SPELL_REPAIR                        = 290880,
	SPELL_TELEPORT_TARGET               = 268294,
	SPELL_PORTAL_CHANNELING_02          = 287432,
	SPELL_PORTAL_CHANNELING_03          = 288451,
	SPELL_PORTAL_CHANNELING_01          = 286636,
	SPELL_RENEW                         = 294342,
	SPELL_THALYSSRA_SPAWNS              = 302492,
	SPELL_WATER_CHANNELLING             = 305033,
	SPELL_COSMETIC_FIRE_LIGHT           = 320348,
	SPELL_RUNIC_SHIELD                  = 346057,
	SPELL_KILL_CREDIT                   = 347365,
    SPELL_SHOW_OFF_FIRE                 = 364153,
    SPELL_CHANNEL_BLUE_MOVING           = 381450,
    SPELL_STASIS                        = 398620,
	SPELL_COSMETIC_LARGE_FIRE           = 414772,

	// GameObjects
    GOB_POWDER_BARREL                   = 294148,
	GOB_MYSTIC_BARRIER_01               = 323860,
	GOB_MYSTIC_BARRIER_02               = 323862,
	GOB_PORTAL_TO_STORMWIND             = 353823,
	GOB_PORTAL_TO_DALARAN               = 323842,
	GOB_PORTAL_TO_ORGRIMMAR             = 353822,
	GOB_LAVISH_REFRESHMENT_TABLE        = 550006,
	GOB_ENERGY_BARRIER                  = 369744,

	// Criteria Trees
	CRITERIA_TREE_FIND_JAINA            = 1000000,
	CRITERIA_TREE_LOCALIZE_JAINA        = 1000001,
	CRITERIA_TREE_THE_COUNCIL           = 1000002,
	CRITERIA_TREE_WAITING               = 1000004,
	CRITERIA_TREE_UNKNOW_TAUREN         = 1000006,
	CRITERIA_TREE_EVACUATION            = 1000008,
	CRITERIA_TREE_A_LITTLE_HELP         = 1000010,
	CRITERIA_TREE_MEET_JAINA            = 1000010,
	CRITERIA_TREE_PREPARATION           = 1000012,
	CRITERIA_TREE_TALK_TO_THADER        = 1000013,
	CRITERIA_TREE_TALK_TO_RHONIN        = 1000014,
	CRITERIA_TREE_REPAIR_TANKS          = 1000015,
	CRITERIA_TREE_THE_BATTLE            = 1000017,
	CRITERIA_TREE_RETRIEVE_JAINA        = 1000018,
	CRITERIA_TREE_SURVIVE_THE_BATTLE    = 1000019,
	CRITERIA_TREE_HELP_THE_WOUNDED      = 1000021,
	CRITERIA_TREE_FOLLOW_JAINA          = 1000022,
	CRITERIA_TREE_HELP_THE_TROOPS       = 1000023,
	CRITERIA_TREE_EXTINGUISH_FIRES      = 1000024,
	CRITERIA_TREE_WAIT_ARCHMAGE_LEESON  = 1000025,
	CRITERIA_TREE_JOIN_JAINA            = 1000026,
	CRITERIA_TREE_ARCHMAGE_LEESON       = 1000027,
	CRITERIA_TREE_RETRIEVE_RHONIN       = 1000028,
	CRITERIA_TREE_RETRIEVE              = 1000029,
	CRITERIA_TREE_REDUCE_EXPLOSION      = 1000030,
	CRITERIA_TREE_DESTROY_SEA_WOLF      = 1000074,
	CRITERIA_TREE_KILL_CAPTAIN_DROK     = 1000075,

	// Phase
	PHASE_THERAMORE_SCENE_EXPLOSION     = 1503,

	// Sounds
	SOUND_FEARFUL_CROWD                 = 15003,
	SOUND_COUNTERSPELL                  = 3227,

	// Point Id
	MOVEMENT_INFO_POINT_NONE            = 0,
	MOVEMENT_INFO_POINT_01              = 89644940,
	MOVEMENT_INFO_POINT_02              = 89644941,
	MOVEMENT_INFO_POINT_03              = 89644942,
	MOVEMENT_INFO_POINT_04              = 89644943,

	// Events
	EVENT_FIND_JAINA_01                 = 65800,    // Find Jaina - Tower
	EVENT_THE_COUNCIL                   = 65801,
	EVENT_WAITING                       = 65802,
	EVENT_THE_UNKNOWN_TAUREN            = 65803,
	EVENT_FIND_JAINA_02                 = 65804,    // Find Jaina - Citadel
	EVENT_FIND_JAINA_03                 = 65805,    // Find Jaina - Gate before the battle
	EVENT_FIND_JAINA_04                 = 65806,    // Find Jaina - Gate after the battle
	EVENT_FIND_JAINA_05                 = 65807,    // Find Jaina - After wounded / fires
	EVENT_WAIT_ARCHMAGE_LESSON          = 65808,
	EVENT_RETRIEVE_RHONIN               = 65809,
	EVENT_REDUCE_IMPACT                 = 65810,
	EVENT_DESTROY_SEA_WOLF              = 65823,
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

	SAY_BATTLE_01         = 32,
	SAY_BATTLE_02         = 33,
	SAY_BATTLE_03         = 34,
	SAY_BATTLE_04         = 35,
	SAY_BATTLE_05         = 0,
	SAY_BATTLE_06         = 0,
	
	SAY_BATTLE_ALERT      = 36,
	SAY_BATTLE_CITADEL    = 37,
	SAY_BATTLE_GATE       = 38,
	SAY_BATTLE_DOCKS      = 39,
	SAY_BATTLE_WEST       = 62,

	SAY_POST_BATTLE_01    = 43,
	SAY_POST_BATTLE_02    = 2,
	SAY_POST_BATTLE_03    = 44,
	SAY_POST_BATTLE_04    = 4,
	SAY_POST_BATTLE_05    = 45,
	SAY_POST_BATTLE_06    = 5,
	SAY_POST_BATTLE_07    = 46,
	SAY_POST_BATTLE_08    = 6,
	SAY_POST_BATTLE_09    = 47,
	SAY_POST_BATTLE_10    = 48,
	SAY_POST_BATTLE_11    = 7,
	SAY_POST_BATTLE_12    = 49,
	SAY_POST_BATTLE_13    = 50,
	SAY_POST_BATTLE_14    = 8,
	SAY_POST_BATTLE_15    = 51,

	SAY_IRIS_WARN_01      = 6,
	SAY_IRIS_WARN_02      = 52,
	SAY_IRIS_WARN_03      = 7,
	SAY_IRIS_WARN_04      = 53,
	SAY_IRIS_WARN_05      = 8,
	SAY_IRIS_WARN_06      = 54,
	SAY_IRIS_WARN_07      = 55,
	SAY_IRIS_WARN_08      = 9,
	SAY_IRIS_WARN_09      = 2,
	SAY_IRIS_WARN_10      = 0,
	SAY_IRIS_WARN_11      = 56,

	SAY_IRIS_XPLOSION_01  = 57,
	SAY_IRIS_XPLOSION_02  = 3,
	SAY_IRIS_XPLOSION_03  = 58,
	SAY_IRIS_XPLOSION_04  = 4,
	SAY_IRIS_XPLOSION_05  = 5,
	SAY_IRIS_XPLOSION_06  = 6,
	SAY_IRIS_XPLOSION_07  = 59,
	SAY_IRIS_XPLOSION_08  = 7,
	SAY_IRIS_XPLOSION_09  = 60,
	SAY_IRIS_XPLOSION_10  = 8,

	SAY_WOUNDED_TROOP     = 61,

	SAY_JAINA_SPELL_01    = 40,
	SAY_JAINA_BLIZZARD_01 = 41,
	SAY_JAINA_SLAY_01     = 42,

	SAY_KALECGOS_SPELL_01 = 0,
};

struct Location
{
	uint32 dataId;
	Position const position;
	Position const destination;
};

Location const perithLocation[PERITH_LOCATION] =
{
	{ NPC_KNIGHT_OF_THERAMORE,  { -3733.33f, -4422.51f, 30.51f, 3.92f }, { -3746.61f, -4435.87f, 30.55f, 3.17f } },
	{ NPC_PAINED,               { -3734.16f, -4425.18f, 30.55f, 3.92f }, { -3746.69f, -4437.91f, 30.55f, 3.57f } },
	{ NPC_PERITH_STORMHOOVE,    { -3731.74f, -4422.76f, 30.49f, 3.92f }, { -3744.75f, -4435.91f, 30.55f, 3.49f } }
};

Location const archmagesLocation[ARCHMAGES_LOCATION] =
{
	{ NPC_RHONIN,		        { 0.f, 0.f, 0.f, 0.f }, { -3718.51f, -4542.53f, 25.82f, 3.59f } },
	{ NPC_VEREESA_WINDRUNNER,	{ 0.f, 0.f, 0.f, 0.f }, { -3716.33f, -4543.03f, 25.82f, 3.59f } },
	{ NPC_THALEN_SONGWEAVER,    { 0.f, 0.f, 0.f, 0.f }, { -3715.66f, -4544.08f, 25.82f, 3.59f } },
	{ NPC_TARI_COGG,		    { 0.f, 0.f, 0.f, 0.f }, { -3717.86f, -4539.88f, 25.82f, 3.59f } },
	{ NPC_AMARA_LEESON,         { 0.f, 0.f, 0.f, 0.f }, { -3716.01f, -4540.03f, 25.82f, 3.59f } },
	{ NPC_THADER_WINDERMERE,	{ 0.f, 0.f, 0.f, 0.f }, { -3717.01f, -4538.31f, 25.82f, 3.59f } }
};

Location const actorsRelocation[ACTORS_RELOCATION] =
{
	{ DATA_JAINA_PROUDMOORE,     { 0.f, 0.f, 0.f, 0.f }, { -3658.39f, -4372.87f,  9.35f, 0.69f } },
	{ DATA_KINNDY_SPARKSHINE,    { 0.f, 0.f, 0.f, 0.f }, { -3666.15f, -4519.95f, 10.03f, 2.44f } },
	{ DATA_ARCHMAGE_TERVOSH,     { 0.f, 0.f, 0.f, 0.f }, { -3808.72f, -4541.01f, 10.68f, 3.09f } },
	{ DATA_HEDRIC_EVENCANE,      { 0.f, 0.f, 0.f, 0.f }, { -3661.38f, -4376.67f,  9.35f, 0.69f } },
	{ DATA_RHONIN,               { 0.f, 0.f, 0.f, 0.f }, { -3677.44f, -4521.55f, 10.21f, 0.50f } },
	{ DATA_THALEN_SONGWEAVER,    { 0.f, 0.f, 0.f, 0.f }, { -3652.05f, -4365.66f,  9.53f, 0.69f } },
	{ DATA_TARI_COGG,            { 0.f, 0.f, 0.f, 0.f }, { -3786.55f, -4276.78f,  7.99f, 1.56f } },
	{ DATA_AMARA_LEESON,         { 0.f, 0.f, 0.f, 0.f }, { -3649.58f, -4369.21f,  9.57f, 0.69f } },
	{ DATA_THADER_WINDERMERE,    { 0.f, 0.f, 0.f, 0.f }, { -3779.07f, -4276.96f,  7.93f, 1.52f } },
	{ DATA_KALECGOS,             { 0.f, 0.f, 0.f, 0.f }, { -3730.39f, -4550.39f, 27.11f, 0.54f } }
};

Location const archmagesRelocation[ARCHMAGES_RELOCATION] =
{
	{ DATA_AMARA_LEESON,         { 0.f, 0.f, 0.f, 0.f }, { -3629.75f, -4462.69f, 13.62f, 0.58f } },
	{ DATA_TARI_COGG,            { 0.f, 0.f, 0.f, 0.f }, { -3629.33f, -4458.57f, 14.33f, 5.77f } },
	{ DATA_THADER_WINDERMERE,    { 0.f, 0.f, 0.f, 0.f }, { -3611.58f, -4452.25f, 14.24f, 3.41f } },
	{ DATA_RHONIN,               { 0.f, 0.f, 0.f, 0.f }, { -3624.61f, -4456.55f, 13.62f, 4.12f } },
	{ DATA_KINNDY_SPARKSHINE,    { 0.f, 0.f, 0.f, 0.f }, { -3618.42f, -4463.22f, 13.62f, 2.41f } },
	{ DATA_ARCHMAGE_TERVOSH,     { 0.f, 0.f, 0.f, 0.f }, { -3626.40f, -4453.89f, 14.24f, 0.44f } }
};

WaypointPath const TervoshPath01 =
{
    1,
    {
        { 0, -3757.20f, -4446.58f, 30.55f, 1.42f },
        { 1, -3756.53f, -4443.35f, 30.55f, 1.25f },
        { 2, -3755.68f, -4441.48f, 30.55f, 0.97f },
        { 3, -3753.32f, -4440.05f, 30.55f, 0.01f },
        { 4, -3751.34f, -4440.64f, 30.55f, 0.46f },
        { 5, -3749.70f, -4440.37f, 30.54f, 0.46f }
    },
    WaypointMoveType::Walk,
    WaypointPathFlags::ExactSplinePath
};

WaypointPath const TervoshPath02 =
{
    2,
    {
        { 0, -3748.30f, -4439.27f, 30.55f, 0.78f },
        { 1, -3746.77f, -4436.25f, 30.55f, 1.40f },
        { 2, -3747.73f, -4433.83f, 30.55f, 2.15f },
        { 3, -3751.01f, -4432.27f, 30.68f, 2.99f },
        { 4, -3753.79f, -4432.02f, 31.87f, 3.16f },
        { 5, -3756.99f, -4432.37f, 33.02f, 3.39f },
        { 6, -3759.40f, -4433.32f, 33.82f, 3.63f },
        { 7, -3761.81f, -4435.31f, 34.96f, 4.01f },
        { 8, -3764.31f, -4439.69f, 35.21f, 0.31f },
        { 9, -3760.45f, -4442.08f, 35.21f, 2.08f }
    },
    WaypointMoveType::Walk,
    WaypointPathFlags::ExactSplinePath
};

WaypointPath const TervoshPath03 =
{
    3,
    {
        { 0 , -3761.81f, -4441.01f, 35.21f, 2.67f },
        { 1 , -3762.99f, -4440.28f, 35.21f, 2.37f },
        { 2 , -3763.56f, -4437.21f, 35.21f, 1.33f },
        { 3 , -3761.65f, -4434.85f, 34.83f, 0.61f },
        { 4 , -3759.32f, -4433.47f, 33.82f, 0.45f },
        { 5 , -3756.29f, -4432.44f, 32.74f, 0.16f },
        { 6 , -3752.55f, -4432.24f, 31.40f, 6.17f },
        { 7 , -3749.87f, -4433.35f, 30.55f, 5.68f },
        { 8 , -3747.98f, -4435.74f, 30.55f, 5.05f },
        { 9 , -3747.66f, -4438.79f, 30.55f, 4.53f },
        { 10, -3748.67f, -4441.65f, 30.55f, 4.46f },
        { 11, -3748.51f, -4444.42f, 30.55f, 4.94f },
        { 12, -3748.51f, -4447.08f, 30.55f, 4.31f },
        { 13, -3750.97f, -4448.87f, 30.55f, 3.45f },
        { 14, -3753.54f, -4449.37f, 30.55f, 3.18f },
        { 15, -3757.23f, -4449.74f, 30.55f, 4.07f }
    },
    WaypointMoveType::Walk,
    WaypointPathFlags::ExactSplinePath
};

WaypointPath const KinndyPath01 =
{
    1,
    {
        { 0 , -3747.85f, -4440.07f, 30.55f, 1.36f },
        { 1 , -3747.79f, -4437.71f, 30.55f, 0.33f },
        { 2 , -3745.78f, -4436.35f, 30.55f, 5.46f },
        { 3 , -3743.54f, -4437.23f, 30.55f, 5.03f },
        { 4 , -3742.10f, -4441.03f, 30.55f, 5.12f },
        { 5 , -3741.28f, -4444.33f, 30.55f, 4.78f },
        { 6 , -3741.65f, -4447.89f, 31.58f, 4.40f },
        { 7 , -3742.86f, -4450.98f, 32.74f, 4.24f },
        { 8 , -3744.70f, -4453.56f, 33.82f, 3.91f },
        { 9 , -3747.31f, -4455.54f, 35.00f, 3.64f },
        { 10, -3749.82f, -4456.48f, 36.02f, 3.37f },
        { 11, -3753.10f, -4456.95f, 37.19f, 3.15f },
        { 12, -3755.77f, -4456.66f, 37.99f, 2.87f },
        { 13, -3758.91f, -4455.53f, 37.99f, 2.70f },
        { 14, -3762.38f, -4452.92f, 37.99f, 2.33f },
        { 15, -3762.73f, -4450.42f, 37.99f, 4.62f }
    },
    WaypointMoveType::Walk,
    WaypointPathFlags::ExactSplinePath
};

WaypointPath const KinndyPath02 =
{
    2,
    {
        { 0, -3761.76f, -4453.74f, 37.99f, 5.44f },
        { 1, -3759.69f, -4455.62f, 37.99f, 5.65f },
        { 2, -3756.62f, -4457.10f, 37.99f, 6.08f },
        { 3, -3752.35f, -4457.36f, 36.91f, 0.10f },
        { 4, -3747.91f, -4456.21f, 35.28f, 0.51f },
        { 5, -3744.90f, -4453.93f, 33.98f, 0.79f },
        { 6, -3742.48f, -4450.88f, 32.63f, 1.10f },
        { 7, -3741.36f, -4446.71f, 31.06f, 1.51f },
        { 8, -3742.50f, -4441.13f, 30.55f, 1.95f },
        { 9, -3745.87f, -4443.12f, 30.55f, 3.83f }
    },
    WaypointMoveType::Walk,
    WaypointPathFlags::ExactSplinePath
};

WaypointPath const KalecgosPath01 =
{
    1,
    {
        { 0 , -3746.33f, -4437.25f, 30.55f, 0.79f },
        { 1 , -3740.72f, -4431.76f, 30.55f, 0.76f },
        { 2 , -3736.23f, -4427.42f, 30.55f, 0.76f },
        { 3 , -3731.04f, -4422.38f, 30.47f, 0.78f },
        { 4 , -3726.24f, -4417.50f, 27.83f, 0.78f },
        { 5 , -3722.28f, -4413.71f, 25.91f, 0.71f },
        { 6 , -3718.76f, -4410.87f, 24.44f, 0.66f },
        { 7 , -3715.83f, -4406.72f, 22.91f, 1.24f },
        { 8 , -3713.90f, -4401.45f, 20.97f, 1.14f },
        { 9 , -3712.16f, -4397.84f, 19.47f, 1.08f },
        { 10, -3709.95f, -4394.53f, 18.17f, 0.87f },
        { 11, -3707.16f, -4392.07f, 17.14f, 0.52f },
        { 12, -3703.63f, -4391.01f, 16.00f, 0.04f },
        { 13, -3699.53f, -4391.18f, 14.48f, 6.16f },
        { 14, -3694.77f, -4391.74f, 12.79f, 6.24f },
        { 15, -3690.79f, -4391.58f, 11.52f, 0.11f },
        { 16, -3685.71f, -4390.25f, 10.67f, 0.45f }
    },
    WaypointMoveType::Walk,
    WaypointPathFlags::ExactSplinePath
};

WaypointPath const KalecgosPath02 =
{
    2,
    {
        { 0, -3615.43f, -4440.26f, 14.23f, 4.72f },
        { 1, -3615.46f, -4448.43f, 14.23f, 4.72f },
        { 2, -3622.92f, -4448.69f, 14.24f, 3.18f },
        { 3, -3621.39f, -4453.81f, 14.24f, 5.10f },
        { 4, -3621.30f, -4459.18f, 13.62f, 4.72f }
    },
    WaypointMoveType::Run,
    WaypointPathFlags::ExactSplinePath
};

WaypointPath const KalecgosPath03 =
{
    3,
    {
        { 0, -3621.30f, -4459.18f, 13.62f, 4.72f },
        { 1, -3621.39f, -4453.81f, 14.24f, 5.10f },
        { 2, -3622.92f, -4448.69f, 14.24f, 3.18f },
        { 3, -3615.46f, -4448.43f, 14.23f, 4.72f },
        { 4, -3615.43f, -4440.26f, 14.23f, 4.72f }
    },
    WaypointMoveType::Run,
    WaypointPathFlags::ExactSplinePath
};

WaypointPath const RhoninPath01 =
{
    1,
    {
        { 0, -3622.97f, -4461.50f, 13.62f, 0.44f },
        { 1, -3621.30f, -4459.18f, 13.62f, 4.72f },
        { 2, -3621.39f, -4453.81f, 14.24f, 5.10f },
        { 3, -3622.92f, -4448.69f, 14.24f, 3.18f },
        { 4, -3615.46f, -4448.43f, 14.23f, 4.72f },
        { 5, -3615.43f, -4440.26f, 14.23f, 4.72f }
    },
    WaypointMoveType::Run,
    WaypointPathFlags::ExactSplinePath
};

WaypointPath const AmaraPath01 =
{
    1,
    {
        { 0, -3627.31f, -4460.97f, 13.62f, 0.32f },
        { 1, -3624.17f, -4459.42f, 13.62f, 0.44f },
        { 2, -3621.30f, -4459.18f, 13.62f, 4.72f },
        { 3, -3621.39f, -4453.81f, 14.24f, 5.10f },
        { 4, -3622.92f, -4448.69f, 14.24f, 3.18f },
        { 5, -3615.46f, -4448.43f, 14.23f, 4.72f },
        { 6, -3615.43f, -4440.26f, 14.23f, 4.72f }
    },
    WaypointMoveType::Run,
    WaypointPathFlags::ExactSplinePath
};

WaypointPath const OfficerPath01 =
{
    1,
    {
        { 0, -3748.30f, -4436.64f, 30.55f, 3.89f },
        { 1, -3749.04f, -4438.16f, 30.55f, 4.76f },
        { 2, -3747.74f, -4440.53f, 30.55f, 5.74f },
        { 3, -3745.61f, -4440.45f, 30.55f, 0.57f },
        { 4, -3744.42f, -4437.99f, 30.55f, 1.44f },
        { 5, -3743.96f, -4435.54f, 30.55f, 1.20f },
        { 6, -3742.57f, -4433.53f, 30.55f, 0.78f },
        { 7, -3740.16f, -4431.23f, 30.55f, 0.77f },
        { 8, -3729.92f, -4421.16f, 30.43f, 0.76f },
        { 9, -3723.88f, -4415.31f, 26.56f, 0.76f }
    },
    WaypointMoveType::Walk,
    WaypointPathFlags::ExactSplinePath
};

WaypointPath const HedricPath01
{
    1,
    {
        { 0, -3717.79f, -4522.24f, 25.82f, 5.16f },
        { 1, -3714.91f, -4528.24f, 25.82f, 5.16f },
        { 2, -3713.09f, -4532.02f, 25.82f, 5.16f },
        { 3, -3711.22f, -4535.91f, 25.82f, 5.16f },
        { 4, -3710.03f, -4538.38f, 25.82f, 5.16f },
        { 5, -3712.85f, -4539.80f, 25.82f, 3.60f },
        { 6, -3716.85f, -4541.81f, 25.82f, 3.60f }
    },
    WaypointMoveType::Run,
    WaypointPathFlags::ExactSplinePath
};

WaypointPath const JainaPath01 =
{
    1,
    {
        { 0 , -3661.13f, -4372.71f,  9.35f, 3.76f },
        { 1 , -3663.74f, -4374.88f,  9.35f, 3.84f },
        { 2 , -3669.37f, -4380.40f,  9.51f, 3.95f },
        { 3 , -3673.05f, -4384.35f,  9.98f, 3.98f },
        { 4 , -3674.91f, -4388.67f, 10.44f, 4.66f },
        { 5 , -3674.43f, -4391.92f, 10.60f, 4.94f },
        { 6 , -3672.68f, -4401.30f, 10.65f, 4.76f },
        { 7 , -3672.45f, -4406.29f, 10.63f, 4.81f },
        { 8 , -3670.06f, -4411.14f, 10.61f, 5.60f },
        { 9 , -3666.06f, -4413.06f, 10.79f, 6.02f },
        { 10, -3660.17f, -4414.00f, 10.57f, 6.23f },
        { 11, -3654.43f, -4414.01f, 10.36f, 6.28f },
        { 12, -3648.35f, -4414.78f, 10.10f, 6.02f },
        { 13, -3641.69f, -4416.73f,  9.85f, 5.93f },
        { 14, -3633.88f, -4420.13f,  9.68f, 5.79f },
        { 15, -3626.89f, -4424.27f, 10.36f, 5.66f },
        { 16, -3620.94f, -4429.00f, 11.27f, 5.55f },
        { 17, -3617.26f, -4432.91f, 12.46f, 5.25f },
        { 18, -3615.86f, -4435.97f, 13.30f, 4.95f },
        { 19, -3615.46f, -4439.98f, 14.23f, 4.74f },
        { 20, -3615.17f, -4448.12f, 14.23f, 3.24f },
        { 21, -3624.34f, -4448.73f, 14.24f, 5.16f },
        { 22, -3621.47f, -4454.07f, 14.24f, 4.75f },
        { 23, -3619.56f, -4461.36f, 13.62f, 2.96f }
    },
    WaypointMoveType::Run,
    WaypointPathFlags::ExactSplinePath
};

WaypointPath const JainaPath02 =
{
    2,
    {
        { 0, -3622.97f, -4461.50f, 13.62f, 0.44f },
        { 1, -3621.30f, -4459.18f, 13.62f, 4.72f },
        { 2, -3621.39f, -4453.81f, 14.24f, 5.10f },
        { 3, -3622.92f, -4448.69f, 14.24f, 3.18f },
        { 4, -3615.46f, -4448.43f, 14.23f, 4.72f },
        { 5, -3615.43f, -4440.26f, 14.23f, 4.72f }
    },
    WaypointMoveType::Run,
    WaypointPathFlags::ExactSplinePath
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

Position const UnitLocation[ARCHMAGES_RELOCATION] =
{
	{ -3615.08f, -4470.40f, 14.24f, 2.61f },
	{ -3609.72f, -4467.73f, 14.24f, 3.78f },
	{ -3619.52f, -4473.30f, 14.24f, 1.62f },
	{ -3622.97f, -4467.49f, 14.24f, 0.16f },
	{ -3618.41f, -4453.12f, 14.24f, 3.47f },
	{ -3613.38f, -4458.41f, 13.62f, 5.84f }
};

Position const KinndyPoint01    = { -3748.06f, -4442.12f, 30.55f, 1.24f };
Position const KinndyPoint02    = { -3725.93f, -4543.47f, 25.82f, 0.11f };
Position const JainaPoint01     = { -3751.32f, -4438.13f, 30.55f, 0.40f };
Position const JainaPoint02     = { -3731.47f, -4547.05f, 27.11f, 0.25f };
Position const JainaPoint03     = { -3658.39f, -4372.87f,  9.35f, 0.69f };
Position const JainaPoint04     = { -3636.94f, -4355.86f,  7.44f, 0.69f };
Position const JainaPoint05     = { -3747.73f, -4447.16f, 64.91f, 3.83f };
Position const JainaPoint06     = { -3619.56f, -4461.36f, 13.62f, 2.96f };
Position const RhoninPoint01    = { -3622.97f, -4461.50f, 13.62f, 0.44f };
Position const RhoninPoint02    = { -3744.36f, -4453.88f, 64.98f, 5.41f };
Position const PainedPoint01    = { -3747.93f, -4442.05f, 30.54f, 1.54f };
Position const OfficerPoint01   = { -3748.43f, -4432.99f, 30.54f, 4.66f };
Position const QuillPoint01     = { -3751.32f, -4438.13f, 31.26f, 3.33f };
Position const TervoshPoint01   = { -3720.83f, -4551.10f, 25.82f, 1.35f };
Position const KalecgosPoint01  = { -3730.39f, -4550.39f, 27.11f, 0.54f };
Position const PortalPoint01    = { -3712.42f, -4539.62f, 25.82f, 3.59f };
Position const PortalPoint02    = { -3782.81f, -4256.24f,  6.52f, 4.69f };
Position const PortalPoint03    = { -3750.82f, -4449.65f, 64.90f, 0.63f };
Position const HedricPoint01    = { -3717.79f, -4522.24f, 25.82f, 5.16f };
Position const HedricPoint02    = { -3725.24f, -4540.07f, 25.82f, 5.98f };
Position const HedricPoint03    = { -3625.58f, -4447.94f, 14.24f, 5.40f };
Position const ExplodingPoint01 = { -3648.24f, -4364.96f,  9.68f, 3.78f };
Position const ThalenPoint01    = { -3632.12f, -4351.22f,  6.38f, 3.79f };
Position const ThalenPoint02    = { -3728.51f, -4555.08f,  4.74f, 2.78f };
Position const TablePoint01     = { -3627.93f, -4459.00f, 13.62f, 2.60f };
Position const TheramorePoint01 = { -3753.48f, -4444.54f, 90.07f, 0.00f };
Position const TheramorePoint02 = { -3764.61f, -4327.58f,  9.82f, 5.20f };

inline Position const GetRandomPositionAroundCircle(Unit* target, float angle, float radius)
{
	// Get center position
	const Position center = target->GetPosition();

	// Get X and Y position around the center with radius
	float x = radius * cosf(angle) + center.GetPositionX();
	float y = radius * sinf(angle) + center.GetPositionY();

	// Get height map Z position
	float z = center.GetPositionZ();

	Trinity::NormalizeMapCoord(x);
	Trinity::NormalizeMapCoord(y);
	target->UpdateGroundPositionZ(x, y, z);

	// Get orientation angle
	const Position position = { x, y, z };
	float o = position.GetAbsoluteAngle(center);

	// Set final position
	return { x, y, z, o };
}

template <class AI>
class TheramoreCreatureScript : public CreatureScript
{
	public:
	TheramoreCreatureScript(char const* name) : CreatureScript(name)
	{
	}
	CreatureAI* GetAI(Creature* creature) const override
	{
		return GetInstanceAI<AI>(creature, BFTScriptName);
	}
};

#define RegisterTheramoreAI(ai_name) new TheramoreCreatureScript<ai_name>(#ai_name);

#endif // BATTLE_FOR_THERAMORE_H_
