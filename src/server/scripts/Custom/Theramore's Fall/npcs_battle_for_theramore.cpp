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
#include "World.h"
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

struct npc_unmanned_tank : public CustomAI
{
	npc_unmanned_tank(Creature* creature) : CustomAI(creature, true, AI_Type::Stay)
	{
	}

	enum Spells
	{
		SPELL_DEMOLISHER_CANNON = 271246
	};

	void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
	{
		if (spellInfo->Id != SPELL_REPAIR)
			return;

		me->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		scheduler.Schedule(1s, [this](TaskContext canon)
		{
			if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				DoCast(target, SPELL_DEMOLISHER_CANNON);
			canon.Repeat(10s, 24s);
		});
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
			#ifdef CUSTOM_DEBUG
				for (uint8 i = 0; i < NUMBER_OF_WOUNDED; i++)
					KillRewarder::Reward(player, me, NPC_THERAMORE_WOUNDED_TROOP);
			#else
				KillRewarder::Reward(player, me, NPC_THERAMORE_WOUNDED_TROOP);
			#endif
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
	npc_theramore_troop(Creature* creature, AI_Type type) : CustomAI(creature, true, type), emoteReceived(false)
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

	void JustEngagedWith(Unit* /*who*/) override
	{
		me->CallAssistance();
	}

	void SetData(uint32 id, uint32 value) override
	{
		if (id == NPC_THERAMORE_TROOPS_CREDIT)
		{
			emoteReceived = value ? true : false;
		}
	}

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
						std::list<Creature*> troops;
						me->GetCreatureListWithEntryInGrid(troops, NPC_THERAMORE_FOOTMAN, 8.f);
						me->GetCreatureListWithEntryInGrid(troops, NPC_THERAMORE_FAITHFUL, 8.f);
						me->GetCreatureListWithEntryInGrid(troops, NPC_THERAMORE_ARCANIST, 8.f);
						me->GetCreatureListWithEntryInGrid(troops, NPC_THERAMORE_OFFICER, 8.f);
						me->GetCreatureListWithEntryInGrid(troops, NPC_THERAMORE_MARKSMAN, 8.f);

						for (Creature* troop : troops)
						{
							float orientation = troop->GetOrientation();
							scheduler.Schedule(2ms, 8ms, [troop, orientation, player, this](TaskContext context)
							{
								switch (context.GetRepeatCounter())
								{
									case 0:
										troop->SetFacingToObject(player);
										context.Repeat(1s);
										break;
									case 1:
										troop->PlayDirectSound(soundEmote, player);
										troop->HandleEmoteCommand(EMOTE_ONESHOT_CHEER_FORTHEALLIANCE);
										troop->AI()->SetData(NPC_THERAMORE_TROOPS_CREDIT, 1);
										KillRewarder::Reward(player, troop, NPC_THERAMORE_TROOPS_CREDIT);
										context.Repeat(3s);
										break;
									case 2:
										troop->SetFacingTo(orientation);
										break;
								}

							});
						}
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

	npc_thader_windermere(Creature* creature) : CustomAI(creature, true)
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
							if (Creature* kinndy = instance->GetCreature(DATA_KINNDY_SPARKSHINE))
								kinndy->CastSpell(kinndy, SPELL_PORTAL_CHANNELING_01);
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
	npc_hedric_evencane(Creature* creature) : CustomAI(creature, true, AI_Type::Melee)
	{
	}

	enum Spells
	{
		SPELL_BATTER                = 66408,
		SPELL_RISING_ANGER          = 136323,
		SPELL_MORTAL_CLEAVE         = 177147,
		SPELL_WHIRLWIND             = 277637,
		SPELL_HEW                   = 319957,
		SPELL_VICIOUS_WOUND         = 334960
	};

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
		SPELL_DIVINE_STORM          = 444705,
		SPELL_HEAL                  = 225638,
		SPELL_EXARCH_BLADE          = 268742,
		SPELL_AVENGING_WRATH        = 292266,
		SPELL_CRUSADER_STRIKE       = 295670,
		SPELL_HOLY_LIGHT            = 295698,
		SPELL_JUDGMENT              = 295671,
		SPELL_LIGHT_OF_DAWN         = 295710,
		SPELL_BLESSING_OF_FREEDOM   = 299256,
		SPELL_REBUKE                = 405397,
		SPELL_CONSECRATION          = 424429,
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

	void DamageTaken(Unit* attacker, uint32& damage, DamageEffectType damageType, SpellInfo const* spellInfo) override
	{
		npc_theramore_troop::DamageTaken(attacker, damage, damageType, spellInfo);

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

	void JustEngagedWith(Unit* who) override
	{
		npc_theramore_troop::JustEngagedWith(who);

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
			.Schedule(3s, 15s, [this](TaskContext consecration)
			{
				DoCast(SPELL_CONSECRATION);
				consecration.Repeat(31s);
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

	void JustEngagedWith(Unit* who) override
	{
		npc_theramore_troop::JustEngagedWith(who);

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
		castSpellArcaneTempo.SetTriggerFlags(TRIGGERED_FULL_MASK & ~(TRIGGERED_IGNORE_POWER_COST | TRIGGERED_IGNORE_REAGENT_COST));
		castSpellArcaneTempo.AddSpellBP0(2U);
	}

	enum Spells
	{
		SPELL_MAGE_ARMOR        = 183079,
		SPELL_ARCANE_BLAST      = 291336,
		SPELL_ARCANE_MISSILES   = 314734,
		SPELL_MASS_POLYMORPH    = 383121,
		SPELL_ARCANE_TEMPO      = 383997,
		SPELL_ARCANE_EXPLOSION  = 414381,
		SPELL_ARCANE_SPLINTER   = 443763
	};

	CastSpellExtraArgs castSpellArcaneTempo;

	void Reset() override
	{
		npc_theramore_troop::Reset();

		scheduler.Schedule(1s, [this](TaskContext /*context*/)
		{
			DoCastSelf(SPELL_MAGE_ARMOR);
		});
	}

	void SpellHitTarget(WorldObject* /*object*/, SpellInfo const* spellInfo) override
	{
		// Ne réagit qu'au sort Arcane Blast
		if (spellInfo->Id != SPELL_ARCANE_BLAST)
			return;

		// Vérifie si le buff Arcane Tempo est actif
		if (Aura* aura = me->GetAura(SPELL_ARCANE_TEMPO))
		{
			// Si moins de 5 stacks, on applique à nouveau le buff
			if (aura->GetStackAmount() < 5)
				me->CastSpell(me, SPELL_ARCANE_TEMPO, castSpellArcaneTempo);
		}
		else
		{
			// Buff non présent, on le lance
			me->CastSpell(me, SPELL_ARCANE_TEMPO, castSpellArcaneTempo);
		}

		// Nombre de projectiles à lancer aléatoirement entre 1 et 8
		const uint8 splinters = irand(1, 5);

		// Planifie une rafale de projectiles espacés
		scheduler.Schedule(1ms, [this, splinters](TaskContext context)
		{
			const uint8 index = context.GetRepeatCounter();

			if (index >= splinters)
				return;

			// Cible aléatoire et lancement du sort
			if (Unit* victim = SelectTarget(SelectTargetMethod::Random))
				DoCast(victim, SPELL_ARCANE_SPLINTER, false);

			// Replanifie avec un délai aléatoire entre 380ms et 560ms
			context.Repeat(380ms, 560ms);
		});
	}

	void JustEngagedWith(Unit* who) override
	{
		npc_theramore_troop::JustEngagedWith(who);

		scheduler
			.Schedule(1ms, [this](TaskContext arcane_blast)
			{
				DoCastVictim(SPELL_ARCANE_BLAST);
				arcane_blast.Repeat(2300ms);
			})
			.Schedule(3s, 5s, [this](TaskContext mass_polymorph)
			{
				if (EnemiesInRange(10.f) >= 4)
				{
					CastStop();
					DoCastSelf(SPELL_MASS_POLYMORPH);
					mass_polymorph.Repeat(2min);
				}
				else
					mass_polymorph.Repeat(1s);
			})
			.Schedule(8s, 10s, [this](TaskContext arcane_explosion)
			{
				if (EnemiesInRange(10.f) >= 2)
				{
					CastStop();
					DoCastSelf(SPELL_ARCANE_EXPLOSION);
					arcane_explosion.Repeat(2min);
				}
				else
					arcane_explosion.Repeat(1s);
			})
			.Schedule(4s, 8s, [this](TaskContext arcane_missiles)
			{
				if (Unit* victim = SelectTarget(SelectTargetMethod::Random))
					DoCast(victim, SPELL_ARCANE_MISSILES);
				arcane_missiles.Repeat(8s, 12s);
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
		SPELL_PSYCHIC_SCREAM        = 65543,
		SPELL_PAIN_SUPPRESSION      = 69910,
		SPELL_HALO                  = 120517,
		SPELL_PRAYER_OF_HEALING     = 266969,
		SPELL_POWER_WORD_FORTITUDE  = 267528,
		SPELL_RENEW                 = 294342,
		SPELL_FLASH_HEAL            = 314655,
		SPELL_POWER_WORD_SHIELD     = 318158,
		SPELL_SMITE                 = 332705,
		SPELL_DIVINE_WORD_SANTUARY  = 372784,
	};

	bool ascension;

	void DamageTaken(Unit* attacker, uint32& damage, DamageEffectType damageType, SpellInfo const* spellInfo) override
	{
		npc_theramore_troop::DamageTaken(attacker, damage, damageType, spellInfo);

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

	void JustEngagedWith(Unit* who) override
	{
		npc_theramore_troop::JustEngagedWith(who);

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
			.Schedule(12s, 14s, [this](TaskContext mass_healing)
			{
				CastStop({ SPELL_RENEW, SPELL_FLASH_HEAL, SPELL_PRAYER_OF_HEALING });

				if (roll_chance_i(60))
				{
					DoCastAOE(SPELL_PRAYER_OF_HEALING);
					mass_healing.Repeat(14s);
				}
				else
				{
					if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 60))
					{
						DoCast(SPELL_DIVINE_WORD_SANTUARY);
						mass_healing.Repeat(25s, 45s);
					}
					else
					{
						mass_healing.Repeat(1s);
					}
				}
			})
			.Schedule(1s, 5s, [this](TaskContext halo)
			{
				CastStop(SPELL_HALO);
				DoCastAOE(SPELL_HALO);
				halo.Repeat(14s, 25s);
			})
			.Schedule(3s, 8s, [this](TaskContext psychic_scream)
			{
				if (EnemiesInRange(10.f) >= 2)
				{
					DoCastAOE(SPELL_PSYCHIC_SCREAM);
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

	void JustEngagedWith(Unit* who) override
	{
		npc_theramore_troop::JustEngagedWith(who);

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
	npc_theramore_horde(Creature* creature, AI_Type type) : CustomAI(creature, true, type)
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
		SPELL_FROSTBOLT         = 116,
		SPELL_BLINK             = 295236,
		SPELL_CONE_OF_COLD      = 292294,
		SPELL_EBONBOLT          = 284752,
		SPELL_FLURRY            = 284858,
		SPELL_FROST_NOVA        = 284879,
		SPELL_GLACIAL_SPIKE     = 284840,
		SPELL_ICE_BLOCK         = 278960,
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

	void DamageTaken(Unit* attacker, uint32& damage, DamageEffectType damageType, SpellInfo const* spellInfo) override
	{
		npc_theramore_horde::DamageTaken(attacker, damage, damageType, spellInfo);

		if (roll_chance_i(30))
		{
			DoCastSelf(SPELL_MASS_ICE_BARRIER);
		}

		if (!iceblock && HealthBelowPct(20))
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
	npc_roknah_grunt(Creature* creature) : npc_theramore_horde(creature, AI_Type::Melee),
		commandingShout(false), slayerStrikeCount(0)
	{
	}

	enum Spells
	{
		SPELL_CHARGE            = 100,
		SPELL_MORTAL_STRIKE     = 32736,
		SPELL_HEROIC_STRIKE     = 57846,
		SPELL_BATTLE_SHOUT      = 81219,
		SPELL_COMMANDING_SHOUT  = 82061,
		SPELL_EXECUTE           = 260798,
		SPELL_SUDDEN_DEATH      = 280776,
		SPELL_SLAYER_STRIKE     = 445579,
		SPELL_PLUMMEL           = 457982,
		SPELL_DEEP_WOUNDS       = 458010,
		SPELL_SLAM              = 458028,
		SPELL_WHIRLWIND         = 1217875,
	};

	bool commandingShout;
	uint8 slayerStrikeCount;

	void Reset() override
	{
		npc_theramore_horde::Reset();

		commandingShout = false;
		slayerStrikeCount = 0;
	}

	void DamageTaken(Unit* /*attacker*/, uint32& /*damage*/, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
	{
		if (!commandingShout && HealthBelowPct(50))
		{
			DoCast(SPELL_COMMANDING_SHOUT);

			commandingShout = true;
			scheduler.Schedule(3min, [this](TaskContext /*context*/)
			{
				commandingShout = false;
			});
		}
	}

	void JustEngagedWith(Unit* who) override
	{
		npc_theramore_horde::JustEngagedWith(who);

		DoCast(SPELL_BATTLE_SHOUT);

		scheduler
			.Schedule(1ms, [this](TaskContext charge)
			{
				Unit* victim = me->GetVictim();
				if (victim && victim->IsWithinDist(me, 25.0f, false))
				{
					CastStop();
					DoCast(victim, SPELL_CHARGE);
					charge.Repeat(1min);
				}
				else
				{
					charge.Repeat(5s, 10s);
				}
			})
			.Schedule(2s, [this](TaskContext plummel)
			{
				if (Unit* target = DoSelectCastingUnit(SPELL_PLUMMEL, 5.0f))
				{
					DoCast(target, SPELL_PLUMMEL);
					plummel.Repeat(25s, 40s);
				}
				else
				{
					plummel.Repeat(1s);
				}
			})
			.Schedule(3s, 5s, [this](TaskContext slam)
			{
				DoCastVictim(SPELL_SLAM);
				slam.Repeat(15s, 28s);
			})
			.Schedule(8s, 24s, [this](TaskContext whirlwind)
			{
				DoCastSelf(SPELL_WHIRLWIND);
				whirlwind.Repeat(8s, 12s);
			})
			.Schedule(10s, 21s, [this](TaskContext slayer_strike)
			{
				if (slayerStrikeCount < 3)
				{
					DoCastVictim(SPELL_SLAYER_STRIKE);
					DoCastSelf(SPELL_SUDDEN_DEATH);
					slayerStrikeCount++;
					slayer_strike.Repeat(8s, 12s);
				}
				else
				{
					DoCastVictim(SPELL_EXECUTE);
					slayerStrikeCount = 0;
					slayer_strike.Repeat(3s, 5s);
				}
			})
			.Schedule(5s, 15s, [this](TaskContext strikes)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, RAND(SPELL_MORTAL_STRIKE, SPELL_HEROIC_STRIKE));
				strikes.Repeat(8s, 25s);
			})
			.Schedule(14s, 22s, [this](TaskContext deep_wounds)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_DEEP_WOUNDS);
				deep_wounds.Repeat(24s, 32s);
			});
	}
};

struct npc_roknah_loasinger : public npc_theramore_horde
{
	npc_roknah_loasinger(Creature* creature) : npc_theramore_horde(creature, AI_Type::Distance)
	{
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

	void DamageTaken(Unit* attacker, uint32& damage, DamageEffectType damageType, SpellInfo const* spellInfo) override
	{
		npc_theramore_horde::DamageTaken(attacker, damage, damageType, spellInfo);

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
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 30.0f, false, true, -SPELL_FROST_SHOCK))
					DoCast(target, SPELL_FROST_SHOCK);
				frost_shock.Repeat(8s, 10s);
			})
			.Schedule(5s, 8s, [this](TaskContext flame_shock)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 30.0f, false, true, -SPELL_FLAME_SHOCK))
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
				if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 40))
				{
					CastStop(SPELL_HEALING_SURGE);
					DoCast(target, SPELL_HEALING_SURGE);
				}
				healing_surge.Repeat(3s);
			})
			.Schedule(5s, [this](TaskContext riptide)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 60))
				{
					if (!target->HasAura(SPELL_RIPTIDE))
						DoCast(target, SPELL_RIPTIDE);
				}
				riptide.Repeat(5s);
			})
			.Schedule(2s, [this](TaskContext healing_tide)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(60.f, 5))
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
				if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 30))
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
		SPELL_CORRUPTION        = 251406,
	};

	void Reset() override
	{
		npc_theramore_horde::Reset();

		if (roll_chance_i(60))
		{
			CastSpellExtraArgs args;
			args.SetTriggerFlags(TRIGGERED_CAST_DIRECTLY);

			DoCastSelf(SPELL_SUMMON_FELHUNTER, args);
		}
	}

	void JustEngagedWith(Unit* who) override
	{
		npc_theramore_horde::JustEngagedWith(who);

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
				{
					CastStop({ SPELL_DRAIN_LIFE, SPELL_IMMOLATE, SPELL_INCINERATE });
					DoCast(target, SPELL_CONFLAGRATE);
				}
				conflagrate.Repeat(1s, 3s);
			})
			.Schedule(3s, 5s, [this](TaskContext chaos_bolt)
			{
				CastStop({ SPELL_DRAIN_LIFE, SPELL_IMMOLATE, SPELL_INCINERATE });
				DoCastVictim(SPELL_CHAOS_BOLT);
				chaos_bolt.Repeat(5s, 8s);
			})
			.Schedule(1ms, [this](TaskContext immolate)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 30.0f, false, true, -SPELL_IMMOLATE))
				{
					CastStop({ SPELL_CHAOS_BOLT, SPELL_INCINERATE });
					DoCast(target, SPELL_IMMOLATE);
				}
				immolate.Repeat(5s, 8s);
			})
			.Schedule(1ms, [this](TaskContext corruption)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 30.0f, false, true, -SPELL_CORRUPTION))
				{
					CastStop();
					DoCast(target, SPELL_CORRUPTION);
				}
				corruption.Repeat(2s, 5s);
			})
			.Schedule(1ms, [this](TaskContext incinerate)
			{
				CastStop(SPELL_DRAIN_LIFE);
				DoCastVictim(SPELL_INCINERATE);
				incinerate.Repeat(2300ms);
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

struct npc_wave_caller_gruhta : public CustomAI
{
	const uint8 MAX_ELEMENTAL_PROTECTION = 10;

	npc_wave_caller_gruhta(Creature* creature) : CustomAI(creature, true, AI_Type::Hybrid),
		stormkeeperStacks(0), phases(PHASE_NORMAL)
	{
		instance = creature->GetInstanceScript();

		// Information des sorts
		infoLightningBolt = sSpellMgr->AssertSpellInfo(SPELL_LIGHTNING_BOLT, DIFFICULTY_NONE);
		infoChainLightning = sSpellMgr->AssertSpellInfo(SPELL_CHAIN_LIGHTNING, DIFFICULTY_NONE);

		tempestPos01 = creature->GetHomePosition();
	}

	enum Spells
	{
		SPELL_FLAME_SHOCK           = 420277,
		SPELL_TEMPEST               = 452201,
		SPELL_ELEMENTAL_BLAST       = 117014,
		SPELL_STORMKEEPER           = 191634,
		SPELL_LIGHTNING_BOLT        = 430109,
		SPELL_CHAIN_LIGHTNING       = 1228260, 
		SPELL_CALL_LIGHTNING        = 157348,
		SPELL_GHOST_WOLF            = 361620,
		SPELL_PRIMORDIAL_STORM      = 1218090,
		SPELL_UNLIMITED_POWER       = 272737,
		SPELL_FOCUS_ELEMENT         = 167205,
		SPELL_TEMPEST_CHANNELING    = 212079,
		SPELL_ELEMENTAL_PROTECTION  = 371756,
		SPELL_LIGHTNING_STORM       = 447930
	};

	enum Misc
	{
		GROUP_NORMAL                = 1,
		GROUP_STORMKEEPER,
		GROUP_TEMPEST
	};

	enum Phases
	{
		PHASE_NORMAL,
		PHASE_TEMPEST
	};

	InstanceScript* instance;
	uint32 stormkeeperStacks;
	Phases phases;
	Position tempestPos01;

	const Position tempestPos02 = { -3922.5f, -4848.2866f, 0.001533f, 0.78f };

	const SpellInfo* infoLightningBolt;
	const SpellInfo* infoChainLightning;

	float GetDistance() override
	{
		return 30.f;
	}

	void Reset() override
	{
		CustomAI::Reset();

		stormkeeperStacks = 0;

		me->SetRegenerateHealth(true);
		me->SetWaterWalking(true);
	}

	void JustDied(Unit* killer) override
	{
		CustomAI::JustDied(killer);

		if (GameObject* barrier = instance->GetGameObject(DATA_ENERGY_BARRIER))
			barrier->Delete();
	}

	void MovementInform(uint32 type, uint32 id) override
	{
		if (type == EFFECT_MOTION_TYPE || type == POINT_MOTION_TYPE)
		{
			switch (id)
			{
				case MOVEMENT_INFO_POINT_01:
					me->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_02, tempestPos02, false, tempestPos02.GetOrientation());
					break;
				case MOVEMENT_INFO_POINT_02:
				{
					me->RemoveAurasDueToSpell(SPELL_GHOST_WOLF);
					me->SetHomePosition(tempestPos02);
					DoCastSelf(SPELL_TEMPEST_CHANNELING);
					scheduler.Schedule(1s, GROUP_TEMPEST, [this](TaskContext lightning_storm)
					{
						CastSpellExtraArgs args;
						args.SetTriggerFlags(TRIGGERED_IGNORE_CAST_IN_PROGRESS);

						for (uint8 i = 0; i < 8; i++)
						{
							Position randomPos = GetRandomPosition(tempestPos02, 100.0f);
							me->CastSpell(randomPos, SPELL_LIGHTNING_STORM, args);
						}

						lightning_storm.Repeat(80ms, 100ms);
					});
					break;
					}
				default:
					break;
			}
		}
	}

	void OnSpellCast(SpellInfo const* spell) override
	{
		switch (spell->Id)
		{
			case SPELL_ELEMENTAL_BLAST:
			{
				CastSpellExtraArgs args;
				args.AddSpellBP0(10);

				DoCastSelf(SPELL_FOCUS_ELEMENT, args);
				break;
			}
			case SPELL_STORMKEEPER:
			{
				scheduler.DelayGroup(GROUP_NORMAL, 4s);
				scheduler.Schedule(1s, GROUP_STORMKEEPER, [this](TaskContext bolt)
				{
					if (bolt.GetRepeatCounter() >= 2)
					{
						me->RemoveAurasDueToSpell(SPELL_STORMKEEPER);
						bolt.CancelGroup(GROUP_STORMKEEPER);
						return;
					}

					CastSpellExtraArgs args;
					args.SetTriggerFlags(TRIGGERED_CAST_DIRECTLY | TRIGGERED_IGNORE_CAST_IN_PROGRESS);

					DoCastVictim(SPELL_LIGHTNING_BOLT, args);
					bolt.Repeat(1s);
				});
				break;
			}
		}
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		scheduler
			.Schedule(1ms, GROUP_NORMAL, [this](TaskContext context)
			{
				if (me->HealthBelowPct(40.f))
				{
					context.CancelGroup(GROUP_NORMAL);

					me->SetReactState(REACT_PASSIVE);
					me->RemoveAllAuras();

					CastStop();

					DoCastSelf(SPELL_GHOST_WOLF, true);

					for (uint8 i = 0; i < MAX_ELEMENTAL_PROTECTION; i++)
					{
						me->AddAura(SPELL_ELEMENTAL_PROTECTION, me);
					}

					me->GetMotionMaster()->Clear();
					me->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_01, tempestPos01, true, tempestPos01.GetOrientation());
				}
				else
					context.Repeat(1s);
			})
			.Schedule(2s, GROUP_NORMAL, [this](TaskContext flame_shock)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, GetDistance(), false, true, -SPELL_FLAME_SHOCK))
				{
					CastStop();
					DoCast(target, SPELL_FLAME_SHOCK);
				}
				flame_shock.Repeat(18s);
			})
			.Schedule(3s, GROUP_NORMAL, [this](TaskContext elemental_blast)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				{
					CastStop(SPELL_TEMPEST);
					DoCast(target, SPELL_ELEMENTAL_BLAST);
				}
				elemental_blast.Repeat(12s);
			})
			.Schedule(6s, GROUP_NORMAL, [this](TaskContext stormkeeper)
			{
				CastStop();
				DoCastSelf(SPELL_STORMKEEPER);
				stormkeeper.Repeat(78s);
			})
			.Schedule(8s, GROUP_NORMAL, [this](TaskContext tempest)
			{
				CastStop(SPELL_STORMKEEPER);
				DoCastVictim(SPELL_TEMPEST);
				tempest.Repeat(20s);
			})
			.Schedule(9s, GROUP_NORMAL, [this](TaskContext lightning_bolt)
			{
				DoCastVictim(SPELL_LIGHTNING_BOLT);
				lightning_bolt.Repeat(Milliseconds(infoLightningBolt->CalcCastTime()) + 300ms);
			})
			.Schedule(12s, GROUP_NORMAL, [this](TaskContext call_lightning)
			{
				DoCastVictim(SPELL_CALL_LIGHTNING);
				call_lightning.Repeat(20s);
			})
			.Schedule(14s, GROUP_NORMAL, [this](TaskContext chain_lightning)
			{
				CastStop({ SPELL_LIGHTNING_BOLT, SPELL_TEMPEST, SPELL_CALL_LIGHTNING, SPELL_ELEMENTAL_BLAST });
				DoCastVictim(SPELL_CHAIN_LIGHTNING);
				chain_lightning.Repeat(Milliseconds(infoChainLightning->CalcCastTime()) + 300ms, 6s);
			});
	}
};

