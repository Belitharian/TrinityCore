/*
 * Ruins of Theramore - InstanceScript principal
 *
 * Gere le flow complet du scenario "Les Ruines de Theramore" :
 *
 *   Phase 1 : FindJaina_Isle           -> les joueurs cherchent Jaina sur l'ilot
 *             FindJaina_Isle_Valided   -> cinematique d'apres bataille (events 1..18)
 *   Phase 2 : FindJaina_Crater         -> retrouvailles au cratere
 *             FindJaina_Crater_Valided -> dialogue de protection de l'iris (events 19..24)
 *   Phase 3 : Standards / Standards_Valided / BackToSender / TheFinalAssault
 *             -> retour a Theramore, combat des hordes (events 25..37)
 *             -> watchdog EVT_HORDE_CHECKER_STANDARDS (44) surveille le nettoyage initial
 *             -> watchdog EVT_HORDE_CHECKER_FINAL (38) surveille la mort des hordes finales
 *   Phase 4 : LeaveTheRuins            -> Jaina ouvre le portail vers Stormwind (events 39..43, 45)
 *
 * L'enchainement entre events est sequentiel (Next() incremente eventId membre
 * et planifie l'event suivant). Les watchdogs (38, 44) auto-replanifient.
 *
 * Commentaires en francais sans accents (encodage TC).
 */

#include "CustomAI.h"
#include "EventMap.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MiscPackets.h"
#include "MotionMaster.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "TemporarySummon.h"
#include "Weather.h"
#include "ruins_of_theramore.h"

// =========================================================================
// Tables de correspondance NPC / GO <-> Data ID
// =========================================================================
const ObjectData creatureData[] =
{
	{ NPC_JAINA_PROUDMOORE,     DATA_JAINA_PROUDMOORE       },
	{ NPC_KALECGOS,             DATA_KALECGOS               },
	{ NPC_KINNDY_SPARKSHINE,    DATA_KINNDY_SPARKSHINE      },
	{ NPC_ROKNAH_WARLORD,       DATA_ROKNAH_WARLORD         },
	{ NPC_BOMBARDING_ZEPPELIN,  DATA_BOMBARDING_ZEPPELIN    },
	{ 0,                        0                           }   // END
};

const ObjectData gameobjectData[] =
{
	{ GOB_BROKEN_GLASS,         DATA_BROKEN_GLASS           },
	{ 0,                        0                           }   // END
};

// =========================================================================
// Identifiants des evenements internes de l'EventMap
// =========================================================================
// L'ordre numerique est SIGNIFICATIF : Next() incremente eventId de 1 et
// planifie ainsi automatiquement l'event suivant dans la sequence.
// Les watchdogs (38, 44) sortent de cette logique (ils s'auto-replanifient).
enum SceneEvent : uint32
{
	// Phase 1 - Trouver Jaina sur l'ilot (cinematique d'apres bataille)
	EVT_ISLE_JAINA_WALK             = 1,    // Jaina marche jusqu'a JainaPoint01
	EVT_ISLE_KALECGOS_GREET         = 2,    // Kalecgos prend Jaina pour cible et la salue
	EVT_ISLE_JAINA_TALK_02          = 3,
	EVT_ISLE_JAINA_TALK_03          = 4,
	EVT_ISLE_KALECGOS_MOVE          = 5,    // Kalecgos s'eloigne lentement
	EVT_ISLE_JAINA_TALK_05          = 6,
	EVT_ISLE_KALECGOS_TALK_06       = 7,
	EVT_ISLE_JAINA_TALK_07          = 8,
	EVT_ISLE_KALECGOS_TALK_08       = 9,
	EVT_ISLE_JAINA_TALK_09          = 10,
	EVT_ISLE_KALECGOS_TALK_10       = 11,
	EVT_ISLE_KALECGOS_TALK_11       = 12,
	EVT_ISLE_JAINA_TALK_12          = 13,
	EVT_ISLE_ECHO_OF_ALUNETH        = 14,   // Spawn du trigger Echo of Aluneth + recul de Jaina
	EVT_ISLE_ALUNETH_FREED          = 15,   // Explosion arcanique + Jaina passe en idle
	EVT_ISLE_JAINA_HIDE             = 16,   // Jaina devient non-interactible
	EVT_ISLE_TELEPORT_PREP          = 17,   // Point d'entree DEBUG (CUSTOM_DEBUG) - prepare le teleport
	EVT_ISLE_TELEPORT_TRIGGER       = 18,   // Teleport groupe + trigger EVENT_HELP_KALECGOS

	// Phase 2 - Cratere
	EVT_CRATER_JAINA_STAND          = 19,   // Jaina se releve
	EVT_CRATER_KINNDY_DISSOLVE      = 20,   // Kinndy lance les visuels arcaniques
	EVT_CRATER_JAINA_TALK_01        = 21,
	EVT_CRATER_JAINA_MOVE           = 22,   // Jaina marche vers JainaPoint03
	EVT_CRATER_JAINA_TALK_02        = 23,
	EVT_CRATER_SPAWN_DUMMY          = 24,   // Spawn du dummy invisible portant les sorts cosmetiques

