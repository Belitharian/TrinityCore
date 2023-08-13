#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "Containers.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "KillRewarder.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "PassiveAI.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellAuraEffects.h"
#include "TemporarySummon.h"
#include "battle_for_theramore.h"

///
///     ALLIANCE NPC
///

struct npc_theramore_citizen : public ScriptedAI
{
	enum Misc
	{
		GOSSIP_MENU_DEFAULT             = 65000,
		NPC_THERAMORE_CITIZEN_CREDIT    = 500005
	};

	npc_theramore_citizen(Creature* creature) : ScriptedAI(creature)
	{
	}

	TaskScheduler scheduler;

	void MovementInform(uint32 type, uint32 id) override
	{
		if (type != POINT_MOTION_TYPE)
			return;

		if (id == MOVEMENT_INFO_POINT_01)
		{
			me->SetVisible(false);
		}
	}

	bool OnGossipHello(Player* player) override
	{
		player->PrepareGossipMenu(me, GOSSIP_MENU_DEFAULT, true);
		player->SendPreparedGossip(me);
		return true;
	}

	bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
	{
		ClearGossipMenuFor(player);

		switch (gossipListId)
		{
			case 0:
			{
				#ifdef CUSTOM_DEBUG
					for (uint8 i = 0; i < NUMBER_OF_CITIZENS; ++i)
					{
						KillRewarder::Reward(player, me, NPC_THERAMORE_CITIZEN_CREDIT);
					}
				#else
					KillRewarder::Reward(player, me, NPC_THERAMORE_CITIZEN_CREDIT);
				#endif

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

struct npc_unmanned_tank : public ScriptedAI
{
	npc_unmanned_tank(Creature* creature) : ScriptedAI(creature)
	{
	}

	void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
	{
		if (spellInfo->Id != SPELL_REPAIR)
			return;

		me->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
	}
};

struct npc_wounded_theramore_troop : public ScriptedAI
{
	npc_wounded_theramore_troop(Creature* creature) : ScriptedAI(creature),
		preventClick(false)
	{
		instance = creature->GetInstanceScript();
	}

	enum Spells
	{
		SPELL_TELEPORT_TROOP = 69074
	};

	InstanceScript* instance;
	bool preventClick;

	void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
	{
		if (spellInfo->Id != SPELL_TELEPORT_TROOP)
			return;

		if (preventClick)
			return;

		if (Player* player = caster->ToPlayer())
		{
			KillRewarder::Reward(player, me, NPC_THERAMORE_WOUNDED_TROOP);
		}

		me->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);

		uint32 counter = instance->GetData(DATA_WOUNDED_TROOPS);

		if (counter < NUMBER_OF_WOUNDED - 1)
		{
			me->DespawnOrUnsummon();
		}
		else
		{
			if (Player* player = caster->ToPlayer())
			{
				me->PlayDistanceSound(SOUND_COUNTERSPELL, player);
				if (Creature* jaina = instance->GetCreature(DATA_JAINA_PROUDMOORE))
					jaina->AI()->Talk(SAY_WOUNDED_TROOP, player);
			}
		}

		counter += 1;

		instance->SetData(DATA_WOUNDED_TROOPS, counter);

		preventClick = true;
	}
};

struct npc_theramore_troop : public CustomAI
{
	npc_theramore_troop(Creature* creature, AI_Type type) : CustomAI(creature, type), emoteReceived(false)
	{
		instance = creature->GetInstanceScript();
        soundEmote = creature->GetGender() == GENDER_FEMALE ? 74679 : 74681;
	}

	enum Misc
	{
		NPC_THERAMORE_TROOPS_CREDIT = 500011,
	};

	InstanceScript* instance;
    uint32 soundEmote;
	bool emoteReceived;

	void ReceiveEmote(Player* player, uint32 emoteId) override
	{
		BFTPhases phase = (BFTPhases)instance->GetData(DATA_SCENARIO_PHASE);
		if (phase == BFTPhases::Preparation || phase == BFTPhases::Preparation_Rhonin)
		{
			#ifdef CUSTOM_DEBUG
				for (uint8 i = 0; i < NUMBER_OF_TROOPS; i++)
				{
					KillRewarder::Reward(player, me, NPC_THERAMORE_TROOPS_CREDIT);
				}
			#else
				if (!emoteReceived && emoteId == TEXT_EMOTE_FORTHEALLIANCE)
				{
					if (player->IsWithinDist(me, 5.f))
					{
                        float orientation = me->GetOrientation();
                        scheduler.Schedule(5ms, [orientation, player, this](TaskContext context)
                        {
                            switch (context.GetRepeatCounter())
                            {
                                case 0:
                                    me->SetFacingToObject(player);
                                    context.Repeat(1s);
                                    break;
                                case 1:
                                    me->PlayDirectSound(soundEmote);
                                    me->HandleEmoteCommand(EMOTE_ONESHOT_CHEER_FORTHEALLIANCE);
                                    KillRewarder::Reward(player, me, NPC_THERAMORE_TROOPS_CREDIT);
                                    emoteReceived = true;
                                    context.Repeat(3s);
                                    break;
                                case 2:
                                    me->SetFacingTo(orientation);
                                    break;
                            }

                        });
					}
				}
			#endif
		}
	}
};

struct npc_thader_windermere : public CustomAI
{
	enum Misc
	{
		GOSSIP_MENU_DEFAULT = 65002,
	};

	npc_thader_windermere(Creature* creature) : CustomAI(creature)
	{
		instance = creature->GetInstanceScript();
	}

	InstanceScript* instance;

	bool OnGossipHello(Player* player) override
	{
		player->PrepareGossipMenu(me, GOSSIP_MENU_DEFAULT, true);
		player->SendPreparedGossip(me);
		return true;
	}

	bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
	{
		ClearGossipMenuFor(player);

		switch (gossipListId)
		{
			case 0:
				me->RemoveAurasDueToSpell(SPELL_CHAT_BUBBLE);
				me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
				KillRewarder::Reward(player, me);
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

struct npc_hedric_evencane : public CustomAI
{
    npc_hedric_evencane(Creature* creature) : CustomAI(creature, AI_Type::Melee),
		banding(false)
	{
	}

	enum Spells
	{
		SPELL_BATTER                = 66408,
		SPELL_RISING_ANGER          = 136323,
		SPELL_MORTAL_CLEAVE         = 177147,
		SPELL_WHIRLWIND             = 277637,
		SPELL_HEW                   = 319957,
		SPELL_BANDAGE               = 333552,
		SPELL_VICIOUS_WOUND         = 334960
	};

	bool banding;

	void OnChannelFinished(SpellInfo const* spell) override
	{
		if (spell->Id == SPELL_BANDAGE)
			banding = false;
	}

	void DamageTaken(Unit* /*attacker*/, uint32& /*damage*/, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
        if (ShouldTakeDamage())
            return;

		if (banding)
			return;

		banding = true;

        CastStop();

		DoCast(me, SPELL_BANDAGE,
                CastSpellExtraArgs(TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD)
                .AddSpellBP0(30));
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		scheduler
			.Schedule(3s, [this](TaskContext mortal_cleave)
			{
				DoCastVictim(SPELL_MORTAL_CLEAVE);
				mortal_cleave.Repeat(3s, 5s);
			})
			.Schedule(8s, [this](TaskContext hew)
			{
				DoCastVictim(SPELL_HEW);
				hew.Repeat(8s, 15s);
			})
			.Schedule(14s, [this](TaskContext whirlwind)
			{
				DoCast(SPELL_WHIRLWIND);
				whirlwind.Repeat(25s, 32s);
			})
			.Schedule(25s, [this](TaskContext vicious_wound)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_VICIOUS_WOUND);
				vicious_wound.Repeat(10s, 15s);
			})
			.Schedule(5s, [this](TaskContext batter)
			{
				if (Unit* target = DoSelectCastingUnit(SPELL_BATTER, 5.0f))
					DoCast(target, SPELL_BATTER);
				batter.Repeat(15s, 20s);
			})
			.Schedule(1min, [this](TaskContext rising_anger)
			{
				DoCast(SPELL_RISING_ANGER);
			});
	}
};

struct npc_theramore_officier : public npc_theramore_troop
{
	npc_theramore_officier(Creature* creature) : npc_theramore_troop(creature, AI_Type::Melee),
        healthLow(false)
	{
	}

	enum Misc
	{
		SPELL_FROST_NOVA            = 284879
	};

	enum Spells
	{
		SPELL_DIVINE_SHIELD         = 642,
        SPELL_HOLY_SHOCK            = 20473,
        SPELL_LIGHT_HAMMER          = 114158,
		SPELL_DIVINE_STORM          = 183897,
		SPELL_HEAL                  = 225638,
        SPELL_EXARCH_BLADE          = 268742,
        SPELL_CONSECRATED_GROUND    = 268918,
		SPELL_AVENGING_WRATH        = 292266,
		SPELL_CRUSADER_STRIKE       = 295670,
		SPELL_HOLY_LIGHT            = 295698,
		SPELL_JUDGMENT              = 295671,
		SPELL_LIGHT_OF_DAWN         = 295710,
		SPELL_BLESSING_OF_FREEDOM   = 299256,
		SPELL_REBUKE                = 405397,
	};

    bool healthLow;

	void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
	{
        if (!me->GetSpellHistory()->HasCooldown(SPELL_BLESSING_OF_FREEDOM))
        {
            if (HasMechanic(spellInfo, MECHANIC_ROOT) || HasMechanic(spellInfo, MECHANIC_SNARE))
            {
                scheduler.Schedule(2s, [this](TaskContext /*blessing_of_freedom*/)
                {
                    DoCastSelf(SPELL_BLESSING_OF_FREEDOM);
                });
            }
        }
	}

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
		if (roll_chance_i(30) && !healthLow && me->HealthBelowPctDamaged(25, damage))
		{
			healthLow = true;

			DoCastSelf(SPELL_DIVINE_SHIELD);

			scheduler
				.Schedule(1s, [this](TaskContext /*context*/)
				{
					CastSpellExtraArgs args;
					args.AddSpellBP0(me->GetMaxHealth());

					CastStop();
					DoCastSelf(SPELL_HEAL, args);
				})
				.Schedule(5min, [this](TaskContext /*context*/)
				{
					healthLow = false;
				});
		}
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		if (roll_chance_i(50))
			DoCastSelf(SPELL_AVENGING_WRATH);

		scheduler
            // Heal
            .Schedule(1s, 2s, [this](TaskContext holy_light)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 80))
				{
                    CastStop({ SPELL_HOLY_LIGHT, SPELL_EXARCH_BLADE, SPELL_HEAL });
					DoCast(target, SPELL_HOLY_LIGHT);
				}
				holy_light.Repeat(8s);
			})
			.Schedule(5s, 7s, [this](TaskContext light_of_dawn)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(15.0f, 50))
				{
                    CastStop({ SPELL_EXARCH_BLADE, SPELL_HEAL });
					DoCast(target, SPELL_LIGHT_OF_DAWN);
				}
				light_of_dawn.Repeat(10s, 15s);
			})
            // Utils
            .Schedule(5ms, [this](TaskContext rebuke)
			{
				if (Unit* target = DoSelectCastingUnit(SPELL_REBUKE, 35.f))
				{
                    CastStop({ SPELL_HOLY_LIGHT, SPELL_EXARCH_BLADE, SPELL_HEAL });
					DoCast(target, SPELL_REBUKE);
					rebuke.Repeat(25s, 40s);
				}
				else
				{
					rebuke.Repeat(1s);
				}
			})
            .Schedule(5ms, [this](TaskContext light_hammer)
            {
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                {
                    CastStop({ SPELL_EXARCH_BLADE, SPELL_HEAL });
                    DoCast(target, SPELL_LIGHT_HAMMER);
                }
                light_hammer.Repeat(30s);
            })
            // Damage
            .Schedule(20s, [this](TaskContext holy_shock)
            {
                uint32 mode = urand(0, 1);
                if (mode == 1)
                {
                    if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 90))
                    {
                        CastStop({ SPELL_EXARCH_BLADE, SPELL_HEAL });
                        DoCast(target, SPELL_HOLY_SHOCK);
                    }
                }
                else
                {
                    DoCastVictim(SPELL_HOLY_SHOCK);
                }
                holy_shock.Repeat(8s, 12s);
            })
            .Schedule(10s, 20s, [this](TaskContext exarch_blade)
            {
                DoCast(SPELL_EXARCH_BLADE);
                exarch_blade.Repeat(30s);
            })
            .Schedule(3s, 15s, [this](TaskContext consecrated_ground)
            {
                DoCast(SPELL_CONSECRATED_GROUND);
                consecrated_ground.Repeat(31s);
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
                if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
                {
                    CastStop({ SPELL_EXARCH_BLADE, SPELL_HEAL });
                    DoCast(target, SPELL_JUDGMENT);
                }
				judgment.Repeat(12s, 15s);
			});
	}
};

