#include "GameObject.h"
#include "Containers.h"
#include "InstanceScript.h"
#include "KillRewarder.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "PassiveAI.h"
#include "Scenario.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "battle_for_theramore.h"

struct npc_jaina_theramore : public CustomAI
{
	npc_jaina_theramore(Creature* creature) : CustomAI(creature, AI_Type::Melee), instance(nullptr)
	{
		Initialize();
	}

    enum NPCs
    {
        NPC_ARCANE_RIFT_TRIGGER     = 196664
    };

	enum Spells
	{
		SPELL_BLIZZARD              = 284968,
		SPELL_FIREBALL              = 20678,
		SPELL_FIREBLAST             = 20679,
		SPELL_FROSTBOLT_COSMETIC    = 237649,
		SPELL_LIGHTNING_FX          = 278455,
        SPELL_ARCANE_RIFT           = 388902,
        SPELL_FROZEN_SHIELD         = 396780
	};

	void Initialize() override
	{
        CustomAI::Initialize();

		instance = me->GetInstanceScript();
	}

	InstanceScript* instance;

	void Reset() override
	{
		Initialize();
	}

    void DoAction(int32 actionId) override
    {
        if (actionId == SPELL_TELEPORT_CAST_TIME)
        {
            uint32 waveId = instance->GetData(DATA_WAVE_GROUP_ID);

            Position position;
            switch (waveId)
            {
                // Portes
                case DATA_WAVE_DOORS:
                    position = JainaPoint03;
                    me->AI()->Talk(SAY_BATTLE_GATE);
                    break;
                    // Citadelle
                case DATA_WAVE_CITADEL:
                    position = CitadelPoint01;
                    me->AI()->Talk(SAY_BATTLE_CITADEL);
                    break;
                    // Docks
                case DATA_WAVE_DOCKS:
                    position = DocksPoint01;
                    me->AI()->Talk(SAY_BATTLE_DOCKS);
                    break;
            }

            DoCastSelf(SPELL_TELEPORT_CAST_TIME);
            scheduler.Schedule(1800ms, [this, position](TaskContext /*teleport*/)
            {
                me->SetHomePosition(position);
                me->NearTeleportTo(position);
            });
        }
    }

    void SummonedCreatureDespawn(Creature* summon) override
    {
        if (summon->GetEntry() == NPC_ARCANE_RIFT_TRIGGER)
        {
            summon->RemoveAllAuras();
            summon->RemoveAllAreaTriggers();
        }
    }

    void DamageTaken(Unit* /*attacker*/, uint32& /*damage*/, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
		if (!ShouldTakeDamage() && !me->HasAura(SPELL_FROZEN_SHIELD))
		{
            CastSpellExtraArgs args(true);
            args.AddSpellBP0(500);

			CastStop();
			DoCastSelf(SPELL_FROZEN_SHIELD, args);
		}
	}

