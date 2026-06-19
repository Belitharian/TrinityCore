/*
 * Ruins of Theramore - AI de Jaina Proudmoore (npc_jaina_ruins)
 *
 * Cette AI gere :
 *   - Les declencheurs de proximite (Phase FindJaina_Isle / FindJaina_Crater)
 *   - Les deplacements scriptes pendant la phase Standards (mort du warlord)
 *   - Le combat actif de Jaina pendant Standards_Valided (sorts mage / glace)
 *   - Le bouclier defensif a 50% PV (one-shot)
 *
 * Toutes les transitions de phase sont pilotees par l'InstanceScript via
 * SetData(DATA_*) ou TriggerGameEvent(EVENT_*) - cette AI lit la phase
 * courante via instance->GetData(DATA_SCENARIO_PHASE).
 *
 * Commentaires en francais sans accents (encodage TC).
 */

#include "InstanceScript.h"
#include "MotionMaster.h"
#include "Custom/CustomAI/CustomAI.h"
#include "ruins_of_theramore.h"

struct npc_jaina_ruins : public CustomAI
{
	// Sorts utilises uniquement par cette AI (rotation de combat).
	enum Spells
	{
		SPELL_FROSTBOLT       = 427863,
		SPELL_GRASP_OF_FROST  = 287626,
		SPELL_COMET_BARRAGE   = 354938,
		SPELL_BLINK           = 357601,
		SPELL_FROZEN_SHIELD   = 372749,
		SPELL_ARCANE_CHAOS    = 406854
	};

	// Cooldowns / delais du combat. Extraits pour faciliter le tuning.
	static constexpr Milliseconds FROSTBOLT_INITIAL_DELAY = 2s;
	static constexpr Milliseconds FROSTBOLT_REPEAT        = 2800ms;
	static constexpr Milliseconds GRASP_INITIAL_DELAY     = 8s;
	static constexpr Milliseconds GRASP_REPEAT_MIN        = 18s;
	static constexpr Milliseconds GRASP_REPEAT_MAX        = 32s;
	static constexpr Milliseconds COMET_INITIAL_DELAY     = 15s;
	static constexpr Milliseconds COMET_REPEAT_MIN        = 12s;
	static constexpr Milliseconds COMET_REPEAT_MAX        = 14s;
	static constexpr Milliseconds ARCANE_CHAOS_DELAY      = 20s;
	static constexpr Milliseconds ARCANE_CHAOS_MIN        = 45s;
	static constexpr Milliseconds ARCANE_CHAOS_MAX        = 60s;
	static constexpr Milliseconds BLINK_INITIAL_DELAY     = 12s;
	static constexpr Milliseconds BLINK_REPEAT_MIN        = 15s;
	static constexpr Milliseconds BLINK_REPEAT_MAX        = 20s;
	static constexpr float        BLINK_RANDOM_RADIUS     = 6.0f;
	static constexpr uint8        SHIELD_HEALTH_PCT       = 50;
	static constexpr uint8        SHIELD_STACKS           = 3;

	InstanceScript* instance;
	bool hasShielded = false;             // Bouclier defensif a 50% PV : one-shot
	float distance = JAINA_TRIGGER_DISTANCE_DEFAULT; // Distance de declenchement des events de proximite

	npc_jaina_ruins(Creature* creature) : CustomAI(creature)
	{
		SetCanRandomMovement(false);
		instance = me->GetInstanceScript();
	}

	// Configuration externe depuis l'InstanceScript.
	void SetData(uint32 id, uint32 value) override
	{
		switch (id)
		{
			case DATA_SET_DISTANCE:
				// Permet a l'InstanceScript d'elargir la portee de detection
				// (ex: 50 yards au cratere pour declencher le dialogue de loin).
				distance = (float)value;
				break;
			case DATA_CANCEL_GROUP:
				// Annule un groupe de taches du scheduler (typiquement DATA_PHASE_COMBAT
				// pour sortir Jaina de sa rotation de sorts a la fin du combat).
				scheduler.CancelGroup(value);
				break;
			default:
				break;
		}
	}

