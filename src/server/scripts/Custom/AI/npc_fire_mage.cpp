#include "ScriptMgr.h"
#include "Unit.h"
#include "SpellAuraEffects.h"
#include "SpellMgr.h"
#include "ScriptedCreature.h"
#include "CustomAI.h"

enum Spells
{
    SPELL_FIREBALL          = 358226,
    SPELL_FLAMESTRIKE       = 330347,
    SPELL_BLAZING_BARRIER   = 255714,
    SPELL_SCORCH            = 358238,
    SPELL_CONFLAGRATION     = 226757
};

enum Misc
{
    NPC_ARCHMAGE_TERVOSH    = 500000
};

class npc_archmage_fire : public CreatureScript
{
    public:
    npc_archmage_fire() : CreatureScript("npc_archmage_fire")
    {
    }

    struct npc_archmage_fireAI : public CustomAI
    {
        npc_archmage_fireAI(Creature* creature) : CustomAI(creature)
        {
        }

        void MovementInform(uint32 /*type*/, uint32 id) override
        {
            if (me->GetEntry() != NPC_ARCHMAGE_TERVOSH)
                return;

            switch (id)
            {
                case 0:
                    me->SetFacingTo(0.70f);
                    break;
            }
        }

        void SpellHitTarget(Unit* target, SpellInfo const* spell) override
        {
            switch (spell->Id)
            {
                case SPELL_FIREBALL:
                case SPELL_FLAMESTRIKE:
                case SPELL_SCORCH:
                {
                    Unit* victim = target->ToUnit();
                    if (victim && !victim->HasAura(SPELL_CONFLAGRATION) && roll_chance_i(40))
                        DoCast(victim, SPELL_CONFLAGRATION, true);
                }
                break;
            }
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            DoCast(SPELL_BLAZING_BARRIER);

            scheduler
                .Schedule(30s, [this](TaskContext blazing_barrier)
                {
                    if (!me->HasAura(SPELL_BLAZING_BARRIER))
                    {
                        DoCast(SPELL_BLAZING_BARRIER);
                        blazing_barrier.Repeat(30s);
                    }
                    else
                    {
                        blazing_barrier.Repeat(1s);
                    }
                })
                .Schedule(8s, 10s, [this](TaskContext fireblast)
                {
                    if (Unit* target = SelectTarget(SelectAggroTarget::SELECT_TARGET_RANDOM, 0))
                        DoCast(target, SPELL_SCORCH);
                    fireblast.Repeat(14s, 22s);
                })
                .Schedule(12s, 18s, [this](TaskContext pyroblast)
                {
                    if (Unit* target = SelectTarget(SelectAggroTarget::SELECT_TARGET_MAXDISTANCE, 0))
                        DoCast(target, SPELL_FLAMESTRIKE);
                    pyroblast.Repeat(22s, 35s);
                })
                .Schedule(5ms, [this](TaskContext fireball)
                {
                    DoCastVictim(SPELL_FIREBALL);
                    fireball.Repeat(2s);
                });
        }

        void UpdateAI(uint32 diff) override
        {
            ScriptedAI::UpdateAI(diff);

            if (!UpdateVictim())
                return;

            scheduler.Update(diff, [this]
            {
                DoMeleeAttackIfReady();
            });
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_archmage_fireAI(creature);
    }
};

void AddSC_npc_archmage_fire()
{
    new npc_archmage_fire();
}
