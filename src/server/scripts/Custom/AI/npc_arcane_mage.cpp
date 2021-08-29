#include "AreaTriggerAI.h"
#include "AreaTrigger.h"
#include "ScriptMgr.h"
#include "Unit.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellHistory.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "ScriptedCreature.h"
#include "CustomAI.h"

enum Spells
{
    SPELL_ARCANE_BOLT       = 154235,
    SPELL_SUPERNOVA         = 157980,
};

enum Misc
{
    NPC_KINNDY_SPARKSHINE   = 500001,
};

class npc_archmage_arcane : public CreatureScript
{
    public:
    npc_archmage_arcane() : CreatureScript("npc_archmage_arcane")
    {
    }

    struct npc_archmage_arcaneAI : public CustomAI
    {
        npc_archmage_arcaneAI(Creature* creature) : CustomAI(creature)
        {
        }

        void MovementInform(uint32 /*type*/, uint32 id) override
        {
            if (me->GetEntry() != NPC_KINNDY_SPARKSHINE)
                return;

            switch (id)
            {
                case 1:
                    me->SetFacingTo(4.62f);
                    me->SetVisible(false);
                    break;
                default:
                    break;
            }
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            scheduler
                .Schedule(8s, 10s, [this](TaskContext supernova)
                {
                    if (Unit* target = SelectTarget(SelectAggroTarget::SELECT_TARGET_MAXDISTANCE, 0))
                        DoCast(target, SPELL_SUPERNOVA);
                    supernova.Repeat(14s, 22s);
                })
                .Schedule(3s, [this](TaskContext arcane_bolt)
                {
                    DoCastVictim(SPELL_ARCANE_BOLT);
                    arcane_bolt.Repeat(2s);
                });
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_archmage_arcaneAI(creature);
    }
};

void AddSC_npc_archmage_arcane()
{
    new npc_archmage_arcane();
}
