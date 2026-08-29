/*
 * Battle for Theramore - AIs des personnages principaux
 *
 * Regroupe les acteurs nommes du scenario "La Chute de Theramore" :
 *   - npc_jaina_theramore    : Jaina, pivot du scenario (declencheurs de
 *                              proximite, rotation feu/givre, annonces de vagues)
 *   - npc_archmage_tervosh   : mage de feu, barriere ardente + Conflagration
 *   - npc_amara_leeson       : mage de feu, triple barriere a l'engagement
 *   - npc_rhonin             : mage arcanique, gossip du bouclier runique
 *   - npc_kinndy_sparkshine  : PNJ de dialogue, purement scripte
 *   - npc_tari_cogg          : mage arcanique, Evocation defensive a 20% PV
 *   - npc_pained             : garde du corps de Jaina, purement scripte
 *   - npc_kalecgos_theramore : Kalecgos sous forme humanoide (mage de givre)
 *   - npc_ziradormi_theramore: PNJ de teleport hors scenario
 *   - npc_kalecgos_dragon    : Kalecgos dragon survolant la ville en combat
 *
 * Les AIs ne pilotent JAMAIS le scenario : elles remontent des evenements a
 * l'InstanceScript (TriggerGameEvent / SetData) qui, lui, decide des
 * transitions de phase. Elles lisent la phase courante via
 * instance->GetData(DATA_SCENARIO_PHASE).
 *
 * Les chemins (WaypointPathEnded) et points (MovementInform) referencent les
 * paths et MOVEMENT_INFO_POINT_* definis dans battle_for_theramore.h.
 *
 * Commentaires en francais sans accents (encodage TC).
 */

#include "GameObject.h"
#include "InstanceScript.h"
#include "KillRewarder.h"
#include "MotionMaster.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "TemporarySummon.h"
#include "CustomAI.h"
#include "battle_for_theramore.h"

// =========================================================================
// npc_jaina_theramore - Lady Jaina Proudmoore
// =========================================================================
// Trois roles distincts selon la phase :
//   1. Declencheur de scenario : MoveInLineOfSight leve l'EVENT_FIND_JAINA_*
//      correspondant a la phase courante quand un joueur l'approche.
//   2. Combattante : rotation mage feu/givre pendant la bataille, avec
//      annonces vocales des vagues via DoAction(DATA_WAVE_*).
//   3. Pompier : pendant HelpTheWounded elle eteint automatiquement les
//      foyers (NPC_THERAMORE_FIRE_CREDIT) qui passent a portee.
struct npc_jaina_theramore : public CustomAI
{
	// Distance a laquelle Jaina reagit (joueurs comme foyers d'incendie).
	static constexpr float TRIGGER_DISTANCE       = 4.0f;
	// Orientation prise a l'arrivee sur la place, face aux joueurs.
	static constexpr float JAINA_SQUARE_FACING    = 3.13f;
	// Echelles cosmetiques du portail de Stormwind et de son trigger d'eclairs.
	static constexpr float PORTAL_SCALE           = 0.8f;
	static constexpr float LIGHTNING_TRIGGER_SCALE = 1.8f;

	npc_jaina_theramore(Creature* creature) : CustomAI(creature, true, AI_Type::Melee),
		instance(nullptr)
	{
		instance = me->GetInstanceScript();

        SetCanRandomMovement(false);
    }

	enum Spells
	{
		SPELL_FIREBALL                  = 20678,
		SPELL_FIREBLAST                 = 20679,
		SPELL_SUMMON_WATER_ELEMENTALS   = 20681,
		SPELL_FROSTBOLT_COSMETIC        = 237649,
		SPELL_LIGHTNING_FX              = 278455,
		SPELL_BLIZZARD                  = 284968,
	};

	InstanceScript* instance;

	void Reset() override
	{
		CustomAI::Reset();

		textOnCooldown = false;
	}

	// Annonce vocale de la vague qui arrive. Appele par l'InstanceScript au
	// moment du spawn, avec l'identifiant du point d'arrivee de la vague.
	void DoAction(int32 actionId) override
	{
		switch (actionId)
		{
			// Portes
			case DATA_WAVE_DOORS:
				me->AI()->Talk(SAY_BATTLE_GATE);
				break;
			// Citadelle
			case DATA_WAVE_CITADEL:
				me->AI()->Talk(SAY_BATTLE_CITADEL);
				break;
			// Docks
			case DATA_WAVE_DOCKS:
				me->AI()->Talk(SAY_BATTLE_DOCKS);
				break;
			// Portes Ouest
			case DATA_WAVE_WEST:
				me->AI()->Talk(SAY_BATTLE_WEST);
				break;
			// Les autres groupes (decorations, bateau) n'ont pas de replique
			default:
				break;
		}
	}

