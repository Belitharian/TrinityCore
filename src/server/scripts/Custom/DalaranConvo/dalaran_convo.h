#ifndef DALARAN_CONVO_H_
#define DALARAN_CONVO_H_

#include "CreatureAIImpl.h"
#include "Position.h"

#define DLCScriptName       "instance_dalaran_convo"
#define DataHeader          "DLC"

enum class Phases : uint32
{
	None,
	Introduction,
	Start,
	Start_CanTeleport,
	Kalecgos,
	Kalecgos_CanTeleport,
	KelThuzad,
	KelThuzad_Combat_Ready,
	KelThuzad_Combat,
	KelThuzad_CanTeleport,
};

enum NPCs
{
	NPC_INVISIBLE_STALKER           = 32780,
	NPC_ANDUIN_WRYNN		        = 68106,
	NPC_JAINA_PROUDMOORE	        = 68108,
	NPC_ARCANIST_ALEC               = 500030,

	// Vision 1 - Kalecgos
	NPC_KALECGOS 			        = 35867,
	NPC_JAINA_PROUDMOORE_VISION     = 500029,

	// Vision 2 - Kel Thuzad
	NPC_SHANNON_NOEL                = 99350,
	NPC_KELTHUZAD			        = 230131,
    NPC_MR_BIGGLESWORTH             = 206009,

	// Vision 3 - Kael'thas
	NPC_KAELTHAS                    = 500026,
	NPC_BLOOD_ELF_NOBLE             = 500028,

	// Vision 4 - Dalaran
	NPC_AETHAS_SUNREAVER            = 68086,
	NPC_SUNREAVER_CITIZEN           = 68052,
	NPC_SUNREAVER_LIEUTENANT        = 217480,
	NPC_SUNREAVER_BATTLEMAGE        = 218340,
	NPC_SUNREAVER_MAGE              = 223395,
	 
	// Vision 5 - Dalaran
	NPC_ARCHMAGE_ANTONIDAS          = 500027,
	NPC_DALARAN_CITIZEN_01          = 113317,
	NPC_DALARAN_CITIZEN_02          = 113318,
	NPC_DALARAN_CITIZEN_03          = 113319,
};

enum Datas
{
	DATA_ANDUIN,
	DATA_JAINA_PROUDMOORE,
	DATA_KAELTHAS,
	DATA_KALECGOS,
	DATA_KELTHUZAD,
	DATA_SHANNON_NOEL,
	DATA_JAINA_PROUDMOORE_VISION,
	DATA_ARCANIST_ALEC,
	DATA_FEL_BARRIER,

	DATA_PHASE = 255
};

enum Spells
{
	SPELL_TELEPORT_DUMMY                    = 51347,
	SPELL_SIT_CHAIR_MED                     = 123161,
	SPELL_READING_BOOK_SITTING              = 223977,
	SPELL_READING_BOOK_STANDING             = 258793,
	SPELL_DISSOLVE                          = 237075,
	SPELL_VOID_CHANNELING                   = 286909,
	SPELL_TAKING_NOTES                      = 164999,
	SPELL_FEIGN_DEATH                       = 265448,
	SPELL_FIRESTRIKE                        = 330347,
	SPELL_TELEPORT                          = 357601,
	SPELL_HAUNTING_MEMORY                   = 432892,
	SPELL_FEL_CANALISATION                  = 1281476
};

enum Events
{
	EVENT_NONE                              = 0,

	// Events
	EVENT_START_01,
};

enum Visions
{
	VISION_KALECGOS,
	VISION_KEL_THUZAD,
	VISION_KAEL_THAS,
	VISION_SUNREAVERS,
	VISION_ANTONIDAS,
};

enum DLPCriteriaTrees
{
	CRITERIA_TREE_INTRODUCTION_PARENT       = 1000076,
	CRITERIA_TREE_INTRODUCTION_FIND_JAINA   = 1000077,
	CRITERIA_TREE_INTRODUCTION_DISCUSS      = 1000078,

	CRITERIA_TREE_KALECGOS_PARENT           = 1000079,
	CRITERIA_TREE_KALECGOS_ASSIST_JAINA     = 1000080,
	CRITERIA_TREE_KALECGOS_SPEAK_TO_ALEC    = 1000081,

	CRITERIA_TREE_KELTHUZAD_PARENT          = 1000082,
	CRITERIA_TREE_KELTHUZAD_WITNESS         = 1000083,
	CRITERIA_TREE_KELTHUZAD_DEFEAT          = 1000084,
};

enum DLPGameEvents
{
	EVENT_FIND_INTRODUCTION_FIND_JAINA      = 68108,
	EVENT_FIND_INTRODUCTION_DISCUSS         = 650060,