struct npc_theramore_footman : public npc_theramore_troop
{
	npc_theramore_footman(Creature* creature) : npc_theramore_troop(creature, AI_Type::Melee)
	{
	}

	enum Spells
	{
		SPELL_VIGILANT_STRIKE       = 260834,
		SPELL_WHIRLWIND             = 17207,
		SPELL_HAMMER_STUN           = 36138
	};

	void JustEngagedWith(Unit* /*who*/) override
	{
		scheduler
			.Schedule(5ms, [this](TaskContext hammer_stun)
			{
				if (Unit* target = DoSelectCastingUnit(SPELL_HAMMER_STUN, 35.f))
				{
					CastStop();
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

struct npc_theramore_arcanist : public npc_theramore_troop
{
	npc_theramore_arcanist(Creature* creature) : npc_theramore_troop(creature, AI_Type::Distance)
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
		npc_theramore_troop::Reset();

		scheduler.Schedule(1s, [this](TaskContext /*context*/)
		{
			DoCastSelf(SPELL_MAGE_ARMOR);
		});
	}

	void JustEngagedWith(Unit* /*who*/) override
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
					CastStop();
					DoCast(SPELL_ARCANE_EXPLOSION);
					arcane_explosion.Repeat(10s, 14s);
				}
				else
					arcane_explosion.Repeat(1s);
			})
			.Schedule(4s, 8s, [this](TaskContext arcane_missiles)
			{
				if (Unit* victim = SelectTarget(SelectTargetMethod::Random))
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

struct npc_theramore_faithful : public npc_theramore_troop
{
	npc_theramore_faithful(Creature* creature) : npc_theramore_troop(creature, AI_Type::Distance),
        ascension(false)
	{
	}

	enum Spells
	{
		SPELL_SMITE                 = 332705,
		SPELL_HALO                  = 120517,
		SPELL_FLASH_HEAL            = 314655,
		SPELL_RENEW                 = 294342,
		SPELL_PRAYER_OF_HEALING     = 266969,
		SPELL_POWER_WORD_FORTITUDE  = 267528,
		SPELL_POWER_WORD_SHIELD     = 318158,
		SPELL_PAIN_SUPPRESSION      = 69910,
		SPELL_PSYCHIC_SCREAM        = 65543,
	};

	bool ascension;

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
		if (!ascension && me->HealthBelowPctDamaged(10, damage))
		{
			ascension = true;

			scheduler.CancelAll();

			CastSpellExtraArgs args;
			args.AddSpellBP0(85000);

			CastStop();
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
		npc_theramore_troop::Reset();

		scheduler.Schedule(1s, 5s, [this](TaskContext fortitude)
		{
			BFTPhases phase = (BFTPhases)instance->GetData(DATA_SCENARIO_PHASE);
			if (phase < BFTPhases::HelpTheWounded)
			{              
                CastSpellExtraArgs args(true);
				args.SetTriggerFlags(TRIGGERED_IGNORE_SET_FACING);

				if (Unit* target = SelectRandomMissingBuff(SPELL_POWER_WORD_FORTITUDE))
					DoCast(target, SPELL_POWER_WORD_FORTITUDE, args);

				fortitude.Repeat(2s);
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
					args.AddSpellBP0(target->CountPctFromMaxHealth(20));

                    CastStop({ SPELL_RENEW, SPELL_FLASH_HEAL });
					DoCast(target, SPELL_POWER_WORD_SHIELD, args);
				}
				power_word_shield.Repeat(8s);
			})
			.Schedule(5s, 7s, [this](TaskContext renew)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 60))
					DoCast(target, SPELL_RENEW);
				renew.Repeat(10s, 15s);
			})
			.Schedule(12s, 14s, [this](TaskContext prayer_of_healing)
			{
				CastStop({ SPELL_RENEW, SPELL_FLASH_HEAL, SPELL_PRAYER_OF_HEALING });
				DoCastSelf(SPELL_PRAYER_OF_HEALING);
				prayer_of_healing.Repeat(14s);
			})
			.Schedule(1s, 3s, [this](TaskContext halo)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(30.f, 60))
				{
					CastStop(SPELL_HALO);
					DoCast(target, SPELL_HALO);
				}
                halo.Repeat(25s);
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
				if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 50))
					DoCast(target, SPELL_FLASH_HEAL);
				flash_heal.Repeat(2s);
			});
	}
};

