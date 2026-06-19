#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "KillRewarder.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PassiveAI.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "CustomAI.h"
#include "battle_for_theramore.h"

///
///     ALLIANCE NPC
///

struct npc_theramore_citizen : public CustomAI
{
	enum Misc
	{
		// Spells
		SPELL_AFRAID                    = 123263,

		// Gossip
		GOSSIP_MENU_DEFAULT             = 65000,

		// Credits
		NPC_THERAMORE_CITIZEN_CREDIT    = 500005
	};

	enum Talks
	{
		SAY_THERAMORE_CITIZEN_FLEE      = 0,
		SAY_THERAMORE_CITIZEN_CRY       = 1,
		SAY_THERAMORE_CITIZEN_FEARED    = 2,
	};

	enum Path : uint32
	{
		WAYPOINT_PATH_01                = 4000031921,
		WAYPOINT_PATH_02                = 4000031922,
		WAYPOINT_PATH_03                = 4000031924,
		WAYPOINT_PATH_04                = 4000031923
	};

	npc_theramore_citizen(Creature* creature) : CustomAI(creature, AI_Type::Stay) { }

	void OnSpellClick(Unit* clicker, bool spellClickHandled) override
	{
		if (!spellClickHandled)
			return;

		Player* player = clicker->ToPlayer();
		if (!player)
			return;

		#ifdef CUSTOM_DEBUG
			for (uint8 i = 0; i < NUMBER_OF_CITIZENS; ++i)
				KillRewarder::Reward(player, me, NPC_THERAMORE_CITIZEN_CREDIT);
		#endif

		KillRewarder::Reward(player, me, NPC_THERAMORE_CITIZEN_CREDIT);

		me->SetVignette(VIGNETTE_NONE);
		me->SetEmoteState(EMOTE_STATE_NONE);
		me->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);

		scheduler.Schedule(780ms, 1500ms, [this, player](TaskContext context)
		{
			switch (context.GetRepeatCounter())
			{
				case 0:
				{
					me->SetFacingToObject(player);
					if (CreatureAddon const* creatureAddon = me->GetCreatureAddon())
					{
						if (!creatureAddon->auras.empty())
						{
							for (auto aura : creatureAddon->auras)
								me->RemoveAurasDueToSpell(aura);
						}
					}
					context.Repeat(780ms, 1500ms);
					break;
				}
				case 1:
				{
					if (me->GetWaypointPathId())
					{
						me->SetWalk(false);
						me->ResumeMovement();
						me->DespawnOrUnsummon(10s);
						me->AI()->Talk(SAY_THERAMORE_CITIZEN_FLEE);
					}
					else if (roll_chance(60))
					{
						me->HandleEmoteCommand(RAND(EMOTE_STATE_CRY, EMOTE_STATE_COWER));
						me->AI()->Talk(SAY_THERAMORE_CITIZEN_CRY);
					}
					else
					{
						me->HandleEmoteCommand(RAND(EMOTE_ONESHOT_WAVE, EMOTE_ONESHOT_NO));
						me->AI()->Talk(SAY_THERAMORE_CITIZEN_FEARED);
					}
					break;
				}
			}
		});
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

	void SpellHit(WorldObject* /*caster*/, SpellInfo const* spell) override
	{
		if (spell->Id != SPELL_REPAIR)
			return;

		me->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
		me->SetVignette(VIGNETTE_NONE);
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

	void SpellHit(WorldObject* caster, SpellInfo const* spell) override
	{
		if (spell->Id != SPELL_TELEPORT_TROOP)
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
	npc_theramore_troop(Creature* creature, AI_Type type)
		: CustomAI(creature, true, type),
		  instance(creature->GetInstanceScript()),
		  soundEmote(creature->GetGender() == GENDER_FEMALE ? 74679 : 74681),
		  emoteReceived(false)
	{
	}

	enum Misc
	{
		NPC_THERAMORE_TROOPS_CREDIT = 500011,
		ACTION_RECEIVE_EMOTE        = 1
	};

	static constexpr float EMOTE_EFFECT_RANGE  = 8.f;
	static constexpr float EMOTE_TRIGGER_RANGE = 3.5f;

	static constexpr uint32 TroopEntries[] =
	{
		NPC_THERAMORE_FOOTMAN,
		NPC_THERAMORE_FAITHFUL,
		NPC_THERAMORE_ARCANIST,
		NPC_THERAMORE_OFFICER,
		NPC_THERAMORE_MARKSMAN
	};

	InstanceScript* instance;
	ObjectGuid playerGuid;
	uint32 soundEmote;
	bool emoteReceived;

	void JustEngagedWith(Unit* /*who*/) override
	{
		me->CallAssistance();
	}

	void SetData(uint32 id, uint32 value) override
	{
		if (id == NPC_THERAMORE_TROOPS_CREDIT)
			emoteReceived = (value != 0);
	}

	void SetGUID(ObjectGuid const& guid, int32 id) override
	{
		if (id == NPC_THERAMORE_TROOPS_CREDIT)
			playerGuid = guid;
	}

	void DoAction(int32 param) override
	{
		if (param != ACTION_RECEIVE_EMOTE)
			return;

		Player* player = ObjectAccessor::GetPlayer(*me, playerGuid);
		if (!player)
			return;

		float const orientation = me->GetOrientation();
		scheduler.Schedule(1ms, 1s, [this, player, orientation](TaskContext context)
		{
			switch (context.GetRepeatCounter())
			{
				case 0:
					me->SetFacingToObject(player);
					context.Repeat(800ms, 1s);
					break;
				case 1:
					me->HandleEmoteCommand(EMOTE_ONESHOT_CHEER_FORTHEALLIANCE);
					KillRewarder::Reward(player, me, NPC_THERAMORE_TROOPS_CREDIT);
					context.Repeat(1ms, 2s);
					break;
				case 2:
					me->PlayDirectSound(soundEmote, player);
					context.Repeat(3s, 5s);
					break;
				case 3:
					me->SetFacingTo(orientation);
					context.CancelAll();
					return;
			}
		});
	}

	void ReceiveEmote(Player* player, uint32 emoteId) override
	{
		BFTPhases const phase = static_cast<BFTPhases>(instance->GetData(DATA_SCENARIO_PHASE));
		if (phase != BFTPhases::Preparation && phase != BFTPhases::Preparation_Rhonin)
			return;

#ifdef CUSTOM_DEBUG
		for (uint8 i = 0; i < NUMBER_OF_TROOPS; ++i)
			KillRewarder::Reward(player, me, NPC_THERAMORE_TROOPS_CREDIT);
		return;
#else
		if (emoteId != TEXT_EMOTE_FORTHEALLIANCE || emoteReceived)
			return;

		if (!player->IsWithinDist(me, EMOTE_TRIGGER_RANGE))
			return;

		std::list<Creature*> troops;
		for (uint32 entry : TroopEntries)
			me->GetCreatureListWithEntryInGrid(troops, entry, EMOTE_EFFECT_RANGE);

		for (Creature* troop : troops)
		{
			troop->AI()->SetGUID(player->GetGUID(), NPC_THERAMORE_TROOPS_CREDIT);
			troop->AI()->SetData(NPC_THERAMORE_TROOPS_CREDIT, 1U);
			troop->AI()->DoAction(ACTION_RECEIVE_EMOTE);
		}
#endif
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
				me->SetVignette(VIGNETTE_NONE);
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
			.Schedule(1min, [this](TaskContext /*rising_anger*/)
			{
				DoCast(SPELL_RISING_ANGER);
			});
	}

	void WaypointPathEnded(uint32 /*pointId*/, uint32 pathId) override
	{
		if (pathId == 2)
		{
			me->StopMoving();
			me->GetMotionMaster()->Clear();
			me->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_02, HedricPoint03, true, HedricPoint03.GetOrientation());
		}
	}

	void MovementInform(uint32 type, uint32 id) override
	{
		if (type == EFFECT_MOTION_TYPE || type == POINT_MOTION_TYPE)
		{
			switch (id)
			{
				case MOVEMENT_INFO_POINT_01:
					me->SetFacingTo(HedricPoint02.GetOrientation());
					break;
				case MOVEMENT_INFO_POINT_02:
					me->SetSheath(SHEATH_STATE_UNARMED);
					me->SetEmoteState(EMOTE_STATE_WAGUARDSTAND01);
					me->SetHomePosition(HedricPoint03);
					break;
				default:
					break;
			}
		}
	}
};

struct npc_theramore_officier : public npc_theramore_troop
{
	npc_theramore_officier(Creature* creature) : npc_theramore_troop(creature, AI_Type::Melee),
		shieldCount(0)
	{
	}

	// Groupes de tasks : permet de retarder en bloc les routines DPS/Heal
	// quand l'officier passe sous Divine Shield pour caster un Holy Light d'urgence.
	enum Groups
	{
		GROUP_NORMAL,
		GROUP_HEAL,
		GROUP_DIVINE_SHIELD
	};

	enum Spells
	{
		SPELL_DIVINE_SHIELD         = 642,
		SPELL_HOLY_SHOCK            = 20473,
		SPELL_JUDGMENT              = 20271,
		SPELL_AVENGER_SHIELD        = 31935,
		SPELL_WORD_OF_GLORY         = 85673,
		SPELL_FLASH_OF_LIGHT        = 283634,
		SPELL_AVENGING_WRATH        = 292266,
		SPELL_HOLY_LIGHT            = 295698,
		SPELL_LIGHT_OF_DAWN         = 295710,
		SPELL_BLESSING_OF_FREEDOM   = 299256,
		SPELL_SHIELD_RIGHTEOUS      = 337629,
		SPELL_DIVINE_STORM          = 357849,
		SPELL_REBUKE                = 405397,
		SPELL_BLESSED_HAMMER        = 420092,

		// Passifs
		SPELL_SHINING_LIGHT         = 327510,
		SPELL_AFTERIMAGE            = 400745
	};

	uint8 shieldCount;

	static constexpr uint8  DIVINE_SHIELD_HP_PCT      = 25;     // HP sous lequel on tente la bulle
	static constexpr int32  DIVINE_SHIELD_CHANCE      = 30;     // % de chance de proc sur le tick de degats
	static constexpr float  HOLY_LIGHT_HP_PCT         = 40.f;   // Allie sous ce % -> Holy Light prioritaire
	static constexpr uint32 HOLY_LIGHT_RANGE          = 60;
	static constexpr float  FLASH_OF_LIGHT_HP_PCT     = 40.f;
	static constexpr uint32 FLASH_OF_LIGHT_RANGE      = 80;
	static constexpr uint8  LIGHT_OF_DAWN_WOUNDED_HP  = 30;     // HP % considere comme "blesse"
	static constexpr float  LIGHT_OF_DAWN_RANGE       = 15.f;
	static constexpr uint32 LIGHT_OF_DAWN_MIN_FRIENDS = 3;      // Seuil pour lancer le cone AOE de heal
	static constexpr float  HOLY_SHOCK_HP_PCT         = 40.f;
	static constexpr uint32 HOLY_SHOCK_RANGE          = 90;
	static constexpr int32  HOLY_SHOCK_HEAL_CHANCE    = 60;     // Si un allie est blesse, % de heal vs damage
	static constexpr uint32 DIVINE_STORM_MIN_ENEMIES  = 3;
	static constexpr float  DIVINE_STORM_RANGE        = 8.f;
	static constexpr float  INTERRUPT_RANGE           = 30.f;
	static constexpr uint8  STACK_AMOUNT_SL           = 2;
	static constexpr uint8  STACK_AMOUNT_AFTERIMAGE   = 15;
	static constexpr float  WORD_OF_GLORY_RANGE       = 35;

	void Reset() override
	{
		npc_theramore_troop::Reset();

		shieldCount = 0;
	}

	void SpellHit(WorldObject* /*caster*/, SpellInfo const* spell) override
	{
		if (spell->Id == SPELL_SHINING_LIGHT)
		{
			scheduler.Schedule(2s, 5s, [this](TaskContext /*context*/)
			{
				if (Aura* aura = me->GetAura(SPELL_SHINING_LIGHT))
				{
					aura->ModStackAmount(-1);
				}

				CastStop();
				DoCastSelf(SPELL_WORD_OF_GLORY, true);
			});
		}

		// Proc Afterimage : double WoG (soi + allie blesse) gratuit
		if (spell->Id == SPELL_AFTERIMAGE)
		{
			Aura* aura = me->GetAura(SPELL_AFTERIMAGE);
			if (aura && aura->GetStackAmount() >= STACK_AMOUNT_AFTERIMAGE)
			{
				scheduler.Schedule(2s, 3s, [this](TaskContext /*context*/)
				{
					// Verifie si une cible est l?, sinon on fait rien
					if (Unit* target = FindLowestHealthFriend(me, WORD_OF_GLORY_RANGE))
					{
						if (Aura* afterimage = me->GetAura(SPELL_AFTERIMAGE))
						{
							CastStop();
							DoCastSelf(SPELL_WORD_OF_GLORY, true);
							DoCast(target, SPELL_WORD_OF_GLORY, true);
							afterimage->ModStackAmount(-STACK_AMOUNT_AFTERIMAGE);
						}
					}
				});
			}
		}

		// Anti-kite : Blessing of Freedom sur root/snare
		if (HasMechanic(spell, MECHANIC_ROOT) || HasMechanic(spell, MECHANIC_SNARE))
		{
			scheduler.Schedule(1s, 2s, [this](TaskContext /*blessing*/)
			{
				DoCastSelf(SPELL_BLESSING_OF_FREEDOM);
			});
		}
	}

