/*
 * Battle for Theramore - AIs de figuration, troupes, sorts et areatriggers
 *
 * Complement de battle_for_theramore.cpp (qui gere les personnages nommes).
 * Le fichier est organise en cinq blocs :
 *
 *   1. ALLIANCE  : civils, tanks, blesses, et la hierarchie des troupes de
 *                  Theramore (npc_theramore_troop -> officier / footman /
 *                  arcanist / faithful / marksman)
 *   2. HORDE     : npc_theramore_horde -> hag / grunt / loasinger / felcaster,
 *                  plus le mini-boss npc_wave_caller_gruhta
 *   3. COSMETIC  : PNJ d'entrainement joues en boucle avant la bataille
 *   4. SPELLS    : SpellScripts custom utilises par ces AIs
 *   5. AREATRIGGERS : zones au sol des memes sorts
 *
 * Les classes de base (npc_theramore_troop, npc_theramore_horde) portent tout
 * ce qui est commun a un camp : credits de kill, appel a l'aide, emotes. Les
 * classes derivees n'apportent que leur rotation de sorts.
 *
 * Les AIs de la Horde sont enregistrees avec RegisterCreatureAI (et non
 * RegisterTheramoreAI) car elles sont aussi utilisees dans le scenario des
 * Ruines de Theramore, hors de cet InstanceScript.
 *
 * Commentaires en francais sans accents (encodage TC).
 */

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "KillRewarder.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PassiveAI.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "CustomAI.h"
#include "spell_warlock.h"
#include "battle_for_theramore.h"

// =========================================================================
//     ALLIANCE NPC
// =========================================================================

// -------------------------------------------------------------------------
// npc_theramore_citizen - Civil a evacuer (phase Evacuation)
// -------------------------------------------------------------------------
// Le joueur clique le civil (spellclick) pour valider un credit d'evacuation.
// Deux comportements ensuite, selon que le civil a un chemin de fuite ou non :
//   - avec chemin  : il se met a courir, parle et despawn au bout de 10s
//   - sans chemin  : il joue une emote de panique sur place
struct npc_theramore_citizen : public CustomAI
{
	enum Misc
	{
		// Spells
		SPELL_AFRAID                    = 123263,

		// Gossip
		GOSSIP_MENU_DEFAULT             = 65000,

		// Credits
		NPC_THERAMORE_CITIZEN_CREDIT    = 500005
	};

	enum Talks
	{
		SAY_THERAMORE_CITIZEN_FLEE      = 0,
		SAY_THERAMORE_CITIZEN_CRY       = 1,
		SAY_THERAMORE_CITIZEN_FEARED    = 2,
	};

	enum Path : uint32
	{
		WAYPOINT_PATH_01                = 4000031921,
		WAYPOINT_PATH_02                = 4000031922,
		WAYPOINT_PATH_03                = 4000031924,
		WAYPOINT_PATH_04                = 4000031923
	};

	// Chance de jouer l'emote "pleurs" plutot que "salut apeure" quand le
	// civil reste sur place.
	static constexpr uint32 CRY_EMOTE_CHANCE = 60;

	npc_theramore_citizen(Creature* creature) : CustomAI(creature, AI_Type::Stay) { }

	void OnSpellClick(Unit* clicker, bool spellClickHandled) override
	{
		if (!spellClickHandled)
			return;

		Player* player = clicker->ToPlayer();
		if (!player)
			return;

		// En debug, un seul civil suffit a valider toute l'etape.
		#ifdef CUSTOM_DEBUG
			for (uint8 i = 0; i < NUMBER_OF_CITIZENS; ++i)
				KillRewarder::Reward(player, me, NPC_THERAMORE_CITIZEN_CREDIT);
		#endif

		KillRewarder::Reward(player, me, NPC_THERAMORE_CITIZEN_CREDIT);

		// Le civil n'est plus une cible d'objectif : on nettoie ses marqueurs.
		me->SetVignette(VIGNETTE_NONE);
		me->SetEmoteState(EMOTE_STATE_NONE);
		me->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);

		// Sequence en deux temps : il se tourne vers le joueur, puis reagit.
		scheduler.Schedule(780ms, 1500ms, [this, player](TaskContext context)
		{
			switch (context.GetRepeatCounter())
			{
				case 0:
				{
					me->SetFacingToObject(player);
					// Retire les auras d'ambiance posees par le creature_addon
					// (peur, sanglots...) avant de jouer la reaction.
					if (CreatureAddon const* creatureAddon = me->GetCreatureAddon())
					{
						if (!creatureAddon->auras.empty())
						{
							for (auto aura : creatureAddon->auras)
								me->RemoveAurasDueToSpell(aura);
						}
					}
					context.Repeat(780ms, 1500ms);
					break;
				}
				case 1:
				{
					if (me->GetWaypointPathId())
					{
						// Ce civil a un chemin de fuite : il s'enfuit.
						me->SetWalk(false);
						me->ResumeMovement();
						me->DespawnOrUnsummon(10s);
						me->AI()->Talk(SAY_THERAMORE_CITIZEN_FLEE);
					}
					else if (roll_chance(CRY_EMOTE_CHANCE))
					{
						me->HandleEmoteCommand(RAND(EMOTE_STATE_CRY, EMOTE_STATE_COWER));
						me->AI()->Talk(SAY_THERAMORE_CITIZEN_CRY);
					}
					else
					{
						me->HandleEmoteCommand(RAND(EMOTE_ONESHOT_WAVE, EMOTE_ONESHOT_NO));
						me->AI()->Talk(SAY_THERAMORE_CITIZEN_FEARED);
					}
					break;
				}
			}
		});
	}
};

// -------------------------------------------------------------------------
// npc_unmanned_tank - Tank a reparer (phase ALittleHelp)
// -------------------------------------------------------------------------
// Spawne endommage (l'InstanceScript lui retire une part aleatoire de ses PV)
// et marque comme interactible. Un sort de reparation le remet en service ;
// il tire ensuite au canon pendant la bataille.
struct npc_unmanned_tank : public CustomAI
{
	npc_unmanned_tank(Creature* creature) : CustomAI(creature, true, AI_Type::Stay)
	{
	}

	enum Spells
	{
		SPELL_DEMOLISHER_CANNON = 271246
	};

	// La reparation retire le marqueur d'objectif : le tank est repare.
	void SpellHit(WorldObject* /*caster*/, SpellInfo const* spell) override
	{
		if (spell->Id != SPELL_REPAIR)
			return;

		me->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
		me->SetVignette(VIGNETTE_NONE);
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		scheduler.Schedule(1s, [this](TaskContext canon)
		{
			if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				DoCast(target, SPELL_DEMOLISHER_CANNON);
			canon.Repeat(10s, 24s);
		});
	}
};

// -------------------------------------------------------------------------
// npc_wounded_theramore_troop - Blesse a evacuer (phase HelpTheWounded)
// -------------------------------------------------------------------------
// Le joueur lance un sort de teleport sur le blesse pour l'evacuer. Le
// compteur DATA_WOUNDED_TROOPS est tenu par l'InstanceScript : tous les
// blesses despawnent en silence sauf le dernier, qui declenche la replique
// de remerciement de Jaina.
struct npc_wounded_theramore_troop : public ScriptedAI
{
	npc_wounded_theramore_troop(Creature* creature) : ScriptedAI(creature),
		preventClick(false)
	{
		instance = creature->GetInstanceScript();
	}

	enum Spells
	{
		SPELL_TELEPORT_TROOP = 69074
	};

	InstanceScript* instance;
	bool preventClick;                      // Anti double-comptage : un blesse ne compte qu'une fois

	void SpellHit(WorldObject* caster, SpellInfo const* spell) override
	{
		if (spell->Id != SPELL_TELEPORT_TROOP)
			return;

		// Deux joueurs peuvent lancer le sort en meme temps : on ne credite
		// qu'une seule fois par blesse.
		if (preventClick)
			return;

		if (Player* player = caster->ToPlayer())
		{
			#ifdef CUSTOM_DEBUG
				for (uint8 i = 0; i < NUMBER_OF_WOUNDED; i++)
					KillRewarder::Reward(player, me, NPC_THERAMORE_WOUNDED_TROOP);
			#else
				KillRewarder::Reward(player, me, NPC_THERAMORE_WOUNDED_TROOP);
			#endif
		}

		me->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);

		uint32 counter = instance->GetData(DATA_WOUNDED_TROOPS);

		// Tous les blesses sauf le dernier partent discretement ; le dernier
		// reste en place et declenche la replique de Jaina.
		if (counter < NUMBER_OF_WOUNDED - 1)
		{
			me->DespawnOrUnsummon();
		}
		else
		{
			if (Player* player = caster->ToPlayer())
			{
				me->PlayDistanceSound(SOUND_COUNTERSPELL, player);
				if (Creature* jaina = instance->GetCreature(DATA_JAINA_PROUDMOORE))
					jaina->AI()->Talk(SAY_WOUNDED_TROOP, player);
			}
		}

		counter += 1;

		instance->SetData(DATA_WOUNDED_TROOPS, counter);

		preventClick = true;
	}
};

// -------------------------------------------------------------------------
// npc_theramore_troop - Classe de base des troupes de Theramore
// -------------------------------------------------------------------------
// Porte tout ce qui est commun aux cinq types de troupe : appel a l'aide au
// pull, et surtout la mecanique de motivation de la phase Preparation.
//
// Motivation : un joueur fait l'emote /fortheAlliance a cote d'une troupe.
// Cette troupe propage alors la reaction a toutes les troupes voisines
// (rayon EMOTE_EFFECT_RANGE) via le trio SetGUID / SetData / DoAction. Chaque
// troupe touchee joue une sequence en quatre temps (se tourner, acclamer,
// crier, revenir a sa position) et credite le joueur une seule fois grace au
// drapeau emoteReceived.
struct npc_theramore_troop : public CustomAI
{
	npc_theramore_troop(Creature* creature, AI_Type type)
		: CustomAI(creature, true, type),
		  instance(creature->GetInstanceScript()),
		  soundEmote(creature->GetGender() == GENDER_FEMALE ? SOUND_CHEER_FEMALE : SOUND_CHEER_MALE),
		  emoteReceived(false)
	{
	}

	enum Misc
	{
		NPC_THERAMORE_TROOPS_CREDIT = 500011,
		ACTION_RECEIVE_EMOTE        = 1
	};

	// Cris d'acclamation, choisis selon le genre de la creature.
	static constexpr uint32 SOUND_CHEER_FEMALE = 74679;
	static constexpr uint32 SOUND_CHEER_MALE   = 74681;

	// Rayon de propagation de la reaction aux troupes voisines.
	static constexpr float EMOTE_EFFECT_RANGE  = 8.f;
	// Distance maximale entre le joueur et la troupe pour que l'emote compte.
	static constexpr float EMOTE_TRIGGER_RANGE = 3.5f;

	// Entries parcourues pour trouver les voisines a faire reagir.
	static constexpr uint32 TroopEntries[] =
	{
		NPC_THERAMORE_FOOTMAN,
		NPC_THERAMORE_FAITHFUL,
		NPC_THERAMORE_ARCANIST,
		NPC_THERAMORE_OFFICER,
		NPC_THERAMORE_MARKSMAN
	};

	InstanceScript* instance;
	ObjectGuid playerGuid;                  // Joueur a remercier (pose par SetGUID)
	uint32 soundEmote;                      // Son d'acclamation selon le genre
	bool emoteReceived;                     // Cette troupe a deja ete motivee

	void JustEngagedWith(Unit* /*who*/) override
	{
		me->CallAssistance();
	}

	void SetData(uint32 id, uint32 value) override
	{
		if (id == NPC_THERAMORE_TROOPS_CREDIT)
			emoteReceived = (value != 0);
	}

	void SetGUID(ObjectGuid const& guid, int32 id) override
	{
		if (id == NPC_THERAMORE_TROOPS_CREDIT)
			playerGuid = guid;
	}

	// Sequence de remerciement, jouee par chaque troupe a portee du joueur.
	void DoAction(int32 param) override
	{
		if (param != ACTION_RECEIVE_EMOTE)
			return;

		Player* player = ObjectAccessor::GetPlayer(*me, playerGuid);
		if (!player)
			return;

		// Memorise l'orientation d'origine pour la restaurer a la fin.
		float const orientation = me->GetOrientation();
		scheduler.Schedule(1ms, 1s, [this, player, orientation](TaskContext context)
		{
			switch (context.GetRepeatCounter())
			{
				case 0:
					me->SetFacingToObject(player);
					context.Repeat(800ms, 1s);
					break;
				case 1:
					me->HandleEmoteCommand(EMOTE_ONESHOT_CHEER_FORTHEALLIANCE);
					KillRewarder::Reward(player, me, NPC_THERAMORE_TROOPS_CREDIT);
					context.Repeat(1ms, 2s);
					break;
				case 2:
					me->PlayDistanceSound(soundEmote, player);
					context.Repeat(3s, 5s);
					break;
				case 3:
					me->SetFacingTo(orientation);
					context.CancelAll();
					return;
			}
		});
	}

	// Point d'entree de la mecanique : uniquement pendant la preparation.
	void ReceiveEmote(Player* player, uint32 emoteId) override
	{
		BFTPhases const phase = static_cast<BFTPhases>(instance->GetData(DATA_SCENARIO_PHASE));
		if (phase != BFTPhases::Preparation && phase != BFTPhases::Preparation_Rhonin)
			return;

#ifdef CUSTOM_DEBUG
		for (uint8 i = 0; i < NUMBER_OF_TROOPS; ++i)
			KillRewarder::Reward(player, me, NPC_THERAMORE_TROOPS_CREDIT);
		return;
#else
		if (emoteId != TEXT_EMOTE_FORTHEALLIANCE || emoteReceived)
			return;

		if (!player->IsWithinDist(me, EMOTE_TRIGGER_RANGE))
			return;

		// Propagation : toutes les troupes voisines reagissent ensemble.
		std::list<Creature*> troops;
		for (uint32 entry : TroopEntries)
			me->GetCreatureListWithEntryInGrid(troops, entry, EMOTE_EFFECT_RANGE);

		for (Creature* troop : troops)
		{
			troop->AI()->SetGUID(player->GetGUID(), NPC_THERAMORE_TROOPS_CREDIT);
			troop->AI()->SetData(NPC_THERAMORE_TROOPS_CREDIT, 1U);
			troop->AI()->DoAction(ACTION_RECEIVE_EMOTE);
		}
#endif
	}
};

// -------------------------------------------------------------------------
// npc_thader_windermere - Archimage Thader Windermere
// -------------------------------------------------------------------------
// Gossip a usage unique (phase Preparation) : parler a Thader lance une
// sequence en trois temps ou lui puis Kinndy canalisent le portail, ce qui
// finit par ouvrir la barriere mystique 02.
struct npc_thader_windermere : public CustomAI
{
	enum Misc
	{
		GOSSIP_MENU_DEFAULT = 65002,
	};

	npc_thader_windermere(Creature* creature) : CustomAI(creature, true)
	{
		instance = creature->GetInstanceScript();
	}

	InstanceScript* instance;

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
				// Le gossip ne sert qu'une fois : on retire tous ses marqueurs.
				me->SetVignette(VIGNETTE_NONE);
				me->RemoveAurasDueToSpell(SPELL_CHAT_BUBBLE);
				me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
				KillRewarder::Reward(player, me);
				scheduler.Schedule(2s, [this](TaskContext context)
				{
					switch (context.GetRepeatCounter())
					{
						case 0: // Thader amorce la canalisation
							me->CastSpell(me, SPELL_PORTAL_CHANNELING_03);
							context.Repeat(1s);
							break;
						case 1: // Kinndy prend le relais
							if (Creature* kinndy = instance->GetCreature(DATA_KINNDY_SPARKSHINE))
								kinndy->CastSpell(kinndy, SPELL_PORTAL_CHANNELING_01);
							context.Repeat(1800ms);
							break;
						case 2: // La barriere s'ouvre, la sequence s'arrete
							if (GameObject* barrier = instance->GetGameObject(DATA_MYSTIC_BARRIER_02))
								barrier->UseDoorOrButton();
							context.CancelAll();
							break;
					}
				});
				break;
		}

		CloseGossipMenuFor(player);
		return true;
	}
};

// -------------------------------------------------------------------------
// npc_hedric_evencane - Hedric Evencane (garde d'elite)
// -------------------------------------------------------------------------
// Guerrier de melee classique. Sa particularite est scriptee : a la fin de
// son second chemin il enchaine sur un MovePoint vers HedricPoint03, ou il
// prend sa position de garde definitive (arme rangee, emote de faction).
struct npc_hedric_evencane : public CustomAI
{
	// Portee de detection d'un ennemi en train d'incanter, pour Batter.
	static constexpr float BATTER_RANGE = 5.0f;

	npc_hedric_evencane(Creature* creature) : CustomAI(creature, true, AI_Type::Melee)
	{
	}

	enum Spells
	{
		SPELL_BATTER                = 66408,
		SPELL_RISING_ANGER          = 136323,
		SPELL_MORTAL_CLEAVE         = 177147,
		SPELL_WHIRLWIND             = 277637,
		SPELL_HEW                   = 319957,
		SPELL_VICIOUS_WOUND         = 334960
	};

	void JustEngagedWith(Unit* /*who*/) override
	{
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
			// Interrupt de melee sur un ennemi en train d'incanter.
			.Schedule(5s, [this](TaskContext batter)
			{
				if (Unit* target = DoSelectCastingUnit(SPELL_BATTER, BATTER_RANGE))
					DoCast(target, SPELL_BATTER);
				batter.Repeat(15s, 20s);
			})
			// Buff de rage, une seule fois par combat (pas de Repeat).
			.Schedule(1min, [this](TaskContext /*rising_anger*/)
			{
				DoCast(SPELL_RISING_ANGER);
			});
	}

