#include "InstanceScript.h"
#include "TemporarySummon.h"
#include "MotionMaster.h"
#include "Custom/CustomAI/CustomAI.h"
#include "ruins_of_theramore.h"

struct npc_jaina_ruins : public CustomAI
{
	enum Spells
	{
		SPELL_FROSTBOLT       = 427863,
		SPELL_GRASP_OF_FROST  = 287626,
		SPELL_COMET_BARRAGE   = 354938,
		SPELL_BLINK           = 357601,
		SPELL_FROZEN_SHIELD   = 372749,
        SPELL_ARCANE_CHAOS    = 406854
	};

	InstanceScript* instance;
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
        for (uint8 i = MECHANIC_NONE; i < MAX_MECHANIC; i++)
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, i, true);

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
		}
	}

    void DamageTaken(Unit* /*attacker*/, uint32& /*damage*/, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
        if (hasShielded)
			return;

        if (HealthBelowPct(50))
        {
            CastStop();

            for (uint8 i = 0; i < 3; i++)
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
			.Schedule(8s, [this](TaskContext grasp_of_frost)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				{
                    CastStop(SPELL_ARCANE_CHAOS);
					DoCast(target, SPELL_GRASP_OF_FROST);
				}
				grasp_of_frost.Repeat(18s, 32s);
			})
			.Schedule(15s, [this](TaskContext comet_barrage)
			{
				DoCastAOE(SPELL_COMET_BARRAGE);
				comet_barrage.Repeat(12s, 14s);
			})
            .Schedule(30s, [this](TaskContext arcane_chaos)
            {
                CastStop(SPELL_COMET_BARRAGE);
                DoCast(SPELL_ARCANE_CHAOS);
                arcane_chaos.Repeat(45s, 60s);
            })
			.Schedule(12s, [this](TaskContext blink)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
				{
                    Position dest = me->GetRandomPoint(target->GetPosition(), 6.0f);
                    me->CastSpell(dest, SPELL_BLINK, true);
				}
				blink.Repeat(30s, 45s);
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