	void OnSpellCast(SpellInfo const* spell) override
	{
		switch (spell->Id)
		{
			case SPELL_HOLY_LIGHT:
			case SPELL_FLASH_OF_LIGHT:
			case SPELL_LIGHT_OF_DAWN:
			case SPELL_AVENGER_SHIELD:
			case SPELL_DIVINE_STORM:
			case SPELL_BLESSED_HAMMER:
				DoCastSelf(SPELL_AFTERIMAGE, true);
				break;
		}
	}

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spell*/) override
	{
		// HasCooldown attend un spellId - ici Divine Shield, pas l'enum de groupe.
		if (me->GetSpellHistory()->HasCooldown(SPELL_DIVINE_SHIELD))
			return;

		if (me->HealthBelowPctDamaged(DIVINE_SHIELD_HP_PCT, damage) && roll_chance(DIVINE_SHIELD_CHANCE))
		{
			DoCastSelf(SPELL_DIVINE_SHIELD);

			// On gele DPS et heals pendant la bulle pour eviter qu'un cast en cours
			// reprenne la main avant le Holy Light d'urgence.
			scheduler.DelayGroup(GROUP_NORMAL, 4s);
			scheduler.DelayGroup(GROUP_HEAL, 4s);

			scheduler.Schedule(1s, GROUP_DIVINE_SHIELD, [this](TaskContext /*context*/)
			{
				// Heal d'urgence pleine vie : on injecte MaxHealth en BP0 pour
				// surcharger la valeur du sort - intentionnel, fait office de full heal sous bulle.
				CastSpellExtraArgs args;
				args.AddSpellBP0(me->GetMaxHealth());

				CastStop();
				DoCastSelf(SPELL_HOLY_LIGHT, args);
			});
		}
	}

	void JustEngagedWith(Unit* who) override
	{
		npc_theramore_troop::JustEngagedWith(who);

		// Ouverture : Judgment immediat
		DoCast(who, SPELL_JUDGMENT);

		scheduler
			// -------- Cooldowns offensifs --------
			.Schedule(1s, GROUP_NORMAL, [this](TaskContext avenging_wrath)
			{
				// Avenging Wrath en boucle sur son CD reel (~2min) pour les combats longs.
				if (roll_chance(60))
					DoCastSelf(SPELL_AVENGING_WRATH);
				avenging_wrath.Repeat(2min, 3min);
			})
			// -------- Heals --------
			.Schedule(5s, 8s, GROUP_HEAL, [this](TaskContext holy_light)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(HOLY_LIGHT_HP_PCT, HOLY_LIGHT_RANGE))
				{
					CastStop(SPELL_FLASH_OF_LIGHT);
					DoCast(target, SPELL_HOLY_LIGHT);
				}
				holy_light.Repeat(8s, 14s);
			})
			.Schedule(1s, 2s, GROUP_HEAL, [this](TaskContext flash_of_light)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(FLASH_OF_LIGHT_HP_PCT, FLASH_OF_LIGHT_RANGE))
				{
					CastStop(SPELL_HOLY_LIGHT);
					DoCast(target, SPELL_FLASH_OF_LIGHT);
				}
				flash_of_light.Repeat(5s, 8s);
			})
			.Schedule(8s, 11s, GROUP_HEAL, [this](TaskContext light_of_dawn)
			{
				// AOE heal en cone : on attend qu'au moins LIGHT_OF_DAWN_MIN_FRIENDS allies
				// soient blesses devant pour rentabiliser le cast.
				if (FriendsInFront(LIGHT_OF_DAWN_RANGE, LIGHT_OF_DAWN_WOUNDED_HP) >= LIGHT_OF_DAWN_MIN_FRIENDS)
				{
					CastStop();
					DoCastSelf(SPELL_LIGHT_OF_DAWN);
					light_of_dawn.Repeat(10s, 15s);
				}
				else
					light_of_dawn.Repeat(1s);
			})
			// -------- Controle / Interrupt --------
			.Schedule(5ms, GROUP_NORMAL, [this](TaskContext rebuke)
			{
				// Interrupt melee : casser meme un heal en cours pour stop un cast adverse.
				if (Unit* target = DoSelectCastingUnit(SPELL_REBUKE, me->GetCombatReach()))
				{
					CastStop();
					DoCast(target, SPELL_REBUKE);
					rebuke.Repeat(25s, 40s);
				}
				else
					rebuke.Repeat(1s);
			})
			.Schedule(5ms, GROUP_NORMAL, [this](TaskContext avenger_shield)
			{
				// Silence ranged : meme priorite que Rebuke sur les casters ennemis.
				if (Unit* target = DoSelectCastingUnit(SPELL_AVENGER_SHIELD, INTERRUPT_RANGE))
				{
					CastStop({ SPELL_HOLY_LIGHT, SPELL_FLASH_OF_LIGHT });
					DoCast(target, SPELL_AVENGER_SHIELD);
					avenger_shield.Repeat(15s, 23s);
				}
				else
					avenger_shield.Repeat(1s);
			})
			// -------- Rotation DPS / Hybrides --------
			.Schedule(20s, GROUP_NORMAL, [this](TaskContext holy_shock)
			{
				// Holy Shock est hybride : heal sur allie blesse en priorite, sinon damage sur la victime.
				CastStop();
				if (Unit* target = DoSelectBelowHpPctFriendly(HOLY_SHOCK_HP_PCT, HOLY_SHOCK_RANGE);
					target && roll_chance(HOLY_SHOCK_HEAL_CHANCE))
					DoCast(target, SPELL_HOLY_SHOCK);
				else
					DoCastVictim(SPELL_HOLY_SHOCK);

				holy_shock.Repeat(3s, 8s);
			})
			.Schedule(8s, 14s, GROUP_NORMAL, [this](TaskContext divine_storm)
			{
				if (EnemiesInRange(DIVINE_STORM_RANGE) >= DIVINE_STORM_MIN_ENEMIES)
				{
					CastStop({ SPELL_HOLY_LIGHT, SPELL_FLASH_OF_LIGHT });
					DoCast(SPELL_DIVINE_STORM);
					divine_storm.Repeat(35s, 42s);
				}
				else
					divine_storm.Repeat(1s);
			})
			.Schedule(14s, 22s, GROUP_NORMAL, [this](TaskContext blessed_hammer)
			{
				for (uint8 i = 0; i < 3; ++i)
				{
					scheduler.Schedule(i * 1s, [this](TaskContext /*context*/)
					{
						CastBlessedHammer();
					});
				}

				blessed_hammer.Repeat(8s, 14s);
			})
			.Schedule(2s, 8s, GROUP_NORMAL, [this](TaskContext shield_righteous)
			{
				DoCastVictim(SPELL_SHIELD_RIGHTEOUS);
				++shieldCount;

				if (shieldCount >= 3)
				{
					Aura const* aura = me->GetAura(SPELL_SHINING_LIGHT);
					if (!aura || aura->GetStackAmount() < STACK_AMOUNT_SL)
					{
						DoCastSelf(SPELL_SHINING_LIGHT, true);
					}

					shieldCount = 0;
				}

				shield_righteous.Repeat(2s, 5s);
			})
			.Schedule(2s, 8s, GROUP_NORMAL, [this](TaskContext judgment)
			{
				// Judgment sur la cible la plus eloignee : applique le debuff aux kiteurs/casters.
				if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
					DoCast(target, SPELL_JUDGMENT);
				judgment.Repeat(8s, 142s);
			});
	}