    void EnterEvadeMode(EvadeReason why)
    {
        CustomAI::EnterEvadeMode(why);

        me->RemoveAllAreaTriggers();
        me->UnsummonAllTotems();
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
			.Schedule(1s, [this](TaskContext fireball)
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
			.Schedule(10s, [this](TaskContext arcane_rift)
			{
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                {
                    if (Creature* summon = me->SummonCreature(NPC_ARCANE_RIFT_TRIGGER, target->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, 5s))
                    {
                        summon->SetFaction(me->GetFaction());
                        summon->SetOwnerGUID(me->GetGUID());
                        summon->CastSpell(summon, SPELL_ARCANE_RIFT);
                    }
                }

                arcane_rift.Repeat(25s, 32s);
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
		SPELL_GREATER_PYROBLAST     = 255998,
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
					CastStop();
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

        NPC_ARCANIC_CRYSTAL         = 86602,
        SPELL_ARCANE_AFFINITY       = 173213,
	};

    enum Groups
    {
        GROUP_NORMAL,
        GROUP_ARCANE_EXPLOSION
    };

	enum Spells
	{
		SPELL_PRISMATIC_BARRIER     = 235450,
		SPELL_ARCANE_PROJECTILES    = 5143,
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
				KillRewarder::Reward(player, me);
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
					CastStop();
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
			.Schedule(1s, GROUP_NORMAL, [this](TaskContext arcane_blast)
			{
				if (arcaneCharges < 4)
					DoCastVictim(SPELL_ARCANE_BLAST);
				else
					DoCastVictim(SPELL_ARCANE_BARRAGE);
				arcane_blast.Repeat(2800ms);
			})
			.Schedule(3s, GROUP_NORMAL, [this](TaskContext evocation)
			{
				if (me->GetPowerPct(POWER_MANA) < 10.0f)
					DoCast(SPELL_EVOCATION);
                evocation.Repeat(3s);
			})
            .Schedule(5s, GROUP_NORMAL, [this](TaskContext arcane_cristal)
            {
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                {
                    const Position pos = target->GetRandomNearPosition(4.f);
                    if (Creature* crystal = me->SummonCreature(NPC_ARCANIC_CRYSTAL, pos, TEMPSUMMON_TIMED_DESPAWN, 31s))
                    {
                        crystal->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
                        crystal->SetFaction(me->GetFaction());
                        crystal->SetCanMelee(false);
                        crystal->SetControlled(true, UNIT_STATE_ROOT);
                        crystal->CastSpell(crystal, SPELL_ARCANE_AFFINITY);
                    }
                }
                arcane_cristal.Repeat(1min);
            })
			.Schedule(5s, GROUP_ARCANE_EXPLOSION, [this](TaskContext arcane_explosion)
			{
				if (EnemiesInRange(9.f) >= 3)
				{
                    scheduler.DelayGroup(GROUP_NORMAL, 5s);

					CastStop();
					DoCast(SPELL_ARCANE_EXPLOSION);
					arcane_explosion.Repeat(2s);
				}
				else
					arcane_explosion.Repeat(5ms);
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
};

struct npc_kinndy_sparkshine : public CustomAI
{
	npc_kinndy_sparkshine(Creature* creature) : CustomAI(creature, AI_Type::Distance), evocating(false)
	{
	}

	enum Spells
	{
		SPELL_ARCANE_BOLT           = 371306,
		SPELL_SUPERNOVA             = 157980,
        SPELL_EVOCATION             = 211765,
        SPELL_UNCONTROLLED_ENERGY   = 388951,
        SPELL_RUNE_OF_ALACRITY      = 388335
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

    void EnterEvadeMode(EvadeReason why)
    {
        CustomAI::EnterEvadeMode(why);

        me->RemoveAllAreaTriggers();
    }

    void OnChannelFinished(SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_EVOCATION)
            evocating = false;
    }

    void DamageTaken(Unit* /*attacker*/, uint32& /*damage*/, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
    {
        if (!ShouldTakeDamage())
        {
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
        DoCast(SPELL_RUNE_OF_ALACRITY);

		scheduler
            .Schedule(45s, [this](TaskContext rune_of_alacrity)
            {
                DoCast(SPELL_RUNE_OF_ALACRITY);
                rune_of_alacrity.Repeat(45s, 1min);
            })
			.Schedule(8s, 10s, [this](TaskContext supernova)
			{
                if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
                {
                    CastStop();
                    DoCast(target, SPELL_SUPERNOVA);
                }
				supernova.Repeat(10s, 15s);
            })
            .Schedule(5s, 15s, [this](TaskContext uncontrolled_energy)
            {
                float slice = 2 * float(M_PI) / 8;

                uint8 index = 0;
                for (uint8 i = 0; i < 8; ++i)
				{
					float angle = slice * index;
					const Position dest = GetRandomPositionAroundCircle(me, angle, 15.0f);
                    me->CastSpell(dest, SPELL_UNCONTROLLED_ENERGY);
					index++;
				}

                uncontrolled_energy.Repeat(1min);
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
		SPELL_TELEPORT              = 400542,
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
					scheduler.Schedule(1s, [this](TaskContext context)
					{
                        switch (context.GetRepeatCounter())
                        {
                            case 0:
                                me->CastSpell(me, SPELL_TELEPORT);
                                context.Repeat(4800ms);
                                break;
                            case 1:
                                DoCastSelf(SPELL_DISSOLVE, true);
                                me->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
                                me->SetImmuneToAll(true);
                                break;
                        }
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
                {
                    CastStop();
                    DoCast(target, SPELL_CHILLED);
                }
				chilled.Repeat(14s, 22s);
			})
			.Schedule(12s, 18s, [this](TaskContext comet_barrage)
			{
                if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
                {
                    CastStop();
                    DoCast(target, SPELL_COMET_STORM);
                }
				comet_barrage.Repeat(18s, 25s);
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
						CastStop();
						DoCast(target, SPELL_ICE_NOVA);
						ice_nova.Repeat(3s, 5s);
						return;
					}
				}
				ice_nova.Repeat(1s);
			});
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
}
