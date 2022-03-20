#ifndef RUINS_OF_THERAMORE_H_
#define RUINS_OF_THERAMORE_H_

#include "CreatureAIImpl.h"
#include "PhasingHandler.h"
#include "Position.h"

#define DLPScriptName "scenario_dalaran_purge"
#define DataHeader "DLP"

#define RATHAELLA_PATH_01               8
#define RATHAELLA_PATH_02               16

#define DEBUG

enum DLPCreatures
{
    NPC_WANTON_HOSTESS				    = 16459,
    NPC_INVISIBLE_STALKER               = 32780,
    NPC_ARCANIST_RATHAELLA			    = 68049,
    NPC_SORIN_MAGEHAND				    = 68587,
    NPC_JAINA_PROUDMOORE_PATROL		    = 68609,
    NPC_ARCHMAGE_LANDALOCK			    = 68617,
    NPC_MAGE_COMMANDER_ZUROS		    = 68632,
    NPC_JAINA_PROUDMOORE			    = 68677,
    NPC_SUMMONED_WATER_ELEMENTAL	    = 68678,
    NPC_AETHAS_SUNREAVER			    = 68679,
    NPC_HIGH_SUNREAVER_MAGE			    = 68680,
    NPC_VEREESA_WINDRUNNER			    = 68687,
    NPC_SUNREAVER_CITIZEN			    = 68695,
    NPC_MAGISTER_HATHOREL			    = 68715,
    NPC_MAGISTER_SURDIEL			    = 68716,
    NPC_MAGISTER_BRASAEL			    = 68751,
    NPC_BOUND_WATER_ELEMENTAL		    = 68956,
    NPC_ICE_WALL					    = 178819,
    NPC_WANTON_HOST				        = 183317,
    NPC_DALARAN_CITIZEN                 = 500016,
    NPC_ARCANE_BARRIER                  = 550003
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
};

enum DLPSpells
{
    SPELL_TELEPORT_VISUAL               = 7077,
    SPELL_TELEPORT_VISUAL_ONLY		    = 51347,
    SPELL_PHASE_CITIZENS                = 77078,
    SPELL_PHASE_HIGH_SUNREAVER_MAGES    = 78263,
    SPELL_FROSTBOLT                     = 135285,
    SPELL_CHAT_BUBBLE                   = 140812,
    SPELL_FROST_CANALISATION            = 192353,
    SPELL_RUNES_OF_SHIELDING        	= 217859,
    SPELL_TELEPORT_CASTER               = 238689,
    SPELL_WAND_OF_DISPELLING            = 243043,
    SPELL_CASTER_READY_01               = 245843,
    SPELL_CASTER_READY_02               = 245848,
    SPELL_CASTER_READY_03               = 245849,
    SPELL_DISSOLVE                      = 255295,
    SPELL_ATTACHED                      = 262121,
	SPELL_TELEPORT_TARGET               = 268294,
    SPELL_PORTAL_CHANNELING_01          = 286636,
	SPELL_PORTAL_CHANNELING_02          = 287432,
	SPELL_PORTAL_CHANNELING_03          = 288451,
    SPELL_HOLD_BAG                      = 288787,
    SPELL_CHILLING_BLAST                = 337053,
    SPELL_ICY_GLARE                     = 338517,
    SPELL_ARCANE_BOMBARDMENT            = 352556,
    SPELL_TELEPORT                      = 357601
};

enum DLPMisc
{
	// Events
	EVENT_FIND_JAINA_01                 = 65817,
	EVENT_ASSIST_JAINA                  = 65818,
	EVENT_FIND_JAINA_02                 = 65819,

	// Factions
	FACTION_DALARAN_PATROL              = 2618,

    // Phases
    PHASE_GROUP_SPECIFIC                = 368,
    PHASE_SPECIFIC_ZONE_A               = 170,

    // Guids
    GUID_PLAYER                         = 1,
    GUID_STALKER                        = 2,

