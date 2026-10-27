/*
 * Ruins of Theramore - AI de Jaina Proudmoore (npc_jaina_ruins)
 *
 * Cette AI gere :
 *   - Le declencheur de proximite sur l'ilot (EVENT_FIND_JAINA_01)
 *   - Le retour des arrivees de trajet vers l'InstanceScript (DATA_JAINA_ARRIVED),
 *     qu'elle suive un chemin en base ou le repli en ligne droite
 *   - Le massacre des pillards (phase Looters), en vrai mage a distance :
 *       - placement : MoveChase a distance, circle-kite et recul (CustomAI Distance)
 *       - Blink des qu'un ennemi arrive au contact, precede d'une Nova de givre
 *         s'ils sont plusieurs
 *       - Teleportation aupres de l'ennemi le plus eloigne quand elle est encerclee ou
 *         acculee et que Blink est en recharge
 *       - Contresort sur le premier pillard qui incante a portee
 *       - Trait de feu instantane pendant les reculs
 *       - rotation feu / givre / arcane, comme dans le roman, plus la pluie de
 *         meteores de Rommath
 *   - Le bouclier defensif a 50% PV (one-shot). Elle ne peut pas mourir
 *     (sparring en base).
 *
 * Toutes les transitions de phase sont pilotees par l'InstanceScript ; cette
 * AI lit la phase courante via instance->GetData(DATA_SCENARIO_PHASE).
 *
 * Commentaires en francais sans accents (encodage TC).
 */

#include "InstanceScript.h"
#include "MotionMaster.h"
#include "MovementDefines.h"
#include "Custom/CustomAI/CustomAI.h"
#include "ruins_of_theramore.h"

struct npc_jaina_ruins : public CustomAI
{
	// Sorts repris des IA de mages des autres scenarios (Bataille, Purge, anciennes Ruines).
	enum Spells
	{
		SPELL_FIREBALL          = 79854,
		SPELL_FIRE_BLAST        = 20679,    // Instantane : lache pendant les reculs
		SPELL_FLAMESTRIKE       = 330347,   // La boule de feu tourbillonnante lancee au milieu des orcs
		SPELL_FROST_NOVA        = 284879,   // Les orcs geles sur place, les jambes prises dans la glace
		SPELL_CONE_OF_COLD      = 292294,   // Le cone d'air glace qui les ralentit
		SPELL_ARCANE_BLAST      = 291316,
		SPELL_METEOR_STORM      = 179215,   // Pluie de meteores de Rommath (spell_meteor_storm, Purge)
		SPELL_FLAMING_TWISTERS  = 1283566,  // Tornades de feu (Embertalon, Midnight) : spell_ruins_flaming_twisters
		SPELL_COUNTERSPELL      = 173077,
		SPELL_BLINK             = 295236,   // Blink des Rok'nah Hag : saut vers l'avant (TARGET_DEST_CASTER_FRONT_LEAP)
		SPELL_TELEPORT          = 135176,   // Teleportation sur une destination (Purge de Dalaran)
		SPELL_FROZEN_SHIELD     = 372749
	};

	enum Groups : uint32
	{
		GROUP_REACTIONS         = 10        // Contresort, Blink au contact : coupes avec le combat
	};

	// Rotation
	static constexpr Milliseconds FILLER_REPEAT           = 2800ms;
	static constexpr Milliseconds FLAMESTRIKE_REPEAT_MIN  = 12s;
	static constexpr Milliseconds FLAMESTRIKE_REPEAT_MAX  = 16s;
	static constexpr Milliseconds CONE_INITIAL_DELAY      = 8s;
	static constexpr Milliseconds CONE_REPEAT_MIN         = 14s;
	static constexpr Milliseconds CONE_REPEAT_MAX         = 18s;
	static constexpr Milliseconds ARCANE_INITIAL_DELAY    = 5s;
	static constexpr Milliseconds ARCANE_REPEAT_MIN       = 8s;
	static constexpr Milliseconds ARCANE_REPEAT_MAX       = 10s;
	static constexpr Milliseconds TWISTERS_INITIAL_DELAY  = 5s;
	static constexpr Milliseconds TWISTERS_REPEAT_MIN     = 9s;
	static constexpr Milliseconds TWISTERS_REPEAT_MAX     = 12s;
	static constexpr float        TWISTERS_RANGE          = 60.0f;    // Rayon du sort (RadiusIndex 48)
	static constexpr Milliseconds METEOR_INITIAL_DELAY    = 9s;
	static constexpr Milliseconds METEOR_REPEAT_MIN       = 8s;     // Deux fois plus souvent que Rommath (15-18 s)
	static constexpr Milliseconds METEOR_REPEAT_MAX       = 10s;
	static constexpr float        CONE_OF_COLD_RANGE      = 10.0f;

