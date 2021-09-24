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

        void MovementInform(uint32 type, uint32 id) override
        {
            if (type != POINT_MOTION_TYPE)
                return;

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
            instance = creature->GetInstanceScript();
        }

        InstanceScript* instance;

        void SetData(uint32 id, uint32 value) override
        {
            if (id == DATA_SCENARIO_PHASE && value == (uint32)BFTPhases::Battle)
                instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, me);
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
                    me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
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
                    me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
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

class npc_arcanic_image : public CreatureScript
{
    public:
    npc_arcanic_image() : CreatureScript("npc_arcanic_image")
    {
    }

    struct npc_arcanic_imageAI : public CustomAI
    {
        enum Spells
        {
            SPELL_BROADSIDE = 288214,
        };

        npc_arcanic_imageAI(Creature* creature) : CustomAI(creature)
        {
        }

        void JustEngagedWith(Unit* who) override
        {
            me->SetEmoteState(EMOTE_STATE_STAND_SETEMOTESTATE);

            scheduler.Schedule(5ms, [this](TaskContext broadside)
            {
                DoCastVictim(SPELL_BROADSIDE);
                broadside.Repeat(8s);
            });
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<npc_arcanic_imageAI>(creature);
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

        enum Spells
        {
            SPELL_HEROIC_LEAP           = 279008,
            SPELL_SHIELD_CHARGE         = 277911,
            SPELL_CLEAVE                = 178532,
            SPELL_REFLECTIVE_SHIELD     = 189070,
            SPELL_WHIRLWIND             = 17207,
            SPELL_PUMMEL                = 6552
        };

        void JustEngagedWith(Unit* who) override
        {
            DoCast(SPELL_SHIELD_CHARGE);

            scheduler
                .Schedule(5ms, [this](TaskContext pummel)
                {
                    if (Unit* target = DoSelectCastingUnit(SPELL_PUMMEL, 35.f))
					{
						me->InterruptNonMeleeSpells(true);
						DoCast(target, SPELL_PUMMEL);
                        pummel.Repeat(25s, 40s);
					}
					else
					{
                        pummel.Repeat(1s);
					}
                })
                .Schedule(8s, [this](TaskContext cleave)
                {
                    DoCast(SPELL_CLEAVE);
                    cleave.Repeat(10s, 15s);
                })
                .Schedule(15s, [this](TaskContext reflective_shield)
                {
                    DoCast(SPELL_REFLECTIVE_SHIELD);
                    reflective_shield.Repeat(30s);
                })
                .Schedule(23s, [this](TaskContext heroic_leap)
                {
                    DoCast(SPELL_HEROIC_LEAP);
                    heroic_leap.Repeat(25s, 32s);
                })
                .Schedule(25s, [this](TaskContext whirlwind)
                {
                    DoCast(SPELL_WHIRLWIND);
                    whirlwind.Repeat(45s);
                });
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

        enum Spells
        {
            SPELL_ARCANE_BLAST      = 270543,
            SPELL_ARCANE_EXPLOSION  = 277012,
            SPELL_ARCANE_MISSILES   = 191293,
            SPELL_ARCANE_HASTE      = 47791,
        };

        void JustEngagedWith(Unit* who) override
        {
            DoCast(SPELL_ARCANE_HASTE);

            scheduler
                .Schedule(1ms, [this](TaskContext arcane_blast)
                {
                    DoCastVictim(SPELL_ARCANE_BLAST);
                    arcane_blast.Repeat(2600ms);
                })
                .Schedule(5s, [this](TaskContext arcane_explosion)
                {
                    DoCast(SPELL_ARCANE_EXPLOSION);
                    arcane_explosion.Repeat(12s, 14s);
                })
                .Schedule(8s, [this](TaskContext arcane_missiles)
                {
                    if (Unit* victim = SelectTarget(SELECT_TARGET_RANDOM))
                        DoCast(victim, SPELL_ARCANE_MISSILES);
                    arcane_missiles.Repeat(8s, 12s);
                })
                .Schedule(10s, [this](TaskContext arcane_haste)
                {
                    DoCast(SPELL_ARCANE_HASTE);
                    arcane_haste.Repeat(30s);
                });
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
        npc_theramore_faithfulAI(Creature* creature) : npc_theramore_troopAI(creature, AI_Type::Distance), _ascension(false)
        {
        }

        enum Spells
        {
            SPELL_SMITE                 = 332705,
            SPELL_HEAL                  = 332706,
            SPELL_FLASH_HEAL            = 314655,
            SPELL_RENEW                 = 294342,
            SPELL_PRAYER_OF_HEALING     = 220118,
            SPELL_POWER_WORD_SHIELD     = 318158,
            SPELL_PAIN_SUPPRESSION      = 69910
        };

        bool _ascension;

        void DamageTaken(Unit* /*attacker*/, uint32& /*damage*/) override
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
                .Schedule(1ms, [this](TaskContext smite)
                {
                    DoCastVictim(SPELL_SMITE);
                    smite.Repeat(2s);
                })
                .Schedule(14s, [this](TaskContext prayer_of_healing)
                {
                    DoCastSelf(SPELL_PRAYER_OF_HEALING);
                    prayer_of_healing.Repeat(25s);
                })
                .Schedule(3s, [this](TaskContext heal)
                {
                    if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 60))
                    {
                        me->InterruptNonMeleeSpells(true);
                        DoCast(target, SPELL_HEAL);
                    }
                    heal.Repeat(10s, 18s);
                })
                .Schedule(8s, [this](TaskContext flash_heal)
                {
                    if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 40))
                    {
                        me->InterruptNonMeleeSpells(true);
                        DoCast(target, SPELL_FLASH_HEAL);
                    }
                    flash_heal.Repeat(2s);
                });
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
    new npc_arcanic_image();
    new npc_unmanned_tank();
    new npc_theramore_footman();
    new npc_theramore_faithful();
    new npc_theramore_arcanist();
    new npc_roknah_hag();
    new npc_roknah_grunt();
    new npc_roknah_loasinger();
    new npc_roknah_felcaster();
}
