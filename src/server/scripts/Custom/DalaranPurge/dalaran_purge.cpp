#include "Custom/CustomAI/CustomAI.h"
#include "GameObject.h"
#include "Group.h"
#include "GroupMgr.h"
#include "InstanceScript.h"
#include "TemporarySummon.h"
#include "ScriptMgr.h"
#include "dalaran_purge.h"

struct npc_jaina_dalaran_purge : public CustomAI
{
	npc_jaina_dalaran_purge(Creature* creature) : CustomAI(creature, AI_Type::Distance)
	{
		Initialize();
	}

	void Initialize()
	{
		instance = me->GetInstanceScript();
		me->SetSheath(SHEATH_STATE_UNARMED);
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

	void MovementInform(uint32 /*type*/, uint32 id) override
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
		}
	}
};

struct npc_magister_rommath_purge : public CustomAI
{
	npc_magister_rommath_purge(Creature* creature) : CustomAI(creature), eventId(0), evocating(false)
	{
		Initialize();
	}

	enum Groups
	{
		GROUP_COMBAT                = 1,
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

	void Initialize()
	{
		instance = me->GetInstanceScript();
	}

	InstanceScript* instance;
	EventMap events;
	uint32 eventId;
	bool evocating;
	GuidVector tracks;

	void Reset() override
	{
		scheduler.CancelGroup(GROUP_COMBAT);

		events.Reset();

		if (Player* player = me->GetCharmerOrOwnerPlayerOrPlayerItself())
		{
			events.ScheduleEvent(1, 5s);
		}

		Initialize();
	}

	void WaypointPathEnded(uint32 /*pointId*/, uint32 pathId) override
	{
		if (pathId == PATH_ROMMATH_01)
		{
			me->AI()->Talk(SAY_INFILTRATE_ROMMATH_04);

			if (GameObject* passage = instance->GetGameObject(DATA_SECRET_PASSAGE))
				passage->UseDoorOrButton(7200000);

			//for (uint8 i = 0; i < TRACKING_PATH_01; i++)
			//{
			//    if (Creature* tracking = map->SummonCreature(NPC_INVISIBLE_STALKER, TrackingPath01[i]))
			//    {
			//        tracking->AddAura(SPELL_ARCANIC_TRACKING, tracking);
			//        tracks.push_back(tracking->GetGUID());
			//    }
			//}

			FollowPlayer();
		}
	}

	void FollowPlayer()
	{
		Map* map = me->GetMap();
		if (!map)
			return;

		auto playerIt = map->GetPlayers().begin();
		if (playerIt != map->GetPlayers().end() && playerIt->GetSource())
		{
			Player* player = playerIt->GetSource();
			player->SetMinionGUID(me->GetGUID());

			me->SetOwnerGUID(player->GetGUID());
			me->SetImmuneToAll(false);
			me->GetMotionMaster()->Clear();
			me->GetMotionMaster()->MoveFollow(player, PET_FOLLOW_DIST, me->GetFollowAngle());
		}
	}

	void MovementInform(uint32 /*type*/, uint32 id) override
	{
		switch (id)
		{
			case MOVEMENT_INFO_POINT_02:
				if (GameObject* portal = instance->GetGameObject(DATA_PORTAL_TO_PRISON))
					portal->RemoveFlag(GO_FLAG_IN_USE | GO_FLAG_NOT_SELECTABLE | GO_FLAG_LOCKED);
				me->HandleEmoteCommand(EMOTE_ONESHOT_POINT);
				break;
			case MOVEMENT_INFO_POINT_03:
				for (ObjectGuid guid : tracks)
				{
					if (Creature* tracking = ObjectAccessor::GetCreature(*me, guid))
						tracking->DespawnOrUnsummon();
				}
				DoCast(SPELL_TELEPORT_VISUAL_ONLY);
				me->SetVisible(false);
				scheduler.Schedule(5s, [this](TaskContext /*context*/)
				{
					instance->TriggerGameEvent(EVENT_FREE_AETHAS_SUNREAVER);
				});
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

			if (player->IsFriendlyTo(me) && player->IsWithinDist(me, 5.f))
			{
				DLPPhases phase = (DLPPhases)instance->GetData(DATA_SCENARIO_PHASE);
				switch (phase)
				{
					case DLPPhases::TheEscape_Events:
						me->RemoveAurasDueToSpell(SPELL_COSMETIC_YELLOW_ARROW);
						instance->TriggerGameEvent(EVENT_FIND_ROMMATH_01);
						break;
					default:
						break;
				}
			}
		}
	}