private:

	/// Lance Blessed Hammer
	void CastBlessedHammer()
	{
		CastStop();
		me->GetSpellHistory()->ResetCooldown(SPELL_BLESSED_HAMMER);
		DoCastAOE(SPELL_BLESSED_HAMMER, TRIGGERED_IGNORE_GCD);
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
	npc_theramore_arcanist(Creature* creature, AI_Type type = AI_Type::Distance) : npc_theramore_troop(creature, type), arcaneCharges(0)
	{
		arcaneBlastInfo = sSpellMgr->AssertSpellInfo(SPELL_ARCANE_BLAST, DIFFICULTY_NONE);
		// Arcane Tempo doit payer son cout en charges -> on retire les flags qui ignorent le cout.
		arcaneTempoArgs.SetTriggerFlags(TRIGGERED_FULL_MASK & ~(TRIGGERED_IGNORE_POWER_COST | TRIGGERED_IGNORE_REAGENT_COST));

		creature->SetPowerType(POWER_ARCANE_CHARGES, true, true);
	}

	enum Spells
	{
		SPELL_ARCANE_MISSILES           = 5143,
		SPELL_ARCANE_MISSILES_DAMAGE    = 7268,
		SPELL_ARCANE_BARRAGE            = 44425,
		SPELL_MAGE_ARMOR                = 183079,
		SPELL_CLEARCASTING              = 263725,
		SPELL_ARCANE_BLAST              = 291336,
		SPELL_TOUCH_OF_THE_MAGI         = 321507,
		SPELL_TOUCH_OF_THE_MAGI_BUFF    = 210824,
		SPELL_MASS_POLYMORPH            = 383121,
		SPELL_ARCANE_TEMPO              = 383997,
		SPELL_ARCANE_EXPLOSION          = 414381,
		SPELL_ARCANE_SPLINTER           = 443763,
		SPELL_ARCANE_ORB                = 440458,
		SPELL_TEMPORAL_REALIGNMENT      = 1244092,
		SPELL_TEMPORAL_REALIGNMENT_BUFF = 1244093
	};

	static constexpr int32  ARCANE_BARRAGE_MIN_CHARGES  = 4;        // Charges requises pour Barrage
	static constexpr uint32 CLEARCASTING_PROC_CHANCE    = 15;       // % de proc au hit d'Arcane Blast
	static constexpr float  AOE_RANGE                   = 10.0f;    // Distance de detection AOE
	static constexpr uint32 MASS_POLYMORPH_THRESHOLD    = 4;        // Au-dela de N ennemis -> Polymorph
	static constexpr uint32 ARCANE_EXPLOSION_THRESHOLD  = 2;        // Au-dela de N ennemis -> Explosion
	static constexpr float  TARGET_RANGE                = 30.0f;    // Portee de selection des cibles a distance

	const SpellInfo* arcaneBlastInfo;
	uint32 arcaneCharges;
	CastSpellExtraArgs arcaneTempoArgs;

	// Drapeau utilise pour tout cast interne (visuels, buffs auto, splinters) :
	// ignore GCD, cast en cours, cout, etc. - ne doit jamais bloquer la rotation.
	static constexpr TriggerCastFlags INTERNAL_CAST = TRIGGERED_FULL_MASK;

	void Reset() override
	{
		npc_theramore_troop::Reset();

		arcaneCharges = 0;

		// Mage Armor au reveil (apres 1s pour laisser le mob spawner proprement).
		scheduler.Schedule(1s, [this](TaskContext /*context*/)
		{
			DoCastSelf(SPELL_MAGE_ARMOR);
		});
	}

	// -------------------------------------------------------------------------
	// Hit handler
	// -------------------------------------------------------------------------

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spell*/) override
	{
		if (me->HealthBelowPctDamaged(25, damage) && !me->HasAura(SPELL_TEMPORAL_REALIGNMENT_BUFF))
		{
			DoCastSelf(SPELL_TEMPORAL_REALIGNMENT, INTERNAL_CAST);
			DoCastSelf(SPELL_TEMPORAL_REALIGNMENT_BUFF, INTERNAL_CAST);
		}
	}

	void SpellHitTarget(WorldObject* object, SpellInfo const* spell) override
	{
		Unit* victim = object->ToUnit();
		if (!victim)
			return;

		// Arcane Blast : genere 1 charge, refresh Tempo, et a 15% de chance de proc Clearcasting.
		if (spell->Id == SPELL_ARCANE_BLAST)
		{
			arcaneCharges++;

			if (roll_chance(CLEARCASTING_PROC_CHANCE))
			{
				DoCastSelf(SPELL_CLEARCASTING);

				// Apres 1-5s on consomme Clearcasting via Arcane Missiles instant.
				scheduler.Schedule(1s, 5s, [this](TaskContext /*arcane_missiles*/)
				{
					if (!me->HasAura(SPELL_CLEARCASTING))
						return;

					Unit* target = SelectTarget(SelectTargetMethod::Random);
					if (!target)
						return;

					CastStop();
					DoCast(target, SPELL_ARCANE_MISSILES);
					me->RemoveAurasDueToSpell(SPELL_CLEARCASTING);
				});
			}
		}

		// Splinters supplementaires en fonction du sort qui a touche.
		switch (spell->Id)
		{
			case SPELL_ARCANE_BLAST:
			case SPELL_ARCANE_MISSILES_DAMAGE:
				CastSplinters(victim, spell, 1);
				break;
			case SPELL_ARCANE_BARRAGE:
				CastSplinters(victim, spell, 4);
				break;
		}
	}

	// -------------------------------------------------------------------------
	// Rotation
	// -------------------------------------------------------------------------

	// Quand le mage commence a reculer (kite).
	void OnBackpedStart(Unit* victim) override
	{
		DoCast(victim, SPELL_ARCANE_BARRAGE);
	}

	void JustEngagedWith(Unit* who) override
	{
		npc_theramore_troop::JustEngagedWith(who);

		// Premier Arcane Blast immediat sur l'agresseur.
		DoCast(who, SPELL_ARCANE_BLAST);

		scheduler
			// --- Arcane Blast continu ---
			// Re-schedule a la duree exacte du cast time (qui descend avec les charges).
			.Schedule(2s, [this](TaskContext arcane_blast)
			{
				DoCastVictim(SPELL_ARCANE_BLAST);
				arcane_blast.Repeat(Milliseconds(arcaneBlastInfo->CalcCastTime()));
			})
			// --- Arcane Orbs ---
			// Salve de 1-5 orbs centree sur soi (cible utilisee uniquement pour l'arret si elle meurt).
			.Schedule(2s, [this](TaskContext arcane_orb)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random))
					CastArcaneOrbs(target);
				arcane_orb.Repeat(10s, 15s);
			})
			// --- Arcane Barrage (finisher) ---
			// Cast quand on a >= 5 charges, sinon on re-check toutes les 2s.
			.Schedule(2s, [this](TaskContext arcane_barrage)
			{
				if (arcaneCharges >= ARCANE_BARRAGE_MIN_CHARGES)
				{
					DoCastVictim(SPELL_ARCANE_BARRAGE);
					arcaneCharges = 0;
				}
				arcane_barrage.Repeat(2s);
			})
			// --- Touch of the Magi ---
			// Sur cible aleatoire qui n'a pas deja le debuff. Toutes les 3-8s.
			.Schedule(4s, 8s, [this](TaskContext touch_of_the_magi)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, TARGET_RANGE, false, true, -SPELL_TOUCH_OF_THE_MAGI_BUFF))
					DoCast(target, SPELL_TOUCH_OF_THE_MAGI);
				touch_of_the_magi.Repeat(3s, 8s);
			})
			// --- Mass Polymorph ---
			// 4+ ennemis colles -> CC AOE, 2 min de cooldown.
			.Schedule(5s, 8s, [this](TaskContext mass_polymorph)
			{
				if (EnemiesInRange(AOE_RANGE) >= MASS_POLYMORPH_THRESHOLD)
				{
					CastStop();
					DoCastSelf(SPELL_MASS_POLYMORPH);
					mass_polymorph.Repeat(1min);
				}
				else
					mass_polymorph.Repeat(1s);
			})
			// --- Arcane Explosion ---
			// 2+ ennemis colles -> degats AOE, 2 min de cooldown.
			.Schedule(2s, 8s, [this](TaskContext arcane_explosion)
			{
				if (EnemiesInRange(AOE_RANGE) >= ARCANE_EXPLOSION_THRESHOLD)
				{
					CastStop();
					DoCastSelf(SPELL_ARCANE_EXPLOSION);
					arcane_explosion.Repeat(45s);
				}
				else
					arcane_explosion.Repeat(1s);
			});
	}

	// -------------------------------------------------------------------------
	// Splinters / Orbs
	// -------------------------------------------------------------------------

	// Apres un hit (Blast/Missiles = 1, Barrage = 4), tire `count` Arcane Splinters
	// espaces de 380-560ms sur la meme cible. Re-resout le GUID a chaque tick pour
	// eviter le pointeur dangling si la cible meurt en cours de salve.
	void CastSplinters(Unit* victim, SpellInfo const* spell, uint8 count)
	{
		// Pas de splinter sur soi-meme.
		if (victim->GetGUID() == me->GetGUID())
			return;

		// Anti-recursion : ne pas chainer un splinter sur un hit de splinter.
		if (spell->Id == SPELL_ARCANE_SPLINTER)
			return;

		ObjectGuid victimGuid = victim->GetGUID();
		scheduler.Schedule(1ms, [this, victimGuid, count](TaskContext context)
		{
			if (context.GetRepeatCounter() >= count)
				return;

			Unit* target = ObjectAccessor::GetUnit(*me, victimGuid);
			if (!target || !target->IsAlive())
				return;

			DoCast(target, SPELL_ARCANE_SPLINTER, INTERNAL_CAST);
			context.Repeat(380ms, 560ms);
		});
	}

	// Tire 1-3 Arcane Orbs sur soi-meme (sort AOE auto-centre) espaces de 325ms.
	// La cible passee en parametre sert uniquement de "trigger" : si elle meurt,
	// la salve s'arrete (cible morte = plus de raison de continuer le burst).
	void CastArcaneOrbs(Unit* victim)
	{
		if (victim->GetGUID() == me->GetGUID())
			return;

		uint8 count = urand(1, 3);
		ObjectGuid victimGuid = victim->GetGUID();
		scheduler.Schedule(1ms, [this, victimGuid, count](TaskContext context)
		{
			if (context.GetRepeatCounter() >= count)
				return;

			Unit* target = ObjectAccessor::GetUnit(*me, victimGuid);
			if (!target || !target->IsAlive())
				return;

			DoCastSelf(SPELL_ARCANE_ORB, INTERNAL_CAST);
			context.Repeat(325ms);
		});
	}
};

struct npc_theramore_faithful : public npc_theramore_troop
{
	npc_theramore_faithful(Creature* creature) : npc_theramore_troop(creature, AI_Type::Distance),
		painSuppression(false)
	{
	}

	enum Groups
	{
		GROUP_NORMAL,           // Rotation offensive (Smite, Halo, Psychic Scream)
		GROUP_HEALING,          // Routines de soin (Shield, Renew, Prayer, Flash Heal)
		GROUP_PAIN_SUPPRESSION  // Sequence defensive a 10% PV
	};

	enum Spells
	{
		SPELL_PRAYER_OF_HEALING     = 596,
		SPELL_SHADOW_WORD_DEATH     = 51818,
		SPELL_DIVINE_HYMN           = 64843,
		SPELL_PSYCHIC_SCREAM        = 65543,
		SPELL_PAIN_SUPPRESSION      = 69910,
		SPELL_ECHO_OF_LIGHT         = 77489,
		SPELL_HALO                  = 120517,
		SPELL_PLEA                  = 200829,
		SPELL_POWER_WORD_FORTITUDE  = 267528,
		SPELL_RENEW                 = 294342,
		SPELL_FLASH_HEAL            = 314655,
		SPELL_POWER_WORD_SHIELD     = 318158,
		SPELL_SMITE                 = 332705,
		SPELL_GREATER_HEAL          = 342797,
		SPELL_SHADOW_WORD_PAIN      = 435397
	};

	static constexpr uint8 PAIN_SUPPRESSION_HP_PCT      = 10;       // PV qui declenche la sequence defensive
	static constexpr Seconds PAIN_SUPPRESSION_DELAY     = 7s;       // Duree pendant laquelle on bloque les autres routines
	static constexpr Minutes PAIN_SUPPRESSION_CD        = 3min;     // Cooldown interne avant de pouvoir re-declencher

	static constexpr float HEAL_FRIENDLY_RANGE          = 40.0f;    // Portee standard des soins cibles
	static constexpr uint8 SHIELD_HP_PCT                = 80;       // Allie sous ce % -> shield
	static constexpr uint8 SHIELD_BP_PCT                = 20;       // Force du shield = % des PV max de la cible
	static constexpr float MELEE_AOE_RANGE              = 10.0f;    // Distance de detection AOE
	static constexpr uint32 PSYCHIC_SCREAM_THRESHOLD    = 2;        // Au-dela de N ennemis colles -> Psychic Scream
	static constexpr float SHADOW_RANGE                 = 30.0f;    // Portee du Shadow Word Pain en backpedal

	// Drapeau utilise pour tout cast interne (procs, buffs auto) : ignore GCD, cast en cours, etc.
	static constexpr TriggerCastFlags INTERNAL_CAST = TRIGGERED_FULL_MASK;

	bool painSuppression;       // True tant que la sequence defensive est en cooldown interne

	void Reset() override
	{
		npc_theramore_troop::Reset();

		painSuppression = false;

		// Buff permanent : Power Word: Fortitude sur les allies qui ne l'ont pas (re-check toutes les 2s).
		scheduler.Schedule(1s, 5s, [this](TaskContext fortitude)
		{
			if (Unit* target = SelectRandomMissingBuff(SPELL_POWER_WORD_FORTITUDE))
				DoCast(target, SPELL_POWER_WORD_FORTITUDE);
			fortitude.Repeat(2s);
		});
	}

	// -------------------------------------------------------------------------
	// Hit handlers
	// -------------------------------------------------------------------------

	void SpellHitTarget(WorldObject* object, SpellInfo const* spell) override
	{
		Unit* victim = object->ToUnit();
		if (!victim)
			return;

		// Echo of Light : proc qui ajoute un HoT secondaire apres certains soins.
		switch (spell->Id)
		{
			case SPELL_PRAYER_OF_HEALING:
			case SPELL_RENEW:
			case SPELL_FLASH_HEAL:
				DoCast(victim, SPELL_ECHO_OF_LIGHT, INTERNAL_CAST);
				break;
		}
	}

