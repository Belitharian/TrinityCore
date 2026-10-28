/*
 * Battle for Theramore - InstanceScript principal
 *
 * Pilote le scenario "La Chute de Theramore" de bout en bout.
 *
 * DEUX MOTEURS COMPLEMENTAIRES
 *
 *   1. OnCompletedCriteriaTree : les criteria trees valident les etapes
 *      cotees joueur (trouver Jaina, evacuer les civils, survivre...). A
 *      chaque validation on fait avancer DATA_SCENARIO_PHASE et on amorce la
 *      cinematique suivante.
 *
 *   2. Scenes de dialogue : chaque cinematique est une scene client (Lua,
 *      sql/custom/scenes/battle_for_theramore) qui fait apparaitre ses propres
 *      acteurs a la place des vrais PNJ : repliques en bulle, regards,
 *      emotes, deplacements, sorts et auras y sont joues. Le serveur masque
 *      les vrais PNJ au debut ("Ready"), agit sur le monde aux signaux de la
 *      scene (portail, barriere, escorte de Thalen...), puis remet les vrais
 *      PNJ dans l'etat final et reprend le scenario quand tous les joueurs
 *      ont fini (voir CustomScenario::DialogueSceneDirector et la region
 *      DIALOGUE_SCENES).
 *
 * SCENES (dans l'ordre du scenario)
 *
 *    SCENE_BFT_COUNCIL            Conseil de guerre dans la tour
 *    SCENE_BFT_UNKNOWN_TAUREN     Perith annonce l'attaque de la Horde, Jaina
 *                                 redige son sauf-conduit
 *    SCENE_BFT_EVACUATION_*       Scenes du livre pendant l'evacuation (quai,
 *                                 Janene, docteur), levees par le gossip de
 *                                 chaque temoin
 *    SCENE_BFT_A_LITTLE_HELP      Arrivee des archimages et des generaux,
 *                                 repartition des generaux, teleport de masse
 *    SCENE_BFT_THE_BATTLE         Trahison et capture de Thalen, debut de la
 *                                 bataille
 *    SCENE_BFT_HELP_THE_WOUNDED   Apres-bataille, partie I (Hedric)
 *    SCENE_BFT_WOUNDED_KINNDY     Apres-bataille, partie II (Kinndy),
 *                                 annulee si l'etape se termine avant
 *    SCENE_BFT_WAIT_FOR_AMARA     Avertissement de Kalecgos, retour d'Amara
 *    SCENE_BFT_RETRIEVE_RHONIN    Sommet de la tour, avant l'explosion
 *
 * En CUSTOM_DEBUG, aucune scene n'est jouee : ses signaux sont rejoues d'un
 * coup et le scenario passe directement a l'etat de fin de la scene.
 *
 * Commentaires en francais sans accents (encodage TC).
 */

#include "CriteriaHandler.h"
#include "CustomAI.h"          // GetRandomPosition / GetRandomPositionAroundCircle
#include "DB2Structure.h"
#include "EventMap.h"
#include "GameObject.h"
#include "InstanceScenario.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "MiscPackets.h"
#include "ObjectMgr.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "Scenario.h"
#include "SceneMgr.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"
#include "Weather.h"
#include "battle_for_theramore.h"
#include "../CustomScenario.h"
#include <algorithm>
#include <array>
#include <bitset>
#include <utility>

// =========================================================================
// Tables de correspondance NPC / GO <-> Data ID
// =========================================================================

// Nombre d'acteurs de dialogue en tete de creatureData : ClearTarget et la
// mise en objet actif (OnCreatureCreate) ne parcourent que ces 20 premieres
// entrees, les suivantes (Kalecgos dragon,
// Drok, Gruhta, figurants de l'evacuation) ne participent pas aux
// cinematiques.
uint8 const eventCreatureDataCount = 20;

const ObjectData creatureData[] =
{
	{ NPC_JAINA_PROUDMOORE,     DATA_JAINA_PROUDMOORE       },
	{ NPC_KINNDY_SPARKSHINE,    DATA_KINNDY_SPARKSHINE      },
	{ NPC_KALECGOS,             DATA_KALECGOS               },
	{ NPC_ARCHMAGE_TERVOSH,     DATA_ARCHMAGE_TERVOSH       },
	{ NPC_PAINED,               DATA_PAINED                 },
	{ NPC_PERITH_STORMHOOVE,    DATA_PERITH_STORMHOOVE      },
	{ NPC_KNIGHT_OF_THERAMORE,  DATA_KNIGHT_OF_THERAMORE    },
	{ NPC_HEDRIC_EVENCANE,      DATA_HEDRIC_EVENCANE        },
	{ NPC_RHONIN,               DATA_RHONIN                 },
	{ NPC_VEREESA_WINDRUNNER,   DATA_VEREESA_WINDRUNNER     },
	{ NPC_THALEN_SONGWEAVER,    DATA_THALEN_SONGWEAVER      },
	{ NPC_TARI_COGG,            DATA_TARI_COGG              },
	{ NPC_AMARA_LEESON,         DATA_AMARA_LEESON           },
	{ NPC_THADER_WINDERMERE,    DATA_THADER_WINDERMERE      },
	{ NPC_MARCUS_JONATHAN,      DATA_MARCUS_JONATHAN        },
	{ NPC_THADDUS_STOUTBLOW,    DATA_THADDUS_STOUTBLOW      },
	{ NPC_HORRAN_REDMANE,       DATA_HORRAN_REDMANE         },
	{ NPC_ADMIRAL_AUBREY,       DATA_ADMIRAL_AUBREY         },
	{ NPC_SHANDRIS_FEATHERMOON, DATA_SHANDRIS_FEATHERMOON   },
	{ NPC_GENERAL_TIRAS_ALAN,   DATA_GENERAL_TIRAS_ALAN     },
	{ NPC_KALECGOS_DRAGON,      DATA_KALECGOS_DRAGON        },
	{ NPC_CAPTAIN_DROK,         DATA_CAPTAIN_DROK           },
	{ NPC_WAVE_CALLER_GRUHTA,   DATA_WAVE_CALLER_GRUHTA     },
	{ NPC_THERAMORE_CRIER,      DATA_THERAMORE_CRIER        },
	{ NPC_INNKEEPER_JANENE,     DATA_INNKEEPER_JANENE       },
	{ NPC_DOCTOR_VANHOWZEN,     DATA_DOCTOR_VANHOWZEN       },
	{ NPC_MORNING_STAR_FIRST_MATE, DATA_MORNING_STAR_FIRST_MATE },
	{ NPC_DRAENEI_SHAMAN,       DATA_DRAENEI_SHAMAN         },
	{ 0,                        0                           }   // END
};

const ObjectData gameobjectData[] =
{
	{ GOB_PORTAL_TO_STORMWIND,  DATA_PORTAL_TO_STORMWIND    },
	{ GOB_PORTAL_TO_DALARAN,    DATA_PORTAL_TO_DALARAN      },
	{ GOB_PORTAL_TO_ORGRIMMAR,  DATA_PORTAL_TO_ORGRIMMAR    },
	{ GOB_MYSTIC_BARRIER_01,    DATA_MYSTIC_BARRIER_01      },
	{ GOB_MYSTIC_BARRIER_02,    DATA_MYSTIC_BARRIER_02      },
	{ GOB_POWDER_BARREL,        DATA_POWDER_BARREL          },
	{ 0,                        0                           }   // END
};

// =========================================================================
// Identifiants des evenements internes de l'EventMap
// =========================================================================
// La chaine ne fait plus que laisser passer le delai prevu entre la
// validation d'une etape et sa cinematique : chaque event lance une scene de
// dialogue (region DIALOGUE_SCENES), dont la fin reprend le scenario.
enum BFTEvents : uint32
{
	EVT_WAITING_TRIGGER = 1,           // EVENT_WAITING, temps mort avant l'arrivee de Perith
	EVT_TAUREN_SCENE,                  // SCENE_BFT_UNKNOWN_TAUREN
	EVT_HELP_SCENE,                    // SCENE_BFT_A_LITTLE_HELP
	EVT_BATTLE_SCENE,                  // SCENE_BFT_THE_BATTLE
	EVT_EVAC_DOCKS_SCENE,              // SCENE_BFT_EVACUATION_DOCKS
	EVT_EVAC_JANENE_SCENE,             // SCENE_BFT_EVACUATION_JANENE
	EVT_EVAC_DOCTOR_SCENE,             // SCENE_BFT_EVACUATION_DOCTOR
	EVT_WOUNDED_SCENE,                 // SCENE_BFT_HELP_THE_WOUNDED
	EVT_WOUNDED_KINNDY_SCENE,          // SCENE_BFT_WOUNDED_KINNDY
	EVT_AMARA_SCENE,                   // SCENE_BFT_WAIT_FOR_AMARA
	EVT_RHONIN_SCENE                   // SCENE_BFT_RETRIEVE_RHONIN
};

// =========================================================================
// Scenes de dialogue
// =========================================================================
// Signaux propres a chaque scene, dans l'ordre ou la scene les envoie (c'est
// aussi l'ordre dans lequel CUSTOM_DEBUG les rejoue), et duree au-dela de
// laquelle le serveur coupe la scene d'un client qui ne repond plus.
namespace BFTScenes
{
	constexpr char const* HelpCues[]   = { "Portal", "ClosePortal" };
	constexpr char const* BattleCues[] = { "Bind", "Kneel", "Escort", "BarrierBreaks", "ThaderWounded" };
	constexpr char const* AmaraCues[]  = { "RhoninGone", "AmaraBack" };

	constexpr CustomScenario::DialogueScene Council       = { SCENE_BFT_COUNCIL,           {},         6min };
	constexpr CustomScenario::DialogueScene UnknownTauren = { SCENE_BFT_UNKNOWN_TAUREN,    {},         8min };
	constexpr CustomScenario::DialogueScene ALittleHelp   = { SCENE_BFT_A_LITTLE_HELP,     HelpCues,   8min };
	constexpr CustomScenario::DialogueScene TheBattle     = { SCENE_BFT_THE_BATTLE,        BattleCues, 4min };
	constexpr CustomScenario::DialogueScene EvacDocks     = { SCENE_BFT_EVACUATION_DOCKS,  {},         2min };
	constexpr CustomScenario::DialogueScene EvacJanene    = { SCENE_BFT_EVACUATION_JANENE, {},         2min };
	constexpr CustomScenario::DialogueScene EvacDoctor    = { SCENE_BFT_EVACUATION_DOCTOR, {},         2min };
	constexpr CustomScenario::DialogueScene Wounded       = { SCENE_BFT_HELP_THE_WOUNDED,  {},         3min };
	constexpr CustomScenario::DialogueScene WoundedKinndy = { SCENE_BFT_WOUNDED_KINNDY,    {},         4min };
	constexpr CustomScenario::DialogueScene WaitForAmara  = { SCENE_BFT_WAIT_FOR_AMARA,    AmaraCues,  5min };
	constexpr CustomScenario::DialogueScene RetrieveRhonin = { SCENE_BFT_RETRIEVE_RHONIN,  {},         4min };

	// Temps mort avant une scene (apres le conseil, avant la scene de la
	// bataille), presque nul en CUSTOM_DEBUG.
	#ifdef CUSTOM_DEBUG
		constexpr Milliseconds StagingPause = 1s;
	#else
		constexpr Milliseconds StagingPause = 10s;
	#endif
}

// =========================================================================
// Evenements d'ambiance de la Horde (bombardement de fond)
// =========================================================================
// Les deux classes suivent le meme schema : Execute lance un tir puis se
// replanifie elle-meme toutes les 8-10s. Elles renvoient toujours false pour
// que l'EventProcessor ne les detruise pas, et elles vivent aussi longtemps
// que la creature porteuse.

// Bombardier aerien : il tourne au-dessus de la ville et lache ses bombes
// sur lui-meme (le sort gere la zone d'impact).
class HordeBombardierThrowBomb : public BasicEvent
{
	public:
		HordeBombardierThrowBomb(Unit* caster) : _caster(caster) { }

		bool Execute(uint64 /*execTime*/, uint32 /*diff*/) override
		{
			_caster->CastSpell(_caster, SPELL_THROW_BOMB, TRIGGERED_FULL_MASK);
			_caster->m_Events.AddEvent(this, _caster->m_Events.CalculateTime(Seconds(urand(8, 10))));
			return false;
		}

	private:
		Unit* _caster;
};

// Demolisseur : il pilonne une bande de terrain fixe. Chaque tir vise un
// point tire au hasard dans cette bande, ce qui donne un bombardement
// disperse mais toujours dans la meme zone. Il se tourne vers le point,
// puis tire une fois tourne.
// Bandes pilonnees (extremites) : le mur ouest, par les demolisseurs de
// DATA_DECORATION_WEST ; l'ile, par celui de l'entree pose sur
// EntranceDemolisherPoint (DATA_DECORATION_ENTRANCE, l'autre ne tire pas).
Position const WestWallStripStart     = { -3771.834717f, -4261.928711f,  7.074570f };
Position const WestWallStripEnd       = { -3793.766357f, -4260.671387f,  6.944610f };
Position const IslandStripStart       = { -3635.380859f, -4372.370117f,  8.702263f };
Position const IslandStripEnd         = { -3695.503662f, -4298.466797f, 11.432985f };
Position const EntranceDemolisherPoint = { -3608.11f, -4283.15f, 13.5707f };

class HordeDemolisherThrowBoulder : public BasicEvent
{
	public:
	HordeDemolisherThrowBoulder(Unit* caster, Position const& start, Position const& end) : _caster(caster), p1(start), p2(end), aiming(false)
	{
	}

