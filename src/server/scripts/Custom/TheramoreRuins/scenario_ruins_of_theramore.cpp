/*
 * Ruins of Theramore - InstanceScript principal
 *
 * Deroule d'apres Tides of War (voir ruins_of_theramore.h pour les etapes) :
 *
 *   Ilot (events 1-19)
 *     Kalecgos rejoint Jaina, elle refuse de l'ecouter et se teleporte seule.
 *     Kalec envoie les joueurs a sa suite (EVENT_HELP_KALECGOS).
 *
 *   Parcours (events 100-149)
 *     Jaina marche de lieu en lieu (RFTLegs). A chaque arret elle joue sa scene,
 *     declenche l'event du lieu, puis attend qu'un joueur la rejoigne
 *     (EVT_TRAIL_WAIT_PLAYERS) avant de repartir.
 *
 *   Kinndy et l'eclat (events 200-206)
 *     Kinndy tombe en poussiere, Jaina hurle, ramasse l'eclat de miroir :
 *     EVENT_FIND_JAINA_02 termine l'etape 5014.
 *
 *   Pillards (events 300-350)
 *     Groupe 0 de creature_summon_groups (chef compris). Le chef ne meurt pas :
 *     a 20 % il s'agenouille. Quand l'etape 5015 est validee ET que le chef est
 *     a genoux, Jaina l'acheve (events 400-405).
 *
 *   Docks, cratere, sortie (events 500-702)
 *
 * Chaque bloc enchaine ses events par Next() (eventId + 1). Les blocs ne se
 * suivent pas : ils demarrent a l'arrivee d'un trajet de Jaina (OnJainaArrived)
 * ou a la validation d'un critere (OnCompletedCriteriaTree).
 *
 * Commentaires en francais sans accents (encodage TC).
 */

#include "CustomAI.h"
#include "EventMap.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "MiscPackets.h"
#include "MotionMaster.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellAuras.h"
#include "TemporarySummon.h"
#include "Weather.h"
#include "ruins_of_theramore.h"
#include "../CustomScenario.h"

// =========================================================================
// Tables de correspondance NPC / GO <-> Data ID
// =========================================================================
const ObjectData creatureData[] =
{
	{ NPC_JAINA_PROUDMOORE,     DATA_JAINA_PROUDMOORE       },
	{ NPC_KALECGOS,             DATA_KALECGOS               },
	{ NPC_KINNDY_SPARKSHINE,    DATA_KINNDY_SPARKSHINE      },
	{ NPC_ROKNAH_WARLORD,       DATA_ROKNAH_WARLORD         },
	{ NPC_KNIGHT_OF_THERAMORE,  DATA_FLOATING_KNIGHT        },
	{ NPC_BLACK_RAT,            DATA_BLACK_RAT              },
	{ NPC_PAINED,               DATA_PAINED                 },
	{ NPC_MARCUS_JONATHAN,      DATA_MARCUS_JONATHAN        },
	{ 0,                        0                           }   // END
};

const ObjectData gameobjectData[] =
{
	{ GOB_BROKEN_GLASS,         DATA_BROKEN_GLASS           },
	{ 0,                        0                           }   // END
};

// =========================================================================
// Trajets de Jaina : le chemin trace (header) quand il existe, sinon tout droit
// vers un point fixe ou en approche d'une cible (creature ou GO).
// =========================================================================
struct JainaLeg
{
	WaypointPath const* path;   // Chemin trace (nullptr : repli en ligne droite)
	Position const* point;      // Point fixe (nullptr : on approche targetData)
	uint32 targetData;          // Data ID de la cible a approcher
	float approach;             // Distance d'arret devant la cible
	bool run;                   // Repli uniquement : le chemin porte son propre MoveType
};

const JainaLeg JainaLegs[(uint32)RFTLegs::Max] =
{
	/* TrailStart   */ { &JainaPathTrailStart, &TrailStartPoint,   0,                      0.0f,  true  },
	/* Armory       */ { &JainaPathArmory,     nullptr,            DATA_FLOATING_KNIGHT,   3.0f,  false },
	/* Rat          */ { &JainaPathRat,        nullptr,            DATA_BLACK_RAT,         1.0f,  false },
	/* Pained       */ { &JainaPathPained,     nullptr,            DATA_PAINED,            1.5f,  false },
	/* Generals     */ { &JainaPathGenerals,   nullptr,            DATA_MARCUS_JONATHAN,   3.0f,  false },
	/* Kinndy       */ { &JainaPathKinndy,     nullptr,            DATA_KINNDY_SPARKSHINE, 1.2f,  false },
	/* BrokenGlass  */ { nullptr,              nullptr,            DATA_BROKEN_GLASS,      0.8f,  false },
	/* Looters      */ { &JainaPathLooters,    nullptr,            DATA_ROKNAH_WARLORD,    25.0f, false },
	/* Finisher     */ { nullptr,              nullptr,            DATA_ROKNAH_WARLORD,    1.3f,  false },
	/* Docks        */ { &JainaPathDocks,      &DocksPoint,        0,                      0.0f,  false },
	/* CraterRim    */ { &JainaPathCraterRim,  &CraterRimPoint,    0,                      0.0f,  false },
	/* CraterBottom */ { &JainaPathCraterBottom, &CraterBottomPoint, 0,                      0.0f,  false }
};

// =========================================================================
// Groupes de creatures spawnes par SummonCreatureGroup (creature_summon_groups)
// =========================================================================
enum RFTCreatureGroups : uint32
{
	CREATURE_GROUP_LOOTERS      = 0     // Les pillards et leur chef
};

// =========================================================================
// Identifiants des evenements internes de l'EventMap
// =========================================================================
// Dans un bloc, l'ordre numerique est SIGNIFICATIF : Next() incremente eventId
// de 1. Les blocs (100, 200...) sont lances explicitement.
enum RFTEvents : uint32
{
	// Ilot
	EVT_ISLE_JAINA_WALK             = 1,    // Jaina marche jusqu'a JainaPoint01
	EVT_ISLE_KALECGOS_GREET         = 2,    // "Jaina ?"
	EVT_ISLE_JAINA_TALK_01          = 3,
	EVT_ISLE_JAINA_TALK_02          = 4,
	EVT_ISLE_KALECGOS_MOVE          = 5,    // "Vous n'y etes pour rien" + Kalec avance
	EVT_ISLE_JAINA_TALK_03          = 6,
	EVT_ISLE_KALECGOS_TALK_03       = 7,
	EVT_ISLE_JAINA_TALK_04          = 8,
	EVT_ISLE_KALECGOS_TALK_04       = 9,
	EVT_ISLE_JAINA_TALK_05          = 10,
	EVT_ISLE_KALECGOS_TALK_05       = 11,
	EVT_ISLE_KALECGOS_TALK_06       = 12,
	EVT_ISLE_JAINA_TALK_06          = 13,   // "Ne me suivez pas."
	EVT_ISLE_ECHO_OF_ALUNETH        = 14,   // Jaina lance sa teleportation
	EVT_ISLE_ALUNETH_FREED          = 15,   // Explosion arcanique, Jaina se dissout
	EVT_ISLE_JAINA_HIDE             = 16,
	EVT_ISLE_TELEPORT_PREP          = 17,   // Point d'entree DEBUG - Kalec : "mais vous oui"
	EVT_ISLE_JAINA_ARRIVES          = 18,   // Jaina apparait dans les ruines et court
	EVT_ISLE_TELEPORT_PLAYERS       = 19,   // Les joueurs la suivent