struct npc_theramore_marksman : public npc_theramore_troop
{
    npc_theramore_marksman(Creature* creature) : npc_theramore_troop(creature, AI_Type::Hybrid)
    {
        Initialize();
    }

    enum Spells
    {
        SPELL_SHOOT                 = 22907,
        SPELL_MULTI_SHOOT           = 38310,
    };

    float GetDistance() override
    {
        return 30.0f;
    }

    void AttackStart(Unit* who) override
    {
        if (!who)
            return;

        if (who && me->Attack(who, true))
        {
            me->GetMotionMaster()->Clear(MOTION_PRIORITY_NORMAL);
            me->PauseMovement();
            me->SetCanMelee(false);
            me->SetSheath(SHEATH_STATE_RANGED);
        }
    }

    void MoveInLineOfSight(Unit* who) override
    {
        CustomAI::MoveInLineOfSight(who);

        if (!me->IsEngaged())
            return;

        if (me->IsInRange(who, 0.0f, me->GetCombatReach()))
        {
            if (me->CanMelee())
                return;

            me->SetCanMelee(true);
            me->SetSheath(SHEATH_STATE_MELEE);
        }
        else
        {
            SetCombatMove(false, GetDistance());
            me->SetCanMelee(false);
            me->SetSheath(SHEATH_STATE_RANGED);
        }
    }