	// Path 2 : fin du trajet vers la place -> il termine a pied jusqu'a son
	// poste de garde (HedricPoint03), signale par MOVEMENT_INFO_POINT_02.
	void WaypointPathEnded(uint32 /*pointId*/, uint32 pathId) override
	{
		if (pathId == 2)
		{
			me->StopMoving();
			me->GetMotionMaster()->Clear();
			me->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_02, HedricPoint03, true, HedricPoint03.GetOrientation());
		}
	}

	void MovementInform(uint32 type, uint32 id) override
	{
		if (type == EFFECT_MOTION_TYPE || type == POINT_MOTION_TYPE)
		{
			switch (id)
			{
				// Recul devant le portail de Dalaran : on le retourne face au portail.
				case MOVEMENT_INFO_POINT_01:
					me->SetFacingTo(HedricPoint02.GetOrientation());
					break;
				// Poste de garde atteint : arme rangee, posture de garde, et la
				// position devient son point de retour d'evade.
				case MOVEMENT_INFO_POINT_02:
					me->SetSheath(SHEATH_STATE_UNARMED);
					me->SetEmoteState(EMOTE_STATE_WAGUARDSTAND01);
					me->SetHomePosition(HedricPoint03);
					break;
				default:
					break;
			}
		}
	}
};

// -------------------------------------------------------------------------
// npc_theramore_officier - Officier de Theramore (paladin hybride)
// -------------------------------------------------------------------------
// Paladin heal / DPS : il soigne ses allies tout en frappant, avec deux
// passifs qui declenchent des Word of Glory gratuits (Shining Light et
// Afterimage), et une bulle d'urgence sous DamageTaken.
struct npc_theramore_officier : public npc_theramore_troop
{
	// Seuils de la bulle d'urgence.
	static constexpr float DIVINE_SHIELD_HEALTH_PCT   = 25.f;
	static constexpr uint32 DIVINE_SHIELD_CHANCE      = 30;
	// Chances des cooldowns opportunistes.
	static constexpr uint32 AVENGING_WRATH_CHANCE     = 60;
	static constexpr uint32 HOLY_SHOCK_HEAL_CHANCE    = 60;
	// Nombre d'allies blesses devant soi requis pour rentabiliser Light of Dawn.
	static constexpr uint32 LIGHT_OF_DAWN_MIN_FRIENDS = 3;
	// Portees de selection des cibles.
	static constexpr float HEAL_RANGE                 = 40.f;
	static constexpr float LIGHT_OF_DAWN_RANGE        = 15.f;
	static constexpr float AVENGER_SHIELD_RANGE       = 30.f;
	static constexpr float AFTERIMAGE_HEAL_RANGE      = 30.f;
	// Divine Storm : nombre d'ennemis au contact requis.
	static constexpr float DIVINE_STORM_RANGE         = 8.f;
	static constexpr uint32 DIVINE_STORM_MIN_ENEMIES  = 3;
	// Nombre de Blessed Hammer lances en rafale (1 par seconde).
	static constexpr uint8 BLESSED_HAMMER_COUNT       = 3;

	npc_theramore_officier(Creature* creature) : npc_theramore_troop(creature, AI_Type::Melee)
    {
        // Le nombre de stacks requis pour declencher Afterimage est lu
        // directement dans l'effet 2 du sort passif, pas code en dur.
        SpellInfo const* afterimageInfo = sSpellMgr->GetSpellInfo(SPELL_AFTERIMAGE, DIFFICULTY_NONE);
        afterimageAmount = afterimageInfo->GetEffect(EFFECT_2).CalcValue(me);
    }

	// Groupes de tasks : permet de retarder en bloc les routines DPS/Heal
	// quand l'officier passe sous Divine Shield pour caster un Holy Light d'urgence.
	enum Groups
	{
		GROUP_NORMAL,
		GROUP_HEAL,
		GROUP_DIVINE_SHIELD
	};

	enum Spells
	{
		SPELL_DIVINE_SHIELD         = 642,
		SPELL_HOLY_SHOCK            = 20473,
		SPELL_JUDGMENT              = 20271,
		SPELL_AVENGER_SHIELD        = 31935,
		SPELL_WORD_OF_GLORY         = 85673,
		SPELL_FLASH_OF_LIGHT        = 283634,
		SPELL_AVENGING_WRATH        = 292266,
		SPELL_HOLY_LIGHT            = 295698,
		SPELL_LIGHT_OF_DAWN         = 295710,
		SPELL_BLESSING_OF_FREEDOM   = 299256,
		SPELL_SHIELD_RIGHTEOUS      = 337629,
		SPELL_DIVINE_STORM          = 357849,
		SPELL_REBUKE                = 405397,
		SPELL_BLESSED_HAMMER        = 420092,

		SPELL_SHINING_LIGHT_BUFF    = 327510,

		SPELL_AFTERIMAGE            = 385414,
		SPELL_AFTERIMAGE_BUFF       = 400745
	};

    uint8 afterimageAmount;

    CastSpellExtraArgs WORD_OF_GLORY_FLAGS = CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_IGNORE_POWER_COST);

	void SpellHit(WorldObject* /*caster*/, SpellInfo const* spell) override
	{
        // Passive
		if (spell->Id == SPELL_SHINING_LIGHT_BUFF)
		{
			scheduler.Schedule(2s, 5s, [this](TaskContext /*context*/)
			{
				DoCastSelf(SPELL_WORD_OF_GLORY, WORD_OF_GLORY_FLAGS);
			});
		}

        // Passive
		if (spell->Id == SPELL_AFTERIMAGE_BUFF)
		{
			Aura* aura = me->GetAura(SPELL_AFTERIMAGE_BUFF);
			if (aura && aura->GetStackAmount() >= afterimageAmount)
			{
				scheduler.Schedule(2s, 3s, [this](TaskContext /*context*/)
				{
					// Verifie si une cible est la, sinon on fait rien
					if (Unit* target = FindLowestHealthFriend(me, AFTERIMAGE_HEAL_RANGE))
					{
						if (Aura* afterimage = me->GetAura(SPELL_AFTERIMAGE_BUFF))
						{
							DoCast(target, SPELL_WORD_OF_GLORY, WORD_OF_GLORY_FLAGS);
							DoCastSelf(SPELL_WORD_OF_GLORY, WORD_OF_GLORY_FLAGS);
						}
					}
				});
			}
		}

		// Anti-kite : Blessing of Freedom sur root/snare
		if (HasMechanic(spell, MECHANIC_ROOT) || HasMechanic(spell, MECHANIC_SNARE))
		{
			scheduler.Schedule(1s, 2s, [this](TaskContext /*blessing*/)
			{
				DoCastSelf(SPELL_BLESSING_OF_FREEDOM);
			});
		}
	}

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spell*/) override
	{
		// HasCooldown attend un spellId - ici Divine Shield, pas l'enum de groupe.
		if (me->GetSpellHistory()->HasCooldown(SPELL_DIVINE_SHIELD))
			return;

		if (me->HealthBelowPctDamaged(DIVINE_SHIELD_HEALTH_PCT, damage) && roll_chance(DIVINE_SHIELD_CHANCE))
		{
			DoCastSelf(SPELL_DIVINE_SHIELD);

			// On gele DPS et heals pendant la bulle pour eviter qu'un cast en cours
			// reprenne la main avant le Holy Light d'urgence.
			scheduler.DelayGroup(GROUP_NORMAL, 4s);
			scheduler.DelayGroup(GROUP_HEAL, 4s);

			scheduler.Schedule(1s, GROUP_DIVINE_SHIELD, [this](TaskContext /*context*/)
			{
				// Heal d'urgence pleine vie : on injecte MaxHealth en BP0 pour
				// surcharger la valeur du sort - intentionnel, fait office de full heal sous bulle.
				CastSpellExtraArgs args;
				args.AddSpellBP0(me->GetMaxHealth());

				CastStop();
                DoCastSelf(SPELL_HOLY_LIGHT, args);
			});
		}
	}

	void JustEngagedWith(Unit* who) override
	{
		npc_theramore_troop::JustEngagedWith(who);

		// Ouverture : Judgment immediat
		DoCast(who, SPELL_JUDGMENT);

		scheduler
			// -------- Cooldowns offensifs --------
			.Schedule(1s, GROUP_NORMAL, [this](TaskContext avenging_wrath)
			{
				// Avenging Wrath en boucle sur son CD reel (~2min) pour les combats longs.
				if (roll_chance(AVENGING_WRATH_CHANCE))
					DoCastSelf(SPELL_AVENGING_WRATH);
				avenging_wrath.Repeat(2min, 3min);
			})
			// -------- Heals --------
			.Schedule(5s, 8s, GROUP_HEAL, [this](TaskContext holy_light)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(HEAL_RANGE, 60))
				{
					CastStop(SPELL_FLASH_OF_LIGHT);
					DoCast(target, SPELL_HOLY_LIGHT);
				}
				holy_light.Repeat(8s, 14s);
			})
			.Schedule(1s, 2s, GROUP_HEAL, [this](TaskContext flash_of_light)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(HEAL_RANGE, 80))
				{
					CastStop(SPELL_HOLY_LIGHT);
					DoCast(target, SPELL_FLASH_OF_LIGHT);
				}
				flash_of_light.Repeat(5s, 8s);
			})
			.Schedule(8s, 11s, GROUP_HEAL, [this](TaskContext light_of_dawn)
			{
				// AOE heal en cone : on attend qu'au moins LIGHT_OF_DAWN_MIN_FRIENDS allies
				// soient blesses devant pour rentabiliser le cast.
				if (FriendsInFront(LIGHT_OF_DAWN_RANGE, 30) >= LIGHT_OF_DAWN_MIN_FRIENDS)
				{
					CastStop();
					DoCastSelf(SPELL_LIGHT_OF_DAWN);
					light_of_dawn.Repeat(10s, 15s);
				}
				else
					light_of_dawn.Repeat(1s);
			})
			// -------- Controle / Interrupt --------
			.Schedule(5ms, GROUP_NORMAL, [this](TaskContext rebuke)
			{
				// Interrupt melee : casser meme un heal en cours pour stop un cast adverse.
				if (Unit* target = DoSelectCastingUnit(SPELL_REBUKE, me->GetCombatReach()))
				{
					CastStop();
					DoCast(target, SPELL_REBUKE);
					rebuke.Repeat(25s, 40s);
				}
				else
					rebuke.Repeat(1s);
			})
			.Schedule(5ms, GROUP_NORMAL, [this](TaskContext avenger_shield)
			{
				// Silence ranged : meme priorite que Rebuke sur les casters ennemis.
				if (Unit* target = DoSelectCastingUnit(SPELL_AVENGER_SHIELD, AVENGER_SHIELD_RANGE))
				{
					CastStop({ SPELL_HOLY_LIGHT, SPELL_FLASH_OF_LIGHT });
					DoCast(target, SPELL_AVENGER_SHIELD);
					avenger_shield.Repeat(15s, 23s);
				}
				else
					avenger_shield.Repeat(1s);
			})
			// -------- Rotation DPS / Hybrides --------
			.Schedule(20s, GROUP_NORMAL, [this](TaskContext holy_shock)
			{
				// Holy Shock est hybride : heal sur allie blesse en priorite, sinon damage sur la victime.
				CastStop();
				if (Unit* target = DoSelectBelowHpPctFriendly(HEAL_RANGE, 90);
					target && roll_chance(HOLY_SHOCK_HEAL_CHANCE))
					DoCast(target, SPELL_HOLY_SHOCK);
				else
					DoCastVictim(SPELL_HOLY_SHOCK);

				holy_shock.Repeat(3s, 8s);
			})
			.Schedule(8s, 14s, GROUP_NORMAL, [this](TaskContext divine_storm)
			{
				if (EnemiesInRange(DIVINE_STORM_RANGE) >= DIVINE_STORM_MIN_ENEMIES)
				{
					CastStop({ SPELL_HOLY_LIGHT, SPELL_FLASH_OF_LIGHT });
					DoCast(SPELL_DIVINE_STORM);
					divine_storm.Repeat(35s, 42s);
				}
				else
					divine_storm.Repeat(1s);
			})
			.Schedule(14s, 22s, GROUP_NORMAL, [this](TaskContext blessed_hammer)
			{
				for (uint8 i = 0; i < BLESSED_HAMMER_COUNT; ++i)
				{
					scheduler.Schedule(i * 1s, [this](TaskContext /*context*/)
					{
						CastBlessedHammer();
					});
				}

				blessed_hammer.Repeat(8s, 14s);
			})
			.Schedule(2s, 8s, GROUP_NORMAL, [this](TaskContext shield_righteous)
			{
				DoCastVictim(SPELL_SHIELD_RIGHTEOUS);
				shield_righteous.Repeat(2s, 5s);
			})
			.Schedule(2s, 8s, GROUP_NORMAL, [this](TaskContext judgment)
			{
				// Judgment sur la cible la plus eloignee : applique le debuff aux kiteurs/casters.
				if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
					DoCast(target, SPELL_JUDGMENT);
				judgment.Repeat(8s, 14s);
			});
	}

private:

	/// Lance Blessed Hammer
	void CastBlessedHammer()
	{
		CastStop();
		me->GetSpellHistory()->ResetCooldown(SPELL_BLESSED_HAMMER);
		DoCastAOE(SPELL_BLESSED_HAMMER, TRIGGERED_IGNORE_GCD);
	}
};

// -------------------------------------------------------------------------
// npc_theramore_footman - Fantassin de Theramore (guerrier Fureur)
// -------------------------------------------------------------------------
// Rotation guerrier complete. Les commentaires en fin de ligne de l'enum
// Spells rappellent la cible attendue de chaque sort.
// Meme rotation que npc_roknah_grunt cote Horde.
struct npc_theramore_footman : public npc_theramore_troop
{
	npc_theramore_footman(Creature* creature) : npc_theramore_troop(creature, AI_Type::Melee) {}

	enum Spells
	{
		SPELL_BLOODTHIRST       = 23881,    // Victim
		SPELL_SPELL_REFLECTION  = 23920,    // UnitCasting (target Self)
		SPELL_RECKLESSNESS      = 122354,   // Buff
		SPELL_HAMSTRING         = 198374,   // Victim
		SPELL_BLADESTORM        = 235661,   // Enemies in range > 3
		SPELL_PUMMEL            = 265431,   // UnitCasting
		SPELL_SUDDEN_DEATH_BUFF = 280721,   // Buff casted by talent (dont use)
		SPELL_MORTAL_STRIKE     = 283410,   // MostMissingEnemy
		SPELL_WHIRLWIND         = 283412,   // Enemies in range >= 2
		SPELL_REND              = 283419,   // Victim
		SPELL_EXECUTE           = 1231790,  // On Victim when SUDDEN_DEATH_BUFF is up (check 1s to 2s)
	};

	void InitializeAI() override
	{
		me->SetOverrideDisplayPowerId(237);
		ScriptedAI::InitializeAI();
	}

	static constexpr uint32  BLADESTORM_MIN_ENEMIES = 3;   // Ennemis au contact requis pour Bladestorm
	static constexpr uint32  WHIRLWIND_MIN_ENEMIES  = 2;   // Ennemis au contact requis pour Whirlwind
	static constexpr uint8   RECKLESSNESS_HP_PCT    = 30;  // Seuil de PV declenchant Recklessness

	void JustEngagedWith(Unit* /*who*/) override
	{
		scheduler

			// --- Recklessness ---
			// Buff offensif de dernier recours : on sonde chaque seconde et on ne
			// le lance qu'une seule fois, quand les PV passent sous 30%.
			.Schedule(1ms, [this](TaskContext recklessness)
			{
				if (HealthBelowPct(RECKLESSNESS_HP_PCT)
					&& !me->HasAura(SPELL_RECKLESSNESS))
				{
					DoCastSelf(SPELL_RECKLESSNESS);
					return;
				}
				recklessness.Repeat(1s);
			})

			// --- Rend ---
			// DoT sur la victime, toutes les 10-14s.
			.Schedule(3s, 6s, [this](TaskContext rend)
			{
				DoCastVictim(SPELL_REND);
				rend.Repeat(10s, 14s);
			})

			// --- Bloodthirst ---
			// Frappe offensive de base, toutes les 5-8s.
			.Schedule(1ms, [this](TaskContext bloodthirst)
			{
				DoCastVictim(SPELL_BLOODTHIRST);
				bloodthirst.Repeat(5s, 8s);
			})

			// --- Hamstring ---
			// Ralentit la cible, toutes les 8-12s.
			.Schedule(5s, 9s, [this](TaskContext hamstring)
			{
				DoCastVictim(SPELL_HAMSTRING);
				hamstring.Repeat(8s, 12s);
			})

			// --- Mortal Strike ---
			// Frappe puissante sur la victime, toutes les 6-8s.
			.Schedule(4s, 7s, [this](TaskContext mortal_strike)
			{
				DoCastVictim(SPELL_MORTAL_STRIKE);
				mortal_strike.Repeat(6s, 8s);
			})

			// --- Whirlwind ---
			// AOE melee si 2+ ennemis a portee, toutes les 10-15s.
			.Schedule(10s, 15s, [this](TaskContext whirlwind)
			{
				if (EnemiesInRange(MELEE_RANGE) >= WHIRLWIND_MIN_ENEMIES)
				{
					DoCastSelf(SPELL_WHIRLWIND);
					whirlwind.Repeat(10s, 15s);
				}
				else
					whirlwind.Repeat(2s);
			})

			// --- Bladestorm ---
			// Tourbillon devastateur si 3+ ennemis a portee, toutes les 30-45s.
			.Schedule(20s, 30s, [this](TaskContext bladestorm)
			{
				if (EnemiesInRange(MELEE_RANGE) >= BLADESTORM_MIN_ENEMIES)
				{
					DoCastSelf(SPELL_BLADESTORM);
					bladestorm.Repeat(30s, 45s);
				}
				else
					bladestorm.Repeat(2s);
			})

			// --- Pummel ---
			// Interrompt un ennemi qui cast a portee melee. Re-check toutes les 1s, puis 15-18s CD.
			.Schedule(1s, [this](TaskContext pummel)
			{
				if (Unit* target = DoSelectCastingUnit(SPELL_PUMMEL, MELEE_RANGE))
				{
					DoCast(target, SPELL_PUMMEL);
					pummel.Repeat(15s, 18s);
				}
				else
					pummel.Repeat(1s);
			})

			// --- Spell Reflection ---
			// Buff defensif pose sur soi-meme lorsqu'un ennemi cast a portee melee. Re-check toutes les 1s.
			.Schedule(1s, [this](TaskContext spell_reflection)
			{
				if (!me->HasAura(SPELL_SPELL_REFLECTION) && DoSelectCastingUnit(SPELL_SPELL_REFLECTION, MELEE_RANGE))
				{
					DoCastSelf(SPELL_SPELL_REFLECTION);
					spell_reflection.Repeat(20s, 25s);
				}
				else
					spell_reflection.Repeat(1s);
			})

			// --- Execute ---
			// Consomme le buff Sudden Death sur la victime. Re-check toutes les 1-2s.
			.Schedule(1s, [this](TaskContext execute)
			{
				if (me->HasAura(SPELL_SUDDEN_DEATH_BUFF))
					DoCastVictim(SPELL_EXECUTE);
				execute.Repeat(1s, 2s);
			});
	}
};

