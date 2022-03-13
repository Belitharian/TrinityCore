#include "Custom/AI/CustomAI.h"
#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "DB2Stores.h"
#include "InstanceScript.h"
#include "MotionMaster.h"
#include "Object.h"
#include "PassiveAI.h"
#include "ScriptMgr.h"
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
		SPELL_BLIZZARD              = 284968,
		SPELL_FROSTBOLT             = 284703,
		SPELL_FRIGID_SHARD          = 354933,
		SPELL_TELEPORT              = 135176,
		SPELL_GLACIAL_SPIKE         = 338488
	};

	void Initialize() override
	{
		CustomAI::Initialize();

		instance = me->GetInstanceScript();
	}

	InstanceScript* instance;

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
					me->CastStop(SPELL_FRIGID_SHARD);
					DoCast(target, SPELL_GLACIAL_SPIKE);
				}
				glacial_spike.Repeat(15s, 32s);
			})
			.Schedule(4s, [this](TaskContext blizzard)
			{
				if (me->GetThreatManager().GetThreatListSize() >= 2)
				{
					me->CastStop(SPELL_BLIZZARD);
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

struct npc_archmage_landalock : public CustomAI
{
	enum Talks
	{
		SAY_LANDALOCK_01,
		SAY_LANDALOCK_02,
		SAY_LANDALOCK_03,
	};

	enum Misc
	{
		// Spells
		SPELL_ICE_BURST             = 69108,
		SPELL_RUNES_OF_SHIELDING    = 217859,
		// Gossips
		GOSSIP_MENU_DEFAULT         = 65003,
		// GameObjects
		GOB_ICEWALL                 = 368620,
		// NPCs
		NPC_ICEWALL                 = 178819
	};

	npc_archmage_landalock(Creature* creature) : CustomAI(creature)
	{
		instance = creature->GetInstanceScript();

		if (Creature* icewall = me->FindNearestCreature(NPC_ICEWALL, 15.f))
			summon = icewall->GetPosition();
	}

	const Position sorinPos = { 5801.87f, 645.19f, 647.55f, 5.16f };

	InstanceScript* instance;
	ObjectGuid icewallGUID;
	Position summon;

	bool OnGossipHello(Player* player) override
	{
		player->PrepareGossipMenu(me, GOSSIP_MENU_DEFAULT, true);
		player->SendPreparedGossip(me);
		return true;
	}

	void MovementInform(uint32 /*type*/, uint32 id) override
	{
		switch (id)
		{
			case MOVEMENT_INFO_POINT_01:
				me->CastSpell(me, SPELL_RUNES_OF_SHIELDING, true);
				me->AddAura(SPELL_CASTER_READY_01, me);
				if (Creature* sorin = instance->GetCreature(DATA_SORIN_MAGEHAND))
					me->SetFacingToObject(sorin);
				break;
		}
	}

	bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
	{
		ClearGossipMenuFor(player);

		switch (gossipListId)
		{
			case 0:
				me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
				me->RemoveAurasDueToSpell(SPELL_FROST_CANALISATION);
				scheduler.Schedule(2s, [player, this](TaskContext context)
				{
					switch (context.GetRepeatCounter())
					{
						case 0:
							me->AI()->Talk(SAY_LANDALOCK_01);
							me->SetFacingToObject(player);
							context.Repeat(3s);
							break;
						case 1:
							me->AI()->Talk(SAY_LANDALOCK_02);
							context.Repeat(4s);
							break;
						case 2:
							me->SetFacingTo(5.55f);
							context.Repeat(2s);
							break;
						case 3:
							if (Creature* icewall = me->SummonCreature(WORLD_TRIGGER, summon, TEMPSUMMON_TIMED_DESPAWN, 10s))
							{
								me->HandleEmoteCommand(EMOTE_ONESHOT_POINT);
								icewall->CastSpell(icewall, SPELL_ICE_BURST);
							}
							context.Repeat(500ms);
							break;
						case 4:
							if (Creature* icewall = me->FindNearestCreature(NPC_ICEWALL, 15.f))
								icewall->SetVisible(false);
							if (GameObject* collider = me->FindNearestGameObject(GOB_ICEWALL, 15.f))
								collider->Delete();
							context.Repeat(2s);
							break;
						case 5:
							me->AI()->Talk(SAY_LANDALOCK_03);
							me->SetFacingToObject(player);
							me->SetWalk(true);
							context.Repeat(4s);
							break;
						case 6:
							me->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_01, sorinPos, true, sorinPos.GetOrientation());
							break;
						default:
							break;
					}
				});
				break;
		}

		CloseGossipMenuFor(player);
		return true;
	}
};

struct npc_mage_commander_zuros : public CustomAI
{
	static constexpr float DAMAGE_REDUCTION = 0.05f;

	enum Spells
	{
		SPELL_ACCELERATING_BLAST    = 203176,
		SPELL_NETHER_WOUND          = 211000,
		SPELL_TIME_STOP             = 279062
	};

	npc_mage_commander_zuros(Creature* creature) : CustomAI(creature, AI_Type::Melee)
	{
		Initialize();
	}

	void DamageTaken(Unit* /*attacker*/, uint32& damage) override
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

        if (Creature* jaina = instance->GetCreature(DATA_JAINA_PROUDMOORE_PATROL))
        {
            if (!roll_chance_i(10) && !jaina->IsInPhase(me))
                return;

            me->GetMotionMaster()->MoveFleeing(jaina);
        }
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
					me->CastStop(SPELL_SCORCH);
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
			.Schedule(8s, 10s, [this](TaskContext flamestrike)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_FLAMESTRIKE);
				flamestrike.Repeat(14s, 22s);
			})
			.Schedule(15s, 18s, [this](TaskContext rinf_of_fire)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_RINF_OF_FIRE);
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
				me->CastStop();
				DoCastSelf(SPELL_BLESSING_OF_FREEDOM);
			});
		}
	}

	void DamageTaken(Unit* /*attacker*/, uint32& damage) override
	{
		if (!healthLow && me->HealthBelowPctDamaged(25, damage))
		{
			healthLow = true;

			DoCastSelf(SPELL_DIVINE_SHIELD);

			scheduler
				.Schedule(1s, [this](TaskContext /*context*/)
				{
					me->CastStop();

					CastSpellExtraArgs args;
					args.AddSpellBP0(me->GetMaxHealth());

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
					me->CastStop(SPELL_HEAL);
					DoCast(SPELL_DIVINE_STORM);
					divine_storm.Repeat(12s, 25s);
				}
				else
					divine_storm.Repeat(1s);
			});
	}
};

