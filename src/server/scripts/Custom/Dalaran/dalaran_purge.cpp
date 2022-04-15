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
		me->SetSheath(SHEATH_STATE_UNARMED);
	}

	enum Misc
	{
		// Gossip
		GOSSIP_MENU_DEFAULT         = 65004,
	};

	InstanceScript* instance;

	bool OnGossipHello(Player* player) override
	{
		DLPPhases phase = (DLPPhases)instance->GetData(DATA_SCENARIO_PHASE);
		if (phase != DLPPhases::TheEscape)
			return false;

		player->PrepareGossipMenu(me, GOSSIP_MENU_DEFAULT, true);
		player->SendPreparedGossip(me);
		return true;
	}

	bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
	{
		ClearGossipMenuFor(player);

		switch (gossipListId)
		{
			case 0:
				me->RemoveAurasDueToSpell(SPELL_CHAT_BUBBLE);
				me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
				instance->DoSendScenarioEvent(EVENT_SPEAK_TO_JAINA);
				break;
		}

		CloseGossipMenuFor(player);
		return true;
	}

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

	void CastHordeIllusionOnPlayers(Player* player)
	{
		me->HandleEmoteCommand(EMOTE_ONESHOT_CASTSTRONG, player);
		if (Map* map = me->GetMap())
		{
			map->DoOnPlayers([this](Player* player)
			{
				player->CombatStop();
				player->CastSpell(player, SPELL_FACTION_OVERRIDE);
				player->CastSpell(player, SPELL_HORDE_ILLUSION);
			});
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

	void MovementInform(uint32 /*type*/, uint32 id) override
	{
		switch (id)
		{
			default:
				break;
		}
	}

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

struct npc_magister_rommath_purge : public CustomAI
{
	npc_magister_rommath_purge(Creature* creature) : CustomAI(creature)
	{
		Initialize();
	}

	void Initialize()
	{
		instance = me->GetInstanceScript();
	}

	InstanceScript* instance;

	void MovementInform(uint32 /*type*/, uint32 id) override
	{
		switch (id)
		{
			case MOVEMENT_INFO_POINT_01:
				me->AI()->Talk(SAY_INFILTRATE_ROMMATH_03);
				scheduler.Schedule(5s, [this](TaskContext /*context*/)
				{
					me->AI()->Talk(SAY_INFILTRATE_ROMMATH_04);
					if (GameObject* passage = instance->GetGameObject(DATA_SECRET_PASSAGE))
						passage->UseDoorOrButton(5000);
				});
				break;
			default:
				break;
		}
	}

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
					case DLPPhases::TheEscape_Events:
						me->RemoveAurasDueToSpell(SPELL_COSMETIC_YELLOW_ARROW);
						instance->DoSendScenarioEvent(EVENT_FIND_ROMMATH_01);
						break;
					default:
						break;
				}
			}
		}
	}
};

void AddSC_dalaran_purge()
{
	RegisterDalaranAI(npc_jaina_dalaran_purge);
	RegisterDalaranAI(npc_aethas_sunreaver_purge);
	RegisterDalaranAI(npc_magister_rommath_purge);
}
