#include "Custom/AI/CustomAI.h"
#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "DB2Stores.h"
#include "InstanceScript.h"
#include "KillRewarder.h"
#include "MotionMaster.h"
#include "Object.h"
#include "PassiveAI.h"
#include "ScriptMgr.h"
#include "SpellAuras.h"
#include "TemporarySummon.h"
#include "dalaran_purge.h"

struct npc_jaina_dalaran_patrol : public CustomAI
{
	npc_jaina_dalaran_patrol(Creature* creature) : CustomAI(creature)
	{
		Initialize();
	}

	enum Spells
	{
        SPELL_FROST_BARRIER         = 69787,
		SPELL_COSMETIC_SNOW         = 83065,
		SPELL_BLIZZARD              = 284968,
		SPELL_FROSTBOLT             = 284703,
		SPELL_FRIGID_SHARD          = 354933,
		SPELL_TELEPORT              = 135176,
		SPELL_GLACIAL_SPIKE         = 338488,
	};

	void Initialize() override
	{
		CustomAI::Initialize();

		instance = me->GetInstanceScript();

		me->AddAura(SPELL_COSMETIC_SNOW, me);
	}

	InstanceScript* instance;

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
    {
        if (!me->HasAura(SPELL_FROST_BARRIER)
            && me->HealthBelowPctDamaged(15, damage))
        {
            DoCast(SPELL_FROST_BARRIER);
        }
    }

	void JustEngagedWith(Unit* who) override
	{
		DoCast(who, SPELL_GLACIAL_SPIKE);

		scheduler
			.Schedule(2s, [this](TaskContext frostbolt)
			{
				DoCastVictim(SPELL_FROSTBOLT);
				frostbolt.Repeat(2s);
			})
			.Schedule(10s, [this](TaskContext glacial_spike)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
				{
					CastStop(SPELL_FRIGID_SHARD);
					DoCast(target, SPELL_GLACIAL_SPIKE);
				}
				glacial_spike.Repeat(15s, 32s);
			})
			.Schedule(4s, [this](TaskContext blizzard)
			{
				if (me->GetThreatManager().GetThreatListSize() >= 2)
				{
					CastStop(SPELL_BLIZZARD);
					DoCastVictim(SPELL_BLIZZARD);
					blizzard.Repeat(5s, 8s);
				}
				else
					blizzard.Repeat(2s);
			})
			.Schedule(8s, [this](TaskContext frigid_shard)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_FRIGID_SHARD);
				frigid_shard.Repeat(10s, 12s);
			});
	}

	void KilledUnit(Unit* victim) override
	{
		if (roll_chance_i(30)
			&& victim->GetEntry() == NPC_SUNREAVER_CITIZEN
			&& !victim->HasAura(SPELL_TELEPORT))
		{
			me->AI()->Talk(SAY_JAINA_PURGE_SLAIN);
		}

		if (victim->GetEntry() == NPC_SUNREAVER_CITIZEN)
		{
            if (Player* player = instance->instance->GetPlayers().begin()->GetSource())
                KillRewarder(player, victim, false).Reward(victim->GetEntry());
		}
	}

	void MoveInLineOfSight(Unit* who) override
	{
		if (me->IsEngaged() || !me->isActiveObject())
			return;

		if (who->GetEntry() != NPC_SUNREAVER_CITIZEN)
		{
			ScriptedAI::MoveInLineOfSight(who);
		}
		else
		{
			if (roll_chance_i(30)
				&& who->IsWithinLOSInMap(me)
				&& who->IsWithinDist(me, 25.f)
				&& !who->HasAura(SPELL_TELEPORT))
			{
				if (who->IsEngaged())
				{
					ScriptedAI::MoveInLineOfSight(who);
				}
				else
				{
					if (roll_chance_i(60))
						me->AI()->Talk(SAY_JAINA_PURGE_TELEPORT);

					DoCast(who, SPELL_TELEPORT);
				}
			}
			else
			{
				ScriptedAI::MoveInLineOfSight(who);
			}
		}
	}
};

struct npc_silver_covenant_assassin : public CustomAI
{
	npc_silver_covenant_assassin(Creature* creature) : CustomAI(creature, AI_Type::Melee)
	{
		Initialize();
	}

	enum Spells
	{
		SPELL_SPRINT                = 66060,
		SPELL_FAN_OF_KNIVES         = 273606,
		SPELL_SINISTER_STRIKE       = 172028,
		SPELL_STEALTH               = 228928
	};

	void Reset() override
	{
		if (roll_chance_i(20))
		{
			scheduler.Schedule(1ms, [this](TaskContext stealth)
			{
				if (!me->HasAura(SPELL_STEALTH))
					DoCastSelf(SPELL_STEALTH);
				stealth.Repeat(5s);
			});
		}
	}

	void JustEngagedWith(Unit* who) override
	{
		if (!me->HasAura(SPELL_SPRINT) && !me->IsWithinCombatRange(who, true))
			DoCast(SPELL_SPRINT);

		scheduler
			.Schedule(2s, 8s, [this](TaskContext sinister_strike)
			{
				DoCastVictim(SPELL_SINISTER_STRIKE);
				sinister_strike.Repeat(5s, 8s);
			})
			.Schedule(5s, 10s, [this](TaskContext fan_of_knives)
			{
				DoCast(SPELL_FAN_OF_KNIVES);
				fan_of_knives.Repeat(15s, 24s);
			});
	}
};

struct npc_mage_commander_zuros : public CustomAI
{
	static constexpr float DAMAGE_REDUCTION = 0.01f;

	npc_mage_commander_zuros(Creature* creature) : CustomAI(creature, AI_Type::Melee)
	{
		Initialize();
	}