// -------------------------------------------------------------------------
// npc_theramore_arcanist - Arcaniste de Theramore (mage Arcane)
// -------------------------------------------------------------------------
// Caster a distance qui empile des charges arcaniques et les depense en
// Arcane Barrage. Le type d'AI est parametrable : npc_arcanist_training en
// derive avec AI_Type::Stay pour la scene d'entrainement.
struct npc_theramore_arcanist : public npc_theramore_troop
{
	npc_theramore_arcanist(Creature* creature, AI_Type type = AI_Type::Distance) : npc_theramore_troop(creature, type), arcaneCharges(0)
	{
	}

	enum Spells
	{
		SPELL_ARCANE_MISSILES           = 5143,
		SPELL_ARCANE_BARRAGE            = 44425,
		SPELL_MAGE_ARMOR                = 183079,
		SPELL_CLEARCASTING_BUFF         = 263725,
		SPELL_ARCANE_BLAST              = 291336,
		SPELL_TOUCH_OF_THE_MAGI         = 321507,
		SPELL_TOUCH_OF_THE_MAGI_BUFF    = 210824,
		SPELL_MASS_POLYMORPH            = 383121,
		SPELL_ARCANE_TEMPO              = 383997,
		SPELL_ARCANE_EXPLOSION          = 414381,
		SPELL_ARCANE_ORB                = 440458
	};

	static constexpr int32  ARCANE_BARRAGE_MIN_CHARGES  = 4;        // Charges requises pour Barrage
	static constexpr float  AOE_RANGE                   = 10.0f;    // Distance de detection AOE
	static constexpr uint32 MASS_POLYMORPH_THRESHOLD    = 4;        // Au-dela de N ennemis -> Polymorph
	static constexpr uint32 ARCANE_EXPLOSION_THRESHOLD  = 2;        // Au-dela de N ennemis -> Explosion
	static constexpr float  TARGET_RANGE                = 30.0f;    // Portee de selection des cibles a distance
	static constexpr uint32 ARCANE_MISSILES_CHANCE      = 60;        // Missiles plutot qu'Arcane Orb sur un proc Clearcasting

	uint32 arcaneCharges;

	void Reset() override
	{
		npc_theramore_troop::Reset();

		arcaneCharges = 0;

		// Mage Armor au reveil (apres 1s pour laisser le mob spawner proprement).
		scheduler.Schedule(1s, [this](TaskContext /*context*/)
		{
			DoCastSelf(SPELL_MAGE_ARMOR);
		});
	}

	// -------------------------------------------------------------------------
	// Rotation
	// -------------------------------------------------------------------------

	// Quand le mage commence a reculer (kite).
	void OnBackpedStart(Unit* victim) override
	{
		DoCast(victim, SPELL_ARCANE_BARRAGE);
	}

	void JustEngagedWith(Unit* who) override
	{
		npc_theramore_troop::JustEngagedWith(who);

		// Premier Arcane Blast immediat sur l'agresseur.
		DoCast(who, SPELL_ARCANE_BLAST);

		scheduler
			// --- Arcane Blast continu ---
			// Re-schedule a la duree exacte du cast time (qui descend avec les charges).
			.Schedule(2s, [this](TaskContext arcane_blast)
			{
				DoCastVictim(SPELL_ARCANE_BLAST);
				arcane_blast.Repeat(1800ms);
			})
			// --- Arcane Missiles on Clearcasting ---
			.Schedule(1s, 5s, [this](TaskContext clearcasting)
			{
				if (me->HasAura(SPELL_CLEARCASTING_BUFF))
				{
					CastStop();
					if (roll_chance(ARCANE_MISSILES_CHANCE))
						DoCastVictim(SPELL_ARCANE_MISSILES);
					else
						DoCastSelf(SPELL_ARCANE_ORB);
				}
                clearcasting.Repeat(1s);
			})
			// --- Arcane Orbs ---
			// Salve de 1-5 orbs centree sur soi (cible utilisee uniquement pour l'arret si elle meurt).
			.Schedule(2s, [this](TaskContext arcane_orb)
			{
				DoCastSelf(SPELL_ARCANE_ORB);
				arcane_orb.Repeat(10s, 40s);
			})
			// --- Arcane Barrage (finisher) ---
			// Cast des que ARCANE_BARRAGE_MIN_CHARGES charges sont accumulees,
			// sinon on re-check toutes les 2s.
			.Schedule(2s, [this](TaskContext arcane_barrage)
			{
				if (arcaneCharges >= ARCANE_BARRAGE_MIN_CHARGES)
				{
					DoCastVictim(SPELL_ARCANE_BARRAGE);
					arcaneCharges = 0;
				}
				arcane_barrage.Repeat(2s);
			})
			// --- Touch of the Magi ---
			// Sur cible aleatoire qui n'a pas deja le debuff. Toutes les 3-8s.
			.Schedule(4s, 8s, [this](TaskContext touch_of_the_magi)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, TARGET_RANGE, false, true, -SPELL_TOUCH_OF_THE_MAGI_BUFF))
					DoCast(target, SPELL_TOUCH_OF_THE_MAGI);
				touch_of_the_magi.Repeat(3s, 8s);
			})
			// --- Mass Polymorph ---
			// MASS_POLYMORPH_THRESHOLD ennemis colles -> CC AOE, 1 min de cooldown.
			.Schedule(5s, 8s, [this](TaskContext mass_polymorph)
			{
				if (EnemiesInRange(AOE_RANGE) >= MASS_POLYMORPH_THRESHOLD)
				{
					CastStop();
					DoCastSelf(SPELL_MASS_POLYMORPH);
					mass_polymorph.Repeat(1min);
				}
				else
					mass_polymorph.Repeat(1s);
			})
			// --- Arcane Explosion ---
			// ARCANE_EXPLOSION_THRESHOLD ennemis colles -> degats AOE, 45s de cooldown.
			.Schedule(2s, 8s, [this](TaskContext arcane_explosion)
			{
				if (EnemiesInRange(AOE_RANGE) >= ARCANE_EXPLOSION_THRESHOLD)
				{
					CastStop();
					DoCastSelf(SPELL_ARCANE_EXPLOSION);
					arcane_explosion.Repeat(45s);
				}
				else
					arcane_explosion.Repeat(1s);
			});
	}
};

// -------------------------------------------------------------------------
// npc_theramore_faithful - Fidele de Theramore (pretre Sacre)
// -------------------------------------------------------------------------
// Soigneur a distance qui maintient Power Word: Fortitude sur ses allies et
// alterne soins et sorts d'ombre. npc_faithful_training en derive pour la
// scene d'entrainement d'avant-bataille.
struct npc_theramore_faithful : public npc_theramore_troop
{
	npc_theramore_faithful(Creature* creature) : npc_theramore_troop(creature, AI_Type::Distance) {}

	enum Groups
	{
		GROUP_NORMAL,               // Rotation offensive
		GROUP_SPIRIT_OF_REDEMPTION, // Routines de soin et Spirit of Redemption
	};

	enum Spells
	{
		SPELL_PRAYER_OF_HEALING     = 596,
        SPELL_GUARDIAN_SPIRIT       = 47788,
		SPELL_SHADOW_WORD_DEATH     = 51818,
		SPELL_PSYCHIC_SCREAM        = 65543,
		SPELL_PLEA                  = 200829,
		SPELL_POWER_WORD_FORTITUDE  = 267528,
		SPELL_RENEW                 = 294342,
		SPELL_FLASH_HEAL            = 314655,
		SPELL_POWER_WORD_SHIELD     = 318158,
        SPELL_HOLY_WORD_SERENITY    = 430546,
		SPELL_SHADOW_WORD_PAIN      = 435397,
    };

	static constexpr float  STANDARD_RANGE              = 40.0f;   // Portee standard des soins et DoT
	static constexpr float  PSYCHIC_SCREAM_RANGE        = 10.0f;   // Distance de detection AOE
	static constexpr uint32 PSYCHIC_SCREAM_MIN_FOES     = 2;       // Au-dela de N ennemis -> Psychic Scream
	// Tirages imbriques du backpedal (voir OnBackpedStart).
	static constexpr uint32 BACKPED_SELF_HEAL_CHANCE    = 25;
	static constexpr uint32 BACKPED_SHADOW_WORD_CHANCE  = 50;

	void Reset() override
	{
		npc_theramore_troop::Reset();

		// Buff permanent : Power Word: Fortitude sur les allies qui ne l'ont pas (re-check toutes les 2s).
		scheduler.Schedule(1s, 5s, [this](TaskContext fortitude)
		{
			if (Unit* target = SelectRandomMissingBuff(SPELL_POWER_WORD_FORTITUDE))
				DoCast(target, SPELL_POWER_WORD_FORTITUDE);
			fortitude.Repeat(2s);
		});
	}

	// En backpedal : trois branches mutuellement exclusives selon roll_chance sequentiel.
	// Les deux tirages etant imbriques, les probabilites finales sont
	// 25% self-heal, 37.5% Shadow Word Death, 37.5% Shadow Word Pain.
	void OnBackpedStart(Unit* victim) override
	{
		if (roll_chance(BACKPED_SELF_HEAL_CHANCE))
			DoCastSelf(RAND(SPELL_RENEW, SPELL_PLEA, SPELL_POWER_WORD_SHIELD));
		else if (roll_chance(BACKPED_SHADOW_WORD_CHANCE))
			DoCast(victim, SPELL_SHADOW_WORD_DEATH);
		else if (Unit* dotTarget = SelectTarget(SelectTargetMethod::Random, 0, STANDARD_RANGE, false, true, -SPELL_SHADOW_WORD_PAIN))
			DoCast(dotTarget, SPELL_SHADOW_WORD_PAIN);
	}

	// -------------------------------------------------------------------------
	// Rotation
	// -------------------------------------------------------------------------

	void JustEngagedWith(Unit* who) override
	{
		npc_theramore_troop::JustEngagedWith(who);

		scheduler
            // Guardian Spirit
			.Schedule(1s, GROUP_NORMAL, [this](TaskContext guardian_spirit)
			{
                if (Unit* target = DoSelectLowestHpFriendly(STANDARD_RANGE))
                    DoCast(target, SPELL_GUARDIAN_SPIRIT, TRIGGERED_IGNORE_CAST_IN_PROGRESS);
                guardian_spirit.Repeat(3min);
			})
			// Psychic Scream si 2+ ennemis colles
			.Schedule(3s, 8s, GROUP_NORMAL, [this](TaskContext psychic_scream)
			{
				if (EnemiesInRange(PSYCHIC_SCREAM_RANGE) >= PSYCHIC_SCREAM_MIN_FOES)
				{
					DoCastAOE(SPELL_PSYCHIC_SCREAM);
					psychic_scream.Repeat(10s, 25s);
				}
				else
					psychic_scream.Repeat(1s);
			})
			// Power Word: Shield preventif sur un allie sous 60% PV.
			.Schedule(3s, GROUP_NORMAL, [this](TaskContext power_word_shield)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(STANDARD_RANGE, 60))
				{
					CastStop(SPELL_FLASH_HEAL);
					DoCast(target, SPELL_POWER_WORD_SHIELD);
				}
				power_word_shield.Repeat(3s);
			})
			// Sorts de soin aleatoires sur l'allie le plus bas (soi-meme inclus).
			// Holy Word: Serenity prend la priorite des qu'il est disponible.
			// Le prochain passage est cale sur le cast time reel du sort choisi
			// pour ne pas se couper soi-meme en plein cast.
            .Schedule(1s, GROUP_NORMAL, [this](TaskContext flash_heal)
            {
                if (Unit* target = FindLowestHealthFriend(me, STANDARD_RANGE, true))
                {
                    uint32 entry = RAND(SPELL_FLASH_HEAL, SPELL_RENEW, SPELL_PRAYER_OF_HEALING);
                    if (!me->GetSpellHistory()->HasCooldown(SPELL_HOLY_WORD_SERENITY))
                        entry = SPELL_HOLY_WORD_SERENITY;

                    DoCast(target, entry);

                    Milliseconds repeat = 1s;
                    if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(entry, DIFFICULTY_NORMAL))
                    {
                        uint32 castTime = spellInfo->CalcCastTime();
                        if (castTime > 0)
                            repeat = Milliseconds(castTime);
                    }

                    flash_heal.Repeat(repeat);
                    return;
                }

                flash_heal.Repeat(1s);
            });
	}
};

// -------------------------------------------------------------------------
// npc_theramore_marksman - Tireur d'elite de Theramore
// -------------------------------------------------------------------------
// La plus simple des troupes : deux sorts, un tir continu et un tir multiple.
// AI_Type::Hybrid : elle tire a distance mais se replie au corps a corps si
// on la colle.
struct npc_theramore_marksman : public npc_theramore_troop
{
	npc_theramore_marksman(Creature* creature) : npc_theramore_troop(creature, AI_Type::Hybrid)
	{
		Initialize();
	}

	enum Spells
	{
		SPELL_SHOOT                 = 22907,
		SPELL_MULTI_SHOOT           = 38310,
	};

	void Reset() override
	{
		npc_theramore_troop::Reset();

		me->SetEmoteState(EMOTE_STATE_READYCROSSBOW);
	}

	void JustEngagedWith(Unit* who) override
	{
		npc_theramore_troop::JustEngagedWith(who);

		DoCast(who, SPELL_SHOOT);

		scheduler
			.Schedule(2s, [this](TaskContext shoot)
			{
				DoCastVictim(SPELL_SHOOT);
				shoot.Repeat(2s);
			})
			.Schedule(15s, [this](TaskContext multi_shoot)
			{
				DoCastVictim(SPELL_MULTI_SHOOT);
				multi_shoot.Repeat(14s, 18s);
			});
	}
};

// =========================================================================
//     HORDE NPC
// =========================================================================

// -------------------------------------------------------------------------
// npc_theramore_horde - Classe de base des assaillants de la Horde
// -------------------------------------------------------------------------
// Trois responsabilites communes a tous les Roknah :
//   - appel a l'aide au pull
//   - credit de kill explicite a la mort (les vagues doivent compter meme
//     quand le KillRewarder standard ne s'applique pas)
//   - interdiction de prendre Kalecgos dragon pour cible : il survole la
//     ville sans etre attaquable, et sans ce garde-fou les hordes se
//     bloqueraient dessus au lieu d'attaquer les joueurs
struct npc_theramore_horde : public CustomAI
{
	npc_theramore_horde(Creature* creature, AI_Type type) : CustomAI(creature, true, type)
	{
		instance = creature->GetInstanceScript();
	}

	InstanceScript* instance;

	void JustEngagedWith(Unit* /*who*/) override
	{
		me->CallAssistance();
	}

	// Le credit de scenario (KillCredit[0] -> NPC_WAVE_MEMBER_CREDIT) est
	// pose par l'InstanceScript dans OnUnitDeath, pour toutes les morts et
	// pas seulement celles achevees par un joueur. Le crediter ici aussi le
	// compterait deux fois.

	bool CanAIAttack(Unit const* who) const override
	{
		// Kalecgos dragon est cosmetique : il n'est jamais une cible valide.
		if (who->GetEntry() == NPC_KALECGOS_DRAGON)
			return false;

		return CustomAI::CanAIAttack(who);
	}
};

// -------------------------------------------------------------------------
// npc_roknah_hag - Sorciere Roknah (mage Givre)
// -------------------------------------------------------------------------
// La plus complexe des AIs de la Horde. Trois mecaniques imbriquees :
//   - shatter : les splinters de Frostbolt empilent Freezing sur les cibles,
//     et une salve d'Ice Lance vient consommer ces stacks
//   - finisher : Glacial Spike, relance des que son buff est disponible
//   - survie : Ice Block a bas PV et Frost Nova + Blink quand elle est
//     encerclee par des melee
// -------------------------------------------------------------------------
struct npc_roknah_hag : public npc_theramore_horde
{
	npc_roknah_hag(Creature* creature) : npc_theramore_horde(creature, AI_Type::Distance),
		iceblock(false)
	{
	}

	// Override : detection d'encerclement plus large (12y comme l'ancien MELEE_FLEE_RANGE).
	float GetEncircleRadius() const override { return MELEE_FLEE_RANGE; }

	// Groupes de scheduler. Chaque groupe peut etre delay/cancel independamment.
	enum Groups
	{
		GROUP_NORMAL,       // Cycle long : Glacial Spike retry, Cone of Cold, Flurry
		GROUP_FLEE,         // Sequence Frost Nova -> Blink declenchee par melee/Ice Block
		GROUP_FROSTBOLT,    // Cast continu de Frostbolt (pilier de la rotation)
		GROUP_ICE_LANCE     // Salves d'Ice Lance sur cibles Freezing
	};

