#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "KillRewarder.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "Custom/AI/CustomAI.h"
#include "battle_for_theramore.h"

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
            SPELL_CLEAVE                = 178532,
            SPELL_REFLECTIVE_SHIELD     = 189070,
            SPELL_WHIRLWIND             = 17207,
            SPELL_HAMMER_STUN           = 36138
        };

        void JustEngagedWith(Unit* who) override
        {
            scheduler
                .Schedule(5ms, [this](TaskContext hammer_stun)
                {
                    if (Unit* target = DoSelectCastingUnit(SPELL_HAMMER_STUN, 35.f))
					{
						me->CastStop();
						DoCast(target, SPELL_HAMMER_STUN);
                        hammer_stun.Repeat(25s, 40s);
					}
					else
					{
                        hammer_stun.Repeat(1s);
					}
                })
                .Schedule(3s, 8s, [this](TaskContext cleave)
                {
                    DoCast(SPELL_CLEAVE);
                    cleave.Repeat(10s, 15s);
                })
                .Schedule(8s, 15s, [this](TaskContext reflective_shield)
                {
                    DoCast(SPELL_REFLECTIVE_SHIELD);
                    reflective_shield.Repeat(30s);
                })
                .Schedule(12s, 23s, [this](TaskContext heroic_leap)
                {
                    DoCast(SPELL_HEROIC_LEAP);
                    heroic_leap.Repeat(2min);
                })
                .Schedule(15s, 25s, [this](TaskContext whirlwind)
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

        float GetDistance() override
        {
            return 35.f;
        }

        void JustEngagedWith(Unit* who) override
        {
            DoCast(SPELL_ARCANE_HASTE);

            scheduler
                .Schedule(1ms, [this](TaskContext arcane_blast)
                {
                    DoCastVictim(SPELL_ARCANE_BLAST);
                    arcane_blast.Repeat(2600ms);
                })
                .Schedule(3s, 5s, [this](TaskContext arcane_explosion)
                {
                    DoCast(SPELL_ARCANE_EXPLOSION);
                    arcane_explosion.Repeat(12s, 14s);
                })
                .Schedule(4s, 8s, [this](TaskContext arcane_missiles)
                {
                    if (Unit* victim = SelectTarget(SELECT_TARGET_RANDOM))
                        DoCast(victim, SPELL_ARCANE_MISSILES);
                    arcane_missiles.Repeat(8s, 12s);
                })
                .Schedule(6s, 10s, [this](TaskContext arcane_haste)
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
        npc_theramore_faithfulAI(Creature* creature) : npc_theramore_troopAI(creature, AI_Type::Distance), ascension(false)
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
            SPELL_PAIN_SUPPRESSION      = 69910,
            SPELL_PSYCHIC_SCREAM        = 65543,
        };

        bool ascension;

        void DamageTaken(Unit* /*attacker*/, uint32& /*damage*/) override
        {
            if (!ascension && HealthBelowPct(30))
            {
                ascension = true;

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
                        ascension = false;
                    });
            }
        }

        float GetDistance() override
        {
            return 20.f;
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
                .Schedule(12s, 14s, [this](TaskContext prayer_of_healing)
                {
                    DoCastSelf(SPELL_PRAYER_OF_HEALING);
                    prayer_of_healing.Repeat(25s);
                })
                .Schedule(1s, 3s, [this](TaskContext heal)
                {
                    if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 60))
                    {
                        me->CastStop();
                        DoCast(target, SPELL_HEAL);
                    }
                    heal.Repeat(5s, 8s);
                })
                .Schedule(1s, 3s, [this](TaskContext psychic_scream)
                {
                    if (EnemiesInRange(10.f) >= 2)
                        DoCastAOE(SPELL_PSYCHIC_SCREAM);
                    psychic_scream.Repeat(10s, 25s);
                })
                .Schedule(1s, 8s, [this](TaskContext flash_heal)
                {
                    if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 40))
                    {
                        me->CastStop();
                        DoCast(target, SPELL_FLASH_HEAL);
                    }
                    flash_heal.Repeat(2s);
                });
        }

        uint32 EnemiesInRange(float distance)
        {
            uint32 count = 0;
            for (ThreatReference const* ref : me->GetThreatManager().GetUnsortedThreatList())
                if (me->GetDistance2d(ref->GetVictim()) < distance)
                    ++count;
            return count;
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

    protected:

        uint32 EnemiesInRange(float distance)
        {
            uint32 count = 0;
            for (ThreatReference const* ref : me->GetThreatManager().GetUnsortedThreatList())
                if (me->GetDistance2d(ref->GetVictim()) < distance)
                    ++count;
            return count;
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

        enum Spells
        {
            SPELL_BLIZZARD      = 284968,
            SPELL_BLINK         = 284877,
            SPELL_CONE_OF_COLD  = 292294,
            SPELL_EBONBOLT      = 284752,
            SPELL_FLURRY        = 284858,
            SPELL_FROST_NOVA    = 284879,
            SPELL_FROSTBOLT     = 284703,
            SPELL_GLACIAL_SPIKE = 284840,
            SPELL_ICE_BLOCK     = 290049,
            SPELL_ICE_BARRIER   = 284882
        };

        void JustEngagedWith(Unit* /*who*/) override
        {
            DoCastSelf(SPELL_ICE_BARRIER);

            scheduler
                .Schedule(5s, 8s, [this](TaskContext blizzard)
                {
                    if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM))
                        DoCast(target, SPELL_BLIZZARD);
                    blizzard.Repeat(10s, 15s);
                })
                .Schedule(13s, 18s, [this](TaskContext cone_of_cold)
                {
                    if (EnemiesInRange(12.0f) < 3)
                    {
                        me->CastStop();
                        DoCastSelf(SPELL_CONE_OF_COLD);
                        cone_of_cold.Repeat(5s, 8s);
                    }
                    else
                        cone_of_cold.Repeat(2s);
                })
                .Schedule(2s, 5s, [this](TaskContext ebonbolt)
                {
                    DoCastVictim(SPELL_EBONBOLT);
                    ebonbolt.Repeat(2s, 5s);
                })
                .Schedule(12s, 15s, [this](TaskContext flurry)
                {
                    if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM))
                    {
                        me->CastStop();
                        DoCast(target, SPELL_FLURRY);
                    }
                    flurry.Repeat(12s, 14s);
                })
                .Schedule(2s, 3s, [this](TaskContext frost_nova)
                {
                    if (EnemiesInRange(12.0f) < 4)
                    {
                        me->CastStop();
                        DoCastSelf(SPELL_BLINK);
                        DoCastSelf(SPELL_FROST_NOVA, true);
                        frost_nova.Repeat(5s, 8s);
                    }
                    else
                        frost_nova.Repeat(5s);
                })
                .Schedule(1ms, [this](TaskContext frostbolt)
                {
                    DoCastVictim(SPELL_FROSTBOLT);
                    frostbolt.Repeat(2s);
                })
                .Schedule(20s, 25s, [this](TaskContext glacial_spike)
                {
                    if (Unit* target = SelectTarget(SELECT_TARGET_MAXDISTANCE))
                        DoCast(target, SPELL_GLACIAL_SPIKE);
                    glacial_spike.Repeat(30s, 45s);
                });
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
            immolateInfo = sSpellMgr->AssertSpellInfo(SPELL_IMMOLATE, DIFFICULTY_NONE);
            corruptionInfo = sSpellMgr->AssertSpellInfo(SPELL_CORRUPTION, DIFFICULTY_NONE);
        }

        enum Spells
        {
            SPELL_DRAIN_LIFE        = 149992,
            SPELL_CONFLAGRATE       = 295418,
            SPELL_BACKDRAFT         = 295419,
            SPELL_BACKDRAFT_AURA    = 196406,
            SPELL_CHAOS_BOLT        = 295420,
            SPELL_IMMOLATE          = 295425,
            SPELL_INCINERATE        = 295438,
            SPELL_MORTAL_COIL       = 295459,
            SPELL_SUMMON_FELHUNTER  = 285232,
            SPELL_CORRUPTION        = 285140,
        };

        const SpellInfo* immolateInfo;
        const SpellInfo* corruptionInfo;

        float GetDistance() override
        {
            return 30.f;
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            if (roll_chance_i(30))
                DoCastSelf(SPELL_SUMMON_FELHUNTER);

            scheduler
                .Schedule(5s, 8s, [this](TaskContext drain_life)
                {
                    if (HealthBelowPct(20))
                    {
                        if (Unit* target = SelectTarget(SELECT_TARGET_MAXDISTANCE, 0))
                        {
                            me->CastStop();
                            DoCast(target, SPELL_DRAIN_LIFE);
                            drain_life.Repeat(8s, 15s);
                        }
                    }
                    else
                        drain_life.Repeat(1s);
                })
                .Schedule(2s, 6s, [this](TaskContext conflagrate)
                {
                    if (Unit* target = SelectTarget(SELECT_TARGET_MAXDISTANCE, 0))
                    {
                        me->CastStop();
                        DoCast(target, SPELL_CONFLAGRATE);
                        if (Aura* aura = me->GetAura(SPELL_BACKDRAFT_AURA))
                        {
                            if (aura->GetStackAmount() < 2)
                                DoCastSelf(SPELL_BACKDRAFT, true);
                        }
                    }
                    else
                        conflagrate.Repeat(1s, 3s);
                })
                .Schedule(3s, 5s, [this](TaskContext chaos_bolt)
                {
                    DoCastVictim(SPELL_CHAOS_BOLT);
                    chaos_bolt.Repeat(5s, 8s);
                })
                .Schedule(1ms, [this](TaskContext immolate)
                {
                    if (Unit* target = DoFindEnemyMissingDot(immolateInfo))
                    {
                        me->CastStop();
                        DoCast(target, SPELL_IMMOLATE);
                    }
                    immolate.Repeat(2s);
                })
                .Schedule(1ms, [this](TaskContext corruption)
                {
                    if (Unit* target = DoFindEnemyMissingDot(corruptionInfo))
                    {
                        me->CastStop();
                        DoCast(target, SPELL_CORRUPTION);
                    }
                    corruption.Repeat(2s);
                })
                .Schedule(1ms, [this](TaskContext incinerate)
                {
                    DoCastVictim(SPELL_INCINERATE);
                    incinerate.Repeat(2s);
                })
                .Schedule(12s, 14s, [this](TaskContext mortal_coil)
                {
                    if (HealthBelowPct(15))
                    {
                        if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0))
                        {
                            me->CastStop();
                            DoCast(target, SPELL_MORTAL_COIL);
                            mortal_coil.Repeat(25s, 45s);
                        }
                    }
                    else
                        mortal_coil.Repeat(1s);
                });
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<npc_roknah_felcasterAI>(creature);
    }
};