	enum Spells
	{
		SPELL_ACCELERATING_BLAST    = 203176,
		SPELL_NETHER_WOUND          = 211000,
		SPELL_TIME_STOP             = 279062
	};

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
		if (me->HealthBelowPctDamaged(25, damage))
			damage *= DAMAGE_REDUCTION;
		else
			damage = 0;
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		scheduler
			.Schedule(2s, [this](TaskContext nether_wound)
			{
				DoCastVictim(SPELL_NETHER_WOUND);
				nether_wound.Repeat(8s, 15s);
			})
			.Schedule(8s, [this](TaskContext accelerating_blast)
			{
				DoCastVictim(SPELL_ACCELERATING_BLAST);
				accelerating_blast.Repeat(24s, 30s);
			})
			.Schedule(12s, [this](TaskContext time_stop)
			{
				DoCastVictim(SPELL_TIME_STOP);
				time_stop.Repeat(45s, 1min);
			});
	}
};

struct npc_arcanist_rathaella : public CustomAI
{
	const Position sorinPos = { 5799.18f, 638.95f, 647.58f, 0.17f };

	npc_arcanist_rathaella(Creature* creature) : CustomAI(creature, AI_Type::Melee)
	{
		Initialize();
	}

	enum Talks
	{
		SAY_ARCANIST_RATHAELLA_01   = 0,
		SAY_ARCANIST_RATHAELLA_02   = 1,
		SAY_ARCANIST_RATHAELLA_03   = 2
	};

	enum Spells
	{
		SPELL_FREE_CAPTIVE          = 312101,
	};

	InstanceScript* instance;
	ObjectGuid playerGUID;

	void Initialize() override
	{
		CustomAI::Initialize();

		instance = me->GetInstanceScript();
	}

	void OnSpellClick(Unit* clicker, bool spellClickHandled) override
	{
		if (!spellClickHandled)
			return;

		if (clicker->GetTypeId() != TYPEID_PLAYER)
			return;

		playerGUID = clicker->GetGUID();
	}

	void MovementInform(uint32 /*type*/, uint32 id) override
	{
		switch (id)
		{
			case MOVEMENT_INFO_POINT_01:
				me->SetFacingTo(5.41f);
				me->AI()->Talk(SAY_ARCANIST_RATHAELLA_03);
				scheduler.Schedule(4s, [this](TaskContext /*context*/)
				{
					me->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_02, RathaellaPath02, RATHAELLA_PATH_02);
					if (Player* player = ObjectAccessor::GetPlayer(*me, playerGUID))
						KillRewarder(player, me, false).Reward(me->GetEntry());
				});

				break;
			case MOVEMENT_INFO_POINT_02:
				if (Creature* sorin = instance->GetCreature(DATA_SORIN_MAGEHAND))
					me->SetFacingToObject(sorin);
				me->AddAura(SPELL_CASTER_READY_01, me);
				me->SetHomePosition(me->GetPosition());
				break;
		}
	}

	void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
	{
		if (spellInfo->Id != SPELL_FREE_CAPTIVE)
			return;

		me->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
		me->RemoveAurasDueToSpell(SPELL_ATTACHED);
		me->AI()->Talk(SAY_ARCANIST_RATHAELLA_01);

		scheduler.Schedule(3s, [caster, this](TaskContext context)
		{
			switch (context.GetRepeatCounter())
			{
				case 0:
					me->SetStandState(UNIT_STAND_STATE_STAND);
					context.Repeat(2s);
					break;
				case 1:
					if (Player* player = ObjectAccessor::GetPlayer(*me, playerGUID))
						me->SetFacingToObject(player);
					context.Repeat(1s);
					break;
				case 2:
					me->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_01, RathaellaPath01, RATHAELLA_PATH_01, true, false);
					context.Repeat(2s);
					break;
				case 3:
					me->AI()->Talk(SAY_ARCANIST_RATHAELLA_02);
					break;
			}
		});
	}
};

struct npc_sorin_magehand : public NullCreatureAI
{
	npc_sorin_magehand(Creature* creature) : NullCreatureAI(creature)
	{
	}

	enum Misc
	{
		SPELL_ARCANE_BARRIER        = 264849,
		SPELL_RUNES_OF_SHIELDING    = 217859
	};

	void JustAppeared() override
	{
		me->SetReactState(REACT_PASSIVE);
		me->CastSpell(me, SPELL_ARCANE_BARRIER, true);
		me->CastSpell(me, SPELL_RUNES_OF_SHIELDING, true);
	}

	void Reset() override
	{
		scheduler.CancelAll();
	}

	void UpdateAI(uint32 diff) override
	{
		scheduler.Update(diff);
	}

	private:
	TaskScheduler scheduler;
};

struct npc_sunreaver_citizen : public CustomAI
{
	npc_sunreaver_citizen(Creature* creature) : CustomAI(creature)
	{
		Initialize();
	}

	enum Spells
	{
		SPELL_SCORCH                = 17195,
		SPELL_FIREBALL              = 358226,
	};

	InstanceScript* instance;

	void Initialize()
	{
		instance = me->GetInstanceScript();

		me->SetEmoteState(EMOTE_STATE_COWER);
	}

	void AttackStart(Unit* who) override
	{
		if (!who)
			return;

		if (me->HasUnitState(UNIT_STATE_FLEEING_MOVE))
			me->GetMotionMaster()->Clear();

		me->SetEmoteState(EMOTE_STATE_NONE);

		if (me->Attack(who, true))
		{
			me->CallAssistance();

			DoStartMovement(who, GetDistance());

			SetCombatMovement(true);
		}
	}

	void JustEngagedWith(Unit* who) override
	{
		DoCast(who, SPELL_FIREBALL);

		scheduler
			.Schedule(2s, [this](TaskContext fireball)
			{
				DoCastVictim(SPELL_FIREBALL);
				fireball.Repeat(2s);
			})
			.Schedule(3s, 5s, [this](TaskContext scorch)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				{
					CastStop(SPELL_SCORCH);
					DoCast(target, SPELL_SCORCH);
				}
				scorch.Repeat(5s, 8s);
			});
	}
};

struct npc_sunreaver_mage : public CustomAI
{
	npc_sunreaver_mage(Creature* creature) : CustomAI(creature)
	{
		Initialize();
	}

