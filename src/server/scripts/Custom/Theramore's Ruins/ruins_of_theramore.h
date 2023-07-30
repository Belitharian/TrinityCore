#ifndef RUINS_OF_THERAMORE_H_
#define RUINS_OF_THERAMORE_H_

#include "CreatureAIImpl.h"
#include "Position.h"

#define RFTScriptName "scenario_ruins_of_theramore"
#define DataHeader "RFT"

//#define CUSTOM_DEBUG

#define KALECGOS_PATH_01                5
#define FELHUNTER_PATH_01               11

#define ELEMENTALS_SIZE                 2

enum RFTCreatures
{
    NPC_INVISIBLE_STALKER               = 32780,
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
	DATA_ROKNAH_WARLORD,
	DATA_BOMBARDING_ZEPPELIN,
	DATA_BROKEN_GLASS,
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
	SAY_IRIS_PROTECTION_JAINA_06        = 0,
	SAY_IRIS_PROTECTION_JAINA_07        = 11,
	SAY_IRIS_PROTECTION_JAINA_08        = 1,
	SAY_IRIS_PROTECTION_JAINA_09        = 12,
	SAY_IRIS_PROTECTION_JAINA_10        = 2,

	SAY_LEAVE_THE_RUINS_JAINA_01        = 13,
	SAY_LEAVE_THE_RUINS_JAINA_02        = 14
};

enum RFTSpells
{
    // OLD
	//SPELL_SCREEN_FX						= 337213,

	SPELL_FEATHER_FALL                  = 130,
	SPELL_TELEPORT_VISUAL_ONLY			= 51347,
	SPELL_COSMETIC_PURPLE_VERTEX_STATE	= 83237,
	SPELL_SUMMON_WATER_ELEMENTALS       = 84374,
	SPELL_COSMETIC_ARCANE_ENERGY_1      = 211746,
	SPELL_ECHO_OF_ALUNETH_SPAWN			= 211768,
	SPELL_ALUNETH_FREED_EXPLOSION		= 225253,
	SPELL_ALUNETH_DRINKS				= 212220,
	SPELL_WATER_BOSS_ENTRANCE           = 240261,
	SPELL_COSMETIC_ARCANE_DISSOLVE		= 254799,
	SPELL_SHIMMERDUST					= 278917,
    SPELL_SKYBOX_EFFECT_ENTRANCE        = 279122,
	SPELL_BURNING                       = 282051,
	SPELL_ARCANE_CHANNELING             = 294676,
	SPELL_EMPOWERED_SUMMON              = 303681,
    SPELL_SKYBOX_EFFECT_RUINS           = 310302,
	SPELL_GLACIAL_SPIKE_COSMETIC		= 346559,
    SPELL_THALYSSRA_SPAWNS              = 302492,
    SPELL_EXPLOSIVE_BRAND               = 374567,
    SPELL_EXPLOSIVE_BRAND_DAMAGE        = 374570,
};

enum RFTMisc
{
	// GameObjects
	GOB_BROKEN_GLASS                    = 349872,
	GOB_PORTAL_TO_STORMWIND             = 353823,

	// Sounds
	SOUND_ZEPPELIN_FLIGHT               = 85549,

	// Events
	EVENT_FIND_JAINA_01                 = 65811,    // Isle
	EVENT_HELP_KALECGOS                 = 65812,
	EVENT_FIND_JAINA_02                 = 65813,    // Crater
	EVENT_BACK_TO_SENDER                = 65814,
	EVENT_WARLORD_ROKNAH_SLAIN	        = 65815,
	EVENT_JAINA_PROTECTED               = 65816,

	// Criteria Tree
	CRITERIA_TREE_FIND_JAINA_01         = 1000031,  // Isle
	CRITERIA_TREE_HELP_KALECGOS         = 1000033,
	CRITERIA_TREE_FIND_JAINA_02         = 1000035,  // Crater
	CRITERIA_TREE_CLEANING              = 1000037,
	CRITERIA_TREE_BACK_TO_SENDER        = 1000040,
	CRITERIA_TREE_THE_LAST_STAND        = 1000042,
	CRITERIA_TREE_WARLORD_ROKNAH        = 1000043,
	CRITERIA_TREE_LEAVE_THE_RUINS       = 1000045,

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
	TheFinalAssault,
	LeaveTheRuins
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

const Position FelhunterPath01[FELHUNTER_PATH_01] =
{
    { -3736.67f, -4289.79f, 8.68f, 4.13f },
    { -3739.53f, -4292.73f, 8.68f, 3.71f },
    { -3743.88f, -4294.67f, 8.68f, 3.34f },
    { -3750.13f, -4295.33f, 8.53f, 3.12f },
    { -3761.51f, -4294.25f, 8.26f, 3.23f },
    { -3767.71f, -4295.11f, 8.66f, 3.36f },
    { -3776.86f, -4300.76f, 9.34f, 5.15f },
    { -3762.99f, -4330.62f, 9.90f, 5.21f },
    { -3751.70f, -4351.62f, 10.6f, 5.42f },
    { -3746.43f, -4361.38f, 10.6f, 5.06f },
    { -3741.30f, -4376.29f, 10.6f, 5.03f }
};

const Position PlayerPoint01    = { -3878.20f, -4589.90f,   8.67f, 0.78f };
const Position JainaPoint01     = { -3012.72f, -4340.22f,   6.64f, 1.70f };
const Position JainaPoint02     = { -3698.21f, -4457.47f, -20.88f, 1.27f };
const Position JainaPoint03     = { -3711.41f, -4467.89f, -20.54f, 0.02f };
const Position JainaPoint04     = { -3825.77f, -4537.44f,   9.21f, 0.73f };
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

            float x = pos.GetPositionX();
            float y = pos.GetPositionY();

            Trinity::NormalizeMapCoord(x);
            Trinity::NormalizeMapCoord(y);
			player->UpdateGroundPositionZ(x, y, pos.m_positionZ);

            player->NearTeleportTo(pos);
		}
	}
}

template <class AI>
class RuinsCreatureScript : public CreatureScript
{
    public:
    RuinsCreatureScript(char const* name) : CreatureScript(name)
    {
    }
    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetInstanceAI<AI>(creature, RFTScriptName);
    }
};

#define RegisterRuinsAI(ai_name) new RuinsCreatureScript<ai_name>(#ai_name);

#endif // RUINS_OF_THERAMORE_H_
