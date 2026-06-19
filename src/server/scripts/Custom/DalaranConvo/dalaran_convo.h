#ifndef DALARAN_CONVO_H_
#define DALARAN_CONVO_H_

#include "CreatureAIImpl.h"
#include "Position.h"

constexpr char const* DLCScriptName = "scenario_dalaran_convo";
constexpr char const* DataHeader    = "DLC";

enum class Phases : uint32
{
    None,

	Introduction,

	Start,
	Start_CanTeleport,

	Visions,
	Visions_Jaina,
	Visions_KalecgosJaina,
	Visions_KelThuzad,
	Visions_KelThuzad_Combat,

	Outro
};

enum DalaranFateNPCs
{
	NPC_INVISIBLE_STALKER                   = 32780,
	NPC_ANDUIN_WRYNN		                = 68106,
	NPC_JAINA_PROUDMOORE	                = 68108,

	// Vision 1 - Kalecgos
	NPC_KALECGOS 			                = 35867,
	NPC_JAINA_PROUDMOORE_VISION             = 500029,

	// Vision 2 - Kel Thuzad
	NPC_KELTHUZAD			                = 500031,
	NPC_MR_BIGGLESWORTH                     = 206009,
	NPC_CAULDRON_BUNNY                      = 41505,

	// Vision 3 - Kael'thas
	NPC_KAELTHAS                            = 500026,
	NPC_BLOOD_ELF_NOBLE                     = 500028,

	// Vision 4 - Dalaran
	NPC_AETHAS_SUNREAVER                    = 68086,
	NPC_SUNREAVER_CITIZEN                   = 68052,
	NPC_SUNREAVER_LIEUTENANT                = 217480,
	NPC_SUNREAVER_BATTLEMAGE                = 218340,
	NPC_SUNREAVER_MAGE                      = 223395,
	 
	// Vision 5 - Dalaran
	NPC_ARCHMAGE_ANTONIDAS                  = 500027,
	NPC_DALARAN_CITIZEN_01                  = 113317,
	NPC_DALARAN_CITIZEN_02                  = 113318,
	NPC_DALARAN_CITIZEN_03                  = 113319,
};

enum DalaranFateDataTypes
{
	// NPCs
	DATA_ANDUIN                             = 0,
	DATA_JAINA_PROUDMOORE,
	DATA_KAELTHAS,
	DATA_KALECGOS,
	DATA_KELTHUZAD,
	DATA_JAINA_PROUDMOORE_VISION,
	DATA_CAULDRON_BUNNY,

	// GameObjects
	DATA_ALCHEMICAL_SOLUTION,
	DATA_SKULL,
	DATA_ESSENCE_OF_DEATH,

	// Misc
	DATA_PHASE                              = 255
};

enum DalaranFateSpells
{
	// Generic
	SPELL_TELEPORT_DUMMY                    = 51347,
	SPELL_TELEPORT                          = 357601,
	SPELL_SKYBOX                            = 389962,

	// Memories
	SPELL_SPOTLIGHT                         = 437208,
	SPELL_FREEZE_ANIMATION                  = 118319,
	SPELL_SPAWN                             = 1291696,
	SPELL_HAUNTING_MEMORY                   = 1220632,

	// Jaina
	SPELL_WATER_WAVE                        = 421222,
	SPELL_WATER_CHANNELING                  = 395307,
	SPELL_LIGHTNING_AURA                    = 427264,

	// Kalecgos
	SPELL_SIT_CHAIR_MED                     = 123161,

	// Kel Thuzad
	SPELL_SLEEPING                          = 1247226,
	SPELL_SLIME_BURST                       = 1263874,

	// ??
	SPELL_FIRESTRIKE                        = 330347,
	SPELL_READING_BOOK_SITTING              = 223977,
	SPELL_READING_BOOK_STANDING             = 258793,
	SPELL_DISSOLVE                          = 237075,
};

enum DalaranFateEvents
{
	EVENT_NONE                              = 0,

	// Events
	EVENT_START,

    // Jaina Kalecgos
	EVENT_KALECGOS_01,
	EVENT_KALECGOS_02,
	EVENT_KALECGOS_03,
	EVENT_KALECGOS_04,
	EVENT_KALECGOS_05,

    // Kel Thuzad
	EVENT_KELTHUZAD_01,
	EVENT_KELTHUZAD_02,
	EVENT_KELTHUZAD_03,
};

enum DalaranFateCriteriaTrees
{
	// Introducion
	CRITERIA_TREE_01_PARENT                 = 1000076,
	CRITERIA_TREE_01_FIND_JAINA             = 1000077,
	CRITERIA_TREE_01_DISCUSS                = 1000078,

	// Guardian's Room
	CRITERIA_TREE_02_PARENT                 = 1000084,
	CRITERIA_TREE_02_FIND_JAINA             = 1000085,

	// Kalecgos
	CRITERIA_TREE_03_PARENT                 = 1000079,
	CRITERIA_TREE_03_ASSIST_JAINA           = 1000080,

	// Kel Thuzad
	CRITERIA_TREE_04_PARENT                 = 1000081,
	CRITERIA_TREE_04_CAULDRON               = 1000082,
	CRITERIA_TREE_04_DEFEAT                 = 1000083,
};