	// Le trait de givre cosmetique tue le credit d'incendie qu'il touche :
	// c'est ce qui fait disparaitre le feu quand Jaina passe a cote.
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
		TalkInCombat(SAY_JAINA_SLAY_01);
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		// Jaina ouvre systematiquement avec ses elementaires d'eau.
		DoCastSelf(SPELL_SUMMON_WATER_ELEMENTALS);

		scheduler
			// Filler : Fireball sur la victime, avec replique a cooldown.
			.Schedule(1s, [this](TaskContext fireball)
			{
				TalkInCombat(SAY_JAINA_SPELL_01);
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
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				{
					TalkInCombat(SAY_JAINA_BLIZZARD_01);
					DoCast(target, SPELL_BLIZZARD);
				}
				blizzard.Repeat(14s, 22s);
			});
	}

	// Path 1 : marche d'apres-bataille jusqu'a la place (JainaPath01)
	// Path 2 : depart vers le sommet de la tour (JainaPath02)
	void WaypointPathEnded(uint32 /*pointId*/, uint32 pathId) override
	{
		if (pathId == 1)
		{
			// Arrivee sur la place : on fige Jaina face aux joueurs et on
			// declenche le dialogue d'apres-bataille.
			me->StopMoving();
			me->GetMotionMaster()->Clear();
			me->GetMotionMaster()->MoveIdle();
			me->SetFacingTo(JAINA_SQUARE_FACING);

			instance->TriggerGameEvent(EVENT_FIND_JAINA_04);
		}
		else if (pathId == 2)
		{
			// Faux teleport : Jaina disparait 2 secondes puis reapparait au
			// sommet de la tour ou elle ouvre le portail vers Stormwind.
			me->SetVisible(false);
			scheduler.Schedule(2s, [this](TaskContext /*context*/)
			{
				me->SetVisible(true);
				me->NearTeleportTo(JainaPoint05);
				if (GameObject* portal = me->SummonGameObject(GOB_PORTAL_TO_STORMWIND, PortalPoint03, QuaternionData::fromEulerAnglesZYX(PortalPoint03.GetOrientation(), 0.f, 0.f), 0s))
					portal->SetObjectScale(PORTAL_SCALE);
				// Trigger invisible agrandi : il ne sert qu'a porter l'effet
				// d'eclairs au-dessus du portail.
				if (TempSummon* summon = me->SummonCreature(WORLD_TRIGGER, PortalPoint03, TEMPSUMMON_MANUAL_DESPAWN))
				{
					summon->SetObjectScale(LIGHTNING_TRIGGER_SCALE);
					summon->CastSpell(summon, SPELL_LIGHTNING_FX, true);
				}
				DoCastSelf(SPELL_PORTAL_CHANNELING_01);

				instance->SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::RetrieveRhonin);
			});
		}
	}

	// MOVEMENT_INFO_POINT_01 : Jaina est arrivee a la table du conseil.
	void MovementInform(uint32 type, uint32 id) override
	{
        CustomAI::MovementInform(type, id);

		if (type == EFFECT_MOTION_TYPE || type == POINT_MOTION_TYPE)
		{
			switch (id)
			{
				case MOVEMENT_INFO_POINT_01:
					instance->TriggerGameEvent(EVENT_THE_COUNCIL);
					break;
				default:
					break;
			}
		}
	}

	// Detection de proximite : deux usages en un seul hook.
	//   - pendant HelpTheWounded, Jaina eteint les foyers qui passent a portee
	//   - le reste du temps, l'approche d'un joueur declenche l'evenement de
	//     scenario correspondant a la phase courante
	void MoveInLineOfSight(Unit* who) override
	{
		ScriptedAI::MoveInLineOfSight(who);

		BFTPhases phase = (BFTPhases)instance->GetData(DATA_SCENARIO_PHASE);
		if (phase == BFTPhases::HelpTheWounded)
		{
			if (who->IsWithinDist(me, TRIGGER_DISTANCE) && who->GetEntry() == NPC_THERAMORE_FIRE_CREDIT)
			{
				// Cast direct et instantane : purement cosmetique, il sert
				// juste a supprimer le credit d'incendie (voir SpellHitTarget).
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

			if (player->IsFriendlyTo(me) && player->IsWithinDist(me, TRIGGER_DISTANCE))
			{
				// La vignette a rempli son role (guider le joueur) : on l'efface.
                me->SetVignette(VIGNETTE_NONE);

				switch (phase)
				{
					case BFTPhases::FindJaina:
						// Premiere rencontre : Jaina rengaine et se fige pour
						// la cinematique du conseil.
                        me->SetSheath(SHEATH_STATE_UNARMED);
                        me->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
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

// =========================================================================
// npc_archmage_tervosh - Archimage Tervosh (mage de feu)
// =========================================================================
// Rotation feu classique : Fireball en filler, Scorch / Flamestrike sur
// cibles secondaires, barriere ardente maintenue en permanence, et Tongues
// of Flame uniquement quand assez d'ennemis sont au contact.
// Ses trois paths correspondent aux allers-retours de la scene du conseil.
struct npc_archmage_tervosh : public CustomAI
{
	// Nombre d'ennemis a portee sous lequel Tongues of Flame ne vaut pas le cast.
	static constexpr float TONGUES_OF_FLAME_RANGE = 12.0f;
	// Chance d'appliquer Conflagration sur une cible touchee par un sort de feu.
	static constexpr uint32 CONFLAGRATION_CHANCE  = 40;

	npc_archmage_tervosh(Creature* creature) : CustomAI(creature, true)
	{
        SetCanRandomMovement(false);
	}

	enum Spells
	{
		SPELL_FIREBALL              = 358226,
		SPELL_FLAMESTRIKE           = 330347,
		SPELL_BLAZING_BARRIER       = 295238,
		SPELL_SCORCH                = 358238,
		SPELL_CONFLAGRATION         = 226757,
		SPELL_TONGUES_OF_FLAME      = 412486
	};

	// Path 1 : approche de la table du conseil
	// Path 2 : montee a l'etage (il disparait le temps de la scene suivante)
	// Path 3 : retour dans la salle du conseil, il se remet a lire
	void WaypointPathEnded(uint32 /*pointId*/, uint32 pathId) override
	{
		switch (pathId)
		{
			case 1:
				me->SetFacingTo(0.70f);
				break;
			case 2:
				me->SetFacingTo(2.14f);
				me->SetVisible(false);
				break;
			case 3:
				me->SetFacingTo(4.05f);
				me->SetEmoteState(EMOTE_STATE_READ);
				break;
		}
	}

	// Chaque sort de feu direct a une chance d'allumer un DoT Conflagration
	// sur la cible, si elle ne l'a pas deja.
	void SpellHitTarget(WorldObject* target, SpellInfo const* spellInfo) override
	{
		switch (spellInfo->Id)
		{
			case SPELL_FIREBALL:
			case SPELL_FLAMESTRIKE:
			case SPELL_SCORCH:
			{
				Unit* victim = target->ToUnit();
				if (victim && !victim->HasAura(SPELL_CONFLAGRATION) && roll_chance(CONFLAGRATION_CHANCE))
					DoCast(victim, SPELL_CONFLAGRATION, true);
			}
			break;
		}
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		DoCastSelf(SPELL_BLAZING_BARRIER);

		scheduler
			// Maintien de la barriere : tant qu'elle tient on repasse toutes
			// les secondes, sinon on la relance et on repart sur son cooldown.
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
			// AoE de melee : on attend d'avoir au moins un ennemi au contact,
			// sinon on repasse plus tot sans consommer le cooldown complet.
			.Schedule(12s, 18s, [this](TaskContext lava_spin)
			{
				if (EnemiesInRange(TONGUES_OF_FLAME_RANGE))
				{
					CastStop(SPELL_TONGUES_OF_FLAME);
					DoCast(SPELL_TONGUES_OF_FLAME);
					lava_spin.Repeat(22s, 35s);
				}
				else
					lava_spin.Repeat(10s);
			})
			// Filler
			.Schedule(5ms, [this](TaskContext fireball)
			{
				DoCastVictim(SPELL_FIREBALL);
				fireball.Repeat(2s);
			});
	}
};

// =========================================================================
// npc_amara_leeson - Archimage Amara Leeson (mage de feu)
// =========================================================================
// Version plus offensive que Tervosh : elle empile ses trois barrieres a
// l'engagement puis enchaine sans interruption.
// Path 2 marque son retour par le portail (declenche EVENT_WAIT_ARCHMAGE_LESSON) ;
// les paths 1 et 3 la font simplement disparaitre en coulisses.
struct npc_amara_leeson : public CustomAI
{
	npc_amara_leeson(Creature* creature) : CustomAI(creature, true)
	{
		instance = me->GetInstanceScript();

        SetCanRandomMovement(false);
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

	void JustEngagedWith(Unit* /*who*/) override
	{
		// Les trois barrieres sont cumulables : Amara est concue pour tenir
		// la ligne de front sans soutien.
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

	void WaypointPathEnded(uint32 /*pointId*/, uint32 pathId) override
	{
		if (pathId == 1 || pathId == 3)
		{
			// Sortie de scene : Amara part en mission, elle est masquee
			// jusqu'a son retour.
			me->SetVisible(false);
		}
		else if (pathId == 2)
		{
			// Retour d'Amara : le scenario peut passer a l'etape suivante.
			instance->TriggerGameEvent(EVENT_WAIT_ARCHMAGE_LESSON);
		}
	}
};

// =========================================================================
// npc_rhonin - Rhonin (mage arcanique)
// =========================================================================
// Deux facettes :
//   - hors combat : gossip unique qui distribue le bouclier runique a tous
//     les joueurs (SPELL_RUNIC_SHIELD) et valide le criteria correspondant
//   - en combat : rotation arcane a charges. Arcane Blast empile jusqu'a
//     ARCANE_CHARGES_MAX puis Arcane Barrage consomme les charges. Arcane
//     Pulse prend la main des que le corps a corps devient dense.
struct npc_rhonin : public CustomAI
{
	// Nombre de charges d'Arcane Blast avant de depenser en Arcane Barrage.
	static constexpr uint8 ARCANE_CHARGES_MAX     = 4;
	// Chance de declencher le combo Time Warp -> Arcane Barrage sur un Blast.
	static constexpr uint32 TIME_WARP_CHANCE      = 40;
	// Rayon et nombre d'ennemis requis pour declencher Arcane Pulse.
	static constexpr float ARCANE_PULSE_RANGE     = 9.0f;
	static constexpr uint32 ARCANE_PULSE_MIN_FOES = 3;
	// Seuil de mana (en %) sous lequel Rhonin part en Evocation.
	static constexpr float EVOCATION_MANA_PCT     = 10.0f;
	// Rayon de dispersion des cristaux arcaniques autour de leur cible.
	static constexpr float CRYSTAL_SPREAD_RADIUS  = 4.0f;

	enum Misc
	{
		// Gossip
		GOSSIP_MENU_DEFAULT         = 65001,

		// NPCs
		NPC_ARCANIC_CRYSTAL         = 86602,

		// Spells
		SPELL_ARCANE_AFFINITY       = 173213,
		SPELL_SHIELD_PLAYERS        = 388194,
	};

	// Groupes de taches du scheduler : GROUP_NORMAL est retarde en bloc
	// pendant qu'Arcane Pulse tourne, pour eviter le chevauchement de casts.
	enum Groups
	{
		GROUP_NORMAL,
		GROUP_ARCANE_PULSE
	};

	enum Spells
	{
		SPELL_TEMPORAL_DISPLACEMENT = 80354,
		SPELL_ARCANE_CAST_INSTANT   = 135030,
		SPELL_PRISMATIC_BARRIER     = 235450,
		SPELL_EVOCATION             = 243070,
		SPELL_ARCANE_BLAST          = 291316,
		SPELL_ARCANE_BARRAGE        = 291318,
		SPELL_TIME_WARP             = 342242,
		SPELL_ARCANE_SALVO          = 378850,
		SPELL_ARCANE_PULSE          = 423607,
	};

	npc_rhonin(Creature* creature) : CustomAI(creature, true), arcaneCharges(0)
	{
		instance = creature->GetInstanceScript();

        SetCanRandomMovement(false);
	}

	InstanceScript* instance;
	uint8 arcaneCharges;                    // Charges accumulees par Arcane Blast

	bool OnGossipHello(Player* player) override
	{
		player->PrepareGossipMenu(me, GOSSIP_MENU_DEFAULT, true);
		player->SendPreparedGossip(me);
		return true;
	}

	// Le gossip est a usage unique : il distribue le bouclier a tous les
	// joueurs, credite le criteria puis se desactive definitivement.
	bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
	{
		ClearGossipMenuFor(player);

		switch (gossipListId)
		{
			case 0:
                me->SetVignette(VIGNETTE_NONE);
				me->RemoveAurasDueToSpell(SPELL_CHAT_BUBBLE);
				me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
				instance->DoCastSpellOnPlayers(SPELL_RUNIC_SHIELD);
				DoCast(SPELL_ARCANE_CAST_INSTANT);
				// Credite le criteria "parler a Rhonin" pour le joueur.
				KillRewarder::Reward(player, me);
				break;
		}

		CloseGossipMenuFor(player);
		return true;
	}

	// Gestion des charges arcaniques + combo Time Warp.
	void OnSpellCast(SpellInfo const* spell) override
	{
		switch (spell->Id)
		{
			case SPELL_ARCANE_BLAST:
				arcaneCharges++;
				// Temporal Displacement sert de "cooldown" au combo : tant
				// qu'il est actif, Time Warp ne peut pas se redeclencher.
				if (roll_chance(TIME_WARP_CHANCE) && !me->HasAura(SPELL_TEMPORAL_DISPLACEMENT))
				{
					CastStop();
					me->AddAura(SPELL_TIME_WARP, me);
					me->AddAura(SPELL_TEMPORAL_DISPLACEMENT, me);
					DoCastVictim(SPELL_ARCANE_BARRAGE);
				}
				break;
			case SPELL_ARCANE_BARRAGE:
				// Barrage depense toutes les charges accumulees.
				arcaneCharges = 0;
				break;
			default:
				break;
		}
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		// Bouclier surdimensionne : Rhonin doit survivre a toute la bataille.
		DoCastSelf(SPELL_PRISMATIC_BARRIER, CastSpellExtraArgs(SPELLVALUE_BASE_POINT0, 256E3));

		scheduler
			// Coeur de la rotation : on empile puis on depense.
			.Schedule(1s, GROUP_NORMAL, [this](TaskContext arcane_blast)
			{
				if (arcaneCharges < ARCANE_CHARGES_MAX)
					DoCastVictim(SPELL_ARCANE_BLAST);
				else
					DoCastVictim(SPELL_ARCANE_BARRAGE);
				arcane_blast.Repeat(2800ms);
			})
			// Surveillance du mana : Evocation des que la barre est trop basse.
			.Schedule(3s, GROUP_NORMAL, [this](TaskContext evocation)
			{
				if (me->GetPowerPct(POWER_MANA) < EVOCATION_MANA_PCT)
					DoCast(SPELL_EVOCATION);
				evocation.Repeat(3s);
			})
			.Schedule(2s, [this](TaskContext arcane_projectiles)
			{
				DoCastVictim(SPELL_ARCANE_SALVO);
				arcane_projectiles.Repeat(14s, 25s);
			})
			// Cristal arcanique : totem immobile et intouchable qui bouclier
			// les joueurs autour de lui pendant sa duree de vie.
			.Schedule(5s, GROUP_NORMAL, [this](TaskContext arcane_cristal)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				{
					const Position pos = target->GetRandomNearPosition(CRYSTAL_SPREAD_RADIUS);
					if (Creature* crystal = me->SummonCreature(NPC_ARCANIC_CRYSTAL, pos, TEMPSUMMON_TIMED_DESPAWN, 31s))
					{
						crystal->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
						crystal->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
						crystal->SetFaction(me->GetFaction());
						crystal->SetCanMelee(false);
						crystal->SetControlled(true, UNIT_STATE_ROOT);
						crystal->CastSpell(crystal, SPELL_ARCANE_AFFINITY);
						crystal->CastSpell(crystal, SPELL_SHIELD_PLAYERS, true);
					}
				}
				arcane_cristal.Repeat(1min);
			})
			// Arcane Pulse prend la priorite quand le corps a corps se remplit :
			// on decale GROUP_NORMAL pour lui laisser la place, sinon on
			// repasse quasi immediatement pour ne pas rater la fenetre.
			.Schedule(5s, GROUP_ARCANE_PULSE, [this](TaskContext arcane_pulse)
			{
				if (EnemiesInRange(ARCANE_PULSE_RANGE) >= ARCANE_PULSE_MIN_FOES)
				{
					scheduler.DelayGroup(GROUP_NORMAL, 5s);

					CastStop();
					DoCast(SPELL_ARCANE_PULSE);
					arcane_pulse.Repeat(2s);
				}
				else
					arcane_pulse.Repeat(5ms);
			});
	}

	// Path 1 : Rhonin quitte la place et "se teleporte" au sommet de la tour
	// ou il canalise le portail que les joueurs viendront rejoindre.
	void WaypointPathEnded(uint32 /*pointId*/, uint32 pathId) override
	{
		if (pathId == 1)
		{
			me->SetVisible(false);
			scheduler.Schedule(2s, [this](TaskContext /*context*/)
			{
				me->SetVisible(true);
				me->NearTeleportTo(RhoninPoint02);
				// Vignette pour guider les joueurs vers le sommet de la tour.
                me->SetVignette(VIGNETTE_RHONIN);
				DoCastSelf(SPELL_PORTAL_CHANNELING_03);
			});
		}
	}
};

// =========================================================================
// npc_kinndy_sparkshine - Kinndy Sparkshine
// =========================================================================
// PNJ purement scripte : aucune rotation de combat, seulement l'orientation
// finale de ses deux trajets de la scene du conseil.
struct npc_kinndy_sparkshine : public CustomAI
{
	// Orientations d'arrivee de chacun des deux trajets.
	static constexpr float FACING_PATH_01 = 4.62f;
	static constexpr float FACING_PATH_02 = 1.24f;

	npc_kinndy_sparkshine(Creature* creature) : CustomAI(creature, true, AI_Type::Stay)
	{
        SetCanRandomMovement(false);
    }

	// Path 1 : sortie de la salle du conseil (elle disparait en coulisses)
	// Path 2 : retour a sa place pour la scene suivante
	void WaypointPathEnded(uint32 /*pointId*/, uint32 pathId) override
	{
		if (pathId == 1)
		{
			me->SetFacingTo(FACING_PATH_01);
			me->SetVisible(false);
		}
		else if(pathId == 2)
		{
			me->SetFacingTo(FACING_PATH_02);
		}
	}
};

// =========================================================================
// npc_tari_cogg - Tari Cogg (mage arcanique)
// =========================================================================
// Caster statique (AI_Type::Stay) : elle ne se deplace jamais et n'attaque
// pas au corps a corps. Sa mecanique propre est l'Evocation defensive
// declenchee une seule fois par combat quand elle passe sous le seuil bas.
struct npc_tari_cogg : public CustomAI
{
	// Seuil de PV (en %) declenchant l'Evocation defensive.
	static constexpr uint8 EVOCATION_HEALTH_PCT = 20;
	// Portee de recherche d'une cible en train d'incanter (cible de Supernova).
	static constexpr float SUPERNOVA_RANGE      = 30.0f;

	npc_tari_cogg(Creature* creature) : CustomAI(creature, true, AI_Type::Stay), evocating(false)
	{
        SetCanRandomMovement(false);
    }

	enum Spells
	{
		SPELL_RUNIC_INTELLECT       = 51799,
		SPELL_SUPERNOVA             = 157980,
		SPELL_EVOCATION             = 211765,
		SPELL_ARCANE_BOLT           = 371306,
		SPELL_UNCONTROLLED_ENERGY   = 388951,
		SPELL_ARCANE_SALVO          = 378850,
	};

	bool evocating;                         // Evocation en cours (voir OnChannelFinished)

	void Reset() override
	{
		Initialize();

		summons.DespawnAll();
		scheduler.CancelAll();

		evocating = false;

		// Buff d'intelligence remis en place a chaque reset.
		scheduler.Schedule(1s, [this](TaskContext /*context*/)
		{
			DoCastSelf(SPELL_RUNIC_INTELLECT);
		});
	}

	// Tari ne se deplace pas et ne frappe pas : elle prend simplement sa
	// cible et reste plantee a caster.
	void AttackStart(Unit* who) override
	{
		if (!who)
			return;

		if (who && me->Attack(who, false))
		{
			me->GetMotionMaster()->Clear(MOTION_PRIORITY_NORMAL);
			me->PauseMovement();
			me->SetCanMelee(false);
		}
	}

	void EnterEvadeMode(EvadeReason why = EvadeReason::Other) override
	{
		CustomAI::EnterEvadeMode(why);

		// Ses zones au sol ne doivent pas survivre a la fin du combat.
		me->RemoveAllAreaTriggers();
	}

	// Immunisee aux effets des sorts recus : on neutralise le hook par defaut.
	void SpellHit(WorldObject* /*caster*/, SpellInfo const* /*spellInfo*/) override { }

	void OnChannelFinished(SpellInfo const* spell) override
	{
		// Une fois l'Evocation terminee, le declencheur redevient armable si
		// Tari repasse sous le seuil plus tard dans le combat.
		if (spell->Id == SPELL_EVOCATION)
			evocating = false;
	}

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
		if (me->HealthBelowPctDamaged(EVOCATION_HEALTH_PCT, damage))
		{
			if (evocating)
				return;

			evocating = true;

			// On interrompt tous les sorts
			CastStop();

			// On lance Evocation : les base points forcent le pourcentage de
			// mana et de PV rendus par tick, quel que soit le sort en BDD.
			DoCast(me, SPELL_EVOCATION,
				CastSpellExtraArgs(TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD)
				.AddSpellBP0(10)
				.AddSpellMod(SPELLVALUE_BASE_POINT1, 20));
		}
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		scheduler
			// Supernova sert d'interruption : on ne la lance que sur une
			// cible reellement en train d'incanter.
			.Schedule(5s, 8s, [this](TaskContext supernova)
			{
				if (Unit* target = DoSelectCastingUnit(SPELL_SUPERNOVA, SUPERNOVA_RANGE))
				{
					CastStop({ SPELL_ARCANE_SALVO });
					DoCast(target, SPELL_SUPERNOVA);
				}
				supernova.Repeat(10s, 15s);
			})
			.Schedule(10s, 15s, [this](TaskContext uncontrolled_energy)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				{
					CastStop({ SPELL_ARCANE_SALVO });
					me->CastSpell(target, SPELL_UNCONTROLLED_ENERGY);
				}
				uncontrolled_energy.Repeat(20s, 25s);
			})
			.Schedule(10s, 12s, [this](TaskContext arcane_salvo)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				{
					CastStop({ SPELL_ARCANE_SALVO });
					DoCast(target, SPELL_ARCANE_SALVO);
				}
				arcane_salvo.Repeat(8s, 14s);
			})
			// Filler
			.Schedule(2s, [this](TaskContext arcane_bolt)
			{
				DoCastVictim(SPELL_ARCANE_BOLT);
				arcane_bolt.Repeat(2800ms);
			});
	}
};

// =========================================================================
// npc_pained - Pained, garde du corps de Jaina
// =========================================================================
// Aucune mecanique de combat : elle ne sert qu'a declencher l'arrivee de
// Perith Stormhoove une fois sortie de la salle du conseil.
struct npc_pained : public ScriptedAI
{
	npc_pained(Creature* creature) : ScriptedAI(creature)
	{
		instance = me->GetInstanceScript();
	}

	InstanceScript* instance;

	// Path 1 : Pained quitte la salle, ce qui lance la scene du tauren inconnu.
	void WaypointPathEnded(uint32 /*pointId*/, uint32 pathId) override
	{
		if (pathId == 1)
		{
			me->SetVisible(false);
			instance->TriggerGameEvent(EVENT_THE_UNKNOWN_TAUREN);
		}
	}
};

// =========================================================================
// npc_kalecgos_theramore - Kalecgos, forme humanoide (mage de givre)
// =========================================================================
// Rotation givre orientee controle : Frozen Beam et Comet Storm sur cibles
// choisies, et Ice Nova reservee aux cibles en mouvement (anti-kiting).
// Path 1 le fait disparaitre dans un teleport puis une dissolution : c'est
// le moment ou il prend sa forme de dragon (npc_kalecgos_dragon).
struct npc_kalecgos_theramore : public CustomAI
{
	npc_kalecgos_theramore(Creature* creature) : CustomAI(creature, true)
	{
		instance = me->GetInstanceScript();

        SetCanRandomMovement(false);
    }

	enum Spells
	{
		SPELL_COMET_STORM           = 153595,
		SPELL_ICE_NOVA              = 157997,
		SPELL_DISSOLVE              = 255295,
		SPELL_FROSTBOLT             = 284703,
		SPELL_FROZEN_BEAM           = 391825,
		SPELL_TELEPORT              = 400542,
	};

	InstanceScript* instance;

	// Path 1 : sequence de sortie en deux temps (teleport puis dissolution)
	// Path 3 : retour a la table de banquet, il sort de scene
	void WaypointPathEnded(uint32 /*pointId*/, uint32 pathId) override
	{
		if (pathId == 1)
		{
			// Une seule tache repetee une fois : l'effet de teleport doit
			// s'afficher entierement (4800ms) avant la dissolution.
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
		}
		else if (pathId == 3)
		{
			me->SetVisible(false);
		}
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		scheduler
			.Schedule(8s, 10s, [this](TaskContext frozen_beam)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				{
					CastStop({ SPELL_FROZEN_BEAM });
					DoCast(target, SPELL_FROZEN_BEAM);
				}
				frozen_beam.Repeat(14s, 22s);
			})
			.Schedule(12s, 18s, [this](TaskContext comet_barrage)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
				{
					CastStop({ SPELL_FROZEN_BEAM });
					DoCast(target, SPELL_COMET_STORM);
				}
				comet_barrage.Repeat(18s, 25s);
			})
			// Anti-kiting : on cherche la premiere cible en mouvement dans la
			// liste de menace et on la gele. Si personne ne bouge, on
			// repasse chaque seconde jusqu'a trouver un candidat.
			.Schedule(5s, [this](TaskContext ice_nova)
			{
				for (auto* ref : me->GetThreatManager().GetUnsortedThreatList())
				{
					Unit* target = ref->GetVictim();
					if (target && target->isMoving())
					{
						CastStop();
						DoCast(target, SPELL_ICE_NOVA);
						DoCast(target, SPELL_COMET_STORM);
						ice_nova.Repeat(3s, 5s);
						return;
					}
				}
				ice_nova.Repeat(1s);
			});
	}
};

// =========================================================================
// npc_ziradormi_theramore - Ziradormi (PNJ utilitaire)
// =========================================================================
// Hors scenario : simple gossip qui renvoie le joueur au point de depart de
// l'instance. Enregistre avec RegisterCreatureAI (et non RegisterTheramoreAI)
// pour rester utilisable en dehors de l'InstanceScript.
struct npc_ziradormi_theramore : public CustomAI
{
	enum Misc
	{
		GOSSIP_MENU_DEFAULT = 65007,
	};

	// Destination du teleport : entree de l'instance (map 5000).
	static constexpr uint32 THERAMORE_MAP_ID = 5000;

	npc_ziradormi_theramore(Creature* creature) : CustomAI(creature)
	{
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
				player->TeleportTo(THERAMORE_MAP_ID, -3735.03f, -4425.95f, 30.55f, 0.f, TELE_REVIVE_AT_TELEPORT);
				break;
		}

		CloseGossipMenuFor(player);
		return true;
	}

	void Reset() override
	{
		CustomAI::Reset();

		// Bulle de dialogue permanente : signale qu'elle est interactible.
		me->AddAura(SPELL_CHAT_BUBBLE, me);
	}
};

