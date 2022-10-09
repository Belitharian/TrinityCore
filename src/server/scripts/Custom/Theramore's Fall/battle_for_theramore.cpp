#include "GameObject.h"
#include "InstanceScript.h"
#include "KillRewarder.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "PassiveAI.h"
#include "Scenario.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "Custom/AI/CustomAI.h"
#include "battle_for_theramore.h"

struct npc_jaina_theramore : public CustomAI
{
	npc_jaina_theramore(Creature* creature) : CustomAI(creature, AI_Type::Melee)
	{
		Initialize();
	}

	enum Spells
	{
		SPELL_BLIZZARD              = 284968,
		SPELL_WONDROUS_RADIANCE     = 227410,
		SPELL_FIREBALL              = 20678,
		SPELL_FIREBLAST             = 20679,
		SPELL_FROST_BARRIER         = 69787,
		SPELL_FROSTBOLT_COSMETIC    = 237649,
		SPELL_LIGHTNING_FX          = 278455
	};

	void Initialize()
	{
		instance = me->GetInstanceScript();
	}

	InstanceScript* instance;

	void Reset() override
	{
		Initialize();
	}

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
		if (me->HealthBelowPctDamaged(10, damage)
			&& !me->HasAura(SPELL_FROST_BARRIER))
		{
			me->CastStop();
			DoCast(SPELL_FROST_BARRIER);
		}
	}

    void SpellHitTarget(WorldObject* target, SpellInfo const* spellInfo) override
	{
		if (target->GetEntry() == NPC_THERAMORE_FIRE_CREDIT
			&& spellInfo->Id == SPELL_FROSTBOLT_COSMETIC)
		{
			if (Creature* credit = target->ToCreature())
				credit->DespawnOrUnsummon();
		}
	}

	void KilledUnit(Unit* /*victim*/) override
	{
		if (roll_chance_i(15))
			Talk(SAY_JAINA_SLAY_01);
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		scheduler
			.Schedule(5ms, [this](TaskContext fireball)
			{
				if (roll_chance_i(20))
					Talk(SAY_JAINA_SPELL_01);
				DoCastVictim(SPELL_FIREBALL);
				fireball.Repeat(2s, 8s);
			})
			.Schedule(3s, [this](TaskContext fireblast)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_FIREBLAST);
				fireblast.Repeat(8s, 14s);
			})
			.Schedule(8s, [this](TaskContext blizzard)
			{
				if (roll_chance_i(10))
					Talk(SAY_JAINA_BLIZZARD_01);
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_BLIZZARD);
				blizzard.Repeat(14s, 22s);
			})
			.Schedule(10s, [this](TaskContext wondrous_radiance)
			{
				CastSpellExtraArgs args(true);
				args.AddSpellBP0(1E8);

				me->CastStop();

				for (auto threat : me->GetThreatManager().GetUnsortedThreatList())
					DoCast(threat->GetVictim(), SPELL_WONDROUS_RADIANCE, args);

				wondrous_radiance.Repeat(25s, 32s);
			});
	}

	void MovementInform(uint32 type, uint32 id) override
	{
		if (type == EFFECT_MOTION_TYPE || type == POINT_MOTION_TYPE)
		{
			switch (id)
			{
				case MOVEMENT_INFO_POINT_01:
					instance->TriggerGameEvent(EVENT_THE_COUNCIL);
					break;
				case MOVEMENT_INFO_POINT_02:
					me->StopMoving();
					me->GetMotionMaster()->Clear();
					me->GetMotionMaster()->MoveIdle();
					me->SetFacingTo(3.13f);
					if (Creature* hedric = instance->GetCreature(DATA_HEDRIC_EVENCANE))
					{
						hedric->StopMoving();
						hedric->SetSheath(SHEATH_STATE_UNARMED);
						hedric->SetEmoteState(EMOTE_STATE_WAGUARDSTAND01);
						hedric->GetMotionMaster()->Clear();
						hedric->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, HedricPoint03, true, HedricPoint03.GetOrientation());
						hedric->SetFacingTo(4.99f);
					}
					instance->TriggerGameEvent(EVENT_FIND_JAINA_04);
					break;
				case MOVEMENT_INFO_POINT_03:
					me->SetVisible(false);
					scheduler.Schedule(2s, [this](TaskContext /*context*/)
					{
						me->SetVisible(true);
						me->NearTeleportTo(JainaPoint05);
						if (GameObject* portal = me->SummonGameObject(GOB_PORTAL_TO_STORMWIND, PortalPoint03, QuaternionData::fromEulerAnglesZYX(PortalPoint03.GetOrientation(), 0.f, 0.f), 0s))
							portal->SetObjectScale(0.8f);
						if (TempSummon* summon = me->SummonCreature(WORLD_TRIGGER, PortalPoint03, TEMPSUMMON_MANUAL_DESPAWN))
						{
							summon->SetObjectScale(1.8f);
							summon->CastSpell(summon, SPELL_LIGHTNING_FX, true);
						}
						DoCastSelf(SPELL_PORTAL_CHANNELING_01);

						instance->SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::RetrieveRhonin);
					});
					break;
				default:
					break;
			}
		}
	}

	void MoveInLineOfSight(Unit* who) override
	{
		ScriptedAI::MoveInLineOfSight(who);

		BFTPhases phase = (BFTPhases)instance->GetData(DATA_SCENARIO_PHASE);
		if (phase == BFTPhases::HelpTheWounded)
		{
			if (who->IsWithinDist(me, 4.f) && who->GetEntry() == NPC_THERAMORE_FIRE_CREDIT)
			{
				CastSpellExtraArgs args(true);
				args.SetTriggerFlags(TRIGGERED_CAST_DIRECTLY);

				DoCast(who, SPELL_FROSTBOLT_COSMETIC, args);
			}
		}

		if (me->IsEngaged())
			return;

		if (who->GetTypeId() != TYPEID_PLAYER)
			return;

		if (Player* player = who->ToPlayer())
		{
			if (player->IsGameMaster())
				return;

			if (player->IsFriendlyTo(me) && player->IsWithinDist(me, 4.f))
			{
				switch (phase)
				{
					case BFTPhases::FindJaina:
						instance->TriggerGameEvent(EVENT_FIND_JAINA_01);
						break;
					case BFTPhases::ALittleHelp:
						instance->TriggerGameEvent(EVENT_FIND_JAINA_02);
						break;
					case BFTPhases::TheBattle:
						instance->TriggerGameEvent(EVENT_FIND_JAINA_03);
						break;
					case BFTPhases::WaitForAmara:
						instance->TriggerGameEvent(EVENT_FIND_JAINA_05);
						break;
					case BFTPhases::RetrieveRhonin:
						instance->TriggerGameEvent(EVENT_RETRIEVE_RHONIN);
						break;
					default:
						break;
				}
			}
		}
	}
};