	enum Spells
	{
		SPELL_FROSTBOLT             = 116,
		SPELL_GLACIAL_SPIKE         = 199786,
		SPELL_GLACIAL_SPIKE_BUFF    = 199844,
		SPELL_ICE_BARRIER           = 198094,
		SPELL_HYPOTHERMIA           = 240132,
		SPELL_ICE_BLOCK             = 278960,
		SPELL_FLURRY                = 284858,
		SPELL_ICE_LANCE             = 284871,
		SPELL_FROST_NOVA            = 284879,
		SPELL_CONE_OF_COLD          = 292294,
		SPELL_BLINK                 = 295236,
		SPELL_FROST_SPLINTER        = 443722,
		SPELL_FREEZING              = 1221389
	};

	static constexpr float MELEE_FLEE_RANGE             = 12.0f;    // Distance "trop proche" pour fuir
	static constexpr uint32 MELEE_FLEE_THRESHOLD        = 2;        // Au-dela de N ennemis melee, on fuit
	static constexpr uint8 ICE_BLOCK_HP_PCT             = 20;       // Seuil de PV declenchant Ice Block + fuite
	static constexpr uint32 ICE_BARRIER_PULL_CHANCE     = 30;       // Chance de se buffer Ice Barrier au pull
	static constexpr float ICE_LANCE_RANGE              = 30.0f;    // Portee de selection des cibles Ice Lance
	static constexpr uint32 FREEZING_STACKS_PER_LANCE   = 4;        // Stacks de Freezing consommes par un Ice Lance qui shatter

	// Drapeau utilise pour tout cast interne (visuels, buffs auto, finishers, retries) :
	// ignore GCD, cast en cours, cout, etc. - ne doit jamais bloquer la rotation.
	static constexpr TriggerCastFlags INTERNAL_CAST = TRIGGERED_FULL_MASK;

	bool iceblock; // True tant qu'Ice Block est en cooldown interne (1 min)

	void Reset() override
	{
		npc_theramore_horde::Reset();

		iceblock = false;
	}

	// -------------------------------------------------------------------------
	// Helpers
	// -------------------------------------------------------------------------

	// Tente de lancer Pointe glaciaire si le buff finisher est encore actif.
	// Centralise la logique appelee depuis SpellHit (proc), OnSpellFailed (retry)
	// et la tache GROUP_NORMAL (filet de securite).
	void TryCastGlacialSpike()
	{
		if (!me->HasAura(SPELL_GLACIAL_SPIKE_BUFF))
			return;

		CastStop();
		DoCastVictim(SPELL_GLACIAL_SPIKE);
	}

	// -------------------------------------------------------------------------
	// Hit handlers
	// -------------------------------------------------------------------------

	void SpellHitTarget(WorldObject* object, SpellInfo const* spell) override
	{
		Unit* victim = object->ToUnit();
		if (!victim)
			return;

		// La generation de splinters (Frostbolt / Flurry / Ice Lance / Glacial Spike)
		// est entierement geree par le passif Splintering Sorcery (443739).
		// La generation d'Icicles + le buff Pointe glaciaire (199844) sont geres
		// par le talent passif SPELL_ICICLE_TALENT (1246832).
		switch (spell->Id)
		{
			case SPELL_FROST_SPLINTER:
				// Chaque splinter applique une stack de Freezing sur sa cible.
				DoCast(victim, SPELL_FREEZING, INTERNAL_CAST);
				break;
		}
	}

	// Pointe glaciaire interrompue / OOR / hors de vue : on relance dans 2s tant
	// que le buff de disponibilite est encore actif (le filet de securite
	// GROUP_NORMAL retentera aussi, mais ce retry immediat reduit le temps mort).
	void OnSpellFailed(SpellInfo const* spell) override
	{
		if (spell->Id == SPELL_GLACIAL_SPIKE && me->HasAura(SPELL_GLACIAL_SPIKE_BUFF))
		{
			scheduler.Schedule(2s, GROUP_NORMAL, [this](TaskContext /*context*/)
			{
				TryCastGlacialSpike();
			});
		}
	}

	void SpellHit(WorldObject* /*caster*/, SpellInfo const* spell) override
	{
		// Blink reactif : si on subit un root/snare et que Blink est dispo, on le lance dans 2s.
		if (!me->GetSpellHistory()->HasCooldown(SPELL_BLINK)
			&& (HasMechanic(spell, MECHANIC_ROOT) || HasMechanic(spell, MECHANIC_SNARE)))
		{
			scheduler.Schedule(2s, [this, spellId = spell->Id](TaskContext /*context*/)
			{
				CastBlink(false, spellId);
			});
		}

		// Le buff Glacial Spike vient d'etre applique -> on tente le finisher immediatement.
		if (spell->Id == SPELL_GLACIAL_SPIKE_BUFF)
			TryCastGlacialSpike();
	}

	// -------------------------------------------------------------------------
	// Reactions
	// -------------------------------------------------------------------------

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spell*/) override
	{
		// En dessous de 20% PV : on annule le coup et on declenche Ice Block + fuite.
		// Cooldown interne d'1 min - Hypothermia empeche cote sort un re-block immediat.
		if (!iceblock && HealthBelowPct(ICE_BLOCK_HP_PCT))
		{
			damage = 0;

			iceblock = true;

			CastStop();
			DoCast(SPELL_ICE_BLOCK);
			DoCastSelf(SPELL_HYPOTHERMIA, INTERNAL_CAST);

			// 10s (+1s) = duree d'Ice Block : on enchaine Frost Nova + Blink.
			CastFleeSequence(11s);

			scheduler.Schedule(1min, [this](TaskContext /*context*/)
			{
				iceblock = false;
			});
		}
	}

	// Quand le mage commence a reculer (kite), on lache une Ice Lance gratuite.
	void OnBackpedTick(Unit* victim) override
	{
		DoCast(victim, SPELL_ICE_LANCE, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_IGNORE_GCD);
	}

	// -------------------------------------------------------------------------
	// Rotation principale
	// -------------------------------------------------------------------------

	void JustEngagedWith(Unit* who) override
	{
		npc_theramore_horde::JustEngagedWith(who);

		// 30% de chance de se buffer Ice Barrier au pull.
		if (roll_chance(ICE_BARRIER_PULL_CHANCE))
			DoCastSelf(SPELL_ICE_BARRIER);

		// Premier Frostbolt immediat sur l'agresseur (sinon il faut attendre 2s).
		DoCast(who, SPELL_FROSTBOLT);

		scheduler
			// --- Salve d'Ice Lance ---
			// Selectionne une cible aleatoire avec Freezing, puis enchaine 2-5 lances
			// espacees de 800ms-1s. Le prochain cycle est planifie a +8/12s apres
			// la fin de la derniere lance (offset cumule).
			.Schedule(1s, GROUP_ICE_LANCE, [this](TaskContext shatter)
			{
				Unit* target = SelectTarget(SelectTargetMethod::Random, 0, ICE_LANCE_RANGE, false, true, SPELL_FREEZING);
				if (!target)
				{
					shatter.Repeat(2s);
					return;
				}

				// Chaque Ice Lance consomme 4 stacks de Freezing pour shatter,
				// donc on plafonne le nombre de lances a stacks/4.
				Aura const* freezing = target->GetAura(SPELL_FREEZING);
				uint32 stacks = freezing ? freezing->GetStackAmount() : 0;
				uint32 maxLances = stacks / FREEZING_STACKS_PER_LANCE;
				if (!maxLances)
				{
					shatter.Repeat(2s);
					return;
				}

				ObjectGuid targetGuid = target->GetGUID();
				uint32 count = std::min<uint32>(urand(2, 5), maxLances);

				// Precalcule les offsets pour connaitre la duree totale de la salve
				// et pouvoir bloquer la rotation pile le temps necessaire.
				std::vector<Milliseconds> offsets;
				offsets.reserve(count);
				Milliseconds offset = 0ms;
				for (uint32 i = 0; i < count; ++i)
				{
					offsets.push_back(offset);
					offset += randtime(800ms, 1s);
				}

				// Laisse respirer Frostbolt et la rotation pendant toute la salve
				// (+1s de marge pour eviter qu'un Frostbolt chevauche la derniere lance).
				Milliseconds salvoDuration = offset + 1s;
				scheduler.DelayGroup(GROUP_NORMAL, salvoDuration);
				scheduler.DelayGroup(GROUP_FROSTBOLT, salvoDuration);

				for (Milliseconds delay : offsets)
				{
					scheduler.Schedule(delay, GROUP_ICE_LANCE, [this, targetGuid](TaskContext /*lance*/)
					{
						Unit* victim = ObjectAccessor::GetUnit(*me, targetGuid);
						if (!victim || !victim->IsAlive())
							return;

						// Si la cible a perdu Freezing entre-temps (dispel, expiration,
						// shatter d'un autre add...), on stoppe la salve : la rotation
						// normale reprend grace au DelayGroup qui arrive a echeance.
						if (!victim->HasAura(SPELL_FREEZING))
							return;

						DoCast(victim, SPELL_ICE_LANCE, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_IGNORE_GCD);
					});
				}

				shatter.Repeat(offset + 5s, offset + 8s);
			})
			// --- Filet de securite Glacial Spike ---
			// Si Glacial Spike rate (interrompu, OOR), retente toutes les 2s tant que le buff est la.
			.Schedule(1s, GROUP_NORMAL, [this](TaskContext glacial_spike)
			{
				if (!me->HasUnitState(UNIT_STATE_CASTING) && me->HasAura(SPELL_GLACIAL_SPIKE_BUFF))
					TryCastGlacialSpike();
				glacial_spike.Repeat(2s);
			})
			// --- Cone of Cold ---
			// Quand 3+ ennemis sont au corps a corps : interrompt Glacial Spike et lance Cone of Cold.
			.Schedule(13s, 18s, GROUP_NORMAL, [this](TaskContext cone_of_cold)
			{
				if (EnemiesInRange(MELEE_FLEE_RANGE) > MELEE_FLEE_THRESHOLD)
				{
					CastStop(SPELL_GLACIAL_SPIKE);
					DoCast(SPELL_CONE_OF_COLD);
					cone_of_cold.Repeat(5s, 8s);
				}
				else
					cone_of_cold.Repeat(2s);
			})
			// --- Flurry ---
			// Cible aleatoire toutes les 12-14s.
			.Schedule(12s, 15s, GROUP_NORMAL, [this](TaskContext flurry)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random))
					DoCast(target, SPELL_FLURRY);
				flurry.Repeat(12s, 14s);
			})
			// --- Frostbolt continu ---
			// Pilier de la rotation : premier cast a 2s, puis 1 Frostbolt toutes
			// les 3s sur la cible courante.
			.Schedule(2s, GROUP_FROSTBOLT, [this](TaskContext frostbolt)
			{
				DoCastVictim(SPELL_FROSTBOLT);
				frostbolt.Repeat(3s);
			});
	}

	// Encerclee par des melee -> Frost Nova + Blink. Le cooldown de re-declenchement
	// est gere par CustomAI (encircleReactOnCooldown).
	bool OnEncircled(Unit* /*victim*/) override
	{
		if (me->HasAura(SPELL_ICE_BLOCK))
			return false;

		CastFleeSequence(1s);
		return true;
	}

	// Sequence de fuite : Delai -> Frost Nova (root les melee) -> 300ms -> Blink (degage).
	// Reutilisee par DamageTaken (Ice Block a 20% PV) et OnEncircled (encerclement).
	void CastFleeSequence(Seconds start)
	{
		scheduler
			.Schedule(start, GROUP_FLEE, [this](TaskContext context)
			{
				if (me->HasAura(SPELL_ICE_BLOCK))
					return;

				switch (context.GetRepeatCounter())
				{
					case 0: // Delai des groupes de combat
						scheduler.DelayGroup(GROUP_NORMAL, 2s);
						scheduler.DelayGroup(GROUP_FROSTBOLT, 2s);
						scheduler.DelayGroup(GROUP_ICE_LANCE, 2s);
						context.Repeat(100ms);
						break;
					case 1: // Root les melee autour
						CastStop();
						DoCastSelf(SPELL_FROST_NOVA, INTERNAL_CAST);
						context.Repeat(300ms);
						break;
					case 2: // Blink pour reprendre de la distance
						CastBlink(true);
						break;
				}
			});
	}

	// -------------------------------------------------------------------------
	// Splinters / Backped / Blink
	// -------------------------------------------------------------------------

	// Blink + nettoyage des effets de mouvement, avec retrait optionnel de l'aura qui nous a touche.
	void CastBlink(bool triggered, Optional<uint32> removeAura = {})
	{
		CastStop();
		DoCastSelf(SPELL_BLINK, triggered);
		me->RemoveMovementImpairingAuras(true);

		// Empeche le circle-kite et le MoveChase de ramener le mage sur sa
		// position d'avant le Blink.
		NotifyTeleported();

		if (removeAura)
			me->RemoveAurasDueToSpell(*removeAura);
	}
};

// -------------------------------------------------------------------------
// npc_roknah_grunt - Grognard Roknah (guerrier Fureur)
// -------------------------------------------------------------------------
// Pendant Horde de npc_theramore_footman : meme liste de sorts et meme
// rotation, seule la classe de base change.
struct npc_roknah_grunt : public npc_theramore_horde
{
	npc_roknah_grunt(Creature* creature) : npc_theramore_horde(creature, AI_Type::Melee) {}

	enum Spells
	{
		SPELL_BLOODTHIRST       = 23881,    // Victim
		SPELL_SPELL_REFLECTION  = 23920,    // UnitCasting (target Self)
		SPELL_RECKLESSNESS      = 122354,   // Buff
		SPELL_HAMSTRING         = 198374,   // Victim
		SPELL_BLADESTORM        = 235661,   // Enemies in range > 3
		SPELL_PUMMEL            = 265431,   // UnitCasting
		SPELL_SUDDEN_DEATH_BUFF = 280721,   // Buff casted by talent (dont use)
		SPELL_MORTAL_STRIKE     = 283410,   // MostMissingEnemy
		SPELL_WHIRLWIND         = 283412,   // Enemies in range >= 2
		SPELL_REND              = 283419,   // Victim
		SPELL_EXECUTE           = 1231790,  // On Victim when SUDDEN_DEATH_BUFF is up (check 1s to 2s)
	};

	void InitializeAI() override
	{
		me->SetOverrideDisplayPowerId(237);
		ScriptedAI::InitializeAI();
	}

	static constexpr uint32  BLADESTORM_MIN_ENEMIES = 3;   // Ennemis au contact requis pour Bladestorm
	static constexpr uint32  WHIRLWIND_MIN_ENEMIES  = 2;   // Ennemis au contact requis pour Whirlwind
	static constexpr uint8   RECKLESSNESS_HP_PCT    = 30;  // Seuil de PV declenchant Recklessness

	void JustEngagedWith(Unit* who) override
	{
		npc_theramore_horde::JustEngagedWith(who);

		scheduler

			// --- Recklessness ---
			// Buff offensif de dernier recours : on sonde chaque seconde et on ne
			// le lance qu'une seule fois, quand les PV passent sous 30%.
			.Schedule(1ms, [this](TaskContext recklessness)
			{
				if (HealthBelowPct(RECKLESSNESS_HP_PCT)
					&& !me->HasAura(SPELL_RECKLESSNESS))
				{
					DoCastSelf(SPELL_RECKLESSNESS);
					return;
				}
				recklessness.Repeat(1s);
			})

			// --- Rend ---
			// DoT sur la victime, toutes les 10-14s.
			.Schedule(3s, 6s, [this](TaskContext rend)
			{
				DoCastVictim(SPELL_REND);
				rend.Repeat(10s, 14s);
			})

			// --- Bloodthirst ---
			// Frappe offensive de base, toutes les 5-8s.
			.Schedule(1ms, [this](TaskContext bloodthirst)
			{
				DoCastVictim(SPELL_BLOODTHIRST);
				bloodthirst.Repeat(5s, 8s);
			})

			// --- Hamstring ---
			// Ralentit la cible, toutes les 8-12s.
			.Schedule(5s, 9s, [this](TaskContext hamstring)
			{
				DoCastVictim(SPELL_HAMSTRING);
				hamstring.Repeat(8s, 12s);
			})

			// --- Mortal Strike ---
			// Frappe puissante sur la victime, toutes les 6-8s.
			.Schedule(4s, 7s, [this](TaskContext mortal_strike)
			{
				DoCastVictim(SPELL_MORTAL_STRIKE);
				mortal_strike.Repeat(6s, 8s);
			})

			// --- Whirlwind ---
			// AOE melee si 2+ ennemis a portee, toutes les 10-15s.
			.Schedule(10s, 15s, [this](TaskContext whirlwind)
			{
				if (EnemiesInRange(MELEE_RANGE) >= WHIRLWIND_MIN_ENEMIES)
				{
					DoCastSelf(SPELL_WHIRLWIND);
					whirlwind.Repeat(10s, 15s);
				}
				else
					whirlwind.Repeat(2s);
			})

			// --- Bladestorm ---
			// Tourbillon devastateur si 3+ ennemis a portee, toutes les 30-45s.
			.Schedule(20s, 30s, [this](TaskContext bladestorm)
			{
				if (EnemiesInRange(MELEE_RANGE) >= BLADESTORM_MIN_ENEMIES)
				{
					DoCastSelf(SPELL_BLADESTORM);
					bladestorm.Repeat(30s, 45s);
				}
				else
					bladestorm.Repeat(2s);
			})

			// --- Pummel ---
			// Interrompt un ennemi qui cast a portee melee. Re-check toutes les 1s, puis 15-18s CD.
			.Schedule(1s, [this](TaskContext pummel)
			{
				if (Unit* target = DoSelectCastingUnit(SPELL_PUMMEL, MELEE_RANGE))
				{
					DoCast(target, SPELL_PUMMEL);
					pummel.Repeat(15s, 18s);
				}
				else
					pummel.Repeat(1s);
			})

			// --- Spell Reflection ---
			// Buff defensif pose sur soi-meme lorsqu'un ennemi cast a portee melee. Re-check toutes les 1s.
			.Schedule(1s, [this](TaskContext spell_reflection)
			{
				if (!me->HasAura(SPELL_SPELL_REFLECTION) && DoSelectCastingUnit(SPELL_SPELL_REFLECTION, MELEE_RANGE))
				{
					DoCastSelf(SPELL_SPELL_REFLECTION);
					spell_reflection.Repeat(20s, 25s);
				}
				else
					spell_reflection.Repeat(1s);
			})

			// --- Execute ---
			// Consomme le buff Sudden Death sur la victime. Re-check toutes les 1-2s.
			.Schedule(1s, [this](TaskContext execute)
			{
				if (me->HasAura(SPELL_SUDDEN_DEATH_BUFF))
					DoCastVictim(SPELL_EXECUTE);
				execute.Repeat(1s, 2s);
			});
	}
};