    // Actions
    ACTION_DISPELL_BARRIER              = 5000000,

	// Gob
	GOB_MYSTIC_BARRIER_01               = 323860,
	GOB_ICE_WALL_COLLISION              = 368620,

	// Criteria Trees
	CRITERIA_TREE_DALARAN               = 1000047,  // Purple Citadel
	CRITERIA_TREE_FINDING_THE_THIEVES   = 1000049,
	CRITERIA_TREE_A_FACELIFT            = 1000051,
	CRITERIA_TREE_FIRST_STEP            = 1000053,
	CRITERIA_TREE_UNFORTUNATE_CAPTURE   = 1000055,
	CRITERIA_TREE_SERVE_AND_PROTECT     = 1000059,
	CRITERIA_TREE_CASHING_OUT           = 1000061,

	// Point Id
	MOVEMENT_INFO_POINT_NONE            = 0,
	MOVEMENT_INFO_POINT_01              = 89644940,
	MOVEMENT_INFO_POINT_02              = 89644941,
	MOVEMENT_INFO_POINT_03              = 89644942
};

enum class DLPPhases
{
	FindJaina01,                        // Purple Citadel (PC)
	FindingTheThieves,                  // PC Indoors
	FindJaina02,                        // PC Stairs
	FreeTheArcanist,
	FreeCitizens,
    KillBraseal,
    KillSurdiel
};

struct SpawnPoint
{
	SpawnPoint(Position spawn, Position destination) : spawn(spawn), destination(destination)
	{
	}

	const Position spawn;
	const Position destination;
};

const SpawnPoint AethasPoint01 = { { 5802.03f, 840.00f, 680.07f, 4.60f }, { 5799.24f, 810.07f, 662.07f, 4.60f } };

inline Position const GetRandomPosition(Position center, float dist)
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

const Position JainaPos01   = { 5792.42f, 732.09f, 640.20f, 4.36f };
const Position BrasaelPos01 = { 5966.97f, 613.83f, 650.62f, 2.80f };

const Position RathaellaPath01[RATHAELLA_PATH_01] =
{
    { 5947.97f, 504.74f, 650.17f, 1.43f },
    { 5948.15f, 507.34f, 650.17f, 1.66f },
    { 5946.82f, 510.48f, 650.17f, 2.27f },
    { 5944.21f, 512.83f, 650.17f, 2.37f },
    { 5941.52f, 515.71f, 650.17f, 2.58f },
    { 5937.82f, 516.24f, 650.17f, 3.53f },
    { 5935.13f, 514.87f, 650.15f, 3.61f },
    { 5931.68f, 515.82f, 650.17f, 2.30f }
};

const Position RathaellaPath02[RATHAELLA_PATH_02] =
{
    { 5924.59f, 523.65f, 650.23f, 2.30f },
    { 5915.04f, 534.27f, 650.07f, 2.30f },
    { 5908.02f, 540.63f, 649.93f, 2.58f },
    { 5901.49f, 545.44f, 645.85f, 2.35f },
    { 5897.59f, 549.84f, 641.07f, 2.20f },
    { 5893.68f, 555.25f, 639.80f, 2.22f },
    { 5889.92f, 562.90f, 639.78f, 1.82f },
    { 5886.52f, 570.65f, 640.54f, 2.36f },
    { 5880.08f, 576.79f, 643.13f, 2.38f },
    { 5869.87f, 586.02f, 648.12f, 2.46f },
    { 5862.28f, 592.31f, 650.58f, 2.44f },
    { 5851.36f, 601.52f, 650.76f, 2.44f },
    { 5840.47f, 610.71f, 650.28f, 2.44f },
    { 5822.38f, 625.94f, 647.22f, 2.43f },
    { 5813.11f, 632.93f, 647.41f, 2.66f },
    { 5799.42f, 638.74f, 647.58f, 0.77f }
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
