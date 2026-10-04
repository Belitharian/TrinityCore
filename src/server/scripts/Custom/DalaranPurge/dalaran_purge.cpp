#include "ConditionMgr.h"
#include "Conversation.h"
#include "ConversationAI.h"
#include "Custom/CustomAI/CustomAI.h"
#include "Custom/FakeParty/FakeParty.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include <mutex>
#include "Player.h"
#include "SceneMgr.h"
#include "TemporarySummon.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
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
		{
			// Fin du scenario : seulement la quete, le menu 65004 relancerait le flashback.
			if (!me->IsQuestGiver())
				return false;

			player->PrepareQuestMenu(me->GetGUID());
			player->SendPreparedQuest(me);
			return true;
		}

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

			if (player->IsFriendlyTo(me) && player->IsWithinDist(me, 10.f))
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
        SPELL_EMBER_BLAST_AURA      = 325873,
        SPELL_BLAZING_SURGE         = 329509,
        SPELL_SCORCHING_DETONATION  = 401525,
    };

private:
    // Au-dela de cette distance, Rommath rejoint le joueur par teleportation
    // (joueur ressuscite au cimetiere, ou Rommath reste en arriere apres un evade).
    static constexpr float REJOIN_DISTANCE = 40.f;
    // Frequence de la surveillance d'escorte.
    static constexpr Milliseconds ESCORT_CHECK_INTERVAL = 1s;

    InstanceScript* m_instance;
    bool m_evocating;
    ObjectGuid m_playerGuid;

    // Etat d'escorte, conserve a travers Reset() : la mort du joueur fait
    // evader Rommath, il ne doit pas pour autant oublier qui il escorte.
    bool m_following = false;      // Rommath suit le joueur (MoveFollow)
    bool m_partyWanted = false;    // Fausse partie a maintenir (joueur solo)
    Milliseconds m_escortCheckTimer = ESCORT_CHECK_INTERVAL;

    // Raccourci avec garde null
    Player* GetFollowedPlayer() const
    {
        if (m_playerGuid.IsEmpty())
            return nullptr;

        return ObjectAccessor::GetPlayer(*me, m_playerGuid);
    }

