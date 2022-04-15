#ifndef RUINS_OF_THERAMORE_H_
#define RUINS_OF_THERAMORE_H_

#include "CreatureAIImpl.h"
#include "PhasingHandler.h"
#include "Position.h"

#define DLPScriptName "scenario_dalaran_purge"
#define DataHeader "DLP"

#define RATHAELLA_PATH_01               25
#define RATHAELLA_PATH_02               16
#define ROMMATH_PATH_01                 28

#define CUSTOM_DEBUG

enum DLPCreatures
{
	NPC_WANTON_HOSTESS				    = 16459,
	NPC_INVISIBLE_STALKER               = 32780,
	NPC_NARASI_SNOWDAWN                 = 67997,
	NPC_ARCANIST_RATHAELLA			    = 68049,
	NPC_GRAND_MAGISTER_ROMMATH          = 68589,
	NPC_SORIN_MAGEHAND				    = 68587,
	NPC_JAINA_PROUDMOORE_PATROL		    = 68609,
	NPC_SILVER_COVENANT_JAILER          = 68616,
	NPC_ARCHMAGE_LANDALOCK			    = 68617,
	NPC_MAGE_COMMANDER_ZUROS		    = 68632,
	NPC_JAINA_PROUDMOORE			    = 68677,
	NPC_SUMMONED_WATER_ELEMENTAL	    = 68678,
	NPC_AETHAS_SUNREAVER			    = 68679,
	NPC_HIGH_SUNREAVER_MAGE			    = 68680,
	NPC_SUNREAVER_MAGE                  = 68050,
	NPC_VEREESA_WINDRUNNER			    = 68687,
	NPC_SUNREAVER_CITIZEN			    = 68695,
	NPC_MAGISTER_HATHOREL			    = 68715,
	NPC_HIGH_ARCANIST_SAVOR             = 68714,
	NPC_MAGISTER_SURDIEL			    = 68716,
	NPC_MAGISTER_BRASAEL			    = 68751,
	NPC_BOUND_WATER_ELEMENTAL		    = 68956,
	NPC_ICE_WALL					    = 178819,
	NPC_WANTON_HOST				        = 183317,
	NPC_DALARAN_CITIZEN                 = 500016,
	NPC_SUNREAVER_CITIZEN_STASIS        = 500017,
	NPC_SUNREAVER_EXTRACTION_TROOP      = 500019,
	NPC_ARCANE_BARRIER                  = 550003,
};

enum DLPData
{
	DATA_JAINA_PROUDMOORE,
	DATA_JAINA_PROUDMOORE_PATROL,
	DATA_AETHAS_SUNREAVER,
	DATA_SUMMONED_WATER_ELEMENTAL,
	DATA_BOUND_WATER_ELEMENTAL,
	DATA_SORIN_MAGEHAND,
	DATA_MAGE_COMMANDER_ZUROS,
	DATA_MAGISTER_SURDIEL,
	DATA_ARCHMAGE_LANDALOCK,
	DATA_MAGISTER_HATHOREL,
	DATA_ARCANIST_RATHAELLA,
	DATA_VEREESA_WINDRUNNER,
	DATA_HIGH_ARCANIST_SAVOR,
	DATA_NARASI_SNOWDAWN,
	DATA_GRAND_MAGISTER_ROMMATH,
	DATA_MAGISTER_BRASAEL,
	DATA_SECRET_PASSAGE,
	DATA_SCENARIO_PHASE
};

enum DLPTalks
{
	// Aethas
	SAY_PURGE_JAINA_01                  = 0,
	SAY_PURGE_JAINA_02                  = 1,
	SAY_PURGE_AETHAS_03                 = 0,
	SAY_PURGE_JAINA_04                  = 2,
	SAY_PURGE_AETHAS_05                 = 1,
	SAY_PURGE_JAINA_06                  = 3,
	SAY_PURGE_JAINA_07                  = 4,