	// -------------------------------------------------------------------------
	// Reactions
	// -------------------------------------------------------------------------

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spell*/) override
	{
		// Sequence defensive declenchee une seule fois par fenetre de 3 min.
		if (painSuppression || !me->HealthBelowPctDamaged(PAIN_SUPPRESSION_HP_PCT, damage))
			return;

		painSuppression = true;

		// Gel les rotations offensives ET de heal pendant la canalisation de Divine Hymn.
		scheduler.DelayGroup(GROUP_NORMAL, PAIN_SUPPRESSION_DELAY);
		scheduler.DelayGroup(GROUP_HEALING, PAIN_SUPPRESSION_DELAY);

		CastStop();
		DoCastSelf(SPELL_PAIN_SUPPRESSION);

		scheduler
			// Lancement d'Hymne divin apres un court delai.
			.Schedule(800ms, [this](TaskContext /*context*/)
			{
				CastStop();
				DoCastSelf(SPELL_DIVINE_HYMN);

				// Empeche le circle-kite et le MoveChase
				NotifyTeleported(PAIN_SUPPRESSION_DELAY);
			})
			// Re-armement du flag apres le cooldown interne.
			.Schedule(PAIN_SUPPRESSION_CD, GROUP_PAIN_SUPPRESSION, [this](TaskContext /*context*/)
			{
				painSuppression = false;
			});
	}

	// En backpedal : trois branches mutuellement exclusives selon roll_chance sequentiel.
	// Probabilites finales : 25% self-heal, 37.5% Shadow Word Death, 37.5% Shadow Word Pain.
	void OnBackpedStart(Unit* victim) override
	{
		if (roll_chance(25))
		{
			DoCastSelf(RAND(SPELL_RENEW, SPELL_PLEA, SPELL_POWER_WORD_SHIELD));
		}
		else if (roll_chance(50))
		{
			DoCast(victim, SPELL_SHADOW_WORD_DEATH);
		}
		else if (Unit* dotTarget = SelectTarget(SelectTargetMethod::Random, 0, SHADOW_RANGE, false, true, -SPELL_SHADOW_WORD_PAIN))
		{
			DoCast(dotTarget, SPELL_SHADOW_WORD_PAIN);
		}
	}

	// -------------------------------------------------------------------------
	// Rotation
	// -------------------------------------------------------------------------

	void JustEngagedWith(Unit* who) override
	{
		npc_theramore_troop::JustEngagedWith(who);

		DoCast(who, SPELL_SMITE);

		scheduler
			// === DPS (GROUP_NORMAL) ===

			// Smite en continu (filler offensif).
			.Schedule(1s, GROUP_NORMAL, [this](TaskContext smite)
			{
				DoCastVictim(SPELL_SMITE);
				smite.Repeat(2s);
			})
			// Psychic Scream si 2+ ennemis colles.
			.Schedule(3s, 8s, GROUP_NORMAL, [this](TaskContext psychic_scream)
			{
				if (EnemiesInRange(MELEE_AOE_RANGE) >= PSYCHIC_SCREAM_THRESHOLD)
				{
					DoCastAOE(SPELL_PSYCHIC_SCREAM);
					psychic_scream.Repeat(10s, 25s);
				}
				else
					psychic_scream.Repeat(1s);
			})

			// === HEAL (GROUP_HEALING) ===

			// Halo : heal AOE radial.
			.Schedule(1s, 5s, GROUP_HEALING, [this](TaskContext halo)
			{
				DoCastAOE(SPELL_HALO);
				halo.Repeat(14s, 25s);
			})
			// Power Word: Shield avec BP0 = 20% PV max de la cible (toutes les 8s).
			.Schedule(1s, 2s, GROUP_HEALING, [this](TaskContext power_word_shield)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(HEAL_FRIENDLY_RANGE, SHIELD_HP_PCT))
				{
					CastSpellExtraArgs args;
					args.AddSpellBP0(target->CountPctFromMaxHealth(SHIELD_BP_PCT));

					CastStop(SPELL_FLASH_HEAL);
					DoCast(target, SPELL_POWER_WORD_SHIELD, args);
				}
				power_word_shield.Repeat(8s);
			})
			// Renew en HoT preventif sur allie sous 60% PV.
			.Schedule(5s, 7s, GROUP_HEALING, [this](TaskContext renew)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(HEAL_FRIENDLY_RANGE, 60))
					DoCast(target, SPELL_RENEW);
				renew.Repeat(10s, 15s);
			})
			// Prayer of Healing AOE toutes les 14s (interrompt Flash Heal au passage).
			.Schedule(12s, 14s, GROUP_HEALING, [this](TaskContext mass_healing)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(HEAL_FRIENDLY_RANGE, 30))
				{
					CastStop(SPELL_SMITE);
					DoCast(target, SPELL_PRAYER_OF_HEALING);
				}
				mass_healing.Repeat(10s, 14s);
			})
			// Flash Heal en spot sur allie sous 60% PV (toutes les 2s).
			.Schedule(1s, 8s, GROUP_HEALING, [this](TaskContext flash_heal)
			{
				if (Unit* target = FindLowestHealthFriend(me, HEAL_FRIENDLY_RANGE))
				{
					CastStop(SPELL_PRAYER_OF_HEALING);
					DoCast(target, SPELL_FLASH_HEAL);
				}
				flash_heal.Repeat(2s, 8s);
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

	void Reset() override
	{
		npc_theramore_troop::Reset();

		me->SetEmoteState(EMOTE_STATE_READYCROSSBOW);
	}

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
		iceblock(false)
	{
	}

	// Override : detection d'encerclement plus large (12y comme l'ancien MELEE_FLEE_RANGE).
	float GetEncircleRadius() const override { return MELEE_FLEE_RANGE; }

	// Groupes de scheduler. Chaque groupe peut etre delay/cancel independamment.
	enum Groups
	{
		GROUP_NORMAL,       // Cycle long : Glacial Spike retry, Cone of Cold, Flurry
		GROUP_FLEE,         // Sequence Frost Nova -> Blink declenchee par melee/Ice Block
		GROUP_FROSTBOLT,    // Cast continu de Frostbolt (pilier de la rotation)
		GROUP_ICE_LANCE     // Salves d'Ice Lance sur cibles Freezing
	};

	enum Spells
	{
		SPELL_FROSTBOLT             = 116,
		SPELL_GLACIAL_SPIKE         = 199786,
		SPELL_GLACIAL_SPIKE_BUFF    = 199844,
		SPELL_ICE_BARRIER           = 198094,
		SPELL_HYPOTHERMIA           = 240132,
		SPELL_ICE_BLOCK             = 278960,
		SPELL_FLURRY                = 284858,
		SPELL_ICE_LANCE             = 284871,
		SPELL_FROST_NOVA            = 284879,
		SPELL_CONE_OF_COLD          = 292294,
		SPELL_BLINK                 = 295236,
		SPELL_FROST_SPLINTER        = 443722,
		SPELL_FREEZING              = 1221389
	};

	static constexpr float MELEE_FLEE_RANGE             = 12.0f;    // Distance "trop proche" pour fuir
	static constexpr uint32 MELEE_FLEE_THRESHOLD        = 2;        // Au-dela de N ennemis melee, on fuit
	static constexpr float ICE_LANCE_RANGE              = 30.0f;    // Portee de selection des cibles Ice Lance
	static constexpr uint32 FREEZING_STACKS_PER_LANCE   = 4;        // Stacks de Freezing consommes par un Ice Lance qui shatter

	// Drapeau utilise pour tout cast interne (visuels, buffs auto, finishers, retries) :
	// ignore GCD, cast en cours, cout, etc. - ne doit jamais bloquer la rotation.
	static constexpr TriggerCastFlags INTERNAL_CAST = TRIGGERED_FULL_MASK;

	bool iceblock; // True tant qu'Ice Block est en cooldown interne (1 min)

	void Reset() override
	{
		npc_theramore_horde::Reset();

		iceblock = false;
	}

	// -------------------------------------------------------------------------
	// Helpers
	// -------------------------------------------------------------------------

	// Tente de lancer Pointe glaciaire si le buff finisher est encore actif.
	// Centralise la logique appelee depuis SpellHit (proc), OnSpellFailed (retry)
	// et la tache GROUP_NORMAL (filet de securite).
	void TryCastGlacialSpike()
	{
		if (!me->HasAura(SPELL_GLACIAL_SPIKE_BUFF))
			return;

		CastStop();
		DoCastVictim(SPELL_GLACIAL_SPIKE);
	}

	// -------------------------------------------------------------------------
	// Hit handlers
	// -------------------------------------------------------------------------

	void SpellHitTarget(WorldObject* object, SpellInfo const* spell) override
	{
		Unit* victim = object->ToUnit();
		if (!victim)
			return;

		// La generation de splinters (Frostbolt / Flurry / Ice Lance / Glacial Spike)
		// est entierement geree par le passif Splintering Sorcery (443739).
		// La generation d'Icicles + le buff Pointe glaciaire (199844) sont geres
		// par le talent passif SPELL_ICICLE_TALENT (1246832).
		switch (spell->Id)
		{
			case SPELL_FROST_SPLINTER:
				// Chaque splinter applique une stack de Freezing sur sa cible.
				DoCast(victim, SPELL_FREEZING, INTERNAL_CAST);
				break;
		}
	}

	// Pointe glaciaire interrompue / OOR / hors de vue : on relance dans 2s tant
	// que le buff de disponibilite est encore actif (le filet de securite
	// GROUP_NORMAL retentera aussi, mais ce retry immediat reduit le temps mort).
	void OnSpellFailed(SpellInfo const* spell) override
	{
		if (spell->Id == SPELL_GLACIAL_SPIKE && me->HasAura(SPELL_GLACIAL_SPIKE_BUFF))
		{
			scheduler.Schedule(2s, GROUP_NORMAL, [this](TaskContext /*context*/)
			{
				TryCastGlacialSpike();
			});
		}
	}

	void SpellHit(WorldObject* /*caster*/, SpellInfo const* spell) override
	{
		// Blink reactif : si on subit un root/snare et que Blink est dispo, on le lance dans 2s.
		if (!me->GetSpellHistory()->HasCooldown(SPELL_BLINK)
			&& (HasMechanic(spell, MECHANIC_ROOT) || HasMechanic(spell, MECHANIC_SNARE)))
		{
			scheduler.Schedule(2s, [this, spellId = spell->Id](TaskContext /*context*/)
			{
				CastBlink(false, spellId);
			});
		}

		// Le buff Glacial Spike vient d'etre applique -> on tente le finisher immediatement.
		if (spell->Id == SPELL_GLACIAL_SPIKE_BUFF)
			TryCastGlacialSpike();
	}

	// -------------------------------------------------------------------------
	// Reactions
	// -------------------------------------------------------------------------

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spell*/) override
	{
		// En dessous de 20% PV : on annule le coup et on declenche Ice Block + fuite.
		// Cooldown interne d'1 min - Hypothermia empeche cote sort un re-block immediat.
		if (!iceblock && HealthBelowPct(20))
		{
			damage = 0;

			iceblock = true;

			CastStop();
			DoCast(SPELL_ICE_BLOCK);
			DoCastSelf(SPELL_HYPOTHERMIA, INTERNAL_CAST);

			// 10s (+1s) = duree d'Ice Block : on enchaine Frost Nova + Blink.
			CastFleeSequence(11s);

			scheduler.Schedule(1min, [this](TaskContext /*context*/)
			{
				iceblock = false;
			});
		}
	}

	// Quand le mage commence a reculer (kite), on lache une Ice Lance gratuite.
	void OnBackpedTick(Unit* victim) override
	{
		DoCast(victim, SPELL_ICE_LANCE, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_IGNORE_GCD);
	}

	// -------------------------------------------------------------------------
	// Rotation principale
	// -------------------------------------------------------------------------

	void JustEngagedWith(Unit* who) override
	{
		npc_theramore_horde::JustEngagedWith(who);

		// 30% de chance de se buffer Ice Barrier au pull.
		if (roll_chance(30))
			DoCastSelf(SPELL_ICE_BARRIER);

		// Premier Frostbolt immediat sur l'agresseur (sinon il faut attendre 2s).
		DoCast(who, SPELL_FROSTBOLT);

		scheduler
			// --- Salve d'Ice Lance ---
			// Selectionne une cible aleatoire avec Freezing, puis enchaine 2-5 lances
			// espacees de 800ms-1s. Le prochain cycle est planifie a +8/12s apres
			// la fin de la derniere lance (offset cumule).
			.Schedule(1s, GROUP_ICE_LANCE, [this](TaskContext shatter)
			{
				Unit* target = SelectTarget(SelectTargetMethod::Random, 0, ICE_LANCE_RANGE, false, true, SPELL_FREEZING);
				if (!target)
				{
					shatter.Repeat(2s);
					return;
				}

				// Chaque Ice Lance consomme 4 stacks de Freezing pour shatter,
				// donc on plafonne le nombre de lances a stacks/4.
				Aura const* freezing = target->GetAura(SPELL_FREEZING);
				uint32 stacks = freezing ? freezing->GetStackAmount() : 0;
				uint32 maxLances = stacks / FREEZING_STACKS_PER_LANCE;
				if (!maxLances)
				{
					shatter.Repeat(2s);
					return;
				}

				ObjectGuid targetGuid = target->GetGUID();
				uint32 count = std::min<uint32>(urand(2, 5), maxLances);

				// Precalcule les offsets pour connaitre la duree totale de la salve
				// et pouvoir bloquer la rotation pile le temps necessaire.
				std::vector<Milliseconds> offsets;
				offsets.reserve(count);
				Milliseconds offset = 0ms;
				for (uint32 i = 0; i < count; ++i)
				{
					offsets.push_back(offset);
					offset += randtime(800ms, 1s);
				}

				// Laisse respirer Frostbolt et la rotation pendant toute la salve
				// (+1s de marge pour eviter qu'un Frostbolt chevauche la derniere lance).
				Milliseconds salvoDuration = offset + 1s;
				scheduler.DelayGroup(GROUP_NORMAL, salvoDuration);
				scheduler.DelayGroup(GROUP_FROSTBOLT, salvoDuration);

				for (Milliseconds delay : offsets)
				{
					scheduler.Schedule(delay, GROUP_ICE_LANCE, [this, targetGuid](TaskContext /*lance*/)
					{
						Unit* victim = ObjectAccessor::GetUnit(*me, targetGuid);
						if (!victim || !victim->IsAlive())
							return;

						// Si la cible a perdu Freezing entre-temps (dispel, expiration,
						// shatter d'un autre add...), on stoppe la salve : la rotation
						// normale reprend grace au DelayGroup qui arrive a echeance.
						if (!victim->HasAura(SPELL_FREEZING))
							return;

						DoCast(victim, SPELL_ICE_LANCE, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_IGNORE_GCD);
					});
				}

				shatter.Repeat(offset + 5s, offset + 8s);
			})
			// --- Filet de securite Glacial Spike ---
			// Si Glacial Spike rate (interrompu, OOR), retente toutes les 2s tant que le buff est la.
			.Schedule(1s, GROUP_NORMAL, [this](TaskContext glacial_spike)
			{
				if (!me->HasUnitState(UNIT_STATE_CASTING) && me->HasAura(SPELL_GLACIAL_SPIKE_BUFF))
					TryCastGlacialSpike();
				glacial_spike.Repeat(2s);
			})
			// --- Cone of Cold ---
			// Quand 3+ ennemis sont au corps a corps : interrompt Glacial Spike et lance Cone of Cold.
			.Schedule(13s, 18s, GROUP_NORMAL, [this](TaskContext cone_of_cold)
			{
				if (EnemiesInRange(MELEE_FLEE_RANGE) > MELEE_FLEE_THRESHOLD)
				{
					CastStop(SPELL_GLACIAL_SPIKE);
					DoCast(SPELL_CONE_OF_COLD);
					cone_of_cold.Repeat(5s, 8s);
				}
				else
					cone_of_cold.Repeat(2s);
			})
			// --- Flurry ---
			// Cible aleatoire toutes les 12-14s.
			.Schedule(12s, 15s, GROUP_NORMAL, [this](TaskContext flurry)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random))
					DoCast(target, SPELL_FLURRY);
				flurry.Repeat(12s, 14s);
			})
			// --- Frostbolt continu ---
			// Pilier de la rotation : 1 Frostbolt toutes les 2s sur la cible courante.
			.Schedule(2s, GROUP_FROSTBOLT, [this](TaskContext frostbolt)
			{
				DoCastVictim(SPELL_FROSTBOLT);
				frostbolt.Repeat(3s);
			});
	}

	// Encerclee par des melee -> Frost Nova + Blink. Le cooldown de re-declenchement
	// est gere par CustomAI (encircleReactOnCooldown).
	bool OnEncircled(Unit* /*victim*/) override
	{
		if (me->HasAura(SPELL_ICE_BLOCK))
			return false;

		CastFleeSequence(1s);
		return true;
	}

	// Sequence de fuite : Delai -> Frost Nova (root les melee) -> 300ms -> Blink (degage).
	// Reutilisee par DamageTaken (Ice Block a 20% PV) et OnEncircled (encerclement).
	void CastFleeSequence(Seconds start)
	{
		scheduler
			.Schedule(start, GROUP_FLEE, [this](TaskContext context)
			{
				if (me->HasAura(SPELL_ICE_BLOCK))
					return;

				switch (context.GetRepeatCounter())
				{
					case 0: // Delai des groupes de combat
						scheduler.DelayGroup(GROUP_NORMAL, 2s);
						scheduler.DelayGroup(GROUP_FROSTBOLT, 2s);
						scheduler.DelayGroup(GROUP_ICE_LANCE, 2s);
						context.Repeat(100ms);
						break;
					case 1: // Root les melee autour
						CastStop();
						DoCastSelf(SPELL_FROST_NOVA, INTERNAL_CAST);
						context.Repeat(300ms);
						break;
					case 2: // Blink pour reprendre de la distance
						CastBlink(true);
						break;
				}
			});
	}

	// -------------------------------------------------------------------------
	// Splinters / Backped / Blink
	// -------------------------------------------------------------------------

	// Blink + nettoyage des effets de mouvement, avec retrait optionnel de l'aura qui nous a touche.
	void CastBlink(bool triggered, Optional<uint32> removeAura = {})
	{
		CastStop();
		DoCastSelf(SPELL_BLINK, triggered);
		me->RemoveMovementImpairingAuras(true);

		// Empeche le circle-kite et le MoveChase de ramener le mage sur sa
		// position d'avant le Blink.
		NotifyTeleported();

		if (removeAura)
			me->RemoveAurasDueToSpell(*removeAura);
	}
};