// =========================================================================
// npc_kalecgos_dragon - Kalecgos, forme draconique
// =========================================================================
// Creature purement cosmetique qui tourne en rond au-dessus de Theramore
// pendant la bataille. Elle est pilotee entierement par l'InstanceScript
// via SetData(DATA_KALECGOS_*_EVENT).
//
// Le cercle de vol est reprogramme a chaque tour complet : m_loopTime est
// la duree d'un tour, calculee a partir du perimetre et de la vitesse de
// course, pour que le prochain MoveCirclePath tombe pile a la fin du
// precedent (sinon le mouvement se fige apres un tour).
struct npc_kalecgos_dragon : public CustomAI
{
	// Rayon du cercle de vol autour de TheramorePoint01.
	const float m_circleRadius = 95.0f;
	// Nombre de points d'interpolation du cercle (fluidite du survol).
	static constexpr uint8 CIRCLE_STEPS      = 16;
	// Chance de lacher une replique de combat a chaque fenetre.
	static constexpr uint32 COMBAT_TALK_CHANCE = 30;

	npc_kalecgos_dragon(Creature* creature) : CustomAI(creature), m_loopTime(0)
	{
		instance = me->GetInstanceScript();
        SetCanRandomMovement(false);
    }

	InstanceScript* instance;
	uint64 m_loopTime;                      // Duree d'un tour complet, en ms