	enum Spells
	{
		SPELL_MOLTEN_ARMOR          = 79849,
		SPELL_FIREBALL              = 79854,
		SPELL_FLAMESTRIKE           = 330347,
		SPELL_RINF_OF_FIRE          = 353082
	};

	void Reset() override
	{
		scheduler.Schedule(1ms, [this](TaskContext spell_molten_armor)
		{
			if (!me->HasAura(SPELL_MOLTEN_ARMOR))
				DoCastSelf(SPELL_MOLTEN_ARMOR);
			spell_molten_armor.Repeat(5s);
		});
	}

	void JustEngagedWith(Unit* who) override
	{
		DoCast(who, SPELL_FIREBALL);

		scheduler
			.Schedule(2s, [this](TaskContext fireball)
			{
				DoCastVictim(SPELL_FIREBALL);
				fireball.Repeat(2s);
			})
			.Schedule(3s, 5s, [this](TaskContext flamestrike)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_FLAMESTRIKE);
				flamestrike.Repeat(14s, 22s);
			})
			.Schedule(10s, 15s, [this](TaskContext rinf_of_fire)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				{
					CastStop(SPELL_FLAMESTRIKE);
					DoCast(target, SPELL_RINF_OF_FIRE);
				}
				rinf_of_fire.Repeat(20s, 25s);
			});
	}
};

struct npc_sunreaver_aegis : public CustomAI
{
	npc_sunreaver_aegis(Creature* creature) : CustomAI(creature, AI_Type::Melee), healthLow(false)
	{
		Initialize();
	}

	enum Spells
	{
		SPELL_DIVINE_SHIELD         = 642,
		SPELL_HEAL                  = 225638,
		SPELL_AVENGING_WRATH        = 292266,
		SPELL_HOLY_LIGHT            = 315535,
		SPELL_BLESSING_OF_FREEDOM   = 299256,
		SPELL_BLESSING_OF_MIGHT     = 79977,
		SPELL_DIVINE_STORM          = 183897,
		SPELL_JUDGMENT              = 295671,
		SPELL_RETRIBUTION_AURA      = 79976
	};

	bool healthLow;

	void Reset() override
	{
		scheduler.Schedule(1ms, [this](TaskContext buffs)
		{
			if (!me->HasAura(SPELL_BLESSING_OF_MIGHT))
				DoCastSelf(SPELL_BLESSING_OF_MIGHT);

			if (!me->HasAura(SPELL_RETRIBUTION_AURA))
				DoCastSelf(SPELL_RETRIBUTION_AURA);

			buffs.Repeat(5s);
		});
	}

	void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
	{
		CustomAI::SpellHit(caster, spellInfo);

		if ((spellInfo->GetAllEffectsMechanicMask() & ((1 << MECHANIC_ROOT) | (1 << MECHANIC_SNARE))) != 0)
		{
			scheduler.Schedule(1s, [this](TaskContext /*context*/)
			{
				CastStop();
				DoCastSelf(SPELL_BLESSING_OF_FREEDOM);
			});
		}
	}

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
		if (!healthLow && me->HealthBelowPctDamaged(25, damage))
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
		if (roll_chance_i(30))
			DoCastSelf(SPELL_AVENGING_WRATH);

		scheduler
			.Schedule(2s, 4s, [this](TaskContext holy_light)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 80))
					DoCast(target, SPELL_HOLY_LIGHT);
				holy_light.Repeat(8s, 15s);
			})
			.Schedule(5s, 8s, [this](TaskContext judgment)
			{
				DoCastVictim(SPELL_JUDGMENT);
				judgment.Repeat(12s, 15s);
			})
			.Schedule(10s, 14s, [this](TaskContext divine_storm)
			{
				if (EnemiesInRange(8.0f) >= 3)
				{
					CastStop({ SPELL_HEAL, SPELL_HOLY_LIGHT });
					DoCast(SPELL_DIVINE_STORM);
					divine_storm.Repeat(12s, 25s);
				}
				else
					divine_storm.Repeat(1s);
			});
	}
};

struct npc_sunreaver_captain : public CustomAI
{
	const Position center = { 5915.39f, 535.81f, 650.06f, 3.14f };

	npc_sunreaver_captain(Creature* creature) : CustomAI(creature, AI_Type::Melee)
	{
	}

	enum Spells
	{
		SPELL_BATTER                = 66408,
		SPELL_RECENTLY_BANDAGED     = 11196,
		SPELL_RISING_ANGER          = 136323,
		SPELL_MORTAL_CLEAVE         = 177147,
		SPELL_WHIRLWIND             = 277637,
		SPELL_HEW                   = 319957,
		SPELL_BANDAGE               = 333552,
		SPELL_VICIOUS_WOUND         = 334960
	};

	enum Misc
	{
		SAY_WANTON_HOSTESS_FLEE     = 2,
		GOB_PORTAL_TO_SILVERMOON    = 323854
	};

	std::vector<Creature*> hosts;

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
		if (!me->HasAura(SPELL_RECENTLY_BANDAGED) && me->HealthBelowPctDamaged(25, damage))
		{
			me->AddAura(SPELL_RECENTLY_BANDAGED, me);

			DoCast(SPELL_BANDAGE);
		}
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		DoFleeWantonHosts();

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

	void DoFleeWantonHosts()
	{
		GetCreatureListWithEntryInGrid(hosts, me, NPC_WANTON_HOST, 60.f);
		GetCreatureListWithEntryInGrid(hosts, me, NPC_WANTON_HOSTESS, 60.f);

		if (!hosts.empty())
		{
			for (Creature* host : hosts)
			{
				host->SetEmoteState(EMOTE_STATE_NONE);
				host->SetStandState(UNIT_STAND_STATE_STAND);
				host->RemoveAllAuras();

				if (host->GetEntry() == NPC_WANTON_HOSTESS)
					host->AI()->Talk(SAY_WANTON_HOSTESS_FLEE);

				const Position destination = GetRandomPosition(center, 10.f);
				scheduler
					.Schedule(2s, 5s, [host, destination](TaskContext /*context*/)
					{
						host->SetEmoteState(EMOTE_STATE_COWER);
						host->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, destination);
					});
			}
		}
	}
};

