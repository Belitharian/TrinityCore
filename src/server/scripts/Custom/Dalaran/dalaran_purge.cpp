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
					DLPPhases phase = (DLPPhases)instance->GetData(DATA_SCENARIO_PHASE);
					switch (phase)
					{
						case DLPPhases::FindJaina:
							instance->DoSendScenarioEvent(EVENT_FIND_JAINA_01);
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

class npc_jaina_dalaran_patrol : public CreatureScript
{
	public:
	npc_jaina_dalaran_patrol() : CreatureScript("npc_jaina_dalaran_patrol")
	{
	}

	struct npc_jaina_dalaran_patrolAI : public CustomAI
	{
		npc_jaina_dalaran_patrolAI(Creature* creature) : CustomAI(creature)
		{
			Initialize();
		}

		enum Spells
		{
			SPELL_BLIZZARD          = 284968,
			SPELL_FROSTBOLT         = 284703,
			SPELL_FRIGID_SHARD      = 354933,
		};

		void Initialize()
		{
			instance = me->GetInstanceScript();
		}

		InstanceScript* instance;

		void JustEngagedWith(Unit* /*who*/) override
		{
			scheduler
				.Schedule(5ms, [this](TaskContext frostbolt)
				{
					DoCastVictim(SPELL_FROSTBOLT);
					frostbolt.Repeat(2s);
				})
				.Schedule(2s, [this](TaskContext frigid_shard)
				{
					if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
						DoCast(target, SPELL_FRIGID_SHARD);
					frigid_shard.Repeat(5s, 8s);
				})
                .Schedule(4s, [this](TaskContext blizzard)
                {
                    if (me->GetThreatManager().GetThreatListSize() >= 3)
                    {
                        DoCastVictim(SPELL_BLIZZARD);
                        blizzard.Repeat(12s, 21s);
                    }
                    else
                        blizzard.Repeat(2s);
                });
		}

		void KilledUnit(Unit* victim) override
		{
			if (roll_chance_i(30)
				&& victim->GetEntry() == NPC_SUNREAVER_CITIZEN
				&& !victim->HasAura(SPELL_TELEPORT_CITIZEN))
			{
				me->AI()->Talk(SAY_JAINA_PURGE_SLAIN);
			}
		}

		void MoveInLineOfSight(Unit* who) override
		{
			if (me->IsEngaged())
				return;

			if (who->GetEntry() != NPC_SUNREAVER_CITIZEN)
			{
				ScriptedAI::MoveInLineOfSight(who);
			}
			else
			{
				if (roll_chance_i(30)
					&& who->IsWithinLOSInMap(me)
					&& who->IsWithinDist(me, 25.f)
					&& !who->HasAura(SPELL_TELEPORT_CITIZEN))
				{
					if (who->IsEngaged())
					{
						ScriptedAI::MoveInLineOfSight(who);
					}
					else
					{
						if (roll_chance_i(60))
							me->AI()->Talk(SAY_JAINA_PURGE_TELEPORT);

						DoCast(who, SPELL_TELEPORT_CITIZEN);
					}
				}
				else
				{
					ScriptedAI::MoveInLineOfSight(who);
				}
			}
		}
	};

	CreatureAI* GetAI(Creature* creature) const override
	{
		return GetDalaranAI<npc_jaina_dalaran_patrolAI>(creature);
	}
};

class npc_sunreaver_citizen : public CreatureScript
{
	public:
	npc_sunreaver_citizen() : CreatureScript("npc_sunreaver_citizen")
	{
	}

	struct npc_sunreaver_citizenAI : public CustomAI
	{
		npc_sunreaver_citizenAI(Creature* creature) : CustomAI(creature)
		{
			Initialize();
		}

		enum Spells
		{
			SPELL_SCORCH                = 17195, 
			SPELL_FIREBALL              = 358226,
		};

		void Initialize()
		{
			instance = me->GetInstanceScript();

            if (Creature* jaina = instance->GetCreature(DATA_JAINA_PROUDMOORE_PATROL))
            {
                me->SetEmoteState(EMOTE_STATE_COWER);
                me->GetMotionMaster()->MoveFleeing(jaina);
            }
		}

		InstanceScript* instance;

		void JustEngagedWith(Unit* /*who*/) override
		{
			if (me->HasUnitState(UNIT_STATE_FLEEING_MOVE))
			{
                me->SetEmoteState(EMOTE_STATE_NONE);
                me->GetMotionMaster()->Clear();
				me->GetMotionMaster()->MoveIdle();
			}

			me->CallAssistance();

			scheduler
				.Schedule(8s, 10s, [this](TaskContext scorch)
				{
					if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
						DoCast(target, SPELL_SCORCH);
					scorch.Repeat(14s, 22s);
				})
				.Schedule(5ms, [this](TaskContext fireball)
				{
					DoCastVictim(SPELL_FIREBALL);
					fireball.Repeat(2s);
				});
		}

		void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
		{
			if (spellInfo->Id == SPELL_TELEPORT_CITIZEN)
			{
				if (Creature* jaina = caster->ToCreature())
					jaina->AttackStop();

				me->DespawnOrUnsummon(860ms);
			}
		}
	};

	CreatureAI* GetAI(Creature* creature) const override
	{
		return GetDalaranAI<npc_sunreaver_citizenAI>(creature);
	}
};

void AddSC_dalaran_purge()
{
	new npc_jaina_dalaran_purge();
	new npc_aethas_sunreaver_purge();
	new npc_jaina_dalaran_patrol();
	new npc_sunreaver_citizen();
}