struct npc_archmage_tervosh : public CustomAI
{
	npc_archmage_tervosh(Creature* creature) : CustomAI(creature)
	{
	}

	enum Spells
	{
		SPELL_FIREBALL              = 358226,
		SPELL_FLAMESTRIKE           = 330347,
		SPELL_BLAZING_BARRIER       = 295238,
		SPELL_SCORCH                = 358238,
		SPELL_CONFLAGRATION         = 226757
	};

	void MovementInform(uint32 type, uint32 id) override
	{
		if (type == EFFECT_MOTION_TYPE || type == POINT_MOTION_TYPE)
		{
			switch (id)
			{
				case MOVEMENT_INFO_POINT_01:
					me->SetFacingTo(0.70f);
					break;
				case MOVEMENT_INFO_POINT_02:
					me->SetFacingTo(2.14f);
					me->SetVisible(false);
					break;
				case MOVEMENT_INFO_POINT_03:
					me->SetFacingTo(4.05f);
					me->SetEmoteState(EMOTE_STATE_READ);
					break;
				default:
					break;
			}
		}
	}

    void SpellHitTarget(WorldObject* target, SpellInfo const* spellInfo) override
	{
		switch (spellInfo->Id)
		{
			case SPELL_FIREBALL:
			case SPELL_FLAMESTRIKE:
			case SPELL_SCORCH:
			{
				Unit* victim = target->ToUnit();
				if (victim && !victim->HasAura(SPELL_CONFLAGRATION) && roll_chance_i(40))
					DoCast(victim, SPELL_CONFLAGRATION, true);
			}
			break;
		}
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		DoCastSelf(SPELL_BLAZING_BARRIER);

		scheduler
			.Schedule(30s, [this](TaskContext blazing_barrier)
			{
				if (!me->HasAura(SPELL_BLAZING_BARRIER))
				{
					DoCast(SPELL_BLAZING_BARRIER);
					blazing_barrier.Repeat(30s);
				}
				else
				{
					blazing_barrier.Repeat(1s);
				}
			})
			.Schedule(8s, 10s, [this](TaskContext fireblast)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_SCORCH);
				fireblast.Repeat(14s, 22s);
			})
			.Schedule(12s, 18s, [this](TaskContext pyroblast)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
					DoCast(target, SPELL_FLAMESTRIKE);
				pyroblast.Repeat(22s, 35s);
			})
			.Schedule(5ms, [this](TaskContext fireball)
			{
				DoCastVictim(SPELL_FIREBALL);
				fireball.Repeat(2s);
			});
	}
};

