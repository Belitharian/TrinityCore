#ifndef DALARAN_CONVO_H_
#define DALARAN_CONVO_H_

#include "CreatureAIImpl.h"
#include "Position.h"

#define DLCScriptName "instance_dalaran_convo"
#define DataHeader "DLC"

#define CAM_PATH_1              8

enum NPCs
{
    NPC_UNDEAD 				    = 3,
    NPC_KAELTHAS 			    = 19622,
    NPC_KELTHUZAD			    = 20350,
    NPC_INVISIBLE_STALKER       = 32780,
    NPC_KALECGOS 			    = 35867,
    NPC_SILVER 				    = 36774,
    NPC_SUNREAVER 			    = 68052,
    NPC_ANDUIN 				    = 68106,
    NPC_JAINA_PROUDMOORE	    = 68108,
    NPC_KAELTHAS_ASSISTANT 	    = 68482,
    NPC_CAPTURED_CIVILIAN       = 92946,
};

enum Datas
{
    DATA_ANDUIN,
    DATA_JAINA_PROUDMOORE,
    DATA_KAELTHAS,
    DATA_KALECGOS,
    DATA_KELTHUZAD,
    DATA_CAMERA,
};

enum Spells
{
    SPELL_FIRST_PERSON_CAMERA   = 42129
};

enum DLPMisc
{
    // AnimKits
    ANIMKIT_BEGGING                     = 626,

	// Point Id
	MOVEMENT_INFO_POINT_NONE            = 0,
	MOVEMENT_INFO_POINT_01              = 89644940,
	MOVEMENT_INFO_POINT_02              = 89644941,
	MOVEMENT_INFO_POINT_03              = 89644942
};

const Position CamPath01[CAM_PATH_1] =
{
    { -803.22f, 4578.27f, 706.26f, 5.42f },
    { -795.44f, 4572.75f, 705.82f, 5.69f },
    { -793.55f, 4569.95f, 705.70f, 5.08f },
    { -792.52f, 4564.05f, 704.60f, 4.74f },
    { -793.39f, 4554.02f, 701.66f, 4.50f },
    { -795.90f, 4544.91f, 700.22f, 4.41f },
    { -797.13f, 4537.48f, 699.61f, 4.86f },
    { -790.06f, 4529.11f, 699.62f, 1.35f },
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
