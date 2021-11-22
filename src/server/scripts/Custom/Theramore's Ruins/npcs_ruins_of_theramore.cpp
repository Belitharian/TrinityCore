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

// Summon Water Elementals - 84374
class spell_summon_water_elementals_theramore : public SpellScript
{
    PrepareSpellScript(spell_summon_water_elementals_theramore);

    void HandleEvent(SpellEffIndex effIndex)
    {
        if (Unit* caster = GetCaster())
        {
            if (InstanceScript* instance = caster->GetInstanceScript())
                instance->DoSendScenarioEvent(EVENT_BACK_TO_SENDER);
        }
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_summon_water_elementals_theramore::HandleEvent, EFFECT_0, SPELL_EFFECT_SEND_EVENT);
    }
};

void AddSC_npcs_ruins_of_theramore()
{
    new npc_water_elementals_theramore();

    new go_theramore_banner();

    RegisterSpellScript(spell_ruins_comet_barrage);
    RegisterSpellScript(spell_summon_water_elementals_theramore);

    RegisterAuraScript(spell_ruins_frigid_shards);
}