///
///     COSMETIC
///

const Position LookAtPos = { -3669.20f, -4504.08f, 10.33f, 1.60f };

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
		creature->SetRegenerateHealth(true);
		creature->SetHealth(creature->GetMaxHealth());
		creature->SetTarget(ObjectGuid::Empty);

		float angle = creature->GetAbsoluteAngle(LookAtPos);
		creature->SetOrientation(angle);
		creature->SetFacingToPoint(LookAtPos);
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

					float angle = me->GetAbsoluteAngle(LookAtPos);
					me->SetOrientation(angle);
					me->SetFacingToPoint(LookAtPos);
					me->SetReactState(REACT_AGGRESSIVE);

					scheduler.CancelGroup(COSMETIC_GROUP);
				}
				else
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
					float angle = me->GetAbsoluteAngle(LookAtPos);
					me->SetOrientation(angle);

					me->SetFacingToPoint(LookAtPos);

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
		SPELL_HEALING_TIDE_TOTEM_HEAL       = 255021,
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

// Powder Keg - BasicEvent
class PowderKegEvent : public BasicEvent
{
	enum Spells
	{
		SPELL_LARGE_EXPLOSION   = 271199,
		SPELL_LARGE_FIRE        = 414772
	};

	public:
	PowderKegEvent(Creature* owner, std::vector<Creature*> triggers) : owner(owner), triggers(triggers)
	{
		counter = 0;
		maxCount = triggers.size();
	}