	// Phase 3 - Back to sender (combat des hordes)
	EVT_BACK_DUMMY_GROW             = 25,   // Le dummy passe a l'echelle finale
	EVT_BACK_JAINA_TALK_05          = 26,
	EVT_BACK_SUMMON_ELEMENTALS      = 27,   // Jaina invoque ses elementaires d'eau
	EVT_BACK_ARCANE_CHANNEL         = 28,   // Channeling + deplacement des elementaires
	EVT_BACK_ZEPPELIN_FLYBY         = 29,   // Passage cosmetique du zeppelin
	EVT_BACK_SPAWN_HORDES           = 30,   // Spawn du groupe de hordes
	EVT_BACK_WARLORD_TALK_06        = 31,
	EVT_BACK_JAINA_TALK_07          = 32,
	EVT_BACK_WARLORD_TALK_08        = 33,
	EVT_BACK_JAINA_TALK_09          = 34,
	EVT_BACK_WARLORD_TALK_10        = 35,
	EVT_BACK_RELEASE_HORDES         = 36,   // Hordes attaquent (warlord -> joueur, autres -> elementaires)
	EVT_BACK_JAINA_IMMUNE           = 37,   // Jaina passe en immune (en attendant la fin)

	// Watchdog : surveille que toutes les hordes (hors warlord) sont mortes
	EVT_HORDE_CHECKER_FINAL         = 38,

	// Phase 4 - Quitter les ruines
	EVT_LEAVE_JAINA_WALK            = 39,   // Jaina marche vers le verre brise
	EVT_LEAVE_JAINA_TALK_01         = 40,
	EVT_LEAVE_JAINA_TALK_02         = 41,
	EVT_LEAVE_OPEN_PORTAL           = 42,   // Spawn du portail vers Stormwind
	EVT_LEAVE_DESPAWN_FINAL         = 43,   // Despawn des elementaires + Jaina disparait

	// Watchdog : surveille la phase de nettoyage initiale (Standards)
	EVT_HORDE_CHECKER_STANDARDS     = 44,

	// Mouvement final de Jaina vers JainaPoint03 (declenche apres watchdog 44)
	EVT_STANDARDS_JAINA_FINAL_MOVE  = 45
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
			eventId(EVT_ISLE_JAINA_WALK), hordeCounter(0),
			phase(RFTPhases::FindJaina_Isle), irisDummy(ObjectGuid::Empty)
		{
			SetHeaders(DataHeader);
			LoadObjectData(creatureData, gameobjectData);
		}

