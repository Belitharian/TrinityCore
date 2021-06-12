#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ThreatManager.h"
#include "CustomAI.h"

enum Spells
{
    SPELL_LIGHTNING_BOLT    = 100026,
    SPELL_HEALING_WAVE      = 100025,
    SPELL_HEX               = 51514,
    SPELL_HEALING           = 100030,
    SPELL_LIGHTNING_CHAIN   = 100031,
    SPELL_SEARING_TOTEM     = 100106,
    SPELL_HEALING_TOTEM     = 100108,

    NPC_HEALING_TOTEM       = 100087
};

class npc_shaman : public CreatureScript
{
    public:
    npc_shaman() : CreatureScript("npc_shaman") {}

    struct npc_shamanAI : public CustomAI
    {
        npc_shamanAI(Creature* creature) : CustomAI(creature)
        {
        }

        void JustEngagedWith(Unit* who) override
        {
            scheduler
                .Schedule(5ms, [this](TaskContext lighting_bolt)
                {
                    DoCastVictim(SPELL_LIGHTNING_BOLT);
                    lighting_bolt.Repeat(3s);
                })
                .Schedule(12s, [this](TaskContext lightning_chain)
                {
                    DoCastVictim(SPELL_LIGHTNING_CHAIN);
                    lightning_chain.Repeat(5s, 10s);
                })
                .Schedule(3s, [this](TaskContext hex)
                {
                    if (Unit* target = SelectTarget(SelectTargetMethod::MaxThreat, 0))
                        DoCast(target, SPELL_HEX);
                    hex.Repeat(25s, 30s);
                })
                .Schedule(3s, [this](TaskContext healing_wave)
                {
                    if (Unit * target = DoSelectBelowHpPctFriendly(40.0f, 80, false))
                    {
                        me->InterruptNonMeleeSpells(true);
                        DoCast(target, SPELL_HEALING_WAVE);
                        healing_wave.Repeat(4s);
                    }
                    else
                    {
                        healing_wave.Repeat(1s);
                    }
                })
                .Schedule(3s, [this](TaskContext searing_totem)
                {
                    DoCast(SPELL_SEARING_TOTEM);
                    searing_totem.Repeat(30s, 45s);
                })
                .Schedule(6s, [this](TaskContext healing_totem)
                {
                    Creature* totem = me->FindNearestCreature(NPC_HEALING_TOTEM, 50.f);
                    if (!totem)
                    {
                        DoCast(SPELL_HEALING_TOTEM);
                    }
                    else
                    {
                        healing_totem.Repeat(20s, 30s);
                    }
                });
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_shamanAI(creature);
    }
};

void AddSC_npc_shaman()
{
    new npc_shaman();
}
