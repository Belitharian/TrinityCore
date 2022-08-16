#ifndef DALARAN_CONVO_H_
#define DALARAN_CONVO_H_

#include "CreatureAIImpl.h"
#include "Position.h"

#define DLCScriptName       "instance_dalaran_convo"
#define DataHeader          "DLC"

#define PHASE_TYPE          255

#define ACTORS_PATH_01      3
#define ACTORS_PATH_02      9

enum class Phases
{
    None,
    Introduction,
    Teleportation,
    Conversation,
    Event,
    Progress
};

enum NPCs
{
    NPC_UNDEAD 				        = 3,
    NPC_KELTHUZAD			        = 20350,
    NPC_INVISIBLE_STALKER           = 32780,
    NPC_KALECGOS 			        = 35867,
    NPC_SILVER 				        = 36774,
    NPC_SUNREAVER 			        = 68052,
    NPC_ANDUIN 				        = 68106,
    NPC_JAINA_PROUDMOORE	        = 68108,
    NPC_KAELTHAS_ASSISTANT 	        = 68482,
    NPC_SHANNON_NOEL                = 99350,
    NPC_GHOUL                       = 146835,
    NPC_KAELTHAS                    = 177103,
};

enum Datas
{
    DATA_ANDUIN,
    DATA_JAINA_PROUDMOORE,
    DATA_KAELTHAS,
    DATA_KALECGOS,
    DATA_KELTHUZAD,
    DATA_SHANNON_NOEL,
};

enum Spells
{
    SPELL_FIRESTRIKE                   = 2120,
    SPELL_TELEPORT_DUMMY               = 51347,
    SPELL_SIT_CHAIR_MED                = 123161,
    SPELL_READING_BOOK_SITTING         = 223977,
    SPELL_READING_BOOK_STANDING        = 258793,
    SPELL_DISSOLVE                     = 237075,
    SPELL_VOID_CHANNELING              = 286909,
    SPELL_TAKING_NOTES                 = 164999,
    SPELL_FEIGN_DEATH                  = 265448,
};

enum Events
{
    EVENT_NONE                          = 500,

    // Events
    EVENT_VISION_01,
    EVENT_VISION_02,
    EVENT_VISION_03,
    EVENT_VISION_04,
    EVENT_VISION_05,
    EVENT_VISION_06,
    EVENT_VISION_07,
    EVENT_VISION_08,
    EVENT_VISION_09,
    EVENT_VISION_10,
    EVENT_VISION_11,
    EVENT_VISION_12,
    EVENT_VISION_13,
};

enum DLPMisc
{
    // AnimKits
    ANIMKIT_BEGGING                     = 626,

    // GameObjects
    GOB_PORTAL_TO_DALARAN               = 323842,

	// Point Id
	MOVEMENT_INFO_POINT_NONE            = 0,
	MOVEMENT_INFO_POINT_01              = 89644940,
	MOVEMENT_INFO_POINT_02              = 89644941,
	MOVEMENT_INFO_POINT_03              = 89644942,

    // Talks
    SAY_ANDUIN_INTRO_01                 = 0,
    SAY_JAINA_INTRO_02                  = 0,

    SAY_JAINA_CONVO_01                  = 1,
    SAY_JAINA_CONVO_02                  = 2,
    SAY_JAINA_CONVO_03                  = 3,
    SAY_JAINA_CONVO_04                  = 4,
    SAY_JAINA_CONVO_05                  = 5,
    SAY_JAINA_CONVO_06                  = 6,
};

const Position PlayerPos01              = { -804.16f, 4453.89f, 598.49f, 3.25f };
const Position JainaPos01               = { -821.50f, 4463.16f, 598.72f, 0.60f };
const Position JainaPos02               = { -816.71f, 4501.78f, 601.50f, 5.79f };
const Position AnduinPos01              = { -817.56f, 4460.48f, 598.72f, 1.15f };
const Position GhoulPos01               = { -830.17f, 4485.71f, 598.85f, 0.87f };
const Position FirestrikePos01          = { -808.46f, 4500.06f, 601.61f, 2.40f };

const Position ActorsPath01[ACTORS_PATH_01] =
{
    { -799.17f, 4659.60f, 933.83f, 1.67f },
    { -799.68f, 4664.68f, 933.84f, 1.63f },
    { -801.00f, 4681.05f, 930.71f, 1.67f },
};

const Position JainaPath02[ACTORS_PATH_02] =
{
    { -820.82f, 4468.03f, 598.71f, 1.66f },
    { -821.27f, 4471.12f, 598.71f, 1.95f },
    { -824.68f, 4476.82f, 598.71f, 2.49f },
    { -828.17f, 4480.55f, 598.85f, 2.14f },
    { -829.79f, 4485.44f, 598.85f, 1.49f },
    { -827.32f, 4489.62f, 599.08f, 0.73f },
    { -824.20f, 4492.97f, 599.30f, 0.89f },
    { -820.60f, 4497.52f, 601.50f, 0.90f },
    { -819.51f, 4502.30f, 601.50f, 2.02f },
};

const Position AnduinPath02[ACTORS_PATH_02] =
{
    { -820.82f, 4468.03f, 598.71f, 1.66f },
    { -821.27f, 4471.12f, 598.71f, 1.95f },
    { -824.68f, 4476.82f, 598.71f, 2.49f },
    { -828.17f, 4480.55f, 598.85f, 2.14f },
    { -829.79f, 4485.44f, 598.85f, 1.49f },
    { -827.32f, 4489.62f, 599.08f, 0.73f },
    { -824.20f, 4492.97f, 599.30f, 0.89f },
    { -820.60f, 4497.52f, 601.50f, 0.90f },
    { -816.97f, 4498.86f, 601.50f, 0.98f },
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

template <class AI>
class ConvoCreatureScript : public CreatureScript
{
	public:
		ConvoCreatureScript(char const* name) : CreatureScript(name) { }
		CreatureAI* GetAI(Creature* creature) const override { return GetInstanceAI<AI>(creature, DLCScriptName); }
};

#define RegisterConvoAI(ai_name) new ConvoCreatureScript<ai_name>(#ai_name);

#endif // DALARAN_CONVO_H_
