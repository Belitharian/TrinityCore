#include "boost/range/algorithm/reverse.hpp"
#include "ScriptMgr.h"
#include "ObjectAccessor.h"
#include "MotionMaster.h"
#include "GameObject.h"
#include "PassiveAI.h"
#include "Scenario.h"
#include "ScriptedCreature.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "SpellHistory.h"
#include "InstanceScript.h"
#include "battle_for_theramore.h"
#include "Custom/AI/CustomAI.h"

class npc_jaina_theramore : public CreatureScript
{
    public:
    npc_jaina_theramore() : CreatureScript("npc_jaina_theramore")
    {
    }

    struct npc_jaina_theramoreAI : public CustomAI
    {
        npc_jaina_theramoreAI(Creature* creature) : CustomAI(creature, AI_Type::Melee)
        {
            Initialize();
        }

        enum Spells
        {
            SPELL_ICE_SHARD         = 290621,
            SPELL_BROADSIDE         = 288218,
            SPELL_RING_OF_FROST     = 285459
        };

        void Initialize()
        {
            instance = me->GetInstanceScript();
        }

        InstanceScript* instance;

        void Reset() override
        {
            Initialize();
        }

        void JustEngagedWith(Unit* who) override
        {
            scheduler
                .Schedule(5ms, [this](TaskContext ice_shard)
                {
                    DoCastVictim(SPELL_ICE_SHARD);
                    ice_shard.Repeat(8s);
                })
                .Schedule(2s, [this](TaskContext broadside)
                {
                    if (Unit* victim = SelectTarget(SELECT_TARGET_RANDOM))
                        DoCast(victim, SPELL_BROADSIDE);
                    broadside.Repeat(12s, 14s);
                })
                .Schedule(15s, [this](TaskContext ring_of_frost)
                {
                    DoCast(SPELL_RING_OF_FROST);
                    ring_of_frost.Repeat(30s);
                });
        }

        void SetData(uint32 id, uint32 value) override
        {
            if (id == 100)
            {
                if (Scenario* scenario = me->GetScenario())
                    scenario->CompleteCurrentStep();
            }
            else if (id == 200)
            {
                instance->SetData(DATA_SCENARIO_PHASE, value);
            }
        }

        void MovementInform(uint32 type, uint32 id) override
        {
            if (type != POINT_MOTION_TYPE)
                return;

            switch (id)
            {
                case 0:
                    instance->DoSendScenarioEvent(EVENT_THE_COUNCIL);
                    instance->SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::Waiting);
                    break;
                default:
                    break;
            }
        }

        void MoveInLineOfSight(Unit* who) override
        {
            ScriptedAI::MoveInLineOfSight(who);

            if (me->IsEngaged())
                return;

            if (who->GetTypeId() != TYPEID_PLAYER)
                return;

            if (Player* player = who->ToPlayer())
            {
                if (player->IsGameMaster())
                    return;

                if (player->IsFriendlyTo(me) && player->IsWithinDist(me, 4.f))
                {
                    BFTPhases phase = (BFTPhases)instance->GetData(DATA_SCENARIO_PHASE);
                    switch (phase)
                    {
                        case BFTPhases::FindJaina:
                            instance->DoSendScenarioEvent(EVENT_FIND_JAINA);
                            instance->SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::TheCouncil);
                            break;
                        case BFTPhases::ALittleHelp:
                            instance->DoSendScenarioEvent(EVENT_A_LITTLE_HELP);
                            instance->SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::Preparation);
                            break;
                        case BFTPhases::ExposeTheSpy:
                            instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, me);
                            instance->DoSendScenarioEvent(EVENT_RETRIEVE_JAINA);
                            instance->SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::Battle);
                            break;
                        default:
                            break;
                    }
                }
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<npc_jaina_theramoreAI>(creature);
    }
};

class npc_pained : public CreatureScript
{
    public:
    npc_pained() : CreatureScript("npc_pained")
    {
    }

    struct npc_painedAI : public ScriptedAI
    {
        npc_painedAI(Creature* creature) : ScriptedAI(creature)
        {
            instance = me->GetInstanceScript();
        }

        InstanceScript* instance;

        void MovementInform(uint32 type, uint32 id) override
        {
            if (type != EFFECT_MOTION_TYPE)
                return;

            switch (id)
            {
                case 2:
                    me->SetVisible(false);
                    instance->DoSendScenarioEvent(EVENT_THE_UNKNOWN_TAUREN);
                    instance->SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::Evacuation);
                    break;
                default:
                    break;
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<npc_painedAI>(creature);
    }
};

class npc_kalecgos_theramore : public CreatureScript
{
    public:
    npc_kalecgos_theramore() : CreatureScript("npc_kalecgos_theramore")
    {
    }

