#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "GameObject.h"
#include "GridNotifiersImpl.h"
#include "InstanceScript.h"
#include "KillRewarder.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "PassiveAI.h"
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

            if (id == MOVEMENT_INFO_POINT_01)
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
                    KillRewarder(player, me, false).Reward(NPC_THERAMORE_CITIZEN_CREDIT);

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
                                    me->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_01, stalker->GetPosition());
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

class npc_wounded_theramore_troop : public CreatureScript
{
    public:
    npc_wounded_theramore_troop() : CreatureScript("npc_wounded_theramore_troop")
    {
    }

    struct npc_wounded_theramore_troopAI : public ScriptedAI
    {
        npc_wounded_theramore_troopAI(Creature* creature) : ScriptedAI(creature),
            preventClick(false)
        {
        }

        enum Spells
        {
            SPELL_TELEPORT_TROOP = 69074
        };

        bool preventClick;

        void SpellHit(Unit* caster, SpellInfo const* spellInfo) override
        {
            if (spellInfo->Id != SPELL_TELEPORT_TROOP)
                return;

            if (preventClick)
                return;

            if (Player* player = caster->ToPlayer())
            {
                KillRewarder(player, me, false).Reward(NPC_THERAMORE_WOUNDED_TROOP);
            }

            me->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
            me->DespawnOrUnsummon();

            preventClick = true;
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_wounded_theramore_troopAI(creature);
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
        NPC_THERAMORE_TROOPS_CREDIT = 500011,
    };

    InstanceScript* instance;
    bool emoteReceived;

    void ReceiveEmote(Player* player, uint32 emoteId) override
    {
        BFTPhases phase = (BFTPhases)instance->GetData(DATA_SCENARIO_PHASE);
        if (phase == BFTPhases::Preparation || phase == BFTPhases::Preparation_Rhonin)
        {
            for (uint8 i = 0; i < 5; i++)
            {
                KillRewarder(player, me, false).Reward(NPC_THERAMORE_TROOPS_CREDIT);
            }

            if (!emoteReceived && emoteId == TEXT_EMOTE_FORTHEALLIANCE)
            {
                me->HandleEmoteCommand(EMOTE_ONESHOT_CHEER_FORTHEALLIANCE);
                KillRewarder(player, me, false).Reward(NPC_THERAMORE_TROOPS_CREDIT);
                emoteReceived = true;
            }
        }
    }

    protected:

    uint32 EnemiesInRange(float distance)
    {
        uint32 count = 0;
        for (ThreatReference const* ref : me->GetThreatManager().GetUnsortedThreatList())
            if (me->IsWithinDist(ref->GetVictim(), distance))
                ++count;
        return count;
    }
};

class npc_rhonin : public CreatureScript
{
    public:
    npc_rhonin() : CreatureScript("npc_rhonin")
    {
    }

    struct npc_rhoninAI : public npc_theramore_troopAI
    {
        enum Misc
        {
            GOSSIP_MENU_DEFAULT         = 65001,
        };

        enum Spells
        {
            SPELL_PRISMATIC_BARRIER     = 235450,
            SPELL_ARCANE_PROJECTILES    = 166995,
            SPELL_ARCANE_EXPLOSION      = 210479,
            SPELL_ARCANE_BLAST          = 291316,
            SPELL_ARCANE_BARRAGE        = 291318,
            SPELL_EVOCATION             = 243070,
            SPELL_TIME_WARP             = 342242,
            SPELL_ARCANE_CAST_INSTANT   = 135030,
        };

        npc_rhoninAI(Creature* creature) : npc_theramore_troopAI(creature, AI_Type::Distance), arcaneCharges(0)
        {
            instance = creature->GetInstanceScript();
        }

        InstanceScript* instance;
        uint8 arcaneCharges;

        float GetDistance() override
        {
            return 15.f;
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
                    instance->DoCastSpellOnPlayers(SPELL_RUNIC_SHIELD);
                    DoCast(SPELL_ARCANE_CAST_INSTANT);
                    KillRewarder(player, me, false).Reward(me->GetEntry());
                    break;
            }

            CloseGossipMenuFor(player);
            return true;
        }

        void OnSuccessfulSpellCast(SpellInfo const* spell) override
        {
            switch (spell->Id)
            {
                case SPELL_ARCANE_BLAST:
                    arcaneCharges++;
                    if (roll_chance_i(20))
                    {
                        me->CastStop();
                        me->AddAura(SPELL_TIME_WARP, me);
                        DoCastVictim(SPELL_ARCANE_PROJECTILES);
                    }
                    break;
                case SPELL_ARCANE_BARRAGE:
                    arcaneCharges = 0;
                    break;
                default:
                    break;
            }
        }