	// Largeur de la bande pilonnee, de part et d'autre de l'axe p1-p2.
	static constexpr float STRIP_WIDTH = 4.0f;
	// Le temps de se tourner vers le point vise.
	static constexpr Milliseconds AIM_TIME = 1200ms;

	bool Execute(uint64 /*execTime*/, uint32 /*diff*/) override
	{
		// D'abord se tourner vers le point vise...
		if (!aiming)
		{
			target = GetRandomPointOnStrip(_caster, p1, p2, STRIP_WIDTH, _caster->GetMap());
			_caster->SetFacingTo(_caster->GetAbsoluteAngle(target));
			aiming = true;
			_caster->m_Events.AddEvent(this, _caster->m_Events.CalculateTime(AIM_TIME));
			return false;
		}

		// ... puis tirer.
		aiming = false;
		// DEBUG temporaire : sans DONT_REPORT_CAST_ERROR, CheckCast renvoie la vraie raison de l'echec.
		SpellCastResult result = _caster->CastSpell(target, SPELL_THROW_BOULDER, TriggerCastFlags(TRIGGERED_FULL_MASK & ~TRIGGERED_DONT_REPORT_CAST_ERROR));
		if (result != SPELL_CAST_OK)
			TC_LOG_ERROR("scripts", "BFT: Boulder Throw a echoue ({}) depuis {} vers {}", uint32(result), _caster->GetPosition().ToString(), target.ToString());
		_caster->m_Events.AddEvent(this, _caster->m_Events.CalculateTime(Seconds(urand(8, 10))));
		return false;
	}

	// Tire un point au hasard dans le rectangle centre sur le segment p1-p2 :
	// on avance d'une fraction t le long du segment, puis on decale
	// lateralement d'au plus widthMeters / 2.
	Position GetRandomPointOnStrip(Unit* unit, Position const& p1, Position const& p2, float widthMeters, Map* map)
	{
		float dx = p2.GetPositionX() - p1.GetPositionX();
		float dy = p2.GetPositionY() - p1.GetPositionY();
		float len = std::sqrt(dx * dx + dy * dy);

		float ux = dx / len, uy = dy / len;   // direction normalisee
		float px = -uy, py = ux;              // perpendiculaire 2D

		float t = frand(0.0f, 1.0f);
		float offset = frand(-widthMeters / 2.0f, widthMeters / 2.0f);

		float x = p1.GetPositionX() + dx * t + px * offset;
		float y = p1.GetPositionY() + dy * t + py * offset;
		float z = p1.GetPositionZ() + (p2.GetPositionZ() - p1.GetPositionZ()) * t;

		// Recalage sur le vrai sol plutot que sur l'interpolation lineaire :
		// sans ca les rochers tombent dans le decor sur terrain accidente.
		float groundZ = map->GetHeight(unit->GetPhaseShift(), x, y, z + 2.0f, true);
		if (groundZ > INVALID_HEIGHT)
			z = groundZ;

		return Position(x, y, z, 0.f);
	}

	private:
	Unit* _caster;
	Position p1, p2;                        // Extremites de la bande pilonnee
	bool aiming;                            // Tourne vers target, tire au prochain passage
	Position target;                        // Point vise par le prochain tir
};

// =========================================================================
// InstanceScript
// =========================================================================
class scenario_battle_for_theramore : public InstanceMapScript
{
	public:
	scenario_battle_for_theramore() : InstanceMapScript(BFTScriptName, 5000)
	{
	}

	struct scenario_battle_for_theramore_InstanceScript : public InstanceScript, public CustomScenario::DialogueSceneHost, public CustomScenario::AmbientVoiceHost
	{
		// Ordre d'initialisation aligne sur l'ordre de declaration des membres
		// (voir "Etat interne" en bas de la classe) pour eviter tout -Wreorder.
		scenario_battle_for_theramore_InstanceScript(InstanceMap* map) : InstanceScript(map),
			woundedTroops(0), waves(0), phase(BFTPhases::FindJaina)
		{
			SetHeaders(DataHeader);
			LoadObjectData(creatureData, gameobjectData);
		}

		enum Spells
		{
			SPELL_WATER_BUCKET          = 42336,
			SPELL_MASS_TELEPORT         = 60516,
			SPELL_MAGIC_QUILL           = 424726,
			SPELL_TIED_UP               = 248932,
			SPELL_CLOSE_PORTAL          = 203542,
			SPELL_PRISMATIC_BARRIER     = 235450,
			SPELL_ARCANE_CANALISATION   = 288451,
			SPELL_FROST_BREATH          = 300548,
			SPELL_VANISH                = 199483,
			SPELL_BIG_EXPLOSION         = 348750,
			SPELL_SCORCHED_EARTH        = 373139,
			SPELL_ARCANIC_CELL          = 398947,
			SPELL_READING_BOOK_STANDING = 397765,
			SPELL_AREA_TRIGGER_VISUAL   = 473554,
			SPELL_CHARGE                = 1253149,
			SPELL_ARCANIC_EXPLOSION     = 1285450,
			SPELL_STUNNED               = 1304390,
		};

		// Auras portees par les joueurs pendant une tranche de phases.
		// Posees et retirees par CustomScenario::SyncPhaseAuras, depuis
		// SetData(DATA_SCENARIO_PHASE) et OnPlayerEnter : plus rien a poser
		// ni a retirer a la main dans les criteria trees.
		static constexpr CustomScenario::PhaseAura PhaseAuras[] =
		{
			// Bouclier runique distribue par Rhonin, valable toute la bataille.
			{ SPELL_RUNIC_SHIELD, (uint32)BFTPhases::Preparation_Rhonin, (uint32)BFTPhases::HelpTheWounded            },
			// Seau d'eau pour eteindre les incendies d'apres-bataille.
			{ SPELL_WATER_BUCKET, (uint32)BFTPhases::HelpTheWounded,     (uint32)BFTPhases::HelpTheWounded_Extinguish }
		};

		// Ordre d'arrivee des vagues de la Horde : l'index `waves` avance d'un
		// cran a chaque appel de NextWave, declenche par OnUnitDeath. Chaque
		// valeur designe un groupe de spawn (voir HordeMembersInvoker).
		uint32 Waves[HORDE_WAVES_COUNT] =
		{
			DATA_WAVE_WEST,
			DATA_WAVE_CITADEL,
			DATA_WAVE_DOCKS,
			DATA_WAVE_DOORS,
			DATA_WAVE_WEST,
			DATA_WAVE_CITADEL,
			DATA_WAVE_DOCKS,
			DATA_WAVE_WEST,
			DATA_WAVE_CITADEL,
			DATA_WAVE_DOORS
		};

		uint32 GetData(uint32 dataId) const override
		{
			if (dataId == DATA_SCENARIO_PHASE)
				return (uint32)phase;
			else if (dataId == DATA_WOUNDED_TROOPS)
				return woundedTroops;
			else if (dataId == DATA_WAVE_GROUP_ID)
				return Waves[waves < HORDE_WAVES_COUNT ? waves : HORDE_WAVES_COUNT - 1];
			return 0;
		}

		// L'Etoile du matin est au port des le debut. Son groupe de spawn est
		// manuel : un groupe automatique sans condition est respawne par
		// Map::UpdateSpawnGroupConditions des que le scenario change d'etape,
		// et le navire revenait juste apres son depart.
		void Create() override
		{
			InstanceScript::Create();
			instance->SpawnGroupSpawn(SPAWN_GROUP_MORNING_STAR, true);
		}

		void OnPlayerEnter(Player* player) override
		{
			// Orage permanent : ambiance de la ville assiegee.
			ForceWeather(WEATHER_STATE_THUNDERS, true);

			CustomScenario::SyncPhaseAuras(player, (uint32)phase, PhaseAuras);

			// Scene d'introduction : une fois par joueur, tant que Jaina n'a pas
			// encore ete trouvee. Differee le temps de l'ecran de chargement.
			// Sautee en CUSTOM_DEBUG, comme les scenes de dialogue.
			#ifndef CUSTOM_DEBUG
			if (phase == BFTPhases::FindJaina && introSeen.insert(player->GetGUID()).second)
			{
				player->m_Events.AddEventAtOffset([player]()
				{
					if (player->IsInWorld() && player->GetMapId() == 5000)
						player->GetSceneMgr().PlayScene(SCENE_THERAMORE_INTRO);
				}, 3s);
			}
			#endif

			// Reconnexion pendant une scene de dialogue : elle reprend pour lui
			// a la replique ou il l'avait quittee, apres l'ecran de chargement.
			player->m_Events.AddEventAtOffset([player]()
			{
				if (!player->IsInWorld() || player->GetMapId() != 5000)
					return;
				if (auto* host = dynamic_cast<CustomScenario::DialogueSceneHost*>(player->GetInstanceScript()))
					host->GetDialogueScenes().OnPlayerEnter(player);
			}, 3s);
		}

		void OnPlayerLeave(Player* player) override
		{
			CustomScenario::RemovePhaseAuras(player, PhaseAuras);
			// Bouton "Evacuer le blesse" (npc_wounded_theramore_troop).
			player->RemoveAurasDueToSpell(SPELL_EVACUATE_OVERRIDE);
			scenes.OnPlayerLeave(player);
		}

		CustomScenario::DialogueSceneDirector& GetDialogueScenes() override
		{
			return scenes;
		}

		CustomScenario::AmbientVoices& GetAmbientVoices() override
		{
			return voices;
		}

		void SetData(uint32 dataId, uint32 value) override
		{
			if (dataId == DATA_SCENARIO_PHASE)
			{
				phase = (BFTPhases)value;
				CustomScenario::SyncPhaseAuras(instance, value, PhaseAuras);
			}
			else if (dataId == DATA_WOUNDED_TROOPS)
				woundedTroops = value;
			else if (dataId == DATA_EVACUATION_SCENE)
				PlayEvacuationScene(EvacuationScene(value));
		}

		// Comptabilite des vagues : seul et unique point de credit.
		//
		// Le chemin automatique (KillRewarder::Reward) ne credite le scenario
		// qu'avec l'entry reelle de la victime et ignore KillCredit, donc les
		// quatre entries de vague ne peuvent pas y alimenter le meme criteria.
		// Et le faire depuis npc_theramore_horde::JustDied laissait de cote
		// toutes les morts non achevees par un joueur. On credite donc
		// NPC_WAVE_MEMBER_CREDIT ici, une fois par mort, quel que soit le
		// tueur.
		//
		// Le filtre est le string id pose au spawn : sans lui, n'importe quel
		// PNJ de la Horde present sur la carte ferait avancer la barre.
		void OnUnitDeath(Unit* unit) override
		{
			InstanceScript::OnUnitDeath(unit);

			Creature* creature = unit->ToCreature();
			if (creature && creature->GetEntry() == NPC_CAPTAIN_DROK)
				creature->SetVignette(VIGNETTE_NONE);

			if (!creature || !creature->HasStringId(WaveMemberStringId))
				return;

			InstanceScenario* scenario = instance->GetInstanceScenario();
			if (!scenario)
				return;

			// CriteriaHandler::UpdateCriteria refuse un referencePlayer nul et
			// ignore les joueurs en mode MJ. Le criteria est a l'echelle du
			// scenario, pas du joueur : n'importe quel eligible fait l'affaire.
			Player* creditPlayer = GetCriteriaCreditPlayer();
			if (!creditPlayer)
			{
				TC_LOG_ERROR("scripts", "BFT: aucun joueur eligible dans l'instance, le credit de vague est perdu "
					"(mode MJ actif ?).");
				return;
			}

			scenario->UpdateCriteria(CriteriaType::KillCreature, NPC_WAVE_MEMBER_CREDIT, 1, 0, creature, creditPlayer);

			// La barre de progression appartient au criteria tree : c'est lui qui
			// decide de la fin de la bataille (CRITERIA_TREE_SURVIVE_WAVES). Le
			// script ne repond qu'a une autre question, qu'aucun criteria ne sait
			// exprimer : le terrain est-il degage pour envoyer la suite ?
			if (!IsWaveCleared())
				return;

			if (waves < HORDE_WAVES_COUNT)
				NextWave();
			else
				CompleteWaves();
		}