	// First Step
	SAY_FIRST_STEP_VEREESA_01           = 0,
	SAY_FIRST_STEP_JAINA_02             = 5,
	SAY_FIRST_STEP_JAINA_03             = 6,
	SAY_FIRST_STEP_VEREESA_04           = 1,

	// Brasael
	SAY_BRASAEL_JAINA_01                = 7,

	// Purge
	SAY_JAINA_PURGE_SLAIN               = 0,
	SAY_JAINA_PURGE_TELEPORT            = 1,

	// Grand Arcanist Savor
	SAY_SAVOR_JAINA_01                  = 8,

	// Romath
	SAY_INFILTRATE_ROMMATH_01           = 0,
	SAY_INFILTRATE_ROMMATH_02           = 1,
	SAY_INFILTRATE_ROMMATH_03           = 2,
	SAY_INFILTRATE_ROMMATH_04           = 3,
	SAY_INFILTRATE_ROMMATH_05           = 4,
	SAY_INFILTRATE_ROMMATH_06           = 5,
	SAY_INFILTRATE_ROMMATH_07           = 6,
};

enum DLPSpells
{
	SPELL_TELEPORT_VISUAL               = 7077,
	SPELL_TELEPORT_VISUAL_ONLY		    = 51347,
	SPELL_COSMETIC_YELLOW_ARROW         = 92230,
	SPELL_WATERFALL                     = 125563,
	SPELL_FROSTBOLT                     = 135285,
	SPELL_CHAT_BUBBLE                   = 140812,
	SPELL_HORDE_ILLUSION                = 161013,
	SPELL_FROST_CANALISATION            = 192353,
	SPELL_CLOSE_PORTAL                  = 203542,
	SPELL_RUNES_OF_SHIELDING        	= 217859,
	SPELL_TELEPORT_CASTER               = 238689,
	SPELL_WAND_OF_DISPELLING            = 243043,
	SPELL_FACTION_OVERRIDE              = 195838,
	SPELL_CASTER_READY_01               = 245843,
	SPELL_CASTER_READY_02               = 245848,
	SPELL_CASTER_READY_03               = 245849,
	SPELL_ARCANE_IMPRISONMENT           = 246016,
	SPELL_DISSOLVE                      = 255295,
	SPELL_READING_BOOK_STANDING         = 258793,
	SPELL_BURNING                       = 282051,
	SPELL_ATTACHED                      = 262121,
	SPELL_ARCANE_BARRIER                = 264849,
	SPELL_TELEPORT_TARGET               = 268294,
	SPELL_PORTAL_CHANNELING_01          = 286636,
	SPELL_PORTAL_CHANNELING_02          = 287432,
	SPELL_PORTAL_CHANNELING_03          = 288451,
	SPELL_HOLD_BAG                      = 288787,
	SPELL_FADING_TO_BLACK               = 296001,
	SPELL_WATER_CHANNELING              = 305033,
	SPELL_CHILLING_BLAST                = 337053,
	SPELL_ICY_GLARE                     = 338517,
	SPELL_FROZEN_SOLID                  = 290090,
	SPELL_ARCANE_BOMBARDMENT            = 352556,
	SPELL_TELEPORT                      = 357601,
};

enum DLPMisc
{
	// Events
	EVENT_FIND_JAINA_01                 = 65817,
	EVENT_ASSIST_JAINA                  = 65818,
	EVENT_FIND_JAINA_02                 = 65819,
	EVENT_SPEAK_TO_JAINA                = 65820,
	EVENT_FIND_ROMMATH_01               = 65821,
	EVENT_FREE_AETHAS_SUNREAVER         = 65822,

	// Factions
	FACTION_DALARAN_PATROL              = 2618,

	// Phases
	PHASE_SPECIFIC_ZONE_A               = 170,

	// Guids
	GUID_PLAYER                         = 1,
	GUID_STALKER                        = 2,

