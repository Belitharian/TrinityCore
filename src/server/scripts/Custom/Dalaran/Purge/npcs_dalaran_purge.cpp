#include "AreaBoundary.h"
#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "Containers.h"
#include "InstanceScript.h"
#include "KillRewarder.h"
#include "MotionMaster.h"
#include "Object.h"
#include "GameObjectAI.h"
#include "PassiveAI.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"
#include "SpellAuraEffects.h"
#include "Custom/AI/CustomAI.h"
#include "dalaran_purge.h"

CircleBoundary const surdielCenter(Position(-1008.04f, 4515.24f), 19.0f);
CircleBoundary const brasaelCenter(Position( -682.22f, 4441.74f), 19.0f);
CircleBoundary const savorCenter(Position( -853.70f, 4457.10f), 19.0f);

CreatureBoundary const surdielBoundaries    = { &surdielCenter };
CreatureBoundary const brasaelBoundaries    = { &brasaelCenter };
CreatureBoundary const savorBoundaries      = { &savorCenter };

enum Atonement
{
    SPELL_ATONEMENT                 = 292010,
    SPELL_ATONEMENT_EFFECT          = 292011,
    SPELL_PRIEST_ATONEMENT_HEAL     = 81751,
    SPELL_POWER_WORD_RADIANCE       = 218589,
    SPELL_RENEW                     = 294342,
    SPELL_FLASH_HEAL                = 314655,
    SPELL_POWER_WORD_SHIELD         = 284471,
};

///
///     ALLIANCE NPC
///

struct npc_guardian_mage_dalaran : public CustomAI
{
	npc_guardian_mage_dalaran(Creature* creature) : CustomAI(creature)
	{
	}

	enum Spells
	{
		SPELL_COUNTERSPELL          = 173077,
		SPELL_ICE_BARRIER           = 198094,
		SPELL_FROSTBITE             = 198121,
		SPELL_FROSTBOLT             = 284703,
		SPELL_FROST_NOVA            = 284879,
		SPELL_BLINK                 = 284877,
		SPELL_BLIZZARD              = 396400,
		SPELL_FREEZE_OVER           = 378886,
	};

	float GetDistance() override
	{
		return 10.f;
	}

	void OnSpellCast(SpellInfo const* spellInfo) override
	{
		if (spellInfo->Id == SPELL_FROST_NOVA)
		{
			scheduler.Schedule(1s, [this](TaskContext blink)
			{
				CastStop();
				DoCast(SPELL_BLINK);
			});
		}
	}

	void JustEngagedWith(Unit* who) override
	{
        if (roll_chance_i(30))
		    DoCast(who, SPELL_FREEZE_OVER);

		scheduler
			.Schedule(5s, [this](TaskContext ice_barrier)
			{
				if (roll_chance_i(60))
				{
					if (!me->HasAura(SPELL_ICE_BARRIER))
					{
						CastStop();
						DoCastSelf(SPELL_ICE_BARRIER, true);
						ice_barrier.Repeat(1min);
					}
					else
						ice_barrier.Repeat(32s);
				}
				else
					ice_barrier.Repeat(5s);
			})
			.Schedule(2s, [this](TaskContext fireball)
			{
				DoCastVictim(SPELL_FROSTBOLT);
				fireball.Repeat(2s);
			})
			.Schedule(2s, [this](TaskContext freeze_over)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_FREEZE_OVER);
				freeze_over.Repeat(8s, 15s);
			})
			.Schedule(10s, [this](TaskContext counterspell)
			{
				if (Unit* target = DoSelectCastingUnit(SPELL_COUNTERSPELL, 30.0f))
				{
					CastStop();
					DoCast(target, SPELL_COUNTERSPELL);
					counterspell.Repeat(15s, 30s);
				}
				else
					counterspell.Repeat(1s);
			})
			.Schedule(8s, [this](TaskContext frost_nova)
			{
				if (EnemiesInRange(12.0f) >= 2)
				{
					CastStop();
					DoCast(SPELL_FROST_NOVA);
					frost_nova.Repeat(8s, 10s);
				}
				else
					frost_nova.Repeat(1s);
			})
			.Schedule(15s, 32s, [this](TaskContext blizzard)
			{
				CastStop(SPELL_BLIZZARD);
				DoCastVictim(SPELL_BLIZZARD);
				blizzard.Repeat(20s, 35s);
			});
	}

	void SpellHitTarget(WorldObject* target, SpellInfo const* spellInfo) override
	{
		if (spellInfo->GetSchoolMask() == SPELL_SCHOOL_MASK_FROST
			&& roll_chance_i(30))
		{
			Unit* victim = target->ToUnit();
			if (victim && victim->GetGUID() != me->GetGUID())
				DoCast(victim, SPELL_FROSTBITE);
		}
	}
};

struct npc_assassin_dalaran : public CustomAI
{
	npc_assassin_dalaran(Creature* creature) : CustomAI(creature, AI_Type::Melee)
	{
		Initialize();
	}

	enum Spells
	{
		SPELL_STEALTH               = 32615,
		SPELL_SPRINT                = 66060,
		SPELL_SINISTER_STRIKE       = 172028,
		SPELL_FAN_OF_KNIVES         = 273606,
	};

	void Reset() override
	{
		CustomAI::Reset();

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

struct npc_jaina_dalaran_patrol : public CustomAI
{
   static constexpr float DISTANCE_SHATTER = 5.0f;

	npc_jaina_dalaran_patrol(Creature* creature) : CustomAI(creature),
		playerGUID(ObjectGuid::Empty), originalSpeedRate(creature->GetSpeedRate(MOVE_WALK))
	{
		Initialize();
	}

	enum Spells
	{
		SPELL_COSMETIC_SNOW         = 83065,
		SPELL_BLIZZARD              = 284968,
		SPELL_FROSTBOLT             = 284703,
		SPELL_FRIGID_SHARD          = 354933,
		SPELL_TELEPORT              = 135176,
		SPELL_GLACIAL_SPIKE         = 338488,
		SPELL_FROZEN_SHIELD         = 396780,
		SPELL_SHATTER               = 263627,
		SPELL_ICY_GLARE             = 263626
	};

    enum Groups
    {
        NORMAL_COMBAT,
        PLAYER_ONESHOT,
    };

	void Initialize() override
	{
		CustomAI::Initialize();

		instance = me->GetInstanceScript();
        textOnCooldown = false;

		me->AddAura(SPELL_COSMETIC_SNOW, me);
	}

	InstanceScript* instance;
	ObjectGuid playerGUID;
	float originalSpeedRate;

	void Reset()
	{
		CustomAI::Reset();

		me->SetSpeedRate(MOVE_WALK, originalSpeedRate);
		me->ResumeMovement();
	}

	void AttackStart(Unit* who)
	{
		if (!who)
			return;

		if (me->Attack(who, false))
		{
			me->PauseMovement();
			me->SetCanMelee(false);
		}
	}

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
		if (!me->HasAura(SPELL_FROZEN_SHIELD)
			&& me->HealthBelowPctDamaged(30, damage))
		{
			CastStop(SPELL_FROZEN_SHIELD);
			DoCast(SPELL_FROZEN_SHIELD);
		}
	}

	void JustEngagedWith(Unit* who) override
	{
		DoCast(who, SPELL_GLACIAL_SPIKE);

		scheduler
            .Schedule(30s, PLAYER_ONESHOT, [this](TaskContext oneshot)
            {
                if (Player* player = me->GetVictim()->ToPlayer())
                {
                    playerGUID = player->GetGUID();

                    scheduler.DelayGroup(NORMAL_COMBAT, 30s);

                    DoCast(player, SPELL_ICY_GLARE, true);

                    float distanceToTravel = me->GetExactDist2d(player) - DISTANCE_SHATTER;
                    if (distanceToTravel > 0.0f)
                    {
                        me->SetWalk(true);
                        me->SetSpeedRate(MOVE_WALK, 1.8f);
                        me->GetMotionMaster()->Remove(CHASE_MOTION_TYPE);
                        me->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_01, player, DISTANCE_SHATTER);
                    }
                    else
                    {
                        me->SetFacingToObject(player);
                        DoCast(player, SPELL_SHATTER);
                    }
                }
                else
                    oneshot.Repeat(5s);
            })
			.Schedule(2s, NORMAL_COMBAT, [this](TaskContext frostbolt)
			{
				DoCastVictim(SPELL_FROSTBOLT);
				frostbolt.Repeat(2s);
			})
			.Schedule(10s, NORMAL_COMBAT, [this](TaskContext glacial_spike)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
				{
					CastStop(SPELL_FRIGID_SHARD);
					DoCast(target, SPELL_GLACIAL_SPIKE);
				}
				glacial_spike.Repeat(15s, 32s);
			})
			.Schedule(4s, NORMAL_COMBAT, [this](TaskContext blizzard)
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
			.Schedule(8s, NORMAL_COMBAT, [this](TaskContext frigid_shard)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_FRIGID_SHARD);
				frigid_shard.Repeat(10s, 12s);
			});
	}

	void MovementInform(uint32 /*type*/, uint32 id) override
	{
		switch (id)
		{
			case MOVEMENT_INFO_POINT_01:
				if (Player* player = ObjectAccessor::GetPlayer(*me, playerGUID))
				{
					me->SetFacingToObject(player);
					DoCast(player, SPELL_SHATTER);
				}
				break;
		}
	}

	void KilledUnit(Unit* victim) override
	{
		if (victim->GetEntry() == NPC_SUNREAVER_CITIZEN && !victim->HasAura(SPELL_TELEPORT))
		{
            TalkInCombat(SAY_JAINA_PURGE_SLAIN);
		}

		if (victim->GetEntry() == NPC_SUNREAVER_CITIZEN)
		{
			if (Map* map = me->GetMap())
			{
                Map::PlayerList const& players = map->GetPlayers();
				if (!players.isEmpty())
				{
                    if (Player* first = players.begin()->GetSource())
                    {
                        KillRewarder::Reward(first, victim);
                    }
				}
			}
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
                && who->IsHostileTo(me)
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
                    TalkInCombat(SAY_JAINA_PURGE_TELEPORT);
					DoCast(who, SPELL_TELEPORT);
				}
			}
			else
			{
				ScriptedAI::MoveInLineOfSight(who);
			}
		}
	}

	bool CanAIAttack(Unit const* who) const override
	{
		return who->IsAlive() && me->IsValidAttackTarget(who)
			&& ScriptedAI::CanAIAttack(who)
			&& who->GetEntry() != NPC_GRAND_MAGISTER_ROMMATH;
	}
};

struct npc_vereesa_windrunner_dalaran : public CustomAI
{
	npc_vereesa_windrunner_dalaran(Creature* creature) : CustomAI(creature, AI_Type::Hybrid)
	{
		Initialize();
	}

	enum Spells
	{
		SPELL_ARCANE_SHOOT          = 255644,
		SPELL_MULTI_SHOOT           = 384868,
		SPELL_SHOOT                 = 445949,
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
			.Schedule(8s, [this](TaskContext arcane_shoot)
			{
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                {
                    CastStop({ SPELL_ARCANE_SHOOT, SPELL_MULTI_SHOOT });
                    DoCast(target, SPELL_ARCANE_SHOOT);
                }
				arcane_shoot.Repeat(1s, 2s);
			})
			.Schedule(15s, [this](TaskContext multi_shoot)
			{
                CastStop();
                DoCastVictim(SPELL_MULTI_SHOOT);
				multi_shoot.Repeat(4s, 8s);
			});
	}

    bool CanAIAttack(Unit const* who) const override
    {
        return who->IsAlive() && me->IsValidAttackTarget(who)
            && ScriptedAI::CanAIAttack(who)
            && who->GetEntry() != NPC_GRAND_MAGISTER_ROMMATH;
    }
};

