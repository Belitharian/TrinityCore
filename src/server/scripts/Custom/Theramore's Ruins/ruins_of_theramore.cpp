#include "InstanceScript.h"
#include "TemporarySummon.h"
#include "MotionMaster.h"
#include "Custom/AI/CustomAI.h"
#include "ruins_of_theramore.h"

struct npc_jaina_ruins : public CustomAI
{
	npc_jaina_ruins(Creature* creature) : CustomAI(creature),
		hasBlinked(false), hasEscaped(false), distance(10.f)
	{
        instance = me->GetInstanceScript();
    }

	enum Misc
	{
		// NPCs
		NPC_MIRROR_IMAGE        = 500020,

		// Display Ids
		DISPLAYID_INVISIBLE     = 41199,
		DISPLAYID_JAINA         = 80015,
	};

	enum Spells
	{
		SPELL_ETERNAL_SILENCE   = 42201,
        SPELL_EVOCATION         = 243070,
		SPELL_FROSTBOLT         = 284703,
		SPELL_RING_OF_ICE       = 285459,
        SPELL_GRASP_OF_FROST    = 287626,
		SPELL_ICEBOUND_ESCAPE   = 289077,
		SPELL_IMMUNE            = 299144,
		SPELL_COMET_BARRAGE     = 354938,
		SPELL_FRIGID_SHARD      = 354933,
		SPELL_BLINK             = 357601,
        SPELL_ARCANE_SURGE      = 365350,
		SPELL_FROZEN_SHIELD     = 396780,
	};

	enum Groups
	{
		GROUP_COMBAT,
		GROUP_ESCAPE,
	};

	InstanceScript* instance;
	Position beforeBlink;
	GuidVector images;
	bool hasBlinked;
	bool hasEscaped;
	float distance;

	void SetData(uint32 /*id*/, uint32 value) override
	{
		distance = (float)value;
	}

	void SpellHit(WorldObject* /*caster*/, SpellInfo const* /*spellInfo*/) override { }

	void AttackStart(Unit* who) override
	{
		if (!who)
			return;

		if (me->Attack(who, false))
		{
			me->SetCanMelee(false);
			me->SetSheath(SHEATH_STATE_UNARMED);
			SetCombatMovement(false);
		}
	}

	void Reset() override
	{
		Initialize();

		summons.DespawnAll();

		images.clear();

		scheduler.CancelGroup(GROUP_COMBAT);

		hasBlinked = false;
		hasEscaped = false;

        me->SetUninteractible(false);
        me->RemoveUnitFlag2(UNIT_FLAG2_UNTARGETABLE_BY_CLIENT);
        me->SetDisplayId(DISPLAYID_JAINA);
        me->RemoveAurasDueToSpell(SPELL_ETERNAL_SILENCE);
        me->RemoveAurasDueToSpell(SPELL_IMMUNE);
	}

	void EnterEvadeMode(EvadeReason why) override
	{
		CustomAI::EnterEvadeMode(why);

		summons.DespawnAll();
		summons.clear();

		scheduler.CancelGroup(GROUP_COMBAT);

		me->SetDisplayId(DISPLAYID_JAINA);
		me->RemoveUnitFlag2(UNIT_FLAG2_UNTARGETABLE_BY_CLIENT);
	}

	void JustSummoned(Creature* summon) override
	{
		if (summon->GetEntry() != NPC_MIRROR_IMAGE)
			return;

		summons.Summon(summon);
	}

	void SummonedCreatureDespawn(Creature* /*summon*/) override { }

	void SummonedCreatureDies(Creature* summon, Unit* /*killer*/) override
	{
		if (summon->GetEntry() != NPC_MIRROR_IMAGE)
			return;

		summon->DespawnOrUnsummon();
		summons.Despawn(summon);

		if (summons.empty())
		{
			me->ResumeMovement();
            me->LoadEquipment(2);
            me->SetUninteractible(false);
            me->RemoveUnitFlag2(UNIT_FLAG2_UNTARGETABLE_BY_CLIENT);
			me->SetDisplayId(DISPLAYID_JAINA);
			me->RemoveAurasDueToSpell(SPELL_ETERNAL_SILENCE);
			me->RemoveAurasDueToSpell(SPELL_IMMUNE);

			CastStop();
			DoCastSelf(SPELL_FROZEN_SHIELD);

			if (Unit* target = me->GetVictim())
				JustEngagedWith(target);
		}
	}