struct npc_roknah_grunt : public npc_theramore_horde
{
    // https://www.wowhead.com/fr/npc=144522/marco-le-malodorant#abilities
	npc_roknah_grunt(Creature* creature) : npc_theramore_horde(creature, AI_Type::Melee) {}

	enum Spells
	{
        SPELL_BLOODTHIRST   = 23881,
        SPELL_RAGING_BLOW   = 85288,
	};

    void InitializeAI() override
    {
        me->SetOverrideDisplayPowerId(237);
        ScriptedAI::InitializeAI();
    }

	void JustEngagedWith(Unit* who) override
	{
		npc_theramore_horde::JustEngagedWith(who);

		scheduler
			.Schedule(1ms, [this](TaskContext bloodthirst)
			{
                DoCastVictim(SPELL_BLOODTHIRST);
                bloodthirst.Repeat(5s, 8s);
			})
			.Schedule(2s, [this](TaskContext raging_blow)
			{
                DoCastVictim(SPELL_RAGING_BLOW);
                raging_blow.Repeat(1s, 3s);
			});
	}
};

struct npc_roknah_loasinger : public npc_theramore_horde
{
	npc_roknah_loasinger(Creature* creature) : npc_theramore_horde(creature, AI_Type::Distance),
		ascending(false)
	{
		ascendanceInfo = sSpellMgr->AssertSpellInfo(SPELL_ASCENDANCE, DIFFICULTY_NONE);
		flameShockInfo = sSpellMgr->AssertSpellInfo(SPELL_FLAME_SHOCK, DIFFICULTY_NONE);
		frostShockInfo = sSpellMgr->AssertSpellInfo(SPELL_FROST_SHOCK, DIFFICULTY_NONE);
	}

	enum Groups
	{
		GROUP_NORMAL,       // Rotation offensive (DPS + interrupt + AOE)
		GROUP_HEALING,      // Routines de soin sur les allies
		GROUP_ASCENDANCE,   // Sequence defensive declenchee a 40% PV
	};

	enum Spells
	{
		SPELL_HEALING_RAIN      = 73920,
		SPELL_UNLEASH_LIFE      = 73685,
		SPELL_HEALING_WAVE      = 77472,
		SPELL_EARTHQUAKE        = 160162,
		SPELL_ASCENDANCE        = 173160,
		SPELL_GUST_OF_WIND      = 204853,
		SPELL_LAVA_BURST        = 290423,
		SPELL_WIND_SHEAR        = 290439,
		SPELL_ASTRAL_SHIFT      = 292158,
		SPELL_CHAIN_LIGHTNING   = 290411,
		SPELL_FLAME_SHOCK       = 290422,
		SPELL_FROST_SHOCK       = 290441,
		SPELL_RIPTIDE           = 241892,
		SPELL_CHAIN_HEAL        = 258099,
		SPELL_WATER_BLAST       = 450908,
		SPELL_HEALING_TIDE      = 127945,
		SPELL_LIGHTNING_BOLT    = 1246687,
	};

	static constexpr uint8 ASCENDANCE_HP_PCT             = 40;       // PV qui declenche la sequence defensive
	static constexpr float SHOCK_RANGE                   = 30.0f;    // Portee des shocks
	static constexpr float WIND_SHEAR_RANGE              = 35.0f;    // Portee du kick
	static constexpr float EARTHQUAKE_RANGE              = 8.0f;     // Distance de detection AOE
	static constexpr uint32 EARTHQUAKE_THRESHOLD         = 3;        // Au-dela de N ennemis -> Earthquake

	// Seuils de heal : (range, pct PV).
	static constexpr float HEAL_FRIENDLY_RANGE           = 40.0f;    // Portee standard des soins cibles
	static constexpr uint8 HEALING_SURGE_PCT             = 60;       // Spot heal
	static constexpr uint8 RIPTIDE_PCT                   = 80;       // HoT preventif
	static constexpr uint8 CHAIN_HEAL_PCT                = 50;       // Heal multi-cible
	static constexpr float HEALING_RAIN_RANGE            = 80.0f;    // Healing Rain (gros radius)
	static constexpr uint8 HEALING_TIDE_PCT              = 30;        // Healing Tide Totem (urgence absolue)
	static constexpr float HEALING_TIDE_RANGE            = 60.0f;    // Healing Tide (gros radius)
	static constexpr float CHAIN_HEAL_FRIENDLY_RANGE     = 30.0f;    // Chain Heal (radius des sauts)

	const SpellInfo* ascendanceInfo;
	const SpellInfo* flameShockInfo;
	const SpellInfo* frostShockInfo;

	bool ascending; // True une fois la sequence Ascendance declenchee (one-shot par combat)

	void Reset() override
	{
		npc_theramore_horde::Reset();
		ascending = false;
	}

	// -------------------------------------------------------------------------
	// Reactions
	// -------------------------------------------------------------------------

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spell*/) override
	{
		// Sequence defensive declenchee une seule fois par combat (ou par cooldown si reset).
		// Le flag `ascending` evite l'empilement de plusieurs sequences entre le declenchement
		// et le cast effectif d'Ascendance (qui pose le cooldown).
		if (ascending)
			return;

		if (!me->HealthBelowPctDamaged(ASCENDANCE_HP_PCT, damage))
			return;

		if (me->GetSpellHistory()->HasCooldown(ascendanceInfo))
			return;

		ascending = true;

		// Gel la rotation offensive pendant toute la duree d'Ascendance.
		scheduler.DelayGroup(GROUP_NORMAL, 30s);

		scheduler.Schedule(1ms, GROUP_ASCENDANCE, [this](TaskContext ascendance)
		{
			switch (ascendance.GetRepeatCounter())
			{
				case 0: // Reduction de degats immediate
					DoCastSelf(SPELL_ASTRAL_SHIFT);
					ascendance.Repeat(400ms);
					break;
				case 1: // Forme Ascendance (heals deviennent instants)
					DoCastSelf(SPELL_ASCENDANCE);
					break;
			}
		});
	}

	// Shock instant en sortie de backpedal.
	void OnBackpedStart(Unit* /*victim*/) override
	{
		DoCastSelf(RAND(SPELL_GUST_OF_WIND, SPELL_WATER_BLAST), TRIGGERED_CAST_DIRECTLY | TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CAST_IN_PROGRESS);
	}

	// -------------------------------------------------------------------------
	// Rotation
	// -------------------------------------------------------------------------

	void JustEngagedWith(Unit* who) override
	{
		npc_theramore_horde::JustEngagedWith(who);

		// Premier Lightning Bolt immediat sur l'agresseur.
		DoCast(who, SPELL_LIGHTNING_BOLT);

		scheduler

			// === BUFF ===

			.Schedule(5s, GROUP_NORMAL, [this](TaskContext unleash_life)
			{
				CastStop();
				DoCastSelf(SPELL_UNLEASH_LIFE);
				unleash_life.Repeat(23s, 34s);
			})

			// === DPS ===

			// Chain Lightning sur cible aleatoire toutes les 3-5s.
			.Schedule(8s, 14s, GROUP_NORMAL, [this](TaskContext chain_lightning)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_CHAIN_LIGHTNING);
				chain_lightning.Repeat(3s, 5s);
			})
			// Frost Shock sur cible qui ne l'a pas deja.
			.Schedule(5s, 8s, GROUP_NORMAL, [this](TaskContext frost_shock)
			{
				if (Unit* target = DoFindEnemyMissingDot(frostShockInfo))
					DoCast(target, SPELL_FROST_SHOCK);
				frost_shock.Repeat(8s, 10s);
			})
			// Flame Shock sur cible qui ne l'a pas deja (DoT).
			.Schedule(5s, 8s, GROUP_NORMAL, [this](TaskContext flame_shock)
			{
				if (Unit* target = DoFindEnemyMissingDot(flameShockInfo))
					DoCast(target, SPELL_FLAME_SHOCK);
				flame_shock.Repeat(5s, 8s);
			})
			// Earthquake si 3+ ennemis colles.
			.Schedule(20s, 25s, GROUP_NORMAL, [this](TaskContext earthquake)
			{
				if (EnemiesInRange(EARTHQUAKE_RANGE) >= EARTHQUAKE_THRESHOLD)
				{
					DoCast(SPELL_EARTHQUAKE);
					earthquake.Repeat(10s, 13s);
				}
				else
					earthquake.Repeat(1s);
			})
			// Lava Burst sur la victime courante.
			.Schedule(11s, 15s, GROUP_NORMAL, [this](TaskContext lava_burst)
			{
				CastStop();
				DoCastVictim(SPELL_LAVA_BURST);
				lava_burst.Repeat(8s, 10s);
			})
			// Wind Shear : kick les casters ennemis dans la portee.
			.Schedule(1s, GROUP_NORMAL, [this](TaskContext wind_shear)
			{
				if (Unit* target = DoSelectCastingUnit(SPELL_WIND_SHEAR, WIND_SHEAR_RANGE))
				{
					CastStop();
					DoCast(target, SPELL_WIND_SHEAR);
					wind_shear.Repeat(10s, 18s);
				}
				else
					wind_shear.Repeat(1s);
			})
			// Lightning Bolt en continu (filler, 2.8s = cast time approche).
			.Schedule(1s, GROUP_NORMAL, [this](TaskContext lightning_bolt)
			{
				DoCastVictim(SPELL_LIGHTNING_BOLT);
				lightning_bolt.Repeat(3200ms);
			})

			// === HEAL ===

			// Healing Wave : spot heal sur allie sous 30% PV (toutes les 3s).
			.Schedule(1s, GROUP_HEALING, [this](TaskContext healing_wave)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(HEAL_FRIENDLY_RANGE, HEALING_SURGE_PCT))
				{
					CastStop(SPELL_HEALING_WAVE);
					DoCast(target, SPELL_HEALING_WAVE);
				}
				healing_wave.Repeat(3s);
			})
			// Riptide : HoT preventif sur allie sous 60% PV qui n'a pas deja le HoT.
			.Schedule(1s, GROUP_HEALING, [this](TaskContext riptide)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(HEAL_FRIENDLY_RANGE, RIPTIDE_PCT, SPELL_RIPTIDE))
					DoCast(target, SPELL_RIPTIDE);
				riptide.Repeat(2s);
			})
			// Chain Heal : heal multi-cible si un allie est sous 40% PV.
			.Schedule(2s, GROUP_HEALING, [this](TaskContext chain_heal)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(HEAL_FRIENDLY_RANGE, CHAIN_HEAL_PCT))
				{
					CastStop();
					DoCast(target, SPELL_CHAIN_HEAL);
				}
				chain_heal.Repeat(8s, 14s);
			})
			// Healing Rain : zone de soin si un allie est tres bas. Cooldown 24-32s.
			.Schedule(8s, GROUP_HEALING, [this](TaskContext healing_rain)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(HEALING_RAIN_RANGE, CHAIN_HEAL_PCT))
					DoCast(target, SPELL_HEALING_RAIN);
				healing_rain.Repeat(24s, 32s);
			})
			// Healing Tide Totem : urgence absolue (allie a 5% PV). Re-check toutes les 2s.
			.Schedule(15s, GROUP_HEALING, [this](TaskContext healing_tide)
			{
				if (DoSelectBelowHpPctFriendly(HEALING_TIDE_RANGE, HEALING_TIDE_PCT))
				{
					CastStop();
					DoCast(SPELL_HEALING_TIDE);
				}
				healing_tide.Repeat(2s);
			});
	}
};

