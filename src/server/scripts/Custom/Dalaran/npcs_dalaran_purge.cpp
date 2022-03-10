#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "DB2Stores.h"
#include "InstanceScript.h"
#include "MotionMaster.h"
#include "Object.h"
#include "PassiveAI.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"
#include "Custom/AI/CustomAI.h"
#include "dalaran_purge.h"

class npc_jaina_dalaran_patrol : public CreatureScript
{
	public:
	npc_jaina_dalaran_patrol() : CreatureScript("npc_jaina_dalaran_patrol")
	{
	}

	struct npc_jaina_dalaran_patrolAI : public CustomAI
	{
		npc_jaina_dalaran_patrolAI(Creature* creature) : CustomAI(creature)
		{
			Initialize();
		}

		enum Spells
		{
			SPELL_BLIZZARD          = 284968,
			SPELL_FROSTBOLT         = 284703,
			SPELL_FRIGID_SHARD      = 354933,
			SPELL_TELEPORT          = 135176,
			SPELL_GLACIAL_SPIKE     = 338488
		};

		void Initialize()
		{
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
						me->CastStop(SPELL_GLACIAL_SPIKE);
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

	CreatureAI* GetAI(Creature* creature) const override
	{
		return GetDalaranAI<npc_jaina_dalaran_patrolAI>(creature);
	}
};

class npc_sunreaver_citizen : public CreatureScript
{
	public:
	npc_sunreaver_citizen() : CreatureScript("npc_sunreaver_citizen")
	{
	}

	struct npc_sunreaver_citizenAI : public CustomAI
	{
		npc_sunreaver_citizenAI(Creature* creature) : CustomAI(creature)
		{
			Initialize();
		}

		enum Spells
		{
			SPELL_SCORCH            = 17195,
			SPELL_FIREBALL          = 358226,
		};

        InstanceScript* instance;

		void Initialize()
		{
			instance = me->GetInstanceScript();

			me->SetEmoteState(EMOTE_STATE_COWER);

			Creature* jaina = instance->GetCreature(DATA_JAINA_PROUDMOORE_PATROL);
			if (roll_chance_i(20) && jaina)
				me->GetMotionMaster()->MoveFleeing(jaina);
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

	CreatureAI* GetAI(Creature* creature) const override
	{
		return GetDalaranAI<npc_sunreaver_citizenAI>(creature);
	}
};

class npc_sunreaver_mage : public CreatureScript
{
    public:
    npc_sunreaver_mage() : CreatureScript("npc_sunreaver_mage")
    {
    }

    struct npc_sunreaver_mageAI : public CustomAI
    {
        npc_sunreaver_mageAI(Creature* creature) : CustomAI(creature)
        {
            Initialize();
        }

        enum Spells
        {
            SPELL_MOLTEN_ARMOR      = 79849,
            SPELL_FIREBALL          = 79854,
            SPELL_FLAMESTRIKE       = 330347,
            SPELL_RINF_OF_FIRE      = 353082
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

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetDalaranAI<npc_sunreaver_mageAI>(creature);
    }
};

class npc_sunreaver_aegis : public CreatureScript
{
    public:
    npc_sunreaver_aegis() : CreatureScript("npc_sunreaver_aegis")
    {
    }

    struct npc_sunreaver_aegisAI : public CustomAI
    {
        npc_sunreaver_aegisAI(Creature* creature) : CustomAI(creature, AI_Type::Melee), healthLow(false)
        {
            Initialize();
        }

        enum Spells
        {
            SPELL_DIVINE_SHIELD         = 642,
            SPELL_HEAL                  = 225638,
            SPELL_AVENGING_WRATH        = 292266,
            SPELL_HOLY_LIGHT            = 324471,
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
                printf("%s procs effect\n", spellInfo->SpellName->Str[LOCALE_frFR]);

                scheduler
                    .Schedule(1s, [this](TaskContext /*context*/)
                    {
                        me->CastStop();
                        DoCastSelf(SPELL_BLESSING_OF_FREEDOM);
                    });
            }
        }

        void DamageTaken(Unit* /*attacker*/, uint32& /*damage*/) override
        {
            if (!healthLow && HealthBelowPct(30))
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
					holy_light.Repeat(8s);
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

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetDalaranAI<npc_sunreaver_aegisAI>(creature);
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

// Arcane Barrier - 264849
// AreaTriggerID - 12833
struct at_arcane_barrier : AreaTriggerAI
{
    at_arcane_barrier(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger)
    {
    }

    enum Spells
    {
        SPELL_ARCANE_BARRIER_DAMAGE = 264848
    };

    void OnUnitEnter(Unit* unit) override
    {
        if (Unit* caster = at->GetCaster())
        {
            if (caster->GetGUID() == unit->GetGUID())
                return;

            if (unit->IsHostileTo(caster))
                unit->CastSpell(unit, SPELL_ARCANE_BARRIER_DAMAGE);
        }
    }
};

// Glacial Spike - 338488
class spell_purge_glacial_spike : public SpellScript
{
	PrepareSpellScript(spell_purge_glacial_spike);

	enum Misc
	{
		SPELL_GLACIAL_WRATH = 346525
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

class npc_glacial_spike : public CreatureScript
{
	public:
	npc_glacial_spike() : CreatureScript("npc_glacial_spike")
	{
	}

	enum Misc
	{
		SPELL_GLACIAL_SPIKE_COSMETIC    = 346559,
		SPELL_FROZEN_DESTRUCTION        = 356957,
	};

	struct npc_glacial_spikeAI : public NullCreatureAI
	{
		npc_glacial_spikeAI(Creature* creature) : NullCreatureAI(creature)
		{
		}

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

	CreatureAI* GetAI(Creature* creature) const override
	{
		return GetDalaranAI<npc_glacial_spikeAI>(creature);
	}
};

void AddSC_npcs_dalaran_purge()
{
	new npc_jaina_dalaran_patrol();

	new npc_sunreaver_citizen();
	new npc_sunreaver_mage();
	new npc_sunreaver_aegis();

    new npc_glacial_spike();

    RegisterAreaTriggerAI(at_arcane_barrier);

	RegisterSpellScript(spell_purge_teleport);
	RegisterSpellScript(spell_purge_glacial_spike);
	RegisterSpellScript(spell_purge_glacial_spike_summon);
}
