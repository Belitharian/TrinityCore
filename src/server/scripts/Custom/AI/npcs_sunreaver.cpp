#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "World.h"
#include "CustomAI.h"

enum Misc
{
    // Quests
    QUEST_SEWER_CLEANING        = 80017,
    NPC_INVISIBLE_STALKER       = 32780,
    SUNREAVER_KILL_CREDIT       = 100062,
    SAY_BRASAEL_AGGRO           = 0,
};

enum Spells
{
    // Assassin
    SPELL_SINISTER_STRIKE       = 59409,
    SPELL_KIDNEY_SHOT           = 72335,
    SPELL_STEALTH               = 32615,
    SPELL_SHADOWSTEP            = 36563,

    // Duelist
    SPELL_MIGHTY_KICK           = 69021,

    // Pyromancer
    SPELL_FIRE_ARMOR            = 43046,
    SPELL_FIREBALL              = 100003,
    SPELL_FIREBLAST             = 100004,
    SPELL_PYROBLAST             = 100005,

    // Frosthand
    SPELL_FROST_ARMOR           = 43008,
    SPELL_FROSTBOLT             = 100006,
    SPELL_ICE_LANCE             = 100007,
    SPELL_FROST_CONE            = 65023,

    // Brasael
    SPELL_SCORCH                = 100055,
    SPELL_METEOR                = 100054,
    SPELL_DRAGON_BREATH         = 37289,
    SPELL_POSTCOMBUSTION        = 100082,
    SPELL_DUMMY_METEOR          = 100056 
};

enum class Phases
{
    Normal                      = 1,
    Combustion
};

void KillCredit(Unit* killer)
{
    if (killer->GetTypeId() != TYPEID_PLAYER)
        return;

    Player* player = killer->ToPlayer();
    if (player && player->GetQuestStatus(QUEST_SEWER_CLEANING) == QUEST_STATUS_INCOMPLETE)
        player->KilledMonsterCredit(SUNREAVER_KILL_CREDIT);
}

class npc_sunreaver_assassin : public CreatureScript
{
    public:
    npc_sunreaver_assassin() : CreatureScript("npc_sunreaver_assassin")
    {
    }

    struct npc_sunreaver_assassinAI : public CustomAI
    {
        npc_sunreaver_assassinAI(Creature* creature) : CustomAI(creature, AI_Type::Melee)
        {
        }

        void JustEngagedWith(Unit* who) override
        {
            scheduler
                .Schedule(100ms, [this, who](TaskContext /*context*/)
                {
                    if (me->HasAura(SPELL_STEALTH))
                    {
                        me->RemoveAurasDueToSpell(SPELL_STEALTH);

                        DoCast(who, SPELL_SHADOWSTEP);
                        me->AddAura(SPELL_KIDNEY_SHOT, who);
                    }
                })
                .Schedule(5s, [this](TaskContext sinister_strike)
                {
                    DoCast(SPELL_SINISTER_STRIKE);
                    sinister_strike.Repeat(14s, 28s);
                })
                .Schedule(10s, [this](TaskContext kidney_shot)
                {
                    if (Unit* target = DoSelectCastingUnit(SPELL_KIDNEY_SHOT, 35.f))
                    {
                        DoCast(target, SPELL_KIDNEY_SHOT);
                        kidney_shot.Repeat(25s, 40s);
                    }
                    else
                    {
                        kidney_shot.Repeat(1s);
                    }
                });
        }

        void JustDied(Unit* killer) override
        {
            KillCredit(killer);
        }

        void UpdateAI(uint32 diff) override
        {
            // aura refresh
            if (!UpdateVictim())
            {
                if (!me->HasAura(SPELL_STEALTH))
                    DoCastSelf(SPELL_STEALTH);
                return;
            }

            CustomAI::UpdateAI(diff);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_sunreaver_assassinAI(creature);
    }
};

class npc_sunreaver_duelist : public CreatureScript
{
    public:
    npc_sunreaver_duelist() : CreatureScript("npc_sunreaver_duelist")
    {
    }

    struct npc_sunreaver_duelistAI : public CustomAI
    {
        npc_sunreaver_duelistAI(Creature* creature) : CustomAI(creature, AI_Type::Melee)
        {
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            scheduler.Schedule(5s, [this](TaskContext mighty_kick)
            {
                DoCast(SPELL_MIGHTY_KICK);
                mighty_kick.Repeat(14s, 28s);
            });
        }

        void JustDied(Unit* killer) override
        {
            KillCredit(killer);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_sunreaver_duelistAI(creature);
    }
};

class npc_sunreaver_pyromancer : public CreatureScript
{
    public:
    npc_sunreaver_pyromancer() : CreatureScript("npc_sunreaver_pyromancer")
    {
    }

    struct npc_sunreaver_pyromancerAI : public CustomAI
    {
        npc_sunreaver_pyromancerAI(Creature* creature) : CustomAI(creature)
        {
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            scheduler
                .Schedule(5ms, [this](TaskContext fireball)
                {
                    DoCast(SPELL_FIREBALL);
                    fireball.Repeat(5s, 8s);
                })
                .Schedule(14s, [this](TaskContext fireblast)
                {
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                        DoCast(target, SPELL_FIREBLAST);
                    fireblast.Repeat(14s, 28s);
                })
                .Schedule(20s, [this](TaskContext pyroblast)
                {
                    DoCast(SPELL_PYROBLAST);
                    pyroblast.Repeat(18s, 30s);
                });
        }

        void JustDied(Unit* killer) override
        {
            KillCredit(killer);
        }