    struct npc_kalecgos_theramoreAI : public CustomAI
    {
        npc_kalecgos_theramoreAI(Creature* creature) : CustomAI(creature)
        {
            instance = me->GetInstanceScript();
        }

        enum Spells
        {
            SPELL_COMET_STORM           = 153595,
            SPELL_DISSOLVE              = 255295,
            SPELL_TELEPORT              = 357601,
            SPELL_CHILLED               = 333602,
            SPELL_FLURRY                = 320008,
            SPELL_ICE_NOVA              = 157997
        };

        InstanceScript* instance;

        void MovementInform(uint32 type, uint32 id) override
        {
            if (type != POINT_MOTION_TYPE)
                return;

            switch (id)
            {
                case 0:
                    me->AddUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                    scheduler.Schedule(2s, [this](TaskContext /*context*/)
                    {
                        DoCastSelf(SPELL_DISSOLVE);
                        DoCastSelf(SPELL_TELEPORT, true);
                    });
                    break;
                default:
                    break;
            }
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
                            me->InterruptNonMeleeSpells(true);
                            DoCast(target, SPELL_ICE_NOVA);
                            ice_nova.Repeat(3s, 5s);
                            return;
                        }
                    }
                    ice_nova.Repeat(1s);
                });
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<npc_kalecgos_theramoreAI>(creature);
    }
};

class event_theramore_training : public CreatureScript
{
    public:
    event_theramore_training() : CreatureScript("event_theramore_training")
    {
    }

    struct event_theramore_trainingAI : public NullCreatureAI
    {
        event_theramore_trainingAI(Creature* creature) : NullCreatureAI(creature), launched(false)
        {
            instance = creature->GetInstanceScript();
        }

        TaskScheduler scheduler;
        InstanceScript* instance;
        bool launched;

        enum Misc
        {
            // NPCs
            NPC_TRAINING_DUMMY              = 87318,

            // Spells
            SPELL_FLASH_HEAL                = 314655,
            SPELL_HEAL                      = 332706,
            SPELL_POWER_WORD_SHIELD         = 318158,
            SPELL_ARCANE_PROJECTILES        = 5143,
            SPELL_EVOCATION                 = 243070,
            SPELL_SUPERNOVA                 = 157980,
            SPELL_ARCANE_BLAST              = 291316,
            SPELL_ARCANE_BARRAGE            = 291318,
        };

        void Initialize(Creature* creature, Emote emote, Creature* target)
        {
            uint64 health = creature->GetMaxHealth() * 0.3f;
            creature->SetEmoteState(emote);
            creature->SetRegenerateHealth(false);
            creature->SetHealth(health);

            if (target)
                creature->SetTarget(target->GetGUID());
        }