		// =================================================================
		// Etat interne
		// =================================================================
		EventMap events;
		uint32 eventId;                       // Dernier event execute (sert a Next() pour planifier eventId+1)
		uint32 hordeCounter;                  // Nombre total de hordes spawnees pour la phase Standards
		RFTPhases phase;                      // Phase courante du scenario
		ObjectGuid irisDummy;                 // GUID du dummy invisible portant les visuels de l'iris
		GuidVector hordeChecker;              // GUIDs des hordes surveillees par EVT_HORDE_CHECKER_STANDARDS
		std::vector<Creature*> elementals;    // Elementaires d'eau invoques par Jaina
		std::list<TempSummon*> hordes;        // Hordes spawnees pour la phase BackToSender

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
			// La skybox change selon la progression :
			// avant la validation de l'ilot -> entree, sinon -> ruines.
			RFTPhases current = (RFTPhases)GetData(DATA_SCENARIO_PHASE);
			if (current >= RFTPhases::FindJaina_Isle_Valided)
				player->AddAura(SPELL_SKYBOX_EFFECT_RUINS, player);
			else
				player->AddAura(SPELL_SKYBOX_EFFECT_ENTRANCE, player);
		}

		void OnPlayerLeave(Player* player) override
		{
			player->RemoveAurasDueToSpell(SPELL_SKYBOX_EFFECT_ENTRANCE);
			player->RemoveAurasDueToSpell(SPELL_SKYBOX_EFFECT_RUINS);
		}

		void SetData(uint32 dataId, uint32 value) override
		{
			switch (dataId)
			{
				case DATA_SCENARIO_PHASE:
					phase = (RFTPhases)value;
					// Entree dans la phase finale : on planifie immediatement
					// le premier dialogue de sortie.
					if (phase == RFTPhases::LeaveTheRuins)
						events.ScheduleEvent(EVT_LEAVE_JAINA_TALK_01, 1s);
					break;

				case EVENT_FIND_JAINA_02:
					// Declenche par npc_jaina_ruins quand un joueur s'approche au cratere.
					SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::FindJaina_Crater_Valided);
					events.ScheduleEvent(EVT_CRATER_JAINA_STAND, 500ms);
					break;

				case EVENT_BACK_TO_SENDER:
					// Declenche apres MOVEMENT_INFO_POINT_03 de Jaina.
					SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::BackToSender);
					events.ScheduleEvent(EVT_BACK_DUMMY_GROW, 1s);
					break;

				case EVENT_WARLORD_ROKNAH_SLAIN:
					// 
                    events.ScheduleEvent(EVT_HORDE_CHECKER_FINAL, 800ms);
					break;

				default:
					break;
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
					OnCriteriaHelpKalecgos();
					break;
				case CRITERIA_TREE_FIND_JAINA_02:
					if (Creature* jaina = GetJaina())
						SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, jaina);
					SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::Standards);
					break;
				case CRITERIA_TREE_CLEANING:
					OnCriteriaCleaning();
					break;
				case CRITERIA_TREE_BACK_TO_SENDER:
					SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::TheFinalAssault);
					break;
				case CRITERIA_TREE_THE_LAST_STAND:
					events.ScheduleEvent(EVT_LEAVE_JAINA_TALK_01, 1s);
					break;
                case CRITERIA_TREE_JAINA_PROTECTED:
                    events.ScheduleEvent(EVT_LEAVE_JAINA_WALK, 1s);
                    break;
				default:
					break;
			}
		}

		// Critere "Trouver Jaina sur l'ilot" : on spawn Kalecgos et on lance le dialogue.
		void OnCriteriaFindJainaIsle()
		{
			instance->SummonCreature(NPC_KALECGOS, KalecgosPoint01);
			if (Creature* kalecgos = GetKalecgos())
			{
				kalecgos->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
				kalecgos->GetMotionMaster()->MovePath(KalecgosPath01, false);
			}
			SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::FindJaina_Isle_Valided);

			// En DEBUG on saute directement a la fin de la cinematique (event 17)
			// pour gagner du temps lors des tests.
			#ifdef CUSTOM_DEBUG
				events.ScheduleEvent(EVT_ISLE_TELEPORT_PREP, 1s);
			#else
				events.ScheduleEvent(EVT_ISLE_JAINA_WALK, 1s);
			#endif
		}

		// Critere "Aider Kalecgos" : Jaina passe en interactible + agenouillee.
		void OnCriteriaHelpKalecgos()
		{
			if (Creature* jaina = GetJaina())
			{
				jaina->LoadEquipment(2);
				jaina->RemoveUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
				jaina->SetStandState(UNIT_STAND_STATE_KNEEL);
				jaina->RemoveAllAuras();
				jaina->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);

				// Elargit la portee de detection pour le declencheur du cratere
				// (les joueurs doivent pouvoir l'apercevoir de loin).
				jaina->AI()->SetData(DATA_SET_DISTANCE, (uint32)JAINA_TRIGGER_DISTANCE_CRATER);
			}
			SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::FindJaina_Crater);
		}

		// Critere "Nettoyage" : spawn du groupe de hordes a nettoyer + repositionne Jaina.
		void OnCriteriaCleaning()
		{
			std::list<TempSummon*> spawned;
			instance->SummonCreatureGroup(0, &spawned);
			hordeCounter = (uint32)spawned.size();

			for (TempSummon* horde : spawned)
			{
				horde->SetTempSummonType(TEMPSUMMON_TIMED_OR_DEAD_DESPAWN);
				hordeChecker.push_back(horde->GetGUID());
			}

			if (Creature* jaina = GetJaina())
			{
				Talk(jaina, SAY_IRIS_PROTECTION_JAINA_03);
                jaina->SetVignette(VIGNETTE_JAINA_PROUDMOORE);
                jaina->RemoveAurasDueToSpell(SPELL_ALUNETH_DRINKS);
				jaina->SetHomePosition(JainaPoint04);
				jaina->NearTeleportTo(JainaPoint04);
			}
			SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::Standards_Valided);
			events.ScheduleEvent(EVT_HORDE_CHECKER_STANDARDS, 2s);
		}

		// =================================================================
		// Initialisation des creatures / gameobjects
		// =================================================================
		void OnCreatureCreate(Creature* creature) override
		{
			InstanceScript::OnCreatureCreate(creature);

			creature->SetVisibilityDistanceOverride(VisibilityDistanceType::Large);
			creature->SetPvpFlag(UNIT_BYTE2_FLAG_PVP);
			creature->SetUnitFlag(UNIT_FLAG_PVP_ENABLING);
			creature->SetBoundingRadius(CREATURE_BOUNDING_RADIUS);

			switch (creature->GetEntry())
			{
                case NPC_JAINA_PROUDMOORE:
                    creature->SetVignette(VIGNETTE_JAINA_PROUDMOORE);
                    break;
				case NPC_WATER_ELEMENTAL:
					elementals.push_back(creature);
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
				case NPC_ARCHMAGE_TERVOSH:
				case NPC_KINNDY_SPARKSHINE:
					// Cadavres scenarises : faux-mort + non-interactible + auras cosmetiques.
					FeignDeath(creature);
					creature->SetUninteractible(true);
					creature->AddAura(SPELL_SHIMMERDUST, creature);
					creature->AddAura(SPELL_COSMETIC_PURPLE_VERTEX_STATE, creature);
					creature->AddAura(SPELL_ARCANE_DEBUFF_VISUAL, creature);
					break;
				default:
					break;
			}
		}

		void OnGameObjectCreate(GameObject* go) override
		{
			InstanceScript::OnGameObjectCreate(go);

			if (go->GetEntry() == GOB_BROKEN_GLASS)
				go->SetFlag(GO_FLAG_NOT_SELECTABLE);
		}

		// =================================================================
		// Dispatcher principal : delegue chaque event a son handler
		// =================================================================
		void Update(uint32 diff) override
		{
			events.Update(diff);

			// eventId est conserve en membre car Next() le re-incremente
			// pour planifier l'event suivant dans la sequence.
			switch (eventId = events.ExecuteEvent())
			{
				#pragma region FIND_JAINA_ISLE
				case EVT_ISLE_JAINA_WALK:       HandleIsleJainaWalk();        break;
				case EVT_ISLE_KALECGOS_GREET:   HandleIsleKalecgosGreet();    break;
				case EVT_ISLE_JAINA_TALK_02:    TalkAndNext(GetJaina(),    SAY_AFTER_BATTLE_JAINA_02,    6s);  break;
				case EVT_ISLE_JAINA_TALK_03:    TalkAndNext(GetJaina(),    SAY_AFTER_BATTLE_JAINA_03,    10s); break;
				case EVT_ISLE_KALECGOS_MOVE:    HandleIsleKalecgosMove();     break;
				case EVT_ISLE_JAINA_TALK_05:    TalkAndNext(GetJaina(),    SAY_AFTER_BATTLE_JAINA_05,    5s);  break;
				case EVT_ISLE_KALECGOS_TALK_06: TalkAndNext(GetKalecgos(), SAY_AFTER_BATTLE_KALECGOS_06, 4s);  break;
				case EVT_ISLE_JAINA_TALK_07:    TalkAndNext(GetJaina(),    SAY_AFTER_BATTLE_JAINA_07,    6s);  break;
				case EVT_ISLE_KALECGOS_TALK_08: TalkAndNext(GetKalecgos(), SAY_AFTER_BATTLE_KALECGOS_08, 4s);  break;
				case EVT_ISLE_JAINA_TALK_09:    TalkAndNext(GetJaina(),    SAY_AFTER_BATTLE_JAINA_09,    4s);  break;
				case EVT_ISLE_KALECGOS_TALK_10: TalkAndNext(GetKalecgos(), SAY_AFTER_BATTLE_KALECGOS_10, 6s);  break;
				case EVT_ISLE_KALECGOS_TALK_11: TalkAndNext(GetKalecgos(), SAY_AFTER_BATTLE_KALECGOS_11, 7s);  break;
				case EVT_ISLE_JAINA_TALK_12:    TalkAndNext(GetJaina(),    SAY_AFTER_BATTLE_JAINA_12,    2s);  break;
				case EVT_ISLE_ECHO_OF_ALUNETH:  HandleIsleEchoOfAluneth();    break;
				case EVT_ISLE_ALUNETH_FREED:    HandleIsleAlunethFreed();     break;
				case EVT_ISLE_JAINA_HIDE:       HandleIsleJainaHide();        break;
				case EVT_ISLE_TELEPORT_PREP:    HandleIsleTeleportPrep();     break;
				case EVT_ISLE_TELEPORT_TRIGGER: HandleIsleTeleportTrigger();  break;
				#pragma endregion

				#pragma region FIND_JAINA_CRATER
				case EVT_CRATER_JAINA_STAND:     HandleCraterJainaStand();     break;
				case EVT_CRATER_KINNDY_DISSOLVE: HandleCraterKinndyDissolve(); break;
				case EVT_CRATER_JAINA_TALK_01:   TalkAndNext(GetJaina(), SAY_IRIS_PROTECTION_JAINA_01, 4s); break;
				case EVT_CRATER_JAINA_MOVE:      HandleCraterJainaMove();      break;
				case EVT_CRATER_JAINA_TALK_02:   TalkAndNext(GetJaina(), SAY_IRIS_PROTECTION_JAINA_02, 6s); break;
				case EVT_CRATER_SPAWN_DUMMY:     HandleCraterSpawnDummy();     break;
				#pragma endregion

				#pragma region BACK_TO_SENDER
				case EVT_BACK_DUMMY_GROW:        HandleBackDummyGrow();        break;
				case EVT_BACK_JAINA_TALK_05:     HandleBackJainaTalk05();      break;
				case EVT_BACK_SUMMON_ELEMENTALS: HandleBackSummonElementals(); break;
				case EVT_BACK_ARCANE_CHANNEL:    HandleBackArcaneChannel();    break;
				case EVT_BACK_ZEPPELIN_FLYBY:    HandleBackZeppelinFlyby();    break;
				case EVT_BACK_SPAWN_HORDES:      HandleBackSpawnHordes();      break;
				case EVT_BACK_WARLORD_TALK_06:   TalkAndNext(GetWarlord(), SAY_IRIS_PROTECTION_JAINA_06, 6s); break;
				case EVT_BACK_JAINA_TALK_07:     TalkAndNext(GetJaina(),   SAY_IRIS_PROTECTION_JAINA_07, 8s); break;
				case EVT_BACK_WARLORD_TALK_08:   TalkAndNext(GetWarlord(), SAY_IRIS_PROTECTION_JAINA_08, 6s); break;
				case EVT_BACK_JAINA_TALK_09:     TalkAndNext(GetJaina(),   SAY_IRIS_PROTECTION_JAINA_09, 9s); break;
				case EVT_BACK_WARLORD_TALK_10:   TalkAndNext(GetWarlord(), SAY_IRIS_PROTECTION_JAINA_10, 2s); break;
				case EVT_BACK_RELEASE_HORDES:    HandleBackReleaseHordes();    break;
				case EVT_BACK_JAINA_IMMUNE:      HandleBackJainaImmune();      break;
				case EVT_HORDE_CHECKER_FINAL:    HandleHordeCheckerFinal();    break;
				#pragma endregion

				#pragma region LEAVE_THE_RUINS
				case EVT_LEAVE_JAINA_WALK:       HandleLeaveJainaWalk();       break;
				case EVT_LEAVE_JAINA_TALK_01:    HandleLeaveJainaTalk01();     break;
				case EVT_LEAVE_JAINA_TALK_02:    HandleLeaveJainaTalk02();     break;
				case EVT_LEAVE_OPEN_PORTAL:      HandleLeaveOpenPortal();      break;
				case EVT_LEAVE_DESPAWN_FINAL:    HandleLeaveDespawnFinal();    break;
				#pragma endregion

				#pragma region HORDE_CHECKER_STANDARDS
				case EVT_HORDE_CHECKER_STANDARDS:    HandleHordeCheckerStandards(); break;
				case EVT_STANDARDS_JAINA_FINAL_MOVE: HandleStandardsJainaFinalMove(); break;
				#pragma endregion

				default:
					break;
			}
		}

		// =================================================================
		// Phase 1 : Trouver Jaina sur l'ilot
		// =================================================================

		// Jaina marche jusqu'a sa position de dialogue.
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

		// Kalecgos arrive, prend Jaina pour cible et la salue.
		void HandleIsleKalecgosGreet()
		{
			Creature* jaina = GetJaina();
			Creature* kalecgos = GetKalecgos();
			if (jaina && kalecgos)
			{
				Talk(kalecgos, SAY_AFTER_BATTLE_KALECGOS_01);
				kalecgos->SetWalk(true);
				kalecgos->SetTarget(jaina->GetGUID());
				jaina->SetTarget(kalecgos->GetGUID());
			}
			Next(2s);
		}

		// Kalecgos s'eloigne lentement pour rejoindre sa marque finale.
		void HandleIsleKalecgosMove()
		{
			if (Creature* kalecgos = GetKalecgos())
			{
				Talk(kalecgos, SAY_AFTER_BATTLE_KALECGOS_04);
				// Vitesse reduite : synchronisation avec les dialogues suivants.
				kalecgos->SetSpeedRate(MOVE_WALK, KALECGOS_WALK_SPEED_RATE);
				kalecgos->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, KalecgosPoint02, true, KalecgosPoint02.GetOrientation());
			}
			Next(8s);
		}

		// Echo of Aluneth : trigger temporaire qui cast le sort d'apparition de l'iris.
		// Jaina recule pour laisser place a l'effet visuel.
		void HandleIsleEchoOfAluneth()
		{
			Creature* jaina = GetJaina();
			Creature* kalecgos = GetKalecgos();
			if (jaina)
			{
				if (TempSummon* trigger = instance->SummonCreature(WORLD_TRIGGER, jaina->GetPosition(), nullptr, 10s))
					trigger->CastSpell(trigger, SPELL_ECHO_OF_ALUNETH_SPAWN, true);

				if (kalecgos)
					jaina->GetMotionMaster()->MoveBackward(MOVEMENT_INFO_POINT_NONE, JainaPointBack, kalecgos, JAINA_KNEEL_APPROACH_DIST);
			}
			Next(6s);
		}

		// Explosion de l'iris liberee : Jaina perd ses cibles et passe en idle.
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

		// Jaina disparait visuellement (en attendant le teleport).
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

		// Prepare le teleport vers le cratere : repositionne Jaina + Kalecgos parle.
		void HandleIsleTeleportPrep()
		{
			if (Creature* jaina = GetJaina())
			{
				jaina->NearTeleportTo(JainaPoint02);
				jaina->SetHomePosition(JainaPoint02);
			}
			if (Creature* kalecgos = GetKalecgos())
			{
				Talk(kalecgos, SAY_AFTER_BATTLE_KALECGOS_13);
				kalecgos->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
				FaceFirstPlayer(kalecgos);
			}
			Next(5s);
		}

		// Teleporte tous les joueurs vers Theramore et change la skybox.
		void HandleIsleTeleportTrigger()
		{
			ForceWeather(WEATHER_ARCANE_BUILD, true);
			TeleportPlayers(PlayerPoint01, TELEPORT_SPREAD_RADIUS);
			DoRemoveAurasDueToSpellOnPlayers(SPELL_SKYBOX_EFFECT_ENTRANCE);
			DoCastSpellOnPlayers(SPELL_SKYBOX_EFFECT_RUINS);
			TriggerGameEvent(EVENT_HELP_KALECGOS);
		}

		// =================================================================
		// Phase 2 : Cratere
		// =================================================================

		void HandleCraterJainaStand()
		{
			if (Creature* jaina = GetJaina())
				jaina->SetStandState(UNIT_STAND_STATE_STAND);
			Next(1800ms);
		}

		// Jaina regarde le joueur, Kinndy emet ses visuels d'arcane.
		void HandleCraterKinndyDissolve()
		{
			if (Creature* jaina = GetJaina())
				FaceFirstPlayer(jaina);

			if (Creature* kinndy = GetCreature(DATA_KINNDY_SPARKSHINE))
			{
				kinndy->AddAura(SPELL_COSMETIC_ARCANE_DISSOLVE, kinndy);
				kinndy->CastSpell(kinndy, SPELL_DISSOLVE_ARCANE_VISUAL);
			}
			Next(2s);
		}

		// Jaina marche vers le cratere et Kinndy perd ses auras cosmetiques.
		void HandleCraterJainaMove()
		{
			if (Creature* jaina = GetJaina())
			{
				jaina->SetWalk(true);
				jaina->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, JainaPoint03, true, JainaPoint03.GetOrientation());
				jaina->SetHomePosition(JainaPoint03);
			}
			if (Creature* kinndy = GetCreature(DATA_KINNDY_SPARKSHINE))
			{
				kinndy->RemoveAurasDueToSpell(SPELL_COSMETIC_PURPLE_VERTEX_STATE);
				kinndy->RemoveAurasDueToSpell(SPELL_SHIMMERDUST);
			}
			Next(8s);
		}

		// Spawn du dummy invisible portant les sorts cosmetiques de l'iris.
		void HandleCraterSpawnDummy()
		{
			TriggerGameEvent(EVENT_FIND_JAINA_02);

			Creature* jaina = GetJaina();
			if (Creature* dummy = instance->SummonCreature(WORLD_TRIGGER, DummyPoint01))
			{
				dummy->SetObjectScale(DUMMY_SCALE_SMALL);
				if (jaina)
					dummy->CastSpell(jaina, SPELL_ALUNETH_DRINKS);
				dummy->CastSpell(dummy, SPELL_EMPOWERED_SUMMON, true);
				irisDummy = dummy->GetGUID();
			}
		}

		// =================================================================
		// Phase 3 : Back to sender (combat des hordes)
		// =================================================================

		// Le dummy explose visuellement (echelle 5x).
		void HandleBackDummyGrow()
		{
			if (Creature* dummy = instance->GetCreature(irisDummy))
			{
				dummy->RemoveAllAuras();
				dummy->SetObjectScale(DUMMY_SCALE_LARGE);
				dummy->AddAura(SPELL_COSMETIC_ARCANE_ENERGY, dummy);
			}
			Next(2s);
		}

		void HandleBackJainaTalk05()
		{
			if (Creature* jaina = GetJaina())
			{
				Talk(jaina, SAY_IRIS_PROTECTION_JAINA_05);
                jaina->SetVignette(VIGNETTE_NONE);
				jaina->SetFacingTo(JainaPoint03.GetOrientation());
			}
			Next(6s);
		}

		// Jaina invoque ses elementaires d'eau et passe en back-to-sender.
		void HandleBackSummonElementals()
		{
			if (Creature* jaina = GetJaina())
				jaina->CastSpell(jaina, SPELL_SUMMON_WATER_ELEMENTALS);
			TriggerGameEvent(EVENT_BACK_TO_SENDER);
			Next(2s);
		}

		// Jaina entre en channeling, les elementaires se mettent en position.
		void HandleBackArcaneChannel()
		{
			Creature* jaina = GetJaina();
			if (!jaina)
			{
				Next(5s);
				return;
			}

            jaina->SetImmuneToAll(true);
			jaina->CastSpell(jaina, SPELL_ARCANE_CHANNELING);

			// Bug-fix : verifier qu'on a bien les deux elementaires avant d'iterer.
			if (elementals.size() < ELEMENTALS_SIZE)
			{
				Next(5s);
				return;
			}

			for (uint8 i = 0; i < ELEMENTALS_SIZE; ++i)
			{
				Creature* elem = elementals[i];
				if (!elem)
					continue;
				elem->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, ElementalsPoint[i].destination, true, ElementalsPoint[i].destination.GetOrientation());
				elem->SetHomePosition(ElementalsPoint[i].destination);
				elem->SetBoundingRadius(ELEMENTAL_BOUNDING_RADIUS);
			}
			Next(5s);
		}

		// Passage cosmetique du zeppelin de bombardement.
		void HandleBackZeppelinFlyby()
		{
			if (TempSummon* zeppelin = instance->SummonCreature(NPC_BOMBARDING_ZEPPELIN, ZeppelinPoint.spawn, nullptr, 13s))
			{
				zeppelin->SetSpeedRate(MOVE_RUN, ZEPPELIN_SPEED_RATE);
				zeppelin->PlayDirectSound(SOUND_ZEPPELIN_FLIGHT);
				zeppelin->GetMotionMaster()->MoveBackward(MOVEMENT_INFO_POINT_NONE, ZeppelinPoint.destination, nullptr, 30.5f,
                    MovementWalkRunSpeedSelectionMode::ForceRun);
			}
			Next(3s);
		}

		// Spawn de la vague de hordes attaquant l'iris.
		void HandleBackSpawnHordes()
		{
			hordes.clear();
			instance->SummonCreatureGroup(1, &hordes);
			for (TempSummon* horde : hordes)
			{
				horde->SetTempSummonType(TEMPSUMMON_TIMED_OR_DEAD_DESPAWN);
				horde->SetImmuneToAll(true);
				horde->CastSpell(horde, SPELL_THALYSSRA_SPAWNS);
			}
			Next(4s);
		}

		// Libere les hordes : le warlord cible un joueur, les autres ciblent
		// l'elementaire correspondant a leur position Y.
		void HandleBackReleaseHordes()
		{
			Player* firstPlayer = GetFirstPlayer();

			// Bug-fix : on s'assure que les elementaires existent avant le split.
			const bool elementalsReady = (elementals.size() >= ELEMENTALS_SIZE && elementals[0] && elementals[1]);

			for (Creature* horde : hordes)
			{
				if (!horde)
					continue;

				horde->SetImmuneToAll(false);
                horde->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, firstPlayer->GetRandomNearPosition(10.f));

				if (horde->GetEntry() == NPC_ROKNAH_WARLORD)
				{
                    horde->SetVignette(VIGNETTE_HORDE_WARLORD);
                    if (firstPlayer)
						horde->Attack(firstPlayer, true);
				}
				else if (elementalsReady)
				{
					// Split Y : les hordes au sud du seuil attaquent l'elementaire 1,
					// les autres l'elementaire 0.
					Creature* target = (horde->GetPositionY() <= HORDE_SPLIT_Y_THRESHOLD) ? elementals[1] : elementals[0];
					horde->Attack(target, true);
                    horde->SetVignette(VIGNETTE_HORDE_TROOPS);
                }
			}
			Next(3s);
		}

		// Jaina passe en immune en attendant la fin du combat.
		void HandleBackJainaImmune()
		{
            // DELETED
			Next(0s);
		}

		// Watchdog : surveille la mort des hordes apres EXPLOSIVE_BRAND.
		// Replanifie tant qu'au moins un membre est encore vivant.
		void HandleHordeCheckerFinal()
		{
			uint32 aliveCount = 0;
			if (!CountAliveHordes(aliveCount))
			{
				TriggerGameEvent(EVENT_JAINA_PROTECTED);
				events.CancelEvent(EVT_HORDE_CHECKER_FINAL);
			}
			else
			{
				events.RescheduleEvent(EVT_HORDE_CHECKER_FINAL, 1s);
			}
		}

		// =================================================================
		// Phase 4 : Quitter les ruines
		// =================================================================

		// Jaina marche vers le verre brise (les hordes survivantes sont tuees).
		void HandleLeaveJainaWalk()
		{
			Creature* jaina = GetJaina();
			if (!jaina)
				return;

			if (Creature* dummy = instance->GetCreature(irisDummy))
				dummy->DespawnOrUnsummon();

			if (GameObject* brokenGlass = GetGameObject(DATA_BROKEN_GLASS))
			{
                jaina->SetWalk(true);

				if (TempSummon* trigger = instance->SummonCreature(WORLD_TRIGGER, brokenGlass->GetPosition()))
					jaina->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_01, trigger, JAINA_BROKEN_GLASS_APPROACH);
			}
		}

		void HandleLeaveJainaTalk01()
		{
			if (Creature* jaina = GetJaina())
			{
				Talk(jaina, SAY_LEAVE_THE_RUINS_JAINA_01);
				FaceFirstPlayer(jaina);
			}
			Next(9s);
		}

		void HandleLeaveJainaTalk02()
		{
			if (Creature* jaina = GetJaina())
			{
				Talk(jaina, SAY_LEAVE_THE_RUINS_JAINA_02);
				FaceFirstPlayer(jaina);
			}
			Next(10s);
		}

		// Spawn et activation du portail vers Stormwind.
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
				}
			}
			Next(1s);
		}

		// Despawn final : elementaires + Jaina disparait.
		void HandleLeaveDespawnFinal()
		{
			if (Creature* jaina = GetJaina())
			{
				for (Creature* elemental : elementals)
				{
					if (elemental)
						elemental->DespawnOrUnsummon(1s);
				}

				SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, jaina);

				jaina->CastSpell(jaina, SPELL_COSMETIC_ARCANE_DISSOLVE);
				jaina->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
			}
		}

		// =================================================================
		// Watchdog Standards : surveille le nettoyage initial des hordes
		// =================================================================

		// Surveille les hordes de la phase Standards. Chaque horde encore vivante
		// est forcee a engager Jaina. Quand toutes sont mortes : transition vers
		// la fin de combat de Jaina + mouvement final.
		void HandleHordeCheckerStandards()
		{
			Creature* jaina = GetJaina();
			if (!jaina)
			{
				events.CancelEvent(EVT_HORDE_CHECKER_STANDARDS);
				return;
			}

			uint32 deadCount = 0;
			for (uint8 i = 0; i < hordeCounter; ++i)
			{
				Creature* horde = ObjectAccessor::GetCreature(*jaina, hordeChecker[i]);

				if (!horde || horde->isDead())
				{
					++deadCount;
					continue;
				}

				if (!horde->IsEngaged())
					horde->Attack(jaina, true);
			}

			// Tous les membres de la Horde sont morts -> Jaina termine son combat.
			if (deadCount >= hordeCounter)
			{
				Talk(jaina, SAY_IRIS_PROTECTION_JAINA_04);
				FaceFirstPlayer(jaina);

				jaina->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
				jaina->SetReactState(REACT_PASSIVE);
				jaina->LoadEquipment(2);
				jaina->AI()->SetData(DATA_CANCEL_GROUP, DATA_PHASE_COMBAT);

				events.CancelEvent(EVT_HORDE_CHECKER_STANDARDS);
				Next(5s);
			}
			else
			{
				events.RescheduleEvent(EVT_HORDE_CHECKER_STANDARDS, 1s);
			}
		}

		// Jaina termine la phase Standards en marchant vers JainaPoint03.
		void HandleStandardsJainaFinalMove()
		{
			if (Creature* jaina = GetJaina())
			{
				jaina->SetWalk(false);
				jaina->SetHomePosition(JainaPoint03);
				jaina->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_03, JainaPoint03, true, JainaPoint03.GetOrientation());
			}
		}

		// =================================================================
		// Accesseurs raccourcis
		// =================================================================
		Creature* GetJaina()    { return GetCreature(DATA_JAINA_PROUDMOORE); }
		Creature* GetKalecgos() { return GetCreature(DATA_KALECGOS); }
		Creature* GetKinndy()   { return GetCreature(DATA_KINNDY_SPARKSHINE); }
		Creature* GetWarlord()  { return GetCreature(DATA_ROKNAH_WARLORD); }
		Creature* GetZeppelin() { return GetCreature(DATA_BOMBARDING_ZEPPELIN); }

		// =================================================================
		// Helpers internes
		// =================================================================

		// Talk null-safe : utilise par les handlers de dialogue sequentiels.
		void Talk(Creature* creature, uint8 id)
		{
			if (creature)
				creature->AI()->Talk(id);
		}

		// Helper combinant Talk + planification de l'event suivant.
		// Permet d'ecrire les sequences de dialogue en une ligne par event.
		void TalkAndNext(Creature* creature, uint8 textId, Milliseconds time)
		{
			Talk(creature, textId);
			Next(time);
		}

		// Incremente eventId membre et planifie l'event suivant dans la sequence.
		// ATTENTION : depend de l'ordre numerique des SceneEvent - ne pas modifier
		// les valeurs sans repenser le flow.
		void Next(const Milliseconds& time)
		{
			eventId++;
			events.ScheduleEvent(eventId, time);
		}

		// Retourne le premier joueur encore en jeu dans l'instance, ou nullptr.
		// Centralise le pattern instance->GetPlayers().begin()->GetSource()
		// qui n'etait pas null-safe partout dans l'ancien code.
		Player* GetFirstPlayer() const
		{
			auto const& playerList = instance->GetPlayers();
			if (playerList.empty())
				return nullptr;
			return playerList.begin()->GetSource();
		}

		// Oriente une creature vers le premier joueur disponible.
		void FaceFirstPlayer(Creature* creature)
		{
			if (!creature)
				return;
			if (Player* player = GetFirstPlayer())
				creature->SetFacingToObject(player);
		}

		// Compte les hordes encore vivantes (hors warlord). Retourne true
		// s'il en reste, false si toutes sont mortes.
		bool CountAliveHordes(uint32& aliveCount) const
		{
			aliveCount = 0;
			for (Creature* horde : hordes)
			{
				if (!horde || horde->GetEntry() == NPC_ROKNAH_WARLORD)
					continue;
				if (horde->IsAlive())
					++aliveCount;
			}
			return aliveCount > 0;
		}

		// Teleporte tous les joueurs aleatoirement autour d'un centre.
		// Le calcul de new_dist suit une distribution radiale "triangulaire"
		// (somme de deux uniformes) pour repartir les joueurs sans accumulation
		// au centre ni sur le bord exact du cercle.
		void TeleportPlayers(const Position center, float distance)
		{
			float angle = (float)rand_norm() * static_cast<float>(2 * M_PI);
			float new_dist = (float)rand_norm() + (float)rand_norm();
			new_dist = distance * (new_dist > 1 ? new_dist - 2 : new_dist);

			float rand_x = center.m_positionX + new_dist * std::cos(angle);
			float rand_y = center.m_positionY + new_dist * std::sin(angle);

			Trinity::NormalizeMapCoord(rand_x);
			Trinity::NormalizeMapCoord(rand_y);

			instance->DoOnPlayers([center, rand_x, rand_y](Player* player)
			{
				float rand_z = center.m_positionZ;
				player->UpdateGroundPositionZ(rand_x, rand_y, rand_z);
				player->NearTeleportTo({ rand_x, rand_y, rand_z, center.GetOrientation() });
			});
		}

		// Force un climat specifique cote client pour chaque joueur de l'instance.
		// On envoie le paquet directement (et non SetZoneWeather) parce que la zone
		// ne change pas : seul l'override visuel par joueur est necessaire.
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
