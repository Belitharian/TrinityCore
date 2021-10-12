#include "GameObject.h"
#include "InstanceScript.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "PassiveAI.h"
#include "Scenario.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Custom/AI/CustomAI.h"
#include "battle_for_theramore.h"

class npc_jaina_theramore : public CreatureScript
{
    public:
    npc_jaina_theramore() : CreatureScript("npc_jaina_theramore")
    {
    }

    struct npc_jaina_theramoreAI : public CustomAI
    {
        npc_jaina_theramoreAI(Creature* creature) : CustomAI(creature)
        {
            Initialize();
        }

        enum Spells
        {
            SPELL_FROST_BARRIER     = 69787,
            SPELL_ICE_LANCE_VOLLEY  = 70464,
            SPELL_ICE_SHARD         = 290621,
            SPELL_RING_OF_FROST     = 285459,
            SPELL_GLACIAL_RAY       = 288345
        };

        void Initialize()
        {
            instance = me->GetInstanceScript();
            healthLow = false;
        }

        InstanceScript* instance;
        bool healthLow;

        void Reset() override
        {
            Initialize();

            me->SetReactState(REACT_AGGRESSIVE);
            me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
        }

        void OnSuccessfulSpellCast(SpellInfo const* spell) override
        {
            switch (spell->Id)
            {
                case SPELL_GLACIAL_RAY:
                {
                    int32 duration = spell->GetDuration();

                    me->StopMoving();
                    me->SetReactState(REACT_PASSIVE);
                    me->GetMotionMaster()->Clear();
                    me->GetMotionMaster()->MoveRotate(MOVEMENT_INFO_POINT_NONE, duration, urand(0, 1) ? ROTATE_DIRECTION_LEFT : ROTATE_DIRECTION_RIGHT);

                    scheduler.Schedule(Milliseconds(duration), [this](TaskContext glacial_ray)
                    {
                        me->SetReactState(REACT_AGGRESSIVE);
                        me->GetMotionMaster()->Clear();
                        me->GetMotionMaster()->MoveChase(me->GetVictim());
                    });

                    break;
                }
            }
        }

        void DamageTaken(Unit* attacker, uint32& damage) override
        {
            if (HealthBelowPct(20))
            {
                if (!me->HasAura(SPELL_FROST_BARRIER))
                {
                    me->CastStop();
                    DoCast(SPELL_FROST_BARRIER);
                }
            }
            else if (!healthLow && HealthBelowPct(30))
            {
                healthLow = true;

                scheduler.Schedule(5ms, [attacker, this](TaskContext ice_lance)
                {
                    CastSpellExtraArgs args(SPELLVALUE_BASE_POINT0, 360000);
                    me->CastStop();
                    me->CastSpell(attacker, SPELL_ICE_LANCE_VOLLEY, args);
                    ice_lance.Repeat(8s, 14s);
                });
            }
        }

        void JustEngagedWith(Unit* who) override
        {
            scheduler
                .Schedule(5ms, [this](TaskContext ice_shard)
                {
                    DoCastVictim(SPELL_ICE_SHARD);
                    ice_shard.Repeat(2s);
                })
                .Schedule(8s, [this](TaskContext ice_shard)
                {
                    DoCast(SPELL_GLACIAL_RAY);
                    ice_shard.Repeat(2min);
                })
                .Schedule(15s, [this](TaskContext ring_of_frost)
                {
                    DoCast(SPELL_RING_OF_FROST);
                    ring_of_frost.Repeat(1min);
                });
        }

