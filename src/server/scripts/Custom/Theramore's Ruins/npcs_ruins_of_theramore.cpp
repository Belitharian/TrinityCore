#include "GameObject.h"
#include "GameObjectAI.h"
#include "InstanceScript.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "Custom/AI/CustomAI.h"
#include "ruins_of_theramore.h"

struct npc_water_elementals_theramore : public CustomAI
{
	npc_water_elementals_theramore(Creature* creature) : CustomAI(creature), shielded(false)
	{
	}

	enum Spells
	{
		SPELL_WATER_SPOUT           = 271287,
		SPELL_WATERY_DOME           = 258153,
		SPELL_WATER_BOLT_VOLLEY     = 290084,
		SPELL_WATER_BOLT            = 355225,
	};

    bool shielded;

	float GetDistance() override
	{
		return 5.f;
	}

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
    {
        if (me->HealthBelowPctDamaged(30, damage) && !shielded)
        {
            DoCastSelf(SPELL_WATERY_DOME);
            shielded = true;
        }
    }

	void JustEngagedWith(Unit* who) override
	{
		DoCast(who, SPELL_WATER_BOLT);

		scheduler
			.Schedule(5ms, [this](TaskContext water_bolt)
			{
				DoCastVictim(SPELL_WATER_BOLT);
				water_bolt.Repeat(2800ms);
			})
			.Schedule(10s, 15s, [this](TaskContext water_spout)
			{
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                {
                    CastStop();
                    DoCast(target, SPELL_WATER_SPOUT);
                }
				water_spout.Repeat(24s, 32s);
			})
			.Schedule(12s, 22s, [this](TaskContext water_bolt_volley)
			{
				CastStop(SPELL_WATER_SPOUT);
				DoCast(SPELL_WATER_BOLT_VOLLEY);
				water_bolt_volley.Repeat(18s, 20s);
			});
	}
};

struct npc_jaina_image : public CustomAI
{
	npc_jaina_image(Creature* creature) : CustomAI(creature, AI_Type::Distance)
	{
	}

	enum Spells
	{
		SPELL_ARCANE_PROJECTILES    = 5143,
		SPELL_EVOCATION             = 243070,
		SPELL_SUPERNOVA             = 157980,
		SPELL_ARCANE_BLAST          = 291316,
		SPELL_ARCANE_BARRAGE        = 291318,
		SPELL_MIRROR_IMAGE_FX       = 370617,
	};

	void Reset()
	{
		me->AddAura(SPELL_MIRROR_IMAGE_FX, me);
	}

	void JustEngagedWith(Unit* who) override
	{
		DoCast(who, SPELL_ARCANE_BLAST);

		scheduler
			.Schedule(5ms, [this](TaskContext context)
			{
				if (me->GetPowerPct(POWER_MANA) <= 20)
				{
					if (Spell* spell = me->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
					{
						if (spell->getState() != SPELL_STATE_FINISHED && spell->IsChannelActive())
						{
							context.Repeat(2s);
						}
					}
					else
					{
						const SpellInfo* info = sSpellMgr->AssertSpellInfo(SPELL_EVOCATION, DIFFICULTY_NONE);
						Milliseconds ms = Milliseconds(info->CalcDuration());
						CastSpellExtraArgs args(TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD);

						me->CastSpell(me, SPELL_EVOCATION, args);
						me->GetSpellHistory()->RestoreCharge(info->ChargeCategoryId);

						context.Repeat(ms + 800ms);
					}
				}
				else
				{
					Milliseconds ms = 50ms;
					if (!me->HasUnitState(UNIT_STATE_CASTING))
					{
						uint32 spellId = SPELL_ARCANE_BLAST;
						if (roll_chance_i(30))
						{
							spellId = SPELL_ARCANE_PROJECTILES;
						}
						else if (roll_chance_i(20))
						{
							spellId = SPELL_ARCANE_BARRAGE;
						}
						else if (roll_chance_i(10))
						{
							spellId = SPELL_SUPERNOVA;
						}

						const SpellInfo* info = sSpellMgr->AssertSpellInfo(spellId, DIFFICULTY_NONE);
						ms = Milliseconds(info->CalcCastTime());

						if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
						{
							me->CastSpell(target, spellId);
							me->GetSpellHistory()->RestoreCharge(info->ChargeCategoryId);
						}

						if (info->IsChanneled())
							ms = Milliseconds(info->CalcDuration(me));
					}

					context.Repeat(ms + 500ms);
				}
			});
	}
};

struct npc_roknah_warlord : public CustomAI
{
	npc_roknah_warlord(Creature* creature) : CustomAI(creature, AI_Type::Melee),
		sendEvent(false)
	{
		instance = creature->GetInstanceScript();
	}

