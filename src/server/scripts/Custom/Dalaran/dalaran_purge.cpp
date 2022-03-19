#include "Custom/AI/CustomAI.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "ScriptMgr.h"
#include "dalaran_purge.h"

struct npc_jaina_dalaran_purge : public CustomAI
{
	npc_jaina_dalaran_purge(Creature* creature) : CustomAI(creature)
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
				DLPPhases phase = (DLPPhases)instance->GetData(DATA_SCENARIO_PHASE);
				switch (phase)
				{
					case DLPPhases::FindJaina01:
						instance->DoSendScenarioEvent(EVENT_FIND_JAINA_01);
						break;
                    case DLPPhases::FindJaina02:
                        instance->SetData(EVENT_FIND_JAINA_02, 1U);
                        break;
                    default:
						break;
				}
			}
		}
	}
};

struct npc_aethas_sunreaver_purge : public CustomAI
{
	npc_aethas_sunreaver_purge(Creature* creature) : CustomAI(creature)
	{
		Initialize();
	}

	void Initialize()
	{
		instance = me->GetInstanceScript();
	}

	InstanceScript* instance;

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id == SPELL_ARCANE_BOMBARDMENT || spellInfo->Id == SPELL_FROSTBOLT)
        {
            damage = 0;
        }
    }

	void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
	{
		if (spellInfo->Id == SPELL_FROSTBOLT)
		{
			DoCastSelf(SPELL_ICY_GLARE);
			DoCastSelf(SPELL_CHILLING_BLAST, true);
		}
	}
};

void AddSC_dalaran_purge()
{
    RegisterDalaranAI(npc_jaina_dalaran_purge);
    RegisterDalaranAI(npc_aethas_sunreaver_purge);
}
