#ifndef DALARAN_PURGE_H_
#define DALARAN_PURGE_H_

#include "CreatureAIImpl.h"
#include "PhasingHandler.h"
#include "Position.h"

#define DLPScriptName "scenario_dalaran_purge"
#define DataHeader "DLP"

#define RATHAELLA_PATH_01               25
#define RATHAELLA_PATH_02               16
#define ROMMATH_PATH_01                 41
#define TRACKING_PATH_01                9
#define SPECTRAL_BARRIER_COUNT          8

//#define CUSTOM_DEBUG

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
    DATA_PORTAL_TO_PRISON,
    DATA_PORTAL_TO_SEWERS,
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
	SAY_INFILTRATE_ROMMATH_08           = 7,

    // Narasi & Surdiel
    SAY_INFILTRATE_NARASI_01            = 0,
    SAY_INFILTRATE_SURDIEL_02           = 2,
    SAY_INFILTRATE_SURDIEL_03           = 3,
    SAY_INFILTRATE_SURDIEL_04           = 4,
    SAY_INFILTRATE_NARASI_05            = 1,
    SAY_INFILTRATE_HATHOREL_06          = 0,
};

enum DLPSpells
{
	SPELL_TELEPORT_VISUAL               = 7077,
	SPELL_TELEPORT_VISUAL_ONLY		    = 51347,
	SPELL_COSMETIC_YELLOW_ARROW         = 92230,
	SPELL_WATERFALL                     = 125563,
	SPELL_FROSTBOLT                     = 427863,
	SPELL_CHAT_BUBBLE                   = 140812,
	SPELL_HORDE_ILLUSION                = 161013,
	SPELL_CLOSE_PORTAL                  = 203542,
	SPELL_RUNES_OF_SHIELDING        	= 217859,
    SPELL_FOR_THE_HORDE                 = 224811,
    SPELL_TELEPORT_CASTER               = 238689,
	SPELL_WAND_OF_DISPELLING            = 243043,
	SPELL_FACTION_OVERRIDE              = 195838,
    SPELL_ARCANIC_TRACKING              = 210126,
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
    SPELL_FLASHBACK_EFFECT              = 279486,
	SPELL_PORTAL_CHANNELING_01          = 286636,
	SPELL_PORTAL_CHANNELING_02          = 287432,
	SPELL_PORTAL_CHANNELING_03          = 288451,
	SPELL_HOLD_BAG                      = 288787,
	SPELL_FADING_TO_BLACK               = 296001,
    SPELL_RAINY_WEATHER                 = 296026,
	SPELL_WATER_CHANNELING              = 305033,
	SPELL_CHILLING_BLAST                = 337053,
	SPELL_ICY_GLARE                     = 338517,
	SPELL_FROZEN_SOLID                  = 290090,
	SPELL_ARCANE_BOMBARDMENT            = 352556,
	SPELL_TELEPORT                      = 357601,
	SPELL_FROST_CANALISATION            = 369850,
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

    // Creature Groups
    CREATURE_GROUP_PRISON               = 0,

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
    GOB_SPECTRAL_BARRIER                = 345361,
	GOB_PORTAL_TO_STORMWIND             = 353823,
	GOB_ICE_WALL_COLLISION              = 368620,
	GOB_PORTAL_TO_PRISON                = 550001,
	GOB_PORTAL_TO_SEWERS                = 550002,
    GOB_PORTAL_TO_LIBRARY               = 550003,

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
	CRITERIA_TREE_WHAT_HAPPENED         = 1000068,  // What happened?
	CRITERIA_TREE_FIND_ROMMATH          = 1000069,  // What happened? - Criteria Tree 1
	CRITERIA_TREE_FOLLOW_TRACKS         = 1000070,  // What happened? - Criteria Tree 2
	CRITERIA_TREE_FREE_AETHAS           = 1000071,  // What happened? - Criteria Tree 3
	CRITERIA_TREE_HANDS_OF_THE_CHEF     = 1000072,

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
	TheEscape_End,
	InTheHandsOfTheChief,
};

inline Position const GetRandomPosition(Position center, float dist)
{
	float alpha = 2 * float(M_PI) * float(rand_norm());
	float r = dist * sqrtf(float(rand_norm()));
	float x = r * cosf(alpha) + center.GetPositionX();
	float y = r * sinf(alpha) + center.GetPositionY();

    Position result = { x, y, center.GetPositionZ(), 0.f };

    float o = result.GetAbsoluteAngle(center);
    result.SetOrientation(o);

    return result;
}

inline Position const GetRandomPosition(Unit* target, float dist, bool fill = true)
{
	// Get center position
	Position center = target->GetPosition();

	// Random angle
	float alpha = 2 * float(M_PI) * float(rand_norm());

	// Random radius
	float r = fill
		? dist * sqrtf(float(rand_norm()))
		: dist;

    // Move to first collision
    target->MovePositionToFirstCollision(center, r, alpha);

    // Get orientation angle
    float o = center.GetAbsoluteAngle(target);

    // Set final position
    return { center.m_positionX, center.m_positionY, center.m_positionZ, o };
}