	// Actions
	ACTION_DISPELL_BARRIER              = 5000000,
	ACTION_ARCANE_ORB_DESPAWN           = 5000001,
	ACTION_HORDE_PORTAL_SPAWN           = 5000002,

	// Gob
	GOB_LAMP_POST                       = 192854,
	GOB_SECRET_PASSAGE                  = 251033,
	GOB_PORTAL_TO_SILVERMOON            = 323854,
	GOB_MYSTIC_BARRIER_01               = 323860,
	GOB_ICE_WALL_COLLISION              = 368620,
	GOB_ARCANE_FIELD                    = 550001,

	// Criteria Trees
	CRITERIA_TREE_DALARAN               = 1000047,  // Purple Citadel
	CRITERIA_TREE_FINDING_THE_THIEVES   = 1000049,
	CRITERIA_TREE_A_FACELIFT            = 1000051,
	CRITERIA_TREE_FIRST_STEP            = 1000053,
	CRITERIA_TREE_UNFORTUNATE_CAPTURE   = 1000055,
	CRITERIA_TREE_SERVE_AND_PROTECT     = 1000059,
	CRITERIA_TREE_CASHING_OUT           = 1000061,
	CRITERIA_TREE_REMAINING_SUNREAVERS  = 1000064,
	CRITERIA_TREE_THE_ESCAPE            = 1000066,  // The Escape - Parent
	CRITERIA_TREE_SPEAK_TO_JAINA        = 1000067,  // The Escape - Criteria Tree 1
	CRITERIA_TREE_FIND_ROMMATH          = 1000068,  // The Escape - Criteria Tree 2
	CRITERIA_TREE_HANDS_OF_THE_CHEF     = 1000070,

	// Point Id
	MOVEMENT_INFO_POINT_NONE            = 0,
	MOVEMENT_INFO_POINT_01              = 89644940,
	MOVEMENT_INFO_POINT_02              = 89644941,
	MOVEMENT_INFO_POINT_03              = 89644942
};

enum class DLPPhases : uint32
{
	None,
	FindJaina01,                        // Purple Citadel (PC)
	FindingTheThieves,                  // PC Indoors
	FindJaina02,                        // PC Stairs
	FreeTheArcanist,
	FreeCitizens,
	KillMagisters,
	RemainingSunreavers,
	TheEscape,
	TheEscape_Events,
	TheEscape_Escort,
	InTheHandsOfTheChief,
};

inline Position const GetRandomPosition(Position center, float dist)
{
	float alpha = 2 * float(M_PI) * float(rand_norm());
	float r = dist * sqrtf(float(rand_norm()));
	float x = r * cosf(alpha) + center.GetPositionX();
	float y = r * sinf(alpha) + center.GetPositionY();
	return { x, y, center.GetPositionZ(), 0.f };
}

inline Position const GetRandomPosition(Unit* target, float dist, bool fill = true)
{
	// Get center position
	const Position center = target->GetPosition();

	// Random angle
	float alpha = 2 * float(M_PI) * float(rand_norm());

	// Random radius
	float r = fill
		? dist * sqrtf(float(rand_norm()))
		: dist;

	// Get X and Y position around the center with radius
	float x = r * cosf(alpha) + center.GetPositionX();
	float y = r * sinf(alpha) + center.GetPositionY();

	// Get height map Z position
	float z = center.GetPositionZ();
	target->UpdateGroundPositionZ(x, y, z);

	// Get orientation angle
	const Position position = { x, y, z };
	float o = position.GetAbsoluteAngle(center);

	// Set final position
	return { x, y, z, o };
}

inline Position const GetRandomPositionAroundCircle(Unit* target, float angle, float radius)
{
	// Get center position
	const Position center = target->GetPosition();

	// Get X and Y position around the center with radius
	float x = radius * cosf(angle) + center. GetPositionX();
	float y = radius * sinf(angle) + center.GetPositionY();

	// Get height map Z position
	float z = center.GetPositionZ();
	target->UpdateGroundPositionZ(x, y, z);

	// Get orientation angle
	const Position position = { x, y, z };
	float o = position.GetAbsoluteAngle(center);

	// Set final position
	return { x, y, z, o };
}

