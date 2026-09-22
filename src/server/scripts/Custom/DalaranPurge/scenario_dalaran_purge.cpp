#include "CreatureGroups.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "KillRewarder.h"
#include "Map.h"
#include "Group.h"
#include "GroupMgr.h"
#include "MotionMaster.h"
#include "Player.h"
#include "TemporarySummon.h"
#include "CustomAI.h"
#include "dalaran_purge.h"
#include "../CustomScenario.h"

const ObjectData creatureData[] =
{
	{ NPC_JAINA_PROUDMOORE,             DATA_JAINA_PROUDMOORE           },
	{ NPC_JAINA_PROUDMOORE_PATROL,      DATA_JAINA_PROUDMOORE_PATROL    },
	{ NPC_AETHAS_SUNREAVER,             DATA_AETHAS_SUNREAVER           },
	{ NPC_VEREESA_WINDRUNNER,           DATA_VEREESA_WINDRUNNER         },
	{ NPC_SUMMONED_WATER_ELEMENTAL,     DATA_SUMMONED_WATER_ELEMENTAL   },
	{ NPC_BOUND_WATER_ELEMENTAL,        DATA_BOUND_WATER_ELEMENTAL      },
	{ NPC_SORIN_MAGEHAND,               DATA_SORIN_MAGEHAND             },
	{ NPC_MAGE_COMMANDER_ZUROS,         DATA_MAGE_COMMANDER_ZUROS       },
	{ NPC_CAPTAIN_ANTHEAS,              DATA_CAPTAIN_ANTHEAS            },
	{ NPC_ARCHMAGE_LANDALOCK,           DATA_ARCHMAGE_LANDALOCK         },
	{ NPC_MAGISTER_HATHOREL,            DATA_MAGISTER_HATHOREL          },
	{ NPC_MAGISTER_SURDIEL,             DATA_MAGISTER_SURDIEL           },
	{ NPC_ARCANIST_RATHAELLA,           DATA_ARCANIST_RATHAELLA         },
	{ NPC_HIGH_ARCANIST_SAVOR,          DATA_HIGH_ARCANIST_SAVOR        },
	{ NPC_GRAND_MAGISTER_ROMMATH,       DATA_GRAND_MAGISTER_ROMMATH     },
	{ NPC_NARASI_SNOWDAWN,              DATA_NARASI_SNOWDAWN            },
	{ NPC_MAGISTER_BRASAEL,             DATA_MAGISTER_BRASAEL           },
	{ NPC_MAGISTRIX_VESARA,             DATA_MAGISTRIX_VESARA           },
	{ NPC_GALENDROR_WHITEWING,          DATA_GALENDROR_WHITEWING        },
	{ 0,                                0                               }   // END
};

const ObjectData gameobjectData[] =
{
	{ GOB_SECRET_PASSAGE,               DATA_SECRET_PASSAGE             },
	{ GOB_PORTAL_TO_PRISON,             DATA_PORTAL_TO_PRISON           },
	{ GOB_PORTAL_TO_SEWERS,             DATA_PORTAL_TO_SEWERS           },
	{ 0,                                0                               }   // END
};

enum PhasesShift
{
    PHASESHIFT_HIDE = 52,
};

// =========================================================================
// Identifiants des evenements internes de l'EventMap
// =========================================================================
// L'ordre numerique est SIGNIFICATIF : Next() incremente eventId de 1 et
// planifie ainsi automatiquement l'event suivant dans la sequence. Ne pas
// reordonner sans verifier les chaines de Next().
// Les watchdogs (EVT_ESCORT_PORTAL_CHECKER, EVT_PRISON_NARASI_CHECKER)
// sortent de cette logique : ils s'auto-replanifient jusqu'a validation.
enum DLPEvents : uint32
{
    // Dalaran : la purge vue en flashback
    EVT_DALARAN_PURGE_CONVERSATION = 1,     // Conversation CONVERSATION_DALARAN_PURGE
    EVT_DALARAN_HIGHMAGES_ASSAULT,          // Les hauts-mages attaquent Jaina
    EVT_DALARAN_JAINA_FROST_NOVA,           // Riposte de Jaina
    EVT_DALARAN_HIGHMAGES_DOWN,             // Les hauts-mages tombent (feign death)
    EVT_DALARAN_JAINA_APPROACH_AETHAS,      // Jaina marche vers Aethas
    EVT_DALARAN_AETHAS_TELEPORT,            // Aethas et Jaina se teleportent
    EVT_DALARAN_ELEMENTAL_MOVE,             // Elementaire en place + EVENT_ASSIST_JAINA

    // First step
    EVT_FIRST_STEP_FACE_EACH_OTHER,         // Jaina et Vereesa se font face
    EVT_FIRST_STEP_VEREESA_TALK_01,
    EVT_FIRST_STEP_JAINA_TALK_02,
    EVT_FIRST_STEP_JAINA_TALK_03,
    EVT_FIRST_STEP_VEREESA_TALK_04,
    EVT_FIRST_STEP_TRIGGER_NEXT,            // EVENT_FIND_JAINA_02

    // Phase 3 - The Arcanist Teleport
    EVT_THE_ARCANIST_TELEPORT_01,
    EVT_THE_ARCANIST_TELEPORT_02,
    EVT_THE_ARCANIST_TELEPORT_03,
    EVT_THE_ARCANIST_TELEPORT_04,

    // What happened! : descente dans les egouts
    EVT_SEWERS_TELEPORT_PLAYERS,            // Teleport du groupe + Surdiel en position
    EVT_SEWERS_HORDE_ILLUSION,              // Illusion horde + override de faction

    // What happened! : escorte de Rommath
    EVT_ESCORT_ROMMATH_TALK_01,
    EVT_ESCORT_ROMMATH_TALK_02,
    EVT_ESCORT_ROMMATH_PATH,                // Rommath suit RommathPath01
    EVT_ESCORT_ROMMATH_TALK_03,
    EVT_ESCORT_PORTAL_CHECKER,              // Watchdog : Rommath a portee du portail ?

