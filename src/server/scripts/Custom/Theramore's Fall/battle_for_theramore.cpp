#include "boost/range/algorithm/reverse.hpp"
#include "ScriptMgr.h"
#include "ObjectAccessor.h"
#include "MotionMaster.h"
#include "GameObject.h"
#include "Scenario.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "SpellHistory.h"
#include "InstanceScript.h"
#include "KillRewarder.h"
#include "battle_for_theramore.h"
#include "Custom/AI/CustomAI.h"

class npc_jaina_theramore : public CreatureScript
{
    public:
    npc_jaina_theramore() : CreatureScript("npc_jaina_theramore")
    {
    }

    struct npc_jaina_theramoreAI : public ScriptedAI
    {
        npc_jaina_theramoreAI(Creature* creature) : ScriptedAI(creature)
        {
            Initialize();
        }

        void Initialize()
        {
            instance = me->GetInstanceScript();
        }

        InstanceScript* instance;

        void Reset() override
        {
            Initialize();
        }

        void MovementInform(uint32 /*type*/, uint32 id) override
        {
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

        void MovementInform(uint32 /*type*/, uint32 id) override
        {
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

class npc_theramore_citizen : public CreatureScript
{
    public:
    npc_theramore_citizen() : CreatureScript("npc_theramore_citizen")
    {
    }

    struct npc_theramore_citizenAI : public ScriptedAI
    {
        enum Misc
        {
            GOSSIP_MENU_DEFAULT             = 65000,
            NPC_THERAMORE_CITIZEN_CREDIT    = 500005
        };

        npc_theramore_citizenAI(Creature* creature) : ScriptedAI(creature)
        {
            instance = creature->GetInstanceScript();
        }

        TaskScheduler scheduler;
        InstanceScript* instance;

        void MovementInform(uint32 /*type*/, uint32 id) override
        {
            if (id == 0)
            {
                me->SetVisible(false);
            }
        }

        bool GossipHello(Player* player) override
        {
            player->PrepareGossipMenu(me, GOSSIP_MENU_DEFAULT, true);
            player->SendPreparedGossip(me);
            return true;
        }

        bool GossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
        {
            ClearGossipMenuFor(player);

            switch (gossipListId)
            {
                case 0:
                {
                    me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                    me->SetEmoteState(EMOTE_STATE_NONE);
                    scheduler.Schedule(5ms, [this, player](TaskContext context)
                    {
                        switch (context.GetRepeatCounter())
                        {
                            case 0:
                                me->SetTarget(player->GetGUID());
                                me->SetWalk(false);
                                context.Repeat(1s);
                                break;
                            case 1:
                                Talk(0);
                                context.Repeat(5s);
                                break;
                            case 2:
                                me->SetTarget(ObjectGuid::Empty);
                                if (Creature* stalker = GetClosestCreatureWithEntry(me, NPC_INVISIBLE_STALKER, 35.f))
                                    me->GetMotionMaster()->MovePoint(0, stalker->GetPosition());
                                context.Repeat(1s);
                                break;
                            case 3:
                                KillRewarder(player, me, false).Reward(NPC_THERAMORE_CITIZEN_CREDIT);
                                break;
                            default:
                                break;
                        }
                    });
                    break;
                }
            }

            CloseGossipMenuFor(player);
            return true;
        }

        void Reset() override
        {
            me->SetVisible(true);
            scheduler.CancelAll();
        }

        void UpdateAI(uint32 diff) override
        {
            scheduler.Update(diff);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<npc_theramore_citizenAI>(creature);
    }
};

class npc_rhonin : public CreatureScript
{
    public:
    npc_rhonin() : CreatureScript("npc_rhonin")
    {
    }

    struct npc_rhoninAI : public CustomAI
    {
        enum Misc
        {
            GOSSIP_MENU_DEFAULT = 65001,
        };

        npc_rhoninAI(Creature* creature) : CustomAI(creature)
        {
        }

        bool GossipHello(Player* player) override
        {
            player->PrepareGossipMenu(me, GOSSIP_MENU_DEFAULT, true);
            player->SendPreparedGossip(me);
            return true;
        }

        bool GossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
        {
            ClearGossipMenuFor(player);

            switch (gossipListId)
            {
                case 0:
                    KillRewarder(player, me, false).Reward(me->GetEntry());
                    break;
            }

            CloseGossipMenuFor(player);
            return true;
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<npc_rhoninAI>(creature);
    }
};

class npc_thader_windermere : public CreatureScript
{
    public:
    npc_thader_windermere() : CreatureScript("npc_thader_windermere")
    {
    }

    struct npc_thader_windermereAI : public CustomAI
    {
        enum Misc
        {
            GOSSIP_MENU_DEFAULT = 65002,
        };

        npc_thader_windermereAI(Creature* creature) : CustomAI(creature)
        {
        }

        bool GossipHello(Player* player) override
        {
            player->PrepareGossipMenu(me, GOSSIP_MENU_DEFAULT, true);
            player->SendPreparedGossip(me);
            return true;
        }

        bool GossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
        {
            ClearGossipMenuFor(player);

            switch (gossipListId)
            {
                case 0:
                    KillRewarder(player, me, false).Reward(me->GetEntry());
                    break;
            }

            CloseGossipMenuFor(player);
            return true;
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<npc_thader_windermereAI>(creature);
    }
};

class event_theramore_training : public CreatureScript
{
    public:
    event_theramore_training() : CreatureScript("event_theramore_training")
    {
    }

    struct event_theramore_trainingAI : public ScriptedAI
    {
        event_theramore_trainingAI(Creature* creature) : ScriptedAI(creature)
        {
            instance = creature->GetInstanceScript();
        }

        TaskScheduler scheduler;
        InstanceScript* instance;

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
                            faithful->SetTarget(victim->GetGUID());
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
        event_theramore_faithfulAI(Creature* creature) : ScriptedAI(creature)
        {
            instance = creature->GetInstanceScript();
        }

        TaskScheduler scheduler;
        InstanceScript* instance;

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
    new npc_theramore_citizen();
    new npc_rhonin();
    new npc_thader_windermere();
    new event_theramore_training();
    new event_theramore_faithful();
}