struct npc_magister_brasael : public CustomAI
{
	static constexpr uint32 BAGS_COUNT = 50;
	static constexpr uint32 DAMAGE_LIMITATION = 5000;

	npc_magister_brasael(Creature* creature) : CustomAI(creature, AI_Type::NoMovement),
		cauterized(false), damagedBags(0)
	{
	}

	enum Misc
	{
		// Spells
		SPELL_SURVIVOR_BAG          = 138208,
		SPELL_GOLDEN_MOSS           = 148559,
		SPELL_RING_OF_FIRE          = 353103,
		// Talks
		SAY_BRASAEL_01              = 0
	};

	enum Spells
	{
		SPELL_HAND_OF_THAURISSAN    = 17492,
		SPELL_FIREBALL              = 79854,
		SPELL_INCANTER_FLOW         = 116267,
		SPELL_CAUTERIZE             = 175620,
		SPELL_COMBUSTION            = 190319,
		SPELL_BLAST_WAVE            = 270285,
		SPELL_PHOENIX_FLAMES        = 257541,
		SPELL_METEOR                = 153561,
		SPELL_DRAGON_BREATH         = 255890,
	};

	bool cauterized;
	uint32 damagedBags;

	void SetData(uint32 /*id*/, uint32 /*value*/) override
	{
		DoAction(ACTION_DISPELL_BARRIER);
	}

	void DoAction(int32 action) override
	{
		if (action != ACTION_DISPELL_BARRIER)
			return;

		me->SetImmuneToAll(true);
		me->SetEmoteState(EMOTE_STATE_NONE);
		me->SetFacingTo(2.65f);

		scheduler.Schedule(2s, [this](TaskContext /*context*/)
		{
			me->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_01, BrasaelPos01, true, BrasaelPos01.GetOrientation());
		});
	}

	void MovementInform(uint32 /*type*/, uint32 id) override
	{
		switch (id)
		{
			case MOVEMENT_INFO_POINT_01:
				me->AI()->Talk(SAY_BRASAEL_01);
				me->SetHomePosition(BrasaelPos01);
				me->RemoveAurasDueToSpell(SPELL_HOLD_BAG);
				scheduler
					.Schedule(1s, [this](TaskContext ring_of_fire)
					{
						switch (ring_of_fire.GetRepeatCounter())
						{
							case 4:
								me->SetImmuneToAll(false);
								break;
							default:
								DoCast(SPELL_GOLDEN_MOSS);
								DoCast(SPELL_RING_OF_FIRE);
								ring_of_fire.Repeat(500ms, 800ms);
								break;
						}
					});
				break;
		}
	}

	void Initialize() override
	{
		CustomAI::Initialize();

		cauterized = false;
		damagedBags = 0;
	}

	void Reset() override
	{
		CustomAI::Reset();

		if (Creature* barrier = GetClosestCreatureWithEntry(me, NPC_ARCANE_BARRIER, 60.f))
		{
			barrier->SetOwnerGUID(me->GetGUID());
			barrier->SetObjectScale(1.2f);
		}

		for (uint8 i = 0; i < BAGS_COUNT; i++)
			me->AddAura(SPELL_SURVIVOR_BAG, me);
	}

	void JustDied(Unit* killer) override
	{
		CustomAI::JustDied(killer);

		me->RemoveAllDynObjects();
		me->RemoveAllAreaTriggers();
	}

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
		// L'aura de [Sac de survivant] est obligatoire pour appliquer la technique Cautérisation
		if (Aura* bags = me->GetAura(SPELL_SURVIVOR_BAG))
		{
			// Récupère le nombre de sacs
			uint32 stack = bags->GetStackAmount();
			if (stack <= 0)
				return;

			// Ajoute les dégâts
			damagedBags += damage;

			// Si les dégâts infligés sont supérieurs à la limite de dégâts
			if (damagedBags >= DAMAGE_LIMITATION)
			{
				// On enlève un sac et on réinitilise les dégâts infligés
				damagedBags = 0;
				bags->SetStackAmount(stack - 1);
			}

			// Si Brasael n'a pas encore utilisé Cautérisation ou si la technique n'est pas encore rechargée (1 min)
			if (cauterized)
				return;

			// Si Brasael est en dessous de 25% de sa vie après que les dégâts actuels soient appliquées
			if (me->HealthBelowPctDamaged(25, damage))
			{
				// On supprime les dégâts actuels
				damage = 0;

				// On met une sécurité
				cauterized = true;

				// On interrompt tous les sorts
				CastStop();

				// La technique Cautérisation dépend du nombre de sacs qui n'ont pas été enlevés par les dégâts
				CastSpellExtraArgs args(TRIGGERED_FULL_MASK);
				args.SetOriginalCaster(me->GetGUID());
				args.AddSpellMod(SPELLVALUE_BASE_POINT0, 8);        // 8% de la vie maximum
				args.AddSpellMod(SPELLVALUE_BASE_POINT1, stack);    // Le pourcentage de soin dépend du nombre de sac

				// On lance Cautérisation avec les arguments de lancement
				DoCast(me, SPELL_CAUTERIZE, args);

				// On attend 1min avant de relancer Cautérisation
				scheduler.Schedule(1min, [this](TaskContext /*context*/)
				{
					cauterized = false;
				});
			}
		}
	}

	void SpellHitTarget(WorldObject* target, SpellInfo const* spellInfo) override
	{
		if (spellInfo->Id == SPELL_FIREBALL)
			DoCast(me, SPELL_INCANTER_FLOW, true);
		else if (spellInfo->Id == SPELL_HAND_OF_THAURISSAN)
		{
			CastStop();
			if (Unit* victim = target->ToUnit())
				DoCast(victim, SPELL_METEOR);
		}
	}

	void DamageDealt(Unit* /*victim*/, uint32& damage, DamageEffectType damageType) override
	{
		if (damageType == DIRECT_DAMAGE || damageType == SPELL_DIRECT_DAMAGE || damageType == DOT)
		{
			if (me->HasAura(SPELL_COMBUSTION))
				damage *= 1.25f;

			uint32 incanterCount = me->GetAuraCount(SPELL_INCANTER_FLOW);
			if (incanterCount > 0)
			{
				int32 incanterBonus = incanterCount * 0.5f;
				damage *= incanterBonus;
			}
		}
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		DoCast(SPELL_COMBUSTION);

		scheduler
			.Schedule(1s, [this](TaskContext fireball)
			{
				DoCastVictim(SPELL_FIREBALL);
				fireball.Repeat(1800ms);
			})
			.Schedule(5s, 15s, [this](TaskContext blast_wave)
			{
				CastStop(SPELL_CAUTERIZE);
				DoCast(SPELL_BLAST_WAVE);
				blast_wave.Repeat(15s);
			})
			.Schedule(8s, 12s, [this](TaskContext dragon_breath)
			{
				if (EnemiesInFront(6.f) > 2)
				{
					CastStop(SPELL_CAUTERIZE);
					DoCast(SPELL_DRAGON_BREATH);
					dragon_breath.Repeat(32s);
				}
				else
					dragon_breath.Repeat();
			})
			.Schedule(3s, [this](TaskContext phoenix_flames)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				{
					CastStop({ SPELL_CAUTERIZE, SPELL_BLAST_WAVE });
					DoCast(target, SPELL_PHOENIX_FLAMES);
				}
				phoenix_flames.Repeat(8s, 15s);
			})
			.Schedule(15s, [this](TaskContext meteor)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				{
					CastStop(SPELL_CAUTERIZE);
					DoCast(target, SPELL_METEOR);
					meteor.Repeat(32s, 45s);
				}
				else
					meteor.Repeat(15s);
			})
			.Schedule(2s, [this](TaskContext counterspell)
			{
				if (Unit* target = DoSelectCastingUnit(SPELL_HAND_OF_THAURISSAN, 35.f))
				{
					CastStop(SPELL_CAUTERIZE);
					DoCast(target, SPELL_HAND_OF_THAURISSAN);
					counterspell.Repeat(12s, 24s);
				}
				else
					counterspell.Repeat();
			});
	}

	bool CanAIAttack(Unit const* who) const
	{
		return who->IsAlive() && me->IsValidAttackTarget(who) && ScriptedAI::CanAIAttack(who);
	}
};

