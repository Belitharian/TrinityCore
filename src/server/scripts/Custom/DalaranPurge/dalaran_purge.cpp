#include "Custom/CustomAI/CustomAI.h"
#include "Custom/FakeParty/FakeParty.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "TemporarySummon.h"
#include "ScriptMgr.h"
#include "dalaran_purge.h"

struct npc_jaina_dalaran_purge : public CustomAI
{
	npc_jaina_dalaran_purge(Creature* creature) : CustomAI(creature)
	{
        instance = me->GetInstanceScript();

        SetCanRandomMovement(false);
    }

	enum Misc
	{
		// Gossip
		GOSSIP_MENU_DEFAULT         = 65004,
	};

	InstanceScript* instance;

	bool OnGossipHello(Player* player) override
	{
		DLPPhases phase = (DLPPhases)instance->GetData(DATA_SCENARIO_PHASE);
		if (phase != DLPPhases::TheEscape)
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
			case 0:
				me->RemoveAurasDueToSpell(SPELL_CHAT_BUBBLE);
				me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
				instance->TriggerGameEvent(EVENT_SPEAK_TO_JAINA);
				break;
		}

		CloseGossipMenuFor(player);
		return true;
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

			if (player->IsFriendlyTo(me) && player->IsWithinDist(me, 5.f))
			{
				DLPPhases phase = (DLPPhases)instance->GetData(DATA_SCENARIO_PHASE);
				switch (phase)
				{
					case DLPPhases::FindJaina01:
						instance->TriggerGameEvent(EVENT_FIND_JAINA_01);
						break;
					case DLPPhases::FindJaina02:
						instance->SetData(EVENT_FIND_JAINA_02, 1U);
						break;
					default:
						break;
				}
			}
		}
	}
};

struct npc_aethas_sunreaver_purge : public CustomAI
{
	npc_aethas_sunreaver_purge(Creature* creature) : CustomAI(creature)
	{
		Initialize();
	}

	void Initialize()
	{
		instance = me->GetInstanceScript();
	}

	InstanceScript* instance;

	void MovementInform(uint32 type, uint32 id) override
	{
		switch (id)
		{
			case MOVEMENT_INFO_POINT_03:
				DoCast(SPELL_TELEPORT_VISUAL_ONLY);
				me->SetVisible(false);
				break;
			default:
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

			if (player->IsFriendlyTo(me) && player->IsWithinDist(me, 15.f))
			{
				DLPPhases phase = (DLPPhases)instance->GetData(DATA_SCENARIO_PHASE);
				switch (phase)
				{
					case DLPPhases::TheEscape_Escort:
						instance->SetData(EVENT_FREE_AETHAS_SUNREAVER, 0U);
						break;
					default:
						break;
				}
			}
		}
	}

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* spellInfo) override
	{
		if (spellInfo->Id == SPELL_ARCANE_BOMBARDMENT || spellInfo->Id == SPELL_FROSTBOLT)
		{
			damage = 0;
		}
	}

	void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
	{
		if (spellInfo->Id == SPELL_FROSTBOLT)
		{
			DoCastSelf(SPELL_ICY_GLARE);
			DoCastSelf(SPELL_CHILLING_BLAST, true);
			DoCastSelf(SPELL_FROZEN_SLAM, true);
		}
	}
};

struct npc_magister_rommath_purge : public CustomAI
{
    npc_magister_rommath_purge(Creature* creature) : CustomAI(creature),
        m_instance(creature->GetInstanceScript()),
        m_evocating(false)
    {
        SetCanRandomMovement(false);
    }

    enum Groups
    {
        GROUP_COMBAT = 1,
    };

    enum Spells
    {
        SPELL_FIRE_CHANNELING       = 45461,
        SPELL_FIREBALL              = 79854,
        SPELL_COMBUSTION            = 190319,
        SPELL_EVOCATION             = 211765,
        SPELL_PHOENIX_FLAMES        = 257541,
        SPELL_METEOR_STORM          = 179215,
        SPELL_DRAGON_BREATH         = 255890,
        SPELL_BLAZING_BARRIER       = 295238,
        SPELL_EMBER_BLAST           = 325877,
        SPELL_BLAZING_SURGE         = 329509,
        SPELL_SCORCHING_DETONATION  = 401525,
    };

private:
    InstanceScript* m_instance;
    bool m_evocating;
    ObjectGuid m_playerGuid;

    // Raccourci avec garde null
    Player* GetFollowedPlayer() const
    {
        if (m_playerGuid.IsEmpty())
            return nullptr;

        return ObjectAccessor::GetPlayer(*me, m_playerGuid);
    }

public:
    void Reset() override
    {
        scheduler.CancelGroup(GROUP_COMBAT);
        m_evocating = false;
        m_playerGuid.Clear();
    }