    void JustEngagedWith(Unit* who) override
    {
        SetCombatMove(false, GetDistance());
        me->SetCanMelee(false);
        me->SetSheath(SHEATH_STATE_RANGED);

        DoCast(who, SPELL_SHOOT);

        scheduler
            .Schedule(2s, [this](TaskContext shoot)
            {
                DoCastVictim(SPELL_SHOOT);
                shoot.Repeat(2s);
            })
            .Schedule(15s, [this](TaskContext multi_shoot)
            {
                DoCastVictim(SPELL_MULTI_SHOOT);
                multi_shoot.Repeat(14s, 18s);
            });
    }
};

///
///     HORDE NPC
///

struct npc_theramore_horde : public CustomAI
{
	npc_theramore_horde(Creature* creature, AI_Type type) : CustomAI(creature, type)
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
                    me->KillSelf();
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

	void JustDied(Unit* killer) override
	{
		CustomAI::JustDied(killer);

		uint32 killCredit = me->GetCreatureTemplate()->KillCredit[0];
		if (Player* player = killer->ToPlayer())
			KillRewarder::Reward(player, me, killCredit);
	}

    bool CanAIAttack(Unit const* who) const override
    {
        if (who->GetEntry() == NPC_KALECGOS_DRAGON)
            return false;

        return CustomAI::CanAIAttack(who);
    }
};

struct npc_roknah_hag : public npc_theramore_horde
{
	npc_roknah_hag(Creature* creature) : npc_theramore_horde(creature, AI_Type::Distance),
		closeTarget(false), iceblock(false), index(0)
	{
	}

	enum Groups
	{
		GROUP_NORMAL,
		GROUP_FLEE,
		GROUP_FROSTBOLT
	};

	const uint32 IciclesDummies[5] =
	{
		214124,
		214125,
		214126,
		214127,
		214130
	};

	const uint32 IciclesProjectiles[5] =
	{
		148021,
		148020,
		148019,
		148018,
		148017
	};

	enum Spells
	{
		SPELL_BLINK             = 295236,
		SPELL_CONE_OF_COLD      = 292294,
		SPELL_EBONBOLT          = 284752,
		SPELL_FLURRY            = 284858,
		SPELL_FROST_NOVA        = 284879,
		SPELL_FROSTBOLT         = 284703,
		SPELL_GLACIAL_SPIKE     = 284840,
		SPELL_ICE_BLOCK         = 290049,
		SPELL_ICE_BARRIER       = 198094,
		SPELL_ICICLES           = 205473,
		SPELL_MASS_ICE_BARRIER  = 382561,
	};

	bool closeTarget;
	bool iceblock;
	uint8 index;

	void SpellHitTarget(WorldObject* /*object*/, SpellInfo const* spellInfo) override
	{
		switch (spellInfo->Id)
		{
			case SPELL_GLACIAL_SPIKE:
				me->RemoveAurasDueToSpell(SPELL_ICICLES);
				for (uint8 i = 0; i < 5; i++)
					me->RemoveAurasDueToSpell(IciclesDummies[i]);
				break;
			case SPELL_FROSTBOLT:
				if (Aura* aura = me->GetAura(SPELL_ICICLES))
				{
					uint8 stacks = aura->GetStackAmount();
					if (stacks < 5)
					{
						CastIcicle(stacks);
					}
					else
					{
						if (index >= 5) index = 0;
						DoCastVictim(IciclesProjectiles[index], true);
						index++;
					}
				}
				else
					CastIcicle(0);
				break;
		}
	}

    void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
    {
        if (!me->GetSpellHistory()->HasCooldown(SPELL_BLINK))
        {
            if (HasMechanic(spellInfo, MECHANIC_ROOT) || HasMechanic(spellInfo, MECHANIC_SNARE))
            {
                scheduler.Schedule(2s, [this](TaskContext /*blink*/)
                {
                    CastStop();
                    DoCastSelf(SPELL_BLINK);
                    me->RemoveMovementImpairingAuras(true);
                });
            }
        }
    }

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
        if (roll_chance_i(60))
        {
            DoCastSelf(SPELL_MASS_ICE_BARRIER);
            return;
        }

		if (!iceblock && me->HealthBelowPctDamaged(20, damage))
		{
			damage = 0;

			scheduler.DelayGroup(GROUP_NORMAL, 2s);
			scheduler.DelayGroup(GROUP_FROSTBOLT, 2s);

			iceblock = true;

			CastStop();

			DoCast(SPELL_ICE_BLOCK);

			scheduler.Schedule(1min, [this](TaskContext /*context*/)
			{
				iceblock = false;
			});

			CastFleeSequence(12s);
		}
	}

	void JustEngagedWith(Unit* who) override
	{
		npc_theramore_horde::JustEngagedWith(who);

        if (roll_chance_i(30))
		    DoCastSelf(SPELL_ICE_BARRIER);

		scheduler
			.Schedule(13s, 18s, GROUP_NORMAL, [this](TaskContext cone_of_cold)
			{
				if (EnemiesInRange(12.0f) > 2)
				{
					CastStop(SPELL_CONE_OF_COLD);
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
			.Schedule(1s, 3s, GROUP_NORMAL, [this](TaskContext glacial_spike)
			{
				if (Aura* aura = me->GetAura(SPELL_ICICLES))
				{
					uint8 stacks = aura->GetStackAmount();
					if (stacks == 5 && roll_chance_i(10))
					{
						CastStop(SPELL_GLACIAL_SPIKE);
						DoCastVictim(SPELL_GLACIAL_SPIKE);
						glacial_spike.Repeat(10s);
					}
					else
					{
						glacial_spike.Repeat(5ms);
					}
				}
				else
				{
					glacial_spike.Repeat(5ms);
				}
			})
			.Schedule(12s, 15s, GROUP_NORMAL, [this](TaskContext flurry)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random))
					DoCast(target, SPELL_FLURRY);
				flurry.Repeat(12s, 14s);
			})
			.Schedule(1ms, GROUP_FROSTBOLT, [this](TaskContext frostbolt)
			{
				DoCastVictim(SPELL_FROSTBOLT);
				frostbolt.Repeat(2s);
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
			scheduler.DelayGroup(GROUP_FROSTBOLT, 2s);
			CastStop();
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
					CastStop();
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

	void CastIcicle(uint8 index)
	{
		uint32 icicle = IciclesDummies[index];
		DoCastSelf(icicle, true);

		DoCastSelf(SPELL_ICICLES, true);
	}
};

struct npc_roknah_grunt : public npc_theramore_horde
{
	npc_roknah_grunt(Creature* creature) : npc_theramore_horde(creature, AI_Type::Melee)
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
		npc_theramore_horde::JustEngagedWith(who);

        if (!me->HasAura(SPELL_OVERPOWER) && roll_chance_i(60))
            DoCastSelf(SPELL_OVERPOWER);

		scheduler
			.Schedule(5ms, [this](TaskContext hammer_stun)
			{
				if (Unit* target = DoSelectCastingUnit(SPELL_HAMMER_STUN, 35.f))
				{
					CastStop();
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
                CastStop();
				DoCastVictim(SPELL_MORTAL_STRIKE);
				mortal_strike.Repeat(8s, 10s);
			})
			.Schedule(14s, 22s, [this](TaskContext rend)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_REND);
                rend.Repeat(8s, 10s);
			})
			.Schedule(32s, 38s, [this](TaskContext rend_slam)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, RAND(SPELL_REND, SPELL_SLAM));
				rend_slam.Repeat(2s, 8s);
			});
	}
};