        void JustEngagedWith(Unit* who) override
        {
            DoCastSelf(SPELL_PRISMATIC_BARRIER,
                       CastSpellExtraArgs(SPELLVALUE_BASE_POINT0, 256E3));

            scheduler
                .Schedule(1s, [this](TaskContext arcane_blast)
                {
                    if (arcaneCharges < 4)
                        DoCastVictim(SPELL_ARCANE_BLAST);
                    else
                        DoCastVictim(SPELL_ARCANE_BARRAGE);
                    arcane_blast.Repeat(2800ms);
                })
                .Schedule(3s, [this](TaskContext evocation)
                {
                    if (me->GetPowerPct(POWER_MANA) < 20)
                    {
                        DoCast(SPELL_EVOCATION);
                        evocation.Repeat(3min);
                    }
                    else
                        evocation.Repeat(3s);
                })
                .Schedule(5s, [this](TaskContext arcane_explosion)
                {
                    if (EnemiesInRange(9.f) >= 3)
                    {
                        me->CastStop();
                        DoCast(SPELL_ARCANE_EXPLOSION);
                        arcane_explosion.Repeat(14s, 18s);
                    }
                    else
                        arcane_explosion.Repeat(1s);
                });
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
                    scheduler.Schedule(2s, [this](TaskContext context)
                    {
                        switch (context.GetRepeatCounter())
                        {
                            case 0:
                                me->CastSpell(me, SPELL_PORTAL_CHANNELING_03);
                                context.Repeat(1s);
                                break;
                            case 1:
                                if (Creature* tari = instance->GetCreature(DATA_TARI_COGG))
                                    tari->CastSpell(tari, SPELL_PORTAL_CHANNELING_01);
                                context.Repeat(1800ms);
                                break;
                            case 2:
                                if (GameObject* barrier = instance->GetGameObject(DATA_MYSTIC_BARRIER_02))
                                    barrier->UseDoorOrButton();
                                context.CancelAll();
                                break;
                        }
                    });
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

class npc_theramore_officier : public CreatureScript
{
    public:
    npc_theramore_officier() : CreatureScript("npc_theramore_officier")
    {
    }

    struct npc_theramore_officierAI : public npc_theramore_troopAI
    {
        npc_theramore_officierAI(Creature* creature) : npc_theramore_troopAI(creature, AI_Type::Melee)
        {
        }

        enum Misc
        {
            SPELL_FROST_NOVA            = 284879
        };

        enum Spells
        {
            SPELL_AVENGING_WRATH        = 292266,
            SPELL_BLESSING_OF_FREEDOM   = 299256,
            SPELL_CRUSADER_STRIKE       = 295670,
            SPELL_HOLY_LIGHT            = 295698,
            SPELL_JUDGMENT              = 295671,
            SPELL_LIGHT_OF_DAWN         = 295710,
            SPELL_REBUKE                = 173085,
            SPELL_DIVINE_STORM          = 183897
        };

        void SpellHit(Unit* caster, SpellInfo const* spellInfo) override
        {
            if (spellInfo->Id == SPELL_FROST_NOVA)
            {
                scheduler.Schedule(2s, [caster, this](TaskContext /*blessing_of_freedom*/)
                {
                    me->AttackedTarget(caster, true);
                    DoCastSelf(SPELL_BLESSING_OF_FREEDOM);
                });
            }
        }

        void JustEngagedWith(Unit* who) override
        {
            if (roll_chance_i(30))
                DoCastSelf(SPELL_AVENGING_WRATH);

            scheduler
                .Schedule(5ms, [this](TaskContext rebuke)
                {
                    if (Unit* target = DoSelectCastingUnit(SPELL_REBUKE, 35.f))
                    {
                        me->CastStop();
                        DoCast(target, SPELL_REBUKE);
                        rebuke.Repeat(25s, 40s);
                    }
                    else
                    {
                        rebuke.Repeat(1s);
                    }
                })
                .Schedule(1s, 2s, [this](TaskContext holy_light)
                {
                    if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 80))
                    {
                        me->CastStop();
                        DoCast(target, SPELL_HOLY_LIGHT);
                    }
                    holy_light.Repeat(8s);
                })
                .Schedule(5s, 7s, [this](TaskContext light_of_dawn)
                {
                    if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 50))
                    {
                        me->CastStop();
                        DoCast(target, SPELL_LIGHT_OF_DAWN);
                    }
                    light_of_dawn.Repeat(10s, 15s);
                })
                .Schedule(8s, 14s, [this](TaskContext divine_storm)
                {
                    if (EnemiesInRange(8.0f) >= 3)
                    {
                        DoCast(SPELL_DIVINE_STORM);
                        divine_storm.Repeat(12s, 25s);
                    }
                    else
                        divine_storm.Repeat(1s);
                })
                .Schedule(14s, 22s, [this](TaskContext crusader_strike)
                {
                    DoCastVictim(SPELL_CRUSADER_STRIKE);
                    crusader_strike.Repeat(5s, 8s);
                })
                .Schedule(2s, 8s, [this](TaskContext judgment)
                {
                    DoCastVictim(SPELL_JUDGMENT);
                    judgment.Repeat(12s, 15s);
                });
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_theramore_officierAI(creature);
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
            SPELL_VIGILANT_STRIKE       = 260834,
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
                .Schedule(1s, 5s, [this](TaskContext vigilant_strike)
                {
                    DoCastVictim(SPELL_VIGILANT_STRIKE);
                    vigilant_strike.Repeat(8s, 14s);
                })
                .Schedule(15s, 25s, [this](TaskContext whirlwind)
                {
                    DoCast(SPELL_WHIRLWIND);
                    whirlwind.Repeat(1min);
                });
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_theramore_footmanAI(creature);
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
            SPELL_MAGE_ARMOR        = 183079,
            SPELL_ARCANE_HASTE      = 50182
        };