inline Position const GetRandomPositionAroundCircle(Unit* target, float angle, float dist)
{
	// Get center position
	Position center = target->GetPosition();

    // Move to first collision
    target->MovePositionToFirstCollision(center, dist, angle);

    // Set orientation
    float o = center.GetAbsoluteAngle(target);

	// Set final position
    return { center.m_positionX, center.m_positionY, center.m_positionZ, o };
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
const Position AethasPos02      = { -882.42f, 4498.13f, 580.31f, 5.59f };
const Position BrasaelPos01     = { -682.22f, 4441.74f, 738.12f, 2.80f };
const Position SurdielPos01     = { -933.42f, 4577.60f, 704.96f, 5.92f };
const Position SurdielPos02     = { -900.41f, 4549.03f, 706.03f, 5.60f };
const Position SurdielPos03     = { -843.55f, 4473.24f, 588.84f, 4.50f };
const Position ZurosPos01       = { -843.55f, 4473.24f, 588.84f, 4.50f };
const Position SewersPos01      = { -898.88f, 4593.17f, 707.75f, 5.48f };
const Position GuardianPos01    = { -779.74f, 4415.03f, 602.62f, 2.44f };
const Position RommathPos01     = { -679.22f, 4444.06f, 694.24f, 5.62f };
const Position RommathPos02     = { -854.29f, 4475.16f, 588.85f, 5.60f };
const Position RommathPos03     = { -875.18f, 4492.31f, 580.07f, 2.47f };
const Position HathorelPos01    = { -805.92f, 4430.16f, 598.65f, 1.76f };
const Position HathorelPos02    = { -876.22f, 4489.65f, 580.05f, 2.37f };
const Position SorinPoint01     = { -856.22f, 4477.19f, 653.60f, 4.78f };
const Position EndPortalPos01   = { -893.05f, 4506.48f, 580.45f, 5.59f };

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
    { -898.20f, 4545.18f, 706.06f, 0.21f },
    { -896.17f, 4545.25f, 706.06f, 0.00f },
    { -892.74f, 4544.54f, 706.04f, 5.83f },
    { -889.83f, 4542.13f, 706.05f, 5.43f },
    { -884.65f, 4537.16f, 706.17f, 5.77f },
    { -876.83f, 4532.60f, 706.37f, 5.67f },
    { -869.97f, 4526.40f, 706.72f, 5.43f },
    { -866.96f, 4522.56f, 706.60f, 5.22f },
    { -866.00f, 4520.08f, 706.10f, 4.91f },
    { -866.02f, 4517.44f, 706.10f, 4.51f },
    { -868.07f, 4513.34f, 706.10f, 4.08f },
    { -875.40f, 4503.94f, 706.10f, 4.08f },
    { -877.07f, 4501.67f, 706.10f, 4.00f },
    { -883.66f, 4496.70f, 706.10f, 3.70f },
    { -889.56f, 4492.92f, 706.10f, 3.75f },
    { -892.55f, 4490.53f, 706.10f, 3.85f },
    { -897.72f, 4485.26f, 706.10f, 4.02f },
    { -903.38f, 4477.20f, 706.10f, 4.18f },
    { -907.52f, 4469.28f, 706.10f, 4.32f },
    { -908.61f, 4466.26f, 706.10f, 4.40f },
    { -910.42f, 4461.33f, 706.10f, 4.22f },
    { -912.43f, 4458.21f, 706.10f, 4.08f },
    { -918.77f, 4450.02f, 706.10f, 4.02f },
    { -948.20f, 4415.15f, 706.35f, 3.96f },
    { -950.47f, 4412.93f, 706.32f, 3.82f },
    { -958.44f, 4407.01f, 709.62f, 3.76f },
    { -970.07f, 4399.00f, 720.27f, 3.69f },
    { -971.62f, 4398.08f, 721.23f, 3.61f },
    { -973.34f, 4397.72f, 721.23f, 3.02f },
    { -975.66f, 4398.88f, 721.23f, 2.37f },
    { -979.81f, 4406.33f, 721.23f, 2.17f },
    { -981.82f, 4409.07f, 721.23f, 1.99f },
    { -982.15f, 4410.79f, 721.23f, 1.50f },
    { -981.56f, 4412.44f, 721.23f, 1.00f },
    { -977.49f, 4415.15f, 724.24f, 0.47f },
    { -967.15f, 4420.29f, 733.22f, 0.46f },
    { -963.78f, 4422.03f, 735.73f, 0.61f },
    { -962.84f, 4423.48f, 735.73f, 1.34f },
    { -962.74f, 4425.25f, 735.73f, 1.73f },
    { -963.61f, 4427.41f, 735.73f, 2.03f },
    { -965.41f, 4431.36f, 735.73f, 1.85f }
};

const Position TrackingPath01[TRACKING_PATH_01] =
{
    { -944.98f, 4454.64f, 733.80f, 0.06f },
    { -926.47f, 4459.54f, 734.04f, 0.79f },
    { -920.02f, 4478.08f, 734.02f, 0.90f },
    { -902.34f, 4492.18f, 731.47f, 0.58f },
    { -891.04f, 4516.35f, 729.98f, 1.02f },
    { -874.79f, 4529.80f, 729.22f, 2.41f },
    { -901.65f, 4553.94f, 729.21f, 2.45f },
    { -925.28f, 4467.79f, 733.91f, 0.88f },
    { -925.28f, 4467.79f, 733.91f, 0.88f }
};

template <class AI>
class DalaranCreatureScript : public CreatureScript
{
	public:
		DalaranCreatureScript(char const* name) : CreatureScript(name) { }
		CreatureAI* GetAI(Creature* creature) const override { return GetInstanceAI<AI>(creature, DLPScriptName); }
};

#define RegisterDalaranAI(ai_name) new DalaranCreatureScript<ai_name>(#ai_name);

#endif // DALARAN_PURGE_H_