	EVENT_FIND_KALECGOS_ASSIST_JAINA        = 650061,
	EVENT_FIND_KALECGOS_SPEAK_TO_ALEC       = 650062,

	EVENT_FIND_KELTHUZAD_WITNESS            = 650063,
	EVENT_FIND_KELTHUZAD_DEFEATED           = 650064,
};

enum DLPMisc
{
	// AnimKits
	ANIMKIT_BEGGING                         = 626,

	// GameObjects
	GOB_PORTAL_TO_DALARAN                   = 323842,
	GOB_TOME_OF_POWER                       = 1550008,
	GOB_FEL_BARRIER                         = 269122,

	// Point Id
	MOVEMENT_INFO_POINT_NONE                = 0,
	MOVEMENT_INFO_POINT_01                  = 89644940,
	MOVEMENT_INFO_POINT_02                  = 89644941,
	MOVEMENT_INFO_POINT_03                  = 89644942,

	// Conversations
	CONVERSATION_INTRODUCTION               = 60000,    // My father isn't asking you to pledge the [...]
	CONVERSATION_START                      = 60001,    // Nobody dislikes Garrosh more than me [...]
	CONVERSATION_KALECGOS                   = 60002,    // In the aftermath of Theramore, my first in [...]
	CONVERSATION_KELTHUZAD                  = 60003,    // The Kirin-Tor has a legacy of abuse [...]

    // Misc
    FACTION_KELTHUZAD_HOSTILE               = 14,

    // DoAction
    ACTION_KELTHUZAD_COMBAT_READY           = 1,
};

// Event - Kalecgos
const Position PlayerPos01                  = { -797.41f, 4487.83f, 735.01f, 4.03f };
const Position JainaPos01                   = { -797.39f, 4472.69f, 735.01f, 2.61f };
const Position AnduinPos01                  = { -796.07f, 4474.83f, 735.01f, 2.86f };

// Event - Kel'thuzad
const Position KelThuzadPos01               = { -912.34f, 4365.91f, 694.24f, 1.41f};
const Position JainaPos02                   = { -925.76f, 4361.27f, 694.24f, 5.07f };
const Position AnduinPos02                  = { -923.70f, 4361.99f, 694.24f, 4.85f };
const Position PlayerPos02                  = { 0.f, 0.f, 0.f, 0.f };

// Generic Path
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

// Kalecgos
WaypointPath const JainaPath01 =
{
	2,
	{
		{ 0, -799.31f, 4474.20f, 735.01f, 1.686f },
		{ 1, -800.02f, 4481.98f, 735.01f, 1.725f },
		{ 2, -796.43f, 4489.03f, 735.01f, 0.842f },
		{ 3, -792.35f, 4495.00f, 735.01f, 0.930f },
		{ 4, -785.21f, 4488.92f, 733.13f, 5.271f }
	},
	WaypointMoveType::Walk,
	WaypointPathFlags::ExactSplinePath
};

// Kel'Thuzad
WaypointPath const JainaPath02 =
{
	3,
	{
        { 0, -924.609f, 4365.01f, 694.24f, 1.07f },
        { 1, -921.145f, 4368.89f, 694.24f, 0.63f },
        { 2, -916.189f, 4372.24f, 694.24f, 0.65f },
        { 3, -912.826f, 4375.97f, 694.24f, 1.17f },
        { 4, -910.717f, 4384.62f, 695.60f, 4.58f }
	},
	WaypointMoveType::Walk,
	WaypointPathFlags::ExactSplinePath
};

// Kalecgos
WaypointPath const AnduinPath01 =
{
	2,
	{
		{ 0, -797.04f, 4478.37f, 735.01f, 1.686f },
		{ 1, -795.87f, 4485.03f, 735.01f, 1.330f }, // Pause
		{ 2, -796.43f, 4489.03f, 735.01f, 0.842f },
		{ 3, -792.35f, 4495.00f, 735.01f, 0.930f },
		{ 4, -785.21f, 4488.92f, 733.13f, 5.271f }
	},
	WaypointMoveType::Walk,
	WaypointPathFlags::ExactSplinePath
};

// Kel'Thuzad
WaypointPath const AnduinPath02 =
{
	3,
	{
        { 0, -919.345f, 4365.83f, 694.24f, 0.67f },
        { 1, -914.201f, 4369.41f, 694.24f, 0.50f },
        { 2, -910.371f, 4373.97f, 694.24f, 1.16f },
        { 3, -908.221f, 4384.82f, 695.60f, 1.25f }
	},
	WaypointMoveType::Walk,
	WaypointPathFlags::ExactSplinePath
};

inline void ProcTeleportVisual(Unit* const unit)
{
	if (Creature* trigger = unit->SummonCreature(NPC_INVISIBLE_STALKER, unit->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, 5s))
		trigger->CastSpell(trigger, SPELL_TELEPORT_DUMMY);
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