        void MovementInform(uint32 type, uint32 id) override
        {
            if (type == EFFECT_MOTION_TYPE || type == POINT_MOTION_TYPE)
            {
                switch (id)
                {
                    case MOVEMENT_INFO_POINT_01:
                        instance->DoSendScenarioEvent(EVENT_THE_COUNCIL);
                        break;
                    default:
                        break;
                }
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
                            break;
                        case BFTPhases::ALittleHelp:
                            instance->DoSendScenarioEvent(EVENT_A_LITTLE_HELP);
                            break;
                        case BFTPhases::TheBattle:
                            instance->DoSendScenarioEvent(EVENT_RETRIEVE_JAINA);
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

class npc_archmage_tervosh : public CreatureScript
{
    public:
    npc_archmage_tervosh() : CreatureScript("npc_archmage_tervosh")
    {
    }

    struct npc_archmage_tervoshAI : public CustomAI
    {
        npc_archmage_tervoshAI(Creature* creature) : CustomAI(creature)
        {
        }

        enum Spells
        {
            SPELL_FIREBALL          = 358226,
            SPELL_FLAMESTRIKE       = 330347,
            SPELL_BLAZING_BARRIER   = 255714,
            SPELL_SCORCH            = 358238,
            SPELL_CONFLAGRATION     = 226757
        };

        void MovementInform(uint32 type, uint32 id) override
        {
            if (type == EFFECT_MOTION_TYPE || type == POINT_MOTION_TYPE)
            {
                printf("type: %u ; id: %u\n", type, id);

                switch (id)
                {
                    case MOVEMENT_INFO_POINT_01:
                        me->SetFacingTo(0.70f);
                        break;
                    case MOVEMENT_INFO_POINT_02:
                        me->SetFacingTo(2.14f);
                        me->SetVisible(false);
                        break;
                    case MOVEMENT_INFO_POINT_03:
                        me->SetFacingTo(4.05f);
                        me->SetEmoteState(EMOTE_STATE_READ);
                        break;
                    default:
                        break;
                }
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
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<npc_archmage_tervoshAI>(creature);
    }
};

class npc_kinndy_sparkshine : public CreatureScript
{
    public:
    npc_kinndy_sparkshine() : CreatureScript("npc_kinndy_sparkshine")
    {
    }

    struct npc_kinndy_sparkshineAI : public CustomAI
    {
        npc_kinndy_sparkshineAI(Creature* creature) : CustomAI(creature)
        {
        }

        enum Spells
        {
            SPELL_ARCANE_BOLT   = 154235,
            SPELL_SUPERNOVA     = 157980,
        };

        void MovementInform(uint32 type, uint32 id) override
        {
            if (type == EFFECT_MOTION_TYPE || type == POINT_MOTION_TYPE)
            {
                switch (id)
                {
                    case MOVEMENT_INFO_POINT_01:
                        me->SetFacingTo(4.62f);
                        me->SetVisible(false);
                        break;
                    case MOVEMENT_INFO_POINT_02:
                        me->SetFacingTo(1.24f);
                        break;
                    default:
                        break;
                }
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
        return GetBattleForTheramoreAI<npc_kinndy_sparkshineAI>(creature);
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
            if (type == EFFECT_MOTION_TYPE || type == POINT_MOTION_TYPE)
            {
                switch (id)
                {
                    case MOVEMENT_INFO_POINT_02:
                        me->SetVisible(false);
                        instance->DoSendScenarioEvent(EVENT_THE_UNKNOWN_TAUREN);
                        break;
                    default:
                        break;
                }
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
            if (type == EFFECT_MOTION_TYPE || type == POINT_MOTION_TYPE)
            {
                switch (id)
                {
                    case MOVEMENT_INFO_POINT_01:
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
                    for (uint8 i = 0; i < 2; i++)
                    {
                        footmen[i]->SetTarget(ObjectGuid::Empty);
                        footmen[i]->SetEmoteState(EMOTE_STATE_NONE);
                        footmen[i]->SetFacingTo(2.60f);
                        footmen[i]->SetRegenerateHealth(false);
                        footmen[i]->SetHealth(footmen[0]->GetMaxHealth());
                        footmen[i]->SetReactState(REACT_AGGRESSIVE);
                    }

                    faithful->SetTarget(ObjectGuid::Empty);
                    faithful->SetFacingTo(2.60f);
                    faithful->SetReactState(REACT_AGGRESSIVE);
                    faithful->SetPower(POWER_MANA, faithful->GetMaxPower(POWER_MANA));

                    arcanist->CastStop();
                    arcanist->CombatStop();
                    arcanist->SetTarget(ObjectGuid::Empty);
                    arcanist->SetFacingTo(2.60f);
                    arcanist->SetReactState(REACT_AGGRESSIVE);
                    arcanist->SetPower(POWER_MANA, arcanist->GetMaxPower(POWER_MANA));

                    training->SetVisible(false);

                    scheduler.CancelAll();
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
                faithful->RemoveAllAuras();
                faithful->SetFacingTo(0.66f);

                scheduler.CancelAll();
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
    new npc_archmage_tervosh();
    new npc_kinndy_sparkshine();
    new npc_pained();
    new npc_kalecgos_theramore();
    new event_theramore_training();
    new event_theramore_faithful();
}