    void WaypointPathEnded(uint32 /*pointId*/, uint32 pathId) override
    {
        if (pathId != PATH_ROMMATH_01)
            return;

        Talk(SAY_INFILTRATE_ROMMATH_04);

        if (m_instance)
        {
            if (GameObject* passage = m_instance->GetGameObject(DATA_SECRET_PASSAGE))
                passage->UseDoorOrButton(7200000);
        }

        FollowPlayer();
    }

    void FollowPlayer()
    {
        Player* player = GetFollowedPlayer();
        if (!player)
            return;

        // Fake party seulement si le joueur est solo
        if (!player->GetGroup())
            StartFakeParty(player);

        player->SetMinionGUID(me->GetGUID());

        me->SetOwnerGUID(m_playerGuid);
        me->SetImmuneToAll(false);
        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MoveFollow(player, PET_FOLLOW_DIST, me->GetFollowAngle());
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        CustomAI::MovementInform(type, id);

        switch (id)
        {
            case MOVEMENT_INFO_POINT_02:
            {
                if (m_instance)
                {
                    if (GameObject* portal = m_instance->GetGameObject(DATA_PORTAL_TO_PRISON))
                        portal->RemoveFlag(GO_FLAG_IN_USE | GO_FLAG_NOT_SELECTABLE | GO_FLAG_LOCKED);
                }
                me->HandleEmoteCommand(EMOTE_ONESHOT_POINT);
                break;
            }
            case MOVEMENT_INFO_POINT_03:
            {
                StopFakeParty();
                DoCast(SPELL_TELEPORT_VISUAL_ONLY);
                me->SetVisible(false);

                if (m_instance)
                {
                    // Capture le GUID de l'instance pour éviter un dangling this
                    ObjectGuid instanceCreatureGuid = me->GetGUID();
                    scheduler.Schedule(5s, [this](TaskContext /*context*/)
                    {
                        if (m_instance)
                            m_instance->TriggerGameEvent(EVENT_FREE_AETHAS_SUNREAVER);
                    });
                }
                break;
            }
            default:
                break;
        }
    }

    void MoveInLineOfSight(Unit* who) override
    {
        ScriptedAI::MoveInLineOfSight(who);

        if (me->IsEngaged())
            return;

        Player* player = who->ToPlayer();
        if (!player || player->IsGameMaster())
            return;

        if (!player->IsFriendlyTo(me) || !player->IsWithinDist(me, 5.f))
            return;

        m_playerGuid = player->GetGUID();

        if (!m_instance)
            return;

        DLPPhases const phase = static_cast<DLPPhases>(m_instance->GetData(DATA_SCENARIO_PHASE));
        if (phase == DLPPhases::TheEscape_Events)
        {
            me->RemoveAurasDueToSpell(SPELL_COSMETIC_YELLOW_ARROW);
            m_instance->TriggerGameEvent(EVENT_FIND_ROMMATH_01);
        }
    }

    void SpellHitTarget(WorldObject* target, SpellInfo const* spellInfo) override
    {
        Unit* victim = target->ToUnit();
        if (!victim || !me->IsValidAttackTarget(victim))
            return;

        if (spellInfo->HasOnlyDamageEffects() && roll_chance_i(60))
            DoCast(victim, SPELL_SCORCHING_DETONATION, true);
    }

    void OnChannelFinished(SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_EVOCATION)
            m_evocating = false;
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (!me->HealthBelowPctDamaged(10, damage))
            return;

        // Toujours annuler les dégâts sous 10%, que l'evocation soit en cours ou non
        damage = 0;

        if (m_evocating)
            return;

        m_evocating = true;

        CastStop();

        scheduler.DelayGroup(GROUP_COMBAT, 10s);

        DoCast(me, SPELL_EVOCATION,
            CastSpellExtraArgs(TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD)
                .AddSpellBP0(30)
                .AddSpellMod(SPELLVALUE_BASE_POINT1, 30));
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        DoCast(SPELL_BLAZING_BARRIER);
        DoCast(SPELL_COMBUSTION);