	bool Execute(uint64 timer, uint32 /*updateTime*/) override
	{
		if (counter >= maxCount)
			return true;

		if (Creature* temp = triggers[counter])
		{
			temp->RemoveAllAuras();
			temp->CastSpell(temp, SPELL_LARGE_EXPLOSION);
			temp->AddAura(SPELL_LARGE_FIRE, temp);
		}

		counter++;

		owner->m_Events.AddEvent(this, Milliseconds(timer) + randtime(100ms, 180ms));

		return false;
	}

	private:
	Creature* owner;
	uint8 counter;
	uint8 maxCount;
	std::vector<Creature*> triggers;
};

// Powder Keg - 205238
class spell_powder_keg : public SpellScript
{
	enum Spells
	{
		SPELL_BIG_FIRE_EXPLOSION    = 183880,
	};

	void HandleDummy(SpellEffIndex /*effIndex*/)
	{
		Unit* caster = GetCaster();
		if (InstanceScript* instance = caster->GetInstanceScript())
		{
			const WorldLocation* destination = GetHitDest();
			if (caster && destination)
			{
				if (Creature* trigger = caster->SummonCreature(WORLD_TRIGGER, destination->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, 1min))
				{
					trigger->RemoveAllAuras();
					trigger->CastSpell(trigger, SPELL_BIG_FIRE_EXPLOSION);

					// The Sea Wolf destroyed event
					instance->TriggerGameEvent(EVENT_DESTROY_SEA_WOLF);

					// Delete Powder Barrel targeted
					if (GameObject* powder = instance->GetGameObject(DATA_POWDER_BARREL))
						powder->Delete();

					// If the boat is destroyed and Captan Drok is alive, he dies
					if (Creature* drok = instance->GetCreature(DATA_CAPTAIN_DROK))
					{
						if (drok->IsAlive() && trigger->IsWithinDistInMap(drok, 25.0f))
						{
							drok->KillSelf();
							if (Player* player = caster->ToPlayer())
							{
								KillRewarder::Reward(player, drok, NPC_CAPTAIN_DROK);
							}
						}
					}

					// If the boat is destroyed fire and explosions
					std::vector<Creature*> stalkers;
					trigger->GetCreatureListWithEntryInGrid(stalkers, NPC_INVISIBLE_STALKER, 50.0f);

					if (stalkers.empty())
						return;

					trigger->m_Events.AddEvent(new PowderKegEvent(trigger, stalkers), trigger->m_Events.CalculateTime(1s));
				}
			}
		}
	}