public:
    // m_playerGuid et l'etat d'escorte ne sont PAS effaces : Reset() est appele
    // a chaque evade, notamment quand le joueur escorte meurt.
    void Reset() override
    {
        scheduler.CancelGroup(GROUP_COMBAT);
        m_evocating = false;
    }

    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        if (id == GUID_PLAYER)
            m_playerGuid = guid;
    }

    // Pilotage de l'escorte par le scenario (prison, portail, Narasi).
    void DoAction(int32 action) override
    {
        switch (action)
        {
            case ACTION_ROMMATH_FOLLOW:
                FollowPlayer();
                break;
            case ACTION_ROMMATH_STOP_FOLLOW:
                // Le scenario reprend la main sur le deplacement ; la fausse
                // partie, elle, reste active jusqu'au portail de fin.
                m_following = false;
                me->SetOwnerGUID(ObjectGuid::Empty);
                break;
            default:
                break;
        }
    }

    void UpdateAI(uint32 diff) override
    {
        CustomAI::UpdateAI(diff);

        m_escortCheckTimer -= Milliseconds(diff);
        if (m_escortCheckTimer > 0ms)
            return;

        m_escortCheckTimer = ESCORT_CHECK_INTERVAL;
        UpdateEscort();
    }

    // Surveillance d'escorte. Tant que le joueur est mort, Rommath l'attend ;
    // une fois ressuscite, il le rejoint par teleportation s'il est trop loin,
    // reprend son suivi et recree la fausse partie dissoute a la mort.
    void UpdateEscort()
    {
        if (!m_following && !m_partyWanted)
            return;

        Player* player = GetFollowedPlayer();
        if (!player || !player->IsInWorld() || !player->IsAlive())
            return;

        if (m_following && !me->IsEngaged())
        {
            if (!me->IsWithinDistInMap(player, REJOIN_DISTANCE))
            {
                me->NearTeleportTo(player->GetRandomNearPosition(3.f));
                DoCastSelf(SPELL_TELEPORT_VISUAL_ONLY, true);
            }

            if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
            {
                FollowPlayer();
                return; // FollowPlayer recree aussi la fausse partie
            }
        }

        if (m_partyWanted && !fakeParty.IsActive() && !player->GetGroup())
        {
            StartFakeParty(player);
            player->SetMinionGUID(me->GetGUID());
        }
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

        m_following = true;

        // Fake party seulement si le joueur est solo
        m_partyWanted = !player->GetGroup();
        if (m_partyWanted)
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
                    if (GameObject* passage = m_instance->GetGameObject(DATA_SECRET_PASSAGE))
                        passage->UseDoorOrButton();

                    if (GameObject* portal = m_instance->GetGameObject(DATA_PORTAL_TO_PRISON))
                    {
                        portal->RemoveFlag(GO_FLAG_IN_USE | GO_FLAG_NOT_SELECTABLE | GO_FLAG_LOCKED);
                        portal->SetVignette(VIGNETTE_PORTAL);
                    }
                }
                me->HandleEmoteCommand(EMOTE_ONESHOT_POINT);
                break;
            }
            case MOVEMENT_INFO_POINT_03:
            {
                // Fin de l'escorte : plus rien a maintenir.
                m_following = false;
                m_partyWanted = false;
                StopFakeParty();
                DoCast(SPELL_TELEPORT_VISUAL_ONLY);
                me->SetVisible(false);

                if (m_instance)
                {
                    // Capture le GUID de l'instance pour �viter un dangling this
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
            m_instance->TriggerGameEvent(EVENT_FIND_ROMMATH_01);
    }

    void SpellHitTarget(WorldObject* target, SpellInfo const* spellInfo) override
    {
        Unit* victim = target->ToUnit();
        if (!victim || !me->IsValidAttackTarget(victim))
            return;

        if (spellInfo->HasOnlyDamageEffects() && roll_chance(60))
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

        // Toujours annuler les d�g�ts sous 10%, que l'evocation soit en cours ou non
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
                    target->AddAura(SPELL_EMBER_BLAST_AURA, target);
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

/*
 * Conversation 50000 - La purge de Dalaran.
 */
class conversation_dalaran_purge : public ConversationAI
{
	public:
	explicit conversation_dalaran_purge(Conversation* conversation) : ConversationAI(conversation) { }

	enum PurgeConversation
	{
		LINE_JAINA_01       = 19,   // Aethas Saccage-Soleil !
		LINE_JAINA_02       = 24,   // Vous avez trahi le Kirin Tor...
		LINE_AETHAS_03      = 26,   // Vous n'y etes pas du tout, Jaina...
		LINE_JAINA_04       = 25,   // Vous avez ferme les yeux...
		LINE_AETHAS_05      = 28,   // C'est aussi NOTRE ville, Portvaillant.
		LINE_JAINA_06       = 29,   // Je vois. Je vais expulser les Saccage-Soleil...
		LINE_JAINA_07       = 30,   // Vous, Aethas, vous venez avec moi.

		ACTOR_IDX_JAINA     = 0,
		ACTOR_IDX_AETHAS    = 1,
	};

	void OnStart() override
	{
        LocaleConstant privateOwnerLocale = conversation->GetPrivateObjectOwnerLocale();

		// Les deux protagonistes se font face des la premiere replique.
		conversation->m_Events.AddEvent([conversation = conversation]()
		{
			Creature* jaina = conversation->GetActorCreature(ACTOR_IDX_JAINA);
			Creature* aethas = conversation->GetActorCreature(ACTOR_IDX_AETHAS);
			if (!jaina || !aethas)
				return;

			jaina->SetFacingToObject(aethas);
			aethas->SetFacingToObject(jaina);

		}, 0ms);

		if (Milliseconds const* frostbolt = conversation->GetLineStartTime(privateOwnerLocale, LINE_JAINA_07))
		{
			conversation->m_Events.AddEvent([conversation = conversation]()
			{
				InstanceScript* instance = conversation->GetInstanceScript();
				if (!instance)
					return;

				Creature* elemental = instance->GetCreature(DATA_SUMMONED_WATER_ELEMENTAL);
				Creature* aethas = conversation->GetActorCreature(ACTOR_IDX_AETHAS);
				if (elemental && aethas)
					elemental->CastSpell(aethas, SPELL_FROSTBOLT);

			}, *frostbolt);
		}
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
		Position destPosition = GetRandomPosition(_caster, _dest, METEROS_RANGE);

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

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        targets.remove_if([caster](WorldObject* target)
        {
            Unit* unit = target->ToUnit();
            return !unit || !caster->IsValidAttackTarget(unit);
        });
    }

	void EffectHit(SpellEffIndex /*effIndex*/)
	{
		GetCaster()->m_Events.AddEventAtOffset(new MeteorStormEvent(GetCaster(), GetSpell()->m_castId, *GetHitDest()), randtime(100ms, 275ms));
	}

	void Register() override
	{
		OnEffectHit += SpellEffectFn(spell_meteor_storm::EffectHit, EFFECT_0, SPELL_EFFECT_DUMMY);
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_meteor_storm::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
    }
};

// Sorts de zone des allies de l'infiltration (Rommath, Surdiel) :
//   401525 - Scorching Detonation, 215555 - Meteor Storm (impacts),
//   255890 - Dragon's Breath, 329509 - Blazing Surge
//
// Ces sorts PNJ ciblent tout ce qui se trouve dans la zone, sans tenir compte
// de la faction. Sous l'illusion de la Horde, le joueur combattait a cote de
// Rommath et encaissait ses explosions ; le premier coup le mettait en plus
// en combat avec lui. On ne garde que ce que le lanceur peut attaquer.
//
// Les types de cibles varient d'un sort a l'autre : le filtre s'accroche a
// chaque cible de zone reellement presente dans les effets du sort, ce qui
// evite d'avoir a les connaitre (et un echec de validation au demarrage).
class spell_purge_hostile_area_only : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        targets.remove_if([caster](WorldObject* target)
        {
            Unit* unit = target->ToUnit();
            return unit && !caster->IsValidAttackTarget(unit);
        });
    }

    void Register() override
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(m_scriptSpellId, DIFFICULTY_NONE);
        if (!spellInfo)
            return;

        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
        {
            for (SpellImplicitTargetInfo const* target : { &effect.TargetA, &effect.TargetB })
            {
                if (!target->GetTarget() || !target->IsArea())
                    continue;

                // TargetA et TargetB identiques : un seul filtre suffit
                if (target == &effect.TargetB && effect.TargetB.GetTarget() == effect.TargetA.GetTarget())
                    continue;

                OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_purge_hostile_area_only::FilterTargets,
                    effect.EffectIndex, target->GetTarget());
            }
        }
    }
};