// -------------------------------------------------------------------------
// npc_roknah_loasinger - Chantre des loas (chaman hybride)
// -------------------------------------------------------------------------
// Chaman degats + soins : il alterne une rotation elementaire et des soins
// sur ses allies blesses, avec une sequence defensive (Astral Shift puis
// Ascendance) declenchee une seule fois par combat a bas PV.
struct npc_roknah_loasinger : public npc_theramore_horde
{
	npc_roknah_loasinger(Creature* creature) : npc_theramore_horde(creature, AI_Type::Distance),
		ascending(false)
	{
		ascendanceInfo = sSpellMgr->AssertSpellInfo(SPELL_ASCENDANCE, DIFFICULTY_NONE);
		flameShockInfo = sSpellMgr->AssertSpellInfo(SPELL_FLAME_SHOCK, DIFFICULTY_NONE);
		frostShockInfo = sSpellMgr->AssertSpellInfo(SPELL_FROST_SHOCK, DIFFICULTY_NONE);
	}

	enum Groups
	{
		GROUP_NORMAL,       // Rotation offensive (DPS + interrupt + AOE)
		GROUP_HEALING,      // Routines de soin sur les allies
		GROUP_ASCENDANCE,   // Sequence defensive declenchee a 40% PV
	};

	enum Spells
	{
		SPELL_HEALING_RAIN      = 73920,
		SPELL_UNLEASH_LIFE      = 73685,
		SPELL_HEALING_WAVE      = 77472,
		SPELL_EARTHQUAKE        = 160162,
		SPELL_ASCENDANCE        = 173160,
		SPELL_GUST_OF_WIND      = 204853,
		SPELL_LAVA_BURST        = 290423,
		SPELL_WIND_SHEAR        = 290439,
		SPELL_ASTRAL_SHIFT      = 292158,
		SPELL_CHAIN_LIGHTNING   = 290411,
		SPELL_FLAME_SHOCK       = 290422,
		SPELL_FROST_SHOCK       = 290441,
		SPELL_RIPTIDE           = 241892,
		SPELL_CHAIN_HEAL        = 258099,
		SPELL_WATER_BLAST       = 450908,
		SPELL_HEALING_TIDE      = 127945,
		SPELL_LIGHTNING_BOLT    = 1246687,
	};

	static constexpr uint8 ASCENDANCE_HP_PCT             = 40;       // PV qui declenche la sequence defensive
	static constexpr float WIND_SHEAR_RANGE              = 35.0f;    // Portee du kick
	static constexpr float EARTHQUAKE_RANGE              = 8.0f;     // Distance de detection AOE
	static constexpr uint32 EARTHQUAKE_THRESHOLD         = 3;        // Au-dela de N ennemis -> Earthquake

	// Seuils de heal : (range, pct PV).
	static constexpr float HEAL_FRIENDLY_RANGE           = 40.0f;    // Portee standard des soins cibles
	static constexpr uint8 HEALING_WAVE_PCT             = 60;       // Spot heal
	static constexpr uint8 RIPTIDE_PCT                   = 80;       // HoT preventif
	static constexpr uint8 CHAIN_HEAL_PCT                = 50;       // Heal multi-cible
	static constexpr float HEALING_RAIN_RANGE            = 80.0f;    // Healing Rain (gros radius)
	static constexpr uint8 HEALING_TIDE_PCT              = 30;        // Healing Tide Totem (urgence absolue)
	static constexpr float HEALING_TIDE_RANGE            = 60.0f;    // Healing Tide (gros radius)

	const SpellInfo* ascendanceInfo;
	const SpellInfo* flameShockInfo;
	const SpellInfo* frostShockInfo;

	bool ascending; // True une fois la sequence Ascendance declenchee (one-shot par combat)

	void Reset() override
	{
		npc_theramore_horde::Reset();
		ascending = false;
	}

	// -------------------------------------------------------------------------
	// Reactions
	// -------------------------------------------------------------------------

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spell*/) override
	{
		// Sequence defensive declenchee une seule fois par combat (ou par cooldown si reset).
		// Le flag `ascending` evite l'empilement de plusieurs sequences entre le declenchement
		// et le cast effectif d'Ascendance (qui pose le cooldown).
		if (ascending)
			return;

		if (!me->HealthBelowPctDamaged(ASCENDANCE_HP_PCT, damage))
			return;

		if (me->GetSpellHistory()->HasCooldown(ascendanceInfo))
			return;

		ascending = true;

		// Gel la rotation offensive pendant toute la duree d'Ascendance.
		scheduler.DelayGroup(GROUP_NORMAL, 30s);

		scheduler.Schedule(1ms, GROUP_ASCENDANCE, [this](TaskContext ascendance)
		{
			switch (ascendance.GetRepeatCounter())
			{
				case 0: // Reduction de degats immediate
					DoCastSelf(SPELL_ASTRAL_SHIFT);
					ascendance.Repeat(400ms);
					break;
				case 1: // Forme Ascendance (heals deviennent instants)
					DoCastSelf(SPELL_ASCENDANCE);
					break;
			}
		});
	}

	// Shock instant en sortie de backpedal.
	void OnBackpedStart(Unit* /*victim*/) override
	{
		DoCastSelf(RAND(SPELL_GUST_OF_WIND, SPELL_WATER_BLAST), TRIGGERED_CAST_DIRECTLY | TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CAST_IN_PROGRESS);
	}

	// -------------------------------------------------------------------------
	// Rotation
	// -------------------------------------------------------------------------

	void JustEngagedWith(Unit* who) override
	{
		npc_theramore_horde::JustEngagedWith(who);

		// Premier Lightning Bolt immediat sur l'agresseur.
		DoCast(who, SPELL_LIGHTNING_BOLT);

		scheduler

			// === BUFF ===

			// Unleash Life : buff de soin entretenu en permanence, il
			// interrompt le cast en cours car il est instantane.
			.Schedule(5s, GROUP_NORMAL, [this](TaskContext unleash_life)
			{
				CastStop();
				DoCastSelf(SPELL_UNLEASH_LIFE);
				unleash_life.Repeat(23s, 34s);
			})

			// === DPS ===

			// Chain Lightning sur cible aleatoire toutes les 3-5s.
			.Schedule(8s, 14s, GROUP_NORMAL, [this](TaskContext chain_lightning)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_CHAIN_LIGHTNING);
				chain_lightning.Repeat(3s, 5s);
			})
			// Frost Shock sur cible qui ne l'a pas deja.
			.Schedule(5s, 8s, GROUP_NORMAL, [this](TaskContext frost_shock)
			{
				if (Unit* target = DoFindEnemyMissingDot(frostShockInfo))
					DoCast(target, SPELL_FROST_SHOCK);
				frost_shock.Repeat(8s, 10s);
			})
			// Flame Shock sur cible qui ne l'a pas deja (DoT).
			.Schedule(5s, 8s, GROUP_NORMAL, [this](TaskContext flame_shock)
			{
				if (Unit* target = DoFindEnemyMissingDot(flameShockInfo))
					DoCast(target, SPELL_FLAME_SHOCK);
				flame_shock.Repeat(5s, 8s);
			})
			// Earthquake si 3+ ennemis colles.
			.Schedule(20s, 25s, GROUP_NORMAL, [this](TaskContext earthquake)
			{
				if (EnemiesInRange(EARTHQUAKE_RANGE) >= EARTHQUAKE_THRESHOLD)
				{
					DoCast(SPELL_EARTHQUAKE);
					earthquake.Repeat(10s, 13s);
				}
				else
					earthquake.Repeat(1s);
			})
			// Lava Burst sur la victime courante.
			.Schedule(11s, 15s, GROUP_NORMAL, [this](TaskContext lava_burst)
			{
				CastStop();
				DoCastVictim(SPELL_LAVA_BURST);
				lava_burst.Repeat(8s, 10s);
			})
			// Wind Shear : kick les casters ennemis dans la portee.
			.Schedule(1s, GROUP_NORMAL, [this](TaskContext wind_shear)
			{
				if (Unit* target = DoSelectCastingUnit(SPELL_WIND_SHEAR, WIND_SHEAR_RANGE))
				{
					CastStop();
					DoCast(target, SPELL_WIND_SHEAR);
					wind_shear.Repeat(10s, 18s);
				}
				else
					wind_shear.Repeat(1s);
			})
			// Lightning Bolt en continu (filler, 2.8s = cast time approche).
			.Schedule(1s, GROUP_NORMAL, [this](TaskContext lightning_bolt)
			{
				DoCastVictim(SPELL_LIGHTNING_BOLT);
				lightning_bolt.Repeat(3200ms);
			})

			// === HEAL ===

			// Healing Wave : spot heal sur allie sous HEALING_WAVE_PCT (60%) PV,
			// re-check toutes les 3s.
			.Schedule(1s, GROUP_HEALING, [this](TaskContext healing_wave)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(HEAL_FRIENDLY_RANGE, HEALING_WAVE_PCT))
				{
					CastStop(SPELL_HEALING_WAVE);
					DoCast(target, SPELL_HEALING_WAVE);
				}
				healing_wave.Repeat(3s);
			})
			// Riptide : HoT preventif sur allie sous RIPTIDE_PCT (80%) PV qui n'a
			// pas deja le HoT.
			.Schedule(1s, GROUP_HEALING, [this](TaskContext riptide)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(HEAL_FRIENDLY_RANGE, RIPTIDE_PCT, SPELL_RIPTIDE))
					DoCast(target, SPELL_RIPTIDE);
				riptide.Repeat(2s);
			})
			// Chain Heal : heal multi-cible si un allie est sous CHAIN_HEAL_PCT (50%) PV.
			.Schedule(2s, GROUP_HEALING, [this](TaskContext chain_heal)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(HEAL_FRIENDLY_RANGE, CHAIN_HEAL_PCT))
				{
					CastStop();
					DoCast(target, SPELL_CHAIN_HEAL);
				}
				chain_heal.Repeat(8s, 14s);
			})
			// Healing Rain : zone de soin si un allie est tres bas. Cooldown 24-32s.
			.Schedule(8s, GROUP_HEALING, [this](TaskContext healing_rain)
			{
				if (Unit* target = DoSelectBelowHpPctFriendly(HEALING_RAIN_RANGE, CHAIN_HEAL_PCT))
					DoCast(target, SPELL_HEALING_RAIN);
				healing_rain.Repeat(24s, 32s);
			})
			// Healing Tide Totem : urgence, un allie sous HEALING_TIDE_PCT (30%) PV.
			// Re-check toutes les 2s.
			.Schedule(15s, GROUP_HEALING, [this](TaskContext healing_tide)
			{
				if (DoSelectBelowHpPctFriendly(HEALING_TIDE_RANGE, HEALING_TIDE_PCT))
				{
					CastStop();
					DoCast(SPELL_HEALING_TIDE);
				}
				healing_tide.Repeat(2s);
			});
	}
};

// -------------------------------------------------------------------------
// npc_roknah_felcaster - Gangregarde Roknah (demoniste Demonologie)
// -------------------------------------------------------------------------
// Demoniste a fragments d'ame : Shadow Bolt et Demonbolt alimentent le
// compteur, Main de Gul'dan le depense en invoquant des diablotins (voir
// spell_wild_imp / spell_hand_of_guldan plus bas). Cotes defensif : Dark
// Pact a mi-vie, puis Drain de vie et Etreinte mortelle a bas PV.
struct npc_roknah_felcaster : public npc_theramore_horde
{
	npc_roknah_felcaster(Creature* creature) : npc_theramore_horde(creature, AI_Type::Distance),
		soulShardsCount(0)
	{
		corruptionInfo = sSpellMgr->AssertSpellInfo(SPELL_CORRUPTION, DIFFICULTY_NONE);
	}

	enum Groups
	{
		GROUP_NORMAL,       // Rotation offensive (Trait de l'ombre / Bolt infernal, Main de Gul'dan)
		GROUP_DEFENSIVE,    // Drain de vie / Etreinte mortelle declenches sur seuil PV
		GROUP_AURAS,        // Auras des talents
	};

	enum Spells
	{
		SPELL_SHADOWBOLT            = 686,
		SPELL_MORTAL_COIL           = 6789,
		SPELL_DARK_PACT             = 108416,
		SPELL_DRAIN_LIFE            = 149992,
		SPELL_IMPLOSION             = 196277,
		SPELL_SUMMON_FELHUNTER      = 285232,
		SPELL_CORRUPTION            = 251406,
		SPELL_DEMONBOLT             = 264178,
		SPELL_DEMONIC_CORE_BUFF     = 264173,
		SPELL_WILD_IMP_AURA_STACK   = 296553,
		SPELL_CALL_DREADSTALKERS    = 464874,
		SPELL_HAND_OF_GULDAN        = 464895,
		SPELL_INFERNAL_BOLT         = 434506,
		SPELL_INFERNAL_BOLT_BUFF    = 433891,
		SPELL_RUINATION             = 434635,
		SPELL_RUINATION_BUFF        = 433885,
		SPELL_SHARDS                = 1279442,
	};

	const SpellInfo* corruptionInfo;

	uint8 soulShardsCount = 0;

	static constexpr uint8   SOUL_SHARDS_MAX        = 3;                // Seuil de fragments d'ame avant Main de Gul'dan
	static constexpr uint8   DRAIN_LIFE_HP_PCT      = 30;               // Drain de vie sous N% PV
	static constexpr uint8   MORTAL_COIL_HP_PCT     = 40;               // Etreinte mortelle sous N% PV
	static constexpr uint32  FELHUNTER_CHANCE       = 60;               // % de chance d'invoquer un Traqueur des Tenebres au pull
	static constexpr Seconds DRAIN_LIFE_CHANNEL     = 6s;               // Duree du channel de Drain de vie (gel la rotation)
	static constexpr uint8   DARK_PACT_HP_PCT       = 60;               // Dark Pact sous N% PV (sacrifie des PV contre un bouclier)
	static constexpr uint8   IMPLOSION_MIN_IMPS     = 6;                // Diablotins requis avant de les faire exploser

	void InitializeAI() override
	{
		me->SetOverrideDisplayPowerId(217);
		ScriptedAI::InitializeAI();
	}