	// Reactions
	static constexpr Milliseconds REACTION_CHECK          = 500ms;
	static constexpr Milliseconds COUNTERSPELL_COOLDOWN   = 12s;
	static constexpr Milliseconds BLINK_COOLDOWN          = 15s;
	static constexpr Milliseconds TELEPORT_COOLDOWN       = 30s;
	static constexpr Milliseconds FROST_NOVA_COOLDOWN     = 20s;
	static constexpr float        COUNTERSPELL_RANGE      = 35.0f;
	static constexpr float        TOO_CLOSE_RANGE         = 6.0f;     // Un ennemi a cette distance : Blink
	static constexpr float        FROST_NOVA_RANGE        = 8.0f;
	static constexpr uint32       FROST_NOVA_MIN_ENEMIES  = 2;        // Nova avant le Blink a partir de N ennemis proches
	static constexpr float        BLINK_DISTANCE          = 15.0f;    // Sert a choisir la direction : la portee vient du sort
	static constexpr float        TELEPORT_DISTANCE       = 25.0f;    // Repli sans cible : vers l'arc degage
	static constexpr float        TELEPORT_AROUND_TARGET_MIN = 4.0f;  // Arrivee autour de l'ennemi le plus eloigne...
	static constexpr float        TELEPORT_AROUND_TARGET_MAX = 8.0f;  // ... ni colle a lui, ni trop loin
	static constexpr float        CHASE_DISTANCE          = 20.0f;

	static constexpr float        SPELL_CRIT_CHANCE       = 25.0f;    // Comme les mages de la Purge (30 %), un peu moins
	static constexpr uint8        SHIELD_HEALTH_PCT       = 50;
	static constexpr uint8        SHIELD_STACKS           = 3;

	InstanceScript* instance;
	bool hasShielded = false;             // Bouclier defensif a 50% PV : one-shot
	bool counterspellReady = true;
	bool blinkReady = true;
	bool teleportReady = true;
	bool frostNovaReady = true;

	npc_jaina_ruins(Creature* creature) : CustomAI(creature, AI_Type::Distance)
	{
		// Circle-kite autour de sa cible pendant le combat (CustomAI Distance).
		SetCanRandomMovement(true);
		instance = me->GetInstanceScript();
	}

	float GetDistance() override { return CHASE_DISTANCE; }

	// Comme les mages de la Purge : la chance de critique se pose a l'apparition.
	void JustAppeared() override
	{
		CustomAI::JustAppeared();
		SetSpellCritChance(SPELL_CRIT_CHANCE);
	}

	void SetData(uint32 id, uint32 value) override
	{
		switch (id)
		{
			case DATA_CANCEL_GROUP:
				// Sort Jaina de sa rotation (DATA_PHASE_COMBAT) a la fin du combat.
				scheduler.CancelGroup(value);
				if (value == DATA_PHASE_COMBAT)
				{
					scheduler.CancelGroup(GROUP_REACTIONS);
					scheduler.CancelGroup(RandomMovement);
					scheduler.CancelGroup(Encircle);
				}
				break;
			default:
				break;
		}
	}

	// Immunite a toutes les mecaniques (CC, etc.). Un reset ne survient que si
	// elle sort de combat : tout est coupe, JustEngagedWith relancera.
	void Reset() override
	{
		CustomAI::Reset();

		for (uint8 i = MECHANIC_NONE; i < MAX_MECHANIC; i++)
			me->ApplySpellImmune(0, IMMUNITY_MECHANIC, i, true);

		counterspellReady = true;
		blinkReady = true;
		teleportReady = true;
		frostNovaReady = true;

		// Hors combat, le baton reste range.
		me->SetSheath(SHEATH_STATE_UNARMED);
	}