struct npc_roknah_loasinger : public npc_theramore_horde
{
	npc_roknah_loasinger(Creature* creature) : npc_theramore_horde(creature, AI_Type::Distance)
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
		SPELL_HEALING_TIDE      = 127945,
	};

    enum Misc
    {
        NPC_HEALING_TIDE_TOTEM  = 65349,
    };

	const SpellInfo* flameShock;
	const SpellInfo* frostShock;

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
		if (me->HealthBelowPctDamaged(50, damage) && !me->HasAura(SPELL_ASTRAL_SHIFT))
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
		npc_theramore_horde::JustEngagedWith(who);

        DoCastVictim(SPELL_LIGHTNING_BOLT);

		scheduler
			.Schedule(8s, 14s, [this](TaskContext chain_lightning)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_CHAIN_LIGHTNING);
				chain_lightning.Repeat(3s, 5s);
			})
			.Schedule(5s, 8s, [this](TaskContext frost_shock)
			{
				if (Unit* target = DoFindEnemyMissingDot(frostShock))
					DoCast(target, SPELL_FROST_SHOCK);
				frost_shock.Repeat(8s, 10s);
			})
			.Schedule(5s, 8s, [this](TaskContext flame_shock)
			{
				if (Unit* target = DoFindEnemyMissingDot(flameShock))
					DoCast(target, SPELL_FLAME_SHOCK);
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
			.Schedule(3s, [this](TaskContext healing_surge)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 60))
				{
					CastStop(SPELL_HEALING_SURGE);
					DoCast(target, SPELL_HEALING_SURGE);
				}
				healing_surge.Repeat(3s);
			})
			.Schedule(5s, [this](TaskContext riptide)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 80))
				{
					if (!target->HasAura(SPELL_RIPTIDE))
						DoCast(target, SPELL_RIPTIDE);
				}
				riptide.Repeat(5s);
			})
			.Schedule(2s, [this](TaskContext healing_tide)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(60.f, 20))
				{
                    Creature* totem = me->FindNearestCreature(NPC_HEALING_TIDE_TOTEM, 60.f);
                    if (!totem)
                    {
                        CastStop();
                        DoCast(SPELL_HEALING_TIDE);
                        healing_tide.Repeat(1min);
                    }
                    else
                    {
                        healing_tide.Repeat(2s);
                    }
				}
				else
					healing_tide.Repeat(2s);
			})
			.Schedule(2s, [this](TaskContext chain_heal)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 50))
					DoCast(target, SPELL_CHAIN_HEAL);
				chain_heal.Repeat(2s);
			})
			.Schedule(11s, 15s, [this](TaskContext lava_burst)
			{
				DoCastVictim(SPELL_LAVA_BURST);
				lava_burst.Repeat(8s, 10s);
			})
			.Schedule(1s, [this](TaskContext wind_shear)
			{
				if (Unit* target = DoSelectCastingUnit(SPELL_WIND_SHEAR, 35.f))
				{
					CastStop();
					DoCast(target, SPELL_WIND_SHEAR);
					wind_shear.Repeat(10s, 18s);
				}
				else
					wind_shear.Repeat(1s);

			})
			.Schedule(1ms, [this](TaskContext lightning_bolt)
			{
				DoCastVictim(SPELL_LIGHTNING_BOLT);
				lightning_bolt.Repeat(2800ms);
			});
	}
};

struct npc_roknah_felcaster : public npc_theramore_horde
{
	npc_roknah_felcaster(Creature* creature) : npc_theramore_horde(creature, AI_Type::Distance)
	{
		immolateInfo = sSpellMgr->AssertSpellInfo(SPELL_IMMOLATE, DIFFICULTY_NONE);
		corruptionInfo = sSpellMgr->AssertSpellInfo(SPELL_CORRUPTION, DIFFICULTY_NONE);
	}

	enum NPCs
	{
		NPC_WILD_IMP            = 70071
	};

	enum Spells
	{
		SPELL_DRAIN_LIFE        = 149992,
		SPELL_CONFLAGRATE       = 295418,
		SPELL_CHAOS_BOLT        = 295420,
		SPELL_IMMOLATE          = 295425,
		SPELL_INCINERATE        = 295438,
		SPELL_MORTAL_COIL       = 295459,
		SPELL_SUMMON_FELHUNTER  = 285232,
		SPELL_SUMMON_WILD_IMPS  = 138685,
		SPELL_CORRUPTION        = 251406,
	};

	const SpellInfo* immolateInfo;
	const SpellInfo* corruptionInfo;