struct npc_amara_leeson : public CustomAI
{
	npc_amara_leeson(Creature* creature) : CustomAI(creature)
	{
		instance = me->GetInstanceScript();
	}

	enum Spells
	{
		SPELL_FIREBALL              = 20678,
		SPELL_BLAZING_BARRIER       = 295238,
		SPELL_PRISMATIC_BARRIER     = 235450,
		SPELL_ICE_BARRIER           = 198094,
		SPELL_GREATER_PYROBLAST     = 295231,
		SPELL_SCORCH                = 301075
	};

	InstanceScript* instance;

	float GetDistance() override
	{
		return 15.f;
	}

	void JustEngagedWith(Unit* /*who*/) override
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
				scorch.Repeat(6s, 8s);
			})
			.Schedule(3s, [this](TaskContext greater_pyroblast)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				{
					me->CastStop();
					DoCast(target, SPELL_GREATER_PYROBLAST);
				}
				greater_pyroblast.Repeat(8s, 10s);
			});
	}

	void MovementInform(uint32 type, uint32 id) override
	{
		if (type == EFFECT_MOTION_TYPE || type == POINT_MOTION_TYPE)
		{
			switch (id)
			{
				case MOVEMENT_INFO_POINT_01:
				case MOVEMENT_INFO_POINT_03:
					me->SetVisible(false);
					break;
				case MOVEMENT_INFO_POINT_02:
					instance->TriggerGameEvent(EVENT_WAIT_ARCHMAGE_LESSON);
					break;
				default:
					break;
			}
		}
	}
};