struct npc_roknah_felcaster : public npc_theramore_horde
{
	npc_roknah_felcaster(Creature* creature) : npc_theramore_horde(creature, AI_Type::Distance),
		soulShardsCount(0)
	{
		corruptionInfo = sSpellMgr->AssertSpellInfo(SPELL_CORRUPTION, DIFFICULTY_NONE);
	}

	enum Groups
	{
		GROUP_NORMAL,       // Rotation offensive (Trait de l'ombre / Bolt infernal, Main de Gul'dan)
		GROUP_DEFENSIVE     // Drain de vie / Etreinte mortelle declenches sur seuil PV
	};

	enum Spells
	{
		SPELL_SHADOWBOLT            = 686,
		SPELL_MORTAL_COIL           = 6789,
		SPELL_DARK_PACT             = 108416,
		SPELL_DRAIN_LIFE            = 149992,
		SPELL_SUMMON_FELHUNTER      = 285232,
		SPELL_CORRUPTION            = 251406,
		SPELL_DEMONBOLT             = 264178,
        SPELL_DEMONIC_CORE_BUFF     = 270176,
		SPELL_CALL_DREADSTALKERS    = 464874,
		SPELL_HAND_OF_GULDAN        = 464895,
		SPELL_INFERNAL_BOLT         = 434506,
		SPELL_INFERNAL_BOLT_BUFF    = 433891,
		SPELL_RUINATION             = 434635,
		SPELL_RUINATION_BUFF        = 433885,
		SPELL_SHARDS                = 1279442,
	};

	const SpellInfo* corruptionInfo;

	uint8 soulShardsCount = 0;

	static constexpr float   DOT_RANGE              = 30.0f;            // Portee de selection pour les DoTs
	static constexpr uint8   SOUL_SHARDS_MAX        = 3;                // Seuil de fragments d'ame avant Main de Gul'dan
	static constexpr uint8   DRAIN_LIFE_HP_PCT      = 30;               // Drain de vie sous N% PV
	static constexpr uint8   MORTAL_COIL_HP_PCT     = 40;               // Etreinte mortelle sous N% PV
	static constexpr uint32  FELHUNTER_CHANCE       = 60;               // % de chance d'invoquer un Traqueur des Tenebres au pull
	static constexpr Seconds DRAIN_LIFE_CHANNEL     = 6s;               // Duree du channel de Drain de vie (gel la rotation)

	void InitializeAI() override
	{
		me->SetOverrideDisplayPowerId(217);
		ScriptedAI::InitializeAI();
	}

	void Reset() override
	{
		npc_theramore_horde::Reset();

		soulShardsCount = 0;

		// Traqueur des Tenebres au pull : 60% de chance.
		if (roll_chance(FELHUNTER_CHANCE))
			DoCastSelf(SPELL_SUMMON_FELHUNTER, TRIGGERED_CAST_DIRECTLY);
	}

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spell*/) override
	{
		if (me->HealthBelowPctDamaged(60, damage)
			&& !me->HasAura(SPELL_DARK_PACT)
			&& !me->GetSpellHistory()->HasCooldown(SPELL_DARK_PACT))
		{
			CastStop(SPELL_DRAIN_LIFE);
			DoCastSelf(SPELL_DARK_PACT);
		}
	}

    void OnAuraApplied(AuraApplication const* aurApp) override
    {
        // On ne s'interesse qu'aux buffs procs auto-appliques.
        if (aurApp->GetBase()->GetCasterGUID() != me->GetGUID())
            return;

        SpellInfo const* spellInfo = aurApp->GetBase()->GetSpellInfo();

        DoCastSpellWithBuff(spellInfo, SPELL_DEMONIC_CORE_BUFF, SPELL_DEMONBOLT);
        DoCastSpellWithBuff(spellInfo, SPELL_RUINATION_BUFF, SPELL_RUINATION);
    }

	// Le cumul de shards se fait a la fin du cast (avant l'impact) pour rester aligne
	// avec le rythme de la rotation : Repeat(2300ms) doit pouvoir s'enchainer sans attendre le projectile.
	void OnSpellCast(SpellInfo const* spell) override
	{
		if (spell->Id == SPELL_SHADOWBOLT)
		{
			if (soulShardsCount < SOUL_SHARDS_MAX)
				soulShardsCount++;
		}
		else if (spell->Id == SPELL_DEMONBOLT)
		{
			// Demonbolt genere 2 fragments d'ame (consomme Demonic Core).
			soulShardsCount = std::min<uint8>(SOUL_SHARDS_MAX, soulShardsCount + 2);
		}
		else if (spell->Id == SPELL_INFERNAL_BOLT)
		{
			soulShardsCount = SOUL_SHARDS_MAX;
			me->RemoveAurasDueToSpell(SPELL_INFERNAL_BOLT_BUFF);
		}
		else if (spell->Id == SPELL_RUINATION)
		{
			soulShardsCount = 0;
			me->RemoveAurasDueToSpell(SPELL_RUINATION_BUFF);
		}
		else if (spell->Id == SPELL_HAND_OF_GULDAN)
			soulShardsCount = 0;

		// Main de Gul'dan : obligatoire quand 3 fragments d'ame sont accumules, interrompt tout sauf Drain de vie.
		DoCastHandOfGuldan();
	}


	// -------------------------------------------------------------------------
	// Rotation
	// -------------------------------------------------------------------------

	void JustEngagedWith(Unit* who) override
	{
		me->AddAura(SPELL_SHARDS, me);

		npc_theramore_horde::JustEngagedWith(who);

		DoCast(who, SPELL_SHADOWBOLT);

		scheduler
			// === DEFENSIVE (GROUP_DEFENSIVE) ===

			// Drain de vie : channel 6s sous 30% PV, gel toute la rotation pendant ce temps.
			.Schedule(1s, GROUP_DEFENSIVE, [this](TaskContext drain_life)
			{
				if (HealthBelowPct(DRAIN_LIFE_HP_PCT))
				{
					if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					{
						scheduler.DelayAll(DRAIN_LIFE_CHANNEL);
						CastStop();
						DoCast(target, SPELL_DRAIN_LIFE);
					}
				}

				drain_life.Repeat(1s);
			})
			// Etreinte mortelle : peur sur cible la plus eloignee, sous 50% PV.
			.Schedule(1s, GROUP_DEFENSIVE, [this](TaskContext mortal_coil)
			{
				if (HealthBelowPct(MORTAL_COIL_HP_PCT)
                    && !me->GetSpellHistory()->HasCooldown(SPELL_MORTAL_COIL))
				{
					if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
					{
						CastStop();
						DoCast(target, SPELL_MORTAL_COIL);
					}
				}

				mortal_coil.Repeat(1s);
			})

			// === DPS (GROUP_NORMAL) ===

			// Trait de l'ombre en filler (toutes les 4s) ; remplace par Bolt infernal si le buff est actif.
			.Schedule(3s, GROUP_NORMAL, [this](TaskContext shadowbolt)
			{
				CastStop({ SPELL_HAND_OF_GULDAN, SPELL_INFERNAL_BOLT });

				if (me->HasAura(SPELL_INFERNAL_BOLT_BUFF))
					DoCastVictim(SPELL_INFERNAL_BOLT);
				else
					DoCastVictim(SPELL_SHADOWBOLT);

				shadowbolt.Repeat(2300ms);
			})
            // Invocation de demons supplementaires pour declencher Demonic Core.
			.Schedule(5s, GROUP_NORMAL, [this](TaskContext call_dreadstalkers)
			{
				CastStop({ SPELL_HAND_OF_GULDAN, SPELL_INFERNAL_BOLT });
                DoCastVictim(SPELL_CALL_DREADSTALKERS);
                call_dreadstalkers.Repeat(20s, 25s);
			})

			// === DoTs ===

			// Corruption : applique le DoT toutes les 3s sur les cibles qui ne l'ont pas.
			.Schedule(1s, GROUP_NORMAL, [this](TaskContext corruption)
			{
				if (Unit* target = DoFindEnemyMissingDot(corruptionInfo))
					DoCast(target, SPELL_CORRUPTION);
				corruption.Repeat(3s);
			});
	}

    // -------------------------------------------------------------------------
	// Helpers
	// -------------------------------------------------------------------------

    void DoCastSpellWithBuff(SpellInfo const* spellInfo, uint32 buff, uint32 spell, Optional<SelectTargetMethod> selectTargetMethod = {})
    {
        if (spellInfo->Id != buff)
            return;

        scheduler.Schedule(1s, 3s, [this, spell, selectTargetMethod](TaskContext /*context*/)
        {
            CastStop(SPELL_DRAIN_LIFE);

            // Recherche une cible en fonction du predicat
            Unit* target = me->GetVictim();
            if (selectTargetMethod)
                target = SelectTarget(*selectTargetMethod);

            DoCast(target, spell);
        });
    }

    void DoCastHandOfGuldan()
    {
        if (soulShardsCount >= SOUL_SHARDS_MAX)
        {
            CastStop(SPELL_DRAIN_LIFE);
            DoCastVictim(SPELL_HAND_OF_GULDAN);
        }
    }
};

struct npc_wave_caller_gruhta : public CustomAI
{
	const uint8 MAX_ELEMENTAL_PROTECTION = 10;

	npc_wave_caller_gruhta(Creature* creature) : CustomAI(creature, true, AI_Type::Hybrid),
		stormkeeperStacks(0)
	{
		SetCanRandomMovement(false);

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
		SPELL_LIGHTNING_STORM       = 447930,
		SPELL_WATER_WALKING         = 73757,
	};

	enum Misc
	{
		GROUP_NORMAL                = 1,
		GROUP_TEMPEST,
		GROUP_CHECKER,
		GROUP_STORMKEEPER
	};

	InstanceScript* instance;
	uint32 stormkeeperStacks;
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

	void SpellHit(WorldObject* caster, SpellInfo const* spell) override
	{
		CustomAI::SpellHit(caster, spell);

		if (Aura* elementalProtection = me->GetAura(SPELL_ELEMENTAL_PROTECTION))
		{
			// R?cup?re le nombre de protections activent
			uint32 stack = elementalProtection->GetStackAmount();

			// Supprime une protection lors des d?g?ts
			elementalProtection->SetStackAmount(stack - 1);

			// Si le stack est ? z?ro, la temp?te s'annule
			if (stack <= 0)
			{
				scheduler.CancelGroup(GROUP_TEMPEST);

				EnterCombatPhase();

				return;
			}
		}
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		scheduler
			.Schedule(1ms, GROUP_CHECKER, [this](TaskContext context)
				{
					if (me->HealthBelowPct(40.f))
					{
						context.CancelGroup(GROUP_CHECKER);
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

						// Marche sur l'eau pour les cibles
						for (auto const& itr : me->GetThreatManager().GetUnsortedThreatList())
						{
							if (Unit* victim = itr->GetVictim())
								victim->AddAura(SPELL_WATER_WALKING, victim);
						}
					}
					else
						context.Repeat(1s);
				});

		EnterCombatPhase();
	}