	void Reset() override
	{
		npc_theramore_horde::Reset();

		soulShardsCount = 0;

		// Traqueur des Tenebres au pull : 60% de chance.
		if (roll_chance(FELHUNTER_CHANCE))
			DoCastSelf(SPELL_SUMMON_FELHUNTER, TRIGGERED_CAST_DIRECTLY);
	}

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spell*/) override
	{
		if (me->HealthBelowPctDamaged(DARK_PACT_HP_PCT, damage)
			&& !me->HasAura(SPELL_DARK_PACT)
			&& !me->GetSpellHistory()->HasCooldown(SPELL_DARK_PACT))
		{
			CastStop(SPELL_DRAIN_LIFE);
			DoCastSelf(SPELL_DARK_PACT);
		}
	}

	void EnterEvadeMode(EvadeReason why = EvadeReason::Other) override
	{
		npc_theramore_horde::EnterEvadeMode(why);
		spell_wild_imp_aura::RemoveImps(me);
	}

	// Le cumul de shards se fait a la fin du cast (avant l'impact) pour rester aligne
	// avec le rythme de la rotation : Repeat(2300ms) doit pouvoir s'enchainer sans attendre le projectile.
	void OnSpellCast(SpellInfo const* spell) override
	{
		if (spell->Id == SPELL_SHADOWBOLT)
		{
			if (soulShardsCount < SOUL_SHARDS_MAX)
				soulShardsCount++;
		}
		else if (spell->Id == SPELL_DEMONBOLT)
		{
			// Demonbolt genere 2 fragments d'ame (consomme Demonic Core).
			soulShardsCount = std::min<uint8>(SOUL_SHARDS_MAX, soulShardsCount + 2);
		}
		else if (spell->Id == SPELL_INFERNAL_BOLT)
		{
			soulShardsCount = SOUL_SHARDS_MAX;
			me->RemoveAurasDueToSpell(SPELL_INFERNAL_BOLT_BUFF);
		}
		else if (spell->Id == SPELL_RUINATION)
		{
			soulShardsCount = 0;
			me->RemoveAurasDueToSpell(SPELL_RUINATION_BUFF);
		}
		else if (spell->Id == SPELL_HAND_OF_GULDAN)
			soulShardsCount = 0;

		// Main de Gul'dan : obligatoire quand 3 fragments d'ame sont accumules, interrompt tout sauf Drain de vie.
		DoCastHandOfGuldan();
	}

	// -------------------------------------------------------------------------
	// Rotation
	// -------------------------------------------------------------------------

	void JustEngagedWith(Unit* who) override
	{
		me->AddAura(SPELL_SHARDS, me);

		npc_theramore_horde::JustEngagedWith(who);

		scheduler
			// === AURAS ===
			.Schedule(1s, GROUP_AURAS, [this](TaskContext auraCheck)
			{
				if (me->HasAura(SPELL_DEMONIC_CORE_BUFF))
				{
					CastStop(SPELL_DRAIN_LIFE);
					DoCastVictim(SPELL_DEMONBOLT, TRIGGERED_CAST_DIRECTLY);
				}

				if (me->HasAura(SPELL_RUINATION_BUFF))
				{
					CastStop(SPELL_DRAIN_LIFE);
					DoCastVictim(SPELL_RUINATION);
				}

				auraCheck.Repeat(2s);
			})

			// === DEFENSIVE (GROUP_DEFENSIVE) ===

			// Drain de vie : channel 6s sous 30% PV, gel toute la rotation pendant ce temps.
			.Schedule(1s, GROUP_DEFENSIVE, [this](TaskContext drain_life)
			{
				if (HealthBelowPct(DRAIN_LIFE_HP_PCT) && !me->HasUnitState(UNIT_STATE_CASTING))
				{
					if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					{
						CastStop(SPELL_DRAIN_LIFE);
						DoCast(target, SPELL_DRAIN_LIFE);
						scheduler.DelayAll(DRAIN_LIFE_CHANNEL);
						drain_life.Repeat(DRAIN_LIFE_CHANNEL);
						return;
					}
				}

				drain_life.Repeat(1s);
			})
			// Etreinte mortelle : peur sur cible la plus eloignee, sous 40% PV.
			.Schedule(1s, GROUP_DEFENSIVE, [this](TaskContext mortal_coil)
			{
				if (HealthBelowPct(MORTAL_COIL_HP_PCT)
					&& !me->GetSpellHistory()->HasCooldown(SPELL_MORTAL_COIL))
				{
					if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
					{
						CastStop(SPELL_DRAIN_LIFE);
						DoCast(target, SPELL_MORTAL_COIL);
					}
				}

				mortal_coil.Repeat(1s);
			})

			// === DPS (GROUP_NORMAL) ===

			// Trait de l'ombre en filler (toutes les 2300ms) ; remplace par Bolt
			// infernal tant que le buff correspondant est actif.
			.Schedule(0s, GROUP_NORMAL, [this](TaskContext shadowbolt)
			{
				CastStop({ SPELL_HAND_OF_GULDAN, SPELL_INFERNAL_BOLT });

				if (me->HasAura(SPELL_INFERNAL_BOLT_BUFF))
					DoCastVictim(SPELL_INFERNAL_BOLT);
				else
					DoCastVictim(SPELL_SHADOWBOLT);

				shadowbolt.Repeat(2300ms);
			})
			// Invocation de demons supplementaires pour declencher Demonic Core.
			.Schedule(5s, GROUP_NORMAL, [this](TaskContext call_dreadstalkers)
			{
				CastStop({ SPELL_HAND_OF_GULDAN, SPELL_INFERNAL_BOLT });
				DoCastVictim(SPELL_CALL_DREADSTALKERS);
				call_dreadstalkers.Repeat(20s, 25s);
			})
			// Explosion des demons
			.Schedule(3s, GROUP_NORMAL, [this](TaskContext implosion)
			{
				if (Aura* wildImpCount = me->GetAura(SPELL_WILD_IMP_AURA_STACK))
				{
					uint8 stack = wildImpCount->GetStackAmount();
					if (stack >= IMPLOSION_MIN_IMPS)
					{
						CastStop(SPELL_DRAIN_LIFE);
						DoCastVictim(SPELL_IMPLOSION);
						implosion.Repeat(5s, 10s);
						return;
					}
				}

				implosion.Repeat(2s);
			})

			// === DoTs ===

			// Corruption : applique le DoT toutes les 3s sur les cibles qui ne l'ont pas.
			.Schedule(1s, GROUP_NORMAL, [this](TaskContext corruption)
			{
				if (Unit* target = DoFindEnemyMissingDot(corruptionInfo))
					DoCast(target, SPELL_CORRUPTION);
				corruption.Repeat(3s);
			});
	}

	// -------------------------------------------------------------------------
	// Helpers
	// -------------------------------------------------------------------------

	void DoCastHandOfGuldan()
	{
		if (soulShardsCount >= SOUL_SHARDS_MAX)
		{
			CastStop(SPELL_DRAIN_LIFE);
			DoCastVictim(SPELL_HAND_OF_GULDAN);
		}
	}
};

// -------------------------------------------------------------------------
// npc_wave_caller_gruhta - Appelante des vagues Gruhta (mini-boss)
// -------------------------------------------------------------------------
// Combat en deux phases :
//   Phase 1 (EnterCombatPhase) : rotation chaman elementaire classique.
//   Phase 2 (sous GRUHTA_TEMPEST_HP_PCT) : elle s'enfuit en loup fantome
//     jusqu'a la mer, s'entoure de MAX_ELEMENTAL_PROTECTION boucliers et
//     canalise une tempete qui pilonne la zone. Les joueurs recoivent la
//     marche sur l'eau pour pouvoir la poursuivre, et doivent briser ses
//     protections pour la ramener en phase 1.
// Sa mort supprime la barriere d'energie qui bloque la suite du scenario.
struct npc_wave_caller_gruhta : public CustomAI
{
	// Nombre de boucliers a briser pour interrompre la tempete.
	const uint8 MAX_ELEMENTAL_PROTECTION = 10;

	// Seuil de PV declenchant la fuite en mer et la tempete.
	static constexpr float GRUHTA_TEMPEST_HP_PCT = 40.f;
	// Portee de combat de Gruhta (elle reste a distance).
	static constexpr float GRUHTA_COMBAT_RANGE   = 30.f;
	// Pilonnage : nombre d'impacts par salve et rayon de dispersion.
	static constexpr uint8 LIGHTNING_STORM_BOLTS = 8;
	static constexpr float LIGHTNING_STORM_RADIUS = 100.0f;
	// Nombre de Lightning Bolt gratuits accordes par Stormkeeper.
	static constexpr uint32 STORMKEEPER_BOLTS    = 2;

	npc_wave_caller_gruhta(Creature* creature) : CustomAI(creature, true, AI_Type::Hybrid)
	{
		SetCanRandomMovement(false);

		instance = creature->GetInstanceScript();

		// Information des sorts : les cast times reels servent a caler les
		// Repeat de la rotation (voir EnterCombatPhase).
		infoLightningBolt = sSpellMgr->AssertSpellInfo(SPELL_LIGHTNING_BOLT, DIFFICULTY_NONE);
		infoChainLightning = sSpellMgr->AssertSpellInfo(SPELL_CHAIN_LIGHTNING, DIFFICULTY_NONE);

		// Point de depart de la fuite : sa position de spawn.
		tempestPos01 = creature->GetHomePosition();
	}

	enum Spells
	{
		SPELL_FLAME_SHOCK           = 420277,
		SPELL_TEMPEST               = 452201,
		SPELL_ELEMENTAL_BLAST       = 117014,
		SPELL_STORMKEEPER           = 191634,
		SPELL_LIGHTNING_BOLT        = 430109,
		SPELL_CHAIN_LIGHTNING       = 1228260, 
		SPELL_CALL_LIGHTNING        = 157348,
		SPELL_GHOST_WOLF            = 361620,
		SPELL_PRIMORDIAL_STORM      = 1218090,
		SPELL_UNLIMITED_POWER       = 272737,
		SPELL_FOCUS_ELEMENT         = 167205,
		SPELL_TEMPEST_CHANNELING    = 212079,
		SPELL_ELEMENTAL_PROTECTION  = 371756,
		SPELL_LIGHTNING_STORM       = 447930,
		SPELL_WATER_WALKING         = 73757,
	};

	enum Misc
	{
		GROUP_NORMAL                = 1,
		GROUP_TEMPEST,
		GROUP_CHECKER,
		GROUP_STORMKEEPER
	};

	InstanceScript* instance;
	Position tempestPos01;                  // Etape 1 de la fuite (position de spawn)

	// Etape 2 de la fuite : au large, sur l'eau. C'est de la qu'elle canalise.
	const Position tempestPos02 = { -3922.5f, -4848.2866f, 0.001533f, 0.78f };

	const SpellInfo* infoLightningBolt;
	const SpellInfo* infoChainLightning;

	float GetDistance() override
	{
		return GRUHTA_COMBAT_RANGE;
	}

	void Reset() override
	{
		CustomAI::Reset();


		me->SetRegenerateHealth(true);
		me->SetWaterWalking(true);
	}

	// Sa mort libere le passage : la barriere d'energie disparait.
	void JustDied(Unit* killer) override
	{
		CustomAI::JustDied(killer);

		if (GameObject* barrier = instance->GetGameObject(DATA_ENERGY_BARRIER))
			barrier->Delete();
	}

	// Fuite en deux etapes : spawn -> large, puis debut de la tempete.
	void MovementInform(uint32 type, uint32 id) override
	{
		if (type == EFFECT_MOTION_TYPE || type == POINT_MOTION_TYPE)
		{
			switch (id)
			{
				case MOVEMENT_INFO_POINT_01:
					me->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_02, tempestPos02, false, tempestPos02.GetOrientation());
					break;
				case MOVEMENT_INFO_POINT_02:
				{
					// Arrivee au large : fin de la course, debut du channel.
					me->RemoveAurasDueToSpell(SPELL_GHOST_WOLF);
					me->SetHomePosition(tempestPos02);
					DoCastSelf(SPELL_TEMPEST_CHANNELING);
					// Pilonnage continu : 8 impacts toutes les ~90ms, annule
					// en bloc via CancelGroup(GROUP_TEMPEST) quand ses
					// protections tombent.
					scheduler.Schedule(1s, GROUP_TEMPEST, [this](TaskContext lightning_storm)
					{
						CastSpellExtraArgs args;
						args.SetTriggerFlags(TRIGGERED_IGNORE_CAST_IN_PROGRESS);

						for (uint8 i = 0; i < LIGHTNING_STORM_BOLTS; i++)
						{
							Position randomPos = GetRandomPosition(tempestPos02, LIGHTNING_STORM_RADIUS);
							me->CastSpell(randomPos, SPELL_LIGHTNING_STORM, args);
						}

						lightning_storm.Repeat(80ms, 100ms);
					});
					break;
					}
				default:
					break;
			}
		}
	}

	void OnSpellCast(SpellInfo const* spell) override
	{
		switch (spell->Id)
		{
			case SPELL_ELEMENTAL_BLAST:
			{
				CastSpellExtraArgs args;
				args.AddSpellBP0(10);

				DoCastSelf(SPELL_FOCUS_ELEMENT, args);
				break;
			}
			// Stormkeeper accorde deux Lightning Bolt instantanes : on gele la
			// rotation normale le temps de les lacher, puis on consomme le buff.
			case SPELL_STORMKEEPER:
			{
				scheduler.DelayGroup(GROUP_NORMAL, 4s);
				scheduler.Schedule(1s, GROUP_STORMKEEPER, [this](TaskContext bolt)
				{
					if (bolt.GetRepeatCounter() >= STORMKEEPER_BOLTS)
					{
						me->RemoveAurasDueToSpell(SPELL_STORMKEEPER);
						bolt.CancelGroup(GROUP_STORMKEEPER);
						return;
					}

					CastSpellExtraArgs args;
					args.SetTriggerFlags(TRIGGERED_CAST_DIRECTLY | TRIGGERED_IGNORE_CAST_IN_PROGRESS);

					DoCastVictim(SPELL_LIGHTNING_BOLT, args);
					bolt.Repeat(1s);
				});
				break;
			}
		}
	}

	// Pendant la tempete, chaque sort recu consomme une protection elementaire.
	// Quand il n'en reste plus, le channel s'arrete et Gruhta revient au combat.
	void SpellHit(WorldObject* caster, SpellInfo const* spell) override
	{
		CustomAI::SpellHit(caster, spell);

		if (Aura* elementalProtection = me->GetAura(SPELL_ELEMENTAL_PROTECTION))
		{
			// Nombre de protections encore actives
			uint32 stack = elementalProtection->GetStackAmount();

			// Supprime une protection a chaque sort recu
			elementalProtection->SetStackAmount(stack - 1);

			// A VERIFIER : `stack` est lu AVANT le decrement et n'est jamais
			// nul pour une aura vivante, donc cette branche ne s'execute
			// jamais et la tempete n'est pas interrompue ici. Le test devrait
			// probablement porter sur `stack <= 1`. Laisse tel quel pour ne
			// pas modifier le gameplay sans validation.
			if (stack <= 0)
			{
				scheduler.CancelGroup(GROUP_TEMPEST);

				EnterCombatPhase();

				return;
			}
		}
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		scheduler
			// Surveillance des PV : declenche la bascule en phase tempete.
			.Schedule(1ms, GROUP_CHECKER, [this](TaskContext context)
				{
					if (me->HealthBelowPct(GRUHTA_TEMPEST_HP_PCT))
					{
						// La bascule est definitive : on coupe le checker et
						// toute la rotation de la phase 1.
						context.CancelGroup(GROUP_CHECKER);
						context.CancelGroup(GROUP_NORMAL);

						me->SetReactState(REACT_PASSIVE);
						me->RemoveAllAuras();

						CastStop();

						DoCastSelf(SPELL_GHOST_WOLF, true);

						// Les protections empilees sont la "barre de vie" de
						// la phase tempete (voir SpellHit).
						for (uint8 i = 0; i < MAX_ELEMENTAL_PROTECTION; i++)
						{
							me->AddAura(SPELL_ELEMENTAL_PROTECTION, me);
						}

						me->GetMotionMaster()->Clear();
						me->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_01, tempestPos01, true, tempestPos01.GetOrientation());

						// Marche sur l'eau pour les cibles : sans ca les
						// joueurs ne peuvent pas la suivre jusqu'au large.
						for (auto const& itr : me->GetThreatManager().GetUnsortedThreatList())
						{
							if (Unit* victim = itr->GetVictim())
								victim->AddAura(SPELL_WATER_WALKING, victim);
						}
					}
					else
						context.Repeat(1s);
				});

		EnterCombatPhase();
	}

	// Rotation de la phase 1. Extraite dans sa propre methode car elle est
	// relancee quand les joueurs brisent les protections de la tempete.
	void EnterCombatPhase()
	{
		scheduler
			.Schedule(2s, GROUP_NORMAL, [this](TaskContext flame_shock)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, GetDistance(), false, true, -SPELL_FLAME_SHOCK))
				{
					CastStop();
					DoCast(target, SPELL_FLAME_SHOCK);
				}
				flame_shock.Repeat(18s);
			})
			.Schedule(3s, GROUP_NORMAL, [this](TaskContext elemental_blast)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				{
					CastStop(SPELL_TEMPEST);
					DoCast(target, SPELL_ELEMENTAL_BLAST);
				}
				elemental_blast.Repeat(12s);
			})
			.Schedule(6s, GROUP_NORMAL, [this](TaskContext stormkeeper)
			{
				CastStop();
				DoCastSelf(SPELL_STORMKEEPER);
				stormkeeper.Repeat(78s);
			})
			.Schedule(8s, GROUP_NORMAL, [this](TaskContext tempest)
			{
				CastStop(SPELL_STORMKEEPER);
				DoCastVictim(SPELL_TEMPEST);
				tempest.Repeat(20s);
			})
			.Schedule(9s, GROUP_NORMAL, [this](TaskContext lightning_bolt)
			{
				DoCastVictim(SPELL_LIGHTNING_BOLT);
				lightning_bolt.Repeat(Milliseconds(infoLightningBolt->CalcCastTime()) + 300ms);
			})
			.Schedule(12s, GROUP_NORMAL, [this](TaskContext call_lightning)
			{
				DoCastVictim(SPELL_CALL_LIGHTNING);
				call_lightning.Repeat(20s);
			})
			.Schedule(14s, GROUP_NORMAL, [this](TaskContext chain_lightning)
			{
				CastStop({ SPELL_LIGHTNING_BOLT, SPELL_TEMPEST, SPELL_CALL_LIGHTNING, SPELL_ELEMENTAL_BLAST });
				DoCastVictim(SPELL_CHAIN_LIGHTNING);
				chain_lightning.Repeat(Milliseconds(infoChainLightning->CalcCastTime()) + 300ms, 6s);
			});
	}
};

// =========================================================================
//     COSMETIC
// =========================================================================
// Scene d'entrainement jouee en boucle tant que le scenario n'a pas atteint
// la phase Preparation : les troupes s'exercent devant les joueurs. Des que
// la phase bascule, chaque AI nettoie sa mise en scene et rend les creatures
// a leur comportement normal.

// Point que les creatures regardent une fois l'entrainement termine.
const Position LookAtPos = { -3669.20f, -4504.08f, 10.33f, 1.60f };

// -------------------------------------------------------------------------
// npc_faithful_training - Duel d'entrainement soigne par un pretre
// -------------------------------------------------------------------------
// Le fidele repere deux fantassins autour de lui et met en scene un duel :
// les deux soldats se tapent dessus (degats scriptes, pas de vrai combat)
// pendant que le pretre les soigne en alternance. Un troisieme task surveille
// la phase du scenario pour tout arreter au bon moment.
struct npc_faithful_training : public npc_theramore_faithful
{
	npc_faithful_training(Creature* creature) : npc_theramore_faithful(creature) { }

	enum Misc
	{
		COSMETIC_GROUP      = 0,
	};

	// Rayon de recherche des deux soldats a mettre en scene.
	static constexpr float SOLDIER_SEARCH_RANGE   = 15.0f;
	// PV de depart des duellistes, en fraction de leur maximum.
	static constexpr float SOLDIER_HEALTH_PCT     = 0.3f;
	// Degats echanges a chaque passe (purement cosmetiques).
	static constexpr uint32 SPARRING_DAMAGE_MIN   = 1000;
	static constexpr uint32 SPARRING_DAMAGE_MAX   = 2000;

	ObjectGuid soldierAGuid;
	ObjectGuid soldierBGuid;