inline void ClosePortal(GameObject*& portal)
{
	if (!portal)
		return;

	portal->Delete();

	CastSpellExtraArgs args;
	args.SetTriggerFlags(TRIGGERED_CAST_DIRECTLY);

	const Position pos = portal->GetPosition();
	if (Creature* special = portal->SummonTrigger(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), pos.GetOrientation(), 5s))
		special->CastSpell(special, SPELL_CLOSE_PORTAL, args);

	portal = nullptr;
}

inline void TeleportPlayersAround(Creature* me, float dist = 15.0f)
{
	if (!me)
		return;

	if (Map* map = me->GetMap())
	{
		map->DoOnPlayers([me, dist](Player* player)
		{
			if (!player)
				return;

			if (player->IsWithinDist2d(me, dist))
				return;

			const Position dest = GetRandomPosition(me, dist);
			player->NearTeleportTo(dest);
		});
	}
}

const Position LandalockPos01   = { -794.86f, 4425.29f, 738.44f, 5.56f };
const Position JainaPos01       = { -855.42f, 4560.77f, 727.64f, 4.59f };
const Position JainaPos02       = { -882.10f, 4495.23f, 580.31f, 5.75f };
const Position AethasPos01      = { -850.01f, 4639.85f, 749.53f, 4.60f };
const Position AethasPos02      = { };
const Position BrasaelPos01     = { -682.22f, 4441.74f, 738.12f, 2.80f };
const Position SurdielPos01     = { -933.42f, 4577.60f, 704.96f, 5.92f };
const Position SurdielPos02     = { -900.41f, 4549.03f, 706.03f, 5.60f };
const Position ZurosPos01       = { -843.55f, 4473.24f, 588.84f, 4.50f };
const Position SewersPos01      = { -898.88f, 4593.17f, 707.75f, 5.48f };

const Position RathaellaPath01[RATHAELLA_PATH_01] =
{
	{ -787.62f, 4289.38f, 719.80f, 3.29f }, 
	{ -790.13f, 4289.03f, 719.80f, 3.29f },
	{ -791.83f, 4293.01f, 719.80f, 1.76f },
	{ -792.78f, 4297.28f, 719.80f, 1.79f },
	{ -798.43f, 4296.72f, 719.80f, 3.29f },
	{ -803.53f, 4297.07f, 719.80f, 2.72f },
	{ -808.70f, 4303.36f, 720.18f, 1.86f },
	{ -792.65f, 4306.62f, 727.28f, 0.19f },
	{ -792.02f, 4303.20f, 727.28f, 4.85f },
	{ -792.49f, 4298.60f, 727.28f, 4.81f },
	{ -789.68f, 4297.28f, 727.49f, 0.20f },
	{ -786.63f, 4297.97f, 728.97f, 0.21f },
	{ -781.63f, 4298.90f, 729.07f, 0.13f },
	{ -773.60f, 4300.01f, 729.07f, 0.45f },
	{ -771.34f, 4305.53f, 729.05f, 1.59f },
	{ -772.34f, 4312.50f, 729.05f, 1.82f },
	{ -773.95f, 4319.54f, 729.05f, 1.67f },
	{ -773.46f, 4323.00f, 729.04f, 1.06f },
	{ -771.61f, 4324.92f, 729.03f, 0.62f },
	{ -766.87f, 4327.40f, 729.05f, 0.45f },
	{ -763.05f, 4330.28f, 729.07f, 0.96f },
	{ -761.56f, 4333.57f, 729.07f, 1.30f },
	{ -761.33f, 4338.09f, 729.07f, 1.69f },
	{ -762.82f, 4346.19f, 729.07f, 1.77f },
	{ -763.60f, 4349.63f, 729.59f, 1.80f }
};

