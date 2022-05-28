#include "Custom/AI/CustomAI.h"
#include "MotionMaster.h"
#include "Object.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"
#include "dalaran_convo.h"

struct npc_jaina_proudmoore_convo : public CustomAI
{
	npc_jaina_proudmoore_convo(Creature* creature) : CustomAI(creature)
	{
	}
	
    void SetData(uint32 /*id*/, uint32 /*value*/)
    {
        if (InstanceScript* instance = me->GetInstanceScript())
            instance->SetData(1, 1);
    }
};

void AddSC_npcs_dalaran_convo()
{
	RegisterConvoAI(npc_jaina_proudmoore_convo);
}
