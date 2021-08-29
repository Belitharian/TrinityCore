#include "ScriptMgr.h"
#include "Unit.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "SpellMgr.h"
#include "ScriptedCreature.h"
#include "CustomAI.h"

enum Spells
{
    SPELL_SMITE                     = 332705,
    SPELL_HEAL                      = 332706,
    SPELL_FLASH_HEAL                = 314655,
    SPELL_RENEW                     = 294342,
    SPELL_PRAYER_OF_HEALING         = 220118,
    SPELL_POWER_WORD_SHIELD         = 318158,
    SPELL_POWER_WORD_BARRIER        = 204756,
    SPELL_POWER_WORD_BARRIER_BUFF   = 204760,
    SPELL_LEVITATE                  = 1706,
    SPELL_PAIN_SUPPRESSION          = 69910,
};

class npc_priest : public CreatureScript
{
    public:
    npc_priest() : CreatureScript("npc_priest")
    {
    }

    struct npc_priestAI : public CustomAI
    {
        npc_priestAI(Creature* creature) : CustomAI(creature), _ascension(false)
        {
        }

        bool _ascension;

        void Reset()
        {
            CustomAI::Reset();

            if (roll_chance_i(60))
                DoCastSelf(SPELL_LEVITATE);
        }

        void DamageTaken(Unit* /*attacker*/, uint32& damage) override
        {
            if (!_ascension && HealthBelowPct(30))
            {
                _ascension = true;

                scheduler.CancelAll();

                CastSpellExtraArgs args;
                args.AddSpellBP0(85000);

                me->CastStop();
                DoCastSelf(SPELL_PRAYER_OF_HEALING, args);
                DoCastSelf(SPELL_PAIN_SUPPRESSION, true);

                scheduler
                    .Schedule(4s, [this](TaskContext /*context*/)
                    {
                        StartCombatRoutine();
                    })
                    .Schedule(1min, [this](TaskContext /*context*/)
                    {
                        _ascension = false;
                    });
            }
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            StartCombatRoutine();
        }

        void StartCombatRoutine()
        {
            scheduler
                .Schedule(5ms, [this](TaskContext smite)
                {
                    DoCastVictim(SPELL_SMITE);
                    smite.Repeat(2s);
                })
                .Schedule(14s, [this](TaskContext prayer_of_healing)
                {
                    prayer_of_healing.Repeat(25s);
                })
                .Schedule(3s, [this](TaskContext heal)
                {
                    if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 60))
                    {
                        me->CastStop();
                        DoCast(target, SPELL_HEAL);
                    }
                    heal.Repeat(8s, 14s);
                })
                .Schedule(8s, [this](TaskContext flash_heal)
                {
                    if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 50))
                    {
                        me->CastStop();
                        DoCast(target, SPELL_FLASH_HEAL);
                    }
                    flash_heal.Repeat(2s);
                });
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_priestAI(creature);
    }
};

void AddSC_npc_priest()
{
    new npc_priest();
}