	void OnSpellCast(SpellInfo const* spell) override
	{
		switch (spell->Id)
		{
			case SPELL_SUMMON_WATER_ELEMENTALS:
			{
				for (uint8 i = 0; i < ELEMENTALS_SIZE; ++i)
				{
					if (Creature* elemental = me->GetMap()->SummonCreature(NPC_WATER_ELEMENTAL, ElementalsPoint[i].spawn))
						elemental->CastSpell(elemental, SPELL_WATER_BOSS_ENTRANCE);
				}
				break;
			}
			case SPELL_RING_OF_ICE:
			{
                if (hasEscaped)
                {
                    hasBlinked = false;
                    break;
                }
                if (!hasBlinked)
                {
                    break;
                }
				scheduler.Schedule(1s, [this](TaskContext /*blink*/)
				{
					CastStop(SPELL_ICEBOUND_ESCAPE);
					me->CastSpell(beforeBlink, SPELL_BLINK, true);
					hasBlinked = false;
				});
				break;
			}
			case SPELL_ICEBOUND_ESCAPE:
			{
				me->PauseMovement();
                me->LoadEquipment(0, true);
                me->SetUninteractible(true);
				me->SetUnitFlag2(UNIT_FLAG2_UNTARGETABLE_BY_CLIENT);
				me->SetDisplayId(DISPLAYID_INVISIBLE);
				break;
			}
		}
	}

	void DamageTaken(Unit* /*attacker*/, uint32& /*damage*/, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
		if (ShouldTakeDamage())
			return;

		if ((!me->HasAura(SPELL_IMMUNE) || !me->HasAura(SPELL_FROZEN_SHIELD)))
		{
			if (hasEscaped)
				return;

			SummonImages();

			scheduler.CancelGroup(GROUP_COMBAT);

            me->AddAura(SPELL_ETERNAL_SILENCE, me);
            me->AddAura(SPELL_IMMUNE, me);

			CastStop();
			DoCast(SPELL_ICEBOUND_ESCAPE);

			hasEscaped = true;
		}
	}

