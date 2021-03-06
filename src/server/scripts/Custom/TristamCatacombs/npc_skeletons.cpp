#include "tristam_catacombs.h"
#include "Custom/AI/CustomAI.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"

enum Spells
{
    SPELL_FROSTBOLT             = 100006,
    SPELL_ICE_LANCE             = 100007,

    SPELL_FIREBALL              = 100003,
    SPELL_FIREBLAST             = 100004,
    SPELL_LIVING_BOMB           = 100080,

    SPELL_UNBALANCING_STRIKE    = 100099,
    SPELL_SPIRIT_STRIKE         = 100100
};

enum DisplayId
{
    SKELETON_MAGE_FROST     = 9783,
    SKELETON_MAGE_FIRE      = 9784,
};

class npc_mage_skeleton : public CreatureScript
{
    public:
    npc_mage_skeleton() : CreatureScript("npc_mage_skeleton") {}

    struct npc_mage_skeletonAI : public CustomAI
    {
        npc_mage_skeletonAI(Creature* creature) : CustomAI(creature)
        {
            SetCombatMovement(false);
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            switch (me->GetDisplayId())
            {
                case SKELETON_MAGE_FROST:
                    scheduler
                        .Schedule(5ms, [this](TaskContext frostbotl)
                        {
                            DoCastVictim(SPELL_FROSTBOLT);
                            frostbotl.Repeat(1890ms);
                        })
                        .Schedule(3s, 6s, [this](TaskContext ice_lance)
                        {
                            me->InterruptNonMeleeSpells(false);
                            if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                                DoCast(target, SPELL_ICE_LANCE);
                            ice_lance.Repeat(8s, 14s);
                        });
                    break;
                case SKELETON_MAGE_FIRE:
                    scheduler
                        .Schedule(5ms, [this](TaskContext fireball)
                        {
                            DoCastVictim(SPELL_FIREBALL);
                            fireball.Repeat(2s);
                        })
                        .Schedule(8s, 12s, [this](TaskContext living_bomb)
                        {
                            if (Unit* target = SelectTarget(SelectTargetMethod::MaxThreat, 0))
                            {
                                if (!target->HasAura(SPELL_LIVING_BOMB))
                                    DoCast(target, SPELL_LIVING_BOMB);
                            }
                            living_bomb.Repeat(14s, 23s);
                        })
                        .Schedule(3s, 6s, [this](TaskContext fireblast)
                        {
                            me->InterruptNonMeleeSpells(false);
                            if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                                DoCast(target, SPELL_FIREBLAST);
                            fireblast.Repeat(8s, 14s);
                        });
                    break;
                default:
                    break;
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetTristamCatacombsAI<npc_mage_skeletonAI>(creature);
    }
};

class npc_skeleton : public CreatureScript
{
    public:
    npc_skeleton() : CreatureScript("npc_skeleton")
    {
    }

    struct npc_skeletonAI : public CustomAI
    {
        npc_skeletonAI(Creature* creature) : CustomAI(creature, AI_Type::Melee)
        {
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            scheduler
                .Schedule(5s, 8s, [this](TaskContext unbalancing_strike)
                {
                    DoCastVictim(SPELL_UNBALANCING_STRIKE);
                    unbalancing_strike.Repeat(20s, 30s);
                })
                .Schedule(12s, 18s, [this](TaskContext spirit_strike)
                {
                    DoCastVictim(SPELL_SPIRIT_STRIKE);
                    spirit_strike.Repeat(40s, 50s);
                });
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetTristamCatacombsAI<npc_skeletonAI>(creature);
    }
};

void AddSC_npc_skeletons()
{
    new npc_mage_skeleton();
    new npc_skeleton();
}