struct npc_stormwind_cleric : public CustomAI
{
	npc_stormwind_cleric(Creature* creature) : CustomAI(creature),
        ultimatePenitence(false)
	{
	}

	enum Spells
	{
        SPELL_POWER_WORD_BARRIER        = 62618,
        SPELL_PURGE_THE_WICKED          = 204197,
        SPELL_PURGE_THE_WICKED_DEBUFF   = 204213,
        SPELL_POWER_WORD_AEGIS          = 222403,
		SPELL_SMITE                     = 291463,
        SPELL_ULTIMATE_PENITENCE        = 421434,
        SPELL_FLASH_HEAL                = 314655,
    };

    bool ultimatePenitence;

    void Reset() override
    {
        CustomAI::Reset();

        ultimatePenitence = false;
    }

    void DamageTaken(Unit* /*attacker*/, uint32& /*damage*/, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
    {
        if (!ultimatePenitence && HealthBelowPct(30))
        {
            ultimatePenitence = true;

            Unit* target = DoSelectLowestHpFriendly(40.f);
            if (target && roll_chance_i(30))
                me->CastSpell(target->GetPosition(), SPELL_POWER_WORD_BARRIER, true);

            CastStop();
            DoCastSelf(SPELL_POWER_WORD_AEGIS, true);
            DoCastSelf(SPELL_ULTIMATE_PENITENCE);
        }
    }

	void JustEngagedWith(Unit* /*who*/) override
	{
        scheduler
            .Schedule(1s, [this](TaskContext smite)
            {
                DoCastVictim(SPELL_SMITE);
                smite.Repeat(2s);
            })
            .Schedule(3s, [this](TaskContext purge_the_wicked)
            {
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 30.0f, false, true, -SPELL_PURGE_THE_WICKED_DEBUFF))
                {
                    CastStop({ SPELL_ULTIMATE_PENITENCE, SPELL_POWER_WORD_RADIANCE, SPELL_FLASH_HEAL });
                    DoCast(target, SPELL_PURGE_THE_WICKED);
                }
                purge_the_wicked.Repeat(2s, 3s);
            })
            .Schedule(15s, 20s, [this](TaskContext power_word_shield)
            {
                if (Unit* target = SelectRandomMissingBuff(SPELL_ATONEMENT_EFFECT))
                {
                    CastSpellExtraArgs args;
                    args.AddSpellBP0(target->CountPctFromMaxHealth(20));

                    CastStop({ SPELL_ULTIMATE_PENITENCE, SPELL_POWER_WORD_RADIANCE, SPELL_FLASH_HEAL });
                    DoCast(target, SPELL_POWER_WORD_SHIELD, args);
                }
                power_word_shield.Repeat(8s, 12s);
            })
            .Schedule(18s, 24s, [this](TaskContext renew)
            {
                if (Unit* target = SelectRandomMissingBuff(SPELL_ATONEMENT_EFFECT))
                {
                    CastStop({ SPELL_ULTIMATE_PENITENCE, SPELL_POWER_WORD_RADIANCE, SPELL_FLASH_HEAL });
                    DoCast(target, SPELL_RENEW);
                }
                renew.Repeat(20s, 30s);
            })
            .Schedule(5s, 8s, [this](TaskContext flash_heal)
            {
                if (Unit* target = DoSelectLowestHpFriendly(40.0f))
                {
                    CastStop({ SPELL_ULTIMATE_PENITENCE, SPELL_POWER_WORD_RADIANCE, SPELL_FLASH_HEAL });
                    DoCast(target, SPELL_FLASH_HEAL);
                }
                flash_heal.Repeat(8s, 12s);
            })
            .Schedule(10s, 15s, [this](TaskContext power_word_radiance)
            {
                if (Unit* target = DoSelectLowestHpFriendly(40.0f))
                {
                    CastStop({ SPELL_ULTIMATE_PENITENCE, SPELL_POWER_WORD_RADIANCE });
                    DoCast(target, SPELL_POWER_WORD_RADIANCE);
                }
                power_word_radiance.Repeat(15s, 22s);
            });
	}
};

struct npc_mage_commander_zuros : public CustomAI
{
	npc_mage_commander_zuros(Creature* creature) : CustomAI(creature, true, AI_Type::Melee),
        cleric(nullptr), surdiel(nullptr)
	{
		Initialize();

        instance = creature->GetInstanceScript();
	}

	enum Spells
	{
		SPELL_ACCELERATING_BLAST    = 203176,
		SPELL_NETHER_WOUND          = 211000,
		SPELL_TIME_STOP             = 279062
	};

    enum Talks
	{
		SAY_INTRO_ZUROS_01          = 0,
		SAY_INTRO_SURDIEL_02        = 0,
		SAY_INTRO_ZUROS_03          = 1,
		SAY_INTRO_SURDIEL_04        = 1
	};

    enum Misc
    {
        // Spells
		SPELL_POWER_WORD_BARRIER    = 62618,
		SPELL_LEAP_OF_FAITH         = 173133,
		SPELL_ARCANE_DETONATION     = 300519,
		SPELL_ICE_BLOCK             = 304463,

        // NPCs
		NPC_STORMWIND_CLERIC        = 68708,
		NPC_SILVER_ASSASSIN         = 68045,
		NPC_SILVER_AGENT            = 68692,
		NPC_SILVER_SPELLBOW         = 68043,
    };

    InstanceScript* instance;
    EventMap events;
    Creature* surdiel;
    Creature* cleric;
    std::list<Creature*> allies, sunreavers;

    const Position surdielPoint01   = { -1021.14f, 4520.91f, 739.49f, 5.91f };

    float GetDamageReductionToUnit() override
    {
        return 0.01f;
    };

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

    void DoAction(int32 action) override
    {
        if (action != ACTION_DISPELL_BARRIER)
            return;

        surdiel = instance->GetCreature(DATA_MAGISTER_SURDIEL);
        if (!surdiel)
            return;

       cleric = GetClosestCreatureWithEntry(me, NPC_STORMWIND_CLERIC, 80.f);
        if (!cleric)
            return;

        cleric->SetImmuneToAll(true);

        cleric->GetCreatureListWithEntryInGrid(allies, NPC_SILVER_ASSASSIN, 40.0f);
        cleric->GetCreatureListWithEntryInGrid(allies, NPC_SILVER_SPELLBOW, 40.0f);
        cleric->GetCreatureListWithEntryInGrid(allies, NPC_SILVER_AGENT, 40.0f);

        if (allies.size() > 0)
        {
            for (Creature* ally : allies)
                ally->SetImmuneToAll(true);
        }

        GetCreatureListWithEntryInGrid(sunreavers, me, NPC_SUNREAVER_CITIZEN_STASIS, 85.0f);
        GetCreatureListWithEntryInGrid(sunreavers, me, NPC_SUNREAVER_CITIZEN, 85.0f);

        events.ScheduleEvent(1, 1s);
    }

    void UpdateAI(uint32 diff) override
    {
        CustomAI::UpdateAI(diff);

        events.Update(diff);

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case 1:
                    me->AI()->Talk(SAY_INTRO_ZUROS_01);
                    events.ScheduleEvent(2, 2s);
                    break;
                case 2:
                    surdiel->AI()->Talk(SAY_INTRO_SURDIEL_02);
                    events.ScheduleEvent(3, 800ms);
                    break;
                case 3:
                    surdiel->SetEmoteState(EMOTE_STATE_CUSTOM_SPELL_01);
                    surdiel->CastSpell(surdiel, SPELL_ICE_BLOCK);
                    me->AI()->Talk(SAY_INTRO_ZUROS_03);
                    cleric->CastSpell(cleric, SPELL_POWER_WORD_BARRIER, true);
                    if (Map* map = cleric->GetMap())
                    {
                        map->DoOnPlayers([this](Player* player)
                        {
                            if (!player->IsWithinDist(cleric, 6.0f))
                                cleric->CastSpell(player, SPELL_LEAP_OF_FAITH, true);
                        });
                    }
                    for (Creature* ally : allies)
                    {
                        const Position dest = GetRandomPosition(cleric, 5.0f, false);
                        ally->SetHomePosition(dest);
                        ally->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, dest, true, dest.GetOrientation());
                    }
                    events.ScheduleEvent(4, 800ms);
                    break;
                case 4:
                    if (Creature* dummy = DoSummon(NPC_INVISIBLE_STALKER, me->GetPosition(), 20s, TEMPSUMMON_TIMED_DESPAWN))
                        dummy->CastSpell(dummy, SPELL_ARCANE_DETONATION);
                    events.ScheduleEvent(5, 1500ms);
                    break;
                case 5:
                    FeingDeath(me);
                    for (Creature* sunreaver : sunreavers)
                    {
                        sunreaver->KillSelf();
                        sunreaver->SetCorpseDelay(7200);
                    }
                    events.ScheduleEvent(6, 3s);
                    break;
                case 6:
                    surdiel->SetEmoteState(EMOTE_STATE_NONE);
                    surdiel->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_02, surdielPoint01, true, surdielPoint01.GetOrientation());
                    surdiel->SetHomePosition(surdielPoint01);
                    surdiel->AI()->Talk(SAY_INTRO_SURDIEL_04);
                    events.ScheduleEvent(7, 2s);
                    break;
                case 7:
                    surdiel->SetImmuneToPC(false);
                    surdiel->SetReactState(REACT_AGGRESSIVE);
                    cleric->SetImmuneToAll(false);
                    for (Creature* ally : allies)
                        ally->SetImmuneToAll(false);
                    break;
                default:
                    break;
            }
        }
    }

    void FeingDeath(Creature* creature)
    {
        creature->RemoveAllAuras();
        creature->SetRegenerateHealth(false);
        creature->SetHealth(0U);
        creature->SetStandState(UNIT_STAND_STATE_DEAD);
        creature->SetUnitFlag2(UNIT_FLAG2_FEIGN_DEATH);
        creature->SetUnitFlag2(UNIT_FLAG2_PLAY_DEATH_ANIM);
        creature->SetImmuneToAll(true);
        creature->SetUninteractible(true);
    }
};

struct npc_narasi_snowdawn : public CustomAI
{
	npc_narasi_snowdawn(Creature* creature) : CustomAI(creature, true)
	{
		Initialize();

        blastInfo = sSpellMgr->AssertSpellInfo(SPELL_ACCELERATING_BLAST, DIFFICULTY_NONE);
	}

	enum Spells
	{
		SPELL_ACCELERATING_BLAST    = 203176,
		SPELL_TIME_STOP             = 215005,
        SPELL_ORB_OF_DEVASTATION    = 382261,
        SPELL_TEMPORAL_BURST        = 376679,
	};

    const SpellInfo* blastInfo;

    float GetDamageReductionToUnit() override
    {
        return 0.01f;
    };

    void DamageFromNPC(Unit* attacker, uint32& damage, DamageEffectType damageType) override
    {
        if (!attacker || damageType == HEAL)
            return;

        if (attacker->GetTypeId() == TYPEID_PLAYER)
        {
            damage = 0;
        }
        else if (attacker->GetTypeId() == TYPEID_UNIT && attacker->ToCreature())
        {
            damage *= GetDamageReductionToUnit();
        }
    }