	void Register() override
	{
		OnEffectHit += SpellEffectFn(spell_powder_keg::HandleDummy, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
	}
};

// Titan Force Shield - 1216608
class spell_titan_force_shield : public AuraScript
{
public:
	spell_titan_force_shield()
	{
		maxHealth = 0;
		absorbedAmount = 0;
	}

	bool Load() override
	{
		maxHealth = GetCaster()->GetMaxHealth();
		absorbedAmount = 0;
		return true;
	}

	void CalculateAmount(AuraEffect const* /*auraEffect*/, int32& amount, bool& canBeRecalculated) const
	{
		canBeRecalculated = false;
		amount = CalculatePct(maxHealth, absorbPct);
	}

	void Register() override
	{
		DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_titan_force_shield::CalculateAmount, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB);
	}

private:
	const int32 absorbPct = 50;
	int32 maxHealth;
	uint32 absorbedAmount;
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

// Consecrated Ground
// AreaTriggerID - 34355
struct at_consecration : AreaTriggerAI
{
	at_consecration(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger)
	{
	}

	enum Spells
	{
		SPELL_CONSECRATION = 461742
	};

	void OnUnitEnter(Unit* /*unit*/) override
	{
		if (Unit* caster = at->GetCaster())
		{
			for (ObjectGuid unit : at->GetInsideUnits())
			{
				if (Unit* target = ObjectAccessor::GetUnit(*caster, unit))
				{
					caster->CastSpell(target, SPELL_CONSECRATION, true);
				}
			}
		}
	}