	void EnterCombatPhase()
	{
		scheduler
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
	npc_faithful_training(Creature* creature) : npc_theramore_faithful(creature) { }

	enum Misc
	{
		COSMETIC_GROUP      = 0,
	};

	ObjectGuid soldierAGuid;
	ObjectGuid soldierBGuid;

	// Pr?pare un soldat pour le combat cosm?tique (emote, ?tat, cible, HP r?duits).
	void SetSoldierState(Creature* creature, Emote emote, Creature* target)
	{
		if (!creature || !target)
			return;

		creature->SetEmoteState(emote);
		creature->SetReactState(REACT_PASSIVE);
		creature->SetUnitFlag(UNIT_FLAG_PACIFIED);
		creature->SetRegenerateHealth(false);

		uint32 health = static_cast<uint32>(creature->GetMaxHealth() * 0.3f);
		creature->SetHealth(health);
		creature->SetTarget(target->GetGUID());
	}

	// Pr?pare le healer (emote idle, passif, pas de cible, HP non modifi?s).
	void SetHealerState(Creature* creature)
	{
		if (!creature)
			return;

		creature->SetEmoteState(EMOTE_STATE_NONE);
		creature->SetReactState(REACT_PASSIVE);
		creature->SetUnitFlag(UNIT_FLAG_PACIFIED);
	}

	// Restaure l'?tat normal d'une cr?ature apr?s la phase cosm?tique.
	void ClearState(Creature* creature)
	{
		if (!creature)
			return;

		creature->SetRegenerateHealth(true);
		creature->SetHealth(creature->GetMaxHealth());
		creature->SetTarget(ObjectGuid::Empty);

		float angle = creature->GetAbsoluteAngle(LookAtPos);
		creature->SetOrientation(angle);
		creature->SetFacingToPoint(LookAtPos);
		creature->SetReactState(REACT_AGGRESSIVE);
		creature->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
	}

	void Reset() override
	{
		npc_theramore_faithful::Reset();

		if ((BFTPhases)instance->GetData(DATA_SCENARIO_PHASE) > BFTPhases::Preparation)
			return;

		std::vector<Creature*> soldiers;
		me->GetCreatureListWithEntryInGrid(soldiers, NPC_THERAMORE_FOOTMAN, 15.0f);

		if (soldiers.size() < 2)
			return;

		Creature* soldierA = soldiers[0];
		Creature* soldierB = soldiers[1];

		if (!soldierA || !soldierB)
			return;

		soldierAGuid = soldierA->GetGUID();
		soldierBGuid = soldierB->GetGUID();

		SetHealerState(me);
		SetSoldierState(soldierA, EMOTE_STATE_ATTACK1H, soldierB);
		SetSoldierState(soldierB, EMOTE_STATE_BLOCK_SHIELD, soldierA);

		scheduler
			.Schedule(2s, COSMETIC_GROUP, [this](TaskContext checkPhase)
			{
				BFTPhases phase = (BFTPhases)instance->GetData(DATA_SCENARIO_PHASE);
				if (phase >= BFTPhases::Preparation)
				{
					ClearState(me);

					if (Creature* soldierA = ObjectAccessor::GetCreature(*me, soldierAGuid))
						ClearState(soldierA);

					if (Creature* soldierB = ObjectAccessor::GetCreature(*me, soldierBGuid))
						ClearState(soldierB);

					scheduler.CancelGroup(COSMETIC_GROUP);
				}
				else
					checkPhase.Repeat(2s);
			})
			.Schedule(2s, COSMETIC_GROUP, [this](TaskContext heal)
			{
				Creature* soldierA = ObjectAccessor::GetCreature(*me, soldierAGuid);
				Creature* soldierB = ObjectAccessor::GetCreature(*me, soldierBGuid);

				if (!soldierA || !soldierB)
					return;

				if (Creature* victim = RAND(soldierA, soldierB))
					me->CastSpell(victim, RAND(SPELL_FLASH_HEAL, SPELL_GREATER_HEAL, SPELL_POWER_WORD_SHIELD));

				heal.Repeat(3s, 5s);
			})
			.Schedule(2s, 8s, COSMETIC_GROUP, [this](TaskContext soldiers)
			{
				Creature* soldierA = ObjectAccessor::GetCreature(*me, soldierAGuid);
				Creature* soldierB = ObjectAccessor::GetCreature(*me, soldierBGuid);

				if (!soldierA || !soldierB)
					return;

				uint32 damage = urand(1000, 2000);
				soldierA->DealDamage(soldierB, soldierA, damage);
				soldierB->DealDamage(soldierA, soldierB, damage);

				soldiers.Repeat(2s, 5s);
			});
	}
};

struct npc_arcanist_training : public npc_theramore_arcanist
{
	npc_arcanist_training(Creature* creature) : npc_theramore_arcanist(creature, AI_Type::Stay)
	{
		SetCanRandomMovement(false);

		SpellInfo const* evocationInfo = sSpellMgr->GetSpellInfo(SPELL_EVOCATION, DIFFICULTY_NONE);
		m_evocationDuration = evocationInfo
			? Milliseconds(evocationInfo->CalcDuration(creature))
			: 8s;
	}

	enum Groups
	{
		COSMETIC_GROUP_NORMAL   = 0,
		COSMETIC_GROUP_MISSILES = 1,
	};

	enum Spells
	{
		SPELL_EVOCATION = 243070,
	};

	Milliseconds m_evocationDuration;

	void SpellHit(WorldObject* /*caster*/, SpellInfo const* spell) override
	{
		if (spell->Id != SPELL_CLEARCASTING)
			return;

		scheduler.Schedule(1s, 3s, COSMETIC_GROUP_MISSILES, [this](TaskContext /*context*/)
		{
			Creature* training = GetClosestCreatureWithEntry(me, NPC_TRAINING_DUMMY, 15.f);
			if (!training)
				return;

			scheduler.DelayGroup(COSMETIC_GROUP_NORMAL, 3s);

			CastStop();
			me->GetSpellHistory()->ResetCooldown(SPELL_ARCANE_MISSILES, true);
			me->CastSpell(training, SPELL_ARCANE_MISSILES, true);
			me->RemoveAurasDueToSpell(SPELL_CLEARCASTING);
		});
	}

	void SpellHitTarget(WorldObject* object, SpellInfo const* spell) override
	{
		npc_theramore_arcanist::SpellHitTarget(object, spell);

		if (spell->Id == SPELL_ARCANE_BLAST && roll_chance(60))
			DoCastSelf(SPELL_CLEARCASTING);
	}

	void Reset() override
	{
		npc_theramore_arcanist::Reset();

		if ((BFTPhases)instance->GetData(DATA_SCENARIO_PHASE) > BFTPhases::Preparation)
			return;

		scheduler
			.Schedule(2s, COSMETIC_GROUP_NORMAL, [this](TaskContext checkPhase)
			{
				BFTPhases phase = (BFTPhases)instance->GetData(DATA_SCENARIO_PHASE);
				if (phase >= BFTPhases::Preparation)
				{
					me->CombatStop();
					me->SetOrientation(me->GetAbsoluteAngle(LookAtPos));
					me->SetFacingToPoint(LookAtPos);

					scheduler.CancelGroup(COSMETIC_GROUP_NORMAL);
					scheduler.CancelGroup(COSMETIC_GROUP_MISSILES);
				}
				else
					checkPhase.Repeat(2s);
			})
			.Schedule(2s, COSMETIC_GROUP_NORMAL, [this](TaskContext context)
			{
				int32 manaPct = me->GetPowerPct(Powers::POWER_MANA);
				if (manaPct > 0 && manaPct <= 5) // Percent
				{
					scheduler.DelayGroup(COSMETIC_GROUP_NORMAL,   m_evocationDuration);
					scheduler.DelayGroup(COSMETIC_GROUP_MISSILES, m_evocationDuration);
					DoCast(SPELL_EVOCATION);
				}

				context.Repeat(2s);
			})
			.Schedule(2s, COSMETIC_GROUP_NORMAL, [this](TaskContext context)
			{
				if (me->GetVictim())
					return;

				Creature* training = GetClosestCreatureWithEntry(me, NPC_TRAINING_DUMMY, 15.f);
				if (!training)
					return;

				me->Attack(training, false);
				context.Repeat(2s);
			});
	}
};

struct npc_dummy_training : NullCreatureAI
{
	npc_dummy_training(Creature* creature) : NullCreatureAI(creature) {}

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
		damage = 0;
	}
};

// Flurry - 284858
class spell_roknah_hag_flurry : public SpellScript
{
	enum Misc
	{
		SPELL_MAGE_FLURRY_DAMAGE = 228596
	};

	class FlurryEvent : public BasicEvent
	{
	public:
		FlurryEvent(Unit* caster, ObjectGuid const& target, ObjectGuid const& originalCastId, int32 count)
			: _caster(caster), _target(target), _originalCastId(originalCastId), _count(count) { }

		bool Execute(uint64 time, uint32 /*diff*/) override
		{
			Unit* target = ObjectAccessor::GetUnit(*_caster, _target);

			if (!target)
				return true;

			_caster->CastSpell(target, SPELL_MAGE_FLURRY_DAMAGE, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS).SetOriginalCastId(_originalCastId));

			if (!--_count)
				return true;

			_caster->m_Events.AddEvent(this, Milliseconds(time) + randtime(300ms, 400ms));
			return false;
		}

	private:
		Unit* _caster;
		ObjectGuid _target;
		ObjectGuid _originalCastId;
		int32 _count;
	};

	bool Validate(SpellInfo const* /*spell*/) override
	{
		return ValidateSpellInfo({ SPELL_MAGE_FLURRY_DAMAGE });
	}

	void EffectHit(SpellEffIndex /*effIndex*/) const
	{
		GetCaster()->m_Events.AddEventAtOffset(new FlurryEvent(GetCaster(), GetHitUnit()->GetGUID(), GetSpell()->m_castId, GetEffectValue() - 1), randtime(300ms, 400ms));
	}

	void Register() override
	{
		OnEffectHitTarget += SpellEffectFn(spell_roknah_hag_flurry::EffectHit, EFFECT_0, SPELL_EFFECT_DUMMY);
	}
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
			SpellRange radius = GetSpellInfo()->GetEffect(effIndex).CalcRadius();

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
					trigger->GetCreatureListWithEntryInGrid(fires, NPC_THERAMORE_FIRE_CREDIT, radius.Max);

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

// 279910 - Wild Imp
class spell_wild_imp : public SpellScript
{
	struct SlotDef
	{
		float radius;
		float angleFromBehind; // 0 = pile derriere ; positif = gauche du caster, negatif = droite
	};

	static constexpr uint8 IMP_MAX_COUNT = 12;

	// 5 imps sur l'arc interieur (R = 2.0m) + 7 sur l'arc exterieur (R = 3.5m),
	// repartis sur le demi-cercle exact derriere le lanceur (180�, d'epaule a epaule).
	static constexpr std::array<SlotDef, IMP_MAX_COUNT> SLOTS = { {
		// Inner ring : 5 slots, pas de pi/4
		{ 2.0f, -float(M_PI_2) },
		{ 2.0f, -float(M_PI_4) },
		{ 2.0f,  0.f },
		{ 2.0f,  float(M_PI_4) },
		{ 2.0f,  float(M_PI_2) },
		// Outer ring : 7 slots, pas de pi/6
		{ 3.5f, -float(M_PI_2) },
		{ 3.5f, -float(M_PI) / 3.f },
		{ 3.5f, -float(M_PI) / 6.f },
		{ 3.5f,  0.f },
		{ 3.5f,  float(M_PI) / 6.f },
		{ 3.5f,  float(M_PI) / 3.f },
		{ 3.5f,  float(M_PI_2) },
	} };

	static Position SlotToWorld(Unit const* caster, SlotDef const& def)
	{
		float worldAngle = caster->GetOrientation() + float(M_PI) + def.angleFromBehind;
		float x = caster->GetPositionX() + def.radius * std::cos(worldAngle);
		float y = caster->GetPositionY() + def.radius * std::sin(worldAngle);
		return Position(x, y, caster->GetPositionZ(), caster->GetOrientation());
	}