    // What happened! : la prison
    EVT_PRISON_AETHAS_REVEAL,               // Declenche par OnUnitDeath une fois le groupe mort
    EVT_PRISON_AETHAS_AURAS,
    EVT_PRISON_ROMMATH_TELEPORT,
    EVT_PRISON_TELEPORT_GROUP,              // Teleport des joueurs vers le gardien
    EVT_PRISON_ROMMATH_FOLLOW,              // Rommath suit le joueur le plus proche
    EVT_PRISON_NARASI_CHECKER,              // Watchdog : Rommath a portee de Narasi ?
    EVT_PRISON_NARASI_TALK_01,
    EVT_PRISON_SURDIEL_TALK_02,             // Surdiel engage Narasi
    EVT_PRISON_ROMMATH_TALK_08,

    // What happened! : l'evasion
    EVT_ESCAPE_SURDIEL_TALK_03,
    EVT_ESCAPE_SURDIEL_TALK_04,
    EVT_ESCAPE_NARASI_IMPRISON,             // Emprisonnement arcanique de Surdiel
    EVT_ESCAPE_HATHOREL_TALK_06,
    EVT_ESCAPE_ROMMATH_PORTAL               // Portail vers Lune-d'argent
};

class scenario_dalaran_purge : public InstanceMapScript
{
	public:
	scenario_dalaran_purge() : InstanceMapScript(DLPScriptName, 5002)
	{
	}

	struct scenario_dalaran_purge_InstanceScript : public InstanceScript
	{
		scenario_dalaran_purge_InstanceScript(InstanceMap* map) : InstanceScript(map),
			eventId(EVT_DALARAN_PURGE_CONVERSATION), phase(DLPPhases::FindJaina01)
		{
			SetHeaders(DataHeader);
			LoadObjectData(creatureData, gameobjectData);
		}

		// Auras portees par les joueurs pendant une tranche de phases.
		// Posees et retirees par CustomScenario::SyncPhaseAuras, depuis
		// SetData(DATA_SCENARIO_PHASE) et OnPlayerEnter.
		static constexpr CustomScenario::PhaseAura PhaseAuras[] =
		{
			// Ambiance neigeuse : toute la duree du scenario.
			{ SPELL_SNOWY_WEATHER, 0, CustomScenario::PhaseAura::ToEnd },
			// Baguette de dissipation, remise pour liberer les citoyens.
			{ SPELL_WAND_OF_DISPELLING, (uint32)DLPPhases::FreeCitizens, (uint32)DLPPhases::RemainingSunreavers },
			// Illusion horde de l'infiltration
			{ SPELL_HORDE_ILLUSION_1,           (uint32)DLPPhases::TheEscape, (uint32)DLPPhases::TheEscape_Escort,
			  CustomScenario::PhaseAuraMode::RestoreOnly },
			{ SPELL_HORDE_ILLUSION_REACTIONS,   (uint32)DLPPhases::TheEscape, (uint32)DLPPhases::TheEscape_Escort,
			  CustomScenario::PhaseAuraMode::RestoreOnly },
            { SPELL_HORDE_ILLUSION,             (uint32)DLPPhases::TheEscape, (uint32)DLPPhases::TheEscape_Escort,
			  CustomScenario::PhaseAuraMode::RestoreOnly },
			{ SPELL_FLASHBACK_EFFECT,           (uint32)DLPPhases::TheEscape, (uint32)DLPPhases::TheEscape_Escort,
			  CustomScenario::PhaseAuraMode::RestoreOnly }
		};

		void OnPlayerEnter(Player* player) override
		{
			CustomScenario::SyncPhaseAuras(player, (uint32)phase, PhaseAuras);
		}

		void OnPlayerLeave(Player* player) override
		{
			CustomScenario::RemovePhaseAuras(player, PhaseAuras);
		}

		uint32 GetData(uint32 dataId) const override
		{
			if (dataId == DATA_SCENARIO_PHASE)
				return (uint32)phase;
			return 0U;
		}

		void SetData(uint32 dataId, uint32 value) override
		{
			switch (dataId)
			{
				case DATA_SCENARIO_PHASE:
					phase = (DLPPhases)value;
					CustomScenario::SyncPhaseAuras(instance, value, PhaseAuras);
					break;
				case EVENT_FIND_JAINA_02:
					#ifdef CUSTOM_DEBUG
						TriggerGameEvent(EVENT_FIND_JAINA_02);
					#endif
					SetData(DATA_SCENARIO_PHASE, (uint32)DLPPhases::FreeTheArcanist);
					events.ScheduleEvent(EVT_FIRST_STEP_FACE_EACH_OTHER, 2s);
					break;
				case EVENT_FREE_AETHAS_SUNREAVER:
					SetData(DATA_SCENARIO_PHASE, (uint32)DLPPhases::TheEscape_End);
					events.ScheduleEvent(EVT_ESCAPE_SURDIEL_TALK_03, 2s);
					break;
                case EVENT_CAPTAIN_ANTHEAS_TELEPORT:
                    events.ScheduleEvent(EVT_THE_ARCANIST_TELEPORT_01, 1ms);
                    break;
				default:
					break;
			}
		}