struct npc_magister_surdiel : public CustomAI
{
	static constexpr float DAMAGE_REDUCTION = 0.01f;

	npc_magister_surdiel(Creature* creature) : CustomAI(creature)
	{
		Initialize();
	}

	enum Spells
	{
		SPELL_FIREBALL              = 79854,
		SPELL_RAIN_OF_FIRE          = 156974,
		SPELL_PYROBLAST             = 246505,
		SPELL_FIRE_BOMB             = 270956
	};

	InstanceScript* instance;

	void Initialize() override
	{
		CustomAI::Initialize();

		instance = me->GetInstanceScript();
	}

	void Reset() override
	{
		if (Creature* barrier = GetClosestCreatureWithEntry(me, NPC_ARCANE_BARRIER, 60.f))
		{
			barrier->SetOwnerGUID(me->GetGUID());
			barrier->SetObjectScale(1.2f);
		}
	}

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
		if (me->HealthBelowPctDamaged(25, damage))
			damage *= DAMAGE_REDUCTION;
		else
			damage = 0;
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		scheduler
			.Schedule(1ms, [this](TaskContext fireball)
			{
				DoCastVictim(SPELL_FIREBALL);
				fireball.Repeat(2s);
			})
			.Schedule(2s, [this](TaskContext pyroblast)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_PYROBLAST);
				pyroblast.Repeat(15s, 28s);
			})
			.Schedule(5s, [this](TaskContext rain_of_fire)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				{
					CastStop();

					Position dest = target->GetPosition();
					me->CastSpell(dest, SPELL_RAIN_OF_FIRE);
				}
				rain_of_fire.Repeat(5min);
			})
			.Schedule(8s, [this](TaskContext fire_bomb)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_FIRE_BOMB);
				fire_bomb.Repeat(45s, 1min);
			});
	}
};

///
///     SPECIAL NPC
///

struct npc_archmage_landalock : public NullCreatureAI
{
	const Position sorinPos = { 5801.87f, 645.19f, 647.55f, 5.16f };

	npc_archmage_landalock(Creature* creature) : NullCreatureAI(creature), eventId(0)
	{
		instance = creature->GetInstanceScript();

		uint32 delay = std::numeric_limits<uint32>::max();
		me->SetRespawnDelay(delay);
		me->SetRespawnTime(delay);

		if (Creature* icewall = me->FindNearestCreature(NPC_ICEWALL, 15.f))
		{
			summon = icewall->GetPosition();

			icewall->SetRespawnDelay(delay);
			icewall->SetRespawnTime(delay);
		}
	}

	enum Talks
	{
		SAY_LANDALOCK_01,
		SAY_LANDALOCK_02,
		SAY_LANDALOCK_03,
		SAY_LANDALOCK_04,
		SAY_LANDALOCK_05,
	};

	enum Misc
	{
		// Spells
		SPELL_ICE_BURST             = 69108,
		// Gossips
		GOSSIP_MENU_DEFAULT         = 65003,
		// GameObjects
		GOB_ICEWALL                 = 368620,
		// NPCs
		NPC_ICEWALL                 = 178819
	};

	std::list<Creature*> citizens;
	EventMap events;
	InstanceScript* instance;
	ObjectGuid stalkerGUID;
	ObjectGuid playerGUID;
	ObjectGuid icewallGUID;
	Position summon;
	uint32 eventId;

	void Reset() override
	{
		me->SetImmuneToAll(true);
	}