        float GetDistance() override
        {
            return 35.f;
        }

        void Reset() override
        {
            npc_theramore_troopAI::Reset();

            scheduler.Schedule(1s, [this](TaskContext /*context*/)
            {
                DoCastSelf(SPELL_MAGE_ARMOR);
            });
        }

        void JustEngagedWith(Unit* who) override
        {
            DoCast(SPELL_ARCANE_HASTE);

            scheduler
                .Schedule(1ms, [this](TaskContext arcane_blast)
                {
                    DoCastVictim(SPELL_ARCANE_BLAST);
                    arcane_blast.Repeat(2300ms);
                })
                .Schedule(3s, 5s, [this](TaskContext arcane_explosion)
                {
                    if (EnemiesInRange(10.f) >= 2)
                    {
                        me->CastStop();
                        DoCast(SPELL_ARCANE_EXPLOSION);
                        arcane_explosion.Repeat(10s, 14s);
                    }
                    else
                        arcane_explosion.Repeat(1s);
                })
                .Schedule(4s, 8s, [this](TaskContext arcane_missiles)
                {
                    if (Unit* victim = SelectTarget(SELECT_TARGET_RANDOM))
                        DoCast(victim, SPELL_ARCANE_MISSILES);
                    arcane_missiles.Repeat(8s, 12s);
                })
                .Schedule(1s, [this](TaskContext arcane_haste)
                {
                    if (!me->HasAura(SPELL_ARCANE_HASTE))
                        DoCast(SPELL_ARCANE_HASTE);
                    arcane_haste.Repeat(2s);
                });
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_theramore_arcanistAI(creature);
    }
};

class npc_amara_leeson : public CreatureScript
{
    public:
    npc_amara_leeson() : CreatureScript("npc_amara_leeson")
    {
    }

    struct npc_amara_leesonAI : public npc_theramore_troopAI
    {
        npc_amara_leesonAI(Creature* creature) : npc_theramore_troopAI(creature, AI_Type::Distance)
        {
        }

        enum Spells
        {
            SPELL_FIREBALL              = 20678,
            SPELL_BLAZING_BARRIER       = 295238,
            SPELL_PRISMATIC_BARRIER     = 235450,
            SPELL_ICE_BARRIER           = 198094,
            SPELL_BLINK                 = 295236,
            SPELL_DRAGON_BREATH         = 295240,
            SPELL_GREATER_PYROBLAST     = 295231,
            SPELL_SCORCH                = 301075
        };

        float GetDistance() override
        {
            return 15.f;
        }