		void OnCompletedCriteriaTree(CriteriaTree const* tree) override
		{
			switch (tree->ID)
			{
				// Dalaran
				case CRITERIA_TREE_DALARAN:
				{
					if (Creature* aethas = GetAethas())
						aethas->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, AethasPos01, true, AethasPos01.GetOrientation());
					if (Creature* landalock = GetCreature(DATA_ARCHMAGE_LANDALOCK))
						landalock->CastSpell(landalock, SPELL_FROST_CANALISATION);
					if (Creature* surdiel = GetCreature(DATA_MAGISTER_SURDIEL))
					{
						if (Creature* zuros = GetCreature(DATA_MAGE_COMMANDER_ZUROS))
						{
							surdiel->AI()->AttackStart(zuros);
							zuros->AI()->AttackStart(surdiel);
						}
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)DLPPhases::FindingTheThieves);
					#ifdef CUSTOM_DEBUG
						events.ScheduleEvent(EVT_DALARAN_ELEMENTAL_MOVE, 1s);
					#else
						events.ScheduleEvent(EVT_DALARAN_PURGE_CONVERSATION, 10s);
					#endif
					break;
				}
				// Finding the thieves
				case CRITERIA_TREE_FINDING_THE_THIEVES:
				{
					DoOnCreatures(patrol, [this](Creature* creature)
					{
						creature->SetFaction(FACTION_DALARAN_PATROL);
						creature->setActive(true);
						creature->SetVisible(true);
					});
					break;
				}
				// A Facelift
				case CRITERIA_TREE_A_FACELIFT:
				{
					if (Creature* jaina = GetJaina())
					{
						jaina->SetVisible(true);
						jaina->SetImmuneToAll(true);
						jaina->RemoveAllAuras();
						jaina->NearTeleportTo(JainaPos01);
						jaina->SetHomePosition(JainaPos01);
                        jaina->AI()->EnterEvadeMode();
					}

					DoOnCreatures(patrol, [this](Creature* creature)
					{
						creature->CombatStop();
						creature->SetFaction(FACTION_FRIENDLY);
						creature->setActive(false);
						creature->SetVisible(false);
					});

					for (ObjectGuid guid : sunreavers)
					{
						if (Creature* creature = instance->GetCreature(guid))
						{
							creature->Respawn(true);
							creature->CombatStop();
							creature->SetFaction(FACTION_FRIENDLY);
							creature->setActive(false);
							creature->SetVisible(false);
						}
						else
						{
							instance->Respawn(SPAWN_TYPE_CREATURE, guid.GetCounter());
						}
					}

					SetData(DATA_SCENARIO_PHASE, (uint32)DLPPhases::FindJaina02);

					break;
				}
				// First Step
				case CRITERIA_TREE_FIRST_STEP:
				{
					if (Creature* rathaella = GetCreature(DATA_ARCANIST_RATHAELLA))
						rathaella->SetNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
					break;
				}
				// An Unfortunate Capture
				case CRITERIA_TREE_UNFORTUNATE_CAPTURE:
				{
					// La baguette est posee par PhaseAuras en entrant dans FreeCitizens.
					SetData(DATA_SCENARIO_PHASE, (uint32)DLPPhases::FreeCitizens);
					break;
				}
				// Serve and protect
				case CRITERIA_TREE_SERVE_AND_PROTECT:
				{
					Talk(GetJaina(), SAY_BRASAEL_JAINA_01);
					SetData(DATA_SCENARIO_PHASE, (uint32)DLPPhases::KillMagisters);
					break;
				}
				// Cashing Out
				case CRITERIA_TREE_CASHING_OUT:
				{
                    if (Creature* jaina = GetJaina())
                    {
                        Talk(GetJaina(), SAY_SAVOR_JAINA_01);

                        jaina->AI()->EnterEvadeMode(EvadeReason::Other);

                        if (GameObject* portal = GetGameObject(DATA_PORTAL_TO_SEWERS))
                            portal->RemoveFlag(GO_FLAG_IN_USE | GO_FLAG_NOT_SELECTABLE | GO_FLAG_LOCKED);
                    }
					SetData(DATA_SCENARIO_PHASE, (uint32)DLPPhases::RemainingSunreavers);
					break;
				}
				// The Remaining Sunreavers
				case CRITERIA_TREE_REMAINING_SUNREAVERS:
				{
					if (Creature* rommath = GetRommath())
						Talk(rommath, SAY_INFILTRATE_ROMMATH_07);
					if (Creature* jaina = GetJaina())
					{
                        if (GameObject* portal = GetGameObject(DATA_PORTAL_TO_SEWERS))
                            ClosePortal(portal);

                        jaina->NearTeleportTo(JainaPos02);
						jaina->SetHomePosition(JainaPos02);
                        jaina->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
						jaina->CastSpell(jaina, SPELL_CHAT_BUBBLE);
                    }
					if (Creature* sorin = GetCreature(DATA_SORIN_MAGEHAND))
					{
						sorin->RemoveAllAuras();
						sorin->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
						sorin->CastSpell(sorin, SPELL_CHAT_BUBBLE);
                        sorin->CastSpell(SorinPoint01, SPELL_TELEPORT);
					}
					if (Creature* narasi = GetCreature(DATA_NARASI_SNOWDAWN))
					{
						if (Creature* surdiel = GetCreature(DATA_MAGISTER_SURDIEL))
							narasi->CastSpell(surdiel, SPELL_ARCANE_IMPRISONMENT);
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)DLPPhases::TheEscape);
					break;
				}
				// What happened! - Speak to Jaina Proudmoore
				case CRITERIA_TREE_SPEAK_TO_JAINA:
				{
					if (Creature* jaina = GetJaina())
					{
						jaina->SetVisible(false);

						Trinity::RespawnDo doRespawn;
						Trinity::WorldObjectWorker<Trinity::RespawnDo> worker(jaina, doRespawn);
						Cell::VisitGridObjects(jaina, worker, INFINITY);

						std::vector<RespawnInfo const*> data;
						instance->GetRespawnInfo(data, SPAWN_TYPEMASK_CREATURE);

						if (!data.empty())
						{
							for (RespawnInfo const* info : data)
								instance->Respawn(info->type, info->spawnId);
						}
					}

					DoOnCreatures(patrol, [this](Creature* creature)
					{
						creature->SetFaction(FACTION_DALARAN_PATROL);
						creature->setActive(true);
						creature->SetVisible(true);
					});
					DoOnCreatures(citizens, [this](Creature* creature)
					{
						creature->SetFaction(FACTION_FRIENDLY);
						creature->setActive(false);
						creature->SetVisible(false);
					});
					DoOnCreatures(barriers, [this](Creature* creature)
					{
						creature->KillSelf();
					});

                    instance->DoOnPlayers([this](Player* player)
                    {
                        player->CastSpell(player, SPELL_FLASHBACK_EFFECT, true);
                        player->CastSpell(player, SPELL_FADING_TO_BLACK, true);
                    });

                    if (Creature* zuros = GetCreature(DATA_MAGE_COMMANDER_ZUROS))
                    {
                        zuros->SetVisible(false);
                    }

                    if (Creature* rathaella = GetCreature(DATA_ARCANIST_RATHAELLA))
                    {
                        rathaella->SetVisible(false);
                    }

					if (Creature* landalock = GetCreature(DATA_ARCHMAGE_LANDALOCK))
					{
						if (GameObject* collider = landalock->FindNearestGameObject(GOB_ICE_WALL_COLLISION, 15.f))
						{
							landalock->SummonGameObject(GOB_ICE_WALL_COLLISION, collider->GetPosition(),
														QuaternionData::fromEulerAnglesZYX(collider->GetOrientation(), 0.0f, 0.0f), 0s);
						}

						landalock->NearTeleportTo(LandalockPos01);
						landalock->SetHomePosition(LandalockPos01);
						landalock->CastSpell(landalock, SPELL_FROST_CANALISATION);
					}

					if (Creature* sorin = GetCreature(DATA_SORIN_MAGEHAND))
					{
						sorin->RemoveAllAuras();
						sorin->CastSpell(sorin, SPELL_ARCANE_BARRIER, true);
						sorin->CastSpell(sorin, SPELL_RUNES_OF_SHIELDING, true);
					}

                    if (Creature* rommath = GetRommath())
                    {
                        rommath->CastSpell(rommath, SPELL_COSMETIC_YELLOW_ARROW);
                        rommath->SetFullHealth();
                        rommath->SetFullPower(POWER_MANA);
                    }

					if (Creature* surdiel = GetCreature(DATA_MAGISTER_SURDIEL))
					{
						surdiel->SetWalk(true);
						surdiel->RemoveAllAuras();
						surdiel->NearTeleportTo(SurdielPos01);
						surdiel->SetHomePosition(SurdielPos01);
					}

					events.ScheduleEvent(EVT_SEWERS_TELEPORT_PLAYERS, 500ms);
					SetData(DATA_SCENARIO_PHASE, (uint32)DLPPhases::TheEscape_Events);
					break;
				}
				// What happened!
				case CRITERIA_TREE_WHAT_HAPPENED:
				{
					if (GameObject* portal = instance->GetGameObject(endPortal))
						ClosePortal(portal);

					if (Creature* jaina = GetJaina())
					{
						jaina->SetVisible(true);
						jaina->SummonGameObject(GOB_PORTAL_TO_STORMWIND, EndPortalPos01,
												QuaternionData::fromEulerAnglesZYX(EndPortalPos01.GetOrientation(), 0.0f, 0.0f), 0s);

						Trinity::RespawnDo doRespawn;
						Trinity::WorldObjectWorker<Trinity::RespawnDo> worker(jaina, doRespawn);
						Cell::VisitGridObjects(jaina, worker, INFINITY);

						std::vector<RespawnInfo const*> data;
						instance->GetRespawnInfo(data, SPAWN_TYPEMASK_ALL);

						if (!data.empty())
						{
							for (RespawnInfo const* info : data)
								instance->Respawn(info->type, info->spawnId);
						}

						if (Player* player = GetNearestPlayer(jaina))
							jaina->SetFacingToObject(player);
					}

					DoOnCreatures(patrol, [this](Creature* creature)
					{
						creature->SetFaction(FACTION_FRIENDLY);
						creature->setActive(false);
						creature->SetVisible(false);
					});
					DoOnCreatures(extraction, [this](Creature* creature)
					{
						creature->SetVisible(true);
					});

					instance->DoOnPlayers([this](Player* player)
					{
						player->CastSpell(player, SPELL_FADING_TO_BLACK, true);
                        ApplyHordeIllusion(player, false);
					});

					break;
				}
                // What happened! - Find the Grand Magister Rommath
				case CRITERIA_TREE_FIND_ROMMATH:
                {
                    events.ScheduleEvent(EVT_ESCORT_ROMMATH_TALK_01, 2s);
                    SetData(DATA_SCENARIO_PHASE, (uint32)DLPPhases::TheEscape_Escort);
                    break;
                }
				// What happened! - Follow the tracks
				case CRITERIA_TREE_FOLLOW_TRACKS:
                {
                    std::list<TempSummon*> summons;
                    instance->SummonCreatureGroup(CREATURE_GROUP_PRISON, &summons);

                    prison.clear();
                    for (TempSummon* summon : summons)
                        prison.push_back(summon->GetGUID());

                    if (Creature* rommath = GetRommath())
                    {
                        rommath->NearTeleportTo(RommathPos01);
                        rommath->SetHomePosition(RommathPos01);
                    }
                    break;
                }
				default:
					break;
			}
		}

		void OnCreatureCreate(Creature* creature) override
		{
			if (creature->HasUnitTypeMask(UNIT_MASK_SUMMON))
				return;

			InstanceScript::OnCreatureCreate(creature);

			creature->SetVisibilityDistanceOverride(VisibilityDistanceType::Gigantic);
			creature->SetPvpFlag(UNIT_BYTE2_FLAG_PVP);
			creature->SetUnitFlag(UNIT_FLAG_PVP_ENABLING);
			creature->SetBoundingRadius(20.f);

			switch (creature->GetEntry())
			{
				case NPC_MAGISTER_BRASAEL:
					creature->SetImmuneToAll(true);
					creature->SetSheath(SHEATH_STATE_UNARMED);
					creature->AddAura(SPELL_HOLD_BAG, creature);
					creature->SetEmoteState(EMOTE_STATE_LOOT_BITE_SOUND);
					break;
				case NPC_DALARAN_CITIZEN:
					if (roll_chance(30))
						creature->SetEmoteState(EMOTE_STATE_COWER);
                    citizens.push_back(creature->GetGUID());
					break;
                case NPC_NARASI_SNOWDAWN:
                    creature->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP | UNIT_NPC_FLAG_QUESTGIVER);
                    break;
                case NPC_MAGE_COMMANDER_ZUROS:
				case NPC_MAGISTER_SURDIEL:
					creature->SetImmuneToPC(true);
                    break;
				case NPC_WANTON_HOST:
				case NPC_WANTON_HOSTESS:
					creature->SetImmuneToAll(true);
					creature->SetLevel(60);
					creature->SetMaxHealth(urand(15800, 35000));
					creature->SetFullHealth();
					creature->SetFaction(FACTION_HORDE_GENERIC);
					break;
				case NPC_ARCANIST_RATHAELLA:
					creature->SetImmuneToNPC(true);
					creature->SetStandState(UNIT_STAND_STATE_KNEEL, 14904);
					creature->CastSpell(creature, SPELL_ATTACHED);
					break;
				case NPC_MAGISTER_HATHOREL:
				case NPC_HIGH_ARCANIST_SAVOR:
				case NPC_GRAND_MAGISTER_ROMMATH:
				case NPC_CAPTAIN_ANTHEAS:
					creature->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
					creature->SetImmuneToAll(true);
					break;
				case NPC_ICE_WALL:
					creature->SetImmuneToAll(true);
					creature->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
					break;
				case NPC_JAINA_PROUDMOORE_PATROL:
				case NPC_BOUND_WATER_ELEMENTAL:
					creature->SetFaction(FACTION_FRIENDLY);
					creature->setActive(false);
					creature->SetVisible(false);
					patrol.push_back(creature->GetGUID());
					break;
				case NPC_VEREESA_WINDRUNNER:
					creature->SetNpcFlag(UNIT_NPC_FLAG_NONE);
					break;
				case NPC_AETHAS_SUNREAVER:
					creature->SetImmuneToAll(true);
					creature->SetWalk(true);
					creature->AddAura(SPELL_CASTER_READY_03, creature);
					break;
				case NPC_HIGH_SUNREAVER_MAGE:
					creature->SetImmuneToAll(true);
					creature->AddAura(SPELL_CASTER_READY_02, creature);
					highmages.push_back(creature->GetGUID());
					break;
				case NPC_SUNREAVER_CITIZEN:
                    if (!creature->HasStringId("ArcanistTeleport"))
                        sunreavers.push_back(creature->GetGUID());
					break;
				case NPC_ARCANE_BARRIER:
					barriers.push_back(creature->GetGUID());
					break;
				case NPC_SUNREAVER_EXTRACTION_TROOP:
					FeignDeath(creature);
					if (roll_chance(50))
						creature->AddAura(RAND(SPELL_FROZEN_SOLID, SPELL_BURNING), creature);
					extraction.push_back(creature->GetGUID());
					break;
                    break;
				default:
					break;
			}
		}

		void OnUnitDeath(Unit* unit) override
		{
			InstanceScript::OnUnitDeath(unit);

			// Le groupe de la prison est compose de plusieurs entries differentes :
			// on suit les GUIDs summonnes plutot que les entries, et l'etape ne
			// se declenche qu'une fois le dernier membre tombe.
			if (std::erase(prison, unit->GetGUID()) && prison.empty())
				events.ScheduleEvent(EVT_PRISON_AETHAS_REVEAL, 2s);
		}

		void OnGameObjectCreate(GameObject* go) override
		{
			InstanceScript::OnGameObjectCreate(go);

			go->SetVisibilityDistanceOverride(VisibilityDistanceType::Gigantic);

			switch (go->GetEntry())
			{
				case GOB_SECRET_PASSAGE:
				case GOB_PORTAL_TO_PRISON:
					go->SetFlag(GO_FLAG_IN_USE | GO_FLAG_NOT_SELECTABLE | GO_FLAG_LOCKED);
					break;
				case GOB_LAMP_POST:
					go->SetLootState(GO_READY);
					go->UseDoorOrButton();
					break;
                case GOB_PORTAL_TO_SEWERS:
                    go->SetLootState(GO_READY);
                    go->UseDoorOrButton();
                    go->SetFlag(GO_FLAG_NOT_SELECTABLE);
                    break;
			}
		}

		void Update(uint32 diff) override
		{
			events.Update(diff);
			switch (eventId = events.ExecuteEvent())
			{
				// Dalaran
				#pragma region DALARAN

                case EVT_DALARAN_PURGE_CONVERSATION:
                {
                    Creature* jaina = GetJaina();
                    if (!jaina)
                        break;

                    Conversation* purge = Conversation::CreateConversation(
                        CONVERSATION_DALARAN_PURGE, jaina, jaina->GetPosition(), ObjectGuid::Empty);

                    LocaleConstant privateOwnerLocale = purge->GetPrivateObjectOwnerLocale();
                    Next(purge ? purge->GetLastLineEndTime(privateOwnerLocale) : 27600ms);
                    break;
                }
				case EVT_DALARAN_HIGHMAGES_ASSAULT:
                {
                    Creature* jaina = GetJaina();
                    for (uint8 i = 0;
                        i < highmages.size(); ++i)
                    {
                        if (Creature* highmage = instance->GetCreature(highmages[i]))
                        {
                            if (i <= 2)
                            {
                                highmage->CastSpell(jaina, SPELL_FIREBALL_COSMETIC);
                            }
                            else
                            {
                                highmage->SetSpeedRate(MOVE_RUN, 0.7f);
                                highmage->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_NONE, jaina, 1.6f);
                            }
                        }
                    }
					Next(760ms);
					break;
                }
				case EVT_DALARAN_JAINA_FROST_NOVA:
                    if (Creature* jaina = GetJaina())
                        jaina->CastSpell(jaina, SPELL_FROST_NOVA_COSMETIC,
                            CastSpellExtraArgs(TRIGGERED_CAST_DIRECTLY));
					Next(380ms);
					break;
                case EVT_DALARAN_HIGHMAGES_DOWN:
                {
                    for (uint8 i = 0;
                        i < highmages.size(); ++i)
                    {
                        if (Creature* highmage = instance->GetCreature(highmages[i]))
                        {
                            FeignDeath(highmage);
                            highmage->CastStop();
                            highmage->GetMotionMaster()->StopOnDeath();
                        }
                    }
                    Next(1s);
                    break;
                }
				case EVT_DALARAN_JAINA_APPROACH_AETHAS:
					if (Creature* jaina = GetJaina())
					{
						jaina->SetWalk(true);
						jaina->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_NONE, GetAethas(), 3.6f);
					}
					Next(6s);
					break;
				case EVT_DALARAN_AETHAS_TELEPORT:
                    GetAethas()->CastSpell(GetAethas(), SPELL_TELEPORT);
                    if (Creature* jaina = GetJaina())
                    {
                        jaina->SetWalk(false);
                        jaina->CastSpell(jaina, SPELL_TELEPORT);
                    }
					Next(1s);
					break;
                case EVT_DALARAN_ELEMENTAL_MOVE:
                {
                    if (Creature* elemental = GetCreature(DATA_SUMMONED_WATER_ELEMENTAL))
                        elemental->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, ElementalPos01, true,
                                                                ElementalPos01.GetOrientation(), {},
                                                                MovementWalkRunSpeedSelectionMode::ForceWalk);
                    GetJaina()->SetVisible(false);
                    GetAethas()->SetVisible(false);
                    TriggerGameEvent(EVENT_ASSIST_JAINA);
                    break;
                }

				#pragma endregion

				// First Step
				#pragma region FIRST_STEP

				case EVT_FIRST_STEP_FACE_EACH_OTHER:
					GetVereesa()->SetFacingToObject(GetJaina());
					GetJaina()->SetFacingToObject(GetVereesa());
					Next(2s);
					break;
				case EVT_FIRST_STEP_VEREESA_TALK_01:
					Talk(GetVereesa(), SAY_FIRST_STEP_VEREESA_01);
					Next(8s);
					break;
				case EVT_FIRST_STEP_JAINA_TALK_02:
					Talk(GetJaina(), SAY_FIRST_STEP_JAINA_02);
					Next(6s);
					break;
				case EVT_FIRST_STEP_JAINA_TALK_03:
					if (Creature* jaina = GetJaina())
					{
						Talk(jaina, SAY_FIRST_STEP_JAINA_03);
						if (Player* player = GetNearestPlayer(jaina))
							jaina->SetFacingToObject(player);
					}
					Next(2s);
					break;
				case EVT_FIRST_STEP_VEREESA_TALK_04:
					if (Creature* vereesa = GetVereesa())
					{
						Talk(vereesa, SAY_FIRST_STEP_VEREESA_04);
						if (Player* player = GetNearestPlayer(vereesa))
							vereesa->SetFacingToObject(player);
					}
					Next(2s);
					break;
				case EVT_FIRST_STEP_TRIGGER_NEXT:
					TriggerGameEvent(EVENT_FIND_JAINA_02);
					break;

				#pragma endregion

                // The Arcanist - Teleport
                #pragma region THE ARCANIST TELEPORT

                case EVT_THE_ARCANIST_TELEPORT_01:
                {
                    GetVesara()->CastSpell(GetVesara(), SPELL_MASS_TELEPORT);
                    if (Creature* antheas = GetAntheas())
                    {
                        antheas->SetFacingToObject(GetGalendor());
                        antheas->SetEmoteState(EMOTE_STATE_READY1H_ALLOW_MOVEMENT);
                    }
                    Next(1s);
                    break;
                }
                case EVT_THE_ARCANIST_TELEPORT_02:
                {
                    Creature* antheas = GetAntheas();
                    Creature* galendor = GetGalendor();
                    if (antheas && galendor)
                    {
                        galendor->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_NONE, antheas, 2.5f);
                        antheas->CastSpell(galendor, SPELL_WHIRLWIND);
                    }
                    Next(2300ms);
                    break;
                }
                case EVT_THE_ARCANIST_TELEPORT_03:
                    GetGalendor()->KillSelf();
                    Next(2s);
                    break;
                case EVT_THE_ARCANIST_TELEPORT_04:
                    GetAntheas()->SetImmuneToAll(false);
                    if (Creature* landalock = GetCreature(DATA_ARCHMAGE_LANDALOCK))
                    {
                        landalock->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
                        landalock->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                        landalock->CastSpell(landalock, SPELL_CHAT_BUBBLE, true);
                    }
                    break;

                #pragma endregion

				// What happened! - Events
				#pragma region WHAT_HAPPENED_EVENTS

				case EVT_SEWERS_TELEPORT_PLAYERS:
					instance->DoOnPlayers([this](Player* player)
					{
						player->NearTeleportTo(SewersPos01);
					});
					if (Creature* surdiel = GetCreature(DATA_MAGISTER_SURDIEL))
						surdiel->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, SurdielPos02, true, SurdielPos02.GetOrientation());
					Next(1s);
					break;
				case EVT_SEWERS_HORDE_ILLUSION:
					DoOnCreatures(extraction, [this](Creature* creature)
					{
						creature->SetVisible(false);
					});
					instance->DoOnPlayers([this](Player* player)
					{
						player->CombatStop();
                        ApplyHordeIllusion(player, true);
					});
					break;

				#pragma endregion

				// What happened! - Escort
				#pragma region WHAT_HAPPENED_ESCORT

				case EVT_ESCORT_ROMMATH_TALK_01:
					Talk(GetRommath(), SAY_INFILTRATE_ROMMATH_01);
					Next(12s);
					break;
				case EVT_ESCORT_ROMMATH_TALK_02:
					Talk(GetRommath(), SAY_INFILTRATE_ROMMATH_02);
					Next(5s);
					break;
				case EVT_ESCORT_ROMMATH_PATH:
					GetRommath()->GetMotionMaster()->MovePath(RommathPath01, false);
					Next(15s);
					break;
				case EVT_ESCORT_ROMMATH_TALK_03:
					Talk(GetRommath(), SAY_INFILTRATE_ROMMATH_03);
					Next(15s);
					break;
				case EVT_ESCORT_PORTAL_CHECKER:
					if (Creature* rommath = GetRommath())
					{
						if (GameObject* portal = GetGameObject(DATA_PORTAL_TO_PRISON))
						{
							if (rommath->IsVisible() && rommath->IsWithinDist(portal, 35.0f))
							{
                                events.CancelEvent(EVT_ESCORT_PORTAL_CHECKER);

                                if (TempSummon* dummy = portal->SummonCreature(WORLD_TRIGGER, portal->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, 10s))
                                {
                                    rommath->SetOwnerGUID(ObjectGuid::Empty);
                                    rommath->GetMotionMaster()->Clear();
                                    rommath->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_02, dummy, 5.0f);
                                }
							}
							else
							{
								events.RescheduleEvent(EVT_ESCORT_PORTAL_CHECKER, 2s);
							}
						}
					}
					break;

				#pragma endregion

				// What happened! - Prison
				#pragma region WHAT_HAPPENED_PRISON

				case EVT_PRISON_AETHAS_REVEAL:
                    if (Creature* aethas = GetAethas())
                    {
                        aethas->RemoveAllAuras();
                        aethas->SetImmuneToAll(true);
                        aethas->SetVisible(true);
                        aethas->NearTeleportTo(AethasPos02);
                        aethas->SetHomePosition(AethasPos02);
                    }
                    Next(2s);
					break;
				case EVT_PRISON_AETHAS_AURAS:
					if (Creature* aethas = GetAethas())
					{
						aethas->AddAura(SPELL_ICY_GLARE, aethas);
						aethas->AddAura(SPELL_CHILLING_BLAST, aethas);
					}
					Talk(GetRommath(), SAY_INFILTRATE_ROMMATH_05);
					Next(8s);
					break;
				case EVT_PRISON_ROMMATH_TELEPORT:
					if (Creature* rommath = GetRommath())
					{
						Talk(rommath, SAY_INFILTRATE_ROMMATH_08);
						rommath->CastSpell(rommath, SPELL_TELEPORT_CASTER);
					}
					Next(5s);
					break;
				case EVT_PRISON_TELEPORT_GROUP:
					if (Creature* rommath = GetRommath())
					{
						rommath->NearTeleportTo(GuardianPos01);
						rommath->SetHomePosition(GuardianPos01);
						instance->DoOnPlayers([](Player* player)
						{
                            const Position dest = GetRandomPosition(player, GuardianPos01, TELEPORT_SPREAD_RADIUS);
                            player->CastSpell(dest, SPELL_TELEPORT);
						});
					}
					if (Creature* hathorel = GetCreature(DATA_MAGISTER_HATHOREL))
						hathorel->NearTeleportTo(HathorelPos01);
					Next(2s);
					break;
				case EVT_PRISON_ROMMATH_FOLLOW:
					if (Creature* rommath = GetRommath())
					{
						if (Player* player = GetNearestPlayer(rommath))
						{
							rommath->SetOwnerGUID(player->GetGUID());
							rommath->SetImmuneToAll(false);
							rommath->GetMotionMaster()->Clear();
							rommath->GetMotionMaster()->MoveFollow(player, PET_FOLLOW_DIST, rommath->GetFollowAngle());
						}
					}
					if (Creature* hathorel = GetCreature(DATA_MAGISTER_HATHOREL))
					{
						hathorel->SetHomePosition(HathorelPos02);
						hathorel->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, HathorelPos02);
					}
					Next(2s);
					break;
				case EVT_PRISON_NARASI_CHECKER:
					if (Creature* rommath = GetRommath())
					{
						if (Creature* narasi = GetCreature(DATA_NARASI_SNOWDAWN))
						{
							if (rommath->IsWithinDist(narasi, 45.0f))
							{
                                events.CancelEvent(EVT_PRISON_NARASI_CHECKER);

								Talk(rommath, SAY_INFILTRATE_ROMMATH_06);

                                rommath->SetImmuneToAll(true);
                                rommath->SetOwnerGUID(ObjectGuid::Empty);
								rommath->SetHomePosition(RommathPos02);
								rommath->GetMotionMaster()->Clear();
								rommath->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, RommathPos02, true, RommathPos02.GetOrientation());

								Next(3s);
							}
							else
							{
								events.RescheduleEvent(EVT_PRISON_NARASI_CHECKER, 2s);
							}
						}
					}
					break;
				case EVT_PRISON_NARASI_TALK_01:
                    if (Creature* narasi = GetCreature(DATA_NARASI_SNOWDAWN))
                    {
                        Talk(narasi, SAY_INFILTRATE_NARASI_01);
                        narasi->SetImmuneToPC(true);
                        narasi->SetImmuneToNPC(false);
                        narasi->SetReactState(REACT_AGGRESSIVE);
                    }
                    if (Creature* surdiel = GetCreature(DATA_MAGISTER_SURDIEL))
                    {
                        surdiel->AI()->SetBoundary(nullptr);
                        surdiel->CombatStop();
                        surdiel->SetImmuneToNPC(false);
                        surdiel->SetReactState(REACT_AGGRESSIVE);
                        surdiel->CastSpell(SurdielPos03, SPELL_TELEPORT);
                        surdiel->SetHomePosition(SurdielPos03);
                    }
					Next(1s);
					break;
				case EVT_PRISON_SURDIEL_TALK_02:
					if (Creature* surdiel = GetCreature(DATA_MAGISTER_SURDIEL))
					{
						Talk(surdiel, SAY_INFILTRATE_SURDIEL_02);
						if (Creature* narasi = GetCreature(DATA_NARASI_SNOWDAWN))
						{
							surdiel->Attack(narasi, true);
                            narasi->Attack(surdiel, true);
						}
					}
                    Next(5s);
                    break;
                case EVT_PRISON_ROMMATH_TALK_08:
                    if (Creature* rommath = GetRommath())
                    {
                        Talk(rommath, SAY_INFILTRATE_ROMMATH_08);
                        rommath->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, RommathPos03);
                    }
                    break;

				#pragma endregion

				// What happened! - Escape
				#pragma region WHAT_HAPPENED_PRISON

				case EVT_ESCAPE_SURDIEL_TALK_03:
					if (Creature* surdiel = GetCreature(DATA_MAGISTER_SURDIEL))
						Talk(surdiel, SAY_INFILTRATE_SURDIEL_03);
                    Next(2s);
                    break;
				case EVT_ESCAPE_SURDIEL_TALK_04:
                    if (Creature* surdiel = GetCreature(DATA_MAGISTER_SURDIEL))
                    {
                        Talk(surdiel, SAY_INFILTRATE_SURDIEL_04);
                        if (Creature* narasi = GetCreature(DATA_NARASI_SNOWDAWN))
                        {
                            narasi->CombatStop();
                            narasi->SetReactState(REACT_PASSIVE);
                            narasi->SetImmuneToPC(true);

                            surdiel->CombatStop();
                            surdiel->SetReactState(REACT_PASSIVE);
                            surdiel->SetImmuneToPC(true);
                        }
                    }
					Next(3s);
					break;
				case EVT_ESCAPE_NARASI_IMPRISON:
					if (Creature* narasi = GetCreature(DATA_NARASI_SNOWDAWN))
					{
						Talk(narasi, SAY_INFILTRATE_NARASI_05);
						if (Creature* surdiel = GetCreature(DATA_MAGISTER_SURDIEL))
							narasi->CastSpell(surdiel, SPELL_ARCANE_IMPRISONMENT);
					}
					Next(1s);
					break;
				case EVT_ESCAPE_HATHOREL_TALK_06:
					if (Creature* hathorel = GetCreature(DATA_MAGISTER_HATHOREL))
						Talk(hathorel, SAY_INFILTRATE_HATHOREL_06);
                    if (Creature* jaina = instance->SummonCreature(NPC_JAINA_PROUDMOORE, RommathPos02))
                    {
                        jaina->SetImmuneToAll(true);
                        jaina->SetWalk(true);
                        jaina->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, RommathPos03);
                        jaina->DespawnOrUnsummon(11s);
                    }
                    Next(2800ms);
					break;
				case EVT_ESCAPE_ROMMATH_PORTAL:
					if (Creature* rommath = GetRommath())
					{
						Talk(rommath, SAY_INFILTRATE_ROMMATH_07);

						if (GameObject* portal = rommath->SummonGameObject(GOB_PORTAL_TO_SILVERMOON, EndPortalPos01,
							QuaternionData::fromEulerAnglesZYX(EndPortalPos01.GetOrientation(), 0.0f, 0.0f), 0s))
						{
							endPortal = portal->GetGUID();

                            portal->SetFlag(GO_FLAG_NOT_SELECTABLE | GO_FLAG_IN_USE);

							rommath->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_03, portal->GetPosition());

							if (Creature* hathorel = GetCreature(DATA_MAGISTER_HATHOREL))
								hathorel->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_03, portal->GetPosition());

                            if (Creature* aethas = GetAethas())
                            {
                                aethas->SetWalk(false);
                                aethas->RemoveAllAuras();
                                aethas->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_03, portal->GetPosition());
                            }
						}
					}
					break;

				#pragma endregion

				default:
					break;
			}
		}

		// =================================================================
		// Etat interne
		// =================================================================
		EventMap events;                  // Chaine des events cinematiques
		uint32 eventId;                   // Dernier event execute (sert a Next() pour planifier eventId + 1)
		DLPPhases phase;                  // Phase courante du scenario

		// Listes de GUID constituees dans OnCreatureCreate, manipulees en
		// masse par les transitions de phase.
		GuidVector highmages;             // Hauts-mages du flashback de la purge
		GuidVector patrol;                // Patrouille de Dalaran
		GuidVector citizens;              // Citoyens de Dalaran
		GuidVector sunreavers;            // Sunreavers a liberer
		GuidVector extraction;            // Prisonniers en cours d'extraction
		GuidVector barriers;              // Barrieres magiques
		GuidVector prison;                // Groupe de la prison (voir OnUnitDeath)

		ObjectGuid endPortal;             // Portail de fin vers Lune-d'argent

		// Accesseurs
		#pragma region ACCESSORS
		
		Creature* GetJaina()        { return GetCreature(DATA_JAINA_PROUDMOORE); }
		Creature* GetVereesa()      { return GetCreature(DATA_VEREESA_WINDRUNNER); }
		Creature* GetAethas()       { return GetCreature(DATA_AETHAS_SUNREAVER); }
		Creature* GetElemental()    { return GetCreature(DATA_SUMMONED_WATER_ELEMENTAL); }
		Creature* GetRommath()      { return GetCreature(DATA_GRAND_MAGISTER_ROMMATH); }
		Creature* GetAntheas()      { return GetCreature(DATA_CAPTAIN_ANTHEAS); }
		Creature* GetGalendor()     { return GetCreature(DATA_GALENDROR_WHITEWING); }
		Creature* GetVesara()       { return GetCreature(DATA_MAGISTRIX_VESARA); }

		#pragma endregion

		// Utils
		#pragma region UTILS

		// Portee en deca de laquelle GetNearestPlayer considere le joueur
		// comme etant "sur place".
		static constexpr float NEAREST_PLAYER_RADIUS = 10.0f;
		// Rayon de dispersion des joueurs lors d'un teleport groupe.
		static constexpr float TELEPORT_SPREAD_RADIUS = 4.5f;

		// Talk null-safe : un acteur despawne en cours de scene ne doit pas
		// faire tomber le serveur, la replique est simplement perdue.
		void Talk(Creature* creature, uint8 textId)
		{
			if (creature)
				creature->AI()->Talk(textId);
		}

		// Enchaine sur l'event suivant de la chaine. C'est ce +1 qui rend
		// l'ordre numerique des DLPEvents significatif.
		void Next(const Milliseconds& time)
		{
			eventId++;
			events.ScheduleEvent(eventId, time);
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

		// Premier joueur de l'instance s'il est a portee de la creature.
		Player* GetNearestPlayer(Creature* creature) const
		{
			Player* player = GetFirstPlayer();
			if (player && creature && player->IsWithinDist(creature, NEAREST_PLAYER_RADIUS, false))
				return player;
			return nullptr;
		}

        void ApplyHordeIllusion(Player* player, bool apply)
        {
            if (apply)
            {
                player->CastSpell(player, SPELL_HORDE_ILLUSION);
                player->CastSpell(player, SPELL_HORDE_ILLUSION_REACTIONS);
                player->CastSpell(player, SPELL_HORDE_ILLUSION_1);
            }
            else
            {
                player->RemoveAurasDueToSpell(SPELL_HORDE_ILLUSION);
                player->RemoveAurasDueToSpell(SPELL_HORDE_ILLUSION_REACTIONS);
                player->RemoveAurasDueToSpell(SPELL_HORDE_ILLUSION_1);
                player->RemoveAurasDueToSpell(SPELL_FLASHBACK_EFFECT);
            }
        }

		template <typename T>
		void DoOnCreatures(GuidVector const& guids, T&& fn)
		{
			for (ObjectGuid const& guid : guids)
			{
				if (Creature* creature = instance->GetCreature(guid))
					fn(creature);
			}
		}

		#pragma endregion
	};

	InstanceScript* GetInstanceScript(InstanceMap* map) const override
	{
		return new scenario_dalaran_purge_InstanceScript(map);
	}
};

void AddSC_scenario_dalaran_purge()
{
	new scenario_dalaran_purge();
}