	float GetDistance() override
	{
		return 18.f;
	}

	void JustSummoned(Creature* summon) override
	{
		CustomAI::JustSummoned(summon);

		if (summon->GetEntry() == NPC_WILD_IMP)
		{
			if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
			{
				summon->Attack(target, true);
			}
		}
	}

	void JustEngagedWith(Unit* who) override
	{
		npc_theramore_horde::JustEngagedWith(who);

		if (roll_chance_i(60))
			DoCastSelf(RAND(SPELL_SUMMON_FELHUNTER, SPELL_SUMMON_WILD_IMPS));

		scheduler
			.Schedule(5s, 8s, [this](TaskContext drain_life)
			{
				if (HealthBelowPct(30))
				{
					if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
					{
						CastStop(SPELL_DRAIN_LIFE);
						DoCast(target, SPELL_DRAIN_LIFE);
						drain_life.Repeat(8s, 15s);
					}
				}
				else
					drain_life.Repeat(1s);
			})
			.Schedule(2s, 6s, [this](TaskContext conflagrate)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_CONFLAGRATE);
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
					DoCast(target, SPELL_IMMOLATE);
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
				if (HealthBelowPct(20))
				{
					if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					{
						CastStop(SPELL_DRAIN_LIFE);
						DoCast(target, SPELL_MORTAL_COIL);
						mortal_coil.Repeat(25s, 45s);
					}
				}
				else
					mortal_coil.Repeat(1s);
			});
	}
};

///
///     COSMETIC
///

struct npc_faithful_training : public npc_theramore_faithful
{
    npc_faithful_training(Creature* creature) : npc_theramore_faithful(creature),
        soldierA(nullptr), soldierB(nullptr)
    {
    }

    enum Misc
	{
        // Cosmetic
        COSMETIC_GROUP,

		// Spells
		SPELL_POWER_WORD_SHIELD         = 318158,
		SPELL_FLASH_HEAL                = 314655,
		SPELL_HEAL                      = 332706,
	};

    Creature* soldierA;
    Creature* soldierB;

    void SetState(Creature* creature, Emote emote, Creature* target)
    {
        creature->SetEmoteState(emote);

        uint64 health = static_cast<uint64>(creature->GetMaxHealth()) * 0.3f;
        creature->SetRegenerateHealth(false);
        creature->SetHealth(health);
        creature->SetTarget(target ? target->GetGUID() : ObjectGuid::Empty);
    }

    void ClearState(Creature* creature)
    {
        creature->SetEmoteState(EMOTE_STATE_NONE);
        creature->SetRegenerateHealth(true);
        creature->SetHealth(creature->GetMaxHealth());
        creature->SetTarget(ObjectGuid::Empty);
    }

    void Reset() override
    {
        npc_theramore_faithful::Reset();

        BFTPhases phase = (BFTPhases)instance->GetData(DATA_SCENARIO_PHASE);
        if (phase > BFTPhases::Preparation)
            return;

        std::vector<Creature*> soldiers;
        me->GetCreatureListWithEntryInGrid(soldiers, NPC_THERAMORE_FOOTMAN, 15.0f);
        if (soldiers.size() <= 0)
            return;

        soldierA = soldiers[0];
        soldierB = soldiers[1];

        if (!soldierA && !soldierB)
            return;

        SetState(soldierA, EMOTE_STATE_ATTACK1H, soldierB);
        SetState(soldierB, EMOTE_STATE_BLOCK_SHIELD, soldierA);

        soldierA->SetReactState(REACT_PASSIVE);
        soldierB->SetReactState(REACT_PASSIVE);

        me->SetReactState(REACT_PASSIVE);

        scheduler
            .Schedule(5s, COSMETIC_GROUP, [this](TaskContext check_phase)
            {
                BFTPhases phase = (BFTPhases)instance->GetData(DATA_SCENARIO_PHASE);
                if (phase >= BFTPhases::Preparation)
                {
                    ClearState(soldierA);
                    ClearState(soldierB);

                    soldierA->SetReactState(REACT_AGGRESSIVE);
                    soldierB->SetReactState(REACT_AGGRESSIVE);

                    me->SetReactState(REACT_AGGRESSIVE);

                    scheduler.CancelGroup(COSMETIC_GROUP);
                }

                check_phase.Repeat(2s);
            })
            .Schedule(5s, 8s, COSMETIC_GROUP, [this](TaskContext heal)
            {
                if (Creature* victim = RAND(soldierA, soldierB))
                    me->CastSpell(victim, RAND(SPELL_FLASH_HEAL, SPELL_HEAL, SPELL_POWER_WORD_SHIELD));
                heal.Repeat(5s, 15s);
            })
            .Schedule(5s, 8s, COSMETIC_GROUP,[this](TaskContext soldiers)
            {
                if (!soldierA->HasAura(SPELL_POWER_WORD_SHIELD))
                {
                    soldierB->DealDamage(soldierA, soldierB, urand(1000, 1500));
                }

                if (!soldierB->HasAura(SPELL_POWER_WORD_SHIELD))
                {
                    soldierA->DealDamage(soldierB, soldierA, urand(1000, 1500));
                }

                soldiers.Repeat(2s);
            });
    }
};

struct npc_arcanist_training : public npc_theramore_arcanist
{
    npc_arcanist_training(Creature* creature) : npc_theramore_arcanist(creature)
	{
	}

	enum Misc
	{
        // Group
        COSMETIC_GROUP,

		// Spells
		SPELL_ARCANE_PROJECTILES        = 5143,
		SPELL_SUPERNOVA                 = 157980,
		SPELL_EVOCATION                 = 243070,
		SPELL_ARCANE_BLAST              = 291316,
		SPELL_ARCANE_BARRAGE            = 291318,
	};