        void JustEngagedWith(Unit* who) override
        {
            DoCastSelf(SPELL_BLAZING_BARRIER, true);
            DoCastSelf(SPELL_PRISMATIC_BARRIER, true);
            DoCastSelf(SPELL_ICE_BARRIER, true);

            scheduler
                .Schedule(1ms, [this](TaskContext fireball)
                {
                    DoCastVictim(SPELL_FIREBALL);
                    fireball.Repeat(1600ms);
                })
                .Schedule(2s, [this](TaskContext scorch)
                {
                    DoCastVictim(SPELL_SCORCH);
                    scorch.Repeat(3s);
                })
                .Schedule(4s, [this](TaskContext blink)
                {
                    if (EnemiesInRange(20.f) >= 3)
                    {
                        me->CastStop();
                        DoCast(SPELL_BLINK);
                        blink.Repeat(24s, 32s);
                    }
                    else
                        blink.Repeat(1s);
                })
                .Schedule(5s, [this](TaskContext dragon_breath)
                {
                    if (EnemiesInRange(12.f) >= 3)
                    {
                        me->CastStop();
                        DoCast(SPELL_DRAGON_BREATH);
                        dragon_breath.Repeat(24s, 32s);
                    }
                    else
                        dragon_breath.Repeat(1s);
                })
                .Schedule(8s, [this](TaskContext greater_pyroblast)
                {
                    if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0))
                    {
                        me->CastStop();
                        DoCast(target, SPELL_GREATER_PYROBLAST);
                    }
                    greater_pyroblast.Repeat(14s, 18s);
                });
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_amara_leesonAI(creature);
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
            instance = creature->GetInstanceScript();
        }

        enum Spells
        {
            SPELL_SMITE                 = 332705,
            SPELL_HEAL                  = 332706,
            SPELL_FLASH_HEAL            = 314655,
            SPELL_RENEW                 = 294342,
            SPELL_PRAYER_OF_HEALING     = 266969,
            SPELL_POWER_WORD_FORTITUDE  = 267528,
            SPELL_POWER_WORD_SHIELD     = 318158,
            SPELL_PAIN_SUPPRESSION      = 69910,
            SPELL_PSYCHIC_SCREAM        = 65543,
        };

        bool ascension;
        InstanceScript* instance;

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

        void Reset() override
        {
            npc_theramore_troopAI::Reset();

            scheduler.Schedule(1s, 5s, [this](TaskContext fortitude)
            {
                BFTPhases phase = (BFTPhases)instance->GetData(DATA_SCENARIO_PHASE);
                if (phase < BFTPhases::HelpTheWounded)
                {
                    CastSpellExtraArgs args(true);
                    args.SetTriggerFlags(TRIGGERED_IGNORE_SET_FACING);

                    if (Unit* target = SelectRandomFriendlyMissingBuff(SPELL_POWER_WORD_FORTITUDE))
                        DoCast(target, SPELL_POWER_WORD_FORTITUDE, args);
                    fortitude.Repeat(5s, 8s);
                }
            });
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
                .Schedule(1s, 2s, [this](TaskContext power_word_shield)
                {
                    if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 80))
                    {
                        CastSpellExtraArgs args;
                        args.AddSpellBP0(target->CountPctFromMaxHealth(50));

                        me->CastStop();
                        DoCast(target, SPELL_POWER_WORD_SHIELD, args);
                    }
                    power_word_shield.Repeat(8s);
                })
                .Schedule(5s, 7s, [this](TaskContext renew)
                {
                    if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 50))
                        DoCast(target, SPELL_RENEW);
                    renew.Repeat(10s, 15s);
                })
                .Schedule(12s, 14s, [this](TaskContext prayer_of_healing)
                {
                    me->CastStop();
                    DoCastSelf(SPELL_PRAYER_OF_HEALING);
                    prayer_of_healing.Repeat(25s);
                })
                .Schedule(1s, 3s, [this](TaskContext heal)
                {
                    if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 60))
                        DoCast(target, SPELL_HEAL);
                    heal.Repeat(5s, 8s);
                })
                .Schedule(1s, 3s, [this](TaskContext psychic_scream)
                {
                    if (EnemiesInRange(10.f) >= 2)
                    {
                        DoCast(SPELL_PSYCHIC_SCREAM);
                        psychic_scream.Repeat(10s, 25s);
                    }
                    else
                        psychic_scream.Repeat(1s);
                })
                .Schedule(1s, 8s, [this](TaskContext flash_heal)
                {
                    if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 40))
                        DoCast(target, SPELL_FLASH_HEAL);
                    flash_heal.Repeat(2s);
                });
        }

        private:

        Unit* SelectRandomFriendlyMissingBuff(uint32 spell)
        {
            std::list<Creature*> list = DoFindFriendlyMissingBuff(spell);
            if (list.empty())
                return nullptr;
            return Trinity::Containers::SelectRandomContainerElement(list);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_theramore_faithfulAI(creature);
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

    void MovementInform(uint32 type, uint32 id) override
    {
        if (me->IsEngaged())
            return;

        if (type == POINT_MOTION_TYPE)
        {
            switch (id)
            {
                case MOVEMENT_INFO_POINT_03:
                    me->CastSpell(me, SPELL_ARCANIC_BARRIER, args);
                    me->DespawnOrUnsummon(2s);
                    break;
                default:
                    break;
            }
        }
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        me->CallAssistance();
    }

    protected:

    uint32 EnemiesInRange(float distance)
    {
        uint32 count = 0;
        for (ThreatReference const* ref : me->GetThreatManager().GetUnsortedThreatList())
            if (ref->GetVictim()->IsWithinDist(me, distance, false))
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
        npc_roknah_hagAI(Creature* creature) : npc_theramore_hordeAI(creature, AI_Type::Distance),
            closeTarget(false), iceblock(false), icicle(0)
        {
        }

        enum Groups
        {
            GROUP_NORMAL,
            GROUP_FLEE
        };

        uint32 Icicles[5] =
        {
            214130,
            214127,
            214126,
            214125,
            214124,
        };

        enum Spells
        {
            SPELL_BLINK         = 284877,
            SPELL_CONE_OF_COLD  = 292294,
            SPELL_EBONBOLT      = 284752,
            SPELL_FLURRY        = 284858,
            SPELL_FROST_NOVA    = 284879,
            SPELL_FROSTBOLT     = 284703,
            SPELL_GLACIAL_SPIKE = 284840,
            SPELL_ICE_BLOCK     = 290049,
            SPELL_ICE_BARRIER   = 198094,
            SPELL_ICICLES       = 205473,
        };

        bool closeTarget;
        bool iceblock;
        uint8 icicle;

        void OnSuccessfulSpellCast(SpellInfo const* spell) override
        {
            switch (spell->Id)
            {
                case SPELL_GLACIAL_SPIKE:
                    icicle = 0;
                    me->RemoveAurasDueToSpell(SPELL_ICICLES);
                    for (uint8 i = 0; i < 5; i++)
                        me->RemoveAurasDueToSpell(Icicles[i]);
                    break;
                case SPELL_FROSTBOLT:
                case SPELL_FROST_NOVA:
                case SPELL_EBONBOLT:
                case SPELL_FLURRY:
                    DoCastSelf(SPELL_ICICLES, true);
                    DoCastSelf(Icicles[icicle], true);
                    icicle++;
                    break;
            }
        }

        void DamageTaken(Unit* /*attacker*/, uint32& damage) override
        {
            if (!iceblock && HealthBelowPct(20))
            {
                damage = 0;

                iceblock = true;

                me->CastStop();

                DoCast(SPELL_ICE_BLOCK);

                scheduler.Schedule(1min, [this](TaskContext /*context*/)
                {
                    iceblock = false;
                });

                CastFleeSequence(11s);
            }
        }

        void JustEngagedWith(Unit* who) override
        {
            npc_theramore_hordeAI::JustEngagedWith(who);

            DoCastSelf(SPELL_ICE_BARRIER);

            scheduler
                .Schedule(13s, 18s, GROUP_NORMAL, [this](TaskContext cone_of_cold)
                {
                    if (EnemiesInRange(12.0f) > 2)
                    {
                        me->CastStop();
                        DoCast(SPELL_CONE_OF_COLD);
                        cone_of_cold.Repeat(5s, 8s);
                    }
                    else
                        cone_of_cold.Repeat(2s);
                })
                .Schedule(2s, 5s, GROUP_NORMAL, [this](TaskContext ebonbolt)
                {
                    DoCastVictim(SPELL_EBONBOLT);
                    ebonbolt.Repeat(2s, 5s);
                })
                .Schedule(12s, 15s, GROUP_NORMAL, [this](TaskContext flurry)
                {
                    if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM))
                        DoCast(target, SPELL_FLURRY);
                    flurry.Repeat(12s, 14s);
                })
                .Schedule(1ms, GROUP_NORMAL, [this](TaskContext frostbolt)
                {
                    DoCastVictim(SPELL_FROSTBOLT);
                    frostbolt.Repeat(2s);
                })
                .Schedule(5s, GROUP_NORMAL, [this](TaskContext glacial_spike)
                {
                    if (Aura* aura = me->GetAura(SPELL_ICICLES))
                    {
                        if (aura->GetStackAmount() >= 5)
                            DoCastVictim(SPELL_GLACIAL_SPIKE);
                    }
                    glacial_spike.Repeat(1s);
                });
        }

        void UpdateAI(uint32 diff) override
        {
            CustomAI::UpdateAI(diff);

            if (!closeTarget
                && !me->HasAura(SPELL_ICE_BLOCK)
                && EnemiesInRange(12.0f) > 2)
            {
                closeTarget = true;
                scheduler.DelayGroup(GROUP_NORMAL, 2s);
                me->CastStop();
                CastFleeSequence(1s);
            }
        }

        void CastFleeSequence(Seconds start)
        {
            if (me->HasAura(SPELL_ICE_BLOCK))
                return;

            scheduler.Schedule(start, GROUP_FLEE, [this](TaskContext context)
            {
                switch (context.GetRepeatCounter())
                {
                    case 0:
                        me->CastStop();
                        DoCastSelf(SPELL_FROST_NOVA, true);
                        context.Repeat(500ms);
                        break;
                    case 1:
                        DoCastSelf(SPELL_BLINK, true);
                        scheduler.CancelGroup(GROUP_FLEE);
                        context.Repeat(5s);
                        break;
                    case 2:
                        closeTarget = false;
                        break;
                }
            });
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_roknah_hagAI(creature);
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

        enum Spells
        {
            SPELL_EXECUTE               = 283424,
            SPELL_MORTAL_STRIKE         = 283410,
            SPELL_OVERPOWER             = 283426,
            SPELL_REND                  = 283419,
            SPELL_SLAM                  = 299995,
            SPELL_HAMMER_STUN           = 36138
        };

        void JustEngagedWith(Unit* who) override
        {
            npc_theramore_hordeAI::JustEngagedWith(who);

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
                .Schedule(5s, 8s, [this](TaskContext execute)
                {
                    DoCastVictim(SPELL_EXECUTE);
                    execute.Repeat(15s, 28s);
                })
                .Schedule(2s, 5s, [this](TaskContext mortal_strike)
                {
                    switch (mortal_strike.GetRepeatCounter())
                    {
                        case 0:
                            if (!me->HasAura(SPELL_OVERPOWER) && roll_chance_i(60))
                                DoCastSelf(SPELL_OVERPOWER);
                            mortal_strike.Repeat(1s);
                            break;
                        case 1:
                            me->CastStop();
                            DoCastVictim(SPELL_MORTAL_STRIKE);
                            mortal_strike.Repeat(8s, 10s);
                            break;
                    }
                })
                .Schedule(14s, 22s, [this](TaskContext overpower)
                {
                    if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0))
                        DoCast(target, SPELL_REND);
                    overpower.Repeat(8s, 10s);
                })
                .Schedule(32s, 38s, [this](TaskContext rend_slam)
                {
                    if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0))
                        DoCast(target, RAND(SPELL_REND, SPELL_SLAM));
                    rend_slam.Repeat(2s, 8s);
                });
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_roknah_gruntAI(creature);
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
            flameShock = sSpellMgr->AssertSpellInfo(SPELL_FLAME_SHOCK, DIFFICULTY_NONE);
            frostShock = sSpellMgr->AssertSpellInfo(SPELL_FROST_SHOCK, DIFFICULTY_NONE);
        }

        enum Spells
        {
            SPELL_ASTRAL_SHIFT      = 292158,
            SPELL_CHAIN_LIGHTNING   = 290411,
            SPELL_FLAME_SHOCK       = 290422,
            SPELL_FROST_SHOCK       = 290441,
            SPELL_EARTHQUAKE        = 160162,
            SPELL_HEALING_SURGE     = 290435,
            SPELL_LAVA_BURST        = 290423,
            SPELL_WIND_SHEAR        = 290439,
            SPELL_LIGHTNING_BOLT    = 290395,
            SPELL_RIPTIDE           = 241892,
            SPELL_CHAIN_HEAL        = 258099,
        };

        const SpellInfo* flameShock;
        const SpellInfo* frostShock;

        void DamageTaken(Unit* /*attacker*/, uint32& /*damage*/) override
        {
            if (HealthBelowPct(40) && !me->HasAura(SPELL_ASTRAL_SHIFT))
            {
                scheduler.Schedule(1ms, [this](TaskContext astral_shift)
                {
                    DoCast(SPELL_ASTRAL_SHIFT);
                    astral_shift.Repeat(1min);
                });
            }
        }

        void JustEngagedWith(Unit* who) override
        {
            npc_theramore_hordeAI::JustEngagedWith(who);

            scheduler
                .Schedule(8s, 14s, [this](TaskContext chain_lightning)
                {
                    if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0))
                        DoCast(target, SPELL_CHAIN_LIGHTNING);
                    chain_lightning.Repeat(3s, 5s);
                })
                .Schedule(5s, 8s, [this](TaskContext frost_shock)
                {
                    if (Unit* target = DoFindEnemyMissingDot(frostShock))
                    {
                        if (!target->HasAura(SPELL_FROST_SHOCK))
                            DoCast(target, SPELL_FROST_SHOCK);
                    }
                    frost_shock.Repeat(8s, 10s);
                })
                .Schedule(5s, 8s, [this](TaskContext flame_shock)
                {
                    if (Unit* target = DoFindEnemyMissingDot(flameShock))
                    {
                        if (!target->HasAura(SPELL_FLAME_SHOCK))
                            DoCast(target, SPELL_FLAME_SHOCK);
                    }
                    flame_shock.Repeat(5s, 8s);
                })
                .Schedule(20s, 25s, [this](TaskContext earthquake)
                {
                    if (EnemiesInRange(8.0f) >= 3)
                    {
                        DoCast(SPELL_EARTHQUAKE);
                        earthquake.Repeat(10s, 13s);
                    }
                    else
                        earthquake.Repeat(1s);
                })
                .Schedule(1ms, [this](TaskContext healing_surge)
                {
                    if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 60))
                        DoCast(target, SPELL_HEALING_SURGE);
                    healing_surge.Repeat(2s);
                })
                .Schedule(1ms, [this](TaskContext riptide)
                {
                    if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 80))
                    {
                        if (!target->HasAura(SPELL_RIPTIDE))
                            DoCast(target, SPELL_RIPTIDE);
                    }
                    riptide.Repeat(3s);
                })
                .Schedule(1ms, [this](TaskContext chain_heal)
                {
                    if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 40))
                        DoCast(target, SPELL_CHAIN_HEAL);
                    chain_heal.Repeat(5s);
                })
                .Schedule(11s, 15s, [this](TaskContext lava_burst)
                {
                    DoCastVictim(SPELL_LAVA_BURST);
                    lava_burst.Repeat(8s, 10s);
                })
                .Schedule(1ms, [this](TaskContext wind_shear)
                {
                    if (Unit* target = DoSelectCastingUnit(SPELL_WIND_SHEAR, 35.f))
                    {
                        me->CastStop();
                        DoCast(target, SPELL_WIND_SHEAR);
                        wind_shear.Repeat(7s, 10s);
                    }
                    else
                        wind_shear.Repeat(1s);

                })
                .Schedule(1ms, [this](TaskContext lightning_bolt)
                {
                    DoCastVictim(SPELL_LIGHTNING_BOLT);
                    lightning_bolt.Repeat(2s);
                });
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_roknah_loasingerAI(creature);
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
            SPELL_CORRUPTION        = 251406,
        };

        const SpellInfo* immolateInfo;
        const SpellInfo* corruptionInfo;

        float GetDistance() override
        {
            return 18.f;
        }

        void JustEngagedWith(Unit* who) override
        {
            npc_theramore_hordeAI::JustEngagedWith(who);

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
                        if (!target->HasAura(SPELL_IMMOLATE))
                            DoCast(target, SPELL_IMMOLATE);
                    }
                    immolate.Repeat(2s, 5s);
                })
                .Schedule(1ms, [this](TaskContext corruption)
                {
                    if (Unit* target = DoFindEnemyMissingDot(corruptionInfo))
                        DoCast(target, SPELL_CORRUPTION);
                    corruption.Repeat(2s, 5s);
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
        return new npc_roknah_felcasterAI(creature);
    }
};