	// Prepare un soldat pour le combat cosmetique (emote, etat, cible, PV reduits).
	void SetSoldierState(Creature* creature, Emote emote, Creature* target)
	{
		if (!creature || !target)
			return;

		creature->SetEmoteState(emote);
		creature->SetReactState(REACT_PASSIVE);
		creature->SetUnitFlag(UNIT_FLAG_PACIFIED);
		creature->SetRegenerateHealth(false);

		uint32 health = static_cast<uint32>(creature->GetMaxHealth() * SOLDIER_HEALTH_PCT);
		creature->SetHealth(health);
		creature->SetTarget(target->GetGUID());
	}

	// Prepare le healer (emote idle, passif, pas de cible, PV non modifies).
	void SetHealerState(Creature* creature)
	{
		if (!creature)
			return;

		creature->SetEmoteState(EMOTE_STATE_NONE);
		creature->SetReactState(REACT_PASSIVE);
		creature->SetUnitFlag(UNIT_FLAG_PACIFIED);
	}

	// Restaure l'etat normal d'une creature apres la phase cosmetique.
	void ClearState(Creature* creature)
	{
		if (!creature)
			return;

		creature->SetRegenerateHealth(true);
		creature->SetHealth(creature->GetMaxHealth());
		creature->SetTarget(ObjectGuid::Empty);

		float angle = creature->GetAbsoluteAngle(LookAtPos);
		creature->SetOrientation(angle);
		creature->SetFacingToPoint(LookAtPos);
		creature->SetReactState(REACT_AGGRESSIVE);
		creature->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
	}

	void Reset() override
	{
		npc_theramore_faithful::Reset();

		// La scene n'a de sens qu'avant la bataille.
		if ((BFTPhases)instance->GetData(DATA_SCENARIO_PHASE) > BFTPhases::Preparation)
			return;

		// Il faut deux fantassins a proximite pour monter le duel.
		std::vector<Creature*> soldiers;
		me->GetCreatureListWithEntryInGrid(soldiers, NPC_THERAMORE_FOOTMAN, SOLDIER_SEARCH_RANGE);

		if (soldiers.size() < 2)
			return;

		Creature* soldierA = soldiers[0];
		Creature* soldierB = soldiers[1];

		if (!soldierA || !soldierB)
			return;

		soldierAGuid = soldierA->GetGUID();
		soldierBGuid = soldierB->GetGUID();

		SetHealerState(me);
		SetSoldierState(soldierA, EMOTE_STATE_ATTACK1H, soldierB);
		SetSoldierState(soldierB, EMOTE_STATE_BLOCK_SHIELD, soldierA);

		scheduler
			// Surveillance de la phase : demonte la scene quand la
			// preparation commence.
			.Schedule(2s, COSMETIC_GROUP, [this](TaskContext checkPhase)
			{
				BFTPhases phase = (BFTPhases)instance->GetData(DATA_SCENARIO_PHASE);
				if (phase >= BFTPhases::Preparation)
				{
					ClearState(me);

					if (Creature* soldierA = ObjectAccessor::GetCreature(*me, soldierAGuid))
						ClearState(soldierA);

					if (Creature* soldierB = ObjectAccessor::GetCreature(*me, soldierBGuid))
						ClearState(soldierB);

					scheduler.CancelGroup(COSMETIC_GROUP);
				}
				else
					checkPhase.Repeat(2s);
			})
			// Le pretre soigne l'un des deux duellistes au hasard.
			.Schedule(2s, COSMETIC_GROUP, [this](TaskContext heal)
			{
				Creature* soldierA = ObjectAccessor::GetCreature(*me, soldierAGuid);
				Creature* soldierB = ObjectAccessor::GetCreature(*me, soldierBGuid);

				if (!soldierA || !soldierB)
					return;

				if (Creature* victim = RAND(soldierA, soldierB))
					me->CastSpell(victim, RAND(SPELL_FLASH_HEAL, SPELL_POWER_WORD_SHIELD));

				heal.Repeat(3s, 5s);
			})
			// Passe d'armes : degats symetriques appliques a la main, sans
			// jamais mettre les soldats en combat reel.
			.Schedule(2s, 8s, COSMETIC_GROUP, [this](TaskContext soldiers)
			{
				Creature* soldierA = ObjectAccessor::GetCreature(*me, soldierAGuid);
				Creature* soldierB = ObjectAccessor::GetCreature(*me, soldierBGuid);

				if (!soldierA || !soldierB)
					return;

				uint32 damage = urand(SPARRING_DAMAGE_MIN, SPARRING_DAMAGE_MAX);
				soldierA->DealDamage(soldierB, soldierA, damage);
				soldierB->DealDamage(soldierA, soldierB, damage);

				soldiers.Repeat(2s, 5s);
			});
	}
};

// -------------------------------------------------------------------------
// npc_arcanist_training - Arcaniste s'entrainant sur un mannequin
// -------------------------------------------------------------------------
// L'arcaniste attaque en boucle le mannequin le plus proche et part en
// Evocation des qu'il tombe a court de mana. Comme pour le duel, un task
// surveille la phase pour arreter la scene et supprimer le mannequin.
struct npc_arcanist_training : public npc_theramore_arcanist
{
	// Rayon de recherche du mannequin d'entrainement.
	static constexpr float DUMMY_SEARCH_RANGE = 15.f;
	// Seuil de mana (en %) declenchant l'Evocation.
	static constexpr int32 EVOCATION_MANA_PCT = 5;

	npc_arcanist_training(Creature* creature) : npc_theramore_arcanist(creature, AI_Type::Stay)
	{
		SetCanRandomMovement(false);

		// Duree reelle du channel, pour geler la scene pile le temps qu'il faut.
		SpellInfo const* evocationInfo = sSpellMgr->GetSpellInfo(SPELL_EVOCATION, DIFFICULTY_NONE);
		m_evocationDuration = evocationInfo
			? Milliseconds(evocationInfo->CalcDuration(creature))
			: 8s;
	}

	enum Groups
	{
		COSMETIC_GROUP_NORMAL   = 0,
	};

	enum Spells
	{
		SPELL_EVOCATION = 243070,
	};

	Milliseconds m_evocationDuration;

	void Reset() override
	{
		npc_theramore_arcanist::Reset();

		if ((BFTPhases)instance->GetData(DATA_SCENARIO_PHASE) > BFTPhases::Preparation)
			return;

		scheduler
			// Surveillance de la phase : arrete l'entrainement et fait
			// disparaitre le mannequin.
			.Schedule(2s, COSMETIC_GROUP_NORMAL, [this](TaskContext checkPhase)
			{
				BFTPhases phase = (BFTPhases)instance->GetData(DATA_SCENARIO_PHASE);
				if (phase >= BFTPhases::Preparation)
				{
					me->CombatStop();
					me->SetOrientation(me->GetAbsoluteAngle(LookAtPos));
					me->SetFacingToPoint(LookAtPos);

                    if (Creature* training = GetClosestCreatureWithEntry(me, NPC_TRAINING_DUMMY, DUMMY_SEARCH_RANGE))
                        training->KillSelf();

					scheduler.CancelGroup(COSMETIC_GROUP_NORMAL);
				}
				else
					checkPhase.Repeat(2s);
			})
			// Regeneration : a court de mana, il recharge et la scene se met
			// en pause pour toute la duree du channel.
			.Schedule(2s, COSMETIC_GROUP_NORMAL, [this](TaskContext context)
			{
				int32 manaPct = me->GetPowerPct(Powers::POWER_MANA);
				if (manaPct > 0 && manaPct <= EVOCATION_MANA_PCT)
				{
					scheduler.DelayGroup(COSMETIC_GROUP_NORMAL, m_evocationDuration);
					DoCast(SPELL_EVOCATION);
				}

				context.Repeat(2s);
			})
			// Reprise de cible : si le mannequin a ete perdu, on le reprend.
			.Schedule(2s, COSMETIC_GROUP_NORMAL, [this](TaskContext context)
			{
				if (me->GetVictim())
					return;

				Creature* training = GetClosestCreatureWithEntry(me, NPC_TRAINING_DUMMY, DUMMY_SEARCH_RANGE);
				if (!training)
					return;

				me->Attack(training, false);
				context.Repeat(2s);
			});
	}
};

// -------------------------------------------------------------------------
// npc_dummy_training - Mannequin d'entrainement
// -------------------------------------------------------------------------
// Encaisse tout sans jamais perdre de PV ni riposter.
struct npc_dummy_training : NullCreatureAI
{
	npc_dummy_training(Creature* creature) : NullCreatureAI(creature) {}

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
		damage = 0;
	}
};

// =========================================================================
//     SPELLS
// =========================================================================

// Flurry - 284858
// Le sort ne pose qu'un effet dummy : le SpellScript enchaine lui-meme les
// impacts (GetEffectValue() - 1 supplementaires) espaces de 300-400ms, tous
// rattaches au cast d'origine pour que les procs se comportent correctement.
class spell_roknah_hag_flurry : public SpellScript
{
	enum Misc
	{
		SPELL_MAGE_FLURRY_DAMAGE = 228596
	};

	class FlurryEvent : public BasicEvent
	{
	public:
		FlurryEvent(Unit* caster, ObjectGuid const& target, ObjectGuid const& originalCastId, int32 count)
			: _caster(caster), _target(target), _originalCastId(originalCastId), _count(count) { }

		bool Execute(uint64 time, uint32 /*diff*/) override
		{
			Unit* target = ObjectAccessor::GetUnit(*_caster, _target);

			if (!target)
				return true;

			_caster->CastSpell(target, SPELL_MAGE_FLURRY_DAMAGE, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS).SetOriginalCastId(_originalCastId));

			if (!--_count)
				return true;

			_caster->m_Events.AddEvent(this, Milliseconds(time) + randtime(300ms, 400ms));
			return false;
		}

	private:
		Unit* _caster;
		ObjectGuid _target;
		ObjectGuid _originalCastId;
		int32 _count;
	};

	bool Validate(SpellInfo const* /*spell*/) override
	{
		return ValidateSpellInfo({ SPELL_MAGE_FLURRY_DAMAGE });
	}

	void EffectHit(SpellEffIndex /*effIndex*/) const
	{
		GetCaster()->m_Events.AddEventAtOffset(new FlurryEvent(GetCaster(), GetHitUnit()->GetGUID(), GetSpell()->m_castId, GetEffectValue() - 1), randtime(300ms, 400ms));
	}

	void Register() override
	{
		OnEffectHitTarget += SpellEffectFn(spell_roknah_hag_flurry::EffectHit, EFFECT_0, SPELL_EFFECT_DUMMY);
	}
};

// Light of Dawn - 295712
// Relaie l'effet dummy vers le vrai sort de soin.
class spell_theramore_light_of_dawn : public SpellScript
{
	enum Spells
	{
		SPELL_LIGHT_OF_DAWN = 295712
	};

	void HandleDummy(SpellEffIndex /*effIndex*/)
	{
		if (Unit* target = GetHitUnit())
		{
			Unit* caster = GetCaster();
			caster->CastSpell(target, SPELL_LIGHT_OF_DAWN, true);
		}
	}

	void Register() override
	{
		OnEffectHitTarget += SpellEffectFn(spell_theramore_light_of_dawn::HandleDummy, EFFECT_1, SPELL_EFFECT_DUMMY);
	}
};

// 	Bucket Lands - 42339
// Seau d'eau lance au sol pendant HelpTheWounded_Extinguish : on invoque un
// trigger a l'impact, on cherche tous les foyers d'incendie dans le rayon de
// l'effet, on credite le joueur pour chacun et on les fait disparaitre.
class spell_theramore_throw_bucket : public SpellScript
{
	void HandleDummy(SpellEffIndex effIndex)
	{
		Unit* caster = GetCaster();
		const WorldLocation* destination = GetHitDest();
		if (caster && destination)
		{
			SpellRange radius = GetSpellInfo()->GetEffect(effIndex).CalcRadius();

			// En debug, un seul seau suffit a valider toute l'etape.
			#ifdef CUSTOM_DEBUG
				for (uint8 i = 0; i < NUMBER_OF_FIRES; ++i)
				{
					if (Player* player = caster->ToPlayer())
						KillRewarder::Reward(player, caster, NPC_THERAMORE_FIRE_CREDIT);
				}
			#else
				if (Creature* trigger = caster->SummonCreature(WORLD_TRIGGER, destination->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, 5s))
				{
					std::list<Creature*> fires;
					trigger->GetCreatureListWithEntryInGrid(fires, NPC_THERAMORE_FIRE_CREDIT, radius.Max);

					for (Creature* fire : fires)
					{
						if (Player* player = caster->ToPlayer())
						{
							KillRewarder::Reward(player, caster, NPC_THERAMORE_FIRE_CREDIT);
						}

						fire->DespawnOrUnsummon();
					}
				}
			#endif
		}
	}

	void Register() override
	{
		OnEffectHitTarget += SpellEffectFn(spell_theramore_throw_bucket::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
	}
};

// Powder Keg - BasicEvent
// Chaine d'explosions le long du bateau : l'event se replanifie lui-meme
// toutes les 100-180ms et fait exploser un stalker de plus a chaque passage,
// ce qui donne l'effet de destruction en cascade.
class PowderKegEvent : public BasicEvent
{
	enum Spells
	{
		SPELL_LARGE_EXPLOSION   = 271199,
		SPELL_LARGE_FIRE        = 414772
	};

	public:
	PowderKegEvent(Creature* owner, std::vector<Creature*> triggers) : owner(owner), triggers(triggers)
	{
		counter = 0;
		maxCount = triggers.size();
	}

	bool Execute(uint64 timer, uint32 /*updateTime*/) override
	{
		if (counter >= maxCount)
			return true;

		if (Creature* temp = triggers[counter])
		{
			temp->RemoveAllAuras();
			temp->CastSpell(temp, SPELL_LARGE_EXPLOSION);
			temp->AddAura(SPELL_LARGE_FIRE, temp);
		}

		counter++;

		owner->m_Events.AddEvent(this, Milliseconds(timer) + randtime(100ms, 180ms));

		return false;
	}

	private:
	Creature* owner;
	uint8 counter;
	uint8 maxCount;
	std::vector<Creature*> triggers;
};

// Powder Keg - 205238
// Destruction du Sea Wolf : le joueur fait sauter le baril de poudre, ce qui
// valide l'objectif, supprime le baril, tue le capitaine Drok s'il est encore
// a bord, puis lance la cascade d'explosions (PowderKegEvent).
class spell_powder_keg : public SpellScript
{
	enum Spells
	{
		SPELL_BIG_FIRE_EXPLOSION    = 183880,
	};

	// Distance a laquelle Drok est considere comme etant a bord du bateau.
	static constexpr float DROK_KILL_RANGE      = 25.0f;
	// Rayon de recherche des stalkers porteurs des explosions en cascade.
	static constexpr float STALKER_SEARCH_RANGE = 50.0f;

	void HandleDummy(SpellEffIndex /*effIndex*/)
	{
		// Le test sur caster doit venir AVANT GetInstanceScript() : plus bas
		// le code retestait caster, ce qui arrivait trop tard.
		Unit* caster = GetCaster();
		if (!caster)
			return;

		if (InstanceScript* instance = caster->GetInstanceScript())
		{
			const WorldLocation* destination = GetHitDest();
			if (caster && destination)
			{
				if (Creature* trigger = caster->SummonCreature(WORLD_TRIGGER, destination->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, 1min))
				{
					trigger->RemoveAllAuras();
					trigger->CastSpell(trigger, SPELL_BIG_FIRE_EXPLOSION);

					// The Sea Wolf destroyed event
					instance->TriggerGameEvent(EVENT_DESTROY_SEA_WOLF);

					// Delete Powder Barrel targeted
					if (GameObject* powder = instance->GetGameObject(DATA_POWDER_BARREL))
						powder->Delete();

					// If the boat is destroyed and Captan Drok is alive, he dies
					if (Creature* drok = instance->GetCreature(DATA_CAPTAIN_DROK))
					{
						if (drok->IsAlive() && trigger->IsWithinDistInMap(drok, DROK_KILL_RANGE))
						{
							drok->KillSelf();
							if (Player* player = caster->ToPlayer())
							{
								KillRewarder::Reward(player, drok, NPC_CAPTAIN_DROK);
							}
						}
					}

					// If the boat is destroyed fire and explosions
					std::vector<Creature*> stalkers;
					trigger->GetCreatureListWithEntryInGrid(stalkers, NPC_INVISIBLE_STALKER, STALKER_SEARCH_RANGE);

					if (stalkers.empty())
						return;

					trigger->m_Events.AddEvent(new PowderKegEvent(trigger, stalkers), trigger->m_Events.CalculateTime(1s));
				}
			}
		}
	}

	void Register() override
	{
		OnEffectHit += SpellEffectFn(spell_powder_keg::HandleDummy, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
	}
};

// 279910 - Wild Imp
// Invocation des diablotins du demoniste, avec placement en formation :
// chaque imp occupe un emplacement fixe sur deux arcs de cercle derriere le
// lanceur. On deduit les emplacements deja pris en rattachant chaque imp
// existant a l'emplacement le plus proche, puis on invoque le nouvel imp sur
// le premier emplacement libre. Le MoveFollow final maintient la formation
// meme quand le lanceur se deplace ou pivote.
class spell_wild_imp : public SpellScript
{
	struct SlotDef
	{
		float radius;
		float angleFromBehind; // 0 = pile derriere ; positif = gauche du caster, negatif = droite
	};

	static constexpr uint8 IMP_MAX_COUNT = 12;