		// =================================================================
		// Progression du scenario
		// =================================================================
		// Point d'entree principal : chaque criteria tree valide represente
		// une etape terminee par les joueurs. On y fait avancer la phase et
		// on amorce la cinematique suivante (events.ScheduleEvent).
		void OnCompletedCriteriaTree(CriteriaTree const* tree) override
		{
			switch (tree->ID)
			{
				// Step 1 : Find Jaina
				case CRITERIA_TREE_FIND_JAINA:
				{
					ClosePortal(DATA_PORTAL_TO_STORMWIND);
					GetTervosh()->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					GetKinndy()->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					GetKalecgosHuman()->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::TheCouncil);
					PlayCouncilScene();
					break;
				}
				// Step 2 : The Council
				case CRITERIA_TREE_THE_COUNCIL:
				{
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::Waiting);
					events.ScheduleEvent(EVT_WAITING_TRIGGER, BFTScenes::StagingPause);
					break;
				}
				// Step 3 : Waiting
				case CRITERIA_TREE_WAITING:
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::UnknownTauren);
					events.ScheduleEvent(EVT_TAUREN_SCENE, 1s);
					break;
				// Step 4 : The Unknow Tauren
				case CRITERIA_TREE_UNKNOW_TAUREN:
				{
					if (Creature* kinndy = GetKinndy())
					{
						kinndy->SetVisible(true);
						kinndy->GetMotionMaster()->MovePath(KinndyPath02, false);
					}
					if (Creature* tervosh = GetTervosh())
					{
						tervosh->SetVisible(true);
						tervosh->GetMotionMaster()->MovePath(TervoshPath03, false);
					}
					for (ObjectGuid guid : citizens)
					{
						if (Creature* citizen = instance->GetCreature(guid))
						{
							citizen->SetNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
							citizen->SetVignette(VIGNETTE_THERAMORE_CITIZEN);
						}
					}
					SetEvacuationWitnesses(true);
					// Le crieur lit l'ordre d'evacuation que Jaina vient de
					// rediger (SCENE_BFT_UNKNOWN_TAUREN).
					if (Creature* crier = GetCreature(DATA_THERAMORE_CRIER))
					{
						crier->SetVisible(true);
						crier->AI()->DoAction(ACTION_EVACUATION_START);
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::Evacuation);
					break;
				}
				// Step 5 : Evacuation
				case CRITERIA_TREE_EVACUATION:
				{
					// Tous les citoyens sont a bord : ceux qu'on a prevenus comme
					// ceux qui attendaient encore, foule du crieur comprise.
					for (ObjectGuid guid : civilians)
					{
						Creature* citizen = instance->GetCreature(guid);
						if (!citizen || (citizen->GetEntry() != NPC_THERAMORE_CITIZEN_MALE && citizen->GetEntry() != NPC_THERAMORE_CITIZEN_FEMALE))
							continue;

						citizen->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
						citizen->SetVignette(VIGNETTE_NONE);
						citizen->AI()->DoAction(ACTION_EVACUATION_END);
					}
					if (Creature* crier = GetCreature(DATA_THERAMORE_CRIER))
						crier->AI()->DoAction(ACTION_EVACUATION_END);
					SetEvacuationWitnesses(false);
					// Un temps de recul avant que L'Etoile du matin ne largue
					// les amarres.
					scheduler.Schedule(MORNING_STAR_DEPARTURE_DELAY, [this](TaskContext /*context*/)
					{
						MorningStarDeparture();
					});
					if (Creature* jaina = GetJaina())
					{
						jaina->SetVignette(VIGNETTE_LADY_JAINA_PROUDMOORE);
						jaina->NearTeleportTo(JainaPoint02);
						jaina->SetHomePosition(JainaPoint02);
						jaina->AI()->SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::ALittleHelp);
					}
					if (Creature* tervosh = GetTervosh())
					{
						tervosh->GetMotionMaster()->Clear();
						tervosh->GetMotionMaster()->MoveIdle();
						tervosh->NearTeleportTo(TervoshPoint01);
						tervosh->SetHomePosition(TervoshPoint01);
						tervosh->CastSpell(tervosh, SPELL_COSMETIC_FIRE_LIGHT);
					}
					if (Creature* kinndy = GetKinndy())
					{
						kinndy->GetMotionMaster()->Clear();
						kinndy->GetMotionMaster()->MoveIdle();
						kinndy->NearTeleportTo(KinndyPoint02);
						kinndy->SetHomePosition(KinndyPoint02);
					}
					if (Creature* kalecgos = GetKalecgosHuman())
					{
						kalecgos->GetMotionMaster()->Clear();
						kalecgos->GetMotionMaster()->MoveIdle();
						kalecgos->NearTeleportTo(KalecgosPoint01);
						kalecgos->SetHomePosition(KalecgosPoint01);
						kalecgos->RemoveUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
						kalecgos->SetImmuneToAll(false);
						kalecgos->RemoveAllAuras();
						// Masque depuis sa sortie de la tour (scene du conseil).
						ShowAfterTeleport(kalecgos);
					}
					if (Creature* hedric = GetHedric())
					{
						hedric->GetMotionMaster()->Clear();
						hedric->GetMotionMaster()->MoveIdle();
						hedric->SetHomePosition(HedricPoint01);
						hedric->NearTeleportTo(HedricPoint01);
						hedric->SetVisible(false);
					}
					// Les generaux de la 7e Legion n'arrivent qu'apres les
					// archimages (SCENE_BFT_A_LITTLE_HELP).
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::ALittleHelp);
					break;
				}
				// Step 6 : A Little Help
				case CRITERIA_TREE_A_LITTLE_HELP:
				{
					// L'Etoile du matin est partie, la flotte de la 7e Legion
					// mouille au large.
					instance->SpawnGroupDespawn(SPAWN_GROUP_MORNING_STAR, true);
					instance->SpawnGroupSpawn(SPAWN_GROUP_SEVENTH_FLEET, true);
					for (uint8 i = 0; i < FIRE_LOCATION; i++)
					{
						const Position pos = FireLocation[i];
						if (TempSummon* trigger = instance->SummonCreature(NPC_THERAMORE_FIRE_CREDIT, pos))
							trigger->AddAura(SPELL_COSMETIC_LARGE_FIRE, trigger);
					}
					for (ObjectGuid guid : tanks)
					{
						if (Creature* tank = instance->GetCreature(guid))
						{
							tank->SetNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
							tank->SetVignette(VIGNETTE_UNMANNED_TANK);
							tank->SetRegenerateHealth(false);
							tank->SetHealth((float)tank->GetHealth() * frand(0.15f, 0.60f));
						}
					}
					for (ObjectGuid guid : civilians)
					{
						if (Creature* citizen = instance->GetCreature(guid))
							citizen->SetVisible(false);
					}
					for (ObjectGuid guid : troops)
					{
						if (Creature* troop = instance->GetCreature(guid))
						{
							troop->SetVignette(VIGNETTE_ALLIANCE_TROOPS);
							switch (troop->GetCreatureTemplate()->unit_class)
							{
								case UNIT_CLASS_PALADIN:
									troop->SetEmoteState(EMOTE_STATE_READY2H);
									break;
								case UNIT_CLASS_MAGE:
									troop->SetEmoteState(RAND(EMOTE_STATE_READY1H, EMOTE_STATE_READY2HL));
									break;
								case UNIT_CLASS_ROGUE:
									break;
								default:
									troop->SetEmoteState(EMOTE_STATE_READY1H);
									break;
							}
						}
					}
					// Objet actif : aucun joueur ne passe pres du portail avant la
					// bataille, sa grille se dechargerait et l'emporterait.
					if (GameObject* portal = GetJaina()->SummonGameObject(GOB_PORTAL_TO_ORGRIMMAR, PortalPoint02, QuaternionData::QuaternionData(), 0s))
						portal->setActive(true);
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::Preparation);
					events.ScheduleEvent(EVT_HELP_SCENE, 1s);
					break;
				}
				// Step 7 : Preparation - Parent
				case CRITERIA_TREE_PREPARATION:
					GetJaina()->SetVignette(VIGNETTE_LADY_JAINA_PROUDMOORE);
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::TheBattle);
					break;
				// Step 7 : Preparation - Troops motivated
				case CRITERIA_TREE_TOOPS_MOTIVATED:
					for (ObjectGuid guid : troops)
					{
						if (Creature* troop = instance->GetCreature(guid))
							troop->SetVignette(VIGNETTE_NONE);
					}
					break;
				// Step 7 : Preparation - Speak with Rhonin
				case CRITERIA_TREE_TALK_TO_RHONIN:
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::Preparation_Rhonin);
					break;
				// Step 7 : Preparation - Tanks events
				case CRITERIA_TREE_REPAIR_TANKS:
				{
					for (ObjectGuid guid : tanks)
					{
						if (Creature* tank = instance->GetCreature(guid))
						{
							if (Creature* fire = tank->FindNearestCreature(NPC_THERAMORE_FIRE_CREDIT, 5.f))
								fire->DespawnOrUnsummon();

							tank->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
							tank->SetRegenerateHealth(true);
							tank->SetHealth(tank->GetMaxHealth());
						}
					}
					break;
				}            
				// Step 8 : The Battle - Retrieve Lady Jaina Proudmoore
				case CRITERIA_TREE_RETRIEVE_JAINA:
				{
					if (Creature* jaina = GetJaina())
					{
						Talk(jaina, SAY_BATTLE_01);
						SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, jaina);
						jaina->SetVignette(VIGNETTE_BATTLE_JAINA);
						jaina->SetBoundingRadius(20.f);
						jaina->SetRegenerateHealth(false);
					}
					if (Creature* rhonin = GetRhonin())
					{
						SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, rhonin);
						rhonin->SetRegenerateHealth(false);
						rhonin->SetVignette(VIGNETTE_BATTLE_RHONIN);
					}
					if (Creature* kalecgos = GetKalecgosDragon())
						kalecgos->SetRegenerateHealth(false);
					instance->SpawnGroupSpawn(SPAWN_GROUP_SEA_WOLF, true);
					// Janene sort de l'auberge pour lutter contre l'incendie.
					if (Creature* janene = GetCreature(DATA_INNKEEPER_JANENE))
						janene->AI()->DoAction(ACTION_FIGHT_FIRE);
					if (GameObject* powder = GetGameObject(DATA_POWDER_BARREL))
						powder->SetVignette(VIGNETTE_POWDER_BARREL);			
					// Tous les tanks sauf le dernier sont detruits au debut de
					// l'assaut. size() etant non signe, un vecteur vide ferait
					// deborder la borne : la garde ci-dessous l'evite.
					for (uint8 i = 0; !tanks.empty() && i < tanks.size() - 1; i++)
					{
						if (Creature* tank = instance->GetCreature(tanks[i]))
							tank->KillSelf();
					}
					HordeMembersInvoker(DATA_DECORATION_WEST, true);
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::TheBattle_Survive);
					events.ScheduleEvent(EVT_BATTLE_SCENE, BFTScenes::StagingPause);
					break;
				}
				// Step 9 : The Battle - Parent
				case CRITERIA_TREE_SURVIVE_THE_BATTLE:
				{
					SpawnWoundedTroops();
					RelocateTroops();
					// Canon : la Horde a libere Thalen pendant l'assaut. Sa cellule
					// est vide quand Hedric vient faire son rapport a Jaina.
					if (Creature* thalen = GetThalen())
						thalen->DespawnOrUnsummon();
					// Shandris et ses sentinelles sont parties le traquer par la
					// route du nord : elle ne sera pas la quand la bombe tombera.
					if (Creature* shandris = GetCreature(DATA_SHANDRIS_FEATHERMOON))
						shandris->DespawnOrUnsummon();
					GetBarrier01()->ResetDoorOrButton();
					GetBarrier02()->ResetDoorOrButton();
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::HelpTheWounded);
					events.ScheduleEvent(EVT_WOUNDED_SCENE, 3s);
					break;
				}
				// Step 9 : The Battle - After 10 waves
				case CRITERIA_TREE_SURVIVE_WAVES:
				{
					DespawnDummies();
					if (Creature* kalecgos = GetKalecgosDragon())
						kalecgos->AI()->SetData(DATA_KALECGOS_CANCEL_EVENT, 0U);
					break;
				}
				// Step 9 : The Battle - After the protection broke
				case CRITERIA_TREE_MAINTAIN_PROTECTION:
					NextWave();
					break;
				// Step 10 : Help the wounded - Parent
				case CRITERIA_TREE_HELP_THE_WOUNDED:
				{
					// Les joueurs ont fini avant la fin des dialogues : on
					// coupe la partie II (sa fin remet Jaina et Kinndy en place).
					events.CancelEvent(EVT_WOUNDED_KINNDY_SCENE);
					scenes.Cancel(instance, SCENE_BFT_WOUNDED_KINNDY);
					FaceAfterWounded();
					GetJaina()->SetVignette(VIGNETTE_LADY_JAINA_PROUDMOORE);
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::WaitForAmara);
					break;
				}
				// Step 10 : Help the wounded - Rejoin Lady Jaina Proudmoore after the attack
				case CRITERIA_TREE_FOLLOW_JAINA:
					events.ScheduleEvent(EVT_WOUNDED_KINNDY_SCENE, 3s);
					break;
				// Step 10 : Help the wounded - Help teleporting the wounded troops
				case CRITERIA_TREE_HELP_THE_TROOPS:
				{
					if (Creature* jaina = GetJaina())
					{
						std::list<Creature*> results;
						jaina->GetCreatureListWithEntryInGrid(results, NPC_THERAMORE_WOUNDED_TROOP, SIZE_OF_GRIDS);

						if (results.empty())
							return;

						// Etape validee : plus de bouton "Evacuer le blesse".
						for (Creature* woundedTroop : results)
						{
							woundedTroop->SetVignette(VIGNETTE_NONE);
							woundedTroop->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
							if (woundedTroop->IsAIEnabled())
								woundedTroop->AI()->DoAction(ACTION_WOUNDED_EVACUATION_END);
						}
					}
					break;
				}
				// Step 10 : Help the wounded - Extinguish the fires
				case CRITERIA_TREE_EXTINGUISH_FIRES:
					MassDespawn(NPC_THERAMORE_FIRE_CREDIT);
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::HelpTheWounded_Extinguish);
					break;
				// Step 11 : Wait for Archmage Leeson returns - Parent
				case CRITERIA_TREE_WAIT_ARCHMAGE_LEESON:
					break;
				// Step 11 : Wait for Archmage Leeson returns - Rejoin Lady Jaina Proudmoore
				case CRITERIA_TREE_JOIN_JAINA:
				{
					if (Creature* kalecgos = GetKalecgosHuman())
					{
						kalecgos->GetMotionMaster()->Clear();
						kalecgos->GetMotionMaster()->MoveIdle();
						kalecgos->NearTeleportTo(KalecPath02.Nodes[0].X, KalecPath02.Nodes[0].Y, KalecPath02.Nodes[0].Z, *KalecPath02.Nodes[0].Orientation);
						ShowAfterTeleport(kalecgos);
					}
					// Rhonin rejoint la table de banquet dans la scene qui suit.
					if (Creature* rhonin = GetRhonin())
					{
						for (uint8 i = 0; i < TOWER_BARRIERS_LOCATION; i++)
						{
							rhonin->SummonGameObject(GOB_ENERGY_BARRIER_TOWER, TowerBarriers[i].position, TowerBarriers[i].quaternion, 0s);
						}
					}
					for (uint8 i = 0; i < eventCreatureDataCount; i++)
					{
						if (Creature* creature = GetCreature(creatureData[i].type))
							creature->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::WaitForAmara_JoinJaina);
					events.ScheduleEvent(EVT_AMARA_SCENE, 1s);
					break;
				}
				// Step 11 : Wait for Archmage Leeson returns - Wait for Archmage Leeson returns
				// Leve par le signal "AmaraBack" : la scene SCENE_BFT_WAIT_FOR_AMARA
				// enchaine elle-meme sur le dialogue avec Amara.
				case CRITERIA_TREE_ARCHMAGE_LEESON:
					break;
				// Step 12 : Retrieve Rhonin - Parent
				case CRITERIA_TREE_RETRIEVE_RHONIN:
					DoCastSpellOnPlayers(SPELL_THERAMORE_EXPLOSION_SCENE);
					break;
				// Step 12 : Retrieve Rhonin - Retrieve Rhonin at the top of the tower
				case CRITERIA_TREE_RETRIEVE:
					GetRhonin()->SetVignette(VIGNETTE_NONE);
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::RetrieveRhonin_JoinRhonin);
					events.ScheduleEvent(EVT_RHONIN_SCENE, 1s);
					break;
			}
		}

		// Classement des creatures au spawn : chaque groupe est memorise dans
		// sa propre liste de GUID, ce qui permet ensuite de les manipuler en
		// masse (marqueurs d'objectif, teleports, despawns).
		void OnCreatureCreate(Creature* creature) override
		{
			InstanceScript::OnCreatureCreate(creature);

			// Portee de visibilite maximale : la bataille se joue a l'echelle
			// de toute la ville, on ne veut aucun pop-in.
			creature->SetVisibilityDistanceOverride(VisibilityDistanceType::Infinite);

			// Acteurs des cinematiques : objets actifs. Theramore est coupee
			// entre deux grilles (tour et porte Ouest / place et portes
			// principales) ; sans joueur a proximite, la grille de la tour se
			// decharge pendant l'evacuation et emporte Jaina, Tervosh et
			// Kinndy, recrees ensuite a leur spawn. Et un NearTeleportTo vers
			// une grille non chargee renvoie la creature a son spawn.
			for (uint8 i = 0; i < eventCreatureDataCount; i++)
			{
				if (creatureData[i].entry == creature->GetEntry())
				{
					creature->setActive(true);
					break;
				}
			}

			if (creature->IsCivilian())
			{
				creature->PauseMovement();
				creature->SetPvP(false);
				creature->SetImmuneToNPC(true);
				civilians.push_back(creature->GetGUID());
			}

			switch (creature->GetEntry())
			{
				case NPC_THERAMORE_CITIZEN_MALE:
				case NPC_THERAMORE_CITIZEN_FEMALE:
					creature->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
					if (!creature->HasStringId("IgnoreClick"))
						citizens.push_back(creature->GetGUID());
					break;
				case NPC_ARCHMAGE_TERVOSH:
					creature->SetEmoteState(EMOTE_STATE_READ_BOOK_AND_TALK);
					break;
				case NPC_UNMANNED_TANK:
					creature->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
					tanks.push_back(creature->GetGUID());
					break;
				case NPC_THERAMORE_MARKSMAN:
				case NPC_THERAMORE_FOOTMAN:
				case NPC_THERAMORE_ARCANIST:
				case NPC_THERAMORE_FAITHFUL:
				case NPC_THERAMORE_OFFICER:
					// Seules les troupes statiques comptent : celles qui
					// patrouillent ou appartiennent a une formation ont deja
					// leur propre comportement et ne doivent pas etre
					// deplacees par les scripts de phase.
					if (creature->GetWaypointPathId() || creature->IsFormationLeader() || creature->GetFormation())
						break;
					troops.push_back(creature->GetGUID());
					break;
				// N'apparait qu'avec l'ordre d'evacuation (CRITERIA_TREE_UNKNOW_TAUREN).
				case NPC_THERAMORE_CRIER:
				case NPC_KALECGOS_DRAGON:
					creature->SetVisible(false);
					break;
				case NPC_JAINA_PROUDMOORE:
					creature->SetVignette(VIGNETTE_LADY_JAINA_PROUDMOORE);
					break;
			}
		}

		void OnGameObjectCreate(GameObject* go) override
		{
			InstanceScript::OnGameObjectCreate(go);

			go->SetVisibilityDistanceOverride(VisibilityDistanceType::Infinite);

			switch (go->GetEntry())
			{
				case GOB_PORTAL_TO_DALARAN:
				case GOB_PORTAL_TO_STORMWIND:
				case GOB_PORTAL_TO_ORGRIMMAR:
					go->SetLootState(GO_READY);
					go->UseDoorOrButton();
					go->SetFlag(GO_FLAG_NOT_SELECTABLE);
					break;
				// N'existe qu'une fois le Loup de mer arrive (SPAWN_GROUP_SEA_WOLF).
				case GOB_POWDER_BARREL:
					go->SetVignette(VIGNETTE_POWDER_BARREL);
					break;
				default:
					break;
			}
		}

		// =================================================================
		// Lancement des cinematiques
		// =================================================================
		// Chaque event laisse passer le delai prevu apres la validation d'une
		// etape, puis lance la scene de dialogue correspondante (region
		// DIALOGUE_SCENES).
		void Update(uint32 diff) override
		{
			scheduler.Update(diff);
			scenes.Update(instance, Milliseconds(diff));

			events.Update(diff);
			switch (events.ExecuteEvent())
			{
				// Simple validation d'etape : laisse aux joueurs le temps de
				// souffler avant l'arrivee de Perith.
				case EVT_WAITING_TRIGGER:
					TriggerGameEvent(EVENT_WAITING);
					break;
				case EVT_TAUREN_SCENE:
					PlayUnknownTaurenScene();
					break;
				case EVT_HELP_SCENE:
					PlayALittleHelpScene();
					break;
				case EVT_BATTLE_SCENE:
					PlayTheBattleScene();
					break;
				case EVT_EVAC_DOCKS_SCENE:
					PlayWitnessScene(BFTScenes::EvacDocks, { DATA_MORNING_STAR_FIRST_MATE, DATA_DRAENEI_SHAMAN });
					break;
				case EVT_EVAC_JANENE_SCENE:
					PlayWitnessScene(BFTScenes::EvacJanene, { DATA_INNKEEPER_JANENE });
					break;
				case EVT_EVAC_DOCTOR_SCENE:
					PlayWitnessScene(BFTScenes::EvacDoctor, { DATA_DOCTOR_VANHOWZEN });
					break;
				case EVT_WOUNDED_SCENE:
					PlayHelpTheWoundedScene();
					break;
				case EVT_WOUNDED_KINNDY_SCENE:
					PlayWoundedKinndyScene();
					break;
				case EVT_AMARA_SCENE:
					PlayWaitForAmaraScene();
					break;
				case EVT_RHONIN_SCENE:
					PlayRetrieveRhoninScene();
					break;
				default:
					break;
			}
		}

		// =================================================================
		// Scenes de dialogue
		// =================================================================
		// Chaque Play...Scene donne :
		//   - les vrais PNJ que la scene remplace, et la place ou elle fait
		//     apparaitre leur acteur (les memes constantes que le Lua, que le
		//     generateur lit dans battle_for_theramore.h) : au signal "Ready",
		//     ils y sont teleportes et masques ;
		//   - ce que fait le serveur aux signaux propres a la scene ;
		//   - l'etat dans lequel la scene laisse les vrais PNJ : ce que
		//     faisaient les anciens events (fin de chemin, emote, aura...),
		//     applique d'un coup quand tous les joueurs ont fini.
		#pragma region DIALOGUE_SCENES

		// Vrai PNJ remplace par un acteur de scene. Sans start, il reste ou il
		// est (spawn en base, que le Lua lit aussi, ou place laissee par la
		// scene precedente).
		struct SceneActor
		{
			uint32 dataId;
			Optional<Position> start = {};
		};

		using SceneCueHandler = CustomScenario::DialogueSceneDirector::CueHandler;
		using SceneEndHandler = CustomScenario::DialogueSceneDirector::EndHandler;

		void PlayDialogue(CustomScenario::DialogueScene const& scene, std::vector<SceneActor> cast,
						  SceneCueHandler onCue, SceneEndHandler onEnd)
		{
			// En debug, les dialogues sont sautes : leur fin s'applique aussitot.
			#ifdef CUSTOM_DEBUG
				bool const skip = true;
			#else
				bool const skip = false;
			#endif

			scenes.Play(instance, scene, [this, cast = std::move(cast), onCue = std::move(onCue)](std::string_view cue)
			{
				if (cue == CustomScenario::SceneCue::Ready)
					HideCast(cast);
				else if (onCue)
					onCue(cue);
			}, std::move(onEnd), skip);
		}

		// Les acteurs viennent d'apparaitre : les vrais PNJ s'effacent a leur place.
		void HideCast(std::vector<SceneActor> const& cast)
		{
			for (SceneActor const& actor : cast)
			{
				Creature* creature = GetCreature(actor.dataId);
				if (!creature)
					continue;

				creature->GetMotionMaster()->Clear();
				creature->GetMotionMaster()->MoveIdle();
				creature->StopMoving();
				if (actor.start)
					creature->NearTeleportTo(*actor.start);
				creature->SetVisible(false);
			}
		}

		// Fin de scene : le vrai PNJ reprend la place ou son acteur s'est arrete.
		Creature* Settle(uint32 dataId, Optional<Position> const& position = {}, bool visible = true)
		{
			Creature* creature = GetCreature(dataId);
			if (!creature)
				return nullptr;

			creature->GetMotionMaster()->Clear();
			creature->GetMotionMaster()->MoveIdle();
			creature->StopMoving();
			creature->SetTarget(ObjectGuid::Empty);
			if (position)
				creature->NearTeleportTo(*position);
			if (!visible)
				creature->SetVisible(false);
			else if (position && !creature->IsVisible())
				ShowAfterTeleport(creature);
			else
				creature->SetVisible(true);
			return creature;
		}

		// Un PNJ masque ne recoit pas le paquet de teleport, et son changement
		// de cellule n'est applique qu'a la mise a jour suivante de la carte :
		// rendu visible tout de suite, il serait cree chez le client a son
		// ancienne place et y resterait. On attend que la carte l'ait deplace.
		void ShowAfterTeleport(Creature* creature)
		{
			ObjectGuid const guid = creature->GetGUID();
			scheduler.Schedule(200ms, [this, guid](TaskContext /*context*/)
			{
				if (Creature* shown = instance->GetCreature(guid))
					shown->SetVisible(true);
			});
		}

		// Dernier point d'un chemin, la ou le MovePath aurait laisse le PNJ.
		static Position PathEnd(WaypointPath const& path, Optional<float> facing = {})
		{
			WaypointNode const& node = path.Nodes.back();
			return { node.X, node.Y, node.Z, facing.value_or(node.Orientation.value_or(0.f)) };
		}

		static Position WithFacing(Position position, float facing)
		{
			position.SetOrientation(facing);
			return position;
		}

		// Destination d'une table Location indexee par data ID.
		template <size_t N>
		static Position Destination(Location const (&table)[N], uint32 dataId)
		{
			for (Location const& location : table)
				if (location.dataId == dataId)
					return location.destination;
			return {};
		}

		// The Council : Kalecgos sort et se dissout, Tervosh monte a l'etage,
		// Kinndy quitte la salle, Jaina rejoint la table, ce qui valide l'etape
		// (npc_jaina_theramore, point 1).
		void PlayCouncilScene()
		{
			PlayDialogue(BFTScenes::Council,
			{
				{ DATA_JAINA_PROUDMOORE }, { DATA_KALECGOS }, { DATA_ARCHMAGE_TERVOSH }, { DATA_KINNDY_SPARKSHINE }
			}, {}, [this]()
			{
				ClearTarget();
				// npc_kalecgos_theramore, path 1 : il s'est teleporte et dissous dans
				// la scene, il reste masque jusqu'a l'evacuation.
				if (Creature* kalecgos = Settle(DATA_KALECGOS, PathEnd(KalecPath01), false))
				{
					kalecgos->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
					kalecgos->SetImmuneToAll(true);
				}
				// npc_archmage_tervosh et npc_kinndy_sparkshine : sortis de scene.
				if (Creature* tervosh = Settle(DATA_ARCHMAGE_TERVOSH, PathEnd(TervoshPath02, 2.14f), false))
					tervosh->SetEmoteState(EMOTE_STATE_NONE);
				Settle(DATA_KINNDY_SPARKSHINE, PathEnd(KinndyPath01, 4.62f), false);
				if (Creature* jaina = Settle(DATA_JAINA_PROUDMOORE, JainaPoint01))
					jaina->SetWalk(true);
				TriggerGameEvent(EVENT_THE_COUNCIL);
			});
		}

		// The Unknown Tauren : Perith et le chevalier n'existent que dans la
		// scene. Peine y quitte la salle (npc_pained, path 1) : elle attend,
		// masquee, la bataille, et sa sortie valide l'etape.
		void PlayUnknownTaurenScene()
		{
			PlayDialogue(BFTScenes::UnknownTauren, { { DATA_JAINA_PROUDMOORE, JainaPoint01 } }, {}, [this]()
			{
				ClearTarget();
				if (Creature* jaina = Settle(DATA_JAINA_PROUDMOORE, WithFacing(JainaPoint01, 0.39f)))
				{
					jaina->SetEmoteState(EMOTE_STATE_NONE);
					jaina->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
				}
				if (Creature* pained = instance->SummonCreature(NPC_PAINED, PathEnd(KinndyPath01)))
				{
					pained->SetWalk(true);
					pained->SetVisible(false);
				}
				TriggerGameEvent(EVENT_THE_UNKNOWN_TAUREN);
			});
		}

		// A Little Help : Hedric ouvre la scene, les archimages sortent du
		// portail de Dalaran, la 7e Legion se range devant Jaina, puis le
		// teleport de masse emmene tout le monde sur le champ de bataille.
		void PlayALittleHelpScene()
		{
			// Ambiance du siege et premiers figurants de la Horde, des le debut.
			ScheduleCameraShakes();
			HordeMembersInvoker(DATA_DECORATION_ENTRANCE, true);
			// Les cris de la foule (SOUND_FEARFUL_CROWD) sont joues par la scene :
			// Hedric est encore masque, personne ne les entendrait venir de lui.
			if (Creature* hedric = GetHedric())
				hedric->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);

			PlayDialogue(BFTScenes::ALittleHelp,
			{
				{ DATA_JAINA_PROUDMOORE, JainaPoint02 }, { DATA_ARCHMAGE_TERVOSH, TervoshPoint01 },
				{ DATA_KINNDY_SPARKSHINE, KinndyPoint02 }, { DATA_KALECGOS, KalecgosPoint01 },
				{ DATA_HEDRIC_EVENCANE, HedricPoint01 }
			}, [this](std::string_view cue)
			{
				if (cue == "Portal")
				{
					if (Creature* jaina = GetJaina())
						jaina->SummonGameObject(GOB_PORTAL_TO_DALARAN, PortalPoint01, QuaternionData::QuaternionData(), 0s);
				}
				else if (cue == "ClosePortal")
					ClosePortal(DATA_PORTAL_TO_DALARAN);
			}, [this]()
			{
				// Les vrais archimages et generaux apparaissent la ou la scene a
				// laisse les siens, juste avant la mise en place.
				for (Location const& location : archmagesLocation)
				{
					if (Creature* archmage = instance->SummonCreature(location.dataId, location.destination))
					{
						archmage->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
						archmage->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
						archmage->SetSheath(SHEATH_STATE_UNARMED);
						archmage->SetWalk(true);
					}
				}
				SpawnGenerals();
				BattlefieldSetup();
			});
		}

		// The Battle : trahison et capture de Thalen Songweaver (Tides of War,
		// chapitre 17). La barriere magique des portes principales faiblit
		// alors que Thalen est cense l'entretenir : Jaina le frappe d'un trait
		// arcanique, Peine l'assomme, deux soldats le ligotent et l'emportent
		// vers la citadelle. Juste apres, la barriere cede et la Horde
		// s'engouffre, pendant qu'a la porte Ouest Thader et Kinndy sont
		// submerges. Les deux soldats sont de vrais PNJ (OnTheBattleCue).
		void PlayTheBattleScene()
		{
			PlayDialogue(BFTScenes::TheBattle,
			{
				{ DATA_JAINA_PROUDMOORE }, { DATA_HEDRIC_EVENCANE }, { DATA_THALEN_SONGWEAVER },
				{ DATA_AMARA_LEESON }, { DATA_PAINED }
			}, [this](std::string_view cue) { OnTheBattleCue(cue); }, [this]()
			{
				ClearTarget();
				Position const thalenPost = Destination(actorsRelocation, DATA_THALEN_SONGWEAVER);
				// Elle est revenue face a la porte, a l'orientation de son poste.
				if (Creature* jaina = Settle(DATA_JAINA_PROUDMOORE))
				{
					jaina->SetFacingTo(Destination(actorsRelocation, DATA_JAINA_PROUDMOORE).GetOrientation());
					jaina->SetEmoteState(EMOTE_STATE_NONE);
					jaina->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
				}
				Settle(DATA_HEDRIC_EVENCANE);
				if (Creature* pained = Settle(DATA_PAINED, PainedPoint03))
				{
					pained->SetFacingTo(pained->GetAbsoluteAngle(thalenPost));
					pained->SetEmoteState(EMOTE_STATE_READY1H);
				}
				if (Creature* amara = Settle(DATA_AMARA_LEESON))
				{
					amara->RemoveAurasDueToSpell(SPELL_PORTAL_CHANNELING_03);
					// Son acteur vient de la lancer dans la scene : l'aura seule,
					// sans rejouer le lancer.
					amara->AddAura(SPELL_PRISMATIC_BARRIER, amara);
				}
				// Premier debarquement et bascule en mode combat : la boucle de
				// vagues demarre avec EVENT_MAINTAIN_THE_PROTECTION.
				HordeMembersInvoker(DATA_WAVE_BOAT);
				TriggerGameEvent(EVENT_MAINTAIN_THE_PROTECTION);
			});
		}

		void OnTheBattleCue(std::string_view cue)
		{
			// Peine vient d'assommer Thalen : les deux fantassins les plus proches
			// de Jaina accourent.
			if (cue == "Bind")
			{
				thalenEscort = SelectEscortFootmen();
				Creature* thalen = GetThalen();

				uint8 index = 0;
				for (ObjectGuid const& guid : thalenEscort)
				{
					if (Creature* soldier = instance->GetCreature(guid))
					{
						float const facing = thalen ? thalen->GetAbsoluteAngle(soldier) : thalenFootmenPos[index].GetOrientation();
						soldier->SetEmoteState(EMOTE_STATE_NONE);
						soldier->GetMotionMaster()->Clear();
						soldier->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, thalenFootmenPos[index],
															  true, facing, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
					}

					index++;
				}
			}
			// Ils le ligotent.
			else if (cue == "Kneel")
			{
				for (ObjectGuid const& guid : thalenEscort)
				{
					if (Creature* soldier = instance->GetCreature(guid))
						soldier->HandleEmoteCommand(EMOTE_ONESHOT_KNEEL);
				}
			}
			// Hedric confie Thalen aux deux soldats : le vrai Thalen reprend la
			// place de son acteur, assomme et ligote, et ils l'emportent a pied
			// vers la citadelle en colonne (un soldat, Thalen, l'autre soldat).
			// Thalen passe ennemi comme au moment ou Peine l'a assomme.
			else if (cue == "Escort")
			{
				Creature* thalen = GetThalen();
				if (!thalen)
					return;

				thalen->RemoveAllAuras();
				thalen->SetEmoteState(EMOTE_STATE_NONE);
				thalen->SetFaction(FACTION_ENEMY);
				thalen->SetImmuneToAll(true);
				thalen->CastSpell(thalen, SPELL_STUNNED);
				thalen->AddAura(SPELL_TIED_UP, thalen);
				thalen->SetVisible(true);

				std::array<Creature*, 3> const column =
				{
					thalenEscort.size() > 0 ? instance->GetCreature(thalenEscort[0]) : nullptr,
					thalen,
					thalenEscort.size() > 1 ? instance->GetCreature(thalenEscort[1]) : nullptr
				};

				for (uint8 i = 0; i < column.size(); i++)
				{
					if (!column[i])
						continue;

					ObjectGuid const guid = column[i]->GetGUID();
					scheduler.Schedule(ESCORT_STAGGER * i, [this, guid](TaskContext /*context*/)
					{
						if (Creature* unit = instance->GetCreature(guid))
						{
							unit->GetMotionMaster()->Clear();
							unit->GetMotionMaster()->MovePath(ThalenEscortPath, false,
															  {}, {},
															  MovementWalkRunSpeedSelectionMode::ForceWalk);
						}
					});
				}

				WatchThalenEscort();
			}
			// La barriere cede. L'escorte est deja en route vers la citadelle
			// (voir WatchThalenEscort).
			else if (cue == "BarrierBreaks")
			{
				if (GameObject* barrier = GetBarrier01())
				{
					scheduler.CancelGroup((uint32)BFTPhases::TheBattle);
					barrier->ResetDoorOrButton();
					if (Creature* trigger = barrier->SummonCreature(WORLD_TRIGGER, ExplodingPoint01, TEMPSUMMON_TIMED_DESPAWN, 5s))
						trigger->CastSpell(trigger, SPELL_ARCANIC_EXPLOSION);
				}
			}
			// A la porte Ouest, Thader et Kinndy sont pris d'assaut : Thader, a
			// bout de forces, appelle des renforts et Kinndy le soigne. C'est ce
			// tableau que les joueurs trouvent en arrivant.
			else if (cue == "ThaderWounded")
			{
				if (Creature* thader = GetCreature(DATA_THADER_WINDERMERE))
				{
					Talk(thader, SAY_BATTLE_05);
					thader->RemoveAllAuras();
					thader->SetRegenerateHealth(false);
					thader->SetReactState(REACT_PASSIVE);
					thader->SetStandState(UNIT_STAND_STATE_KNEEL);
					thader->SetHealth(thader->CountPctFromMaxHealth(5));
					thader->CastSpell(thader, SPELL_ARCANE_FX);

					if (Creature* kinndy = GetCreature(DATA_KINNDY_SPARKSHINE))
					{
						kinndy->RemoveAllAuras();
						kinndy->SetReactState(REACT_PASSIVE);
						kinndy->CastSpell(kinndy, SPELL_CHANNEL_BLUE_MOVING);
						kinndy->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_NONE, thader, 0.8f);
					}
				}
				GetBarrier02()->ResetDoorOrButton();
			}
		}

		// Scenes du livre pendant l'evacuation (Tides of War, chapitre 7) :
		// le temoin parle, puis reprend sa place.
		void PlayWitnessScene(CustomScenario::DialogueScene const& scene, std::initializer_list<uint32> witnesses)
		{
			std::vector<SceneActor> cast;
			for (uint32 dataId : witnesses)
				cast.push_back({ dataId });

			PlayDialogue(scene, std::move(cast), {}, [this, ids = std::vector<uint32>(witnesses)]()
			{
				for (uint32 dataId : ids)
					Settle(dataId);
			});
		}

		// Help The Wounded, partie I : Jaina et Hedric constatent les degats,
		// puis Jaina gagne la place (npc_jaina_theramore, path 1 : elle s'y
		// tourne vers les joueurs et leve EVENT_FIND_JAINA_04) et Hedric
		// rejoint son poste de garde (npc_hedric_evencane, point 2). Hedric a
		// combattu : il repart de son poste de la bataille.
		void PlayHelpTheWoundedScene()
		{
			PlayDialogue(BFTScenes::Wounded,
			{
				{ DATA_JAINA_PROUDMOORE, JainaPoint03 },
				{ DATA_HEDRIC_EVENCANE, Destination(actorsRelocation, DATA_HEDRIC_EVENCANE) }
			}, {}, [this]()
			{
				ClearTarget();
				if (Creature* jaina = Settle(DATA_JAINA_PROUDMOORE, PathEnd(JainaPath01)))
				{
					jaina->SetEmoteState(EMOTE_STATE_STAND);
					jaina->SetHomePosition(JainaPoint06);
					if (jaina->IsAIEnabled())
						jaina->AI()->WaypointPathEnded(0, JainaPath01.Id);
				}
				if (Creature* hedric = Settle(DATA_HEDRIC_EVENCANE, HedricPoint03))
				{
					if (hedric->IsAIEnabled())
						hedric->AI()->MovementInform(POINT_MOTION_TYPE, MOVEMENT_INFO_POINT_02);
				}
			});
		}

		// Help The Wounded, partie II : Jaina reconforte Kinndy pendant que les
		// joueurs soignent les blesses (annulee par CRITERIA_TREE_HELP_THE_WOUNDED).
		void PlayWoundedKinndyScene()
		{
			PlayDialogue(BFTScenes::WoundedKinndy, { { DATA_JAINA_PROUDMOORE }, { DATA_KINNDY_SPARKSHINE } }, {}, [this]()
			{
				Settle(DATA_JAINA_PROUDMOORE);
				if (Creature* kinndy = Settle(DATA_KINNDY_SPARKSHINE))
					kinndy->SetEmoteState(EMOTE_STATE_NONE);
				FaceAfterWounded();
			});
		}

		// Fin des dialogues d'apres-bataille, que la partie II ait ete jouee ou non.
		void FaceAfterWounded()
		{
			ClearTarget();
			if (Creature* jaina = GetJaina())
				jaina->SetFacingTo(3.15f);
			if (Creature* kinndy = GetKinndy())
				kinndy->SetFacingTo(2.73f);
		}

		// Wait For Archmage Leeson : Kalecgos previent Jaina du danger que
		// represente l'iris, Rhonin et Kalecgos s'en vont, Amara revient puis
		// repart, et Jaina part vers le sommet de la tour (npc_jaina_theramore,
		// path 2 : elle y reapparait et ouvre le portail vers Hurlevent).
		void PlayWaitForAmaraScene()
		{
			PlayDialogue(BFTScenes::WaitForAmara,
			{
				{ DATA_JAINA_PROUDMOORE }, { DATA_KALECGOS }, { DATA_RHONIN }, { DATA_AMARA_LEESON },
				{ DATA_KINNDY_SPARKSHINE }, { DATA_HEDRIC_EVENCANE }
			}, [this](std::string_view cue)
			{
				// Rhonin quitte la place et reapparait au sommet de la tour
				// (npc_rhonin, path 1).
				if (cue == "RhoninGone")
				{
					if (Creature* rhonin = Settle(DATA_RHONIN, PathEnd(RhoninPath01), false))
						if (rhonin->IsAIEnabled())
							rhonin->AI()->WaypointPathEnded(0, RhoninPath01.Id);
				}
				// Retour d'Amara (npc_amara_leeson, path 2).
				else if (cue == "AmaraBack")
					TriggerGameEvent(EVENT_WAIT_ARCHMAGE_LESSON);
			}, [this]()
			{
				ClearTarget();
				// npc_kalecgos_theramore et npc_amara_leeson, path 3 : sortis de scene.
				Settle(DATA_KALECGOS, PathEnd(KalecPath03), false);
				Settle(DATA_AMARA_LEESON, PathEnd(KalecPath03), false);
				Settle(DATA_KINNDY_SPARKSHINE);
				Settle(DATA_HEDRIC_EVENCANE);
				if (Creature* jaina = Settle(DATA_JAINA_PROUDMOORE, PathEnd(JainaPath02), false))
					if (jaina->IsAIEnabled())
						jaina->AI()->WaypointPathEnded(0, JainaPath02.Id);
			});
		}

		// Retrieve Rhonin : dernier acte au sommet de la tour. La fin leve
		// EVENT_REDUCE_IMPACT, qui bascule les joueurs dans la scene de
		// l'explosion (voir scene_theramore_explosion en bas de fichier).
		void PlayRetrieveRhoninScene()
		{
			PlayDialogue(BFTScenes::RetrieveRhonin, { { DATA_JAINA_PROUDMOORE }, { DATA_RHONIN } }, {}, [this]()
			{
				Creature* jaina = Settle(DATA_JAINA_PROUDMOORE);
				Creature* rhonin = Settle(DATA_RHONIN);
				if (jaina)
				{
					jaina->RemoveAllAuras();
					if (rhonin)
					{
						jaina->SetFacingToObject(rhonin);
						jaina->SetTarget(rhonin->GetGUID());
					}
				}
				if (rhonin)
				{
					rhonin->RemoveAllAuras();
					rhonin->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					if (jaina)
						rhonin->SetTarget(jaina->GetGUID());
				}
				TriggerGameEvent(EVENT_REDUCE_IMPACT);
			});
		}

		// Mise en place du champ de bataille, au bout du teleport de masse :
		// Kalecgos dragon prend son vol, tous les acteurs sont teleportes a leur
		// poste (actorsRelocation) et recoivent leur role de la bataille
		// (vignette, gossip, channel). La suite passe par
		// CRITERIA_TREE_RETRIEVE_JAINA.
		void BattlefieldSetup()
		{
			EnsureBarrierHasDamage();
			if (Creature* kalecgos = GetKalecgosDragon())
			{
				kalecgos->setActive(true);
				kalecgos->SetVisible(true);
				kalecgos->SetSpeed(MOVE_RUN, 25.f);
				kalecgos->AI()->SetData(DATA_KALECGOS_CIRCLE_EVENT, 0U);
			}
			for (uint8 i = 0; i < ACTORS_RELOCATION; i++)
			{
				if (Creature* creature = GetCreature(actorsRelocation[i].dataId))
				{
					// Z recale sur le sol : les points sont releves manuellement
					Position destination = actorsRelocation[i].destination;
					creature->UpdateAllowedPositionZ(destination.GetPositionX(), destination.GetPositionY(), destination.m_positionZ);

					creature->RemoveAllAuras();
					creature->SetTarget(ObjectGuid::Empty);
					creature->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					creature->SetSheath(SHEATH_STATE_MELEE);
					creature->GetMotionMaster()->Clear();
					creature->GetMotionMaster()->MoveIdle();
					creature->NearTeleportTo(destination);
					creature->SetHomePosition(destination);
					creature->DoNotReacquireSpellFocusTarget();
					// Masques pendant la scene A Little Help (voir ShowAfterTeleport).
					if (!creature->IsVisible() && creature->GetEntry() != NPC_KALECGOS
						&& creature->GetEntry() != NPC_VEREESA_WINDRUNNER)
						ShowAfterTeleport(creature);

					switch (creature->GetEntry())
					{
						case NPC_AMARA_LEESON:
							creature->CastSpell(creature, SPELL_PORTAL_CHANNELING_03);
							break;
						case NPC_RHONIN:
							creature->SetVignette(VIGNETTE_RHONIN);
							creature->CastSpell(creature, SPELL_CHAT_BUBBLE, true);
							creature->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
							break;
						case NPC_THADER_WINDERMERE:
							creature->SetVignette(VIGNETTE_THADER_WINDERMERE);
							creature->CastSpell(creature, SPELL_CHAT_BUBBLE, true);
							creature->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
							break;
						case NPC_THALEN_SONGWEAVER:
							creature->CastSpell(creature, SPELL_PORTAL_CHANNELING_02);
							break;
						case NPC_JAINA_PROUDMOORE:
							GetBarrier01()->UseDoorOrButton();
							TeleportPlayers(GetJaina(), actorsRelocation[i].destination, 15.0f);
							break;
						case NPC_KALECGOS:
						case NPC_VEREESA_WINDRUNNER:
							creature->SetVisible(false);
							break;
					}
				}
			}
			// Peine reste aux cotes de Jaina : c'est elle qui assommera Thalen
			// (SCENE_BFT_THE_BATTLE). Masquee depuis sa sortie de la salle du
			// conseil, elle reapparait ici.
			if (Creature* pained = GetPained())
			{
				pained->SetSheath(SHEATH_STATE_MELEE);
				pained->GetMotionMaster()->Clear();
				pained->GetMotionMaster()->MoveIdle();
				pained->NearTeleportTo(PainedPoint02);
				pained->SetHomePosition(PainedPoint02);
				ShowAfterTeleport(pained);
			}
		}

		#pragma endregion

		// =================================================================
		// Etat interne
		// =================================================================
		EventMap events;                  // Delais avant les cinematiques
		CustomScenario::DialogueSceneDirector scenes;   // Scenes de dialogue en cours
		CustomScenario::AmbientVoices voices{ scenes }; // Crieur et civils : une voix a la fois, aucune pendant une scene
		TaskScheduler scheduler;          // Taches recurrentes (auras, explosions d'ambiance)
		uint32 woundedTroops;             // Blesses deja evacues par les joueurs
		uint8 waves;                      // Index de la prochaine vague dans Waves[]
		BFTPhases phase;                  // Phase courante du scenario

		// Listes de GUID constituees dans OnCreatureCreate, manipulees en
		// masse par les transitions de phase.
		GuidVector citizens;              // Civils cliquables pendant Evacuation
		GuidVector civilians;             // Tous les civils (masques au debut de la bataille)
		GuidVector tanks;                 // Tanks a reparer pendant ALittleHelp
		GuidVector troops;                // Troupes statiques (motivation, blesses, banquet)
		GuidVector dummyMembers;          // Figurants Horde cosmetiques, despawnes en fin de bataille
		GuidVector waveMembers;           // Combattants de la vague en cours (voir IsWaveCleared)
		GuidVector thalenEscort;          // Les deux soldats qui emportent Thalen (scene de la trahison)
		GuidVector thalenEscortWalking;   // Membres de l'escorte encore sur ThalenEscortPath (voir WatchThalenEscort)
		GuidUnorderedSet introSeen;       // Joueurs a qui la scene d'introduction a deja ete jouee
		std::bitset<size_t(EvacuationScene::Max)> evacuationScenesPlayed;   // Scenes du livre deja jouees

		// =================================================================
		// Accesseurs
		// =================================================================
		// Raccourcis de lecture sur les acteurs du scenario. Ils peuvent
		// renvoyer nullptr : les appels directs sans test (GetJaina()->...)
		// supposent que l'acteur est vivant a ce stade du scenario.
		#pragma region ACCESSORS

		// Kalecgos existe en deux exemplaires distincts : la forme humanoide
		// qui participe aux dialogues, et le dragon qui survole la bataille.
		// Les deux accesseurs sont explicites pour qu'on ne les confonde pas.
		Creature* GetJaina()            { return GetCreature(DATA_JAINA_PROUDMOORE); }
		Creature* GetKinndy()           { return GetCreature(DATA_KINNDY_SPARKSHINE); }
		Creature* GetTervosh()          { return GetCreature(DATA_ARCHMAGE_TERVOSH); }
		Creature* GetKalecgosHuman()    { return GetCreature(DATA_KALECGOS); }
		Creature* GetKalecgosDragon()   { return GetCreature(DATA_KALECGOS_DRAGON); }
		Creature* GetPained()           { return GetCreature(DATA_PAINED); }
		Creature* GetPerith()           { return GetCreature(DATA_PERITH_STORMHOOVE); }
		Creature* GetKnight()           { return GetCreature(DATA_KNIGHT_OF_THERAMORE); }
		Creature* GetHedric()           { return GetCreature(DATA_HEDRIC_EVENCANE); }
		Creature* GetRhonin()           { return GetCreature(DATA_RHONIN); }
		Creature* GetVereesa()          { return GetCreature(DATA_VEREESA_WINDRUNNER); }
		Creature* GetThalen()           { return GetCreature(DATA_THALEN_SONGWEAVER); }
		Creature* GetAmara()            { return GetCreature(DATA_AMARA_LEESON); }
		Creature* GetDrok()             { return GetCreature(DATA_CAPTAIN_DROK); }
		Creature* GetGruhta()           { return GetCreature(DATA_WAVE_CALLER_GRUHTA); }

		GameObject* GetBarrier01()      { return GetGameObject(DATA_MYSTIC_BARRIER_01); }
		GameObject* GetBarrier02()      { return GetGameObject(DATA_MYSTIC_BARRIER_02); }

		#pragma endregion

		// =================================================================
		// Utilitaires
		// =================================================================
		#pragma region UTILS

		// Rayon de dispersion et decalage vertical du teleport groupe.
		static constexpr float TELEPORT_SPREAD_RADIUS     = 8.0f;
		// Scene de Thalen : Peine le prend a revers pour l'assommer.
		static constexpr float PAINED_BEHIND_DISTANCE     = 1.5f;
		// Recul de Thalen sous le trait arcanique de Jaina.
		static constexpr float THALEN_KNOCKBACK_SPEED_XY  = 6.0f;
		static constexpr float THALEN_KNOCKBACK_SPEED_Z   = 3.0f;
		// Escorte : rayon de recherche des fantassins autour de Jaina, et
		// ecart entre les departs de la colonne sur ThalenEscortPath.
		static constexpr float ESCORT_SEARCH_RANGE        = 60.0f;
		static constexpr Milliseconds ESCORT_STAGGER      = 1500ms;
		static constexpr float TELEPORT_Z_OFFSET          = 3.0f;
		// Proportion des troupes survivantes qui laissent un blesse a evacuer.
		static constexpr uint32 WOUNDED_SPAWN_CHANCE  = 80;
		// Fin de l'evacuation -> le second crie l'ordre du depart (les civils
		// disparaissent des la fin de l'etape ; le delai laisse finir la scene
		// du quai si le second est le dernier temoin), puis monte a bord.
		static constexpr Milliseconds MORNING_STAR_DEPARTURE_DELAY = 32s;
		static constexpr Milliseconds MORNING_STAR_CAST_OFF_DELAY  = 35s;

		// Talk null-safe : un acteur despawne ne doit pas faire tomber le
		// serveur, la replique est simplement perdue. Les dialogues des
		// cinematiques, eux, sont dits par les scenes.
		void Talk(Creature* creature, uint8 textId)
		{
			if (creature && creature->IsAIEnabled())
				creature->AI()->Talk(textId);
		}

		// Joueur au nom duquel crediter un criteria de scenario.
		//
		// CriteriaHandler::UpdateCriteria sort immediatement si referencePlayer
		// est nul, et ignore aussi les joueurs en mode MJ : passer le premier
		// venu suffit a perdre silencieusement tous les credits pendant un test.
		// On prefere donc un joueur hors mode MJ, avec repli sur le premier.
		Player* GetCriteriaCreditPlayer() const
		{
			Player* fallback = nullptr;
			for (MapReference const& reference : instance->GetPlayers())
			{
				Player* player = reference.GetSource();
				if (!player)
					continue;

				if (!player->IsGameMaster())
					return player;

				if (!fallback)
					fallback = player;
			}

			return fallback;
		}

		// Retourne le premier joueur encore en jeu dans l'instance, ou nullptr.
		// Centralise le pattern instance->GetPlayers().begin()->GetSource(),
		// qui n'est pas null-safe : la liste est vide des que le dernier
		// joueur a quitte l'instance.
		Player* GetFirstPlayer() const
		{
			auto const& playerList = instance->GetPlayers();
			if (playerList.empty())
				return nullptr;
			return playerList.begin()->GetSource();
		}

		// Temoins de l'evacuation : gossip et vignette le temps de l'etape.
		// Le gossip de chacun se retire seul une fois qu'on lui a parle.
		void SetEvacuationWitnesses(bool active)
		{
			static constexpr std::pair<uint32, uint32> Witnesses[] =
			{
				{ DATA_INNKEEPER_JANENE,        VIGNETTE_INNKEEPER_JANENE        },
				{ DATA_DOCTOR_VANHOWZEN,        VIGNETTE_DOCTOR_VANHOWZEN        },
				{ DATA_MORNING_STAR_FIRST_MATE, VIGNETTE_MORNING_STAR_FIRST_MATE }
			};

			for (auto const& [dataId, vignette] : Witnesses)
			{
				Creature* witness = GetCreature(dataId);
				if (!witness)
					continue;

				if (active)
				{
					witness->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
					witness->SetVignette(vignette);
				}
				else
				{
					witness->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
					witness->SetVignette(VIGNETTE_NONE);
				}
			}
		}

		// Scene du livre demandee par le gossip d'un temoin : une seule fois,
		// et seulement pendant l'evacuation (le quai n'a plus de navire apres).
		void PlayEvacuationScene(EvacuationScene scene)
		{
			if (phase != BFTPhases::Evacuation || scene >= EvacuationScene::Max)
				return;

			size_t const index = size_t(scene);
			if (evacuationScenesPlayed.test(index))
				return;
			evacuationScenesPlayed.set(index);

			switch (scene)
			{
				case EvacuationScene::Docks:
					events.ScheduleEvent(EVT_EVAC_DOCKS_SCENE, 1s);
					break;
				case EvacuationScene::Janene:
					events.ScheduleEvent(EVT_EVAC_JANENE_SCENE, 1s);
					break;
				case EvacuationScene::Doctor:
					events.ScheduleEvent(EVT_EVAC_DOCTOR_SCENE, 1s);
					break;
				default:
					break;
			}
		}

		// L'Etoile du matin appareille pour Gadgetzan : le second crie
		// l'ordre, puis monte a bord avec le chaman. Le navire et sa
		// passerelle ne partent qu'a la validation de l'etape Une petite aide,
		// quand la flotte de la 7e Legion prend leur place et que les joueurs
		// sont a la citadelle. Les civils, eux, ont embarque a la fin de
		// l'etape.
		void MorningStarDeparture()
		{
			Talk(GetCreature(DATA_MORNING_STAR_FIRST_MATE), SAY_EVAC_MATE_DEPARTURE);

			scheduler.Schedule(MORNING_STAR_CAST_OFF_DELAY, [this](TaskContext /*context*/)
			{
				if (Creature* firstMate = GetCreature(DATA_MORNING_STAR_FIRST_MATE))
					firstMate->DespawnOrUnsummon();
				if (Creature* shaman = GetCreature(DATA_DRAENEI_SHAMAN))
					shaman->DespawnOrUnsummon();
			});
		}

		// Fin de dialogue : tout le monde relache sa cible (les regards des
		// scenes ne touchent que leurs acteurs, mais un vrai PNJ peut garder
		// une cible d'avant la scene).
		void ClearTarget()
		{
			for (uint8 i = 0; i < eventCreatureDataCount; i++)
			{
				if (Creature* creature = GetCreature(creatureData[i].type))
					creature->SetTarget(ObjectGuid::Empty);
			}
		}

		// Ferme un portail proprement : on supprime le GameObject et on laisse
		// un trigger jouer l'effet visuel de fermeture a sa place.
		void ClosePortal(uint32 dataId)
		{
			if (GameObject* portal = GetGameObject(dataId))
			{
				portal->Delete();

				CastSpellExtraArgs args;
				args.SetTriggerFlags(TRIGGERED_CAST_DIRECTLY);

				const Position pos = portal->GetPosition();
				if (Creature* special = portal->SummonTrigger(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), pos.GetOrientation(), 5s))
				{
					special->CastSpell(special, SPELL_CLOSE_PORTAL, args);
				}
			}
		}

		// Teleport groupe : seuls les joueurs a moins de maxDist du caster
		// suivent. La destination est tiree au hasard autour du centre et
		// remontee de quelques metres pour eviter de faire apparaitre
		// quelqu'un dans le sol.
		void TeleportPlayers(Creature* caster, Position const& center, float maxDist)
		{
			Position pos = GetRandomPosition(caster, center, TELEPORT_SPREAD_RADIUS);
			pos.m_positionZ += TELEPORT_Z_OFFSET;

			instance->DoOnPlayers([caster, maxDist, pos](Player* player)
			{
				if (player->IsWithinDist(caster, maxDist))
				{
					player->NearTeleportTo(pos);
				}
			});
		}

		// Fait apparaitre un groupe de spawn de la Horde.
		//   waveId  : identifiant du groupe (DATA_WAVE_* / DATA_DECORATION_*)
		//   dummies : true pour un groupe purement decoratif (invulnerable,
		//             en posture de combat, sans jamais attaquer) - c'est ce
		//             qui peuple l'horizon pendant la bataille
		// Seuls les groupes listes dans Waves[] sont comptabilises : ils portent
		// WaveMemberStringId, qui est ce qui les fait crediter la barre de
		// progression (voir OnUnitDeath), et ils remplacent le contenu de
		// waveMembers - seule la vague en cours retient l'enchainement, un
		// trainard d'une vague precedente ne bloque rien. Le debarquement du
		// bateau et les figurants restent hors du decompte.
		void HordeMembersInvoker(uint32 waveId, bool dummies = false)
		{
			std::list<TempSummon*> members;

			bool const scored = std::ranges::find(Waves, waveId) != std::ranges::end(Waves);
			if (scored)
				waveMembers.clear();

			instance->SummonCreatureGroup(waveId, &members);
			for (TempSummon* horde : members)
			{
				switch (horde->GetEntry())
				{
					case NPC_WAVE_CALLER_GRUHTA:
						horde->SetVignette(VIGNETTE_WAVE_CALLER_GRUHTA);
						break;
					case NPC_CAPTAIN_DROK:
						horde->SetVignette(VIGNETTE_CAPTAIN_DROK);
						break;
				}

				horde->SetRegenerateHealth(false);
				horde->setActive(true);

				if (Unit* target = SelectNearestHostileInRange(horde))
					horde->AI()->AttackStart(target);

				if (scored)
				{
					horde->SetScriptStringId(WaveMemberStringId);
					waveMembers.push_back(horde->GetGUID());
				}

				if (dummies)
				{
					horde->SetImmuneToAll(true);

					switch (horde->GetClass())
					{
						case UNIT_CLASS_PALADIN:
							horde->SetEmoteState(EMOTE_STATE_READY2H);
							break;
						case UNIT_CLASS_MAGE:
							horde->SetEmoteState(RAND(EMOTE_STATE_READY1H, EMOTE_STATE_READY2HL));
							break;
						case UNIT_CLASS_ROGUE:
							break;
						default:
							horde->SetEmoteState(EMOTE_STATE_READY1H);
							break;
					}

					enum Spells
					{
						SPELL_CHANNEL_WATER     = 237594,
						SPELL_CHANNEL_FROST     = 1271695
					};

					switch (horde->GetEntry())
					{
						case NPC_PORTAL_TO_ORGRIMMAR:
							horde->SetUninteractible(true);
							break;
						case NPC_ROKNAH_LOA_SINGER:
							horde->CastSpell(horde, SPELL_CHANNEL_WATER);
							break;
						case NPC_ROKNAH_HAG:
							horde->CastSpell(horde, SPELL_CHANNEL_FROST);
							break;
						case NPC_HORDE_BOMBARDIER:
							horde->SetWalk(false);
							horde->SetCanFly(true);
							horde->SetDisableGravity(true);
							horde->GetMotionMaster()->MoveRandom(20.0f);
							horde->SetSpeedRate(MOVE_RUN, 2.f);
							horde->SetSpeedRate(MOVE_FLIGHT, 2.f);
							horde->m_Events.AddEvent(new HordeBombardierThrowBomb(horde),
													 horde->m_Events.CalculateTime(Seconds(urand(2, 8))));
							break;
						case NPC_HORDE_DEMOLISHER:
							if (waveId == DATA_DECORATION_WEST)
								horde->m_Events.AddEvent(new HordeDemolisherThrowBoulder(horde, WestWallStripStart, WestWallStripEnd),
														horde->m_Events.CalculateTime(Seconds(urand(2, 8))));
							else if (waveId == DATA_DECORATION_ENTRANCE && horde->GetExactDist2d(EntranceDemolisherPoint) < 5.0f)
								horde->m_Events.AddEvent(new HordeDemolisherThrowBoulder(horde, IslandStripStart, IslandStripEnd),
														horde->m_Events.CalculateTime(Seconds(urand(2, 8))));
							break;
					}

					dummyMembers.push_back(horde->GetGUID());
				}
			}

			// Jaina annonce vocalement le point d'arrivee de la vague.
			if (Creature* jaina = GetJaina())
				jaina->AI()->DoAction(waveId);

			// Chaque vague doit compter HORDE_WAVE_SIZE membres : c'est ce qui
			// fait tomber le total sur l'Amount du noeud ProgressBar. Un groupe
			// de spawn incomplet plafonnerait la barre sous les 100 %.
			if (scored && waveMembers.size() != HORDE_WAVE_SIZE)
				TC_LOG_ERROR("scripts", "BFT: le groupe de spawn {} a fait apparaitre {} membres au lieu de {}.",
					waveId, waveMembers.size(), HORDE_WAVE_SIZE);
		}

		// La vague en cours est nettoyee quand plus aucun de ses membres n'est
		// vivant.
		//
		// Une creature introuvable est lachee elle aussi : elle ne peut plus
		// etre tuee, la retenir bloquerait la bataille pour toujours. Mais elle
		// n'est jamais passee par OnUnitDeath, donc elle n'a rien credite : ce
		// cas est le seul par lequel la barre peut perdre un point, on le trace.
		bool IsWaveCleared()
		{
			std::erase_if(waveMembers, [this](ObjectGuid const& guid)
			{
				Creature* member = instance->GetCreature(guid);
				if (!member)
				{
					TC_LOG_ERROR("scripts", "BFT: membre de vague {} disparu sans mourir, credit perdu.", guid.ToString());
					return true;
				}

				return !member->IsAlive();
			});

			return waveMembers.empty();
		}

		// Envoie la vague suivante.
		void NextWave()
		{
			if (waves >= HORDE_WAVES_COUNT)
				return;

			HordeMembersInvoker(Waves[waves]);
			waves++;
		}

		// Dixieme vague nettoyee : la bataille est finie, quoi qu'affiche la
		// barre. HORDE_WAVES_COUNT * HORDE_WAVE_SIZE doit tomber exactement sur
		// l'Amount du noeud ProgressBar, donc un ecart ici est un bug, pas une
		// tolerance : on le comble pour ne pas bloquer les joueurs, mais on le
		// signale. Les TC_LOG_ERROR de HordeMembersInvoker et IsWaveCleared
		// disent lequel des deux cas s'est produit.
		void CompleteWaves()
		{
			InstanceScenario* scenario = instance->GetInstanceScenario();
			if (!scenario)
				return;

			CriteriaTree const* tree = sCriteriaMgr->GetCriteriaTree(CRITERIA_TREE_SURVIVE_WAVES);
			if (!tree)
				return;

			uint64 const progress = scenario->GetCriteriaProgressCounter(CRITERIA_SURVIVE_WAVES);
			if (progress >= tree->Entry->Amount)
				return;

			TC_LOG_ERROR("scripts", "BFT: les {} vagues sont nettoyees mais la barre est a {}/{}. {} credits ont fuite.",
				HORDE_WAVES_COUNT, progress, tree->Entry->Amount, tree->Entry->Amount - progress);

			// miscValue2 est l'increment applique au criteria : un seul appel
			// suffit pour combler l'ecart.
			scenario->UpdateCriteria(CriteriaType::KillCreature, NPC_WAVE_MEMBER_CREDIT,
				tree->Entry->Amount - progress, 0);
		}

		// Supprime toutes les creatures d'une entry donnee sur la grille
		// (utilise pour nettoyer les credits d'incendie en fin d'etape).
		void MassDespawn(uint32 entry)
		{
			std::list<Creature*> results;
			if (Creature* jaina = GetJaina())
			{
				jaina->GetCreatureListWithEntryInGrid(results, entry, SIZE_OF_GRIDS);
				if (results.empty())
					return;

				for (Creature* c : results)
					c->DespawnOrUnsummon();
			}
		}

		// Retire les figurants Horde cosmetiques une fois la bataille finie.
		void DespawnDummies()
		{
			for (ObjectGuid guid : dummyMembers)
			{
				if (Creature* creature = instance->GetCreature(guid))
					creature->DespawnOrUnsummon();
			}
		}

		// Prepare l'etape HelpTheWounded : une bonne partie des troupes
		// survivantes est remplacee par un clone "blesse" a leur place. Le
		// clone reprend l'apparence et les ressources de l'original, mais avec
		// tres peu de PV et les auras cosmetiques d'agonie ; l'original est
		// simplement masque et reapparaitra au banquet.
		void SpawnWoundedTroops()
		{
			Creature* jaina = GetJaina();
			if (!jaina)
				return;

			for (ObjectGuid guid : troops)
			{
				Creature* troop = ObjectAccessor::GetCreature(*jaina, guid);

				if (!troop || troop->isDead())
					continue;

				if (roll_chance(WOUNDED_SPAWN_CHANCE))
				{
					troop->SetVisible(false);
					if (Creature* wounded = troop->SummonCreature(NPC_THERAMORE_WOUNDED_TROOP,
																  troop->GetPosition(),
																  TempSummonType::TEMPSUMMON_MANUAL_DESPAWN))
					{
						uint32 health = troop->GetMaxHealth();
						Powers power = troop->GetPowerType();

						wounded->SetPowerType(power);
						wounded->SetPower(power, troop->GetPower(power));
						wounded->SetRegenerateHealth(false);
						wounded->SetMaxHealth(health);
						wounded->SetHealth(health * frand(0.15f, 0.20f));
						wounded->SetDisplayId(troop->GetDisplayId());
						wounded->SetImmuneToNPC(true);
						wounded->AddAura(SPELL_COSMETIC_DEATH, wounded);
						wounded->AddAura(SPELL_COSMETIC_FREEZE, wounded);
						wounded->SetVignette(VIGNETTE_ALLIANCE_TROOPS);
					}
				}
			}
		}

		// Bascule de la ville en mode "apres-bataille" : on sort tout le monde
		// du combat, on ferme le portail de la Horde, et on reinstalle les
		// survivants autour de la table de banquet (troupes qui mangent,
		// archimages a leur place, Thader fige, Kinndy en pleurs).
		void RelocateTroops()
		{
			if (Creature* rhonin = GetRhonin())
			{
				SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, rhonin);
				rhonin->SetVignette(VIGNETTE_NONE);
			}

			if (Creature* jaina = GetJaina())
			{
				SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, jaina);
				jaina->SetVignette(VIGNETTE_NONE);
				jaina->CombatStop();
				jaina->SetReactState(REACT_PASSIVE);
				jaina->NearTeleportTo(JainaPoint03);
				jaina->SetHomePosition(JainaPoint03);
				jaina->SetSheath(SHEATH_STATE_UNARMED);
			}

			if (GameObject* portal = GetGameObject(DATA_PORTAL_TO_ORGRIMMAR))
				portal->Delete();

			if (Creature* kalecgos = GetKalecgosDragon())
				kalecgos->SetVisible(false);

			uint8 slot = 0;
			for (ObjectGuid const& guid : troops)
			{
				if (slot >= ARCHMAGES_RELOCATION)
					break;

				Creature* creature = instance->GetCreature(guid);

				if (!creature || creature->isDead())
					continue;

				creature->SetVisible(true);
				creature->NearTeleportTo(UnitLocation[slot]);
				creature->SetHomePosition(UnitLocation[slot]);
				creature->SetSheath(SHEATH_STATE_UNARMED);
				creature->SetStandState(UNIT_STAND_STATE_SIT);
				creature->SetEmoteState(EMOTE_STATE_NONE);
				creature->RemoveAllAuras();
				creature->Dismount();
				creature->AddAura(RAND(SPELL_COSMETIC_EAT_SOUP, SPELL_COSMETIC_DRINK), creature);

				slot++;
			}

			for (uint8 i = 0; i < ARCHMAGES_RELOCATION; i++)
			{
				if (Creature* creature = GetCreature(archmagesRelocation[i].dataId))
				{
					creature->NearTeleportTo(archmagesRelocation[i].destination);
					creature->SetHomePosition(archmagesRelocation[i].destination);
					creature->SetSheath(SHEATH_STATE_UNARMED);
					creature->SetEmoteState(EMOTE_STATE_NONE);
					creature->RemoveAllAuras();

					switch (creature->GetEntry())
					{
						case NPC_ARCHMAGE_TERVOSH:
							creature->CastSpell(creature, SPELL_SHOW_OFF_FIRE);
							break;
						case NPC_THADER_WINDERMERE:
							creature->AddAura(SPELL_STASIS, creature);
							creature->SetStandState(UNIT_STAND_STATE_STAND);
							creature->SetEmoteState(EMOTE_STATE_STUN_NO_SHEATHE);
							break;
						case NPC_TARI_COGG:
							creature->SetStandState(UNIT_STAND_STATE_SIT);
							creature->SetEmoteState(EMOTE_STATE_EAT);
							creature->SummonGameObject(GOB_LAVISH_REFRESHMENT_TABLE, TablePoint01, QuaternionData::fromEulerAnglesZYX(TablePoint01.GetOrientation(), 0.f, 0.f), 0s);
							break;
						case NPC_KINNDY_SPARKSHINE:
							creature->SetEmoteState(EMOTE_STATE_CRY);
							break;
					}
				}
			}

			// Les generaux font le point entre eux, la ou les Ruines exposeront
			// leurs corps. Shandris a deja quitte la ville (absente de la table).
			for (uint8 i = 0; i < GENERALS_AFTERMATH; i++)
			{
				Creature* general = GetCreature(generalsAftermath[i].dataId);
				if (!general)
					continue;

				general->CombatStop(true);
				general->SetReactState(REACT_PASSIVE);
				general->RemoveAllAuras();
				general->SetSheath(SHEATH_STATE_UNARMED);
				general->SetEmoteState(EMOTE_STATE_NONE);
				PlaceGeneral(general, generalsAftermath[i].destination);
			}
		}

		// La flotte de la 7e Legion a accoste : ses generaux sont entres dans
		// la citadelle pendant la scene A Little Help (generalsLocation). Les
		// vrais apparaissent a leur place du briefing, au bout de leur chemin
		// d'arrivee. Ils remplacent les anciens spawns statiques d'Aubrey et de
		// Tiras'alan (supprimes en base).
		void SpawnGenerals()
		{
			for (Location const& location : generalsLocation)
			{
				WaypointNode const& node = location.path->Nodes.back();
				Position const place(node.X, node.Y, node.Z, node.Orientation.value_or(location.position.GetOrientation()));

				Creature* general = instance->SummonCreature(location.dataId, place);
				if (!general)
					continue;

				general->SetSheath(SHEATH_STATE_UNARMED);
				general->SetHomePosition(place);
			}
		}

		// Les deux fantassins de la ville les plus proches de Jaina : ce sont
		// eux qui ligotent et emportent Thalen. Seules les troupes statiques
		// sont candidates, pour ne pas casser une patrouille ou une formation.
		GuidVector SelectEscortFootmen()
		{
			Creature* jaina = GetJaina();
			if (!jaina)
				return {};

			std::vector<Creature*> candidates;
			GetCreatureListWithEntryInGrid(candidates, jaina, NPC_THERAMORE_FOOTMAN, 10.f);

			GuidVector escort;
			for (Creature* troop : candidates)
			{
				if (escort.size() >= 2)
					break;
				escort.push_back(troop->GetGUID());
			}

			return escort;
		}

		// Suit l'escorte de Thalen jusqu'au bout de ThalenEscortPath, une fois
		// les trois departs donnes. Thalen, arrive hors de vue, est place dans
		// sa cellule a la citadelle (c'est la que la Horde viendra le
		// chercher) ; les soldats reprennent leur role de troupe sur place.
		void WatchThalenEscort()
		{
			thalenEscortWalking = thalenEscort;
			if (Creature* thalen = GetThalen())
				thalenEscortWalking.push_back(thalen->GetGUID());

			scheduler.Schedule(ESCORT_STAGGER * 2 + 1s, [this](TaskContext watch)
			{
				std::erase_if(thalenEscortWalking, [this](ObjectGuid const& guid)
				{
					Creature* unit = instance->GetCreature(guid);
					if (!unit)
						return true;

					if (!HasFinishedPath(unit))
						return false;

					if (unit->GetEntry() == NPC_THALEN_SONGWEAVER)
					{
						unit->GetMotionMaster()->Clear();
						unit->GetMotionMaster()->MoveIdle();
						unit->RemoveAllAuras();
						unit->NearTeleportTo(ThalenPoint02);
						unit->SetHomePosition(ThalenPoint02);
						unit->CastSpell(unit, SPELL_ARCANIC_CELL, true);
						unit->SetEmoteState(EMOTE_STATE_STUN_NO_SHEATHE);
					}
					else
					{
						unit->DespawnOrUnsummon();
					}
					return true;
				});

				if (!thalenEscortWalking.empty())
					watch.Repeat(300ms);
			});
		}

		// Un chemin non repetable qui se termine ne libere pas le
		// WaypointMovementGenerator : le core le marque FINALIZED et le laisse
		// en place. Le type courant reste donc WAYPOINT_MOTION_TYPE, c'est le
		// drapeau qui dit si le chemin est fini.
		static bool HasFinishedPath(Creature const* unit)
		{
			MovementGenerator const* movement = unit->GetMotionMaster()->GetCurrentMovementGenerator();
			return !movement
				|| movement->GetMovementGeneratorType() != WAYPOINT_MOTION_TYPE
				|| movement->HasFlag(MOVEMENTGENERATOR_FLAG_FINALIZED);
		}

		// Teleport d'un general, Z recale sur le sol : les points sont releves
		// a la main et la ville a des marches partout.
		void PlaceGeneral(Creature* general, Position pos)
		{
			general->UpdateAllowedPositionZ(pos.GetPositionX(), pos.GetPositionY(), pos.m_positionZ);
			general->GetMotionMaster()->Clear();
			general->GetMotionMaster()->MoveIdle();
			general->NearTeleportTo(pos);
			general->SetHomePosition(pos);
			general->DoNotReacquireSpellFocusTarget();
		}

		// Secousses de camera pendant le siege. Contrairement aux auras de
		// phase, c'est un effet reellement periodique : il doit etre rejoue
		// regulierement, il ne s'agit pas de maintenir un etat.
		void ScheduleCameraShakes()
		{
			scheduler.Schedule(1s, [this](TaskContext shake)
			{
				if (phase >= BFTPhases::Preparation && phase < BFTPhases::HelpTheWounded)
				{
					DoCastSpellOnPlayers(SPELL_CAMERA_SHAKE_VOLCANO);
				}

				shake.Repeat(15s, 30s);
			});
		}

		// Explosions d'ambiance sur la barriere pendant toute la bataille.
		// Taggee (uint32)BFTPhases::TheBattle pour pouvoir etre annulee en bloc
		// quand la barriere cede (signal "BarrierBreaks" de SCENE_BFT_THE_BATTLE).
		void EnsureBarrierHasDamage()
		{
			scheduler.Schedule(1s, (uint32)BFTPhases::TheBattle, [this](TaskContext explosion)
			{
				if (Creature* thalen = GetThalen())
				{
					if (Creature* trigger = thalen->SummonCreature(WORLD_TRIGGER, ExplodingPoint01, TEMPSUMMON_TIMED_DESPAWN, 2s))
						trigger->CastSpell(trigger, SPELL_BIG_EXPLOSION);
					explosion.Repeat(2s, 5s);
				}
			});
		}

		// Force une meteo cote client, ou rend la main a la meteo de zone.
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

		// Cible d'ouverture d'une horde qui vient de spawner : sans ca elle
		// resterait plantee a son point d'arrivee.
		Unit* SelectNearestHostileInRange(Creature* creature) const
		{
			Unit* target = nullptr;
			Trinity::NearestHostileUnitInAggroRangeCheck check(creature, false, true);
			Trinity::UnitSearcher<Trinity::NearestHostileUnitInAggroRangeCheck> searcher(creature, target, check);
			Cell::VisitGridObjects(creature, searcher, MAX_VISIBILITY_DISTANCE);
			return target;
		}

		#pragma endregion
	};

	InstanceScript* GetInstanceScript(InstanceMap* map) const override
	{
		return new scenario_battle_for_theramore_InstanceScript(map);
	}
};

