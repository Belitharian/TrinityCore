#include "GameObject.h"
#include "InstanceScript.h"
#include "KillRewarder.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "PassiveAI.h"
#include "Scenario.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Custom/AI/CustomAI.h"
#include "dalaran_purge.h"

class npc_jaina_dalaran_purge : public CreatureScript
{
	public:
	npc_jaina_dalaran_purge() : CreatureScript("npc_jaina_dalaran_purge")
	{
	}

	struct npc_jaina_dalaran_purgeAI : public CustomAI
	{
		npc_jaina_dalaran_purgeAI(Creature* creature) : CustomAI(creature)
		{
			Initialize();
		}

		void Initialize()
		{
			instance = me->GetInstanceScript();
		}

		InstanceScript* instance;

        void MoveInLineOfSight(Unit* who) override
        {
            ScriptedAI::MoveInLineOfSight(who);

            if (me->IsEngaged())
                return;

            if (who->GetTypeId() != TYPEID_PLAYER)
                return;

            if (Player* player = who->ToPlayer())
            {
                if (player->IsGameMaster())
                    return;

                if (player->IsFriendlyTo(me) && player->IsWithinDist(me, 5.f))
                {
                    DLJPhases phase = (DLJPhases)instance->GetData(DATA_SCENARIO_PHASE);
                    switch (phase)
                    {
                        case DLJPhases::FindJaina_Aethas:
                            instance->SetData(100, 0U);
                            //instance->DoSendScenarioEvent(EVENT_FIND_JAINA_01);
                            break;
                        default:
                            break;
                    }
                }
            }
        }
	};

	CreatureAI* GetAI(Creature* creature) const override
	{
		return GetDalaranAI<npc_jaina_dalaran_purgeAI>(creature);
	}
};

class npc_aethas_sunreaver_purge : public CreatureScript
{
    public:
    npc_aethas_sunreaver_purge() : CreatureScript("npc_aethas_sunreaver_purge")
    {
    }

    struct npc_aethas_sunreaver_purgeAI : public CustomAI
    {
        npc_aethas_sunreaver_purgeAI(Creature* creature) : CustomAI(creature)
        {
            Initialize();
        }

        void Initialize()
        {
            instance = me->GetInstanceScript();
        }

        InstanceScript* instance;

        void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
        {
            if (spellInfo->Id == SPELL_FROSTBOLT)
            {
                DoCastSelf(SPELL_ICY_GLARE);
                DoCastSelf(SPELL_CHILLING_BLAST, true);
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetDalaranAI<npc_aethas_sunreaver_purgeAI>(creature);
    }
};

void AddSC_dalaran_purge()
{
	new npc_jaina_dalaran_purge();
	new npc_aethas_sunreaver_purge();
}