	void SpellHitTarget(WorldObject* target, SpellInfo const* spellInfo) override
	{
		if (Unit* victim = target->ToUnit())
		{
			if (spellInfo->HasOnlyDamageEffects() && roll_chance_i(60))
			{
				DoCast(victim, SPELL_SCORCHING_DETONATION, true);
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

			scheduler.DelayGroup(GROUP_COMBAT, 10s);

			// On lance Evocation
			DoCast(me, SPELL_EVOCATION,
				   CastSpellExtraArgs(TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD)
				   .AddSpellBP0(30)
				   .AddSpellMod(SPELLVALUE_BASE_POINT1, 30));
		}
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
			.Schedule(8s, 12s, GROUP_COMBAT, [this](TaskContext dragon_breath)
			{
				if (EnemiesInFront(6.f) >= 2)
				{
					CastStop(SPELL_EVOCATION);
					DoCast(SPELL_DRAGON_BREATH);
					dragon_breath.Repeat(32s);
				}
				else
					dragon_breath.Repeat();
			})
			.Schedule(20s, GROUP_COMBAT, [this](TaskContext blazing_surge)
			{
				if (EnemiesInFront(15.f) >= 2)
				{
					CastStop({ SPELL_EMBER_BLAST, SPELL_EVOCATION });
					DoCast(SPELL_BLAZING_SURGE);
					blazing_surge.Repeat(1min);
				}
				else
					blazing_surge.Repeat();
			})
			.Schedule(8s, GROUP_COMBAT, [this](TaskContext ember_blast)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
				{
					CastStop({ SPELL_BLAZING_SURGE, SPELL_EVOCATION });
					DoCast(target, SPELL_EMBER_BLAST);
				}
				ember_blast.Repeat(15s, 40s);
			})
			.Schedule(3s, GROUP_COMBAT, [this](TaskContext phoenix_flames)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				{
					CastStop({ SPELL_BLAZING_SURGE, SPELL_EMBER_BLAST, SPELL_EVOCATION });
					DoCast(target, SPELL_PHOENIX_FLAMES);
				}
				phoenix_flames.Repeat(3s, 8s);
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
					meteor.Repeat(15s);
			});
};

	bool CanAIAttack(Unit const* who) const override
	{
		return who->IsAlive() && me->IsValidAttackTarget(who)
			&& ScriptedAI::CanAIAttack(who)
			&& who->GetEntry() != NPC_NARASI_SNOWDAWN
			&& who->GetEntry() != NPC_JAINA_PROUDMOORE_PATROL
			&& who->GetEntry() != NPC_VEREESA_WINDRUNNER;
	}
};

enum Spells
{
	SPELL_METEOR_STORM_VISUAL   = 215555
};

class MeteorStormEvent : public BasicEvent
{
public:
	MeteorStormEvent(Unit* caster, ObjectGuid originalCastId, Position const& dest) : _caster(caster), _originalCastId(originalCastId), _dest(dest), _count(0) { }

	bool Execute(uint64 time, uint32 /*diff*/) override
	{
		Position destPosition = { _dest.GetPositionX() + frand(-8.0f, 8.0f), _dest.GetPositionY() + frand(-8.0f, 8.0f), _dest.GetPositionZ() };
		_caster->CastSpell(destPosition, SPELL_METEOR_STORM_VISUAL,
			CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS).SetOriginalCastId(_originalCastId));
		++_count;

		if (_count >= 12)
			return true;

		_caster->m_Events.AddEvent(this, Milliseconds(time) + randtime(100ms, 275ms));
		return false;
	}

private:
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
