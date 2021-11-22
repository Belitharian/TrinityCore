#ifndef RUINS_OF_THERAMORE_H_
#define RUINS_OF_THERAMORE_H_

#include "CreatureAIImpl.h"
#include "Position.h"

#define RFTScriptName "scenario_ruins_of_theramore"
#define DataHeader "RFT"

#define DEBUG

#define KALECGOS_PATH_01                5

#define ELEMENTALS_SIZE                 2

enum RFTCreatures
{
    NPC_HEDRIC_EVENCANE		            = 58840,
    NPC_THERAMORE_OFFICER	            = 58913,
    NPC_THERAMORE_FAITHFUL	            = 59595,
    NPC_THERAMORE_ARCANIST	            = 59596,
    NPC_KALECGOS			            = 64565,
    NPC_JAINA_PROUDMOORE	            = 64727,
    NPC_ROKNAH_GRUNT		            = 64732,
    NPC_ROKNAH_LOA_SINGER	            = 64733,
    NPC_ROKNAH_HAG			            = 64734,
    NPC_ROKNAH_WARLORD	                = 65442,
    NPC_ROKNAH_SKIRMISHER               = 65494,
    NPC_ROKNAH_FELCASTER	            = 65507,
    NPC_WATER_ELEMENTAL                 = 65680,
    NPC_GENERAL_TIRAS_ALAN	            = 100007,
    NPC_ADMIRAL_AUBREY		            = 121953,
    NPC_BOMBARDING_ZEPPELIN             = 136957,
    NPC_ARCHMAGE_TERVOSH	            = 500000,
    NPC_KINNDY_SPARKSHINE	            = 500001,
    NPC_DEAD_ROKNAH_TROOP               = 500015
};

enum RFTData
{
    DATA_JAINA_PROUDMOORE               = 1,
    DATA_KALECGOS,
    DATA_KINNDY_SPARKSHINE,
    DATA_SCENARIO_PHASE
};

enum RFTTalks
{
    SAY_AFTER_BATTLE_KALECGOS_01 		= 10,
    SAY_AFTER_BATTLE_JAINA_02 			= 0,
    SAY_AFTER_BATTLE_JAINA_03 			= 1,
    SAY_AFTER_BATTLE_KALECGOS_04		= 11,
    SAY_AFTER_BATTLE_JAINA_05 			= 2,
    SAY_AFTER_BATTLE_KALECGOS_06		= 12,
    SAY_AFTER_BATTLE_JAINA_07 			= 3,
    SAY_AFTER_BATTLE_KALECGOS_08		= 13,
    SAY_AFTER_BATTLE_JAINA_09 			= 4,
    SAY_AFTER_BATTLE_KALECGOS_10		= 14,
    SAY_AFTER_BATTLE_KALECGOS_11		= 15,
    SAY_AFTER_BATTLE_JAINA_12 			= 5,
    SAY_AFTER_BATTLE_KALECGOS_13		= 16,

    SAY_IRIS_PROTECTION_JAINA_01        = 6,
    SAY_IRIS_PROTECTION_JAINA_02        = 7,
    SAY_IRIS_PROTECTION_JAINA_03        = 8,
    SAY_IRIS_PROTECTION_JAINA_04        = 9,
    SAY_IRIS_PROTECTION_JAINA_05        = 10,
    SAY_IRIS_PROTECTION_JAINA_06        = 11,
    SAY_IRIS_PROTECTION_JAINA_07        = 12,
};

enum RFTSpells
{
    SPELL_TELEPORT_VISUAL_ONLY			= 51347,
    SPELL_COSMETIC_PURPLE_VERTEX_STATE	= 83237,
    SPELL_SUMMON_WATER_ELEMENTALS       = 84374,
    SPELL_ECHO_OF_ALUNETH_SPAWN			= 211768,
    SPELL_ALUNETH_FREED_EXPLOSION		= 225253,
    SPELL_ALUNETH_DRINKS				= 212220,
    SPELL_WATER_BOSS_ENTRANCE           = 240261,
    SPELL_COSMETIC_ARCANE_DISSOLVE		= 254799,
    SPELL_SHIMMERDUST					= 278917,
    SPELL_ARCANE_CHANNELING             = 294676,
    SPELL_EMPOWERED_SUMMON              = 303681,
    SPELL_PARACHUTE                     = 314810,
    SPELL_SCREEN_FX						= 337213,
    SPELL_GLACIAL_SPIKE_COSMETIC		= 346559,
};