// Blizzard - 284968
// AreaTriggerID - 15411
class at_blizzard_theramore : public AreaTriggerEntityScript
{
    public:
    at_blizzard_theramore() : AreaTriggerEntityScript("at_blizzard_theramore")
    {
    }

    struct at_blizzard_theramoreAI : AreaTriggerAI
    {
        at_blizzard_theramoreAI(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger)
        {
            timeInterval = 1000;
        }

        int32 timeInterval;

        enum Spells
        {
            SPELL_BLIZZARD_DAMAGE = 335953
        };

        void OnUpdate(uint32 diff) override
        {
            Unit* caster = at->GetCaster();

            if (!caster)
                return;

            if (caster->GetTypeId() != TYPEID_PLAYER)
                return;

            timeInterval += diff;
            if (timeInterval < 1000)
                return;

            if (TempSummon* tempSumm = caster->SummonCreature(WORLD_TRIGGER, at->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, 200))
            {
                tempSumm->SetFaction(caster->GetFaction());
                tempSumm->SetOwnerGUID(caster->GetGUID());
                PhasingHandler::InheritPhaseShift(tempSumm, caster);
                caster->CastSpell(tempSumm, SPELL_BLIZZARD_DAMAGE, true);
            }

            timeInterval -= 1000;
        }
    };

    AreaTriggerAI* GetAI(AreaTrigger* areatrigger) const override
    {
        return new at_blizzard_theramoreAI(areatrigger);
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

    new at_blizzard_theramore();
}