	void JustEngagedWith(Unit* who) override
	{
        #ifndef CUSTOM_DEBUG
        RFTPhases phase = (RFTPhases)instance->GetData(DATA_SCENARIO_PHASE);
        if (phase != RFTPhases::Standards_Valided)
            return;
        #endif

		DoCast(who, SPELL_FROSTBOLT);

		scheduler
			.Schedule(2s, GROUP_COMBAT, [this](TaskContext frostbolt)
			{
				DoCastVictim(SPELL_FROSTBOLT);
				frostbolt.Repeat(2800ms);
			})
			.Schedule(8s, GROUP_COMBAT, [this](TaskContext frigid_shard)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
					DoCast(target, SPELL_FRIGID_SHARD);
				frigid_shard.Repeat(14s, 18s);
			})
            .Schedule(24s, GROUP_COMBAT, [this](TaskContext grasp_of_frost)
            {
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                {
                    CastStop({ SPELL_ICEBOUND_ESCAPE, SPELL_FRIGID_SHARD });
                    DoCast(target, SPELL_GRASP_OF_FROST);
                }
                grasp_of_frost.Repeat(18s, 32s);
            })
            .Schedule(12s, GROUP_COMBAT, [this](TaskContext arcane_surge)
            {
                CastStop(SPELL_ICEBOUND_ESCAPE);

                me->GetSpellHistory()->ResetCooldown(SPELL_ARCANE_SURGE);
                me->GetSpellHistory()->ResetCooldown(SPELL_EVOCATION);

                if (me->GetPowerPct(POWER_MANA) <= 20.0f)
                {
                    DoCastSelf(SPELL_EVOCATION);
                    arcane_surge.Repeat(6s);
                }
                else
                {
                    CastSpellExtraArgs args(SPELLVALUE_BASE_POINT0, 85000);
                    DoCastVictim(SPELL_ARCANE_SURGE, args);
                    arcane_surge.Repeat(45s, 1min);
                }
            })
			.Schedule(14s, GROUP_COMBAT, [this](TaskContext comet_barrage)
			{
				DoCastAOE(SPELL_COMET_BARRAGE);
				comet_barrage.Repeat(12s, 14s);
			})
			.Schedule(50s, GROUP_COMBAT, [this](TaskContext blink)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
				{
					if (me->IsWithinDist(target, 10.f))
					{
                        CastStop(SPELL_ICEBOUND_ESCAPE);
						DoCastAOE(SPELL_RING_OF_ICE);
					}
					else
					{
						beforeBlink = me->GetPosition();

                        Position dest = me->GetRandomPoint(target->GetPosition(), 6.0f);

                        CastStop(SPELL_ICEBOUND_ESCAPE);
						me->CastSpell(dest, SPELL_BLINK, true);

						scheduler.Schedule(1s, [this](TaskContext /*ring_of_ice*/)
						{
							me->CastStop();
							DoCastAOE(SPELL_RING_OF_ICE);
						});

						hasBlinked = true;
					}
				}

				blink.Repeat(1min);
			});
	}

	bool CanAIAttack(Unit const* /*who*/) const override
	{
		return true;
	}

	void MovementInform(uint32 /*type*/, uint32 id) override
	{
		switch (id)
		{
			case MOVEMENT_INFO_POINT_01:
				me->SetStandState(UNIT_STAND_STATE_KNEEL);
				scheduler.Schedule(2s, [this](TaskContext context)
				{
					switch (context.GetRepeatCounter())
					{
						case 0:
							me->SetWalk(false);
							me->SetStandState(UNIT_STAND_STATE_STAND);
							if (Creature* warlord = me->FindNearestCreature(NPC_ROKNAH_WARLORD, 10.f))
								me->SetWalk(true);
							context.Repeat(2s);
							break;
						case 1:
							if (Creature* warlord = instance->GetCreature(DATA_ROKNAH_WARLORD))
								me->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_02, warlord, 1.3f);
							break;
					}
				});
				break;
			case MOVEMENT_INFO_POINT_02:
				me->HandleEmoteCommand(EMOTE_ONESHOT_CASTSTRONG);
				scheduler.Schedule(1s, [this](TaskContext /*context*/)
				{
					if (Creature* warlord = instance->GetCreature(DATA_ROKNAH_WARLORD))
					{
						warlord->KillSelf();

						instance->TriggerGameEvent(EVENT_WARLORD_ROKNAH_SLAIN);
					}
				});
				break;
			case MOVEMENT_INFO_POINT_03:
				instance->SetData(EVENT_BACK_TO_SENDER, 0U);
				break;
		}
	}

	void MoveInLineOfSight(Unit* who) override
	{
		ScriptedAI::MoveInLineOfSight(who);

		if (me->IsEngaged())
			return;

		if (who->GetTypeId() != TYPEID_PLAYER)
			return;

		if (Player* player = who->ToPlayer())
		{
			if (player->IsGameMaster())
				return;

			if (player->IsFriendlyTo(me) && player->IsWithinDist(me, distance))
			{
				RFTPhases phase = (RFTPhases)instance->GetData(DATA_SCENARIO_PHASE);
				switch (phase)
				{
					case RFTPhases::FindJaina_Isle:
						instance->TriggerGameEvent(EVENT_FIND_JAINA_01);
						break;
					case RFTPhases::FindJaina_Crater:
						instance->SetData(EVENT_FIND_JAINA_02, 0U);
						break;
					default:
						break;
				}
			}
		}
	}

	void UpdateAI(uint32 diff) override
	{
        scheduler.Update(diff, [this]
        {
            UpdateVictim();
        });
	}

	void SummonImages()
	{
		for (uint8 i = 0; i < 3; i++)
		{
			const Position pos = me->GetRandomPoint(me->GetPosition(), 8.f);
			if (Creature* image = me->SummonCreature(NPC_MIRROR_IMAGE, pos, TEMPSUMMON_DEAD_DESPAWN, 1s))
			{
				image->SetFaction(me->GetFaction());
				for (ThreatReference const* ref : me->GetThreatManager().GetUnsortedThreatList())
				{
					if (Unit* victim = ref->GetVictim())
					{
						image->AI()->AttackStart(victim);
						image->SetFacingToObject(victim);
						victim->GetThreatManager().AddThreat(image, INFINITY);
						image->GetThreatManager().AddThreat(victim, INFINITY);
					}
				}
			}
		}
	}
};

void AddSC_ruins_of_theramore()
{
	RegisterRuinsAI(npc_jaina_ruins);
}