	// Parcours
	EVT_TRAIL_WAIT_PLAYERS          = 100,  // S'auto-replanifie tant que personne n'a rejoint Jaina
	EVT_TRAIL_ARMORY_TALK           = 110,  // Emote : le soldat dans son sang
	EVT_TRAIL_ARMORY_FLOATING       = 111,  // ... et celui qui flotte a trois pas
	EVT_TRAIL_RAT_TALK              = 120,
	EVT_TRAIL_RAT_SURVIVORS         = 121,
	EVT_TRAIL_PAINED_KNEEL          = 130,
	EVT_TRAIL_PAINED_HAIR           = 131,
	EVT_TRAIL_PAINED_TALK           = 132,
	EVT_TRAIL_PAINED_STAND          = 133,
	EVT_TRAIL_GENERALS_TALK         = 140,

	// Kinndy et l'eclat de miroir
	EVT_KINNDY_KNEEL                = 200,
	EVT_KINNDY_TOUCH                = 201,  // Kinndy tombe en poussiere violette
	EVT_KINNDY_SCREAM               = 202,
	EVT_KINNDY_RAGE                 = 203,
	EVT_KINNDY_STAND                = 204,  // Jaina va vers l'eclat
	EVT_GLASS_PICK                  = 205,
	EVT_GLASS_TAKEN                 = 206,  // EVENT_FIND_JAINA_02

	// Pillards
	EVT_LOOTERS_NOTICE              = 300,  // Le chef remarque Jaina
	EVT_LOOTERS_HEAD                = 301,
	EVT_LOOTERS_ATTACK              = 302,
	EVT_LOOTERS_AGGRO_NUDGE         = 350,  // Watchdog : relance les pillards restes passifs

	// Le dernier survivant
	EVT_FINISHER_WALK               = 400,
	EVT_FINISHER_JAINA_01           = 401,
	EVT_FINISHER_WARLORD            = 402,
	EVT_FINISHER_JAINA_02           = 403,
	EVT_FINISHER_STAB               = 404,
	EVT_FINISHER_KILL               = 405,

	// Docks
	EVT_DOCKS_WALK                  = 500,
	EVT_DOCKS_TALK_01               = 501,
	EVT_DOCKS_TALK_02               = 502,
	EVT_DOCKS_LEAVE                 = 503,

	// Cratere
	EVT_CRATER_RIM_TALK             = 600,
	EVT_CRATER_DESCEND              = 601,
	EVT_IRIS_TALK_01                = 602,
	EVT_IRIS_CHANNEL                = 603,
	EVT_IRIS_TALK_02                = 604,
	EVT_IRIS_SEAL                   = 605,
	EVT_IRIS_TALK_03                = 606,
	EVT_IRIS_SEALED                 = 607,

	// Sortie
	EVT_LEAVE_TALK                  = 700,
	EVT_LEAVE_OPEN_PORTAL           = 701,
	EVT_LEAVE_DESPAWN_FINAL         = 702
};

class scenario_ruins_of_theramore : public InstanceMapScript
{
	public:
	scenario_ruins_of_theramore() : InstanceMapScript(RFTScriptName, 5001)
	{
	}

	struct scenario_ruins_of_theramore_InstanceScript : public InstanceScript
	{
		scenario_ruins_of_theramore_InstanceScript(InstanceMap* map) : InstanceScript(map),
			eventId(EVT_ISLE_JAINA_WALK), phase(RFTPhases::FindJaina_Isle),
			pendingLeg(RFTLegs::Max), cleaningDone(false), warlordKneeling(false), finisherStarted(false), lootersCredited(0)
		{
			SetHeaders(DataHeader);
			LoadObjectData(creatureData, gameobjectData);
		}

		// Skybox : celle de l'ilot jusqu'a l'arrivee dans les ruines (phase Trail,
		// posee au teleport des joueurs), celle des ruines ensuite.
		static constexpr CustomScenario::PhaseAura PhaseAuras[] =
		{
			{ SPELL_SKYBOX_EFFECT_ENTRANCE, 0,                         (uint32)RFTPhases::Trail },
			{ SPELL_SKYBOX_EFFECT_RUINS,    (uint32)RFTPhases::Trail,  CustomScenario::PhaseAura::ToEnd }
		};

		// =================================================================
		// Etat interne
		// =================================================================
		EventMap events;
		CustomScenario::DialogueClock dialogue;   // Temps de parole : Next() attend la fin de la replique
		uint32 eventId;                       // Dernier event execute (sert a Next() pour planifier eventId+1)
		RFTPhases phase;
		ObjectGuid irisDummy;                 // Dummy invisible portant les visuels de l'iris
		RFTLegs pendingLeg;                   // Trajet que Jaina reprendra quand un joueur l'aura rejointe

		// Pillards suivis par GUID (chef exclu) : le watchdog ne relance que ceux-la.
		GuidVector lootersWave;
		GuidVector standards;                 // Etendards pilles (GO 214965), vignette posee avec les pillards
		bool cleaningDone;                    // Etape 5015 validee (pillards tues + etendards)
		bool warlordKneeling;                 // Le chef est a genoux
		bool finisherStarted;
		uint32 lootersCredited;               // Events EVENT_LOOTER_SLAIN envoyes pour la vague en cours

		// =================================================================
		// Lecture / ecriture de donnees externes
		// =================================================================
		uint32 GetData(uint32 dataId) const override
		{
			if (dataId == DATA_SCENARIO_PHASE)
				return (uint32)phase;
			return 0U;
		}

		void OnPlayerEnter(Player* player) override
		{
			CustomScenario::SyncPhaseAuras(player, (uint32)phase, PhaseAuras);
		}

		void OnPlayerLeave(Player* player) override
		{
			CustomScenario::RemovePhaseAuras(player, PhaseAuras);
		}

		void SetData(uint32 dataId, uint32 value) override
		{
			switch (dataId)
			{
				case DATA_SCENARIO_PHASE:
					phase = (RFTPhases)value;
					CustomScenario::SyncPhaseAuras(instance, value, PhaseAuras);
					break;
				case DATA_JAINA_ARRIVED:
					OnJainaArrived((RFTLegs)value);
					break;
				case DATA_WARLORD_KNEELING:
					warlordKneeling = true;
					TryStartFinisher();
					break;
				default:
					break;
			}
		}