        void UpdateAI(uint32 diff) override
        {
            // aura refresh
            if (!UpdateVictim())
            {
                if (!me->HasAura(SPELL_FIRE_ARMOR))
                    DoCastSelf(SPELL_FIRE_ARMOR);
                return;
            }

            CustomAI::UpdateAI(diff);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_sunreaver_pyromancerAI(creature);
    }
};

class npc_sunreaver_frosthand : public CreatureScript
{
    public:
    npc_sunreaver_frosthand() : CreatureScript("npc_sunreaver_frosthand")
    {
    }

    struct npc_sunreaver_frosthandAI : public CustomAI
    {
        npc_sunreaver_frosthandAI(Creature* creature) : CustomAI(creature)
        {
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            scheduler
                .Schedule(5ms, [this](TaskContext frostbolt)
                {
                    DoCast(SPELL_FROSTBOLT);
                    frostbolt.Repeat(5s, 8s);
                })
                .Schedule(14s, [this](TaskContext ice_lance)
                {
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                        DoCast(target, SPELL_ICE_LANCE);
                    ice_lance.Repeat(14s, 28s);
                })
                .Schedule(20s, [this](TaskContext frost_cone)
                {
                    DoCast(SPELL_FROST_CONE);
                    frost_cone.Repeat(18s, 30s);
                });
        }

        void JustDied(Unit* killer) override
        {
            KillCredit(killer);
        }

        void UpdateAI(uint32 diff) override
        {
            // aura refresh
            if (!UpdateVictim())
            {
                if (!me->HasAura(SPELL_FROST_ARMOR))
                    DoCastSelf(SPELL_FROST_ARMOR);
                return;
            }

            CustomAI::UpdateAI(diff);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_sunreaver_frosthandAI(creature);
    }
};

class npc_magister_brasael : public CreatureScript
{
    public:
    npc_magister_brasael() : CreatureScript("npc_magister_brasael")
    {
    }

    struct npc_magister_brasaelAI : public CustomAI
    {
        npc_magister_brasaelAI(Creature* creature) : CustomAI(creature),
            phase(Phases::Normal), meteors(0), combustionUsed(false)
        {
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            Talk(SAY_BRASAEL_AGGRO);
            SetCombatToNormal();
        }

        void DamageTaken(Unit* attacker, uint32& /*damage*/) override
        {
            if (!combustionUsed && HealthBelowPct(80))
            {
                combustionUsed = true;

                phase = Phases::Combustion;

                scheduler.Schedule(1s, (int)Phases::Combustion, [this](TaskContext postcombustion)
                {
                    if (me->GetVictim()->GetTypeId() == TYPEID_PLAYER)
                    {
                        scheduler.CancelGroup((int)Phases::Normal);

                        DoCastSelf(SPELL_POSTCOMBUSTION);

                        me->SetControlled(true, UNIT_STATE_ROOT);
                        me->NearTeleportTo(5966.76f, 613.82f, 650.62f, 2.77f);
                        me->PlayDistanceSound(3226, me->GetVictim()->ToPlayer());
                    }
                    else
                    {
                        phase = Phases::Normal;
                        scheduler.CancelGroup((int)Phases::Combustion);
                    }
                });
            }
        }

        void KilledUnit(Unit* /*victim*/) override
        {
            me->SetControlled(false, UNIT_STATE_ROOT);
        }

        void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
        {
            if (spellInfo->Id == SPELL_POSTCOMBUSTION)
            {
                scheduler.Schedule(1s, (int)Phases::Combustion, [this](TaskContext meteor)
                {
                    if (meteors > 2)
                    {
                        me->SetControlled(false, UNIT_STATE_ROOT);
                        SetCombatToNormal();
                        return;
                    }

                    meteors++;

                    const Position spellDestination = SelectTarget(SelectTargetMethod::Random, 0)->GetPosition();
                    if (Creature* fx = DoSummon(NPC_INVISIBLE_STALKER, spellDestination, 7s, TEMPSUMMON_TIMED_DESPAWN))
                        fx->CastSpell(fx, SPELL_DUMMY_METEOR);

                    me->CastSpell(spellDestination, SPELL_METEOR);

                    meteor.Repeat(2830ms);
                });
            }
        }

        void JustDied(Unit* killer) override
        {
            KillCredit(killer);
        }

        void Reset() override
        {
            phase = Phases::Normal;
            combustionUsed = false;
            meteors = 0;
        }

        void UpdateAI(uint32 diff) override
        {
            // Combat
            if (!UpdateVictim())
            {
                if (!me->HasAura(SPELL_FIRE_ARMOR))
                    DoCastSelf(SPELL_FIRE_ARMOR);
                return;
            }

            CustomAI::UpdateAI(diff);
        }

        private:
        Phases phase;
        uint8 meteors;
        bool combustionUsed;

        void SetCombatToNormal()
        {
            phase = Phases::Normal;

            scheduler
                .Schedule(5ms, (int)Phases::Normal, [this](TaskContext scorch)
                {
                    DoCast(SPELL_SCORCH);
                    scorch.Repeat(5s, 8s);
                })
                .Schedule(14s, (int)Phases::Normal, [this](TaskContext dragonBreath)
                {
                    DoCast(SPELL_DRAGON_BREATH);
                    dragonBreath.Repeat(14s, 28s);
                });
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_magister_brasaelAI(creature);
    }
};

void AddSC_npcs_sunreaver()
{
    new npc_sunreaver_assassin();
    new npc_sunreaver_duelist();
    new npc_sunreaver_pyromancer();
    new npc_sunreaver_frosthand();
    new npc_magister_brasael();
}