// 195838 - Horde (illusion : faction Horde + reactions forcees envers la Horde et l'Alliance)
//
// Un PNJ dont la faction a une reputation (Rommath : Silvermoon City, Surdiel)
// juge un joueur sur sa reputation, avant la faction : pour un personnage de
// l'Alliance, Deteste. Le joueur voyait ses allies en amis, mais eux le
// voyaient en ennemi et leurs sorts de zone le touchaient. Pendant l'illusion,
// seule la faction compte (UNIT_FLAG2_IGNORE_REPUTATION).
class spell_purge_horde_illusion_reactions : public AuraScript
{
    void AfterApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->SetUnitFlag2(UNIT_FLAG2_IGNORE_REPUTATION);
    }

    void AfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveUnitFlag2(UNIT_FLAG2_IGNORE_REPUTATION);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_purge_horde_illusion_reactions::AfterApply, EFFECT_0, SPELL_AURA_MOD_FACTION, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_purge_horde_illusion_reactions::AfterRemove, EFFECT_0, SPELL_AURA_MOD_FACTION, AURA_EFFECT_HANDLE_REAL);
    }
};

// 500000 - Ce qui devait etre fait (copie de 32423, rendue a Varian au donjon de Hurlevent)
//
// Varian (68690) n'est visible qu'avec la quete, puis apres la scene tant que le joueur
// reste pres de lui (conditions, source 32). Pendant la scene, il disparait pour le joueur
// et l'acteur de la scene (SmoothPhaseSpawnActor) prend sa place : Jaina arrive par un
// portail de Dalaran. L'acteur finit a la place du vrai Varian, qui reapparait a la fin.
namespace WhatHadToBeDone
{
    // Au-dela, Varian disparait pour de bon apres la scene.
    static constexpr float VARIAN_VISIBLE_DISTANCE = 30.0f;