		void OnUnitDeath(Unit* unit) override
		{
			InstanceScript::OnUnitDeath(unit);

			if (!std::erase(lootersWave, unit->GetGUID()))
				return;

			// Credit de l'etape 5015 quel que soit le tueur : un critere de kill
			// ne compte pas les pillards tues par Jaina.
			TriggerGameEvent(EVENT_LOOTER_SLAIN);
			++lootersCredited;

			if (!lootersWave.empty())
				return;

			events.CancelEvent(EVT_LOOTERS_AGGRO_NUDGE);

			// Filet de securite : plus aucun pillard debout, le compteur doit etre
			// plein. S'il manque des credits (pillard jamais apparu, mort perdue),
			// on les complete plutot que de bloquer l'etape.
			if (lootersCredited < LOOTERS_REQUIRED)
			{
				TC_LOG_ERROR("scripts", "Ruins of Theramore: {} looter(s) credited out of {}, completing the counter.",
					lootersCredited, LOOTERS_REQUIRED);

				for (; lootersCredited < LOOTERS_REQUIRED; ++lootersCredited)
					TriggerGameEvent(EVENT_LOOTER_SLAIN);
			}
		}

		// =================================================================
		// Reaction aux criteres remplis
		// =================================================================
		void OnCompletedCriteriaTree(CriteriaTree const* tree) override
		{
			switch (tree->ID)
			{
				case CRITERIA_TREE_FIND_JAINA_01:
					OnCriteriaFindJainaIsle();
					break;
				case CRITERIA_TREE_HELP_KALECGOS:
					// Les joueurs sont dans les ruines, Jaina court deja devant eux.
					SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::Trail);
					break;
				case CRITERIA_TREE_FIND_JAINA_02:
					OnCriteriaFindJainaKinndy();
					break;
				case CRITERIA_TREE_CLEANING:
					cleaningDone = true;
					TryStartFinisher();
					break;
				case CRITERIA_TREE_LAST_SURVIVOR:
					SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::BackToSender);
					events.ScheduleEvent(EVT_DOCKS_WALK, 4s);
					break;
				case CRITERIA_TREE_BACK_TO_SENDER:
					SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::LeaveTheRuins);
					events.ScheduleEvent(EVT_LEAVE_TALK, 2s);
					break;
				default:
					break;
			}
		}

		// 5012 : Kalecgos arrive et la cinematique de l'ilot demarre.
		void OnCriteriaFindJainaIsle()
		{
			instance->SummonCreature(NPC_KALECGOS, KalecgosPoint01);
			if (Creature* kalecgos = GetKalecgos())
			{
				kalecgos->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);

				kalecgos->GetMotionMaster()->MovePath(KalecgosPath01, false);
			}
			SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::FindJaina_Isle_Valided);

			// En DEBUG on saute directement a la fin de la cinematique.
			#ifdef CUSTOM_DEBUG
				events.ScheduleEvent(EVT_ISLE_TELEPORT_PREP, 1s);
			#else
				events.ScheduleEvent(EVT_ISLE_JAINA_WALK, 1s);
			#endif
		}

		// 5014 : Jaina a ramasse l'eclat. Les pillards apparaissent en train de
		// fouiller les decombres, Jaina s'avance vers eux.
		void OnCriteriaFindJainaKinndy()
		{
			std::list<TempSummon*> spawned;
			instance->SummonCreatureGroup(CREATURE_GROUP_LOOTERS, &spawned);

			// Les etendards a reprendre apparaissent sur la carte avec leurs pillards.
			for (ObjectGuid const& guid : standards)
				if (GameObject* standard = instance->GetGameObject(guid))
					standard->SetVignette(VIGNETTE_PILLAGED_STANDARD);

			lootersWave.clear();
			lootersCredited = 0;
			for (TempSummon* looter : spawned)
			{
				// Les corps restent sur place, comme ceux de Theramore.
				looter->SetTempSummonType(TEMPSUMMON_MANUAL_DESPAWN);
				looter->SetImmuneToAll(true);
				looter->SetReactState(REACT_PASSIVE);
				looter->SetEmoteState(RAND(EMOTE_STATE_USE_STANDING, EMOTE_STATE_LOOT));

				// Le chef ne meurt pas des joueurs (il s'agenouille a 20 %) :
				// il reste hors de la vague.
				if (looter->GetEntry() != NPC_ROKNAH_WARLORD)
					lootersWave.push_back(looter->GetGUID());
			}

			if (lootersWave.size() != LOOTERS_REQUIRED)
				TC_LOG_ERROR("scripts", "Ruins of Theramore: {} looter(s) spawned, the scenario expects {} (creature_summon_groups 5001 / 0).",
					lootersWave.size(), LOOTERS_REQUIRED);

			SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::Looters);
			MoveJaina(RFTLegs::Looters);
		}

		// =================================================================
		// Initialisation des creatures / gameobjects
		// =================================================================
		void OnCreatureCreate(Creature* creature) override
		{
			InstanceScript::OnCreatureCreate(creature);

			creature->SetVisibilityDistanceOverride(VisibilityDistanceType::Large);
			creature->SetBoundingRadius(CREATURE_BOUNDING_RADIUS);

			switch (creature->GetEntry())
			{
				case NPC_JAINA_PROUDMOORE:
					creature->SetVignette(VIGNETTE_JAINA_PROUDMOORE);
					break;
				case NPC_DEAD_ROKNAH_TROOP:
					FeignDeath(creature);
					// 50% des cadavres ont un effet visuel aleatoire (glace ou feu).
					if (roll_chance(50))
						creature->AddAura(RAND(SPELL_GLACIAL_SPIKE_COSMETIC, SPELL_BURNING), creature);
					break;
				case NPC_GENERAL_TIRAS_ALAN:
				case NPC_ADMIRAL_AUBREY:
				case NPC_HEDRIC_EVENCANE:
				case NPC_THERAMORE_FAITHFUL:
				case NPC_THERAMORE_ARCANIST:
				case NPC_THERAMORE_OFFICER:
				case NPC_THERAMORE_FOOTMAN:
				case NPC_KNIGHT_OF_THERAMORE:
				case NPC_SWIFT_WHITE_STEED:
				case NPC_ARCHMAGE_TERVOSH:
				case NPC_PAINED:
				case NPC_THADDUS_STOUTBLOW:
				case NPC_HORRAN_REDMANE:
				case NPC_LIEUTENANT_ADEN:
				case NPC_MARCUS_JONATHAN:
					SetupCorpse(creature);
					break;
				case NPC_KINNDY_SPARKSHINE:
					// Face contre terre dans son sang, mais deja gorgee d'arcane :
					// elle tombera en poussiere violette quand Jaina la touchera.
					SetupCorpse(creature);
					creature->AddAura(SPELL_BLOOD_STAIN, creature);
					break;
				case NPC_BLACK_RAT:
					// Le rat luit de violet : pas de poussiere scintillante, juste l'arcane.
					FeignDeath(creature);
					creature->SetUninteractible(true);
					creature->AddAura(SPELL_COSMETIC_PURPLE_VERTEX_STATE, creature);
					break;
				default:
					break;
			}
		}

		// Cadavre du souffle de la bombe : faux-mort, non-interactible et auras
		// arcaniques. Deux spawns s'en ecartent par leur StringId (voir le header).
		void SetupCorpse(Creature* creature)
		{
			// A trois pas du soldat de l'armurerie, un autre flotte a hauteur des
			// yeux : pas de faux-mort (il resterait couche), l'animation de noye le
			// laisse suspendu. La hauteur est portee par le spawn, on coupe la gravite.
			bool const floating = creature->HasStringId(STRING_ID_CORPSE_FLOATING);
			if (floating)
			{
				creature->RemoveAllAuras();
				creature->SetRegenerateHealth(false);
				creature->SetImmuneToAll(true);
				creature->SetDisableGravity(true);
				creature->AddAura(SPELL_DROWNED_ANIM, creature);
			}
			else
				FeignDeath(creature);

			creature->SetUninteractible(true);

			// Le soldat de l'armurerie est mort dans son sang, sans trace de la bombe.
			if (creature->HasStringId(STRING_ID_CORPSE_BLOOD))
			{
				creature->AddAura(SPELL_BLOOD_STAIN, creature);
				return;
			}

			creature->AddAura(SPELL_SHIMMERDUST, creature);
			creature->AddAura(SPELL_COSMETIC_PURPLE_VERTEX_STATE, creature);
			creature->AddAura(SPELL_ARCANE_DEBUFF_VISUAL, creature);
		}

		void OnGameObjectCreate(GameObject* go) override
		{
			InstanceScript::OnGameObjectCreate(go);

			switch (go->GetEntry())
			{
				case GOB_BROKEN_GLASS:
				case GOB_LOST_PAGE:
					go->SetFlag(GO_FLAG_NOT_SELECTABLE);
					break;
				case GOB_THERAMORE_BANNER:
					// Leur vignette n'apparait qu'avec les pillards (OnCriteriaFindJainaKinndy).
					standards.push_back(go->GetGUID());
					break;
				default:
					break;
			}
		}

		// =================================================================
		// Dispatcher principal : delegue chaque event a son handler
		// =================================================================
		void Update(uint32 diff) override
		{
			events.Update(diff);
			dialogue.Reset();

			switch (eventId = events.ExecuteEvent())
			{
				#pragma region ISLE
				case EVT_ISLE_JAINA_WALK:       HandleIsleJainaWalk();        break;
				case EVT_ISLE_KALECGOS_GREET:   HandleIsleKalecgosGreet();    break;
				case EVT_ISLE_JAINA_TALK_01:    TalkAndNext(GetJaina(),    SAY_ISLE_JAINA_01,    9s);  break;
				case EVT_ISLE_JAINA_TALK_02:    TalkAndNext(GetJaina(),    SAY_ISLE_JAINA_02,    16s); break;
				case EVT_ISLE_KALECGOS_MOVE:    HandleIsleKalecgosMove();     break;
				case EVT_ISLE_JAINA_TALK_03:    TalkAndNext(GetJaina(),    SAY_ISLE_JAINA_03,    6s);  break;
				case EVT_ISLE_KALECGOS_TALK_03: TalkAndNext(GetKalecgos(), SAY_ISLE_KALECGOS_03, 5s);  break;
				case EVT_ISLE_JAINA_TALK_04:    TalkAndNext(GetJaina(),    SAY_ISLE_JAINA_04,    8500ms); break;
				case EVT_ISLE_KALECGOS_TALK_04: TalkAndNext(GetKalecgos(), SAY_ISLE_KALECGOS_04, 9s);  break;
				case EVT_ISLE_JAINA_TALK_05:    TalkAndNext(GetJaina(),    SAY_ISLE_JAINA_05,    3s);  break;
				case EVT_ISLE_KALECGOS_TALK_05: TalkAndNext(GetKalecgos(), SAY_ISLE_KALECGOS_05, 9500ms);  break;
				case EVT_ISLE_KALECGOS_TALK_06: TalkAndNext(GetKalecgos(), SAY_ISLE_KALECGOS_06, 8s);  break;
				case EVT_ISLE_JAINA_TALK_06:    TalkAndNext(GetJaina(),    SAY_ISLE_JAINA_06,    13500ms); break;
				case EVT_ISLE_ECHO_OF_ALUNETH:  HandleIsleEchoOfAluneth();    break;
				case EVT_ISLE_ALUNETH_FREED:    HandleIsleAlunethFreed();     break;
				case EVT_ISLE_JAINA_HIDE:       HandleIsleJainaHide();        break;
				case EVT_ISLE_TELEPORT_PREP:    HandleIsleTeleportPrep();     break;
				case EVT_ISLE_JAINA_ARRIVES:    HandleIsleJainaArrives();     break;
				case EVT_ISLE_TELEPORT_PLAYERS: HandleIsleTeleportPlayers();  break;
				#pragma endregion

				#pragma region TRAIL
				case EVT_TRAIL_WAIT_PLAYERS:    HandleTrailWaitPlayers();     break;
				case EVT_TRAIL_ARMORY_TALK:     HandleTrailArmoryTalk();      break;
				case EVT_TRAIL_ARMORY_FLOATING: HandleTrailArmoryFloating();  break;
				case EVT_TRAIL_RAT_TALK:        HandleTrailRatTalk();         break;
				case EVT_TRAIL_RAT_SURVIVORS:   HandleTrailRatSurvivors();    break;
				case EVT_TRAIL_PAINED_KNEEL:    HandleTrailPainedKneel();     break;
				case EVT_TRAIL_PAINED_HAIR:     TalkAndNext(GetJaina(), SAY_TRAIL_PAINED_HAIR_EMOTE, 3s); break;
				case EVT_TRAIL_PAINED_TALK:     HandleTrailPainedTalk();      break;
				case EVT_TRAIL_PAINED_STAND:    HandleTrailPainedStand();     break;
				case EVT_TRAIL_GENERALS_TALK:   HandleTrailGeneralsTalk();    break;
				#pragma endregion

				#pragma region KINNDY
				case EVT_KINNDY_KNEEL:          HandleKinndyKneel();          break;
				case EVT_KINNDY_TOUCH:          HandleKinndyTouch();          break;
				case EVT_KINNDY_SCREAM:         TalkAndNext(GetJaina(), SAY_KINNDY_SCREAM_EMOTE, 3s); break;
				case EVT_KINNDY_RAGE:           TalkAndNext(GetJaina(), SAY_KINNDY_RAGE_JAINA,   8500ms); break;
				case EVT_KINNDY_STAND:          HandleKinndyStand();          break;
				case EVT_GLASS_PICK:            HandleGlassPick();            break;
				case EVT_GLASS_TAKEN:           HandleGlassTaken();           break;
				#pragma endregion

				#pragma region LOOTERS
				case EVT_LOOTERS_NOTICE:        HandleLootersNotice();        break;
				case EVT_LOOTERS_HEAD:          TalkAndNext(GetWarlord(), SAY_LOOTERS_WARLORD_HEAD, 4500ms); break;
				case EVT_LOOTERS_ATTACK:        HandleLootersAttack();        break;
				case EVT_LOOTERS_AGGRO_NUDGE:   HandleLootersAggroNudge();    break;
				#pragma endregion

				#pragma region LAST_SURVIVOR
				case EVT_FINISHER_WALK:         MoveJaina(RFTLegs::Finisher); break;
				case EVT_FINISHER_JAINA_01:     TalkAndNext(GetJaina(),   SAY_FINISHER_JAINA_01, 6500ms); break;
				case EVT_FINISHER_WARLORD:      TalkAndNext(GetWarlord(), SAY_FINISHER_WARLORD,  4s); break;
				case EVT_FINISHER_JAINA_02:     TalkAndNext(GetJaina(),   SAY_FINISHER_JAINA_02, 9s); break;
				case EVT_FINISHER_STAB:         HandleFinisherStab();         break;
				case EVT_FINISHER_KILL:         HandleFinisherKill();         break;
				#pragma endregion

				#pragma region BACK_TO_SENDER
				case EVT_DOCKS_WALK:            MoveJaina(RFTLegs::Docks);    break;
				case EVT_DOCKS_TALK_01:         TalkAndNext(GetJaina(), SAY_DOCKS_JAINA_01, 5s); break;
				case EVT_DOCKS_TALK_02:         TalkAndNext(GetJaina(), SAY_DOCKS_JAINA_02, 3s); break;
				case EVT_DOCKS_LEAVE:           MoveJaina(RFTLegs::CraterRim); break;
				case EVT_CRATER_RIM_TALK:       TalkAndNext(GetJaina(), SAY_CRATER_RIM_JAINA, 4s); break;
				case EVT_CRATER_DESCEND:        MoveJaina(RFTLegs::CraterBottom); break;
				case EVT_IRIS_TALK_01:          TalkAndNext(GetJaina(), SAY_IRIS_JAINA_01, 5s); break;
				case EVT_IRIS_CHANNEL:          HandleIrisChannel();          break;
				case EVT_IRIS_TALK_02:          TalkAndNext(GetJaina(), SAY_IRIS_JAINA_02, 3500ms); break;
				case EVT_IRIS_SEAL:             HandleIrisSeal();             break;
				case EVT_IRIS_TALK_03:          TalkAndNext(GetJaina(), SAY_IRIS_JAINA_03, 4s); break;
				case EVT_IRIS_SEALED:           HandleIrisSealed();           break;
				#pragma endregion

				#pragma region LEAVE_THE_RUINS
				case EVT_LEAVE_TALK:            HandleLeaveTalk();            break;
				case EVT_LEAVE_OPEN_PORTAL:     HandleLeaveOpenPortal();      break;
				case EVT_LEAVE_DESPAWN_FINAL:   HandleLeaveDespawnFinal();    break;
				#pragma endregion

				default:
					break;
			}
		}

		// =================================================================
		// Trajets de Jaina
		// =================================================================

		// Lance un trajet : son chemin trace s il en a un, sinon le repli de JainaLegs.
		void MoveJaina(RFTLegs leg)
		{
			Creature* jaina = GetJaina();
			if (!jaina)
				return;

			JainaLeg const& data = JainaLegs[(uint32)leg];
			jaina->SetWalk(!data.run);

			// Chemin trace : l'arrivee revient par WaypointPathEnded.
			if (data.path)
			{
				jaina->GetMotionMaster()->MovePath(*data.path, false);
				return;
			}

			uint32 const pointId = MOVEMENT_JAINA_LEG_BASE + (uint32)leg;
			if (data.point)
			{
				jaina->GetMotionMaster()->MovePoint(pointId, *data.point, true, data.point->GetOrientation());
				return;
			}

			if (data.targetData == DATA_BROKEN_GLASS)
			{
				if (GameObject* glass = GetGameObject(DATA_BROKEN_GLASS))
				{
					jaina->GetMotionMaster()->MovePoint(pointId, glass->GetPosition());
					return;
				}
			}
			else if (Creature* target = GetCreature(data.targetData))
			{
				jaina->GetMotionMaster()->MoveCloserAndStop(pointId, target, data.approach);
				return;
			}

			// Cible absente (spawn supprime) : on joue la scene sur place plutot
			// que de bloquer le scenario.
			OnJainaArrived(leg);
		}

		// Arrivee d'un trajet : lance la scene du lieu.
		void OnJainaArrived(RFTLegs leg)
		{
			// Chaque arret devient son point de retour : si elle sort de combat
			// (joueurs morts, pillards qui reset), elle reste la ou elle est.
			if (Creature* jaina = GetJaina())
				jaina->SetHomePosition(jaina->GetPosition());

			switch (leg)
			{
				case RFTLegs::TrailStart:
					// Premier mort qu'elle voit : un fidele, son blesse encore dans les bras.
					if (Creature* jaina = GetJaina())
					{
						jaina->SetFacingTo(jaina->GetAbsoluteAngle(HealerCorpsePoint));
						Talk(jaina, SAY_TRAIL_HEALER_EMOTE);
					}
					WaitForPlayers(RFTLegs::Armory, 1s);
					break;
				case RFTLegs::Armory:       events.ScheduleEvent(EVT_TRAIL_ARMORY_TALK, 500ms);   break;
				case RFTLegs::Rat:          events.ScheduleEvent(EVT_TRAIL_RAT_TALK, 500ms);      break;
				case RFTLegs::Pained:       events.ScheduleEvent(EVT_TRAIL_PAINED_KNEEL, 500ms);  break;
				case RFTLegs::Generals:     events.ScheduleEvent(EVT_TRAIL_GENERALS_TALK, 500ms); break;
				case RFTLegs::Kinndy:
					SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::Kinndy);
					events.ScheduleEvent(EVT_KINNDY_KNEEL, 500ms);
					break;
				case RFTLegs::BrokenGlass:  events.ScheduleEvent(EVT_GLASS_PICK, 500ms);      break;
				case RFTLegs::Looters:      events.ScheduleEvent(EVT_LOOTERS_NOTICE, 500ms);  break;
				case RFTLegs::Finisher:     events.ScheduleEvent(EVT_FINISHER_JAINA_01, 1s);  break;
				case RFTLegs::Docks:        events.ScheduleEvent(EVT_DOCKS_TALK_01, 1s);      break;
				case RFTLegs::CraterRim:    events.ScheduleEvent(EVT_CRATER_RIM_TALK, 1s);    break;
				case RFTLegs::CraterBottom: events.ScheduleEvent(EVT_IRIS_TALK_01, 1s);       break;
				default:
					break;
			}
		}

		// Jaina reprendra le trajet `leg` des qu'un joueur l'aura rejointe.
		void WaitForPlayers(RFTLegs leg, Milliseconds delay)
		{
			pendingLeg = leg;
			events.ScheduleEvent(EVT_TRAIL_WAIT_PLAYERS, dialogue.Consume(delay));
		}

		void HandleTrailWaitPlayers()
		{
			Creature* jaina = GetJaina();
			if (!jaina || pendingLeg == RFTLegs::Max)
				return;

			bool joined = false;
			instance->DoOnPlayers([jaina, &joined](Player* player)
			{
				if (!player->IsGameMaster() && player->IsWithinDist(jaina, TRAIL_WAIT_DISTANCE))
					joined = true;
			});

			if (!joined)
			{
				events.RescheduleEvent(EVT_TRAIL_WAIT_PLAYERS, 1s);
				return;
			}

			RFTLegs const leg = pendingLeg;
			pendingLeg = RFTLegs::Max;
			MoveJaina(leg);
		}

		// =================================================================
		// Ilot
		// =================================================================

		void HandleIsleJainaWalk()
		{
			if (Creature* jaina = GetJaina())
			{
				jaina->SetWalk(true);
				jaina->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
				jaina->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, JainaPoint01, true, JainaPoint01.GetOrientation());
			}
			Next(3s);
		}

		void HandleIsleKalecgosGreet()
		{
			Creature* jaina = GetJaina();
			Creature* kalecgos = GetKalecgos();
			if (jaina && kalecgos)
			{
				Talk(kalecgos, SAY_ISLE_KALECGOS_01);
				kalecgos->SetWalk(true);
				kalecgos->SetTarget(jaina->GetGUID());
				jaina->SetTarget(kalecgos->GetGUID());
			}
			Next(2s);
		}

		void HandleIsleKalecgosMove()
		{
			if (Creature* kalecgos = GetKalecgos())
			{
				Talk(kalecgos, SAY_ISLE_KALECGOS_02);
				kalecgos->SetSpeedRate(MOVE_WALK, KALECGOS_WALK_SPEED_RATE);
				kalecgos->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, KalecgosPoint02, true, KalecgosPoint02.GetOrientation());
			}
			Next(19s);
		}

		// Jaina lance sa teleportation (l'echo d'Aluneth sert de visuel).
		void HandleIsleEchoOfAluneth()
		{
			if (Creature* jaina = GetJaina())
			{
				if (TempSummon* trigger = instance->SummonCreature(WORLD_TRIGGER, jaina->GetPosition(), nullptr, 10s))
					trigger->CastSpell(trigger, SPELL_ECHO_OF_ALUNETH_SPAWN, true);

				// Kalec recule lentement devant la magie de Jaina, sans la quitter des yeux.
				if (Creature* kalecgos = GetKalecgos())
				{
					Position dest = kalecgos->GetPosition();
					kalecgos->MovePositionToFirstCollision(dest, KALECGOS_BACK_OFF_DISTANCE,
						jaina->GetAbsoluteAngle(kalecgos) - kalecgos->GetOrientation());
					kalecgos->GetMotionMaster()->MoveBackward(MOVEMENT_INFO_POINT_NONE, dest, jaina, KALECGOS_BACK_OFF_SPEED);
				}
			}
			Next(6s);
		}

		void HandleIsleAlunethFreed()
		{
			if (Creature* kalecgos = GetKalecgos())
				kalecgos->SetTarget(ObjectGuid::Empty);

			if (Creature* jaina = GetJaina())
			{
				jaina->SetTarget(ObjectGuid::Empty);
				jaina->CastSpell(jaina, SPELL_COSMETIC_ARCANE_DISSOLVE, true);

				if (TempSummon* trigger = instance->SummonCreature(WORLD_TRIGGER, jaina->GetPosition(), nullptr, 5s))
					trigger->CastSpell(trigger, SPELL_ALUNETH_FREED_EXPLOSION, true);
			}
			Next(800ms);
		}

		void HandleIsleJainaHide()
		{
			if (Creature* jaina = GetJaina())
			{
				jaina->StopMoving();
				jaina->GetMotionMaster()->Clear();
				jaina->GetMotionMaster()->MoveIdle();
				jaina->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
			}
			Next(2s);
		}

		// Kalec : "Je ne peux pas la suivre, mais vous oui." Jaina, dissoute, est
		// deja deplacee sur son point d'arrivee dans les ruines.
		void HandleIsleTeleportPrep()
		{
			if (Creature* jaina = GetJaina())
			{
				jaina->NearTeleportTo(JainaArrivalPoint);
				jaina->SetHomePosition(TrailStartPoint);
			}
			if (Creature* kalecgos = GetKalecgos())
			{
				Talk(kalecgos, SAY_ISLE_KALECGOS_07);
				kalecgos->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
				FaceFirstPlayer(kalecgos);
			}
			Next(5s);
		}

		// Jaina reapparait dans les ruines et court vers le debut du parcours.
		void HandleIsleJainaArrives()
		{
			if (Creature* jaina = GetJaina())
			{
				jaina->RemoveAurasDueToSpell(SPELL_COSMETIC_ARCANE_DISSOLVE);
				jaina->RemoveUnitFlag(UNIT_FLAG_UNINTERACTIBLE);   // Posee a sa disparition sur l'ilot
				jaina->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
				jaina->LoadEquipment(2);
				jaina->SetSheath(SHEATH_STATE_UNARMED);           // Baton range jusqu'au combat contre les pillards
				jaina->SetVignette(VIGNETTE_JAINA_PROUDMOORE);   // Les joueurs suivent sa trace
				jaina->CastSpell(jaina, SPELL_TELEPORT_VISUAL_ONLY, true);
			}
			MoveJaina(RFTLegs::TrailStart);
			Next(1500ms);
		}

		// Les joueurs arrivent la ou Jaina est apparue : ils la voient courir
		// devant eux. EVENT_HELP_KALECGOS fait passer en phase Trail (skybox).
		void HandleIsleTeleportPlayers()
		{
			ForceWeather(WEATHER_ARCANE_BUILD, true);
			TeleportPlayers(JainaArrivalPoint, TELEPORT_SPREAD_RADIUS);
			instance->DoOnPlayers([](Player* player)
			{
				player->CastSpell(player, SPELL_TELEPORT_VISUAL_ONLY, true);
			});
			TriggerGameEvent(EVENT_HELP_KALECGOS);
		}

		// =================================================================
		// Parcours
		// =================================================================

		// L'armurerie : un soldat flotte a hauteur des yeux.
		void HandleTrailArmoryTalk()
		{
			Talk(GetJaina(), SAY_TRAIL_BLOOD_EMOTE);
			Next(3s);
		}

		void HandleTrailArmoryFloating()
		{
			if (Creature* jaina = GetJaina())
			{
				if (Creature* knight = GetCreature(DATA_FLOATING_KNIGHT))
					jaina->SetFacingToObject(knight);
				Talk(jaina, SAY_TRAIL_FLOATING_OFFICER_JAINA);
			}
			TriggerGameEvent(EVENT_TRAIL_FLOATING_OFFICER);
			WaitForPlayers(RFTLegs::Rat, 3s);
		}

		// Jaina marche sur quelque chose de mou : un rat, violet, son fromage en gueule.
		void HandleTrailRatTalk()
		{
			if (Creature* jaina = GetJaina())
			{
				if (Creature* rat = GetCreature(DATA_BLACK_RAT))
					jaina->SetFacingToObject(rat);
				Talk(jaina, SAY_TRAIL_RAT_JAINA);
			}
			TriggerGameEvent(EVENT_TRAIL_RAT);
			Next(4500ms);
		}

		void HandleTrailRatSurvivors()
		{
			Talk(GetJaina(), SAY_TRAIL_SURVIVORS_JAINA);
			WaitForPlayers(RFTLegs::Pained, 7s);
		}

		void HandleTrailPainedKneel()
		{
			if (Creature* jaina = GetJaina())
			{
				if (Creature* pained = GetCreature(DATA_PAINED))
					jaina->SetFacingToObject(pained);
				jaina->SetStandState(UNIT_STAND_STATE_KNEEL);
			}
			Next(2s);
		}

		void HandleTrailPainedTalk()
		{
			Talk(GetJaina(), SAY_TRAIL_PAINED_JAINA);
			TriggerGameEvent(EVENT_TRAIL_PAINED);
			Next(6s);
		}

		void HandleTrailPainedStand()
		{
			if (Creature* jaina = GetJaina())
				jaina->SetStandState(UNIT_STAND_STATE_STAND);
			WaitForPlayers(RFTLegs::Generals, 2s);
		}

		void HandleTrailGeneralsTalk()
		{
			if (Creature* jaina = GetJaina())
			{
				if (Creature* marcus = GetCreature(DATA_MARCUS_JONATHAN))
					jaina->SetFacingToObject(marcus);
				Talk(jaina, SAY_TRAIL_GENERALS_JAINA);
			}
			TriggerGameEvent(EVENT_TRAIL_GENERALS);
			WaitForPlayers(RFTLegs::Kinndy, 8s);
		}

		// =================================================================
		// Kinndy et l'eclat de miroir
		// =================================================================

		void HandleKinndyKneel()
		{
			if (Creature* jaina = GetJaina())
			{
				if (Creature* kinndy = GetKinndy())
					jaina->SetFacingToObject(kinndy);
				jaina->SetStandState(UNIT_STAND_STATE_KNEEL);
				Talk(jaina, SAY_KINNDY_JAINA);
			}
			Next(4s);
		}

		// Jaina pose la main sur son epaule : Kinndy tombe en poussiere violette.
		void HandleKinndyTouch()
		{
			if (Creature* kinndy = GetKinndy())
			{
				kinndy->RemoveAurasDueToSpell(SPELL_COSMETIC_PURPLE_VERTEX_STATE);
				kinndy->RemoveAurasDueToSpell(SPELL_SHIMMERDUST);
				kinndy->CastSpell(kinndy, SPELL_COSMETIC_ARCANE_DISSOLVE);
                if (TempSummon* trigger = kinndy->SummonCreature(WORLD_TRIGGER, kinndy->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, 2s))
                    trigger->CastSpell(trigger, SPELL_KINNDY_DUST);
			}
			Next(2s);
		}

		void HandleKinndyStand()
		{
			if (Creature* jaina = GetJaina())
				jaina->SetStandState(UNIT_STAND_STATE_STAND);
			MoveJaina(RFTLegs::BrokenGlass);
		}

		// Un reflet sur la terre morte : l'eclat du miroir.
		void HandleGlassPick()
		{
			if (Creature* jaina = GetJaina())
				jaina->SetStandState(UNIT_STAND_STATE_KNEEL);
			Next(1500ms);
		}

		void HandleGlassTaken()
		{
			if (GameObject* glass = GetGameObject(DATA_BROKEN_GLASS))
				glass->DespawnOrUnsummon();
			if (Creature* jaina = GetJaina())
				jaina->SetStandState(UNIT_STAND_STATE_STAND);

			// Termine l'etape 5014 : OnCriteriaFindJainaKinndy fait apparaitre les pillards.
			TriggerGameEvent(EVENT_FIND_JAINA_02);
		}

		// =================================================================
		// Pillards
		// =================================================================

		// Le chef remarque Jaina au milieu des ruines.
		void HandleLootersNotice()
		{
			Creature* jaina = GetJaina();
			if (Creature* warlord = GetWarlord())
			{
				warlord->SetEmoteState(EMOTE_ONESHOT_NONE);
				if (jaina)
					warlord->SetFacingToObject(jaina);
				Talk(warlord, SAY_LOOTERS_WARLORD_TAUNT);
			}
			if (jaina)
			{
				if (Creature* warlord = GetWarlord())
					jaina->SetFacingToObject(warlord);
			}

			DoOnCreatures(lootersWave, [jaina](Creature* looter)
			{
				looter->SetEmoteState(EMOTE_ONESHOT_NONE);
				if (jaina)
					looter->SetFacingToObject(jaina);
			});
			Next(4s);
		}

		// Jaina frappe : l'onde arcanique laisse les pillards affaiblis, puis la
		// rotation de npc_jaina_ruins prend le relais (ouverture au Choc de flammes).
		void HandleLootersAttack()
		{
			Creature* jaina = GetJaina();
			Creature* warlord = GetWarlord();

			if (jaina)
			{
				SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, jaina);

				// L'onde arcanique, gonflee par les restes de la bombe : un dummy qui
				// porte le visuel de Time Stop (hotfix XSV 600000). L'affaiblissement
				// des pillards est le visuel pose juste en dessous.
				jaina->CastSpell(jaina, SPELL_ARCANE_WAVE, true);
			}

			auto release = [jaina](Creature* looter)
			{
				// Sonnes par l'onde arcanique, ils s'en remettent vite : le visuel
				// est le meme que celui des cadavres, il ne doit pas rester.
				if (Aura* weakened = looter->AddAura(SPELL_ARCANE_DEBUFF_VISUAL, looter))
				{
					weakened->SetMaxDuration(LOOTERS_WEAKENED_DURATION);
					weakened->SetDuration(LOOTERS_WEAKENED_DURATION);
				}
				looter->SetImmuneToAll(false);
				looter->SetReactState(REACT_AGGRESSIVE);
				if (jaina)
					looter->AI()->AttackStart(jaina);
			};

			DoOnCreatures(lootersWave, [release](Creature* looter)
			{
				release(looter);
				looter->SetVignette(VIGNETTE_HORDE_TROOPS);
			});

			if (warlord)
			{
				release(warlord);
				warlord->SetVignette(VIGNETTE_HORDE_WARLORD);
			}

			if (jaina)
			{
				jaina->SetReactState(REACT_AGGRESSIVE);
				if (warlord)
					jaina->AI()->AttackStart(warlord);
			}

			if (!lootersWave.empty())
				events.ScheduleEvent(EVT_LOOTERS_AGGRO_NUDGE, 2s);
		}

		// Watchdog d'aggro : chaque pillard encore passif est force a engager Jaina.
		void HandleLootersAggroNudge()
		{
			Creature* jaina = GetJaina();
			if (!jaina || lootersWave.empty())
				return;

			DoOnCreatures(lootersWave, [jaina](Creature* looter)
			{
				if (looter->IsAlive() && !looter->IsEngaged())
					looter->AI()->AttackStart(jaina);
			});

			events.RescheduleEvent(EVT_LOOTERS_AGGRO_NUDGE, 1s);
		}

		// =================================================================
		// Le dernier survivant
		// =================================================================

		// Le finisher ne peut partir que quand l'etape 5015 est validee : les
		// criteres de 5016 ne comptent pas avant. Et il faut le chef a genoux.
		void TryStartFinisher()
		{
			if (finisherStarted || !cleaningDone || !warlordKneeling)
				return;

			finisherStarted = true;
			events.CancelEvent(EVT_LOOTERS_AGGRO_NUDGE);
			SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::LastSurvivor);

			if (Creature* jaina = GetJaina())
			{
				jaina->AI()->SetData(DATA_CANCEL_GROUP, DATA_PHASE_COMBAT);
				jaina->CastStop();
				jaina->CombatStop(true);
				jaina->SetReactState(REACT_PASSIVE);
				jaina->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
				jaina->SetSheath(SHEATH_STATE_UNARMED);
			}
			events.ScheduleEvent(EVT_FINISHER_WALK, 2s);
		}

		void HandleFinisherStab()
		{
			if (Creature* jaina = GetJaina())
				jaina->HandleEmoteCommand(EMOTE_ONESHOT_ATTACK1H);
			Next(800ms);
		}

		// L'eclat reste plante dans la gorge du chef.
		void HandleFinisherKill()
		{
			Creature* jaina = GetJaina();
			if (Creature* warlord = GetWarlord())
			{
				warlord->SetVignette(VIGNETTE_NONE);
				warlord->KillSelf();

				if (jaina)
					jaina->SummonGameObject(GOB_BROKEN_GLASS, warlord->GetPosition(),
						QuaternionData::fromEulerAnglesZYX(warlord->GetOrientation(), 0.f, 0.f), 0s);
			}
			if (jaina)
				SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, jaina);

			// Termine l'etape 5016 : la suite part de CRITERIA_TREE_LAST_SURVIVOR.
			TriggerGameEvent(EVENT_WARLORD_ROKNAH_SLAIN);
		}

		// =================================================================
		// L'iris
		// =================================================================

		// Jaina s'accorde a l'iris : le dummy porte les visuels a sa place (l'iris
		// est dans l'ADT).
		void HandleIrisChannel()
		{
			Creature* jaina = GetJaina();
			if (Creature* dummy = instance->SummonCreature(WORLD_TRIGGER, IrisPoint))
			{
				dummy->SetObjectScale(DUMMY_SCALE_SMALL);
				if (jaina)
					dummy->CastSpell(jaina, SPELL_ALUNETH_DRINKS);
				dummy->CastSpell(dummy, SPELL_EMPOWERED_SUMMON, true);
				irisDummy = dummy->GetGUID();
			}
			if (jaina)
				jaina->CastSpell(jaina, SPELL_ARCANE_CHANNELING);
			Next(4s);
		}

		// Le sceau : l'iris rayonne de l'essence de Jaina, Kalec ne sentira qu'elle.
		void HandleIrisSeal()
		{
			if (Creature* dummy = instance->GetCreature(irisDummy))
			{
				dummy->RemoveAllAuras();
				dummy->SetObjectScale(DUMMY_SCALE_LARGE);
				dummy->AddAura(SPELL_COSMETIC_ARCANE_ENERGY, dummy);
			}
			Next(3s);
		}

		void HandleIrisSealed()
		{
			if (Creature* jaina = GetJaina())
			{
				jaina->CastStop();
				jaina->RemoveAurasDueToSpell(SPELL_ALUNETH_DRINKS);
			}

			// Termine l'etape 5017 : la suite part de CRITERIA_TREE_BACK_TO_SENDER.
			TriggerGameEvent(EVENT_IRIS_SEALED);
		}

		// =================================================================
		// Quitter les ruines
		// =================================================================

		void HandleLeaveTalk()
		{
			if (Creature* jaina = GetJaina())
			{
				FaceFirstPlayer(jaina);
				Talk(jaina, SAY_LEAVE_THE_RUINS_JAINA);
			}
			Next(8s);
		}

		void HandleLeaveOpenPortal()
		{
			if (Creature* jaina = GetJaina())
			{
				GameObject* portal = jaina->SummonGameObject(GOB_PORTAL_TO_STORMWIND, jaina->GetPosition(),
					QuaternionData::fromEulerAnglesZYX(jaina->GetOrientation(), 0.f, 0.f), 0s);
				if (portal)
				{
					portal->SetGoState(GO_STATE_ACTIVE);
					portal->UseDoorOrButton();
					portal->SetVignette(VIGNETTE_PORTAL_TO_STORMWIND);
				}
			}
			Next(1s);
		}

		void HandleLeaveDespawnFinal()
		{
			if (Creature* jaina = GetJaina())
			{
				jaina->SetVignette(VIGNETTE_NONE);
				jaina->CastSpell(jaina, SPELL_COSMETIC_ARCANE_DISSOLVE);
				jaina->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
			}
		}

		// =================================================================
		// Accesseurs raccourcis
		// =================================================================
		Creature* GetJaina()    { return GetCreature(DATA_JAINA_PROUDMOORE); }
		Creature* GetKalecgos() { return GetCreature(DATA_KALECGOS); }
		Creature* GetKinndy()   { return GetCreature(DATA_KINNDY_SPARKSHINE); }
		Creature* GetWarlord()  { return GetCreature(DATA_ROKNAH_WARLORD); }

		// =================================================================
		// Helpers internes
		// =================================================================

		// Talk null-safe : un acteur despawne en cours de scene ne doit pas
		// faire tomber le serveur, la replique est simplement perdue.
		void Talk(Creature* creature, uint8 textId)
		{
			dialogue.Say(creature, textId);
		}

		void TalkAndNext(Creature* creature, uint8 textId, const Milliseconds& time)
		{
			Talk(creature, textId);
			Next(time);
		}

		// Incremente eventId membre et planifie l'event suivant du bloc.
		void Next(const Milliseconds& time)
		{
			eventId++;
			events.ScheduleEvent(eventId, dialogue.Consume(time));
		}

		Player* GetFirstPlayer() const
		{
			auto const& playerList = instance->GetPlayers();
			if (playerList.empty())
				return nullptr;
			return playerList.begin()->GetSource();
		}

		void FaceFirstPlayer(Creature* creature)
		{
			if (!creature)
				return;
			if (Player* player = GetFirstPlayer())
				creature->SetFacingToObject(player);
		}

		// Applique une action a chaque creature encore presente d'une liste de GUIDs.
		template <typename T>
		void DoOnCreatures(GuidVector const& guids, T&& fn)
		{
			for (ObjectGuid const& guid : guids)
			{
				if (Creature* creature = instance->GetCreature(guid))
					fn(creature);
			}
		}

		// Teleporte tous les joueurs autour d'un centre, chacun sur son propre
		// point tire dans le disque.
		void TeleportPlayers(Position const& center, float spreadRadius)
		{
			instance->DoOnPlayers([&center, spreadRadius](Player* player)
			{
				Position dest = GetRandomPosition(player, center, spreadRadius);
				dest.SetOrientation(center.GetOrientation());
				player->NearTeleportTo(dest);
			});
		}

		// Force un climat specifique cote client pour chaque joueur de l'instance.
		void ForceWeather(uint32 weatherEntry, bool apply)
		{
			instance->DoOnPlayers([weatherEntry, apply](Player* player)
			{
				if (apply)
					player->SendDirectMessage(WorldPackets::Misc::Weather(WeatherState(weatherEntry), 1.0f).Write());
				else
					player->GetMap()->SendZoneWeather(player->GetZoneId(), player);
			});
		}
	};

	InstanceScript* GetInstanceScript(InstanceMap* map) const override
	{
		return new scenario_ruins_of_theramore_InstanceScript(map);
	}
};

void AddSC_scenario_ruins_of_theramore()
{
	new scenario_ruins_of_theramore();
}
