#include "GameObject.h"
#include "GameObjectAI.h"
#include "InstanceScript.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "Custom/AI/CustomAI.h"
#include "ruins_of_theramore.h"

class npc_water_elementals_theramore : public CreatureScript
{
    public:
    npc_water_elementals_theramore() : CreatureScript("npc_water_elementals_theramore")
    {
    }

    struct npc_water_elementals_theramoreAI : public CustomAI
    {
        npc_water_elementals_theramoreAI(Creature* creature) : CustomAI(creature, AI_Type::Melee)
        {
        }

        enum Spells
        {
            SPELL_WATER_BOLT    = 125995,
            SPELL_WATER_SPOUT   = 39207
        };

        float GetDistance() override
        {
            return 12.f;
        }

        void JustEngagedWith(Unit* who) override
        {
            scheduler
                .Schedule(5s, 8s, [this](TaskContext water_bolt)
                {
                    DoCastVictim(SPELL_WATER_BOLT);
                    water_bolt.Repeat(8s, 10s);
                })
                .Schedule(12s, 22s, [this](TaskContext water_spout)
                {
                    if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0))
                        DoCast(target, SPELL_WATER_SPOUT);
                    water_spout.Repeat(18s, 20s);
                });
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_water_elementals_theramoreAI(creature);
    }
};

class npc_roknah_warlord : public CreatureScript
{
	public:
	npc_roknah_warlord() : CreatureScript("npc_roknah_warlord")
	{
	}

	struct npc_roknah_warlordAI : public CustomAI
	{
		npc_roknah_warlordAI(Creature* creature) : CustomAI(creature, AI_Type::Melee),
            isLow(false)
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
        bool isLow;

        void DamageTaken(Unit* /*attacker*/, uint32& damage) override
        {
            if (HealthAbovePct(40))
                return;

            if (!isLow)
            {
                instance->DoSendScenarioEvent(EVENT_WARLORD_ROKNAH_SLAIN);
                isLow = true;
            }
            else
            {
                damage = 0;
            }
        }

		void JustEngagedWith(Unit* who) override
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
					if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0))
						DoCast(target, SPELL_REND);
					overpower.Repeat(8s, 10s);
				})
				.Schedule(25s, 32s, [this](TaskContext rend_slam)
				{
					if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0))
						DoCast(target, RAND(SPELL_REND, SPELL_SLAM));
					rend_slam.Repeat(2s, 8s);
				});
		}
	};

	CreatureAI* GetAI(Creature* creature) const override
	{
		return GetRuinsOfTheramoreAI<npc_roknah_warlordAI>(creature);
	}
};

class go_theramore_banner : public GameObjectScript
{
    public:
    go_theramore_banner() : GameObjectScript("go_theramore_banner")
    {
    }

    struct go_theramore_bannerAI : public GameObjectAI
    {
        go_theramore_bannerAI(GameObject* go) : GameObjectAI(go)
        {
            instance = go->GetInstanceScript();
        }

        enum Spells
        {
            SPELL_STANDARD_OF_THERAMORE = 105690
        };

        InstanceScript* instance;

        bool GossipHello(Player* player) override
        {
            RFTPhases phase = (RFTPhases)instance->GetData(DATA_SCENARIO_PHASE);
            if (phase != RFTPhases::Standards)
                return false;
            player->CastSpell(player, SPELL_STANDARD_OF_THERAMORE, true);
            me->DespawnOrUnsummon();
            return true;
        }
    };

    GameObjectAI* GetAI(GameObject* go) const override
    {
        return GetRuinsOfTheramoreAI<go_theramore_bannerAI>(go);
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
    new npc_water_elementals_theramore();
    new npc_roknah_warlord();

    new go_theramore_banner();

    RegisterSpellScript(spell_ruins_comet_barrage);

    RegisterAuraScript(spell_ruins_frigid_shards);
}