	// 5 imps sur l'arc interieur (R = 2.0m) + 7 sur l'arc exterieur (R = 3.5m),
	// repartis sur le demi-cercle exact derriere le lanceur (180 degres,
	// d'epaule a epaule).
	static constexpr std::array<SlotDef, IMP_MAX_COUNT> SLOTS = { {
		// Inner ring : 5 slots, pas de pi/4
		{ 2.0f, -float(M_PI_2) },
		{ 2.0f, -float(M_PI_4) },
		{ 2.0f,  0.f },
		{ 2.0f,  float(M_PI_4) },
		{ 2.0f,  float(M_PI_2) },
		// Outer ring : 7 slots, pas de pi/6
		{ 3.5f, -float(M_PI_2) },
		{ 3.5f, -float(M_PI) / 3.f },
		{ 3.5f, -float(M_PI) / 6.f },
		{ 3.5f,  0.f },
		{ 3.5f,  float(M_PI) / 6.f },
		{ 3.5f,  float(M_PI) / 3.f },
		{ 3.5f,  float(M_PI_2) },
	} };

	static Position SlotToWorld(Unit const* caster, SlotDef const& def)
	{
		float worldAngle = caster->GetOrientation() + float(M_PI) + def.angleFromBehind;
		float x = caster->GetPositionX() + def.radius * std::cos(worldAngle);
		float y = caster->GetPositionY() + def.radius * std::sin(worldAngle);
		return Position(x, y, caster->GetPositionZ(), caster->GetOrientation());
	}

	void HandleSummon(SpellEffIndex effIndex)
	{
		PreventHitDefaultEffect(effIndex);

		Unit* caster = GetCaster();
		if (!caster)
			return;

		SpellEffectInfo const& effInfo = GetEffectInfo();
		const uint32 wildImpEntry = effInfo.MiscValue;

		// Pre-calcul des 12 positions de slot dans la frame actuelle du caster.
		std::array<Position, IMP_MAX_COUNT> slotPositions;
		for (uint8 s = 0; s < IMP_MAX_COUNT; ++s)
			slotPositions[s] = SlotToWorld(caster, SLOTS[s]);

		// Derive les slots occupes via "nearest slot" : chaque imp est rattache au slot le plus proche.
		std::array<bool, IMP_MAX_COUNT> occupied{};
		uint8 count = 0;
		for (Unit* controlled : caster->m_Controlled)
		{
			if (!controlled || controlled->GetEntry() != wildImpEntry)
				continue;

			++count;

			uint8 bestSlot = 0;
			float bestDistSq = std::numeric_limits<float>::max();
			for (uint8 s = 0; s < IMP_MAX_COUNT; ++s)
			{
				float d2 = controlled->GetExactDist2dSq(&slotPositions[s]);
				if (d2 < bestDistSq)
				{
					bestDistSq = d2;
					bestSlot = s;
				}
			}
			ASSERT(bestSlot < IMP_MAX_COUNT);
			occupied[bestSlot] = true;
		}

		// Au-dela de la limite : le sort se cast quand meme, mais aucun summon.
		if (count >= IMP_MAX_COUNT)
			return;

		uint8 freeSlot = 0;
		for (; freeSlot < IMP_MAX_COUNT; ++freeSlot)
			if (!occupied[freeSlot])
				break;

		// Garde-fou : si la derivation "nearest slot" a sature tous les slots alors que count < 12 (theoriquement impossible), on abandonne.
		if (freeSlot >= IMP_MAX_COUNT)
			return;

		SummonPropertiesEntry const* properties = sSummonPropertiesStore.LookupEntry(uint32(effInfo.MiscValueB));
		Milliseconds duration = Milliseconds(GetSpellInfo()->CalcDuration(caster));

		TempSummon* summon = caster->GetMap()->SummonCreature(wildImpEntry, slotPositions[freeSlot], properties, duration, caster, GetSpellInfo()->Id);
		if (!summon)
			return;

		if (caster->IsInCombat())
		{
			spell_wild_imp_aura::AddImp(caster);

			Unit* victim = caster->GetVictim();
			if (victim)
				summon->Attack(victim, true);
		}

		// Chaque imp suit le lanceur dans son slot relatif : la formation se maintient meme si le caster bouge ou tourne.
		summon->GetMotionMaster()->MoveFollow(caster, SLOTS[freeSlot].radius, ChaseAngle(float(M_PI) + SLOTS[freeSlot].angleFromBehind));
	}

	void Register() override
	{
		OnEffectLaunch += SpellEffectFn(spell_wild_imp::HandleSummon, EFFECT_0, SPELL_EFFECT_SUMMON);
	}
};

// 464895 - Hand of Gul'dan
// Applique les degats de zone puis invoque autant de diablotins que la valeur
// de base de l'effet 0 (le nombre depend des fragments d'ame depenses).
class spell_hand_of_guldan : public SpellScript
{
	enum Spells
	{
		SPELL_HAND_OF_GULDAN            = 464895,
		SPELL_HAND_OF_GULDAN_DAMAGE     = 464890,
		SPELL_WILD_IMP                  = 279910,
	};

	bool Validate(SpellInfo const* /*spellInfo*/) override
	{
		return ValidateSpellInfo({ SPELL_HAND_OF_GULDAN_DAMAGE, SPELL_WILD_IMP });
	}

	void HandleDummy(SpellEffIndex effIndex)
	{
		Unit* caster = GetCaster();
		Unit* target = GetHitUnit();

		if (!caster || !target)
			return;

		caster->CastSpell(target->GetPosition(), SPELL_HAND_OF_GULDAN_DAMAGE, CastSpellExtraArgsInit{
			.TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
		});

		const uint8 wildImp = GetEffectInfo(effIndex).BasePoints;
		for (uint8 i = 0; i < wildImp; ++i)
			caster->CastSpell(caster, SPELL_WILD_IMP, CastSpellExtraArgsInit{
			.TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
		});
	}

	void Register() override
	{
		OnEffectHitTarget += SpellEffectFn(spell_hand_of_guldan::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
	}
};

// 295710 - Light of Dawn
// Cone de soin du paladin : l'effet dummy touche tout le monde dans le cone,
// on ne relaie le soin que vers les cibles amies.
class spell_light_of_dawn : public SpellScript
{
	enum Spells
	{
		SPELL_LIGHT_OF_DAWN_HEAL = 295712
	};

	bool Validate(SpellInfo const* /*spellInfo*/) override
	{
		return ValidateSpellInfo({ SPELL_LIGHT_OF_DAWN_HEAL });
	}

	void HandleDummy(SpellEffIndex /*effIndex*/) const
	{
		Unit* caster = GetCaster();
		Unit* target = GetHitUnit();

		if (!caster || !target)
			return;

		if (caster->IsValidAttackTarget(target))
			return;

		caster->CastSpell(target, SPELL_LIGHT_OF_DAWN_HEAL, CastSpellExtraArgsInit{
			.TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
			.TriggeringSpell = GetSpell()
			});
	}

	void Register() override
	{
		OnEffectHitTarget += SpellEffectFn(spell_light_of_dawn::HandleDummy, EFFECT_1, SPELL_EFFECT_DUMMY);
	}
};

// =========================================================================
//     AREATRIGGERS
// =========================================================================
// Zones au sol des sorts ci-dessus. Deux familles :
//   - a tick (OnCreate + scheduler) : elles appliquent leur effet a
//     intervalle regulier a tout ce qui se trouve dedans
//   - a l'entree / sortie (OnUnitEnter / OnUnitExit) : elles posent ou
//     retirent une aura au passage
// Toutes filtrent amis / ennemis, car un areatrigger touche par defaut tout
// ce qui entre dedans.

// Blizzard - 284968
// AreaTriggerID - 15411
// Zone de degats a tick : 1 salve par seconde sur les unites hostiles.
struct at_blizzard_theramore : AreaTriggerAI
{
	using AreaTriggerAI::AreaTriggerAI;

	enum Spells
	{
		SPELL_BLIZZARD_DAMAGE = 335953
	};

	void OnCreate(Spell const* /*creatingSpell*/) override
	{
		_scheduler.Schedule(1s, [this](TaskContext task)
		{
			if (Unit* caster = at->GetCaster())
			{
				for (ObjectGuid unit : at->GetInsideUnits())
				{
					if (Unit* target = ObjectAccessor::GetUnit(*caster, unit))
					{
						if (!caster->IsHostileTo(target))
							continue;

						caster->CastSpell(target, SPELL_BLIZZARD_DAMAGE, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
					}
				}
			}

			task.Repeat(1s);
		});
	}

	void OnUpdate(uint32 diff) override
	{
		_scheduler.Update(diff);
	}

private:
	TaskScheduler _scheduler;
};

// Consecrated Ground
// AreaTriggerID - 34355
// Zone persistante : l'aura est posee a l'entree et retiree a la sortie ou
// a la disparition de la zone.
struct at_consecration : AreaTriggerAI
{
	using AreaTriggerAI::AreaTriggerAI;

	enum Spells
	{
		SPELL_CONSECRATION = 461742
	};

	void OnUnitEnter(Unit* /*unit*/) override
	{
		if (Unit* caster = at->GetCaster())
		{
			for (ObjectGuid unit : at->GetInsideUnits())
			{
				if (Unit* target = ObjectAccessor::GetUnit(*caster, unit))
				{
					caster->CastSpell(target, SPELL_CONSECRATION, true);
				}
			}
		}
	}

	void OnUnitExit(Unit* unit, AreaTriggerExitReason /*reason*/) override
	{
		unit->RemoveAurasDueToSpell(SPELL_CONSECRATION);
	}

	void OnRemove() override
	{
		if (Unit* caster = at->GetCaster())
		{
			for (ObjectGuid unit : at->GetInsideUnits())
			{
				if (Unit* target = ObjectAccessor::GetUnit(*caster, unit))
				{
					target->RemoveAurasDueToSpell(SPELL_CONSECRATION);
				}
			}
		}
	}
};

// Divine Word: Sanctuary - 372784
// AreaTriggerID - 35546
// Zone de soin a tick, symetrique de Blizzard : elle ne soigne que les allies.
struct at_divine_word_sanctuary : AreaTriggerAI
{
	using AreaTriggerAI::AreaTriggerAI;

	enum Spells
	{
		SPELL_DIVINE_WORD_SANCTUARY_HEAL = 372787
	};

	void OnCreate(Spell const* /*creatingSpell*/) override
	{
		_scheduler.Schedule(1s, [this](TaskContext task)
		{
			// Recuperation du lanceur du sort
			if (Unit* caster = at->GetCaster())
			{
				for (const ObjectGuid& unitGuid : at->GetInsideUnits())
				{
					if (Unit* target = ObjectAccessor::GetUnit(*caster, unitGuid))
					{
						if (caster->IsHostileTo(target))
							continue;

						caster->CastSpell(target, SPELL_DIVINE_WORD_SANCTUARY_HEAL);
					}
				}
			}

			task.Repeat(1s);
		});
	}

	void OnUpdate(uint32 diff) override
	{
		_scheduler.Update(diff);
	}

private:
	TaskScheduler _scheduler;
};

// Uncontrolled Energy
// AreaTriggerID - 26658
// Piege a usage unique : le premier ennemi qui entre declenche l'explosion
// et la zone se supprime aussitot (SetDuration(0)).
struct at_uncontrolled_energy : AreaTriggerAI
{
	using AreaTriggerAI::AreaTriggerAI;

	enum Spells
	{
		SPELL_ARCANE_RIFT_EXPLOSION = 388996
	};

	void OnUnitEnter(Unit* unit) override
	{
		if (Unit* caster = at->GetCaster())
		{
			if (caster->IsFriendlyTo(unit))
				return;

			caster->CastSpell(at->GetPosition(), SPELL_ARCANE_RIFT_EXPLOSION, true);
			at->SetDuration(0);
		}
	}
};

// Scorched Earth - 373139
// AreaTriggerID - 25183
// Sol brulant : aura de degats posee tant que l ennemi reste dedans.
struct at_scorched_earth : AreaTriggerAI
{
	using AreaTriggerAI::AreaTriggerAI;

	enum Spells
	{
		SPELL_SCORCHED_EARTH = 372820
	};

	void OnUnitEnter(Unit* unit) override
	{
		if (Unit* caster = at->GetCaster())
		{
			if (caster->IsFriendlyTo(unit))
				return;

			unit->AddAura(SPELL_SCORCHED_EARTH, unit);
		}
	}

	void OnUnitExit(Unit* unit, AreaTriggerExitReason /*reason*/) override
	{
		unit->RemoveAurasDueToSpell(SPELL_SCORCHED_EARTH);
	}
};

// Arcane Orb - 440458
// AreaTriggerID - 32530
// Projectile au sol : la zone suit une spline en ligne droite sur 40 yards et
// frappe tout ennemi traverse, puis se supprime a l'arrivee.
struct at_arcane_orb : AreaTriggerAI
{
	using AreaTriggerAI::AreaTriggerAI;

	enum Spells
	{
		SPELL_ARCANE_ORB_DAMAGE = 440552
	};

	void OnInitialize() override
	{
		Position destPos = at->GetPosition();
		at->MovePositionToFirstCollision(destPos, 40.0f, 0.0f);

		std::vector<G3D::Vector3> points;
		points.emplace_back(at->GetPositionX(), at->GetPositionY(), at->GetPositionZ());
		points.emplace_back(destPos.GetPositionX(), destPos.GetPositionY(), destPos.GetPositionZ());

		// Avoid degenerate spline (zero length -> timeToTarget = 0 -> NaN in UpdateSplinePosition)
		if ((points[1] - points[0]).squaredMagnitude() < 0.01f)
		{
			at->Remove();
			return;
		}

		at->InitSplines(points);
	}

	void OnUnitEnter(Unit* unit) override
	{
		Unit* caster = at->GetCaster();
		if (!caster)
			return;

		if (caster->IsValidAttackTarget(unit))
			caster->CastSpell(unit, SPELL_ARCANE_ORB_DAMAGE, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CAST_IN_PROGRESS);
	}

	void OnDestinationReached() override
	{
		at->Remove();
	}
};

// Blessed Hammer - 420092
// AreaTriggerID - 29807
// Marteau tournoyant : la zone suit une spirale d'Archimede calculee a la
// creation autour du lanceur, et frappe les ennemis qu elle traverse.
struct at_blessed_hammer : AreaTriggerAI
{
	using AreaTriggerAI::AreaTriggerAI;

	enum Spells
	{
		SPELL_BLESSED_HAMMER_DAMAGE = 420091
	};

	void OnCreate(Spell const* /*creatingSpell*/) override
	{
		Unit* caster = at->GetCaster();
		if (!caster)
			return;

		Position center  = caster->GetPosition();
		float startAngle = caster->GetOrientation();
		float fixedZ     = center.GetPositionZ() + Z_OFFSET;

		std::vector<G3D::Vector3> path;
		path.reserve(POINTS + 1);

		for (std::size_t i = 0; i <= POINTS; ++i)
		{
			float t     = float(i) / float(POINTS);
			float theta = t * TURNS * 2.0f * float(M_PI);
			float r     = R0 + GROWTH * theta;

			float x = center.GetPositionX() + r * std::cos(startAngle + theta);
			float y = center.GetPositionY() + r * std::sin(startAngle + theta);

			path.emplace_back(x, y, fixedZ);
		}

		at->InitSplines(std::move(path));
	}

	void OnUnitEnter(Unit* unit) override
	{
		Unit* caster = at->GetCaster();
		if (!caster)
			return;

		if (caster->IsValidAttackTarget(unit))
			caster->CastSpell(unit, SPELL_BLESSED_HAMMER_DAMAGE, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CAST_IN_PROGRESS);
	}

private:
	// AreaTrigger 29807 dure 5 s : on etale la spirale sur toute la duree.
	static constexpr std::size_t POINTS = 200;    // Densite totale (40 pts/seconde)
	static constexpr float TURNS        = 10.0f;  // 10 tours sur 5 s = 2 tours/s
	static constexpr float R0           = 2.0f;   // Rayon de depart
	static constexpr float GROWTH       = 0.35f;  // Expansion par radian (spirale d'Archimede)
	static constexpr float Z_OFFSET     = 1.5f;   // Hauteur du marteau au-dessus du sol
};

// =========================================================================
//     Registration
// =========================================================================
void AddSC_npcs_battle_for_theramore()
{
	RegisterTheramoreAI(npc_theramore_citizen);
	RegisterTheramoreAI(npc_thader_windermere);
	RegisterTheramoreAI(npc_hedric_evencane);
	RegisterTheramoreAI(npc_unmanned_tank);
	RegisterTheramoreAI(npc_theramore_officier);
	RegisterTheramoreAI(npc_theramore_footman);
	RegisterTheramoreAI(npc_theramore_faithful);
	RegisterTheramoreAI(npc_theramore_arcanist);
	RegisterTheramoreAI(npc_theramore_marksman);
	RegisterTheramoreAI(npc_wounded_theramore_troop);
	RegisterTheramoreAI(npc_faithful_training);
	RegisterTheramoreAI(npc_arcanist_training);
	RegisterTheramoreAI(npc_dummy_training);
	RegisterTheramoreAI(npc_wave_caller_gruhta);

	// Utilisables dans les Ruines de Theramore
	RegisterCreatureAI(npc_roknah_hag);
	RegisterCreatureAI(npc_roknah_grunt);
	RegisterCreatureAI(npc_roknah_loasinger);
	RegisterCreatureAI(npc_roknah_felcaster);

	RegisterSpellScript(spell_roknah_hag_flurry);
	RegisterSpellScript(spell_theramore_light_of_dawn);
	RegisterSpellScript(spell_theramore_throw_bucket);
	RegisterSpellScript(spell_powder_keg);
	RegisterSpellScript(spell_wild_imp);
	RegisterSpellScript(spell_hand_of_guldan);
	RegisterSpellScript(spell_light_of_dawn);

	RegisterAreaTriggerAI(at_blizzard_theramore);
	RegisterAreaTriggerAI(at_consecration);
	RegisterAreaTriggerAI(at_divine_word_sanctuary);
	RegisterAreaTriggerAI(at_uncontrolled_energy);
	RegisterAreaTriggerAI(at_scorched_earth);
	RegisterAreaTriggerAI(at_arcane_orb);
	RegisterAreaTriggerAI(at_blessed_hammer);
}