struct npc_rhonin : public CustomAI
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

	npc_rhonin(Creature* creature) : CustomAI(creature), arcaneCharges(0)
	{
		instance = creature->GetInstanceScript();
	}

	InstanceScript* instance;
	uint8 arcaneCharges;

	float GetDistance() override
	{
		return 15.f;
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
				me->RemoveAurasDueToSpell(SPELL_CHAT_BUBBLE);
				me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
				instance->DoCastSpellOnPlayers(SPELL_RUNIC_SHIELD);
				DoCast(SPELL_ARCANE_CAST_INSTANT);
				KillRewarder(player, me, false).Reward(me->GetEntry());
				break;
		}

		CloseGossipMenuFor(player);
		return true;
	}

    void OnSpellCast(SpellInfo const* spell) override
    {
		switch (spell->Id)
		{
			case SPELL_ARCANE_BLAST:
				arcaneCharges++;
				if (roll_chance_i(40))
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

	void JustEngagedWith(Unit* /*who*/) override
	{
		DoCastSelf(SPELL_PRISMATIC_BARRIER, CastSpellExtraArgs(SPELLVALUE_BASE_POINT0, 256E3));

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

	void MovementInform(uint32 type, uint32 id) override
	{
		if (type == EFFECT_MOTION_TYPE || type == POINT_MOTION_TYPE)
		{
			switch (id)
			{
				case MOVEMENT_INFO_POINT_01:
					me->SetVisible(false);
					scheduler.Schedule(2s, [this](TaskContext /*context*/)
					{
						me->SetVisible(true);
						me->NearTeleportTo(RhoninPoint02);
						DoCastSelf(SPELL_PORTAL_CHANNELING_03);
					});
					break;
				default:
					break;
			}
		}
	}

	uint32 EnemiesInRange(float distance)
	{
		uint32 count = 0;
		for (ThreatReference const* ref : me->GetThreatManager().GetUnsortedThreatList())
			if (me->IsWithinDist(ref->GetVictim(), distance))
				++count;
		return count;
	}
};

struct npc_kinndy_sparkshine : public CustomAI
{
	npc_kinndy_sparkshine(Creature* creature) : CustomAI(creature), evocating(false)
	{
	}

	enum Spells
	{
		SPELL_ARCANE_BOLT           = 154235,
		SPELL_SUPERNOVA             = 157980,
        SPELL_EVOCATION             = 211765,
	};

    bool evocating;

	void MovementInform(uint32 type, uint32 id) override
	{
		if (type == EFFECT_MOTION_TYPE || type == POINT_MOTION_TYPE)
		{
			switch (id)
			{
				case MOVEMENT_INFO_POINT_01:
					me->SetFacingTo(4.62f);
					me->SetVisible(false);
					break;
				case MOVEMENT_INFO_POINT_02:
					me->SetFacingTo(1.24f);
					break;
				default:
					break;
			}
		}
	}

    void OnChannelFinished(SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_EVOCATION)
            evocating = false;
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
    {
        if (me->HealthBelowPctDamaged(10, damage))
        {
            // On supprime les dégâts actuels
            damage = 0;

            if (evocating)
                return;

            evocating = true;

            // On interrompt tous les sorts
            CastStop();

            // On lance Evocation
            DoCast(me, SPELL_EVOCATION,
                    CastSpellExtraArgs(TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD)
                    .AddSpellBP0(10)
                    .AddSpellMod(SPELLVALUE_BASE_POINT1, 20));
        }
    }

	void JustEngagedWith(Unit* /*who*/) override
	{
		scheduler
			.Schedule(8s, 10s, [this](TaskContext supernova)
			{
                if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
                {
                    CastStop();
                    DoCast(target, SPELL_SUPERNOVA);
                }
				supernova.Repeat(10s, 15s);
			})
			.Schedule(3s, [this](TaskContext arcane_bolt)
			{
				DoCastVictim(SPELL_ARCANE_BOLT);
				arcane_bolt.Repeat(2s);
			});
	}
};

struct npc_pained : public ScriptedAI
{
	npc_pained(Creature* creature) : ScriptedAI(creature)
	{
		instance = me->GetInstanceScript();
	}

	InstanceScript* instance;

	void MovementInform(uint32 type, uint32 id) override
	{
		if (type == EFFECT_MOTION_TYPE || type == POINT_MOTION_TYPE)
		{
			switch (id)
			{
				case MOVEMENT_INFO_POINT_02:
					me->SetVisible(false);
					instance->TriggerGameEvent(EVENT_THE_UNKNOWN_TAUREN);
					break;
				default:
					break;
			}
		}
	}
};

struct npc_kalecgos_theramore : public CustomAI
{
	npc_kalecgos_theramore(Creature* creature) : CustomAI(creature)
	{
		instance = me->GetInstanceScript();
	}

	enum Spells
	{
		SPELL_COMET_STORM           = 153595,
		SPELL_DISSOLVE              = 255295,
		SPELL_TELEPORT              = 357601,
		SPELL_CHILLED               = 333602,
		SPELL_FLURRY                = 320008,
		SPELL_ICE_NOVA              = 157997
	};

	InstanceScript* instance;

	void MovementInform(uint32 type, uint32 id) override
	{
		if (type == EFFECT_MOTION_TYPE || type == POINT_MOTION_TYPE)
		{
			switch (id)
			{
				case MOVEMENT_INFO_POINT_01:
					me->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
					me->SetImmuneToAll(true);
					scheduler.Schedule(2s, [this](TaskContext /*context*/)
					{
						DoCastSelf(SPELL_DISSOLVE);
						DoCastSelf(SPELL_TELEPORT, true);
					});
					break;
				case MOVEMENT_INFO_POINT_02:
					me->SetVisible(false);
					break;
				default:
					break;
			}
		}
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		scheduler
			.Schedule(8s, 10s, [this](TaskContext chilled)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_CHILLED);
				chilled.Repeat(14s, 22s);
			})
			.Schedule(12s, 18s, [this](TaskContext comet_barrage)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
					DoCast(target, SPELL_COMET_STORM);
				comet_barrage.Repeat(30s, 35s);
			})
			.Schedule(5ms, [this](TaskContext frostbolt)
			{
				DoCastVictim(SPELL_FLURRY);
				frostbolt.Repeat(2s);
			})
			.Schedule(5s, [this](TaskContext ice_nova)
			{
				for (auto* ref : me->GetThreatManager().GetUnsortedThreatList())
				{
					Unit* target = ref->GetVictim();
					if (target && target->isMoving())
					{
						me->CastStop();
						DoCast(target, SPELL_ICE_NOVA);
						ice_nova.Repeat(3s, 5s);
						return;
					}
				}
				ice_nova.Repeat(1s);
			});
	}
};