enum DalaranFateGameEvents
{
	// Introducion
	EVENT_FIND_INTRODUCTION_FIND_JAINA      = 50030,
	EVENT_FIND_INTRODUCTION_DISCUSS         = 50031,

	// Guardian's Room
	EVENT_FIND_GUARDIAN_FIND_JAINA          = 50033,

	// Kalecgos
	EVENT_FIND_KALECGOS_ASSIST_JAINA        = 50032,
};

enum DalaranFateMisc
{
	// VisualKit
	VIGNETTE_NONE                           = 0,
	VIGNETTE_YELLOW                         = 50001,
	VIGNETTE_PURPLE                         = 50002,

	// GameObjects
	GOB_PORTAL_TO_DALARAN                   = 323842,

	// Kalecgos
	GOB_DALARAN_COUCH                       = 417908,
	GOB_DALARAN_TABLE                       = 1550015,
	GOB_TOME_OF_POWER                       = 1550008,

	// Kel Thuzad
	GOB_CAULDRON                            = 1550009,
	GOB_ALCHEMICAL_SOLUTION                 = 1550010,
	GOB_SKULL                               = 1550011,
	GOB_PUISUIT_TERNAL_LIFE                 = 1550012,
	GOB_BAG_OF_GRAIN                        = 1550013,
	GOB_ESSENCE_OF_DEATH                    = 1550014,

	// Point Id
	MOVEMENT_INFO_POINT_NONE                = 0,
	MOVEMENT_INFO_POINT_01                  = 89644940,
	MOVEMENT_INFO_POINT_02                  = 89644941,
	MOVEMENT_INFO_POINT_03                  = 89644942,

	// Conversations
	CONVERSATION_INTRODUCTION               = 60000,    // My father isn't asking you to pledge the [...]
	CONVERSATION_START                      = 60001,    // Nobody dislikes Garrosh more than me [...]
	CONVERSATION_VISIONS                    = 60002,    // In the aftermath of Theramore, my first in [...]
	CONVERSATION_KELTHUZAD_COMBAT           = 60003,

	// Misc
	FACTION_KELTHUZAD_HOSTILE               = 14,

	// DoAction
	ACTION_KELTHUZAD_COMBAT_READY           = 1,
};

// Position d'Anduin apres l'introduction
const Position AnduinPos01                  = { -836.36f, 4458.15f, 588.77f, 2.37f };
const Position JainaPos01                   = { -833.39f, 4461.56f, 588.77f, 2.60f };

// Position des joueurs apres l'introduction
const Position PlayerPos01                  = { -806.74f, 4436.83f, 598.49f, 2.48f };

// Room Center
const Position RoomCenter                   = { -844.71f, 4467.60f, 588.84f, 5.57f };

// Start Path (Jaina et Anduin)
WaypointPath const ActorsPath01 =
{
	1,
	{
		{ 0, -799.26f, 4660.82f, 933.83f, 1.6859f },
		{ 1, -800.28f, 4670.31f, 931.27f, 1.6859f },
		{ 2, -801.05f, 4680.95f, 930.71f, 1.6827f }
	},
	WaypointMoveType::Walk,
	WaypointPathFlags::ExactSplinePath
};

/*****
* VISIONS
*****/

enum VisionType
{
	VISION_TYPE_JAINA,
	VISION_TYPE_KALECGOS,
	VISION_TYPE_KEL_THUZAD,
	VISION_TYPE_KAEL_THAS,
	VISION_TYPE_SUNREAVERS,
	VISION_TYPE_ANTONIDAS,
};

enum VisionFlag : uint32
{
	None        = 0x000,
	NoDespawn   = 0x001
};

/// <summary>
/// Definition des visions
/// </summary>
struct VisionData
{
	uint32 EntryOrData;
	VisionFlag Flags = None;
	HighGuid Type;
	Position Position;
	std::unordered_set<uint32> Emotes;
	std::unordered_set<uint32> Auras;

	bool HasFlags(VisionFlag flag) const
	{
		return (Flags & flag) != 0;
	}
};

/// <summary>
/// Stocke les visions
/// </summary>
struct VisionGuid
{
	const VisionData* Data;
	ObjectGuid Guid;
};

inline void ProcTeleportVisual(Creature* const creature, Position const position, uint32 visual)
{
	if (Creature* trigger = creature->SummonCreature(NPC_INVISIBLE_STALKER, *creature, TEMPSUMMON_TIMED_DESPAWN, 5s))
		trigger->CastSpell(trigger, visual);

	creature->NearTeleportTo(position);
}

inline void ProcTeleportVisual(Creature* const creature, Position const position)
{
	ProcTeleportVisual(creature, position, SPELL_TELEPORT_DUMMY);
	creature->SetHomePosition(position);
}

template <class AI>
class ConvoCreatureScript : public CreatureScript
{
	public:
		ConvoCreatureScript(char const* name) : CreatureScript(name) { }
		CreatureAI* GetAI(Creature* creature) const override { return GetInstanceAI<AI>(creature, DLCScriptName); }
};

#define RegisterConvoAI(ai_name) new ConvoCreatureScript<ai_name>(#ai_name);

#endif // DALARAN_CONVO_H_
