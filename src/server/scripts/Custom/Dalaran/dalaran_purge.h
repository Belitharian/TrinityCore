#ifndef RUINS_OF_THERAMORE_H_
#define RUINS_OF_THERAMORE_H_

#include "CreatureAIImpl.h"
#include "PhasingHandler.h"
#include "Position.h"

#define DLPScriptName "scenario_dalaran_purge"
#define DataHeader "DLP"

//#define DEBUG

enum DLPCreatures
{
	NPC_JAINA_PROUDMOORE                = 68677,
	NPC_JAINA_PROUDMOORE_PATROL         = 68609,
	NPC_VEREESA_WINDRUNNER              = 68687,
	NPC_ARCHMAGE_LANDALOCK              = 68617,
	NPC_SORIN_MAGEHAND                  = 68587,
	NPC_AETHAS_SUNREAVER                = 68679,
	NPC_HIGH_SUNREAVER_MAGE             = 68680,
	NPC_SUNREAVER_CITIZEN               = 68695,
	NPC_SUMMONED_WATER_ELEMENTAL        = 68678,
	NPC_BOUND_WATER_ELEMENTAL           = 68956,
	NPC_ARCANIST_RATHAELLA              = 68049,
	NPC_MAGISTER_BRASAEL                = 68751,
	NPC_MAGISTER_HATHOREL               = 68715,
	NPC_MAGE_COMMANDER_ZUROS            = 68632,
	NPC_ICE_WALL                        = 178819,
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
	DATA_ARCHMAGE_LANDALOCK,
	DATA_MAGISTER_HATHOREL,
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

	// Purge
	SAY_JAINA_PURGE_SLAIN               = 0,
	SAY_JAINA_PURGE_TELEPORT            = 1,
};

enum DLPSpells
{
	SPELL_TELEPORT_VISUAL_ONLY		    = 51347,
	SPELL_FROSTBOLT                     = 135285,
	SPELL_DISSOLVE                      = 255295,
	SPELL_TELEPORT                      = 357601,
	SPELL_CASTER_READY_01               = 245843,
	SPELL_CASTER_READY_02               = 245848,
	SPELL_CASTER_READY_03               = 245849,
	SPELL_ARCANE_BOMBARDMENT            = 352556,
	SPELL_ICY_GLARE                     = 338517,
	SPELL_CHILLING_BLAST                = 337053,
	SPELL_FROST_CANALISATION            = 192353,
	SPELL_ATTACHED                      = 262121,
	SPELL_CHAT_BUBBLE                   = 140812,

    SPELL_PHASE_CITIZENS                = 77078,
    SPELL_PHASE_HIGH_SUNREAVER_MAGES    = 78263,
};

enum DLPMisc
{
	// Events
	EVENT_FIND_JAINA_01                 = 65817,
	EVENT_ASSIST_JAINA                  = 65818,
	EVENT_FIND_JAINA_02                 = 65819,

	// Factions
	FACTION_DALARAN_PATROL              = 2618,

	// Gob
	GOB_MYSTIC_BARRIER_01               = 323860,
	GOB_ICE_WALL_COLLISION              = 368620,

	// Criteria Trees
	CRITERIA_TREE_DALARAN               = 1000047,  // Purple Citadel
	CRITERIA_TREE_FINDING_THE_THIEVES   = 1000049,
	CRITERIA_TREE_A_FACELIFT            = 1000051,

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
	FindJaina02                         // PC Stairs
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

inline Position GetRandomPosition(Position center, float dist)
{
	float alpha = 2 * float(M_PI) * float(rand_norm());
	float r = dist * sqrtf(float(rand_norm()));
	float x = r * cosf(alpha) + center.GetPositionX();
	float y = r * sinf(alpha) + center.GetPositionY();
	return { x, y, center.GetPositionZ(), 0.f };
}

const Position JainaPos01   = { 5792.42f, 732.09f, 640.20f, 4.36f };

template <class AI>
class DalaranCreatureScript : public CreatureScript
{
	public:
		DalaranCreatureScript(char const* name) : CreatureScript(name) { }
		CreatureAI* GetAI(Creature* creature) const override { return GetInstanceAI<AI>(creature, DLPScriptName); }
};

#define RegisterDalaranAI(ai_name) new DalaranCreatureScript<ai_name>(#ai_name);

#endif // RUINS_OF_THERAMORE_H_