        scheduler
            .Schedule(1s, GROUP_COMBAT, [this](TaskContext fireball)
            {
                DoCastVictim(SPELL_FIREBALL);
                fireball.Repeat(1800ms);
            })
            .Schedule(30s, GROUP_COMBAT, [this](TaskContext combustion)
            {
                DoCast(SPELL_COMBUSTION);
                combustion.Repeat(30s, 45s);
            })
            .Schedule(8s, 12s, GROUP_COMBAT, [this](TaskContext dragonBreath)
            {
                if (EnemiesInFront(6.f) >= 2)
                {
                    CastStop(SPELL_EVOCATION);
                    DoCast(SPELL_DRAGON_BREATH);
                    dragonBreath.Repeat(32s);
                }
                else
                {
                    dragonBreath.Repeat();
                }
            })
            .Schedule(20s, GROUP_COMBAT, [this](TaskContext blazingSurge)
            {
                if (EnemiesInFront(15.f) >= 2)
                {
                    CastStop({ SPELL_EMBER_BLAST, SPELL_EVOCATION });
                    DoCast(SPELL_BLAZING_SURGE);
                    blazingSurge.Repeat(1min);
                }
                else
                {
                    blazingSurge.Repeat();
                }
            })
            .Schedule(8s, GROUP_COMBAT, [this](TaskContext emberBlast)
            {
                if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
                {
                    CastStop({ SPELL_BLAZING_SURGE, SPELL_EVOCATION });
                    DoCast(target, SPELL_EMBER_BLAST);
                }
                emberBlast.Repeat(15s, 40s);
            })
            .Schedule(3s, GROUP_COMBAT, [this](TaskContext phoenixFlames)
            {
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                {
                    CastStop({ SPELL_BLAZING_SURGE, SPELL_EMBER_BLAST, SPELL_EVOCATION });
                    DoCast(target, SPELL_PHOENIX_FLAMES);
                }
                phoenixFlames.Repeat(3s, 8s);
            })
            .Schedule(15s, 25s, GROUP_COMBAT, [this](TaskContext meteor)
            {
                if (Unit* target = SelectTarget(SelectTargetMethod::MinThreat, 0))
                {
                    CastStop(SPELL_EVOCATION);
                    DoCast(target, SPELL_METEOR_STORM);
                    meteor.Repeat(15s, 18s);
                }
                else
                {
                    meteor.Repeat(15s);
                }
            });
    }

    bool CanAIAttack(Unit const* who) const override
    {
        if (!who->IsAlive() || !me->IsValidAttackTarget(who))
            return false;

        if (!ScriptedAI::CanAIAttack(who))
            return false;

        uint32 const entry = who->GetEntry();
        return entry != NPC_NARASI_SNOWDAWN
            && entry != NPC_JAINA_PROUDMOORE_PATROL
            && entry != NPC_VEREESA_WINDRUNNER;
    }
};

enum Spells
{
    SPELL_METEOR_STORM_VISUAL = 215555
};

class MeteorStormEvent : public BasicEvent
{
public:
	MeteorStormEvent(Unit* caster, ObjectGuid originalCastId, Position const& dest)
        : _caster(caster),
        _originalCastId(originalCastId),
        _dest(dest), _count(0)
    {
    }

	bool Execute(uint64 time, uint32 /*diff*/) override
	{
		float angle = frand(0.0f, 2.0f * float(M_PI));
		float radius = METEROS_RANGE * std::sqrt(frand(0.0f, 1.0f));

		Position destPosition = {
            _dest.GetPositionX() + radius * std::cos(angle),
            _dest.GetPositionY() + radius * std::sin(angle),
            _dest.GetPositionZ()
        };

		_caster->CastSpell(destPosition, SPELL_METEOR_STORM_VISUAL,
			CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS).SetOriginalCastId(_originalCastId));

		++_count;

		if (_count >= METEROS_COUNT)
			return true;

		_caster->m_Events.AddEvent(this, Milliseconds(time) + randtime(100ms, 275ms));
		return false;
	}

private:

    const int METEROS_COUNT = 12;
    const float METEROS_RANGE = 8;

	Unit* _caster;
	ObjectGuid _originalCastId;
	Position _dest;
	uint8 _count;
};

// 179215 - Meteor Storm (launch)
class spell_meteor_storm : public SpellScript
{
	bool Validate(SpellInfo const* /*spellInfo*/) override
	{
		return ValidateSpellInfo({ SPELL_METEOR_STORM_VISUAL });
	}

	void EffectHit(SpellEffIndex /*effIndex*/)
	{
		GetCaster()->m_Events.AddEventAtOffset(new MeteorStormEvent(GetCaster(), GetSpell()->m_castId, *GetHitDest()), randtime(100ms, 275ms));
	}

	void Register() override
	{
		OnEffectHit += SpellEffectFn(spell_meteor_storm::EffectHit, EFFECT_0, SPELL_EFFECT_DUMMY);
	}
};

void AddSC_dalaran_purge()
{
	RegisterDalaranAI(npc_jaina_dalaran_purge);
	RegisterDalaranAI(npc_aethas_sunreaver_purge);
	RegisterDalaranAI(npc_magister_rommath_purge);

	RegisterSpellScript(spell_meteor_storm);
}
