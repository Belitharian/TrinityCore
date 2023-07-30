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
		Initialize();
	}

    enum NPCs
    {
        NPC_MIRROR_IMAGE = 500020
    };

	enum Spells
	{

        SPELL_ETERNAL_SILENCE   = 42201,
        SPELL_DISSOLVE          = 237075,
		SPELL_FROSTBOLT         = 284703,
		SPELL_RING_OF_ICE       = 285459,
        SPELL_ICEBOUND_ESCAPE   = 290878,
		SPELL_IMMUNE            = 299144,
		SPELL_COMET_BARRAGE     = 354938,
		SPELL_FRIGID_SHARD      = 354933,
		SPELL_BLINK             = 357601,
        SPELL_FROZEN_SHIELD     = 396780
	};

    enum Groups
    {
        GROUP_COMBAT,
        GROUP_ESCAPE,
    };

	void Initialize() override
	{
        CustomAI::Initialize();

		instance = me->GetInstanceScript();
	}

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

	void AttackStart(Unit* who) override
	{
		if (!who)
			return;

		if (me->Attack(who, false))
			SetCombatMovement(false);
	}

	void Reset() override
	{
		Initialize();

        summons.DespawnAll();

        images.clear();

		hasBlinked = false;
        hasEscaped = false;
	}

	void EnterEvadeMode(EvadeReason why) override
	{
		CustomAI::EnterEvadeMode(why);

        summons.DespawnAll();

        summons.clear();

        me->RemoveAurasDueToSpell(SPELL_DISSOLVE);
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

        summons.Despawn(summon);

        if (summons.empty())
        {
            me->RemoveUnitFlag2(UNIT_FLAG2_UNTARGETABLE_BY_CLIENT);

            me->ResumeMovement();
            me->RemoveAurasDueToSpell(SPELL_DISSOLVE);
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
                if (!hasBlinked)
                    break;
                scheduler.Schedule(1s, [this](TaskContext /*blink*/)
                {
                    me->CastStop(SPELL_ICEBOUND_ESCAPE);
                    me->CastSpell(beforeBlink, SPELL_BLINK, true);
                    hasBlinked = false;
                });
                break;
            }
            case SPELL_ICEBOUND_ESCAPE:
            {
                me->SetUnitFlag2(UNIT_FLAG2_UNTARGETABLE_BY_CLIENT);
                me->PauseMovement();
                me->AddAura(SPELL_DISSOLVE, me);
                me->AddAura(SPELL_ETERNAL_SILENCE, me);
                me->AddAura(SPELL_IMMUNE, me);
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

            float angle = 0.0f;
            for (uint8 i = 0; i < 3; i++)
            {
                const Position pos = GetRandomPositionAroundCircle(me, angle, 6.f);
                if (Creature* image = me->SummonCreature(NPC_MIRROR_IMAGE, pos, TEMPSUMMON_DEAD_DESPAWN, 1s))
                {
                    image->SetFaction(me->GetFaction());
                    for (ThreatReference const* ref : me->GetThreatManager().GetUnsortedThreatList())
                    {
                        if (Unit* victim = ref->GetVictim())
                        {
                            image->AI()->AttackStart(victim);
                            victim->GetThreatManager().AddThreat(image, INFINITY);
                            image->GetThreatManager().AddThreat(victim, INFINITY);
                        }
                    }
                }

                angle += 120.0f;
            }

            scheduler.CancelGroup(GROUP_COMBAT);

            CastStop();
            DoCast(SPELL_ICEBOUND_ESCAPE);

            hasEscaped = true;
        }
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		scheduler
			.Schedule(5ms, GROUP_COMBAT, [this](TaskContext frostbolt)
			{
				DoCastVictim(SPELL_FROSTBOLT);
				frostbolt.Repeat(3s);
			})
			.Schedule(2s, GROUP_COMBAT, [this](TaskContext frigid_shard)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_FRIGID_SHARD);
				frigid_shard.Repeat(5s, 8s);
			})
			.Schedule(8s, GROUP_COMBAT, [this](TaskContext comet_barrage)
			{
				DoCast(SPELL_COMET_BARRAGE);
				comet_barrage.Repeat(12s, 14s);
			})
			.Schedule(14s, GROUP_COMBAT, [this](TaskContext blink)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
				{
					if (me->IsWithinDist(target, 10.f))
					{
						CastStop(SPELL_ICEBOUND_ESCAPE);
						DoCast(SPELL_RING_OF_ICE);
					}
					else
					{
						beforeBlink = me->GetPosition();

						Position dest = GetRandomPosition(target->GetPosition(), 8.f);

                        float x = dest.GetPositionX();
                        float y = dest.GetPositionY();

                        Trinity::NormalizeMapCoord(x);
                        Trinity::NormalizeMapCoord(y);
                        me->UpdateGroundPositionZ(x, y, dest.m_positionZ);

                        CastStop(SPELL_ICEBOUND_ESCAPE);
						me->CastSpell(dest, SPELL_BLINK, true);

						scheduler.Schedule(1s, [this](TaskContext /*ring_of_ice*/)
						{
							me->CastStop();
							DoCast(SPELL_RING_OF_ICE);
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
};

void AddSC_ruins_of_theramore()
{
    RegisterRuinsAI(npc_jaina_ruins);
}