enum RFTMisc
{
    // GameObjects

    // Events
    EVENT_FIND_JAINA_01                 = 65811,    // Isle
    EVENT_HELP_KALECGOS                 = 65812,
    EVENT_FIND_JAINA_02                 = 65813,    // Crater
    EVENT_BACK_TO_SENDER                = 65814,

    // Criteria Tree
    CRITERIA_TREE_FIND_JAINA_01         = 1000031,  // Isle
    CRITERIA_TREE_HELP_KALECGOS         = 1000033,
    CRITERIA_TREE_FIND_JAINA_02         = 1000035,  // Crater
    CRITERIA_TREE_CLEANING              = 1000037,
    CRITERIA_TREE_BACK_TO_SENDER        = 1000040,

    // Point Id
	MOVEMENT_INFO_POINT_NONE            = 0,
	MOVEMENT_INFO_POINT_01              = 89644940,
	MOVEMENT_INFO_POINT_02              = 89644941,
	MOVEMENT_INFO_POINT_03              = 89644942,
};

enum class RFTPhases
{
    FindJaina_Isle,
    FindJaina_Isle_Valided,
    FindJaina_Crater,
    FindJaina_Crater_Valided,
    Standards,
    Standards_Valided,
    BackToSender,
};

struct SpawnPoint
{
    SpawnPoint(Position spawn, Position destination) : spawn(spawn), destination(destination)
    {
    }

    const Position spawn;
    const Position destination;
};

const SpawnPoint ElementalsPoint[ELEMENTALS_SIZE] =
{
    {
        { -3714.59f, -4465.40f, -19.87f, 6.26f },
        { -3699.80f, -4453.12f, -20.12f, 6.26f },
    },
    {
        { -3714.30f, -4472.51f, -20.15f, 6.26f },
        { -3698.08f, -4482.82f, -19.68f, 6.26f }
    }
};

const SpawnPoint ZeppelinPoint =
{
    { -3681.30f, -4568.94f, 52.12f, 1.09f },
    { -3479.77f, -4314.78f, 37.16f, 0.89f }
};

const Position KalecgosPath01[KALECGOS_PATH_01] =
{
    { -3044.71f, -4328.60f, 7.38f, 0.64f },
    { -3040.14f, -4325.38f, 6.72f, 0.55f },
    { -3031.74f, -4324.43f, 7.66f, 5.94f },
    { -3024.75f, -4327.67f, 7.68f, 5.77f },
    { -3013.87f, -4332.47f, 7.10f, 4.84f }
};

const Position PlayerPoint01    = { -3878.20f, -4589.90f,   8.67f, 0.78f };
const Position JainaPoint01     = { -3012.72f, -4340.22f,   6.64f, 1.70f };
const Position JainaPoint02     = { -3698.21f, -4457.47f, -20.88f, 1.27f };
const Position JainaPoint03     = { -3711.41f, -4467.89f, -20.54f, 0.02f };
const Position KalecgosPoint01  = { -3013.10f, -4336.91f,   6.77f, 4.82f };
const Position DummyPoint01     = { -3698.69f, -4467.94f, -20.87f, 3.55f };

inline Position GetRandomPosition(Position center, float dist)
{
    float alpha = 2 * float(M_PI) * float(rand_norm());
    float r = dist * sqrtf(float(rand_norm()));
    float x = r * cosf(alpha) + center.GetPositionX();
    float y = r * sinf(alpha) + center.GetPositionY();
    return { x, y, center.GetPositionZ(), 0.f };
}

inline void DoTeleportPlayers(Map::PlayerList const& players, const Position center, float dist = 8.0f)
{
    if (players.isEmpty())
        return;

    for (Map::PlayerList::const_iterator i = players.begin(); i != players.end(); ++i)
    {
        Position pos = GetRandomPosition(center, dist);
        if (Player* player = i->GetSource())
        {
            player->CastSpell(player, SPELL_TELEPORT_VISUAL_ONLY, true);
            player->UpdateGroundPositionZ(pos.GetPositionX(), pos.GetPositionY(), pos.m_positionZ);
            player->NearTeleportTo(pos);
        }
    }
}

template <class AI, class T>
inline AI* GetRuinsOfTheramoreAI(T* obj)
{
	return GetInstanceAI<AI>(obj, RFTScriptName);
}

#endif // RUINS_OF_THERAMORE_H_