	void JustEngagedWith(Unit* /*who*/) override
	{
		scheduler
			.Schedule(8s, [this](TaskContext accelerating_blast)
			{
				DoCastVictim(SPELL_ACCELERATING_BLAST);
				accelerating_blast.Repeat(Milliseconds(blastInfo->CalcCastTime()));
			})
            .Schedule(18s, 25s, [this](TaskContext temporal_burst)
            {
                CastStop();
                DoCastAOE(SPELL_TEMPORAL_BURST);
                temporal_burst.Repeat(50s, 1min);
            })
			.Schedule(12s, [this](TaskContext time_stop)
			{
				DoCastVictim(SPELL_TIME_STOP);
				time_stop.Repeat(45s, 50s);
            })
            .Schedule(21s, [this](TaskContext orb_of_devastation)
            {
                DoCastVictim(SPELL_ORB_OF_DEVASTATION);
                orb_of_devastation.Repeat(30s, 50s);
            });
	}

	bool CanAIAttack(Unit const* who) const override
	{
		return who->IsAlive() && who->GetEntry() != NPC_GRAND_MAGISTER_ROMMATH && who->GetTypeId() != TYPEID_PLAYER;
	}
};

struct npc_archmage_landalock : public NullCreatureAI
{
	const Position sorinPos = { -844.82f, 4471.00f, 735.87f, 5.50f };

