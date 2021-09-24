#include "ScriptMgr.h"
#include "Unit.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "SpellMgr.h"
#include "ScriptedCreature.h"
#include "CustomAI.h"

enum Spells
{
    SPELL_CHILLED               = 333602,
    SPELL_FLURRY                = 320008,
    SPELL_ICE_NOVA              = 157997,
    SPELL_COMET_STORM           = 153595,
    SPELL_COMET_STORM_DAMAGE    = 285127,
    SPELL_COMET_STORM_VISUAL    = 242210
};

class npc_archmage_frost : public CreatureScript
{
    public:
    npc_archmage_frost() : CreatureScript("npc_archmage_frost")
    {
    }

    struct npc_archmage_frostAI : public CustomAI
    {
        npc_archmage_frostAI(Creature* creature) : CustomAI(creature)
        {
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            scheduler
                .Schedule(8s, 10s, [this](TaskContext chilled)
                {
                    if (Unit* target = SelectTarget(SelectAggroTarget::SELECT_TARGET_RANDOM, 0))
                        DoCast(target, SPELL_CHILLED);
                    chilled.Repeat(14s, 22s);
                })
                .Schedule(12s, 18s, [this](TaskContext comet_barrage)
                {
                    if (Unit* target = SelectTarget(SelectAggroTarget::SELECT_TARGET_MAXDISTANCE, 0))
                        DoCast(target, SPELL_COMET_STORM);
                    comet_barrage.Repeat(30s, 35s);
                })
                .Schedule(5ms, [this](TaskContext frostbolt)
                {
                    DoCastVictim(SPELL_FLURRY);
                    frostbolt.Repeat(2s);
                })
                .Schedule(5s, [this](TaskContext ice_nova)
                {
                    for (auto* ref : me->GetThreatManager().GetUnsortedThreatList())
                    {
                        Unit* target = ref->GetVictim();
                        if (target && target->isMoving())
                        {
                            me->CastStop();
                            DoCast(target, SPELL_ICE_NOVA);
                            ice_nova.Repeat(3s, 5s);
                            return;
                        }
                    }
                    ice_nova.Repeat(1s);
                });
        }

        void UpdateAI(uint32 diff) override
        {
            ScriptedAI::UpdateAI(diff);

            scheduler.Update(diff);

            if (!UpdateVictim())
                return;

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_archmage_frostAI(creature);
    }
};

// 285126 - Comet storm
class spell_comet_storm_ai : public SpellScriptLoader
{
    public:
    spell_comet_storm_ai() : SpellScriptLoader("spell_comet_storm_ai")
    {
    }

    class spell_comet_storm_ai_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_comet_storm_ai_SpellScript);

        void HandleDummy()
        {
            if (Unit* caster = GetCaster())
            {
                if (WorldLocation const* dest = GetExplTargetDest())
                {
                    Position targetPos = dest->GetPosition();
                    for (uint8 i = 0; i < 7; ++i)
                        caster->CastSpell(targetPos, SPELL_COMET_STORM_VISUAL, true);
                }
            }
        }

        void HandleDamage()
        {
            if (Unit* caster = GetCaster())
            {
                if (WorldLocation const* dest = GetExplTargetDest())
                {
                    Position targetPos = dest->GetPosition();
                    for (uint8 i = 0; i < 7; ++i)
                        caster->CastSpell(targetPos, SPELL_COMET_STORM_DAMAGE, true);
                }
            }
        }

        void Register() override
        {
            OnCast += SpellCastFn(spell_comet_storm_ai_SpellScript::HandleDummy);
            AfterCast += SpellCastFn(spell_comet_storm_ai_SpellScript::HandleDamage);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_comet_storm_ai_SpellScript();
    }
};

void AddSC_npc_archmage_frost()
{
    new npc_archmage_frost();
    new spell_comet_storm_ai();
}