    // Joueurs qui voient encore Varian apres la scene. Les visibilites sont
    // calculees par les maps, en parallele : acces sous verrou.
    std::mutex Lock;
    GuidUnorderedSet Lingering;

    void SetLingering(ObjectGuid const& guid, bool lingering)
    {
        std::lock_guard<std::mutex> lock(Lock);
        if (lingering)
            Lingering.insert(guid);
        else
            Lingering.erase(guid);
    }

    bool IsLingering(ObjectGuid const& guid)
    {
        std::lock_guard<std::mutex> lock(Lock);
        return Lingering.contains(guid);
    }
}

class quest_what_had_to_be_done_stormwind : public QuestScript
{
public:
    quest_what_had_to_be_done_stormwind() : QuestScript("quest_what_had_to_be_done_stormwind") { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        if (newStatus != QUEST_STATUS_REWARDED)
            return;

        // La scene d'abord : condition_what_had_to_be_done_scene_over cache Varian pendant qu'elle se joue.
        // Les conditions de visibilite par entry ne sont pas reevaluees seules.
        WhatHadToBeDone::SetLingering(player->GetGUID(), true);
        player->GetSceneMgr().PlayScene(SCENE_WHAT_HAD_TO_BE_DONE);
        player->UpdateObjectVisibility();
    }
};

// 150 - Scene « What Had To Be Done » (package 313) : le vrai Varian revient a la fin
class scene_what_had_to_be_done : public SceneScript
{
public:
    scene_what_had_to_be_done() : SceneScript("scene_what_had_to_be_done") { }

    void OnSceneCancel(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/) override
    {
        player->UpdateObjectVisibility();
    }

    void OnSceneComplete(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/) override
    {
        player->UpdateObjectVisibility();
    }
};

// Varian (68690), quete 500000 rendue : visible une fois la scene 313 terminee, tant que
// le joueur reste a portee. Une fois qu'il s'est eloigne, Varian ne revient plus.
class condition_what_had_to_be_done_scene_over : public ConditionScript
{
public:
    condition_what_had_to_be_done_scene_over() : ConditionScript("condition_what_had_to_be_done_scene_over") { }

    bool OnConditionCheck(Condition const* /*condition*/, ConditionSourceInfo& sourceInfo) override
    {
        Player const* player = sourceInfo.mConditionTargets[0] ? sourceInfo.mConditionTargets[0]->ToPlayer() : nullptr;
        WorldObject const* varian = sourceInfo.mConditionTargets[1];
        if (!player || !varian)
            return false;

        if (!WhatHadToBeDone::IsLingering(player->GetGUID()))
            return false;

        if (!player->IsWithinDist(varian, WhatHadToBeDone::VARIAN_VISIBLE_DISTANCE))
        {
            WhatHadToBeDone::SetLingering(player->GetGUID(), false);
            return false;
        }

        return player->GetSceneMgr().GetActiveSceneCount(SCENE_PACKAGE_WHAT_HAD_TO_BE_DONE) == 0;
    }
};

void AddSC_dalaran_purge()
{
	RegisterDalaranAI(npc_jaina_dalaran_purge);
	RegisterDalaranAI(npc_aethas_sunreaver_purge);
	RegisterDalaranAI(npc_magister_rommath_purge);

    RegisterConversationAI(conversation_dalaran_purge);

    RegisterSpellScript(spell_meteor_storm);
    RegisterSpellScript(spell_purge_hostile_area_only);
    RegisterSpellScript(spell_purge_horde_illusion_reactions);

    new quest_what_had_to_be_done_stormwind();
    new scene_what_had_to_be_done();
    new condition_what_had_to_be_done_scene_over();
}
