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
#include "Custom/CustomAI/CustomAI.h"
#include "ruins_of_theramore.h"

struct npc_water_elementals_theramore : public CustomAI
{
	npc_water_elementals_theramore(Creature* creature) : CustomAI(creature), shielded(false)
	{
        SetCanRandomMovement(false);
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

struct npc_roknah_warlord : public CustomAI
{
	npc_roknah_warlord(Creature* creature) : CustomAI(creature, AI_Type::Melee), isAlmostDead(false)
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
	bool isAlmostDead;

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
        if (me->HealthBelowPctDamaged(20, damage))
        {
            damage = 0;

            if (isAlmostDead)
                return;

            isAlmostDead = true;

            me->SetHomePosition(me->GetPosition());
            me->SetReactState(REACT_PASSIVE);
            me->SetRegenerateHealth(false);
            me->SetImmuneToAll(true);
            me->RemoveAllAttackers();

            scheduler.Schedule(2s, [this](TaskContext /*context*/)
            {
                me->SetStandState(UNIT_STAND_STATE_KNEEL);
            });

            if (Creature* jaina = instance->GetCreature(DATA_JAINA_PROUDMOORE))
            {
                jaina->RemoveAllAuras();
            }

            instance->SetData(EVENT_WARLORD_ROKNAH_SLAIN, 0U);
        }
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
						if (!me->HasAura(SPELL_OVERPOWER) && roll_chance(60))
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

// Arcane Chaos - 406854
class spell_arcane_chaos : public AuraScript
{
    const float maxRange = 20.0f;

    enum Spells
    {
        SPELL_ARCANE_CHAOS_MISSILE = 406859,
    };

	void OnPeriodic(AuraEffect const* /*aurEff*/)
	{
        Unit* caster = GetCaster();
        if (!caster)
            return;

        std::list<Unit*> targets;

        // Check hostile unit
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck uCheck(caster, caster, maxRange);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(caster, targets, uCheck);
        Cell::VisitAllObjects(caster, searcher, maxRange);

        targets.remove_if([](Unit* unit)
        {
            return unit->isDead() || unit->HasUnitFlag2(UNIT_FLAG2_FEIGN_DEATH);;
        });

        for (Unit* victim : targets)
            caster->CastSpell(victim, SPELL_ARCANE_CHAOS_MISSILE, true);
	}

	void Register() override
	{
		OnEffectPeriodic += AuraEffectPeriodicFn(spell_arcane_chaos::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
	}

private:
    std::list<Unit*> targets;
};

void AddSC_npcs_ruins_of_theramore()
{
	// Utilisable en dehors de l'instance
	RegisterCreatureAI(npc_water_elementals_theramore);

	RegisterRuinsAI(npc_roknah_warlord);

	RegisterGameObjectAI(go_theramore_banner);

	RegisterSpellScript(spell_ruins_comet_barrage);
	RegisterSpellScript(spell_ruins_frigid_shards);
	RegisterSpellScript(spell_arcane_chaos);
}