	// Empeche tout deplacement / melee : Jaina caste uniquement.
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

	// Immunite a toutes les mecaniques (CC, etc.) - Jaina est intouchable.
	void Reset() override
	{
		for (uint8 i = MECHANIC_NONE; i < MAX_MECHANIC; i++)
			me->ApplySpellImmune(0, IMMUNITY_MECHANIC, i, true);

		Initialize();
	}

	// Quand Jaina invoque ses elementaires, on les fait apparaitre sur les
	// deux points de spawn definis dans le header.
	void OnSpellCast(SpellInfo const* spell) override
	{
		switch (spell->Id)
		{
			case SPELL_SUMMON_WATER_ELEMENTALS:
				for (uint8 i = 0; i < ELEMENTALS_SIZE; ++i)
				{
					if (Creature* elemental = me->GetMap()->SummonCreature(NPC_WATER_ELEMENTAL, ElementalsPoint[i].spawn))
						elemental->CastSpell(elemental, SPELL_WATER_BOSS_ENTRANCE);
				}
				break;
			default:
				break;
		}
	}

	// A 50% PV, Jaina se protege une seule fois avec 3 stacks de bouclier givre.
	void DamageTaken(Unit* /*attacker*/, uint32& /*damage*/, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
	{
		if (hasShielded)
			return;

		if (HealthBelowPct(SHIELD_HEALTH_PCT))
		{
			CastStop();
			for (uint8 i = 0; i < SHIELD_STACKS; i++)
				DoCastSelf(SPELL_FROZEN_SHIELD);

			hasShielded = true;
		}
	}

	// Demarre la rotation de combat lorsque Jaina est engagee.
	void JustEngagedWith(Unit* who) override
	{
		// Sans CUSTOM_DEBUG, on n'autorise le combat que pendant la phase Standards_Valided.
		// Cela evite que Jaina riposte hors-scenario (ex: pendant les dialogues).
		#ifndef CUSTOM_DEBUG
		if ((RFTPhases)instance->GetData(DATA_SCENARIO_PHASE) != RFTPhases::Standards_Valided)
			return;
		#endif

		// Premier cast immediat pour eviter un temps mort visuel.
		DoCast(who, SPELL_FROSTBOLT);

		// Toutes les taches sont taggees DATA_PHASE_COMBAT pour pouvoir etre
		// annulees en bloc via SetData(DATA_CANCEL_GROUP, DATA_PHASE_COMBAT).
		scheduler
			.Schedule(FROSTBOLT_INITIAL_DELAY, DATA_PHASE_COMBAT, [this](TaskContext frostbolt)
			{
				DoCastVictim(SPELL_FROSTBOLT);
				frostbolt.Repeat(FROSTBOLT_REPEAT);
			})
			.Schedule(GRASP_INITIAL_DELAY, DATA_PHASE_COMBAT, [this](TaskContext grasp_of_frost)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				{
					// Annule un Arcane Chaos en cours pour ne pas chevaucher.
					CastStop(SPELL_ARCANE_CHAOS);
					DoCast(target, SPELL_GRASP_OF_FROST);
				}
				grasp_of_frost.Repeat(GRASP_REPEAT_MIN, GRASP_REPEAT_MAX);
			})
			.Schedule(COMET_INITIAL_DELAY, DATA_PHASE_COMBAT, [this](TaskContext comet_barrage)
			{
				DoCastAOE(SPELL_COMET_BARRAGE);
				comet_barrage.Repeat(COMET_REPEAT_MIN, COMET_REPEAT_MAX);
			})
			.Schedule(ARCANE_CHAOS_DELAY, DATA_PHASE_COMBAT, [this](TaskContext arcane_chaos)
			{
				// Annule un Comet Barrage en cours pour eviter le chevauchement visuel.
				CastStop(SPELL_COMET_BARRAGE);
				DoCast(SPELL_ARCANE_CHAOS);
				arcane_chaos.Repeat(ARCANE_CHAOS_MIN, ARCANE_CHAOS_MAX);
			})
			.Schedule(BLINK_INITIAL_DELAY, DATA_PHASE_COMBAT, [this](TaskContext blink)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
				{
					// Blink aleatoire autour de la cible la plus eloignee (kiting).
					Position dest = me->GetRandomPoint(target->GetPosition(), BLINK_RANDOM_RADIUS);
					me->CastSpell(dest, SPELL_BLINK, true);
				}
				blink.Repeat(BLINK_REPEAT_MIN, BLINK_REPEAT_MAX);
			});
	}

	bool CanAIAttack(Unit const*) const override { return true; }

	// Sequence scriptee de la mort du warlord :
	//   POINT_01 : Jaina arrive sur le verre brise -> elle s'agenouille, attend,
	//              se releve, puis s'approche du warlord
	//   POINT_02 : Jaina caste l'animation finale et tue le warlord -> trigger event
	//   POINT_03 : Jaina termine sa marche -> declenche EVENT_BACK_TO_SENDER
	void MovementInform(uint32, uint32 id) override
	{
		switch (id)
		{
			case MOVEMENT_INFO_POINT_01:
			{
				me->SetStandState(UNIT_STAND_STATE_KNEEL);
				// Deux taches sequentielles : on se releve d'abord, puis on s'approche
				// du warlord 2 secondes plus tard. Plus lisible qu'un TaskContext repete.
				scheduler.Schedule(2s, [this](TaskContext)
				{
					me->SetWalk(true);
					me->SetStandState(UNIT_STAND_STATE_STAND);
				});
				scheduler.Schedule(4s, [this](TaskContext)
				{
					if (Creature* warlord = instance->GetCreature(DATA_ROKNAH_WARLORD))
						me->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_02, warlord, JAINA_KNEEL_APPROACH_DIST);
				});
				break;
			}
			case MOVEMENT_INFO_POINT_02:
				me->HandleEmoteCommand(EMOTE_ONESHOT_CASTSTRONG);
				scheduler.Schedule(1s, [this](TaskContext)
				{
					if (Creature* warlord = instance->GetCreature(DATA_ROKNAH_WARLORD))
					{
                        warlord->SetVignette(VIGNETTE_NONE);
						warlord->KillSelf();

						instance->TriggerGameEvent(EVENT_WARLORD_ROKNAH_SLAIN);
					}
				});
				break;
			case MOVEMENT_INFO_POINT_03:
				// Jaina a fini sa marche -> on passe a la phase Back-to-sender.
				instance->SetData(EVENT_BACK_TO_SENDER, 0U);
				break;
			default:
				break;
		}
	}

	// Detection de proximite : declenche les events de phase quand un joueur
	// approche Jaina pendant les phases d'exploration.
	void MoveInLineOfSight(Unit* who) override
	{
		ScriptedAI::MoveInLineOfSight(who);

		if (me->IsEngaged() || who->GetTypeId() != TYPEID_PLAYER)
			return;

		Player* player = who->ToPlayer();
		if (!player || player->IsGameMaster())
			return;

		if (!player->IsFriendlyTo(me) || !player->IsWithinDist(me, distance))
			return;

		switch ((RFTPhases)instance->GetData(DATA_SCENARIO_PHASE))
		{
			case RFTPhases::FindJaina_Isle:
                me->SetVignette(VIGNETTE_NONE);
				instance->TriggerGameEvent(EVENT_FIND_JAINA_01);
				break;
			case RFTPhases::FindJaina_Crater:
                me->SetVignette(VIGNETTE_NONE);
                instance->SetData(EVENT_FIND_JAINA_02, 0U);
				break;
			default:
				break;
		}
	}

	void UpdateAI(uint32 diff) override
	{
		scheduler.Update(diff, [this]
		{
			UpdateVictim();
		});
	}
};

void AddSC_ruins_of_theramore()
{
	RegisterRuinsAI(npc_jaina_ruins);
}