	npc_archmage_landalock(Creature* creature) : NullCreatureAI(creature), eventId(0)
	{
		instance = creature->GetInstanceScript();

		if (Creature* icewall = me->FindNearestCreature(NPC_ICEWALL, 15.f))
		{
			summon = icewall->GetPosition();

			// 24H
			uint32 delay = 86400;
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
		SPELL_ICE_BURST             = 283591,
		// Gossips
		GOSSIP_MENU_DEFAULT         = 65003,
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

		me->RemoveAllAuras();
		me->CastSpell(stalker->GetPosition(), SPELL_TELEPORT);

		citizens.clear();

		GetCreatureListWithEntryInGrid(citizens, stalker, NPC_DALARAN_CITIZEN, 35.f);
		if (!citizens.empty())
		{
			events.ScheduleEvent(9, 800ms);
		}
		else
		{
			me->RemoveAllAuras();
			me->CastSpell(stalker->GetPosition(), SPELL_TELEPORT);
			me->DespawnOrUnsummon(800ms);
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
			#ifdef CUSTOM_DEBUG
				Creature* citizen = citizens.front();
				for (uint8 i = 0; i < 20; i++)
					KillRewarder::Reward(player, citizen);
			#else
				for (Creature* citizen : citizens)
				{
                    if (!citizen || citizen->isDead())
                        continue;

					KillRewarder::Reward(player, citizen);

					citizen->CastSpell(citizen, SPELL_TELEPORT_TARGET);
					citizen->DisappearAndDie();
				}
			#endif
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
						KillRewarder::Reward(player, me);
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
					if (GameObject* collider = me->FindNearestGameObject(GOB_ICE_WALL_COLLISION, 15.f))
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
					float slice = 2 * (float)M_PI / (float)citizens.size();
					for (Creature* citizen : citizens)
					{
                        if (!citizen || citizen->isDead())
                            continue;

						constexpr uint32 delay = std::numeric_limits<uint32>::max();
						citizen->SetRespawnDelay(delay);
						citizen->SetRespawnTime(delay);

						float angle = slice * index;
						const Position dest = GetRandomPositionAroundCircle(me, angle, 3.2f);
						citizen->SetHomePosition(dest);
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

struct npc_arcanist_rathaella : public CustomAI
{
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

		me->SetWalk(false);
	}

	void OnSpellClick(Unit* clicker, bool spellClickHandled) override
	{
		if (!spellClickHandled)
			return;

		if (clicker->GetTypeId() != TYPEID_PLAYER)
			return;

		playerGUID = clicker->GetGUID();

		#ifdef CUSTOM_DEBUG
			if (Player* player = ObjectAccessor::GetPlayer(*me, playerGUID))
				KillRewarder::Reward(player, me);
		#endif
	}

    void WaypointPathEnded(uint32 /*pointId*/, uint32 pathId) override
    {
        if (pathId == PATH_RATHAELLA_01)
        {
            me->SetFacingTo(5.41f);
            me->AI()->Talk(SAY_ARCANIST_RATHAELLA_03);
            scheduler.Schedule(4s, [this](TaskContext /*context*/)
            {
                me->GetMotionMaster()->MovePath(RathaellaPath02, false);
                if (Player* player = ObjectAccessor::GetPlayer(*me, playerGUID))
                    KillRewarder::Reward(player, me);
            });
        }
        else if (pathId == PATH_RATHAELLA_02)
        {
            me->SetFacingTo(0.05f);
            me->AddAura(SPELL_READING_BOOK_STANDING, me);
            me->SetHomePosition(me->GetPosition());
        }
    }

	void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
	{
		if (spellInfo->Id != SPELL_FREE_CAPTIVE)
			return;

		me->RemoveAllAuras();
		me->GetMotionMaster()->Clear();
		me->GetMotionMaster()->MoveIdle();
		me->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
		me->AI()->Talk(SAY_ARCANIST_RATHAELLA_01);
		me->SetWalk(false);

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
					me->GetMotionMaster()->MovePath(RathaellaPath01, false);
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
		instance = me->GetInstanceScript();
	}

	enum Misc
	{
		GOSSIP_MENU_DEFAULT         = 65005
	};

	InstanceScript* instance;

	bool OnGossipHello(Player* player) override
	{
		DLPPhases phase = (DLPPhases)instance->GetData(DATA_SCENARIO_PHASE);
		if (phase > DLPPhases::TheEscape)
			return false;

		player->PrepareGossipMenu(me, GOSSIP_MENU_DEFAULT, true);
		player->SendPreparedGossip(me);
		return true;
	}

	bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
	{
		ClearGossipMenuFor(player);

		switch (gossipListId)
		{
			case 1:
				player->CastSpell(GuardianPos01, SPELL_TELEPORT);
				break;
		}

		CloseGossipMenuFor(player);
		return true;
	}

	void JustAppeared() override
	{
		me->SetImmuneToAll(true);
		me->CastSpell(me, SPELL_ARCANE_BARRIER, true);
		me->CastSpell(me, SPELL_RUNES_OF_SHIELDING, true);
	}

	void UpdateAI(uint32 diff) override
	{
		scheduler.Update(diff);
	}

	private:
	TaskScheduler scheduler;
};

///
///     HORDE NPC
///

struct npc_sunreaver_citizen : public CustomAI
{
	npc_sunreaver_citizen(Creature* creature) : CustomAI(creature)
	{
		Initialize();
	}

	enum Spells
	{
		SPELL_SCORCH                = 17195,
		SPELL_CINDER_BOLT           = 384194,
	};

	InstanceScript* instance;

	void Initialize()
	{
		instance = me->GetInstanceScript();

		me->SetSpawnHealth();
		me->SetFullHealth();
		me->SetEmoteState(EMOTE_STATE_COWER);
	}

	void JustAppeared() override
	{
		DLPPhases phase = (DLPPhases)instance->GetData(DATA_SCENARIO_PHASE);
		if (phase >= DLPPhases::FindJaina02)
		{
			me->CombatStop();
			me->SetFaction(FACTION_FRIENDLY);
			me->setActive(false);
			me->SetVisible(false);
		}
	}

	void AttackStart(Unit* who) override
	{
		if (me->HasUnitState(UNIT_STATE_FLEEING_MOVE))
		{
			me->GetMotionMaster()->Clear();
		}

		me->CallAssistance();
		me->SetEmoteState(EMOTE_STATE_NONE);

		CustomAI::AttackStart(who);
	}

	void JustEngagedWith(Unit* who) override
	{
		DoCast(who, SPELL_CINDER_BOLT);

		scheduler
			.Schedule(2s, [this](TaskContext cinder_bolt)
			{
				DoCastVictim(SPELL_CINDER_BOLT);
				cinder_bolt.Repeat(2800ms);
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

struct npc_sunreaver_unit : public CustomAI
{
	npc_sunreaver_unit(Creature* creature, AI_Type type = AI_Type::Distance) : CustomAI(creature, type)
	{
		Initialize();
	}

	void Reset() override
	{
		CustomAI::Reset();
	}

	void MovementInform(uint32 /*type*/, uint32 id) override
	{
		switch (id)
		{
			case MOVEMENT_INFO_POINT_01:
				me->SetReactState(REACT_AGGRESSIVE);
				me->RemoveUnitFlag(UnitFlags::UNIT_FLAG_NON_ATTACKABLE);
				break;
		}
	}
};

struct npc_sunreaver_pyromancer : public npc_sunreaver_unit
{
	npc_sunreaver_pyromancer(Creature* creature) : npc_sunreaver_unit(creature)
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
		npc_sunreaver_unit::Reset();

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
			.Schedule(3s, 8s, [this](TaskContext flamestrike)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_FLAMESTRIKE);
				flamestrike.Repeat(14s, 22s);
			})
			.Schedule(14s, 25s, [this](TaskContext rinf_of_fire)
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

struct npc_sunreaver_aegis : public npc_sunreaver_unit
{
	npc_sunreaver_aegis(Creature* creature) : npc_sunreaver_unit(creature, AI_Type::Melee), healthLow(false)
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
		npc_sunreaver_unit::Reset();

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
		if (roll_chance_i(30))
			DoCastSelf(SPELL_AVENGING_WRATH);

		scheduler
			.Schedule(2s, 8s, [this](TaskContext holy_light)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(40.f, 80))
					DoCast(target, SPELL_HOLY_LIGHT);
				holy_light.Repeat(8s, 15s);
			})
			.Schedule(12s, 20s, [this](TaskContext judgment)
			{
				DoCastVictim(SPELL_JUDGMENT);
				judgment.Repeat(12s, 15s);
			})
			.Schedule(25s, 32s, [this](TaskContext divine_storm)
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

struct npc_sunreaver_summoner : public npc_sunreaver_unit
{
	npc_sunreaver_summoner(Creature* creature) : npc_sunreaver_unit(creature)
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
		return 15.f;
	}

	void Reset() override
	{
		npc_sunreaver_unit::Reset();

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
			.Schedule(5s, 8s, [this](TaskContext arcane_explosion)
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

struct npc_sunreaver_captain : public CustomAI
{
	const Position center = { -743.33f, 4289.46f, 729.07f, 2.29f };

	npc_sunreaver_captain(Creature* creature) : CustomAI(creature, AI_Type::Melee),
		hostsEvent(false)
	{
	}

	enum Spells
	{
		SPELL_BATTER                = 43518,
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
	bool hostsEvent;

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
		if (hostsEvent)
			return;

		hostsEvent = true;

		hosts.clear();

		GetCreatureListWithEntryInGrid(hosts, me, NPC_WANTON_HOST, 8.f);
		GetCreatureListWithEntryInGrid(hosts, me, NPC_WANTON_HOSTESS, 8.f);

		if (!hosts.empty())
		{
			for (Creature* host : hosts)
			{
				host->SetEmoteState(EMOTE_STATE_NONE);
				host->SetStandState(UNIT_STAND_STATE_STAND);
				host->RemoveAllAuras();

				if (host->GetEntry() == NPC_WANTON_HOSTESS)
					host->AI()->Talk(SAY_WANTON_HOSTESS_FLEE);

				const Position dest = GetRandomPosition(center, 5.f);
				scheduler
					.Schedule(2s, 5s, [host, dest, this](TaskContext /*context*/)
					{
						host->SetEmoteState(EMOTE_STATE_COWER);
						host->SetHomePosition(dest);
						host->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, dest);
						host->SetFacingToObject(me);
					})
					.Schedule(30s, [host, this](TaskContext /*context*/)
					{
						host->DisappearAndDie();
					});
			}
		}
	}
};

struct npc_magister_brasael : public CustomAI
{
	static constexpr uint32 BAGS_COUNT = 50;
	static constexpr uint32 DAMAGE_LIMITATION = 5000;

	npc_magister_brasael(Creature* creature) : CustomAI(creature, AI_Type::Distance),
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
	Position barrierPoint01;

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

	void JustAppeared() override
	{
		if (Creature* barrier = GetClosestCreatureWithEntry(me, NPC_ARCANE_BARRIER, 60.f))
		{
			barrierPoint01 = barrier->GetPosition();

			barrier->SetOwnerGUID(me->GetGUID());
			barrier->SetObjectScale(1.2f);
		}
	}

	void Initialize() override
	{
		CustomAI::Initialize();

		cauterized = false;
		damagedBags = 0;

        SetBoundary(&brasaelBoundaries);
    }

	void Reset() override
	{
		CustomAI::Reset();

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

    bool CheckInRoom() override
    {
        if (Unit* victim = me->GetVictim())
        {
            if (IsInBoundary(victim) && IsInBoundary())
                return true;
        }
        else
        {
            if (IsInBoundary())
                return true;
        }

        EnterEvadeMode(EvadeReason::Boundary);
        return false;
    }

	bool CanAIAttack(Unit const* who) const
	{
		return who->IsAlive() && me->IsValidAttackTarget(who) && ScriptedAI::CanAIAttack(who);
	}
};

struct npc_magister_surdiel : public CustomAI
{
	static constexpr int ELEMENTAL_COUNT = 2;
	static constexpr int TELEPORT_COUNT = 2;
	static constexpr int HEALTH_PERCENT_COUNT = 3;

	npc_magister_surdiel(Creature* creature) : CustomAI(creature, true, AI_Type::Stay), combatFinal(false),
        healthPercentIndex(0), elementals(creature), countdown(0), fireballInfo(nullptr), canSummon(false)
	{
		instance = me->GetInstanceScript();
	}

	enum Spells
	{
		SPELL_MASS_POLYMORPH        = 29963,
		SPELL_FIREBALL              = 79854,
        SPELL_BLAZING_SPEED         = 116592,
		SPELL_RAIN_OF_FIRE          = 418554,
		SPELL_PYROBLAST             = 255998,
		SPELL_FIRE_BOMB             = 270956,
	};

	enum Misc
	{
		// Spells
		SPELL_FIRE_DISSOLVE_IN      = 233300,
        SPELL_FIRE_BOMB_VISUAL      = 396694,
        SPELL_TELEPORT_VISUAL       = 389213,

		// NPCs
		NPC_ROMMATH_PORTAL          = 68636,
		NPC_TEMP_MAGISTER_ROMMATH   = 68586,
		NPC_FIRE_ELEMENTAL          = 500024,

        // Say
        SAY_INTRO_ROMMATH_01        = 0,
        SAY_BOMBS_ALERT             = 5,
        SAY_COUNTDOWN_BOMBS         = 6,

        // Groups
        GROUP_COMBAT                = 500,
        GROUP_OUTRO
	};

    const SpellInfo* fireballInfo;

	InstanceScript* instance;
	Position barrierPoint01;
	ObjectGuid rommathGUID;
    SummonList elementals;
    uint32 countdown;
    uint8 healthPercentIndex;
    bool combatFinal;
    bool canSummon;

	const Position portalPoint01    = { -1011.90f, 4530.87f, 739.49f, 4.92f };
	const Position rommathPoint01   = { -1013.63f, 4526.73f, 739.49f, 5.16f };

    const float healthPercent[HEALTH_PERCENT_COUNT] =
    {
        75.f, 50.f, 25.f
    };

	const Position elementalPoints[ELEMENTAL_COUNT] =
	{
		{ -1005.18f, 4522.26f, 739.49f, 5.55f },
		{ -1011.13f, 4508.41f, 739.49f, 6.17f }
	};

    const Position teleportPoints[TELEPORT_COUNT] =
    {
        { -1013.45f, 4503.02f, 739.49f, 1.18f },
        { -1002.79f, 4527.43f, 739.49f, 4.29f },
    };

    void Initialize() override
    {
        CustomAI::Initialize();

        fireballInfo = sSpellMgr->AssertSpellInfo(SPELL_FIREBALL, DIFFICULTY_NONE);
    }

    void Reset() override
    {
        Initialize();

        me->RemoveAllAreaTriggers();
        me->RemoveAllDynObjects();

        scheduler.CancelGroup(GROUP_COMBAT);

        ScriptedAI::Reset();
    }

    float GetDamageReductionToUnit() override
    {
        return 0.01f;
    };

	void JustAppeared() override
	{
		if (Creature* barrier = GetClosestCreatureWithEntry(me, NPC_ARCANE_BARRIER, 60.f))
		{
			barrierPoint01 = barrier->GetPosition();

			barrier->SetOwnerGUID(me->GetGUID());
			barrier->SetObjectScale(1.2f);
		}
	}

    void JustSummoned(Creature* summon) override
    {
        if (summon->GetEntry() != NPC_FIRE_ELEMENTAL)
            CustomAI::JustSummoned(summon);
        else
            elementals.Summon(summon);
    }

    void SummonedCreatureDies(Creature* summon, Unit* killer) override
    {
        if (summon->GetEntry() == NPC_FIRE_ELEMENTAL)
        {
            summon->DespawnOrUnsummon();
            elementals.Despawn(summon);
        }

        CustomAI::SummonedCreatureDies(summon, killer);
    }

	void EnterEvadeMode(EvadeReason why) override
	{
        me->RemoveAllAreaTriggers();
        me->RemoveAllDynObjects();

        scheduler.CancelGroup(GROUP_COMBAT);

        ScriptedAI::EnterEvadeMode(why);

        elementals.DespawnAll();

        if (canSummon && !combatFinal)
            SummonElementals();
	}

	void DoAction(int32 action) override
	{
        DLPPhases phase = (DLPPhases)instance->GetData(DATA_SCENARIO_PHASE);
        if (phase >= DLPPhases::TheEscape)
            return;

		if (action != ACTION_DISPELL_BARRIER)
			return;

		Creature* zuros = instance->GetCreature(DATA_MAGE_COMMANDER_ZUROS);
		if (!zuros)
			return;

		me->SetReactState(REACT_PASSIVE);
		me->CombatStop(true);

		zuros->SetReactState(REACT_PASSIVE);
		zuros->SetRegenerateHealth(false);
		zuros->CombatStop(true);
		zuros->SetImmuneToPC(false);
		zuros->SetStandState(UNIT_STAND_STATE_KNEEL);
        zuros->AI()->DoAction(ACTION_DISPELL_BARRIER);
	}

	void MovementInform(uint32 /*type*/, uint32 id) override
	{
        DLPPhases phase = (DLPPhases)instance->GetData(DATA_SCENARIO_PHASE);
        if (phase >= DLPPhases::TheEscape)
            return;

		switch (id)
		{
			case MOVEMENT_INFO_POINT_01:
			{
				me->SetHomePosition(portalPoint01);
				DoCast(SPELL_TELEPORT_VISUAL_ONLY);
				if (Creature* rommath = ObjectAccessor::GetCreature(*me, rommathGUID))
				{
					rommath->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, portalPoint01);
					scheduler.Schedule(1s, GROUP_OUTRO, [rommath, this](TaskContext /*context*/)
					{
						Map* map = me->GetMap();
						if (map && map->GetPlayers().getSize() > 0)
						{
							if (Player* player = map->GetPlayers().begin()->GetSource())
								KillRewarder::Reward(player, me);
						}

						rommath->CastSpell(rommath, SPELL_TELEPORT_VISUAL_ONLY);
						rommath->DespawnOrUnsummon(1s);

						me->CombatStop();
						me->SetImmuneToAll(true);
						me->NearTeleportTo(SurdielPos03);
						me->SetHomePosition(SurdielPos03);
						me->SetRegenerateHealth(true);
						me->SetFullHealth();
					});
				}
				break;
			}
			case MOVEMENT_INFO_POINT_02:
                canSummon = true;
                SummonElementals();
				break;
		}
	}

	void DamageTaken(Unit* attacker, uint32& damage, DamageEffectType damageType, SpellInfo const* /*spellInfo = nullptr*/) override
	{
        DamageFromNPC(attacker, damage, damageType);

        DLPPhases phase = (DLPPhases)instance->GetData(DATA_SCENARIO_PHASE);
        if (phase >= DLPPhases::TheEscape)
            return;

		if (me->HealthBelowPctDamaged(15, damage))
		{
			damage = 0;

			if (!combatFinal)
			{
				combatFinal = true;

				CastStop();

                scheduler.CancelGroup(GROUP_COMBAT);

                me->RemoveAllDynObjects();
				me->RemoveAllAreaTriggers();
				me->SetReactState(REACT_PASSIVE);
				me->SetSpeedRate(MOVE_RUN, 0.8f);

				scheduler.Schedule(1s, GROUP_OUTRO, [this](TaskContext context)
				{
					switch (context.GetRepeatCounter())
					{
						case 0:
                            DoCast(SPELL_MASS_POLYMORPH);
                            context.Repeat(1s);
                            break;
						case 1:
                            DoSummon(NPC_ROMMATH_PORTAL, portalPoint01, 15s, TEMPSUMMON_TIMED_DESPAWN);
							context.Repeat(1s);
							break;
						case 2:
							if (Creature* rommath = DoSummon(NPC_TEMP_MAGISTER_ROMMATH, portalPoint01))
							{
								rommathGUID = rommath->GetGUID();
								rommath->CastSpell(rommath, SPELL_TELEPORT_VISUAL_ONLY);
								rommath->SetImmuneToAll(true);
								rommath->SetSpeedRate(MOVE_RUN, 0.6f);
								rommath->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, rommathPoint01, true, rommathPoint01.GetOrientation());
							}
							context.Repeat(2s);
							break;
						case 3:
							if (Creature* rommath = ObjectAccessor::GetCreature(*me, rommathGUID))
								rommath->AI()->Talk(SAY_INTRO_ROMMATH_01);
							context.Repeat(2s);
							break;
						case 4:
							me->SetRegenerateHealth(false);
							me->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_01, portalPoint01);
                            break;
					}
				});
			}
		}
    }

	void JustEngagedWith(Unit* /*who*/) override
	{
        me->CallAssistance();

		scheduler
            .Schedule(1ms, GROUP_COMBAT, [this](TaskContext fire_bombs)
            {
                if (healthPercentIndex < HEALTH_PERCENT_COUNT)
                {
                    float percent = healthPercent[healthPercentIndex];
                    if (me->HealthBelowPct(percent))
                    {
                        healthPercentIndex++;

                        CastStop();
                        SummonFireBombs();
                        fire_bombs.Repeat(1min, 3min);
                    }
                    else
                        fire_bombs.Repeat(1s);
                }
            })
			.Schedule(1ms, GROUP_COMBAT, [this](TaskContext fireball)
			{
				DoCastVictim(SPELL_FIREBALL);
				fireball.Repeat(Milliseconds(fireballInfo->CalcCastTime()));
			})
			.Schedule(2s, GROUP_COMBAT, [this](TaskContext pyroblast)
			{
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                {
                    CastStop({ SPELL_PYROBLAST, SPELL_MASS_POLYMORPH });
                    DoCast(target, SPELL_PYROBLAST);
                }
				pyroblast.Repeat(8s, 14s);
			})
			.Schedule(5s, GROUP_COMBAT, [this](TaskContext rain_of_fire)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				{
					CastStop(SPELL_MASS_POLYMORPH);
					DoCast(target, SPELL_RAIN_OF_FIRE);
				}
				rain_of_fire.Repeat(1min, 90s);
            })
            .Schedule(8s, GROUP_COMBAT, [this](TaskContext blazing_speed)
            {
                DoCastSelf(SPELL_BLAZING_SPEED);
                blazing_speed.Repeat(45s, 1min);
            });
	}

    bool CheckInRoom() override
    {
        DLPPhases phase = (DLPPhases)instance->GetData(DATA_SCENARIO_PHASE);
        if (phase >= DLPPhases::TheEscape)
            return true;

        if (Unit* victim = me->GetVictim())
        {
            if (IsInBoundary(victim) && IsInBoundary())
                return true;
        }
        else
        {
            if (IsInBoundary())
                return true;
        }

        EnterEvadeMode(EvadeReason::Boundary);
        return false;
    }

    void SummonElementals()
    {
        for (int i = 0; i < ELEMENTAL_COUNT; i++)
        {
            if (Creature* elemental = me->SummonCreature(NPC_FIRE_ELEMENTAL, elementalPoints[i]))
            {
                elemental->CastSpell(elemental, SPELL_FIRE_DISSOLVE_IN, true);
                elemental->SetOwnerGUID(me->GetGUID());
                elemental->SetHomePosition(elementalPoints[i]);
            }
        }
    }

    void SummonFireBombs()
    {
        for (auto* ref : me->GetThreatManager().GetUnsortedThreatList())
        {
            if (Unit* target = ref->GetVictim())
            {
                if (target->HasAura(SPELL_FIRE_BOMB_VISUAL))
                    return;

                target->AddAura(SPELL_FIRE_BOMB_VISUAL, target);
            }
        }

        Talk(SAY_BOMBS_ALERT);

        scheduler
            .Schedule(2s, GROUP_COMBAT, [this](TaskContext counterAlert)
            {
                switch (counterAlert.GetRepeatCounter())
                {
                    case 0:
                    case 1:
                    case 2:
                        Talk(SAY_COUNTDOWN_BOMBS + countdown);
                        countdown++;
                        counterAlert.Repeat(1s);
                        break;
                }
            })
            .Schedule(5s, GROUP_COMBAT, [this](TaskContext /*context*/)
            {
                CastStop();
                Talk(SAY_COUNTDOWN_BOMBS + countdown);

                countdown = 0;

                CastStop();

                const Position dest = teleportPoints[urand(0, TELEPORT_COUNT)];
                me->CastSpell(dest, SPELL_TELEPORT_VISUAL, true);

                for (auto* ref : me->GetThreatManager().GetUnsortedThreatList())
                {
                    if (Unit* target = ref->GetVictim())
                    {
                        target->RemoveAurasDueToSpell(SPELL_FIRE_BOMB_VISUAL);

                        CastSpellExtraArgs args(TRIGGERED_FULL_MASK | TRIGGERED_CAST_DIRECTLY | TRIGGERED_IGNORE_GCD);
                        uint32 random = urand(0, 7);
                        float slice = 2 * float(M_PI) / 8;

                        for (uint8 i = 0; i < 8; ++i)
                        {
                            float angle = slice * i;
                            if (random != i)
                            {
                                const Position dest = GetRandomPositionAroundCircle(target, angle, 3.0f);
                                me->CastSpell(dest, SPELL_FIRE_BOMB, args);
                                me->GetSpellHistory()->ResetCooldown(SPELL_FIRE_BOMB);
                            }
                        }
                    }
                }
            });
    }
};

struct npc_high_arcanist_savor : public CustomAI
{
	static constexpr uint32 SAVOR_MAX_WAVES = 4;
	static constexpr int32 PLAYER_MAX_ALT_POWER = 2;
	static constexpr float SAVOR_SPAWN_COUNT = 2.0f;

	npc_high_arcanist_savor(Creature* creature) : CustomAI(creature, AI_Type::Stay), phase(Phases::None),
		clones(creature), lastSpeed(SPELL_SPEED_SLOW), timeCount(0), sunreaversCount(0), wavesCount(0),
		sunreaversPortal(nullptr), arcaneBarrier(ObjectGuid::Empty)
	{
		instance = me->GetInstanceScript();
	}

	enum Spells
	{
		// Player Clones
		SPELL_ETERNAL_SILENCE       = 42201,
		SPELL_TRANSPARENCY_50       = 44816,
		SPELL_CLONE_ME              = 45204,
		SPELL_REWIND_TIME           = 101590,
		SPELL_FROZEN_IN_TIME        = 195289,
		SPELL_EVOCATION             = 243070,

		// Time
		SPELL_SPEED_SLOW            = 207011,
		SPELL_SPEED_NORMAL          = 207012,
		SPELL_SPEED_FAST            = 207013,
        SPELL_ALT_POWER_BAR         = 102668,

		// Portal
		SPELL_ARCANE_FX             = 200065,
        SPELL_ARCANE_BARRIER        = 385965,

		// Final
		SPELL_BIG_BANG              = 222761,
        SPELL_AREA_ZONE_FX          = 230330,

		// Savor
		SPELL_IMMUNE                = 299144,
		SPELL_LIVING_LEDGERS        = 224745,
		SPELL_THROW_ARCANE_TOME     = 239101,
		SPELL_ARCANE_BOLT           = 290007,
		SPELL_LEVITATE              = 252620,
        SPELL_DELPHURIC_BEAM        = 423771
	};

	enum Misc
	{
		// NPCs
		NPC_SUNREAVER_PYROMANCER    = 68757,
		NPC_SUNREAVER_AEGIS         = 68051,
		NPC_SUNREAVER_SUMMONER      = 68760,
		NPC_BOOK_OF_ARCANE          = 120646,
		NPC_PLAYER_CLONE            = 500018,
		NPC_HORDE_PEON              = 126471,

		// Gobs
		GOB_PORTAL_TO_SEWERS        = 550003,

		// Talks
		SAY_SAVOR_SPEED_FAST        = 0,
		SAY_SAVOR_SPEED_SLOW        = 1,
		SAY_SAVOR_SPEED_NORMAL      = 2,
		SAY_SAVOR_REWIND            = 3,
		SAY_SAVOR_PORTAL_SPAWN      = 4,
		SAY_SAVOR_AGGRO             = 5,
		SAY_SAVOR_BIG_BANG          = 6,
	};

	enum class Phases
	{
		None,
		Intro,
		LivingLedger,
		Time,
		Portal,
		Final
	};

	enum Groups
	{
		GROUP_ALWAYS,
		GROUP_LIVING_LEDGERS,
		GROUP_TIME,
		GROUP_PORTAL
	};

	InstanceScript* instance;
	Phases phase;
	SummonList clones;
    ObjectGuid arcaneBarrier;
	GameObject* sunreaversPortal;
	uint32 lastSpeed;
	uint32 timeCount;
	uint32 sunreaversCount;
	uint32 wavesCount;

	const Position portalPoint01    = { -851.03f, 4438.99f, 653.60f, 1.62f };

    void Initialize() override
    {
        CustomAI::Initialize();

        me->SetRegenerateHealth(false);

        SetBoundary(&savorBoundaries);
    }

	void Reset() override
	{
		CustomAI::Reset();

        DoOnPlayers([this](Player* player)
        {
            player->RemoveAura(SPELL_ALT_POWER_BAR);
        });

        phase = Phases::None;
	}

	void EnterEvadeMode(EvadeReason why) override
	{
		CustomAI::EnterEvadeMode(why);
		DoCastOnPlayers(SPELL_SPEED_NORMAL);
		ClosePortal(sunreaversPortal);

		me->SetFullHealth();
		me->SetFullPower(me->GetPowerType());
	}

	void JustDied(Unit* killer) override
	{
		CustomAI::JustDied(killer);
		DoCastOnPlayers(SPELL_SPEED_NORMAL);
		ClosePortal(sunreaversPortal);

        DoOnPlayers([this](Player* player)
        {
            player->RemoveAura(SPELL_ALT_POWER_BAR);
        });
	}

	void JustSummoned(Creature* summon) override
	{
		CustomAI::JustSummoned(summon);

		switch (summon->GetEntry())
		{
			case NPC_SUNREAVER_PYROMANCER:
			case NPC_SUNREAVER_AEGIS:
			case NPC_SUNREAVER_SUMMONER:
				summon->SetMaxHealth(me->CountPctFromMaxHealth(urand(8, 15)));
				summon->SetFullHealth();
				break;
		}
	}

	void SummonedCreatureDies(Creature* summon, Unit* killer) override
	{
		CustomAI::SummonedCreatureDies(summon, killer);

		switch (summon->GetEntry())
		{
			case NPC_SUNREAVER_PYROMANCER:
			case NPC_SUNREAVER_AEGIS:
			case NPC_SUNREAVER_SUMMONER:
				sunreaversCount++;
				break;
		}
	}

	void DoAction(int32 action) override
	{
		switch (action)
		{
			case ACTION_ARCANE_ORB_DESPAWN:
				phase = Phases::Time;
				me->RemoveAurasDueToSpell(SPELL_IMMUNE);
				scheduler.CancelGroup(GROUP_LIVING_LEDGERS);
				scheduler.Schedule(2s, GROUP_TIME, [this](TaskContext spell_speed)
				{
					if (lastSpeed == SPELL_SPEED_FAST)
					{
						DoCastOnPlayers(SPELL_SPEED_SLOW);
						lastSpeed = SPELL_SPEED_SLOW;
					}
					else
					{
						DoCastOnPlayers(SPELL_SPEED_FAST);
						lastSpeed = SPELL_SPEED_FAST;
					}
					spell_speed.Repeat(15s);
				});
				break;
			case ACTION_HORDE_PORTAL_SPAWN:
				SummonSunreavers();
				scheduler.Schedule(1s, GROUP_PORTAL, [this](TaskContext check_hordes)
				{
					if (phase == Phases::Final)
						return;

					if (sunreaversCount >= SAVOR_SPAWN_COUNT)
					{
						if (wavesCount >= SAVOR_MAX_WAVES)
						{
							phase = Phases::Final;

							scheduler.CancelGroup(GROUP_PORTAL);
							scheduler.CancelGroup(GROUP_ALWAYS);

							me->AI()->Talk(SAY_SAVOR_BIG_BANG);
							me->RemoveAurasDueToSpell(SPELL_IMMUNE);
							me->RemoveAurasDueToSpell(SPELL_ARCANE_FX);
							me->RemoveAurasDueToSpell(SPELL_PORTAL_CHANNELING_03);
							me->RemoveAurasDueToSpell(SPELL_LEVITATE);
							me->SetReactState(REACT_AGGRESSIVE);

							ClosePortal(sunreaversPortal);

                            if (Creature* barrier = ObjectAccessor::GetCreature(*me, arcaneBarrier))
                            {
                                barrier->RemoveAllAuras();
                                barrier->CastSpell(barrier, SPELL_AREA_ZONE_FX);
                            }

							CastStop();
							DoCast(SPELL_BIG_BANG);

							DoCastOnPlayers(SPELL_SPEED_FAST);
						}
						else
						{
							SummonSunreavers();
						}
					}
					check_hordes.Repeat(1s);
				});
				break;
			default:
				break;
		}
	}

	void JustReachedHome() override
	{
		if (phase == Phases::Portal)
		{
			DoCast(me, SPELL_PORTAL_CHANNELING_03, true);
			DoCast(me, SPELL_ARCANE_FX, true);

            if (Creature* barrier = me->SummonCreature(WORLD_TRIGGER, me->GetPosition()))
            {
                barrier->CastSpell(barrier, SPELL_ARCANE_BARRIER);
                barrier->SetObjectScale(0.7f);
                arcaneBarrier = barrier->GetGUID();
            }
		}
	}

	void DamageTaken(Unit* /*attacker*/, uint32& /*damage*/, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
		if (phase != Phases::Time)
			return;

		// Only in Time phase

		if (HealthBelowPct(30))
		{
			timeCount++;
			if (phase != Phases::Final && timeCount >= 3)
			{
				phase = Phases::Portal;

				DoOnPlayers([this](Player* player)
				{
                    player->RemoveAura(SPELL_ALT_POWER_BAR);
					player->CastStop();
				});

				scheduler.CancelGroup(GROUP_TIME);
				scheduler.CancelGroup(GROUP_ALWAYS);

				summons.DespawnAll();

				me->SetReactState(REACT_PASSIVE);
				me->GetMotionMaster()->MoveTargetedHome();

				DoCastOnPlayers(SPELL_SPEED_NORMAL);

				DoCast(me, SPELL_IMMUNE, true);

				sunreaversPortal = me->SummonGameObject(GOB_PORTAL_TO_SILVERMOON, portalPoint01, QuaternionData::fromEulerAnglesZYX(portalPoint01.GetOrientation(), 0.0f, 0.0f), 0s);

				DoAction(ACTION_HORDE_PORTAL_SPAWN);
			}
			else
			{
				scheduler.Schedule(5ms, GROUP_TIME, [this](TaskContext rewind_time)
				{
					switch (rewind_time.GetRepeatCounter())
					{
						case 0:
							me->SetFullHealth();
							RewindTime(true);
							rewind_time.Repeat(4s);
							break;
						case 1:
							RewindTime(false);
							break;
					}
				});
			}
		}
	}

	void MoveInLineOfSight(Unit* who) override
	{
		ScriptedAI::MoveInLineOfSight(who);

        if (who->GetTypeId() != TYPEID_PLAYER)
            return;

        Player* player = who->ToPlayer();
        if (player && player->IsGameMaster())
            return;

		if (phase != Phases::None
			|| me->IsEngaged()
			|| who->IsFriendlyTo(me)
			|| !who->IsWithinDist(me, 45.0f))
		{
			return;
		}

		phase = Phases::Intro;

		std::list<Creature*> creatures;
		GetCreatureListWithEntryInGrid(creatures, me, NPC_SUNREAVER_PYROMANCER, 25.0f);
		GetCreatureListWithEntryInGrid(creatures, me, NPC_SUNREAVER_AEGIS, 25.0f);
		GetCreatureListWithEntryInGrid(creatures, me, NPC_SUNREAVER_SUMMONER, 25.0f);
		GetCreatureListWithEntryInGrid(creatures, me, NPC_HORDE_PEON, 25.0f);

        if (!creatures.empty())
        {
            for (Creature* creature : creatures)
            {
                creature->SetImmuneToAll(true);
                creature->CastSpell(creature, SPELL_TELEPORT_VISUAL_ONLY);
                creature->DespawnOrUnsummon(1300ms);
            }
        }

		me->RemoveAllAuras();
		me->AddAura(SPELL_LEVITATE, me);
        me->SetImmuneToAll(false);

		DoCast(me, SPELL_IMMUNE, true);
	}

    void JustEngagedWith(Unit* /*who*/) override
    {
        me->AI()->Talk(SAY_SAVOR_AGGRO);

        DoOnPlayers([this](Player* player)
        {
            player->AddAura(SPELL_ALT_POWER_BAR, player);
        });

		scheduler
			.Schedule(1ms, [this](TaskContext /*summon_time*/)
			{
				SummonPlayerClones();
			})
			.Schedule(2s, GROUP_ALWAYS,[this](TaskContext arcane_bolt)
			{
				DoCastVictim(SPELL_ARCANE_BOLT);
                arcane_bolt.Repeat(3s);
			})
			.Schedule(10s, GROUP_ALWAYS, [this](TaskContext delphuric_beam)
			{
                CastStop({ SPELL_EVOCATION, SPELL_THROW_ARCANE_TOME });
				DoCastAOE(SPELL_DELPHURIC_BEAM);
                delphuric_beam.Repeat(8s, 30s);
			})
			.Schedule(3s, GROUP_ALWAYS, [this](TaskContext evocation)
			{
				if (me->GetPowerPct(POWER_MANA) < 10.0f)
					DoCast(SPELL_EVOCATION);
				evocation.Repeat(3s);
			})
			.Schedule(5s, GROUP_LIVING_LEDGERS, [this](TaskContext throw_arcane_tome)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				{
                    CastStop(SPELL_EVOCATION);
					DoCast(target, SPELL_THROW_ARCANE_TOME);
				}
				throw_arcane_tome.Repeat(14s, 45s);
			})
			.Schedule(1s, GROUP_LIVING_LEDGERS, [this](TaskContext /*living_ledgers*/)
			{
				phase = Phases::LivingLedger;
				DoCast(SPELL_LIVING_LEDGERS);
			});
	}

    bool CheckInRoom() override
    {
        if (Unit* victim = me->GetVictim())
        {
            if (IsInBoundary(victim) && IsInBoundary())
                return true;
        }
        else
        {
            if (IsInBoundary())
                return true;
        }

        EnterEvadeMode(EvadeReason::Boundary);
        return false;
    }

	/*
	*       UTILS
	*

	*/

	#pragma region UTILS

	template <typename T>
	void DoOnPlayers(T&& fn)
	{
		if (Map* map = me->GetMap())
		{
			for (MapReference const& ref : map->GetPlayers())
				if (Player* player = ref.GetSource())
					fn(player);
		}
	}

	Player* GetFirstPlayer()
	{
		Map* map = me->GetMap();
		if (!map)
			return nullptr;

		return map->GetPlayers().getSize() <= 0 ? nullptr : map->GetPlayers().begin()->GetSource();
	}

	void SummonPlayerClones()
	{
		DoOnPlayers([this](Player* player)
		{
			if (player->IsWithinDist(me, 18.f))
			{
                if (Creature* clone = me->SummonCreature(NPC_PLAYER_CLONE, player->GetPosition()))
				{
					Powers power = player->GetPowerType();
                    int32 value = player->GetMaxPower(power);

					clone->SetImmuneToAll(true);
					clone->SetEmoteState(EMOTE_STATE_READY_UNARMED);
					clone->AddAura(SPELL_TRANSPARENCY_50, clone);
					clone->AddAura(SPELL_FROZEN_IN_TIME, clone);
                    clone->SetMaxHealth(player->GetMaxHealth());
					clone->SetFullHealth();
					clone->SetMaxPower(power, value);
					clone->SetFullPower(power);
					clone->SetOwnerGUID(player->GetGUID());

                    player->CastSpell(clone, SPELL_CLONE_ME, true);
                    player->SetPower(POWER_ALTERNATE_POWER, PLAYER_MAX_ALT_POWER);

					clones.Summon(clone);
				}
			}
		});
	}

	void SummonSunreavers()
	{
		me->AI()->Talk(SAY_SAVOR_PORTAL_SPAWN);
		me->SetFacingTo(4.11f);

		sunreaversCount = 0;

		wavesCount++;

		uint8 index = 0;
		const Position center = me->GetPosition();
		float slice = 2 * (float)M_PI / SAVOR_SPAWN_COUNT;
		for (uint8 i = 0; i < SAVOR_SPAWN_COUNT; i++)
		{
			uint32 entry = RAND(NPC_SUNREAVER_PYROMANCER, NPC_SUNREAVER_AEGIS, NPC_SUNREAVER_SUMMONER);
			float angle = slice * index;
			const Position dest = GetRandomPositionAroundCircle(me, angle, 3.2f);
			if (Creature* spawn = DoSummon(entry, portalPoint01))
			{
                spawn->SetReactState(REACT_PASSIVE);
				spawn->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
				spawn->CastSpell(spawn, SPELL_TELEPORT_VISUAL_ONLY);
				spawn->SetHomePosition(dest);
				spawn->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_01, dest, true, dest.GetOrientation());
			}

			index++;
		}
	}

	void RewindTime(bool apply)
	{
		DoOnPlayers([this, apply](Player* player)
		{
			if (!apply)
			{
				player->RemoveAurasDueToSpell(SPELL_ETERNAL_SILENCE);
				player->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
				player->SetClientControl(player, true);
				player->SetFacingToObject(me);
				player->CastSpell(player, lastSpeed, true);
			}
			else
			{
				me->AI()->Talk(SAY_SAVOR_REWIND);

				for (ObjectGuid guid : clones)
				{
					if (Creature* clone = ObjectAccessor::GetCreature(*me, guid))
					{
						if (!clone || clone->isDead())
							continue;

						if (clone->GetOwnerGUID() != player->GetGUID())
							continue;
						else
						{
							if (player->isDead())
							{
								player->RemoveAllAuras();
								player->ResurrectPlayer(100.0f);
							}

							float cloneDist = player->GetDistance2d(clone);

							player->CastSpell(player, SPELL_SPEED_NORMAL, true);
							player->CastSpell(player, SPELL_REWIND_TIME, true);
							player->CastSpell(player, SPELL_ETERNAL_SILENCE, true);
							player->SetUnitFlag(UNIT_FLAG_PACIFIED);
							player->SetClientControl(player, false);
							player->GetMotionMaster()->MoveCharge(clone->GetPositionX(), clone->GetPositionY(), clone->GetPositionZ(), cloneDist / 3.0f);
							player->ToPlayer()->SetFullHealth();
							player->ToPlayer()->SetFullPower(player->GetPowerType());
							player->GetSpellHistory()->ResetAllCooldowns();
							player->GetSpellHistory()->ResetAllCharges();

                            int32 charges = player->GetPower(POWER_ALTERNATE_POWER);
                            charges -= 1;

                            player->SetPower(POWER_ALTERNATE_POWER, charges);

							player->RemoveAura(57723);      // Heroism
							player->RemoveAura(57724);      // Bloodlust
							player->RemoveAura(80354);      // Time Warp
							player->RemoveAura(102381);     // Temporal Blast
						}
					}
				}
			}
		});
	}

	void DoCastOnPlayers(uint32 spellId)
	{
		switch (spellId)
		{
			case SPELL_SPEED_FAST:
				me->AI()->Talk(SAY_SAVOR_SPEED_FAST);
				break;
			case SPELL_SPEED_SLOW:
				me->AI()->Talk(SAY_SAVOR_SPEED_SLOW);
				break;
			case SPELL_SPEED_NORMAL:
				me->AI()->Talk(SAY_SAVOR_SPEED_NORMAL);
				break;
		}

		DoOnPlayers([this, spellId](Player* player)
		{
			player->CastSpell(player, spellId, true);
		});
	}

	#pragma endregion
};

struct npc_magister_hathorel : public CustomAI
{
	npc_magister_hathorel(Creature* creature) : CustomAI(creature)
	{
	}

	void MovementInform(uint32 /*type*/, uint32 id) override
	{
		switch (id)
		{
			case MOVEMENT_INFO_POINT_03:
				DoCast(SPELL_TELEPORT_VISUAL_ONLY);
				me->SetVisible(false);
				break;
		}
	}
};

///
///     SPECIAL NPC
///

struct npc_arcane_barrier : public NullCreatureAI
{
	npc_arcane_barrier(Creature* creature) : NullCreatureAI(creature),
        triggerGUID(ObjectGuid::Empty), mageGUID(ObjectGuid::Empty), colliderGUID(ObjectGuid::Empty)
	{
		instance = creature->GetInstanceScript();
	}

	enum Misc
	{
		// GameObjects
		GOB_COLLIDER                = 368679,

		// Spells
		SPELL_WAND_OF_DISPELLING    = 234966,
		SPELL_FREED_EXPLOSION       = 225253,
		SPELL_ARCANE_BARRIER_DAMAGE = 264848,
		SPELL_ARCANE_BARRIER        = 271187
	};

	TaskScheduler scheduler;
	InstanceScript* instance;
	ObjectGuid mageGUID;
    ObjectGuid triggerGUID;
	ObjectGuid colliderGUID;

	void JustDied(Unit* /*killer*/) override
	{
        Unit* owner = me->GetOwner();
        if (!owner)
            me->RemoveAllAuras();

		if (GameObject* collider = ObjectAccessor::GetGameObject(*me, colliderGUID))
			collider->Delete();
	}

	void JustAppeared() override
	{
		DLPPhases phase = (DLPPhases)instance->GetData(DATA_SCENARIO_PHASE);
		if (phase > DLPPhases::TheEscape)
			return;

		mageGUID.Clear();

		me->AddAura(SPELL_ARCANE_BARRIER, me);

        if (Creature* stalker = GetClosestCreatureWithEntry(me, NPC_INVISIBLE_STALKER, 10.f))
        {
            triggerGUID = stalker->GetGUID();
        }

		if (GameObject* collider = me->SummonGameObject(GOB_COLLIDER, me->GetPosition(), QuaternionData::fromEulerAnglesZYX(me->GetOrientation(), 0.f, 0.f), 0s))
		{
			colliderGUID = collider->GetGUID();

			collider->SetFlag(GO_FLAG_NOT_SELECTABLE);
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

		// Pour la barrière de Surdiel et de Brasael l'event est selon la phase du scénario
        if (Unit* owner = me->GetOwner())
        {
            #ifdef CUSTOM_DEBUG
            {
                if (owner->GetEntry() == NPC_MAGISTER_BRASAEL || owner->GetEntry() == NPC_MAGISTER_SURDIEL)
                {
                    mageGUID = owner->GetGUID();

                    if (Creature* creature = owner->ToCreature())
                        creature->AI()->DoAction(ACTION_DISPELL_BARRIER);

                    Dispell(player);
                }
            }
            #else
            {
                DLPPhases phase = (DLPPhases)instance->GetData(DATA_SCENARIO_PHASE);
                if (phase == DLPPhases::KillMagisters
                    && (owner->GetEntry() == NPC_MAGISTER_BRASAEL || owner->GetEntry() == NPC_MAGISTER_SURDIEL))
                {
                    mageGUID = owner->GetGUID();

                    if (Creature* creature = owner->ToCreature())
                        creature->AI()->DoAction(ACTION_DISPELL_BARRIER);

                    Dispell(player);
                }
            }
            #endif
		}
		else
		{
            if (triggerGUID.IsEmpty())
                return;

			if (Creature* stalker = ObjectAccessor::GetCreature(*me, triggerGUID))
			{
				if (Creature* mage = me->SummonCreature(NPC_ARCHMAGE_LANDALOCK, stalker->GetPosition()))
				{
					mageGUID = mage->GetGUID();

					// Set guids
					mage->RemoveAllAuras();
					mage->AI()->SetGUID(player->GetGUID(), GUID_PLAYER);
					mage->AI()->SetGUID(stalker->GetGUID(), GUID_STALKER);
					mage->AI()->DoAction(ACTION_DISPELL_BARRIER);

                    Dispell(nullptr);
				}
			}
		}
	}

	void Reset() override
	{
		mageGUID.Clear();
	}

	void UpdateAI(uint32 diff) override
	{
		scheduler.Update(diff);
	}

	void Dispell(Player* player)
	{
		if (GameObject* collider = ObjectAccessor::GetGameObject(*me, colliderGUID))
			collider->Delete();

        if (player)
        {
            DoCast(SPELL_FREED_EXPLOSION);
            player->CastSpell(player, SPELL_ARCANE_BARRIER_DAMAGE);
        }

		me->setActive(false);
		me->RemoveAllAuras();
		me->DisappearAndDie();
	}
};

struct npc_book_of_arcane_monstrosities : public CustomAI
{
	npc_book_of_arcane_monstrosities(Creature* creature) : CustomAI(creature, AI_Type::Distance)
	{

	}

	enum Misc
	{
		// Spells
		SPELL_CLEANSING_FORCE       = 196115,
		SPELL_CLEANSING_FORCE_AT    = 196088
	};

	void Initialize() override
	{
		scheduler.SetValidator([this]
		{
			return !me->HasUnitState(UNIT_STATE_CASTING);
		});
	}

	void Reset() override
	{
		CustomAI::Reset();

		scheduler
			.Schedule(1ms, [this](TaskContext cleansing_force)
			{
				DoCast(me, SPELL_CLEANSING_FORCE, CastSpellExtraArgs(TRIGGERED_CAST_DIRECTLY));
                DoCast(me, SPELL_CLEANSING_FORCE_AT, true);
			});
	}

	void OnSpellCast(SpellInfo const* spellInfo) override
	{
		if (spellInfo->Id == SPELL_CLEANSING_FORCE)
			me->DespawnOrUnsummon(Milliseconds(spellInfo->GetDuration()));
	}

	void JustDied(Unit* killer) override
	{
		CustomAI::JustDied(killer);

		me->DespawnOrUnsummon();
	}

	bool CanAIAttack(Unit const* who) const override
	{
		return who->IsAlive() && who->GetTypeId() == TYPEID_PLAYER;
	}
};

struct npc_living_ledgers : public CustomAI
{
    npc_living_ledgers(Creature* creature) : CustomAI(creature, AI_Type::Distance)
	{
		instance = creature->GetInstanceScript();
		me->SetCombatReach(50.f);
	}

	enum Spells
	{
		SPELL_ARCANE_RAIN = 388895
	};

	InstanceScript* instance;

	void Initialize() override
	{
		scheduler.SetValidator([this]
		{
			return !me->HasUnitState(UNIT_STATE_CASTING);
		});
	}

	void AttackStart(Unit* who) override
	{
		if (!who)
			return;

		if (me->Attack(who, false))
			SetCombatMovement(false);
	}

	void JustDied(Unit* killer) override
	{
        if (!instance)
            return;

		CustomAI::JustDied(killer);

		if (Creature* savor = instance->GetCreature(DATA_HIGH_ARCANIST_SAVOR))
			savor->AI()->DoAction(ACTION_ARCANE_ORB_DESPAWN);

		me->DespawnOrUnsummon();
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		scheduler.Schedule(1ms, [this](TaskContext arcane_barrage)
		{
            for (auto* ref : me->GetThreatManager().GetUnsortedThreatList())
            {
                if (Unit* target = ref->GetVictim())
                {
                    DoCast(target, SPELL_ARCANE_RAIN);
                    arcane_barrage.Repeat(3s);
                }
            }
		});
	}

	bool CanAIAttack(Unit const* who) const override
	{
		return who->IsAlive() && who->GetTypeId() == TYPEID_PLAYER;
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
		me->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
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
		SPELL_FIRE_BOMB_DAMAGE          = 270958,
        SPELL_FIRE_BOMB_STUN            = 400296,
	};

	void OnUnitEnter(Unit* unit) override
	{
		if (Unit* caster = at->GetCaster())
		{
            if (unit->GetGUID() == caster->GetGUID())
                return;

            if (unit->IsFriendlyTo(caster))
                return;

			caster->CastSpell(unit, SPELL_FIRE_BOMB_STUN, CastSpellExtraArgs(TRIGGERED_CAST_DIRECTLY | TRIGGERED_FULL_MASK));
			at->Remove();
		}
	}
};

// Rain of fire - 418554
// Rain of fire damages - 407123
// AreaTriggerID - 29665
struct at_rain_of_fire : AreaTriggerAI
{
	at_rain_of_fire(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger)
	{
	}

	enum Spells
	{
		SPELL_RAIN_OF_FIRE_DAMAGE       = 407123
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

// Frigid Blizzard - 396400
// Frigid Blizzard damages - 386743
// AreaTriggerID - 27361
struct at_frigid_blizzard_dalaran : AreaTriggerAI
{
    at_frigid_blizzard_dalaran(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger)
	{
	}

	enum Spells
	{
		SPELL_FRIGID_BLIZZARD_DAMAGE = 386743
	};

	void OnUnitEnter(Unit* unit) override
	{
		if (Unit* caster = at->GetCaster())
		{
			if (unit->GetGUID() == caster->GetGUID())
				return;

			if (unit->IsFriendlyTo(caster))
				return;

			caster->CastSpell(unit, SPELL_FRIGID_BLIZZARD_DAMAGE, true);
		}
	}

	void OnUnitExit(Unit* unit)
	{
		if (unit->HasAura(SPELL_FRIGID_BLIZZARD_DAMAGE))
		{
			unit->RemoveAurasDueToSpell(SPELL_FRIGID_BLIZZARD_DAMAGE);
		}
	}
};

// 5234 - Cleansing Force
struct at_cleansing_force : AreaTriggerAI
{
    at_cleansing_force(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger)
    {
    }

    void OnUnitEnter(Unit* unit) override
    {
        if (!unit->IsPlayer())
            return;

        unit->ApplyMovementForce(at->GetGUID(), at->GetPosition(), 5.0f, MovementForceType::Gravity);
    }

    void OnUnitExit(Unit* unit) override
    {
        if (!unit->IsPlayer())
            return;

        unit->RemoveMovementForce(at->GetGUID());
    }
};

// GameObjects
struct go_portal_savor : public GameObjectAI
{
	go_portal_savor(GameObject* gameobject) : GameObjectAI(gameobject)
	{
		instance = gameobject->GetInstanceScript();
	}

	InstanceScript* instance;

	const Position toSavor      = { -903.22f, 4470.40f, 659.74f, 5.58f };
	const Position fromSavor    = { -673.41f, 4377.76f, 748.58f, 2.65f };

	bool OnReportUse(Player* player) override
	{
		DLPPhases phase = (DLPPhases)instance->GetData(DATA_SCENARIO_PHASE);
		if (phase == DLPPhases::TheEscape)
		{
			player->CastSpell(fromSavor, SPELL_TELEPORT);
		}
		else
		{
			player->CastSpell(toSavor, SPELL_TELEPORT);
		}

		return false;
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

	void HandleSummon(SpellEffIndex /*effIndex*/)
	{
        Unit* caster = GetCaster();
        for (SpellLogEffectGenericVictimParams const& summonedObject : GetSpell()->GetExecuteLogEffectTargets(SPELL_EFFECT_SUMMON, &SpellLogEffect::GenericVictimTargets))
        {
            if (Creature* summon = ObjectAccessor::GetCreature(*caster, summonedObject.Victim))
            {
                summon->SetMaxHealth(caster->CountPctFromMaxHealth(5));
                summon->SetFullHealth();

                UnitState cannotMove = UnitState(UNIT_STATE_ROOT | UNIT_STATE_CANNOT_TURN);
                summon->SetControlled(true, cannotMove);
            }
        }
	}

	void Register() override
	{
		OnEffectHit += SpellEffectFn(spell_purge_glacial_spike_summon::HandleSummon, EFFECT_0, SPELL_EFFECT_SUMMON);
	}
};

// Speed: Slow - 207011
// Speed: Normal - 207012
// Speed: Fast - 207013
class spell_speed : public AuraScript
{
	PrepareAuraScript(spell_speed);

	void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
	{
		if (Unit* target = GetTarget())
		{
			uint32 spellId = GetSpellInfo()->Id;
			if (target->HasAura(spellId))
			{
				target->RemoveAurasDueToSpell(spellId, ObjectGuid::Empty, 0, AURA_REMOVE_BY_EXPIRE);

				float rate = 1.0f;
				target->SetSpeedRate(MOVE_RUN, rate);
				target->SetSpeedRate(MOVE_RUN_BACK, rate);
				target->SetSpeedRate(MOVE_WALK, rate);
				target->SetModCastingSpeed(rate);
				target->SetModSpellHaste(rate);
				target->SetModHaste(rate);
				target->SetModRangedHaste(rate);
				target->SetModHasteRegen(rate);
				target->SetModTimeRate(rate);
			}
		}
	}

	void OnApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
	{
		if (Unit* target = GetTarget())
		{
			uint32 spellId = GetSpellInfo()->Id;
			target->CastSpell(target, spellId, true);

			int32 basePoints = aurEff->GetSpellEffectInfo().BasePoints;

			float speedRate = basePoints != 1.0f
				? 1.0f * (basePoints / 100.0f) + 1.0f
				: 1.0f;

			target->SetSpeedRate(MOVE_RUN, speedRate);
			target->SetSpeedRate(MOVE_RUN_BACK, speedRate);
			target->SetSpeedRate(MOVE_WALK, speedRate);

			float castRate = basePoints != 1.0f
				? 1.0f * (-basePoints / 100.0f) + 1.0f
				: 1.0f;

			target->SetModCastingSpeed(castRate);
			target->SetModSpellHaste(castRate);
			target->SetModHaste(castRate);
			target->SetModRangedHaste(castRate);
			target->SetModHasteRegen(castRate);
			target->SetModTimeRate(castRate);
		}
	}

	void Register() override
	{
		AfterEffectApply += AuraEffectApplyFn(spell_speed::OnApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
		AfterEffectRemove += AuraEffectRemoveFn(spell_speed::OnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
	}
};

// Living Ledgers - 224745
class spell_living_ledgers : public SpellScript
{
	static constexpr uint32 HEALTH_PERCENT = 10;

	PrepareSpellScript(spell_living_ledgers);

    enum NPCs
    {
        NPC_LIVING_LEDGERS = 112906
    };

	void HandleDummy(SpellEffIndex effIndex)
	{
		PreventHitDefaultEffect(effIndex);

        Unit* caster = GetCaster();
        if (!caster)
            return;

        uint32 creatureId = NPC_LIVING_LEDGERS;
        if (sObjectMgr->GetCreatureTemplate(creatureId))
        {
            if (Creature* summon = caster->SummonCreature(creatureId, -852.50885f, 4456.1465f, 653.60596f, 6.630659f))
            {
                PhasingHandler::InheritPhaseShift(summon, caster);

                summon->SetReactState(REACT_PASSIVE);
                summon->SetFaction(caster->GetFaction());
                //summon->SetFacingTo(pos.GetOrientation());
                summon->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);

                uint32 maxHealth = caster->CountPctFromMaxHealth(HEALTH_PERCENT);
                summon->SetMaxHealth(maxHealth);
                summon->SetFullHealth();

                if (caster->GetVictim())
                    summon->Attack(caster->EnsureVictim(), false);
            }
        }
	}

	void Register() override
	{
		OnEffectHit += SpellEffectFn(spell_living_ledgers::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
	}
};

// Big Band - 222761
class spell_big_bang : public SpellScript
{
	PrepareSpellScript(spell_big_bang);

	void FilterTargets(std::list<WorldObject*>& targets)
	{
		Unit* caster = GetCaster();
		if (caster && caster->GetEntry() != NPC_HIGH_ARCANIST_SAVOR)
			return;

		targets.remove_if([](WorldObject* target)
		{
			return target->GetTypeId() != TYPEID_PLAYER;
		});
	}

	void Register() override
	{
		OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_big_bang::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
	}
};

// Atonement - 292010
class spell_atonement_stormwind_cleric : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_ATONEMENT_HEAL }) && ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    static bool CheckProc(ProcEventInfo const& eventInfo)
    {
        return eventInfo.GetDamageInfo() != nullptr;
    }

    void HandleOnProc(AuraEffect const* aurEff, ProcEventInfo const& eventInfo)
    {
        TriggerAtonementHealOnTargets(aurEff, eventInfo);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_atonement_stormwind_cleric::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_atonement_stormwind_cleric::HandleOnProc, EFFECT_0, SPELL_AURA_DUMMY);
    }

    std::vector<ObjectGuid> _appliedAtonements;

public:
    void AddAtonementTarget(ObjectGuid const& target)
    {
        _appliedAtonements.push_back(target);
    }

    void RemoveAtonementTarget(ObjectGuid const& target)
    {
        std::erase(_appliedAtonements, target);
    }

    std::vector<ObjectGuid> const& GetAtonementTargets() const
    {
        return _appliedAtonements;
    }

    struct TriggerArgs
    {
        SpellInfo const* TriggeredBy = nullptr;
        SpellSchoolMask DamageSchoolMask = SPELL_SCHOOL_MASK_NONE;
    };

    void TriggerAtonementHealOnTargets(AuraEffect const* atonementEffect, ProcEventInfo const& eventInfo)
    {
        Unit* priest = GetUnitOwner();
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        CastSpellExtraArgs args(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);

        // Note: atonementEffect holds the correct amount since we passed the effect in the AuraScript that calls this method.
        args.AddSpellMod(SPELLVALUE_BASE_POINT0, CalculatePct(damageInfo->GetDamage(), atonementEffect->GetAmount()));

        args.SetCustomArg(TriggerArgs{
            .TriggeredBy = eventInfo.GetSpellInfo(),
            .DamageSchoolMask = eventInfo.GetDamageInfo()->GetSchoolMask() });

        float distanceLimit = GetEffectInfo(EFFECT_1).CalcValue();

        std::erase_if(_appliedAtonements, [priest, distanceLimit, &args](ObjectGuid const& targetGuid)
        {
            if (Unit* target = ObjectAccessor::GetUnit(*priest, targetGuid))
            {
                if (target->IsInDist2d(priest, distanceLimit))
                    priest->CastSpell(target, SPELL_PRIEST_ATONEMENT_HEAL, args);

                return false;
            }

            return true;
        });
    }
};

// Renew - 294342
// Flash Heal - 314655
// Power Word: Shield - 318158
// Power Word: Radiance - 218589
class spell_atonement_effect_stormwind_cleric : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo
        ({
            SPELL_ATONEMENT,
            SPELL_ATONEMENT_EFFECT,
            SPELL_POWER_WORD_RADIANCE,
            SPELL_POWER_WORD_SHIELD
        })
            && ValidateSpellEffect({ { SPELL_POWER_WORD_RADIANCE, EFFECT_3 } });
    }

    bool Load() override
    {
        Unit* caster = GetCaster();
        if (!caster->HasAura(SPELL_ATONEMENT))
            return false;

        return true;
    }

    void HandleOnHitTarget() const
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();

        CastSpellExtraArgs args(TRIGGERED_FULL_MASK);
        args.SetTriggeringSpell(GetSpell());

        // Power Word: Radiance applies Atonement at 100 % (without modifiers) of its total duration.
        if (GetSpellInfo()->Id == SPELL_POWER_WORD_RADIANCE)
            args.AddSpellMod(SPELLVALUE_DURATION_PCT, GetSpellInfo()->GetEffect(EFFECT_3).CalcValue(caster));

        caster->CastSpell(target, SPELL_ATONEMENT_EFFECT, args);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_atonement_effect_stormwind_cleric::HandleOnHitTarget);
    }
};

// Atonement (buff) - 292011
class spell_atonement_aura_stormwind_cleric : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ATONEMENT });
    }

    void HandleOnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        RegisterHelper<&spell_atonement_stormwind_cleric::AddAtonementTarget>();
    }

    void HandleOnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        RegisterHelper<&spell_atonement_stormwind_cleric::RemoveAtonementTarget>();
    }

    template<void(spell_atonement_stormwind_cleric::* func)(ObjectGuid const&)>
    void RegisterHelper()
    {
        if (Unit* caster = GetCaster())
            if (Aura* atonement = caster->GetAura(SPELL_ATONEMENT))
                if (spell_atonement_stormwind_cleric* script = atonement->GetScript<spell_atonement_stormwind_cleric>())
                    (script->*func)(GetTarget()->GetGUID());
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_atonement_aura_stormwind_cleric::HandleOnApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(spell_atonement_aura_stormwind_cleric::HandleOnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// Delphuric Beam - 423771
class spell_delphuric_beam : public SpellScript
{
    PrepareSpellScript(spell_delphuric_beam);

    enum Misc
    {
        SPELL_DELPHURIC_BEAM_DAMAGE = 423634
    };

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        for (auto const& itr : caster->GetThreatManager().GetUnsortedThreatList())
        {
            if (Unit* victim = itr->GetVictim())
            {
                if (Creature* trigger = caster->SummonCreature(NPC_INVISIBLE_STALKER, caster->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, 4s))
                {
                    trigger->SetFaction(caster->GetFaction());
                    trigger->SetTarget(victim->GetGUID());
                    trigger->SetControlled(true, UNIT_STATE_ROOT);
                    trigger->CastSpell(trigger, SPELL_DELPHURIC_BEAM_DAMAGE);
                }
            }
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_delphuric_beam::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

void AddSC_npcs_dalaran_purge()
{
	// Neutral
	RegisterDalaranAI(npc_assassin_dalaran);
	RegisterDalaranAI(npc_guardian_mage_dalaran);

	// Alliance
	RegisterDalaranAI(npc_vereesa_windrunner_dalaran);
	RegisterDalaranAI(npc_jaina_dalaran_patrol);
	RegisterDalaranAI(npc_stormwind_cleric);
	RegisterDalaranAI(npc_archmage_landalock);
	RegisterDalaranAI(npc_mage_commander_zuros);
	RegisterDalaranAI(npc_narasi_snowdawn);
	RegisterDalaranAI(npc_arcanist_rathaella);
	RegisterDalaranAI(npc_sorin_magehand);

	// Horde
	RegisterDalaranAI(npc_sunreaver_citizen);
	RegisterDalaranAI(npc_sunreaver_pyromancer);
	RegisterDalaranAI(npc_sunreaver_aegis);
	RegisterDalaranAI(npc_sunreaver_captain);
	RegisterDalaranAI(npc_sunreaver_summoner);
	RegisterDalaranAI(npc_magister_brasael);
	RegisterDalaranAI(npc_magister_surdiel);
	RegisterDalaranAI(npc_high_arcanist_savor);
	RegisterDalaranAI(npc_magister_hathorel);

	// Special
	RegisterCreatureAI(npc_arcane_barrier);
	RegisterCreatureAI(npc_glacial_spike);
	RegisterCreatureAI(npc_living_ledgers);
	RegisterCreatureAI(npc_book_of_arcane_monstrosities);

	// Area Triggers
	RegisterAreaTriggerAI(at_arcane_barrier);
	RegisterAreaTriggerAI(at_arcane_protection);
	RegisterAreaTriggerAI(at_fire_bomb);
	RegisterAreaTriggerAI(at_rain_of_fire);
	RegisterAreaTriggerAI(at_frigid_blizzard_dalaran);
	RegisterAreaTriggerAI(at_cleansing_force);

	// Spells
	RegisterSpellScript(spell_purge_teleport);
	RegisterSpellScript(spell_purge_glacial_spike);
	RegisterSpellScript(spell_purge_glacial_spike_summon);
	RegisterSpellScript(spell_speed);
	RegisterSpellScript(spell_living_ledgers);
	RegisterSpellScript(spell_big_bang);
    RegisterSpellScript(spell_atonement_stormwind_cleric);
    RegisterSpellScript(spell_atonement_effect_stormwind_cleric);
    RegisterSpellScript(spell_atonement_aura_stormwind_cleric);
    RegisterSpellScript(spell_delphuric_beam);

	// Gameobjects
	RegisterGameObjectAI(go_portal_savor);
}