	void SetGUID(ObjectGuid const& guid, int32 id) override
	{
		switch (id)
		{
			case GUID_PLAYER:
				playerGUID = guid;
				break;
			case GUID_STALKER:
				stalkerGUID = guid;
				break;
		}
	}

	void DoAction(int32 action) override
	{
		if (action != ACTION_DISPELL_BARRIER)
			return;

		Creature* stalker = ObjectAccessor::GetCreature(*me, stalkerGUID);
		if (!stalker)
			return;

		Player* player = ObjectAccessor::GetPlayer(*me, playerGUID);
		if (!player)
			return;

		citizens.clear();

		GetCreatureListWithEntryInGrid(citizens, stalker, NPC_DALARAN_CITIZEN, 35.f);
		if (!citizens.empty())
		{
			me->RemoveAllAuras();
			me->CastSpell(stalker->GetPosition(), SPELL_TELEPORT);

			events.ScheduleEvent(9, 800ms);
		}
	}

	void MovementInform(uint32 /*type*/, uint32 id) override
	{
		switch (id)
		{
			case MOVEMENT_INFO_POINT_01:
				if (Creature* sorin = instance->GetCreature(DATA_SORIN_MAGEHAND))
					me->SetFacingToObject(sorin);
				me->AddAura(SPELL_CASTER_READY_01, me);
				me->SetHomePosition(me->GetPosition());
				break;
		}
	}

	void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
	{
		if (spellInfo->Id != SPELL_TELEPORT_CASTER)
			return;

		me->DisappearAndDie();

		if (Player* player = ObjectAccessor::GetPlayer(*me, playerGUID))
		{
			for (Creature* citizen : citizens)
			{
				KillRewarder(player, citizen, false).Reward(citizen->GetEntry());

				citizen->CastSpell(citizen, SPELL_TELEPORT_TARGET);
				citizen->DisappearAndDie();
			}
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
				playerGUID = player->GetGUID();
				me->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
				me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
				me->RemoveAurasDueToSpell(SPELL_FROST_CANALISATION);
				me->RemoveAurasDueToSpell(SPELL_CHAT_BUBBLE);
				events.ScheduleEvent(1, 2s);
				break;
		}

		CloseGossipMenuFor(player);
		return true;
	}

	void UpdateAI(uint32 diff) override
	{
		events.Update(diff);

		while (eventId = events.ExecuteEvent())
		{
			switch (eventId)
			{
				// Wall
				#pragma region WALL

				case 1:
					me->AI()->Talk(SAY_LANDALOCK_01);
					if (Player* player = ObjectAccessor::GetPlayer(*me, playerGUID))
					{
						me->SetFacingToObject(player);
						KillRewarder(player, me, false).Reward(me->GetEntry());
					}
					Next(3s);
					break;
				case 2:
					me->AI()->Talk(SAY_LANDALOCK_02);
					Next(4s);
					break;
				case 3:
					me->SetFacingTo(5.55f);
					Next(2s);
					break;
				case 4:
					if (Creature* icewall = me->SummonCreature(WORLD_TRIGGER, summon, TEMPSUMMON_TIMED_DESPAWN, 10s))
					{
						me->HandleEmoteCommand(EMOTE_ONESHOT_POINT);
						icewall->CastSpell(icewall, SPELL_ICE_BURST);
					}
					Next(500ms);
					break;
				case 5:
					if (Creature* icewall = me->FindNearestCreature(NPC_ICEWALL, 15.f))
						icewall->KillSelf();
					if (GameObject* collider = me->FindNearestGameObject(GOB_ICEWALL, 15.f))
						collider->Delete();
					Next(2s);
					break;
				case 6:
					if (Player* player = ObjectAccessor::GetPlayer(*me, playerGUID))
						me->SetFacingToObject(player);
					Next(2s);
					break;
				case 7:
					me->AI()->Talk(SAY_LANDALOCK_03);
					me->SetWalk(true);
					Next(4s);
					break;
				case 8:
					me->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_01, sorinPos, true, sorinPos.GetOrientation());
					break;

				#pragma endregion

				// Citizen
				#pragma region CITIZEN

				case 9:
					if (Creature* barrier = GetClosestCreatureWithEntry(me, NPC_ARCANE_BARRIER, 15.f))
						me->SetFacingToObject(barrier);
					Next(1s);
					break;
				case 10:
					me->AI()->Talk(SAY_LANDALOCK_04);
					Next(2s);
					break;
				case 11:
				{
					uint8 index = 0;
					const Position center = me->GetPosition();
					float slice = 2 * (float)M_PI / (float)citizens.size();
					for (Creature* citizen : citizens)
					{
						uint32 delay = std::numeric_limits<uint32>::max();
						citizen->SetRespawnDelay(delay);
						citizen->SetRespawnTime(delay);

						float angle = slice * index;
						const Position dest = GetRandomPositionAroundCircle(me, angle, 3.2f);
						citizen->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_01, dest, true, dest.GetOrientation());

						index++;
					}
					Next(1s);
					break;
				}
				case 12:
				{
					std::list<Creature*> temp;
					GetCreatureListWithEntryInGrid(temp, me, NPC_DALARAN_CITIZEN, 6.f);
					if (temp.size() >= citizens.size())
					{
						Next(500ms);
						break;
					}
					events.Repeat(2s);
					break;
				}
				case 13:
					DoCast(SPELL_TELEPORT_CASTER);
					break;

				#pragma endregion
			}
		}
	}

	void Next(const Milliseconds& time)
	{
		eventId++;
		events.ScheduleEvent(eventId, time);
	}
};

struct npc_arcane_barrier : public NullCreatureAI
{
	const Position destination = { 5789.28f, 726.24f, 685.52f, 1.44f };

	npc_arcane_barrier(Creature* creature) : NullCreatureAI(creature)
	{
		instance = creature->GetInstanceScript();
	}