struct event_theramore_training : public NullCreatureAI
{
	event_theramore_training(Creature* creature) : NullCreatureAI(creature), launched(false)
	{
		instance = creature->GetInstanceScript();
	}

	TaskScheduler scheduler;
	InstanceScript* instance;
	bool launched;

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
				for (uint8 i = 0; i < 2; i++)
				{
					footmen[i]->SetTarget(ObjectGuid::Empty);
					footmen[i]->SetEmoteState(EMOTE_STATE_NONE);
					footmen[i]->SetFacingTo(2.60f);
					footmen[i]->SetRegenerateHealth(false);
					footmen[i]->SetHealth(footmen[0]->GetMaxHealth());
					footmen[i]->SetReactState(REACT_AGGRESSIVE);
				}

				faithful->SetTarget(ObjectGuid::Empty);
				faithful->SetFacingTo(2.60f);
				faithful->SetReactState(REACT_AGGRESSIVE);
				faithful->SetPower(POWER_MANA, faithful->GetMaxPower(POWER_MANA));

				arcanist->CastStop();
				arcanist->CombatStop();
				arcanist->SetTarget(ObjectGuid::Empty);
				arcanist->SetFacingTo(2.60f);
				arcanist->SetReactState(REACT_AGGRESSIVE);
				arcanist->SetPower(POWER_MANA, arcanist->GetMaxPower(POWER_MANA));

				training->SetVisible(false);

				scheduler.CancelAll();
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
						faithful->CastSpell(victim, RAND(SPELL_FLASH_HEAL, SPELL_HEAL, SPELL_POWER_WORD_SHIELD));
						context.Repeat(8s);
					})
					.Schedule(1s, [footmen](TaskContext context)
					{
						if (!footmen[0]->HasAura(SPELL_POWER_WORD_SHIELD))
							footmen[0]->DealDamage(footmen[1], footmen[0], urand(1500, 2000));

						if (!footmen[1]->HasAura(SPELL_POWER_WORD_SHIELD))
							footmen[1]->DealDamage(footmen[0], footmen[1], urand(1500, 2000));

						context.Repeat(8s);
					})
					.Schedule(1s, [arcanist, training](TaskContext context)
					{
						if (arcanist->GetPowerPct(POWER_MANA) <= 20)
						{
							const SpellInfo* info = sSpellMgr->AssertSpellInfo(SPELL_EVOCATION, DIFFICULTY_NONE);
                            Milliseconds ms = Milliseconds(info->CalcDuration());

							arcanist->CastSpell(arcanist, SPELL_EVOCATION);
							arcanist->GetSpellHistory()->ResetCooldown(info->Id, true);
							arcanist->GetSpellHistory()->RestoreCharge(info->ChargeCategoryId);

							context.Repeat(ms + 500ms);
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
		if (!launched)
		{
			DoAction(1U);
			launched = true;
		}

		scheduler.Update(diff);
	}
};

struct event_theramore_faithful : public ScriptedAI
{
	event_theramore_faithful(Creature* creature) : ScriptedAI(creature), launched(false)
	{
		instance = creature->GetInstanceScript();
	}

	TaskScheduler scheduler;
	InstanceScript* instance;
	bool launched;

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
			faithful->RemoveAllAuras();
			faithful->SetFacingTo(0.66f);

			scheduler.CancelAll();
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
		if (!launched)
		{
			DoAction(1U);
			launched = true;
		}

		scheduler.Update(diff);
	}
};

void AddSC_battle_for_theramore()
{
    RegisterTheramoreAI(npc_jaina_theramore);
	RegisterTheramoreAI(npc_archmage_tervosh);
	RegisterTheramoreAI(npc_amara_leeson);
	RegisterTheramoreAI(npc_rhonin);
	RegisterTheramoreAI(npc_kinndy_sparkshine);
	RegisterTheramoreAI(npc_pained);
	RegisterTheramoreAI(npc_kalecgos_theramore);
	RegisterTheramoreAI(event_theramore_training);
	RegisterTheramoreAI(event_theramore_faithful);
}
