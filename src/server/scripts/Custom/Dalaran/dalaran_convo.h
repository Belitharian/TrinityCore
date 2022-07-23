#ifndef DALARAN_CONVO_H_
#define DALARAN_CONVO_H_

#include "CreatureAIImpl.h"
#include "Position.h"

#define DLCScriptName       "instance_dalaran_convo"
#define DataHeader          "DLC"

#define PHASE_TYPE          255

enum class Phases
{
    None,
    Introduction,
    Teleportation,
    Conversation,
    Progress
};

enum NPCs
{
    NPC_UNDEAD 				        = 3,
    NPC_KAELTHAS 			        = 19622,
    NPC_KELTHUZAD			        = 20350,
    NPC_INVISIBLE_STALKER           = 32780,
    NPC_KALECGOS 			        = 35867,
    NPC_SILVER 				        = 36774,
    NPC_SUNREAVER 			        = 68052,
    NPC_ANDUIN 				        = 68106,
    NPC_JAINA_PROUDMOORE	        = 68108,
    NPC_KAELTHAS_ASSISTANT 	        = 68482,
    NPC_SHANNON_NOEL                = 99350,
};

enum Datas
{
    DATA_ANDUIN,
    DATA_JAINA_PROUDMOORE,
    DATA_KAELTHAS,
    DATA_KALECGOS,
    DATA_KELTHUZAD,
};

enum Spells
{
    SPELL_SIT_CHAIR_MED                = 123161,
    SPELL_READING_BOOK_SITTING         = 223977,
    SPELL_DISSOLVE                     = 237075,
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
};

const Position PlayerPos01              = { -804.16f, 4453.89f, 598.49f, 3.25f };
const Position JainaPos01               = { -821.50f, 4463.16f, 598.72f, 0.60f };
const Position AnduinPos01              = { -817.56f, 4460.48f, 598.72f, 1.15f };

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