	enum Misc
	{
		// GameObjects
		GOB_COLLIDER                = 368679,
		// Spells
		SPELL_SLOW_FALL             = 88473,
		SPELL_WAND_OF_DISPELLING    = 234966,
		SPELL_FREED_EXPLOSION       = 225253,
		SPELL_ARCANE_BARRIER_DAMAGE = 264848,
		SPELL_ARCANE_BARRIER        = 271187
	};

	TaskScheduler scheduler;
	InstanceScript* instance;
	ObjectGuid mageGUID;
	ObjectGuid colliderGUID;

	void JustAppeared() override
	{
		me->AddAura(SPELL_ARCANE_BARRIER, me);

		if (GameObject* collider = me->SummonGameObject(GOB_COLLIDER, me->GetPosition(), QuaternionData::fromEulerAnglesZYX(me->GetOrientation(), 0.f, 0.f), 0s))
		{
			colliderGUID = collider->GetGUID();

			collider->AddFlag(GO_FLAG_NOT_SELECTABLE);
		}
	}

	void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
	{
		if (spellInfo->Id != SPELL_WAND_OF_DISPELLING)
			return;

		if (!mageGUID.IsEmpty())
			return;

		Player* player = caster->ToPlayer();
		if (!player)
			return;

		// Pas d'event pour la barrière de Surdiel et de Brasael
		if (Unit* owner = me->GetOwner())
		{
			if (Creature* creature = owner->ToCreature())
				creature->AI()->DoAction(ACTION_DISPELL_BARRIER);

			mageGUID = owner->GetGUID();

			DLPPhases phase = (DLPPhases)instance->GetData(DATA_SCENARIO_PHASE);
			if (phase == DLPPhases::KillBraseal && owner->GetEntry() == NPC_MAGISTER_BRASAEL)
			{
				Dispell(player);
			}
			else if (phase == DLPPhases::KillSurdiel && owner->GetEntry() == NPC_MAGISTER_SURDIEL)
			{
				Dispell(player);
			}
		}
		else
		{
			if (Creature* stalker = GetClosestCreatureWithEntry(me, NPC_INVISIBLE_STALKER, 25.f))
			{
				if (Creature* mage = me->SummonCreature(NPC_ARCHMAGE_LANDALOCK, stalker->GetPosition()))
				{
					if (GameObject* collider = ObjectAccessor::GetGameObject(*me, colliderGUID))
						collider->Delete();

					mageGUID = mage->GetGUID();

					// Set guids
					mage->RemoveAllAuras();
					mage->AI()->SetGUID(player->GetGUID(), GUID_PLAYER);
					mage->AI()->SetGUID(stalker->GetGUID(), GUID_STALKER);
					mage->AI()->DoAction(ACTION_DISPELL_BARRIER);

					me->setActive(false);
					me->RemoveAllAuras();
					me->DisappearAndDie();
				}
			}
		}
	}

	void UpdateAI(uint32 diff) override
	{
		scheduler.Update(diff);
	}

	void Dispell(Player* player)
	{
		if (GameObject* collider = ObjectAccessor::GetGameObject(*me, colliderGUID))
			collider->Delete();

		DoCast(SPELL_FREED_EXPLOSION);

		player->CastSpell(player, SPELL_ARCANE_BARRIER_DAMAGE);

		me->setActive(false);
		me->RemoveAllAuras();
		me->DisappearAndDie();
	}
};

// Glacial Spike
struct npc_glacial_spike : public NullCreatureAI
{
	npc_glacial_spike(Creature* creature) : NullCreatureAI(creature)
	{
	}

	enum Misc
	{
		SPELL_GLACIAL_SPIKE_COSMETIC    = 346559,
		SPELL_FROZEN_DESTRUCTION        = 356957,
	};

	void IsSummonedBy(WorldObject* summoner) override
	{
		owner = summoner->GetGUID();
	}

	void SpellHitTarget(WorldObject* target, SpellInfo const* spellInfo) override
	{
		if (spellInfo->Id == SPELL_FROZEN_DESTRUCTION)
		{
			if (Unit* victim = target->ToUnit())
			{
				if (victim->GetTypeId() != TYPEID_PLAYER)
				{
					if (Unit* summoner = ObjectAccessor::GetCreature(*me, owner))
						victim->GetAI()->AttackStart(summoner);
				}
			}
		}
	}

	void JustAppeared() override
	{
		me->SetUnitFlags(UNIT_FLAG_UNINTERACTIBLE);
		me->AddAura(SPELL_GLACIAL_SPIKE_COSMETIC, me);
		me->DespawnOrUnsummon(10s);

		scheduler.Schedule(5s, [this](TaskContext expire)
		{
			DoCast(SPELL_FROZEN_DESTRUCTION);
		});
	}

	void UpdateAI(uint32 diff) override
	{
		scheduler.Update(diff);
	}

	private:
	TaskScheduler scheduler;
	ObjectGuid owner;
};

// Arcane Barrier - 264849
// AreaTriggerID - 12833
struct at_arcane_barrier : AreaTriggerAI
{
	static constexpr Milliseconds TICK_PERIOD = Milliseconds(1000);

	at_arcane_barrier(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger), _tickTimer(TICK_PERIOD)
	{
	}

	enum Spells
	{
		SPELL_ARCANE_BARRIER_DAMAGE         = 264848
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

						target->CastSpell(target, SPELL_ARCANE_BARRIER_DAMAGE);
					}
				}
			}

			_tickTimer += TICK_PERIOD;
		}
	}

	private:
	Milliseconds _tickTimer;
	int32 timeInterval;
};

// Arcane Barrier - 271187
// AreaTriggerID - 13455
struct at_arcane_protection : AreaTriggerAI
{
	at_arcane_protection(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger)
	{
	}

	enum Spells
	{
		SPELL_ARCANE_BARRIER_DAMAGE         = 264848
	};

	void OnUnitEnter(Unit* unit) override
	{
		if (Unit* caster = at->GetCaster())
		{
			if (unit->GetGUID() == caster->GetGUID())
				return;

			Player* player = unit->ToPlayer();
			if (player && player->IsGameMaster())
				return;

			unit->CastSpell(unit, SPELL_ARCANE_BARRIER_DAMAGE);
		}
	}
};