struct npc_magister_brasael : public CustomAI
{
	static constexpr uint8 BAGS_COUNT = 150;

	npc_magister_brasael(Creature* creature) : CustomAI(creature)
	{
		Initialize();
	}

	enum Spells
	{
		SPELL_SURVIVOR_BAG          = 138208,
		SPELL_HOLD_BAG              = 288787
	};

	void Reset() override
	{
		me->SetImmuneToAll(true);
		me->SetSheath(SHEATH_STATE_UNARMED);
		me->AddAura(SPELL_HOLD_BAG, me);
		for (uint8 i = 0; i < BAGS_COUNT; i++)
			me->AddAura(SPELL_SURVIVOR_BAG, me);
	}
};

struct npc_magister_surdiel : public CustomAI
{
	static constexpr float DAMAGE_REDUCTION = 0.05f;

	enum Spells
	{
		SPELL_FIREBALL              = 79854,
		SPELL_RAIN_OF_FIRE          = 156974,
		SPELL_PYROBLAST             = 246505,
		SPELL_FIRE_BOMB             = 270956
	};

	InstanceScript* instance;

	npc_magister_surdiel(Creature* creature) : CustomAI(creature)
	{
		Initialize();
	}

	void Initialize() override
	{
		CustomAI::Initialize();

		instance = me->GetInstanceScript();
	}

	void DamageTaken(Unit* /*attacker*/, uint32& damage) override
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
					Position dest = target->GetPosition();
					me->CastStop();
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

	}

	void UpdateAI(uint32 diff) override
	{
		scheduler.Update(diff);
	}

	private:
	TaskScheduler scheduler;
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
		me->SetUnitFlags(UNIT_FLAG_NOT_SELECTABLE);
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

	void OnUnitEnter(Unit* unit) override
	{
		if (Unit* caster = at->GetCaster())
		{
			if (unit->GetGUID() == caster->GetGUID())
				return;

			if (unit->IsFriendlyTo(caster))
				return;

			unit->CastSpell(unit, SPELL_FIRE_BOMB_DAMAGE);

			at->Remove();
		}
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
	RegisterDalaranAI(npc_archmage_landalock);
	RegisterDalaranAI(npc_mage_commander_zuros);
	RegisterDalaranAI(npc_sorin_magehand);

	// Sunreavers
	RegisterDalaranAI(npc_sunreaver_citizen);
	RegisterDalaranAI(npc_sunreaver_mage);
	RegisterDalaranAI(npc_sunreaver_aegis);
	RegisterDalaranAI(npc_magister_brasael);
	RegisterDalaranAI(npc_magister_surdiel);

	RegisterCreatureAI(npc_glacial_spike);

	RegisterAreaTriggerAI(at_arcane_barrier);
	RegisterAreaTriggerAI(at_arcane_protection);
	RegisterAreaTriggerAI(at_fire_bomb);
	RegisterAreaTriggerAI(at_rain_of_fire);

	RegisterSpellScript(spell_purge_teleport);
	RegisterSpellScript(spell_purge_glacial_spike);
	RegisterSpellScript(spell_purge_glacial_spike_summon);
}