	void Reset() override
	{
		CustomAI::Reset();

		// Recalculer m_loopTime au moment de DATA_KALECGOS_CIRCLE_EVENT
		// reglerait le probleme : a valider en jeu.
		float perimeter = 2.f * float(M_PI) * m_circleRadius;
		m_loopTime = (perimeter / me->GetSpeed(MOVE_RUN)) * 1000.f;
	}

	// Toutes les taches commencent par verifier isActiveObject : quand
	// DATA_KALECGOS_CANCEL_EVENT desactive la creature, les taches deja
	// planifiees s'arretent d'elles-memes sans se replanifier.
	void SetData(uint32 id, uint32 /*value*/) override
	{
		switch (id)
		{
			// Vol en cercle, relance a chaque tour complet.
			case DATA_KALECGOS_CIRCLE_EVENT:
			{
				scheduler.Schedule(1s, [this](TaskContext circle_path)
				{
                    if (!me->isActiveObject())
                        return;

					me->GetMotionMaster()->MoveCirclePath
					(
						TheramorePoint01.GetPositionX(),
						TheramorePoint01.GetPositionY(),
						TheramorePoint01.GetPositionZ(),
						m_circleRadius,
						true,
						CIRCLE_STEPS
					);
					circle_path.Repeat(Milliseconds(m_loopTime));
				});
				break;
			}
			// Arret complet : le survol est termine, la creature est retiree
			// de la liste des objets actifs.
			case DATA_KALECGOS_CANCEL_EVENT:
			{
				me->CastStop();
				me->GetMotionMaster()->Clear();
				me->GetMotionMaster()->MoveIdle();
                me->setActive(false);
                scheduler.CancelAll();
                break;
			}
		}
	}
};

// =========================================================================
// Registration
// =========================================================================
void AddSC_battle_for_theramore()
{
	// Utilisable en dehors de l'instance
	RegisterCreatureAI(npc_ziradormi_theramore);

	RegisterTheramoreAI(npc_jaina_theramore);
	RegisterTheramoreAI(npc_archmage_tervosh);
	RegisterTheramoreAI(npc_amara_leeson);
	RegisterTheramoreAI(npc_rhonin);
	RegisterTheramoreAI(npc_kinndy_sparkshine);
	RegisterTheramoreAI(npc_tari_cogg);
	RegisterTheramoreAI(npc_pained);
	RegisterTheramoreAI(npc_kalecgos_theramore);
	RegisterTheramoreAI(npc_kalecgos_dragon);
}