	void OnUnitExit(Unit* unit) override
	{
		unit->RemoveAurasDueToSpell(SPELL_CONSECRATION);
	}

	void OnRemove() override
	{
		if (Unit* caster = at->GetCaster())
		{
			for (ObjectGuid unit : at->GetInsideUnits())
			{
				if (Unit* target = ObjectAccessor::GetUnit(*caster, unit))
				{
					target->RemoveAurasDueToSpell(SPELL_CONSECRATION);
				}
			}
		}
	}
};

// Divine Word: Sanctuary - 372784
// AreaTriggerID - 35546
struct at_divine_word_sanctuary : AreaTriggerAI
{
	static constexpr Milliseconds TICK_PERIOD = Milliseconds(1000);

	at_divine_word_sanctuary(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger), _tickTimer(TICK_PERIOD)
	{
	}

	enum Spells
	{
		SPELL_DIVINE_WORD_SANCTUARY_HEAL = 372787
	};

	void OnUpdate(uint32 diff) override
	{
		_tickTimer -= Milliseconds(diff);

		if (_tickTimer > 0s)
			return;

		// Récupération du lanceur du sort
		if (Unit* caster = at->GetCaster())
		{
			for (const ObjectGuid& unitGuid : at->GetInsideUnits())
			{
				if (Unit* target = ObjectAccessor::GetUnit(*caster, unitGuid))
				{
					if (caster->IsFriendlyTo(target))
					{
						caster->CastSpell(target, SPELL_DIVINE_WORD_SANCTUARY_HEAL);
					}
				}
			}
		}

		_tickTimer += TICK_PERIOD;
	}

	private:
	Milliseconds _tickTimer;
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
	RegisterTheramoreAI(npc_wave_caller_gruhta);

	// Utilisables dans les Ruines de Theramore
	RegisterCreatureAI(npc_roknah_hag);
	RegisterCreatureAI(npc_roknah_grunt);
	RegisterCreatureAI(npc_roknah_loasinger);
	RegisterCreatureAI(npc_roknah_felcaster);
	RegisterCreatureAI(npc_healing_tide_totem);
	//-

	RegisterSpellScript(spell_theramore_light_of_dawn);
	RegisterSpellScript(spell_theramore_throw_bucket);
	RegisterSpellScript(spell_powder_keg);
	RegisterSpellScript(spell_titan_force_shield);

	RegisterAreaTriggerAI(at_blizzard_theramore);
	RegisterAreaTriggerAI(at_consecration);
	RegisterAreaTriggerAI(at_divine_word_sanctuary);
	RegisterAreaTriggerAI(at_uncontrolled_energy);
	RegisterAreaTriggerAI(at_arcane_rift);
	RegisterAreaTriggerAI(at_scorched_earth);
}
