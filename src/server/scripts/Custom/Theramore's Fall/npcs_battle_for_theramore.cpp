#include "ScriptMgr.h"
#include "ObjectAccessor.h"
#include "MotionMaster.h"
#include "GameObject.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "InstanceScript.h"
#include "SpellInfo.h"
#include "Player.h"
#include "KillRewarder.h"
#include "battle_for_theramore.h"
#include "Custom/AI/CustomAI.h"

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

class npc_unmanned_tank : public CreatureScript
{
    public:
    npc_unmanned_tank() : CreatureScript("npc_unmanned_tank")
    {
    }

    struct npc_unmanned_tankAI : public ScriptedAI
    {
        npc_unmanned_tankAI(Creature* creature) : ScriptedAI(creature)
        {
        }

        void SpellHit(Unit* /*caster*/, SpellInfo const* spellInfo) override
        {
            if (spellInfo->Id != SPELL_REPAIR)
                return;

            me->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<npc_unmanned_tankAI>(creature);
    }
};

struct npc_theramore_troopAI : public CustomAI
{
    npc_theramore_troopAI(Creature* creature, AI_Type type) : CustomAI(creature, type), emoteReceived(false)
    {
        instance = creature->GetInstanceScript();
    }

    enum Misc
    {
        NPC_THERAMORE_TROOPS_CREDIT = 500011
    };

    InstanceScript* instance;
    bool emoteReceived;

    void ReceiveEmote(Player* player, uint32 emoteId) override
    {
        BFTPhases phase = (BFTPhases)instance->GetData(DATA_SCENARIO_PHASE);
        if (phase != BFTPhases::Preparation)
            return;

        if (!emoteReceived && emoteId == TEXT_EMOTE_FORTHEALLIANCE)
        {
            me->HandleEmoteCommand(EMOTE_ONESHOT_CHEER_FORTHEALLIANCE);
            KillRewarder(player, me, false).Reward(NPC_THERAMORE_TROOPS_CREDIT);
            emoteReceived = true;
        }
    }
};

class npc_theramore_footman : public CreatureScript
{
    public:
    npc_theramore_footman() : CreatureScript("npc_theramore_footman")
    {
    }

    struct npc_theramore_footmanAI : public npc_theramore_troopAI
    {
        npc_theramore_footmanAI(Creature* creature) : npc_theramore_troopAI(creature, AI_Type::Melee)
        {
        }


    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<npc_theramore_footmanAI>(creature);
    }
};

class npc_theramore_arcanist : public CreatureScript
{
    public:
    npc_theramore_arcanist() : CreatureScript("npc_theramore_arcanist")
    {
    }

    struct npc_theramore_arcanistAI : public npc_theramore_troopAI
    {
        npc_theramore_arcanistAI(Creature* creature) : npc_theramore_troopAI(creature, AI_Type::Distance)
        {
        }


    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<npc_theramore_arcanistAI>(creature);
    }
};

class npc_theramore_faithful : public CreatureScript
{
    public:
    npc_theramore_faithful() : CreatureScript("npc_theramore_faithful")
    {
    }

    struct npc_theramore_faithfulAI : public npc_theramore_troopAI
    {
        npc_theramore_faithfulAI(Creature* creature) : npc_theramore_troopAI(creature, AI_Type::Distance)
        {
        }


    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<npc_theramore_faithfulAI>(creature);
    }
};

struct npc_theramore_hordeAI : public CustomAI
{
    npc_theramore_hordeAI(Creature* creature, AI_Type type) : CustomAI(creature, type)
    {
        instance = creature->GetInstanceScript();
        args.AddSpellBP0(10000);
    }

    enum Misc
    {
        SPELL_ARCANIC_BARRIER = 301407
    };

    InstanceScript* instance;
    CastSpellExtraArgs args;

    void MovementInform(uint32 /*type*/, uint32 id) override
    {
        switch (id)
        {
            case 0:
                me->CastSpell(me, SPELL_ARCANIC_BARRIER, args);
                me->DespawnOrUnsummon(2s);
                break;
            default:
                break;
        }
    }
};

class npc_roknah_hag : public CreatureScript
{
    public:
    npc_roknah_hag() : CreatureScript("npc_roknah_hag")
    {
    }

    struct npc_roknah_hagAI : public npc_theramore_hordeAI
    {
        npc_roknah_hagAI(Creature* creature) : npc_theramore_hordeAI(creature, AI_Type::Distance)
        {
        }


    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<npc_roknah_hagAI>(creature);
    }
};

class npc_roknah_grunt : public CreatureScript
{
    public:
    npc_roknah_grunt() : CreatureScript("npc_roknah_grunt")
    {
    }

    struct npc_roknah_gruntAI : public npc_theramore_hordeAI
    {
        npc_roknah_gruntAI(Creature* creature) : npc_theramore_hordeAI(creature, AI_Type::Melee)
        {
        }


    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<npc_roknah_gruntAI>(creature);
    }
};

class npc_roknah_loasinger : public CreatureScript
{
    public:
    npc_roknah_loasinger() : CreatureScript("npc_roknah_loasinger")
    {
    }

    struct npc_roknah_loasingerAI : public npc_theramore_hordeAI
    {
        npc_roknah_loasingerAI(Creature* creature) : npc_theramore_hordeAI(creature, AI_Type::Distance)
        {
        }


    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<npc_roknah_loasingerAI>(creature);
    }
};

class npc_roknah_felcaster : public CreatureScript
{
    public:
    npc_roknah_felcaster() : CreatureScript("npc_roknah_felcaster")
    {
    }

    struct npc_roknah_felcasterAI : public npc_theramore_hordeAI
    {
        npc_roknah_felcasterAI(Creature* creature) : npc_theramore_hordeAI(creature, AI_Type::Distance)
        {
        }


    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<npc_roknah_felcasterAI>(creature);
    }
};

void AddSC_npcs_battle_for_theramore()
{
    new npc_theramore_citizen();
    new npc_rhonin();
    new npc_thader_windermere();
    new npc_unmanned_tank();
    new npc_theramore_footman();
    new npc_theramore_faithful();
    new npc_theramore_arcanist();
    new npc_roknah_hag();
    new npc_roknah_grunt();
    new npc_roknah_loasinger();
    new npc_roknah_felcaster();
}