	enum Spells
	{
		SPELL_EXECUTE               = 283424,
		SPELL_MORTAL_STRIKE         = 283410,
		SPELL_OVERPOWER             = 283426,
		SPELL_REND                  = 283419,
		SPELL_SLAM                  = 299995
	};

	InstanceScript* instance;
	bool sendEvent;

	void JustDied(Unit* killer) override
	{
		CustomAI::JustDied(killer);

		instance->TriggerGameEvent(EVENT_WARLORD_ROKNAH_SLAIN);
	}

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
		if (ShouldTakeDamage())
			return;

		if (!sendEvent)
		{
			sendEvent = true;

			me->SetHomePosition(me->GetPosition());
			me->SetRegenerateHealth(false);
			me->AI()->EnterEvadeMode();
			me->SetImmuneToAll(true);

			scheduler.Schedule(2s, [this](TaskContext /*context*/)
			{
				me->SetStandState(UNIT_STAND_STATE_KNEEL);
			});

			if (Creature* jaina = instance->GetCreature(DATA_JAINA_PROUDMOORE))
			{
				jaina->RemoveAllAuras();
				instance->SetData(EVENT_WARLORD_ROKNAH_SLAIN, 0U);
			}
		}

		damage = 0;
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		scheduler
			.Schedule(5s, 8s, [this](TaskContext execute)
			{
				DoCastVictim(SPELL_EXECUTE);
				execute.Repeat(15s, 28s);
			})
			.Schedule(2s, 5s, [this](TaskContext mortal_strike)
			{
				switch (mortal_strike.GetRepeatCounter())
				{
					case 0:
						if (!me->HasAura(SPELL_OVERPOWER) && roll_chance_i(60))
							DoCastSelf(SPELL_OVERPOWER);
						mortal_strike.Repeat(1s);
						break;
					case 1:
						me->CastStop();
						DoCastVictim(SPELL_MORTAL_STRIKE);
						mortal_strike.Repeat(8s, 10s);
						break;
				}
			})
			.Schedule(14s, 22s, [this](TaskContext overpower)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_REND);
				overpower.Repeat(8s, 10s);
			})
			.Schedule(25s, 32s, [this](TaskContext rend_slam)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, RAND(SPELL_REND, SPELL_SLAM));
				rend_slam.Repeat(2s, 8s);
			});
	}
};

struct go_theramore_banner : public GameObjectAI
{
	go_theramore_banner(GameObject* go) : GameObjectAI(go)
	{
		instance = go->GetInstanceScript();
	}

	enum Spells
	{
		SPELL_STANDARD_OF_THERAMORE = 105690
	};

	InstanceScript* instance;

	bool OnGossipHello(Player* player) override
	{
		RFTPhases phase = (RFTPhases)instance->GetData(DATA_SCENARIO_PHASE);
		if (phase != RFTPhases::Standards)
			return false;
		player->CastSpell(player, SPELL_STANDARD_OF_THERAMORE, true);
		me->DespawnOrUnsummon();
		return true;
	}
};

// Frigid Shards - 354933
class spell_ruins_frigid_shards : public AuraScript
{
	PrepareAuraScript(spell_ruins_frigid_shards);

	void OnPeriodic(AuraEffect const* aurEff)
	{
		Unit* target = GetTarget();
		Unit* caster = GetCaster();
		if (target && caster)
		{
			uint32 triggerSpell = GetSpellInfo()->GetEffect(aurEff->GetEffIndex()).TriggerSpell;
			caster->CastSpell(target, triggerSpell, true);
		}
	}

	void Register() override
	{
		OnEffectPeriodic += AuraEffectPeriodicFn(spell_ruins_frigid_shards::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
	}
};

// Comet Barrage - 354938
class spell_ruins_comet_barrage : public SpellScript
{
	PrepareSpellScript(spell_ruins_comet_barrage);

	void HandleDamages(SpellEffIndex effIndex)
	{
		Unit* caster = GetCaster();
		Unit* victim = GetHitUnit();
		if (caster && victim)
		{
			uint32 triggerSpell = GetSpellInfo()->GetEffect(effIndex).TriggerSpell;
			caster->CastSpell(victim, triggerSpell, true);
		}
	}

	void Register() override
	{
		OnEffectHitTarget += SpellEffectFn(spell_ruins_comet_barrage::HandleDamages, EFFECT_0, SPELL_EFFECT_DUMMY);
	}
};

void AddSC_npcs_ruins_of_theramore()
{
	// Utilisable en dehors de l'instance
	RegisterCreatureAI(npc_water_elementals_theramore);
	RegisterCreatureAI(npc_jaina_image);

	RegisterRuinsAI(npc_roknah_warlord);

	RegisterGameObjectAI(go_theramore_banner);

	RegisterSpellScript(spell_ruins_comet_barrage);
	RegisterSpellScript(spell_ruins_frigid_shards);
}