	void HandleSummon(SpellEffIndex effIndex)
	{
		PreventHitDefaultEffect(effIndex);

		Unit* caster = GetCaster();
		if (!caster)
			return;

        SpellEffectInfo const& effInfo = GetEffectInfo();
        const uint32 wildImpEntry = effInfo.MiscValue;

		// Pre-calcul des 12 positions de slot dans la frame actuelle du caster.
		std::array<Position, IMP_MAX_COUNT> slotPositions;
		for (uint8 s = 0; s < IMP_MAX_COUNT; ++s)
			slotPositions[s] = SlotToWorld(caster, SLOTS[s]);

		// Derive les slots occupes via "nearest slot" : chaque imp est rattache au slot le plus proche.
		std::array<bool, IMP_MAX_COUNT> occupied{};
		uint8 count = 0;
		for (Unit* controlled : caster->m_Controlled)
		{
			if (!controlled || controlled->GetEntry() != wildImpEntry)
				continue;

			++count;

			uint8 bestSlot = 0;
			float bestDistSq = std::numeric_limits<float>::max();
			for (uint8 s = 0; s < IMP_MAX_COUNT; ++s)
			{
				float d2 = controlled->GetExactDist2dSq(&slotPositions[s]);
				if (d2 < bestDistSq)
				{
					bestDistSq = d2;
					bestSlot = s;
				}
			}
			ASSERT(bestSlot < IMP_MAX_COUNT);
			occupied[bestSlot] = true;
		}

		// Au-dela de la limite : le sort se cast quand meme, mais aucun summon.
		if (count >= IMP_MAX_COUNT)
			return;

		uint8 freeSlot = 0;
		for (; freeSlot < IMP_MAX_COUNT; ++freeSlot)
			if (!occupied[freeSlot])
				break;

		// Garde-fou : si la derivation "nearest slot" a sature tous les slots alors que count < 12 (theoriquement impossible), on abandonne.
		if (freeSlot >= IMP_MAX_COUNT)
			return;

		SummonPropertiesEntry const* properties = sSummonPropertiesStore.LookupEntry(uint32(effInfo.MiscValueB));
		Milliseconds duration = Milliseconds(GetSpellInfo()->CalcDuration(caster));

		TempSummon* summon = caster->GetMap()->SummonCreature(wildImpEntry, slotPositions[freeSlot], properties, duration, caster, GetSpellInfo()->Id);
		if (!summon)
			return;

        if (caster->IsInCombat())
        {
            Unit* victim = caster->GetVictim();
            if (victim)
                summon->Attack(victim, true);
        }

		// Chaque imp suit le lanceur dans son slot relatif : la formation se maintient meme si le caster bouge ou tourne.
		summon->GetMotionMaster()->MoveFollow(caster, SLOTS[freeSlot].radius, ChaseAngle(float(M_PI) + SLOTS[freeSlot].angleFromBehind));
	}

	void Register() override
	{
		OnEffectLaunch += SpellEffectFn(spell_wild_imp::HandleSummon, EFFECT_0, SPELL_EFFECT_SUMMON);
	}
};

// 464895 - Hand of Gul'dan
class spell_hand_of_guldan : public SpellScript
{
	enum Spells
	{
		SPELL_HAND_OF_GULDAN            = 464895,
		SPELL_HAND_OF_GULDAN_DAMAGE     = 464890,
		SPELL_WILD_IMP                  = 279910,
	};

	bool Validate(SpellInfo const* /*spellInfo*/) override
	{
		return ValidateSpellInfo({ SPELL_HAND_OF_GULDAN_DAMAGE, SPELL_WILD_IMP });
	}

	void HandleDummy(SpellEffIndex effIndex)
	{
		Unit* caster = GetCaster();
		Unit* target = GetHitUnit();

		if (!caster || !target)
			return;

		caster->CastSpell(target->GetPosition(), SPELL_HAND_OF_GULDAN_DAMAGE, CastSpellExtraArgsInit{
			.TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
		});

		const uint8 wildImp = GetEffectInfo(effIndex).BasePoints;
		for (uint8 i = 0; i < wildImp; ++i)
			caster->CastSpell(caster, SPELL_WILD_IMP, CastSpellExtraArgsInit{
			.TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
		});
	}

	void Register() override
	{
		OnEffectHitTarget += SpellEffectFn(spell_hand_of_guldan::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
	}
};

// 295710 - Light of Dawn
class spell_light_of_dawn : public SpellScript
{
	enum Spells
	{
		SPELL_LIGHT_OF_DAWN_HEAL = 295712
	};

	bool Validate(SpellInfo const* /*spellInfo*/) override
	{
		return ValidateSpellInfo({ SPELL_LIGHT_OF_DAWN_HEAL });
	}

	void HandleDummy(SpellEffIndex /*effIndex*/) const
	{
		Unit* caster = GetCaster();
		Unit* target = GetHitUnit();

		if (!caster || !target)
			return;

		if (caster->IsValidAttackTarget(target))
			return;

		caster->CastSpell(target, SPELL_LIGHT_OF_DAWN_HEAL, CastSpellExtraArgsInit{
			.TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
			.TriggeringSpell = GetSpell()
			});
	}

	void Register() override
	{
		OnEffectHitTarget += SpellEffectFn(spell_light_of_dawn::HandleDummy, EFFECT_1, SPELL_EFFECT_DUMMY);
	}
};

// Blizzard - 284968
// AreaTriggerID - 15411
struct at_blizzard_theramore : AreaTriggerAI
{
	using AreaTriggerAI::AreaTriggerAI;

	enum Spells
	{
		SPELL_BLIZZARD_DAMAGE = 335953
	};

	void OnCreate(Spell const* /*creatingSpell*/) override
	{
		_scheduler.Schedule(1s, [this](TaskContext task)
		{
			if (Unit* caster = at->GetCaster())
			{
				for (ObjectGuid unit : at->GetInsideUnits())
				{
					if (Unit* target = ObjectAccessor::GetUnit(*caster, unit))
					{
						if (!caster->IsHostileTo(target))
							continue;

						caster->CastSpell(target, SPELL_BLIZZARD_DAMAGE, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
					}
				}
			}

			task.Repeat(1s);
		});
	}

	void OnUpdate(uint32 diff) override
	{
		_scheduler.Update(diff);
	}

private:
	TaskScheduler _scheduler;
};

// Consecrated Ground
// AreaTriggerID - 34355
struct at_consecration : AreaTriggerAI
{
	using AreaTriggerAI::AreaTriggerAI;

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

	void OnUnitExit(Unit* unit, AreaTriggerExitReason /*reason*/) override
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
	using AreaTriggerAI::AreaTriggerAI;

	enum Spells
	{
		SPELL_DIVINE_WORD_SANCTUARY_HEAL = 372787
	};

	void OnCreate(Spell const* /*creatingSpell*/) override
	{
		_scheduler.Schedule(1s, [this](TaskContext task)
		{
			// R?cup?ration du lanceur du sort
			if (Unit* caster = at->GetCaster())
			{
				for (const ObjectGuid& unitGuid : at->GetInsideUnits())
				{
					if (Unit* target = ObjectAccessor::GetUnit(*caster, unitGuid))
					{
						if (caster->IsHostileTo(target))
							continue;

						caster->CastSpell(target, SPELL_DIVINE_WORD_SANCTUARY_HEAL);
					}
				}
			}

			task.Repeat(1s);
		});
	}

	void OnUpdate(uint32 diff) override
	{
		_scheduler.Update(diff);
	}

private:
	TaskScheduler _scheduler;
};

// Uncontrolled Energy
// AreaTriggerID - 26658
struct at_uncontrolled_energy : AreaTriggerAI
{
	using AreaTriggerAI::AreaTriggerAI;

	enum Spells
	{
		SPELL_ARCANE_RIFT_EXPLOSION = 388996
	};

	void OnUnitEnter(Unit* unit) override
	{
		if (Unit* caster = at->GetCaster())
		{
			if (caster->IsFriendlyTo(unit))
				return;

			caster->CastSpell(at->GetPosition(), SPELL_ARCANE_RIFT_EXPLOSION, true);
			at->SetDuration(0);
		}
	}
};

// Scorched Earth - 373139
// AreaTriggerID - 25183
struct at_scorched_earth : AreaTriggerAI
{
	using AreaTriggerAI::AreaTriggerAI;

	enum Spells
	{
		SPELL_SCORCHED_EARTH = 372820
	};

	void OnUnitEnter(Unit* unit) override
	{
		if (Unit* caster = at->GetCaster())
		{
			if (caster->IsFriendlyTo(unit))
				return;

			unit->AddAura(SPELL_SCORCHED_EARTH, unit);
		}
	}

	void OnUnitExit(Unit* unit, AreaTriggerExitReason /*reason*/) override
	{
		unit->RemoveAurasDueToSpell(SPELL_SCORCHED_EARTH);
	}
};

// Arcane Orb - 440458
// AreaTriggerID - 32530
struct at_arcane_orb : AreaTriggerAI
{
	using AreaTriggerAI::AreaTriggerAI;

	enum Spells
	{
		SPELL_ARCANE_ORB_DAMAGE = 440552
	};

	void OnInitialize() override
	{
		Position destPos = at->GetPosition();
		at->MovePositionToFirstCollision(destPos, 40.0f, 0.0f);

		std::vector<G3D::Vector3> points;
		points.emplace_back(at->GetPositionX(), at->GetPositionY(), at->GetPositionZ());
		points.emplace_back(destPos.GetPositionX(), destPos.GetPositionY(), destPos.GetPositionZ());

		// Avoid degenerate spline (zero length -> timeToTarget = 0 -> NaN in UpdateSplinePosition)
		if ((points[1] - points[0]).squaredMagnitude() < 0.01f)
		{
			at->Remove();
			return;
		}

		at->InitSplines(points);
	}

	void OnUnitEnter(Unit* unit) override
	{
		Unit* caster = at->GetCaster();
		if (!caster)
			return;

		if (caster->IsValidAttackTarget(unit))
			caster->CastSpell(unit, SPELL_ARCANE_ORB_DAMAGE, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CAST_IN_PROGRESS);
	}

	void OnDestinationReached() override
	{
		at->Remove();
	}
};

// Blessed Hammer - 420092
// AreaTriggerID - 29807
struct at_blessed_hammer : AreaTriggerAI
{
	using AreaTriggerAI::AreaTriggerAI;

	enum Spells
	{
		SPELL_BLESSED_HAMMER_DAMAGE = 420091
	};

	void OnCreate(Spell const* /*creatingSpell*/) override
	{
		Unit* caster = at->GetCaster();
		if (!caster)
			return;

		Position center  = caster->GetPosition();
		float startAngle = caster->GetOrientation();
		float fixedZ     = center.GetPositionZ() + Z_OFFSET;

		std::vector<G3D::Vector3> path;
		path.reserve(POINTS + 1);

		for (std::size_t i = 0; i <= POINTS; ++i)
		{
			float t     = float(i) / float(POINTS);
			float theta = t * TURNS * 2.0f * float(M_PI);
			float r     = R0 + GROWTH * theta;

			float x = center.GetPositionX() + r * std::cos(startAngle + theta);
			float y = center.GetPositionY() + r * std::sin(startAngle + theta);

			path.emplace_back(x, y, fixedZ);
		}

		at->InitSplines(std::move(path));
	}

	void OnUnitEnter(Unit* unit) override
	{
		Unit* caster = at->GetCaster();
		if (!caster)
			return;

		if (caster->IsValidAttackTarget(unit))
			caster->CastSpell(unit, SPELL_BLESSED_HAMMER_DAMAGE, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CAST_IN_PROGRESS);
	}

private:
	// AreaTrigger 29807 dure 5 s : on ?tale la spirale sur toute la dur?e.
	static constexpr std::size_t POINTS = 200;    // Densit? totale (40 pts/seconde)
	static constexpr float TURNS        = 10.0f;  // 10 tours sur 5 s = 2 tours/s
	static constexpr float R0           = 2.0f;   // Rayon de d?part
	static constexpr float GROWTH       = 0.35f;  // Expansion par radian (spirale d'Archim?de)
	static constexpr float Z_OFFSET     = 1.5f;   // Hauteur du marteau au-dessus du sol
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
	RegisterTheramoreAI(npc_dummy_training);
	RegisterTheramoreAI(npc_wave_caller_gruhta);

	// Utilisables dans les Ruines de Theramore
	RegisterCreatureAI(npc_roknah_hag);
	RegisterCreatureAI(npc_roknah_grunt);
	RegisterCreatureAI(npc_roknah_loasinger);
	RegisterCreatureAI(npc_roknah_felcaster);

	RegisterSpellScript(spell_roknah_hag_flurry);
	RegisterSpellScript(spell_theramore_light_of_dawn);
	RegisterSpellScript(spell_theramore_throw_bucket);
	RegisterSpellScript(spell_powder_keg);
	RegisterSpellScript(spell_wild_imp);
	RegisterSpellScript(spell_hand_of_guldan);
	RegisterSpellScript(spell_light_of_dawn);

	RegisterAreaTriggerAI(at_blizzard_theramore);
	RegisterAreaTriggerAI(at_consecration);
	RegisterAreaTriggerAI(at_divine_word_sanctuary);
	RegisterAreaTriggerAI(at_uncontrolled_energy);
	RegisterAreaTriggerAI(at_scorched_earth);
	RegisterAreaTriggerAI(at_arcane_orb);
	RegisterAreaTriggerAI(at_blessed_hammer);
}