// Fire Bomb - 270956
// AreaTriggerID - 13417
struct at_fire_bomb : AreaTriggerAI
{
	at_fire_bomb(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger)
	{
	}

	enum Spells
	{
		SPELL_FIRE_BOMB_DAMAGE          = 270958
	};

	void OnUnitEnter(Unit* /*unit*/) override
	{
		if (Unit* caster = at->GetCaster())
		{
			caster->CastSpell(at->GetPosition(), SPELL_FIRE_BOMB_DAMAGE, true);
			at->Remove();
		}
	}

	void OnRemove() override
	{
		if (Unit* caster = at->GetCaster())
			caster->CastSpell(at->GetPosition(), SPELL_FIRE_BOMB_DAMAGE, true);
	}
};

// Rain of fire - 156974
// AreaTriggerID - 1902
struct at_rain_of_fire : AreaTriggerAI
{
	at_rain_of_fire(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger)
	{
	}

	enum Spells
	{
		SPELL_RAIN_OF_FIRE_DAMAGE       = 221183
	};

	void OnUnitEnter(Unit* unit) override
	{
		if (Unit* caster = at->GetCaster())
		{
			if (unit->GetGUID() == caster->GetGUID())
				return;

			if (unit->IsFriendlyTo(caster))
				return;

			caster->CastSpell(unit, SPELL_RAIN_OF_FIRE_DAMAGE, true);
		}
	}

	void OnUnitExit(Unit* unit)
	{
		if (unit->HasAura(SPELL_RAIN_OF_FIRE_DAMAGE))
		{
			unit->RemoveAurasDueToSpell(SPELL_RAIN_OF_FIRE_DAMAGE);
		}
	}
};

// Teleport - 135176
class spell_purge_teleport : public SpellScript
{
	PrepareSpellScript(spell_purge_teleport);

	void HandleTeleport(SpellEffIndex effIndex)
	{
		PreventHitDefaultEffect(effIndex);

		Unit* target = GetHitUnit();
		if (target->GetTypeId() != TYPEID_UNIT)
			return;

		GetCaster()->AttackStop();

		if (Creature* creature = target->ToCreature())
			creature->DespawnOrUnsummon(860ms);
	}

	void Register() override
	{
		OnEffectHitTarget += SpellEffectFn(spell_purge_teleport::HandleTeleport, EFFECT_0, SPELL_EFFECT_TELEPORT_UNITS);
	}
};

// Glacial Spike - 338488
class spell_purge_glacial_spike : public SpellScript
{
	PrepareSpellScript(spell_purge_glacial_spike);

	enum Misc
	{
		SPELL_GLACIAL_WRATH             = 346525
	};

	void HandleDummy(SpellEffIndex /*effIndex*/)
	{
		Unit* caster = GetCaster();
		if (Unit* target = GetHitUnit())
		{
			caster->CastSpell(target, SPELL_GLACIAL_WRATH);
		}
	}

	void Register() override
	{
		OnEffectHitTarget += SpellEffectFn(spell_purge_glacial_spike::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
	}
};

// Glacial Spike - 346469
class spell_purge_glacial_spike_summon : public SpellScript
{
	PrepareSpellScript(spell_purge_glacial_spike_summon);

	void HandleSummon(SpellEffIndex effIndex)
	{
		PreventHitDefaultEffect(effIndex);
		Unit* caster = GetCaster();
		uint32 entry = uint32(GetEffectInfo().MiscValue);
		SummonPropertiesEntry const* properties = sSummonPropertiesStore.LookupEntry(uint32(GetEffectInfo().MiscValueB));
		uint32 duration = uint32(GetSpellInfo()->GetDuration());
		uint32 health = caster->CountPctFromMaxHealth(5);

		WorldLocation* pos = GetHitDest();
		if (Creature* summon = caster->GetMap()->SummonCreature(entry, pos->GetPosition(), properties, duration, caster, GetSpellInfo()->Id))
		{
			summon->SetMaxHealth(health);
			summon->SetFullHealth();
			summon->SetFaction(caster->GetFaction());

			UnitState cannotMove = UnitState(UNIT_STATE_ROOT | UNIT_STATE_CANNOT_TURN);
			summon->SetControlled(true, cannotMove);
		}

	}

	void Register() override
	{
		OnEffectHit += SpellEffectFn(spell_purge_glacial_spike_summon::HandleSummon, EFFECT_0, SPELL_EFFECT_SUMMON);
	}
};

void AddSC_npcs_dalaran_purge()
{
	RegisterDalaranAI(npc_jaina_dalaran_patrol);
	RegisterDalaranAI(npc_silver_covenant_assassin);
	RegisterDalaranAI(npc_archmage_landalock);
	RegisterDalaranAI(npc_mage_commander_zuros);
	RegisterDalaranAI(npc_arcanist_rathaella);
	RegisterDalaranAI(npc_sorin_magehand);

	// Sunreavers
	RegisterDalaranAI(npc_sunreaver_citizen);
	RegisterDalaranAI(npc_sunreaver_mage);
	RegisterDalaranAI(npc_sunreaver_aegis);
	RegisterDalaranAI(npc_sunreaver_captain);
	RegisterDalaranAI(npc_magister_brasael);
	RegisterDalaranAI(npc_magister_surdiel);

	RegisterCreatureAI(npc_arcane_barrier);
	RegisterCreatureAI(npc_glacial_spike);

	RegisterAreaTriggerAI(at_arcane_barrier);
	RegisterAreaTriggerAI(at_arcane_protection);
	RegisterAreaTriggerAI(at_fire_bomb);
	RegisterAreaTriggerAI(at_rain_of_fire);

	RegisterSpellScript(spell_purge_teleport);
	RegisterSpellScript(spell_purge_glacial_spike);
	RegisterSpellScript(spell_purge_glacial_spike_summon);
}
