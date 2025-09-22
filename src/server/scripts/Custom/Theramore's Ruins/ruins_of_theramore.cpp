#include "InstanceScript.h"
#include "TemporarySummon.h"
#include "MotionMaster.h"
#include "Custom/AI/CustomAI.h"
#include "ruins_of_theramore.h"

struct npc_jaina_ruins : public CustomAI
{
	enum Spells
	{
		SPELL_EVOCATION       = 243070,
		SPELL_FROSTBOLT       = 284703,
		SPELL_RING_OF_ICE     = 285459,
		SPELL_GRASP_OF_FROST  = 287626,
		SPELL_COMET_BARRAGE   = 354938,
		SPELL_FRIGID_SHARD    = 354933,
		SPELL_BLINK           = 357601,
		SPELL_ARCANE_SURGE    = 365350,
		SPELL_FROZEN_SHIELD   = 396780,
	};

	InstanceScript* instance;
	Position beforeBlink;
	bool hasBlinked = false;
	bool hasShielded = false;
	float distance = 10.f;

	npc_jaina_ruins(Creature* creature) : CustomAI(creature)
	{
		instance = me->GetInstanceScript();
	}

	void SetData(uint32 /*id*/, uint32 value) override
	{
		distance = (float)value;
	}

	void SpellHit(WorldObject*, SpellInfo const*) override { }

	void AttackStart(Unit* who) override
	{
		if (!who)
			return;

		if (me->Attack(who, false))
		{
			me->SetCanMelee(false);
			me->SetSheath(SHEATH_STATE_UNARMED);
			SetCombatMovement(false);
		}
	}

	void Reset() override
	{
		Initialize();
		hasBlinked = false;
	}

	void OnSpellCast(SpellInfo const* spell) override
	{
		switch (spell->Id)
		{
            case SPELL_SUMMON_WATER_ELEMENTALS:
                for (uint8 i = 0; i < ELEMENTALS_SIZE; ++i)
                {
                    if (Creature* elemental = me->GetMap()->SummonCreature(NPC_WATER_ELEMENTAL, ElementalsPoint[i].spawn))
                        elemental->CastSpell(elemental, SPELL_WATER_BOSS_ENTRANCE);
                }
                break;
			case SPELL_RING_OF_ICE:
				if (!hasBlinked)
					break;
				scheduler.Schedule(1s, [this](TaskContext)
				{
					CastStop();
					me->CastSpell(beforeBlink, SPELL_BLINK, true);
					hasBlinked = false;
				});
				break;
		}
	}

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
        if (hasShielded)
			return;

        if (me->HealthBelowPctDamaged(50, damage))
        {
            CastStop();
            DoCastSelf(SPELL_FROZEN_SHIELD);
            hasShielded = true;
        }
	}

	void JustEngagedWith(Unit* who) override
	{
		#ifndef CUSTOM_DEBUG
		if ((RFTPhases)instance->GetData(DATA_SCENARIO_PHASE) != RFTPhases::Standards_Valided)
			return;
		#endif

		DoCast(who, SPELL_FROSTBOLT);

		scheduler
			.Schedule(2s, [this](TaskContext frostbolt)
			{
				DoCastVictim(SPELL_FROSTBOLT);
				frostbolt.Repeat(2800ms);
			})
			.Schedule(8s, [this](TaskContext frigid_shard)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
					DoCast(target, SPELL_FRIGID_SHARD);
				frigid_shard.Repeat(14s, 18s);
			})
			.Schedule(24s, [this](TaskContext grasp_of_frost)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				{
                    CastStop({ SPELL_RING_OF_ICE, SPELL_FRIGID_SHARD });
					DoCast(target, SPELL_GRASP_OF_FROST);
				}
				grasp_of_frost.Repeat(18s, 32s);
			})
			.Schedule(14s, [this](TaskContext comet_barrage)
			{
				DoCastAOE(SPELL_COMET_BARRAGE);
				comet_barrage.Repeat(12s, 14s);
			})
			.Schedule(50s, [this](TaskContext blink)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
				{
					if (me->IsWithinDist(target, 10.f))
					{
                        CastStop({ SPELL_RING_OF_ICE });
						DoCastAOE(SPELL_RING_OF_ICE);
					}
					else
					{
						beforeBlink = me->GetPosition();
                        CastStop({ SPELL_RING_OF_ICE });
						Position dest = me->GetRandomPoint(target->GetPosition(), 6.0f);
						me->CastSpell(dest, SPELL_BLINK, true);
						scheduler.Schedule(1s, [this](TaskContext)
						{
							CastStop();
							DoCastAOE(SPELL_RING_OF_ICE);
						});
						hasBlinked = true;
					}
				}
				blink.Repeat(1min);
			});
	}

	bool CanAIAttack(Unit const*) const override { return true; }

	void MovementInform(uint32, uint32 id) override
	{
		switch (id)
		{
			case MOVEMENT_INFO_POINT_01:
				me->SetStandState(UNIT_STAND_STATE_KNEEL);
				scheduler.Schedule(2s, [this](TaskContext context)
				{
					switch (context.GetRepeatCounter())
					{
						case 0:
							me->SetWalk(false);
							me->SetStandState(UNIT_STAND_STATE_STAND);
							if (Creature* warlord = me->FindNearestCreature(NPC_ROKNAH_WARLORD, 10.f))
								me->SetWalk(true);
							context.Repeat(2s);
							break;
						case 1:
							if (Creature* warlord = instance->GetCreature(DATA_ROKNAH_WARLORD))
								me->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_02, warlord, 1.3f);
							break;
					}
				});
				break;
			case MOVEMENT_INFO_POINT_02:
				me->HandleEmoteCommand(EMOTE_ONESHOT_CASTSTRONG);
				scheduler.Schedule(1s, [this](TaskContext)
				{
					if (Creature* warlord = instance->GetCreature(DATA_ROKNAH_WARLORD))
					{
						warlord->KillSelf();
						instance->TriggerGameEvent(EVENT_WARLORD_ROKNAH_SLAIN);
					}
				});
				break;
			case MOVEMENT_INFO_POINT_03:
				instance->SetData(EVENT_BACK_TO_SENDER, 0U);
				break;
		}
	}

	void MoveInLineOfSight(Unit* who) override
	{
		ScriptedAI::MoveInLineOfSight(who);
		if (me->IsEngaged() || who->GetTypeId() != TYPEID_PLAYER)
			return;
		if (Player* player = who->ToPlayer())
		{
			if (player->IsGameMaster())
				return;
			if (player->IsFriendlyTo(me) && player->IsWithinDist(me, distance))
			{
				switch ((RFTPhases)instance->GetData(DATA_SCENARIO_PHASE))
				{
					case RFTPhases::FindJaina_Isle:
						instance->TriggerGameEvent(EVENT_FIND_JAINA_01);
						break;
					case RFTPhases::FindJaina_Crater:
						instance->SetData(EVENT_FIND_JAINA_02, 0U);
						break;
					default:
						break;
				}
			}
		}
	}

	void UpdateAI(uint32 diff) override
	{
		scheduler.Update(diff, [this] { UpdateVictim(); });
	}
};

void AddSC_ruins_of_theramore()
{
	RegisterRuinsAI(npc_jaina_ruins);
}