const Position RathaellaPath02[RATHAELLA_PATH_02] =
{
	{ -767.98f, 4366.10f, 728.16f, 1.07f },
	{ -764.36f, 4374.96f, 727.64f, 1.18f },
	{ -760.16f, 4385.81f, 727.35f, 1.28f },
	{ -760.22f, 4393.08f, 727.30f, 1.83f },
	{ -764.85f, 4399.75f, 728.53f, 2.40f },
	{ -772.95f, 4406.71f, 732.00f, 2.45f },
	{ -785.37f, 4417.97f, 737.79f, 2.32f },
	{ -791.04f, 4424.86f, 738.41f, 2.16f },
	{ -794.15f, 4433.51f, 738.23f, 1.72f },
	{ -793.58f, 4441.55f, 737.79f, 1.17f },
	{ -787.97f, 4448.57f, 736.76f, 0.74f },
	{ -785.27f, 4451.50f, 736.29f, 1.30f },
	{ -787.07f, 4460.11f, 735.31f, 2.24f },
	{ -792.53f, 4464.04f, 735.01f, 2.44f },
	{ -796.16f, 4468.52f, 735.01f, 2.03f },
	{ -813.66f, 4475.48f, 735.01f, 4.09f }
};

const Position RommathPath01[ROMMATH_PATH_01] =
{
	{ -896.17f, 4546.52f, 706.06f, 5.62f },
	{ -888.21f, 4540.05f, 706.08f, 5.62f },
	{ -885.38f, 4538.51f, 706.13f, 5.87f },
	{ -877.71f, 4534.02f, 706.27f, 5.60f },
	{ -872.05f, 4528.93f, 706.38f, 5.45f },
	{ -866.47f, 4521.56f, 706.60f, 5.10f },
	{ -866.02f, 4517.12f, 706.10f, 4.48f },
	{ -869.97f, 4510.04f, 706.10f, 4.02f },
	{ -877.49f, 4501.05f, 706.10f, 3.95f },
	{ -884.30f, 4495.50f, 706.10f, 3.70f },
	{ -890.75f, 4491.21f, 706.10f, 3.80f },
	{ -897.09f, 4485.56f, 706.10f, 3.95f },
	{ -903.26f, 4477.23f, 706.10f, 4.18f },
	{ -907.68f, 4468.69f, 706.10f, 4.27f },
	{ -909.67f, 4463.83f, 706.10f, 4.39f },
	{ -914.35f, 4454.64f, 706.10f, 4.19f },
	{ -919.73f, 4446.00f, 706.10f, 4.06f },
	{ -924.83f, 4439.67f, 706.10f, 3.97f },
	{ -929.49f, 4435.10f, 706.10f, 3.89f },
	{ -938.46f, 4425.56f, 706.36f, 3.99f },
	{ -945.78f, 4417.59f, 706.36f, 3.90f },
	{ -952.05f, 4411.70f, 706.23f, 3.82f },
	{ -972.35f, 4397.75f, 721.23f, 3.74f },
	{ -974.38f, 4396.35f, 721.23f, 3.74f },
	{ -981.23f, 4406.92f, 721.23f, 2.15f },
	{ -982.65f, 4410.74f, 721.23f, 1.41f },
	{ -961.94f, 4424.14f, 735.73f, 0.62f },
	{ -964.51f, 4428.35f, 735.73f, 2.11f }
};

template <class AI>
class DalaranCreatureScript : public CreatureScript
{
	public:
		DalaranCreatureScript(char const* name) : CreatureScript(name) { }
		CreatureAI* GetAI(Creature* creature) const override { return GetInstanceAI<AI>(creature, DLPScriptName); }
};

#define RegisterDalaranAI(ai_name) new DalaranCreatureScript<ai_name>(#ai_name);

#endif // RUINS_OF_THERAMORE_H_