	void Reset() override
	{
        npc_theramore_arcanist::Reset();

        BFTPhases phase = (BFTPhases)instance->GetData(DATA_SCENARIO_PHASE);
        if (phase > BFTPhases::Preparation)
            return;

        scheduler
            .Schedule(5s, COSMETIC_GROUP, [this](TaskContext check_phase)
            {
                BFTPhases phase = (BFTPhases)instance->GetData(DATA_SCENARIO_PHASE);
                if (phase >= BFTPhases::Preparation)
                {
                    scheduler.CancelGroup(COSMETIC_GROUP);
                }

                check_phase.Repeat(2s);
            })
            .Schedule(5s, 8s, COSMETIC_GROUP, [this](TaskContext context)
            {
                Creature* training = GetClosestCreatureWithEntry(me, NPC_TRAINING_DUMMY, 15.f);
                if (!training)
                    return;

                if (me->GetPowerPct(POWER_MANA) <= 20)
                {
                    if (Spell* spell = me->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
                    {
                        if (spell->getState() != SPELL_STATE_FINISHED && spell->IsChannelActive())
                        {
                            context.Repeat(2s);
                        }
                    }
                    else
                    {
                        const SpellInfo* info = sSpellMgr->AssertSpellInfo(SPELL_EVOCATION, DIFFICULTY_NONE);
                        Milliseconds ms = Milliseconds(info->CalcDuration());
                        CastSpellExtraArgs args(TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD);

                        me->CastSpell(me, SPELL_EVOCATION, args);
                        me->GetSpellHistory()->RestoreCharge(info->ChargeCategoryId);

                        context.Repeat(ms + 800ms);
                    }
                }
                else
                {
                    Milliseconds ms = 50ms;
                    if (!me->HasUnitState(UNIT_STATE_CASTING))
                    {
                        uint32 spellId = SPELL_ARCANE_BLAST;
                        if (roll_chance_i(30))
                        {
                            spellId = SPELL_ARCANE_PROJECTILES;
                        }
                        else if (roll_chance_i(20))
                        {
                            spellId = SPELL_ARCANE_BARRAGE;
                        }
                        else if (roll_chance_i(10))
                        {
                            spellId = SPELL_SUPERNOVA;
                        }

                        const SpellInfo* info = sSpellMgr->AssertSpellInfo(spellId, DIFFICULTY_NONE);
                        ms = Milliseconds(info->CalcCastTime());

                        me->CastSpell(training, spellId);
                        me->GetSpellHistory()->RestoreCharge(info->ChargeCategoryId);

                        if (info->IsChanneled())
                            ms = Milliseconds(info->CalcDuration(me));
                    }

                    context.Repeat(ms + 500ms);
                }
            });
	}
};

// Healing Tide Totem - 65349
struct npc_healing_tide_totem : public TotemAI
{
	npc_healing_tide_totem(Creature* creature) : TotemAI(creature)
	{
		Initialize();
	}

	enum Spells
	{
		SPELL_HEALING_TIDE_TOTEM_DUMMY      = 114941,
		SPELL_HEALING_TIDE_TOTEM_HEAL       = 114942,
	};

	void Initialize()
	{
		scheduler.SetValidator([this]
		{
			return me->ToTotem()->GetTotemType() != TOTEM_ACTIVE || !me->IsAlive() || me->IsNonMeleeSpellCast(false);
		});
	}

	void Reset() override
	{
		DoCastSelf(SPELL_HEALING_TIDE_TOTEM_DUMMY, true);

		DoCast(SPELL_HEALING_TIDE_TOTEM_HEAL);

		scheduler.Schedule(2s, [this](TaskContext spell)
		{
			DoCast(SPELL_HEALING_TIDE_TOTEM_HEAL);
			spell.Repeat(2s);
		});
	}

	void UpdateAI(uint32 diff) override
	{
		scheduler.Update(diff);
	}

	private:
	TaskScheduler scheduler;
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
			caster->CastSpell(target, SPELL_LIGHT_OF_DAWN, true);
		}
	}

	void Register() override
	{
		OnEffectHitTarget += SpellEffectFn(spell_theramore_light_of_dawn::HandleDummy, EFFECT_1, SPELL_EFFECT_DUMMY);
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

			#ifdef CUSTOM_DEBUG
				for (uint8 i = 0; i < NUMBER_OF_FIRES; ++i)
				{
					if (Player* player = caster->ToPlayer())
						KillRewarder::Reward(player, caster, NPC_THERAMORE_FIRE_CREDIT);
				}
			#else
				if (Creature* trigger = caster->SummonCreature(WORLD_TRIGGER, destination->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, 5s))
				{
					std::list<Creature*> fires;
					trigger->GetCreatureListWithEntryInGrid(fires, NPC_THERAMORE_FIRE_CREDIT, radius);

					for (Creature* fire : fires)
					{
						if (Player* player = caster->ToPlayer())
						{
							KillRewarder::Reward(player, caster, NPC_THERAMORE_FIRE_CREDIT);
						}

						fire->DespawnOrUnsummon();
					}
				}
			#endif
		}
	}

	void Register() override
	{
		OnEffectHitTarget += SpellEffectFn(spell_theramore_throw_bucket::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
	}
};

// Blizzard - 284968
// AreaTriggerID - 15411
struct at_blizzard_theramore : AreaTriggerAI
{
	static constexpr Milliseconds TICK_PERIOD = Milliseconds(1000);

	at_blizzard_theramore(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger), _tickTimer(TICK_PERIOD)
	{
	}

	enum Spells
	{
		SPELL_BLIZZARD_DAMAGE   = 335953
	};

	void OnUpdate(uint32 diff) override
	{
		_tickTimer -= Milliseconds(diff);

		while (_tickTimer <= 0s)
		{
			if (Unit* caster = at->GetCaster())
			{
				for (ObjectGuid unit : at->GetInsideUnits())
				{
					if (Unit* target = ObjectAccessor::GetUnit(*caster, unit))
					{
						if (!caster->IsHostileTo(target))
							continue;

						caster->CastSpell(target, SPELL_BLIZZARD_DAMAGE);
					}
				}
			}

			_tickTimer += TICK_PERIOD;
		}
	}

	private:
	Milliseconds _tickTimer;
};

// Rune of Alacrity
// AreaTriggerID - 26613
struct at_rune_alacrity : AreaTriggerAI
{
	at_rune_alacrity(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger)
	{
	}

	enum Spells
	{
		SPELL_RUNE_OF_ALACRITY = 388334
	};