// Light of Dawn - 295712
class spell_theramore_light_of_dawn : public SpellScript
{
    PrepareSpellScript(spell_theramore_light_of_dawn);

    enum Spells
    {
        SPELL_LIGHT_OF_DAWN = 295712
    };

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
        {
            Unit* caster = GetCaster();
            caster->CastSpell(caster, SPELL_LIGHT_OF_DAWN, true);
            caster->CastSpell(target, SPELL_LIGHT_OF_DAWN, true);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_theramore_light_of_dawn::HandleDummy, EFFECT_1, SPELL_EFFECT_DUMMY);
    }
};

// Greater Pyroblast - 295231
class spell_theramore_greater_pyroblast : public SpellScript
{
    PrepareSpellScript(spell_theramore_greater_pyroblast);

    void HandleDamages(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* victim = GetHitUnit();
        if (caster && victim)
        {
            int32 pct = GetSpellInfo()->GetEffect(EFFECT_1).BasePoints;
            int32 damage = victim->CountPctFromMaxHealth(pct);
            Unit::DealDamage(caster, victim, damage);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_theramore_greater_pyroblast::HandleDamages, EFFECT_0, SPELL_EFFECT_DAMAGE_FROM_MAX_HEALTH_PCT);
    }
};

// 	Bucket Lands - 42339
class spell_theramore_throw_bucket : public SpellScript
{
    PrepareSpellScript(spell_theramore_throw_bucket);