// =========================================================================
// scene_theramore_intro - Survol de Theramore a l'entree de l'instance
// =========================================================================
// Cinematique maison jouee cote client (package 50000, voir
// 2026_10_25_01_hotfixes_theramore_intro_scene.sql). Le serveur ne fait
// qu'immobiliser le joueur : la camera de jeu le retrouve ou la scene l'a pris.
class scene_theramore_intro : public SceneScript
{
	public:
		scene_theramore_intro() : SceneScript("scene_theramore_intro") { }

	void OnSceneStart(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/) override
	{
		player->SetControlled(true, UNIT_STATE_ROOT);
	}

	void OnSceneComplete(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/) override
	{
		Release(player);
	}

	void OnSceneCancel(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/) override
	{
		Release(player);
	}

	void Release(Player* player)
	{
		player->SetControlled(false, UNIT_STATE_ROOT);
	}
};

// =========================================================================
// scene_theramore_explosion - Scene finale (destruction de Theramore)
// =========================================================================
// Cinematique jouee cote client. Le serveur n'intervient que sur trois
// points : il fige le joueur pendant la scene, il fait apparaitre le modele
// de la bombe au moment ou la scene le demande ("DropBombServer"), et il
// teleporte le joueur vers les Ruines de Theramore a la fin - que la scene
// se termine normalement ou qu'elle soit annulee.
class scene_theramore_explosion : public SceneScript
{
	public:
		scene_theramore_explosion() : SceneScript("scene_theramore_explosion") { }