	void OnUnitEnter(Unit* /*unit*/) override
	{
		if (Unit* caster = at->GetCaster())
		{
			for (ObjectGuid unit : at->GetInsideUnits())
			{
				if (Unit* target = ObjectAccessor::GetUnit(*caster, unit))
				{
					if (caster->IsHostileTo(target))
						continue;

					target->CastSpell(target, SPELL_RUNE_OF_ALACRITY);
				}
			}
		}
	}

	void OnUnitExit(Unit* unit) override
	{
		unit->RemoveAurasDueToSpell(SPELL_RUNE_OF_ALACRITY);
	}

	void OnRemove() override
	{
		if (Unit* caster = at->GetCaster())
		{
			for (ObjectGuid unit : at->GetInsideUnits())
			{
				if (Unit* target = ObjectAccessor::GetUnit(*caster, unit))
				{
					target->RemoveAurasDueToSpell(SPELL_RUNE_OF_ALACRITY);
				}
			}
		}
	}
};

// Consecrated Ground
// AreaTriggerID - 13272
struct at_consecrated_ground : AreaTriggerAI
{
    at_consecrated_ground(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger)
	{
	}

	enum Spells
	{
		SPELL_CONSECRATED_GROUND = 268923
	};

	void OnUnitEnter(Unit* /*unit*/) override
	{
		if (Unit* caster = at->GetCaster())
		{
			for (ObjectGuid unit : at->GetInsideUnits())
			{
				if (Unit* target = ObjectAccessor::GetUnit(*caster, unit))
				{
					if (caster->IsFriendlyTo(target))
						continue;

					target->CastSpell(target, SPELL_CONSECRATED_GROUND);
				}
			}
		}
	}

	void OnUnitExit(Unit* unit) override
	{
		unit->RemoveAurasDueToSpell(SPELL_CONSECRATED_GROUND);
	}

	void OnRemove() override
	{
		if (Unit* caster = at->GetCaster())
		{
			for (ObjectGuid unit : at->GetInsideUnits())
			{
				if (Unit* target = ObjectAccessor::GetUnit(*caster, unit))
				{
					target->RemoveAurasDueToSpell(SPELL_CONSECRATED_GROUND);
				}
			}
		}
	}
};

// Uncontrolled Energy
// AreaTriggerID - 26658
struct at_uncontrolled_energy : AreaTriggerAI
{
	at_uncontrolled_energy(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger)
	{
	}

	enum Spells
	{
		SPELL_ARCANE_RIFT_EXPLOSION = 388996
	};

	void OnUnitEnter(Unit* unit) override
	{
		if (Unit* caster = at->GetCaster())
		{
			if (!caster->IsHostileTo(unit))
				return;

			caster->CastSpell(at->GetPosition(), SPELL_ARCANE_RIFT_EXPLOSION, true);
			at->Remove();
		}
	}
};

// Aracane Rift - 388902
// AreaTriggerID - 26656
struct at_arcane_rift : AreaTriggerAI
{
	static constexpr Milliseconds TICK_PERIOD = Milliseconds(1000);

	at_arcane_rift(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger), _tickTimer(TICK_PERIOD)
	{
	}

	enum Spells
	{
		SPELL_ARCANE_RIFT_EXPLOSION = 388996
	};

	void OnUpdate(uint32 diff) override
	{
		_tickTimer -= Milliseconds(diff);

		while (_tickTimer <= 0s)
		{
			if (Unit* caster = at->GetCaster())
			{
				for (ObjectGuid unit : at->GetInsideUnits())
				{
					if (Unit* target = ObjectAccessor::GetUnit(*caster, unit))
					{
						if (!caster->IsHostileTo(target))
							continue;

						caster->CastSpell(target->GetPosition(), SPELL_ARCANE_RIFT_EXPLOSION, true);
					}
				}
			}

			_tickTimer += TICK_PERIOD;
		}
	}

	private:
	Milliseconds _tickTimer;
};

// Scorched Earth - 373139
// AreaTriggerID - 25183
struct at_scorched_earth : AreaTriggerAI
{
	at_scorched_earth(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger)
	{
	}

	enum Spells
	{
		SPELL_SCORCHED_EARTH = 372820
	};

	void OnUnitEnter(Unit* unit) override
	{
		if (Unit* caster = at->GetCaster())
		{
			if (caster->IsHostileTo(unit))
			{
				unit->AddAura(SPELL_SCORCHED_EARTH, unit);
			}
		}
	}

	void OnUnitExit(Unit* unit) override
	{
		unit->RemoveAurasDueToSpell(SPELL_SCORCHED_EARTH);
	}
};

void AddSC_npcs_battle_for_theramore()
{
	RegisterTheramoreAI(npc_theramore_citizen);
	RegisterTheramoreAI(npc_thader_windermere);
	RegisterTheramoreAI(npc_hedric_evencane);
	RegisterTheramoreAI(npc_unmanned_tank);
	RegisterTheramoreAI(npc_theramore_officier);
	RegisterTheramoreAI(npc_theramore_footman);
	RegisterTheramoreAI(npc_theramore_faithful);
	RegisterTheramoreAI(npc_theramore_arcanist);
	RegisterTheramoreAI(npc_theramore_marksman);
	RegisterTheramoreAI(npc_wounded_theramore_troop);
    RegisterTheramoreAI(npc_faithful_training);
    RegisterTheramoreAI(npc_arcanist_training);

	// Utilisables dans les Ruines de Theramore
	RegisterCreatureAI(npc_roknah_hag);
	RegisterCreatureAI(npc_roknah_grunt);
	RegisterCreatureAI(npc_roknah_loasinger);
	RegisterCreatureAI(npc_roknah_felcaster);
	RegisterCreatureAI(npc_healing_tide_totem);

	RegisterSpellScript(spell_theramore_light_of_dawn);
	RegisterSpellScript(spell_theramore_throw_bucket);

	RegisterAreaTriggerAI(at_blizzard_theramore);
	RegisterAreaTriggerAI(at_rune_alacrity);
	RegisterAreaTriggerAI(at_consecrated_ground);
	RegisterAreaTriggerAI(at_uncontrolled_energy);
	RegisterAreaTriggerAI(at_arcane_rift);
	RegisterAreaTriggerAI(at_scorched_earth);
}