    void HandleDummy(SpellEffIndex effIndex)
    {
        Unit* caster = GetCaster();
        const WorldLocation* destination = GetHitDest();
        if (caster && destination)
        {
            float radius = GetSpellInfo()->GetEffect(effIndex).CalcRadius();
            float x = destination->GetPositionX();
            float y = destination->GetPositionY();
            float z = destination->GetPositionZ();

            if (Creature* trigger = caster->SummonTrigger(x, y, z, 0.0f, 5 * IN_MILLISECONDS))
            {
                std::list<Creature*> fires;
                trigger->GetCreatureListWithEntryInGrid(fires, NPC_THERAMORE_FIRE_CREDIT, radius);

                for (Creature* fire : fires)
                {
                    if (!fire->HasAura(SPELL_COSMETIC_LARGE_FIRE))
                        continue;

                    if (Player* player = caster->ToPlayer())
                    {
                        KillRewarder(player, fire, false).Reward(NPC_THERAMORE_FIRE_CREDIT);
                    }

                    fire->DespawnOrUnsummon();
                }
            }
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_theramore_throw_bucket::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 295238 - Blazing Barrier
// 198094 - Ice Barrier
class spell_theramore_barrier : public AuraScript
{
    PrepareAuraScript(spell_theramore_barrier);

    void CalculateAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& canBeRecalculated)
    {
        canBeRecalculated = false;

        if (Unit* caster = GetCaster())
        {
            uint32 health = caster->GetMaxHealth();
            amount = int32(health * 0.20f);
        }
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_theramore_barrier::CalculateAmount, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB);
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
            timeInterval = interval;
        }

        const int32 interval = 1000;

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

            timeInterval += diff;
            if (timeInterval < interval)
                return;

            if (TempSummon* tempSumm = caster->SummonCreature(WORLD_TRIGGER, at->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, 200))
            {
                tempSumm->SetFaction(caster->GetFaction());
                tempSumm->SetOwnerGUID(caster->GetGUID());

                PhasingHandler::InheritPhaseShift(tempSumm, caster);

                for (ObjectGuid unit : at->GetInsideUnits())
                {
                    if (Unit* target = ObjectAccessor::GetUnit(*caster, unit))
                    {
                        if (!caster->IsHostileTo(target))
                            continue;

                        caster->CastSpell(target, SPELL_BLIZZARD_DAMAGE, true);

                        if (target->GetTypeId() == TYPEID_PLAYER)
                            continue;

                        if (target->HasUnitState(UNIT_STATE_FLEEING) || target->HasUnitState(UNIT_STATE_FLEEING_MOVE))
                            continue;

                        if (roll_chance_i(60))
                        {
                            target->CastStop();
                            target->GetMotionMaster()->Clear();
                            target->GetMotionMaster()->MoveFleeing(tempSumm, 3 * IN_MILLISECONDS);
                        }
                    }
                }
            }

            timeInterval -= interval;
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
    new npc_unmanned_tank();
    new npc_theramore_officier();
    new npc_theramore_footman();
    new npc_theramore_faithful();
    new npc_theramore_arcanist();
    new npc_wounded_theramore_troop();
    new npc_amara_leeson();
    new npc_roknah_hag();
    new npc_roknah_grunt();
    new npc_roknah_loasinger();
    new npc_roknah_felcaster();

    RegisterSpellScript(spell_theramore_light_of_dawn);
    RegisterSpellScript(spell_theramore_greater_pyroblast);
    RegisterSpellScript(spell_theramore_throw_bucket);

    RegisterAuraScript(spell_theramore_barrier);

    new at_blizzard_theramore();
}