	enum Misc
	{
		MAP_THERAMORE_RUINS     = 5001,
	};

	// Centre de la zone d'arrivee dans les Ruines de Theramore.
	const Position Center = { -3002.74f, -4342.11f, 6.044930f, 3.76716f };

	// Rayon de dispersion des joueurs a l'arrivee.
	const float Distance = 8.f;

	void OnSceneComplete(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/) override
	{
		Finish(player);
	}

	void OnSceneCancel(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/) override
	{
		Finish(player);
	}

	void Finish(Player* player)
	{
		player->TeleportTo(GetRevivePosition(player), TELE_REVIVE_AT_TELEPORT);
	}

	// Point de reapparition apres la scene : tirage uniforme dans un disque de
	// rayon Distance autour de Center. Nom distinct du helper libre
	// GetRandomPosition (CustomAI.h) auquel il delegue, pour qu'on ne confonde
	// pas les deux a la lecture.
	WorldLocation GetRevivePosition(Player const* player)
	{
		Position dest = GetRandomPosition(player, Center, Distance);
		dest.SetOrientation(Center.GetOrientation());
		return { MAP_THERAMORE_RUINS, dest };
	}
};

// =========================================================================
// Registration
// =========================================================================
void AddSC_scenario_battle_for_theramore()
{
	new scenario_battle_for_theramore();
	new scene_theramore_intro();
	new scene_theramore_explosion();
	new CustomScenario::DialogueSceneScript("scene_bft_dialogue");
}
