#include "Custom/AI/CustomAI.h"
#include "MotionMaster.h"
#include "Object.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"
#include "dalaran_convo.h"

struct npc_jaina_proudmoore_convo : public CustomAI
{
    enum Misc
    {
        // Spells
        SPELL_TELEPORT = 134634
    };

	npc_jaina_proudmoore_convo(Creature* creature) : CustomAI(creature), phase(Phases::None)
	{
        instance = me->GetInstanceScript();
    }
	
    void SetData(uint32 id, uint32 value)
    {
        switch (id)
        {
            case 100:
                instance->SetData((uint32)Phases::Conversation, 1U);
                break;
            case PHASE_TYPE:
                phase = (Phases)value;
                break;
        }
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (who->GetTypeId() != TYPEID_PLAYER)
            return;

        Player* player = who->ToPlayer();
        if (!player)
            return;

        switch (phase)
        {
            case Phases::Introduction:
                scheduler.Schedule(5ms, [this, player](TaskContext teleport)
                {
                    if (player->GetPositionZ() < 920.f)
                        me->CastSpell(player, SPELL_TELEPORT, true);
                    teleport.Repeat(850ms);
                });
                break;
            case Phases::None:
                if (player->IsWithinDist(me, 5.0f))
                {
                    SetData(PHASE_TYPE, (uint32)Phases::Progress);
                    instance->SetData((uint32)Phases::Introduction, 1U);
                }
                break;
            default:
            case Phases::Progress:
                break;
        }
    }

    private:
    Phases phase;
    InstanceScript* instance;
};

void AddSC_npcs_dalaran_convo()
{
	RegisterConvoAI(npc_jaina_proudmoore_convo);
}