        void DoAction(int32 param) override
        {
            std::vector<Creature*> footmen;
            GetCreatureListWithEntryInGrid(footmen, me, NPC_THERAMORE_FOOTMAN, 15.f);
            if (footmen.empty())
                return;

            Creature* faithful = GetClosestCreatureWithEntry(me, NPC_THERAMORE_FAITHFUL, 15.f);
            Creature* arcanist = GetClosestCreatureWithEntry(me, NPC_THERAMORE_ARCANIST, 15.f);
            Creature* training = GetClosestCreatureWithEntry(me, NPC_TRAINING_DUMMY, 15.f);

            if (faithful && arcanist && training)
            {
                if (param == 2)
                {
                    footmen[0]->SetVisible(false);
                    footmen[1]->SetVisible(false);
                    faithful->SetVisible(false);
                    arcanist->SetVisible(false);
                    training->SetVisible(false);
                }
                else
                {
                    Initialize(footmen[0], EMOTE_STATE_ATTACK1H, footmen[1]);
                    Initialize(footmen[1], EMOTE_STATE_BLOCK_SHIELD, footmen[0]);

                    footmen[0]->SetReactState(REACT_PASSIVE);
                    footmen[1]->SetReactState(REACT_PASSIVE);
                    faithful->SetReactState(REACT_PASSIVE);
                    arcanist->SetReactState(REACT_PASSIVE);

                    training->SetImmuneToPC(true);

                    scheduler
                        .Schedule(1s, [faithful, footmen](TaskContext context)
                        {
                            Creature* victim = footmen[urand(0, 1)];
                            faithful->CastSpell(victim, RAND(SPELL_FLASH_HEAL, SPELL_HEAL, SPELL_POWER_WORD_SHIELD));
                            context.Repeat(8s);
                        })
                        .Schedule(1s, [footmen](TaskContext context)
                        {
                            if (!footmen[0]->HasAura(SPELL_POWER_WORD_SHIELD))
                                footmen[0]->DealDamage(footmen[1], footmen[0], urand(150, 200));

                            if (!footmen[1]->HasAura(SPELL_POWER_WORD_SHIELD))
                                footmen[1]->DealDamage(footmen[0], footmen[1], urand(150, 200));

                            context.Repeat(8s);
                        })
                        .Schedule(1s, [arcanist, training](TaskContext context)
                        {
                            if (arcanist->GetPowerPct(POWER_MANA) <= 20)
                            {
                                const SpellInfo* info = sSpellMgr->AssertSpellInfo(SPELL_EVOCATION, DIFFICULTY_NONE);

                                arcanist->CastSpell(arcanist, SPELL_EVOCATION);
                                arcanist->GetSpellHistory()->ResetCooldown(info->Id, true);
                                arcanist->GetSpellHistory()->RestoreCharge(info->ChargeCategoryId);

                                context.Repeat(7s);
                            }
                            else
                            {
                                uint32 spellId = SPELL_ARCANE_BLAST;
                                if (roll_chance_i(30))
                                {
                                    spellId = SPELL_ARCANE_PROJECTILES;
                                }
                                else if (roll_chance_i(40))
                                {
                                    spellId = SPELL_ARCANE_BARRAGE;
                                }
                                else if (roll_chance_i(20))
                                {
                                    spellId = SPELL_SUPERNOVA;
                                }

                                const SpellInfo* info = sSpellMgr->AssertSpellInfo(spellId, DIFFICULTY_NONE);
                                Milliseconds ms = Milliseconds(info->CalcCastTime());

                                arcanist->CastSpell(training, spellId);

                                arcanist->GetSpellHistory()->ResetCooldown(info->Id, true);
                                arcanist->GetSpellHistory()->RestoreCharge(info->ChargeCategoryId);

                                if (info->IsChanneled())
                                    ms = Milliseconds(info->CalcDuration(arcanist));

                                context.Repeat(ms + 500ms);
                            }
                        });
                }
            }
        }

        void Reset() override
        {
            scheduler.CancelAll();
        }

        void UpdateAI(uint32 diff) override
        {
            if (!launched)
            {
                DoAction(1U);
                launched = true;
            }

            scheduler.Update(diff);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<event_theramore_trainingAI>(creature);
    }
};

class event_theramore_faithful : public CreatureScript
{
    public:
    event_theramore_faithful() : CreatureScript("event_theramore_faithful")
    {
    }

    struct event_theramore_faithfulAI : public ScriptedAI
    {
        event_theramore_faithfulAI(Creature* creature) : ScriptedAI(creature), launched(false)
        {
            instance = creature->GetInstanceScript();
        }

        TaskScheduler scheduler;
        InstanceScript* instance;
        bool launched;

        enum Misc
        {
            // Spells
            SPELL_HOLY_CHANNELING           = 235056,
            SPELL_RENEW                     = 294342,
        };

        void DoAction(int32 param) override
        {
            std::vector<Creature*> citizens;
            GetCreatureListWithEntryInGrid(citizens, me, NPC_THERAMORE_CITIZEN_MALE, 5.f);
            GetCreatureListWithEntryInGrid(citizens, me, NPC_THERAMORE_CITIZEN_FEMALE, 5.f);
            if (citizens.empty())
                return;

            Creature* faithful = GetClosestCreatureWithEntry(me, NPC_THERAMORE_FAITHFUL, 5.f);

            if (param == 2)
            {
                if (citizens.empty())
                    return;

                for (Creature* citizen : citizens)
                    citizen->SetVisible(false);

                faithful->SetVisible(false);
            }
            else
            {
                if (faithful)
                {
                    for (Creature* citizen : citizens)
                        citizen->SetFacingToObject(faithful);

                    faithful->CastSpell(faithful, SPELL_HOLY_CHANNELING);

                    scheduler.Schedule(2s, [citizens](TaskContext context)
                    {
                        uint32 random = urand(0, citizens.size() - 1);
                        citizens[random]->AddAura(SPELL_RENEW, citizens[random]);
                        context.Repeat(5s, 8s);
                    });
                }
            }

        }

        void Reset() override
        {
            scheduler.CancelAll();
        }

        void UpdateAI(uint32 diff) override
        {
            if (!launched)
            {
                DoAction(1U);
                launched = true;
            }

            scheduler.Update(diff);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<event_theramore_faithfulAI>(creature);
    }
};

void AddSC_battle_for_theramore()
{
    new npc_jaina_theramore();
    new npc_pained();
    new npc_kalecgos_theramore();
    new event_theramore_training();
    new event_theramore_faithful();
}
