#ifndef RUINS_OF_THERAMORE_H_
#define RUINS_OF_THERAMORE_H_

#include "CreatureAIImpl.h"
#include "Position.h"

#define DLJScriptName "scenario_dalaran_purge"
#define DataHeader "DLJ"

//#define DEBUG

enum DLJCreatures
{
	NPC_JAINA_PROUDMOORE               = 68677,
	NPC_AETHAS_SUNREAVER               = 68679,
	NPC_HIGH_SUNREAVER_MAGE            = 68680,
	NPC_SUMMONED_WATER_ELEMENTAL       = 68678,
};

enum DLJData
{
	DATA_JAINA_PROUDMOORE,
	DATA_AETHAS_SUNREAVER,               
	DATA_SUMMONED_WATER_ELEMENTAL,
	DATA_SCENARIO_PHASE
};

enum DLJTalks
{
	// Aethas
	SAY_PURGE_JAINA_01                 = 0,
	SAY_PURGE_JAINA_02                 = 1,
	SAY_PURGE_AETHAS_03                = 0,
	SAY_PURGE_JAINA_04                 = 2,
	SAY_PURGE_AETHAS_05                = 1,
	SAY_PURGE_JAINA_06                 = 3,
	SAY_PURGE_JAINA_07                 = 4,
};

enum DLJSpells
{
	SPELL_TELEPORT_VISUAL_ONLY		   = 51347,
    SPELL_FROSTBOLT                    = 135285,
    SPELL_DISSOLVE                     = 255295,
    SPELL_TELEPORT                     = 357601,
	SPELL_CASTER_READY_01              = 245843,
	SPELL_CASTER_READY_02              = 245848,
	SPELL_CASTER_READY_03              = 245849,
	SPELL_ARCANE_BOMBARDMENT           = 352556,
    SPELL_ICY_GLARE                    = 338517,
    SPELL_CHILLING_BLAST               = 337053,
};

enum DLJMisc
{
	// Events
	EVENT_FIND_JAINA_01                 = 65817,

	// Criteria Trees
	CRITERIA_TREE_FIND_JAINA_01         = 1000047,  // Purple Citadel

	// Point Id
	MOVEMENT_INFO_POINT_NONE            = 0,
	MOVEMENT_INFO_POINT_01              = 89644940,
	MOVEMENT_INFO_POINT_02              = 89644941,
	MOVEMENT_INFO_POINT_03              = 89644942
};

enum class DLJPhases
{
	FindJaina_Aethas,
	FindJaina_Convo,
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
			player->UpdateGroundPositionZ(pos.GetPositionX(), pos.GetPositionY(), pos.m_positionZ);
			player->NearTeleportTo(pos);
		}
	}
}

template <class AI, class T>
inline AI* GetDalaranAI(T* obj)
{
	return GetInstanceAI<AI>(obj, DLJScriptName);
}

#endif // RUINS_OF_THERAMORE_H_