	// CustomAI range l'arme des casters a chaque AttackStart (rappele a chaque
	// changement de cible) : Jaina, elle, combat le baton en main.
	void AttackStart(Unit* who) override
	{
		CustomAI::AttackStart(who);

		if (me->GetVictim())
			me->SetSheath(SHEATH_STATE_MELEE);
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

	// =====================================================================
	// Combat
	// =====================================================================

	// Le massacre des pillards. Hors de cette phase, Jaina ne riposte pas.
	void JustEngagedWith(Unit* who) override
	{
		#ifndef CUSTOM_DEBUG
		if ((RFTPhases)instance->GetData(DATA_SCENARIO_PHASE) != RFTPhases::Looters)
			return;
		#endif

		// Ouverture : le feu au milieu des orcs encore sonnes par l'onde arcanique.
		DoCast(who, SPELL_FLAMESTRIKE);

		// Toutes les taches de rotation sont taggees DATA_PHASE_COMBAT pour
		// etre annulees en bloc via SetData(DATA_CANCEL_GROUP, DATA_PHASE_COMBAT).
		scheduler
			// Filler : boule de feu (l'eclair de givre a laisse sa place aux tornades de feu).
			.Schedule(FILLER_REPEAT, DATA_PHASE_COMBAT, [this](TaskContext filler)
			{
				DoCastVictim(SPELL_FIREBALL);
				filler.Repeat(FILLER_REPEAT);
			})
			.Schedule(FLAMESTRIKE_REPEAT_MIN, DATA_PHASE_COMBAT, [this](TaskContext flamestrike)
			{
				// Sur un pillard au hasard : le cercle de feu touche ceux qui l entourent.
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				{
					CastStop();
					DoCast(target, SPELL_FLAMESTRIKE);
				}
				flamestrike.Repeat(FLAMESTRIKE_REPEAT_MIN, FLAMESTRIKE_REPEAT_MAX);
			})
			.Schedule(CONE_INITIAL_DELAY, DATA_PHASE_COMBAT, [this](TaskContext cone_of_cold)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::MinDistance, 0, CONE_OF_COLD_RANGE))
				{
					CastStop();
					me->SetFacingToObject(target);
					DoCast(target, SPELL_CONE_OF_COLD);
					cone_of_cold.Repeat(CONE_REPEAT_MIN, CONE_REPEAT_MAX);
					return;
				}
				cone_of_cold.Repeat(2s);
			})
			// Tornades de feu : trois tornades sur des pillards a moins de 60 m.
			.Schedule(TWISTERS_INITIAL_DELAY, DATA_PHASE_COMBAT, [this](TaskContext twisters)
			{
				if (SelectTarget(SelectTargetMethod::Random, 0, TWISTERS_RANGE))
				{
					CastStop();
					DoCastSelf(SPELL_FLAMING_TWISTERS);
					twisters.Repeat(TWISTERS_REPEAT_MIN, TWISTERS_REPEAT_MAX);
					return;
				}
				twisters.Repeat(2s);
			})
			.Schedule(ARCANE_INITIAL_DELAY, DATA_PHASE_COMBAT, [this](TaskContext arcane_blast)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_ARCANE_BLAST);
				arcane_blast.Repeat(ARCANE_REPEAT_MIN, ARCANE_REPEAT_MAX);
			})
			// Pluie de meteores sur un pillard au hasard, comme Rommath.
			.Schedule(METEOR_INITIAL_DELAY, DATA_PHASE_COMBAT, [this](TaskContext meteor)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				{
					CastStop();
					DoCast(target, SPELL_METEOR_STORM);
					meteor.Repeat(METEOR_REPEAT_MIN, METEOR_REPEAT_MAX);
					return;
				}
				meteor.Repeat(METEOR_REPEAT_MIN);
			})
			// Reactions : surveillees en continu, chacune avec sa propre recharge.
			.Schedule(REACTION_CHECK, GROUP_REACTIONS, [this](TaskContext reactions)
			{
				TryCounterspell();
				TryBlinkAway();
				reactions.Repeat(REACTION_CHECK);
			});
	}

	// Contresort sur le premier pillard qui incante a portee.
	void TryCounterspell()
	{
		if (!counterspellReady)
			return;

		Unit* caster = DoSelectCastingUnit(SPELL_COUNTERSPELL, COUNTERSPELL_RANGE);
		if (!caster)
			return;

		CastStop();
		DoCast(caster, SPELL_COUNTERSPELL, TRIGGERED_IGNORE_GCD);
		StartCooldown(counterspellReady, COUNTERSPELL_COOLDOWN);
	}

	// Un ennemi au contact : Nova de givre s'ils sont plusieurs, puis Blink vers
	// l'arc le plus degage. L'encerclement complet est gere par OnEncircled.
	void TryBlinkAway()
	{
		if (!blinkReady || me->HasUnitState(UNIT_STATE_CASTING) || !EnemiesInRange(TOO_CLOSE_RANGE))
			return;

		if (EnemiesInRange(FROST_NOVA_RANGE) >= FROST_NOVA_MIN_ENEMIES)
			CastFrostNova();

		CastBlink();
	}

	void CastFrostNova()
	{
		if (!frostNovaReady)
			return;

		CastStop();
		DoCastSelf(SPELL_FROST_NOVA, TRIGGERED_FULL_MASK);
		StartCooldown(frostNovaReady, FROST_NOVA_COOLDOWN);
	}

	void CastBlink()
	{
		Position const dest = GetBestEscapePosition(BLINK_DISTANCE);
		if (me->GetExactDist2d(dest) < TOO_CLOSE_RANGE)
			return;

		// Le Blink part droit devant : on se tourne d'abord vers l'arc degage.
		CastStop();
		me->SetFacingTo(me->GetAbsoluteAngle(dest));
		DoCastSelf(SPELL_BLINK, TRIGGERED_FULL_MASK);
		me->RemoveMovementImpairingAuras(true);
		NotifyTeleported();
		StartCooldown(blinkReady, BLINK_COOLDOWN);
	}

	// Elle sort de la melee en se teleportant aupres de l'ennemi le plus eloigne,
	// sur un point libre autour de lui. Sans cible, vers l'arc le plus degage.
	void CastTeleport()
	{
		Position dest;
		if (Unit* farthest = SelectTarget(SelectTargetMethod::MaxDistance, 0))
			dest = GetRandomPosition(me, farthest->GetPosition(), TELEPORT_AROUND_TARGET_MAX, TELEPORT_AROUND_TARGET_MIN);
		else
			dest = GetBestEscapePosition(TELEPORT_DISTANCE);

		if (me->GetExactDist2d(dest) < TOO_CLOSE_RANGE)
			return;

		CastStop();
		me->CastSpell(dest, SPELL_TELEPORT, true);
		NotifyTeleported(3s);
		StartCooldown(teleportReady, TELEPORT_COOLDOWN);
	}

	// Encerclee (CustomAI) : Nova puis Blink, ou Teleportation si Blink est en recharge.
	bool OnEncircled(Unit* /*victim*/) override
	{
		CastFrostNova();

		if (blinkReady)
			CastBlink();
		else if (teleportReady)
			CastTeleport();
		else
			return false;   // Fuite a pied (PanicFlee de CustomAI)

		return true;
	}

	// Acculee (ennemis et murs partout) : la teleportation l'en sort, sinon la Nova
	// les tient a distance.
	void OnCornered(Unit* /*victim*/) override
	{
		if (teleportReady)
			CastTeleport();
		else
			CastFrostNova();
	}

	// Pendant les reculs de CustomAI, le cast normal est coupe : trait de feu instantane.
	void OnBackpedTick(Unit* victim) override
	{
		DoCast(victim, SPELL_FIRE_BLAST, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_IGNORE_GCD);
	}

	// Recharge interne : le drapeau repasse a vrai apres `cooldown`.
	void StartCooldown(bool& ready, Milliseconds cooldown)
	{
		ready = false;
		scheduler.Schedule(cooldown, [&ready](TaskContext /*context*/)
		{
			ready = true;
		});
	}

	bool CanAIAttack(Unit const*) const override { return true; }

	// =====================================================================
	// Trajets et declencheurs
	// =====================================================================

	// Fin d'un trajet en repli (MovePoint / MoveCloserAndStop), point ID
	// MOVEMENT_JAINA_LEG_BASE + leg. Les autres IDs (saut, recul) vont a CustomAI.
	void MovementInform(uint32 type, uint32 id) override
	{
		CustomAI::MovementInform(type, id);

		if (type != POINT_MOTION_TYPE || id < MOVEMENT_JAINA_LEG_BASE || id >= MOVEMENT_JAINA_LEG_BASE + (uint32)RFTLegs::Max)
			return;

		instance->SetData(DATA_JAINA_ARRIVED, id - MOVEMENT_JAINA_LEG_BASE);
	}

	// Pauses du parcours (Delay des noeuds) : Jaina regarde les morts qu'elle
	// croise, comme une somnambule, sans s'y attarder.
	void WaypointReached(uint32 nodeId, uint32 pathId) override
	{
		switch (pathId)
		{
			case PATH_JAINA_LEG_BASE + (uint32)RFTLegs::Armory:
				switch (nodeId)
				{
					case 4:     // Le cavalier et son cheval
						FacePoint(RiderCorpsePoint);
						Talk(SAY_TRAIL_RIDER_EMOTE);
						break;
					case 7:     // Les soldats tombes l'arme au fourreau
						FacePoint(SoldiersCorpsePoint);
						Talk(SAY_TRAIL_SOLDIERS_EMOTE);
						break;
					case 9:     // Elle ramasse une page, qui tombe en poussiere
						FacePoint(LostPagePoint);
						me->SetStandState(UNIT_STAND_STATE_KNEEL);
						Talk(SAY_TRAIL_PAGE_EMOTE);
						scheduler.Schedule(2500ms, [this](TaskContext /*context*/)
						{
							me->SetStandState(UNIT_STAND_STATE_STAND);
						});
						break;
					default:
						break;
				}
				break;
			case PATH_JAINA_LEG_BASE + (uint32)RFTLegs::Kinndy:
				if (nodeId == 6)    // Thaddus et Horran, puis Aden sur son toit
				{
					FacePoint(ThaddusCorpsePoint);
					Talk(SAY_TRAIL_THADDUS_EMOTE);
					scheduler.Schedule(2500ms, [this](TaskContext /*context*/)
					{
						FacePoint(AdenCorpsePoint);
						Talk(SAY_TRAIL_ADEN_EMOTE);
					});
				}
				break;
			default:
				break;
		}
	}

	void FacePoint(Position const& pos)
	{
		me->SetFacingTo(me->GetAbsoluteAngle(pos));
	}

	// Fin d'un chemin trace (WaypointPath du header, ID PATH_JAINA_LEG_BASE + leg).
	void WaypointPathEnded(uint32 /*nodeId*/, uint32 pathId) override
	{
		if (pathId < PATH_JAINA_LEG_BASE || pathId >= PATH_JAINA_LEG_BASE + (uint32)RFTLegs::Max)
			return;

		instance->SetData(DATA_JAINA_ARRIVED, pathId - PATH_JAINA_LEG_BASE);
	}

	// Sur l'ilot, un joueur qui approche declenche la cinematique.
	void MoveInLineOfSight(Unit* who) override
	{
		ScriptedAI::MoveInLineOfSight(who);

		if (me->IsEngaged() || who->GetTypeId() != TYPEID_PLAYER)
			return;

		Player* player = who->ToPlayer();
		if (!player || player->IsGameMaster())
			return;

		if (!player->IsFriendlyTo(me) || !player->IsWithinDist(me, JAINA_TRIGGER_DISTANCE))
			return;

		if ((RFTPhases)instance->GetData(DATA_SCENARIO_PHASE) == RFTPhases::FindJaina_Isle)
		{
			me->SetVignette(VIGNETTE_NONE);
			instance->TriggerGameEvent(EVENT_FIND_JAINA_01);
		}
	}
};

void AddSC_ruins_of_theramore()
{
	RegisterRuinsAI(npc_jaina_ruins);
}
