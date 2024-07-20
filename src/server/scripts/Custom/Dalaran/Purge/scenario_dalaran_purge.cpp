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
#include "dalaran_purge.h"

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
	{ NPC_ARCHMAGE_LANDALOCK,           DATA_ARCHMAGE_LANDALOCK         },
	{ NPC_MAGISTER_HATHOREL,            DATA_MAGISTER_HATHOREL          },
	{ NPC_MAGISTER_SURDIEL,             DATA_MAGISTER_SURDIEL           },
	{ NPC_ARCANIST_RATHAELLA,           DATA_ARCANIST_RATHAELLA         },
	{ NPC_HIGH_ARCANIST_SAVOR,          DATA_HIGH_ARCANIST_SAVOR        },
	{ NPC_GRAND_MAGISTER_ROMMATH,       DATA_GRAND_MAGISTER_ROMMATH     },
	{ NPC_NARASI_SNOWDAWN,              DATA_NARASI_SNOWDAWN            },
	{ NPC_MAGISTER_BRASAEL,             DATA_MAGISTER_BRASAEL           },
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

class scenario_dalaran_purge : public InstanceMapScript
{
	public:
	scenario_dalaran_purge() : InstanceMapScript(DLPScriptName, 5002)
	{
	}

	struct scenario_dalaran_purge_InstanceScript : public InstanceScript
	{
		scenario_dalaran_purge_InstanceScript(InstanceMap* map) : InstanceScript(map),
			eventId(1), phase(DLPPhases::FindJaina01), index(0)
		{
			SetHeaders(DataHeader);
			LoadObjectData(creatureData, gameobjectData);
		}

		void OnPlayerEnter(Player* player) override
		{
			player->CastSpell(player, SPELL_RAINY_WEATHER, true);

			DLPPhases phase = (DLPPhases)GetData(DATA_SCENARIO_PHASE);
			if (phase >= DLPPhases::FreeCitizens && phase < DLPPhases::RemainingSunreavers)
			{
				player->RemoveAurasDueToSpell(SPELL_WAND_OF_DISPELLING);
				player->CastSpell(player, SPELL_WAND_OF_DISPELLING, true);
			}
			else if (phase >= DLPPhases::TheEscape && phase < DLPPhases::InTheHandsOfTheChief)
			{
				player->CastSpell(player, SPELL_HORDE_ILLUSION, true);
				player->CastSpell(player, SPELL_FACTION_OVERRIDE, true);
				player->CastSpell(player, SPELL_FLASHBACK_EFFECT, true);
			}
		}

		void OnPlayerLeave(Player* player) override
		{
			player->RemoveAurasDueToSpell(SPELL_RAINY_WEATHER);
			player->RemoveAurasDueToSpell(SPELL_WAND_OF_DISPELLING);
			player->RemoveAurasDueToSpell(SPELL_HORDE_ILLUSION);
			player->RemoveAurasDueToSpell(SPELL_FACTION_OVERRIDE);
			player->RemoveAurasDueToSpell(SPELL_FLASHBACK_EFFECT);
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
					break;
				case EVENT_FIND_JAINA_02:
					#ifdef CUSTOM_DEBUG
						TriggerGameEvent(EVENT_FIND_JAINA_02);
					#endif
					SetData(DATA_SCENARIO_PHASE, (uint32)DLPPhases::FreeTheArcanist);
					events.ScheduleEvent(15, 2s);
					break;
				case EVENT_FREE_AETHAS_SUNREAVER:
					SetData(DATA_SCENARIO_PHASE, (uint32)DLPPhases::TheEscape_End);
					events.ScheduleEvent(37, 2s);
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
						events.ScheduleEvent(13, 1s);
					#else
						events.ScheduleEvent(1, 10s);
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

					DoOnCreatures(highmages, [this](Creature* creature)
					{
						creature->SetFaction(FACTION_FRIENDLY);
						creature->setActive(false);
						creature->SetVisible(false);
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
					if (Creature* landalock = GetCreature(DATA_ARCHMAGE_LANDALOCK))
					{
						landalock->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
						landalock->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
						landalock->CastSpell(landalock, SPELL_CHAT_BUBBLE, true);
					}
					break;
				}
				// An Unfortunate Capture
				case CRITERIA_TREE_UNFORTUNATE_CAPTURE:
				{
					DoRemoveAurasDueToSpellOnPlayers(SPELL_WAND_OF_DISPELLING);
					DoCastSpellOnPlayers(SPELL_WAND_OF_DISPELLING);
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
					DoRemoveAurasDueToSpellOnPlayers(SPELL_WAND_OF_DISPELLING);
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

					if (Creature* zuros = GetCreature(DATA_MAGE_COMMANDER_ZUROS))
                        zuros->SetVisible(false);

					if (Creature* rathaella = GetCreature(DATA_ARCANIST_RATHAELLA))
						rathaella->SetVisible(false);

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

					instance->DoOnPlayers([this](Player* player)
					{
						player->CastSpell(player, SPELL_FADING_TO_BLACK, true);
					});

					events.ScheduleEvent(21, 1s);
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
						player->RemoveAurasDueToSpell(SPELL_HORDE_ILLUSION);
						player->RemoveAurasDueToSpell(SPELL_FACTION_OVERRIDE);
						player->RemoveAurasDueToSpell(SPELL_FLASHBACK_EFFECT);
						player->RemoveAurasDueToSpell(SPELL_FOR_THE_HORDE);
					});

					break;
				}
                // What happened! - Find the Grand Magister Rommath
				case CRITERIA_TREE_FIND_ROMMATH:
                {
                    events.ScheduleEvent(23, 2s);
                    SetData(DATA_SCENARIO_PHASE, (uint32)DLPPhases::TheEscape_Escort);
                    break;
                }
				// What happened! - Follow the tracks
				case CRITERIA_TREE_FOLLOW_TRACKS:
                {
                    instance->SummonCreatureGroup(CREATURE_GROUP_PRISON, &prison);
                    if (Creature* rommath = GetRommath())
                    {
                        rommath->NearTeleportTo(RommathPos01);
                        rommath->SetHomePosition(RommathPos01);
                    }
                    events.ScheduleEvent(28, 2s);
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
			creature->SetBoundingRadius(80.f);

			switch (creature->GetEntry())
			{
				case NPC_MAGISTER_BRASAEL:
					creature->SetImmuneToAll(true);
					creature->SetSheath(SHEATH_STATE_UNARMED);
					creature->AddAura(SPELL_HOLD_BAG, creature);
					creature->SetEmoteState(EMOTE_STATE_LOOT_BITE_SOUND);
					break;
				case NPC_DALARAN_CITIZEN:
					if (roll_chance_i(30))
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
					sunreavers.push_back(creature->GetGUID());
					break;
				case NPC_ARCANE_BARRIER:
					barriers.push_back(creature->GetGUID());
					break;
				case NPC_SUNREAVER_EXTRACTION_TROOP:
					FeingDeath(creature);
					if (roll_chance_i(50))
						creature->AddAura(RAND(SPELL_FROZEN_SOLID, SPELL_BURNING), creature);
					extraction.push_back(creature->GetGUID());
					break;
				default:
					break;
			}
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

				case 1:
                    if (Creature* jaina = GetJaina())
                    {
                        jaina->SetAIAnimKitId(18213);
                        Talk(jaina, SAY_PURGE_JAINA_01);
                    }
					Next(2s);
					break;
				case 2:
					Talk(GetJaina(), SAY_PURGE_JAINA_02);
					Next(6s);
					break;
				case 3:
					Talk(GetAethas(), SAY_PURGE_AETHAS_03);
					Next(5s);
					break;
				case 4:
					Talk(GetJaina(), SAY_PURGE_JAINA_04);
					Next(5s);
					break;
				case 5:
					Talk(GetAethas(), SAY_PURGE_AETHAS_05);
					Next(3s);
					break;
				case 6:
					Talk(GetJaina(), SAY_PURGE_JAINA_06);
					Next(5s);
					break;
				case 7:
					Talk(GetJaina(), SAY_PURGE_JAINA_07);
					if (Creature* aethas = GetAethas())
						GetElemental()->CastSpell(GetAethas(), SPELL_FROSTBOLT);
					Next(2s);
					break;
				case 8:
					DoOnCreatures(highmages, [this](Creature* creature)
					{
						creature->SetSpeedRate(MOVE_RUN, 0.8f);
						creature->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_NONE, GetJaina(), 1.6f);
					});
					Next(760ms);
					break;
				case 9:
					GetJaina()->CastSpell(GetJaina(), SPELL_ARCANE_BOMBARDMENT);
					Next(630ms);
					break;
				case 10:
					DoOnCreatures(highmages, [this](Creature* creature)
					{
						creature->KillSelf();
					});
					Next(1s);
					break;
				case 11:
					if (Creature* jaina = GetJaina())
					{
						jaina->SetWalk(true);
						jaina->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_NONE, GetAethas(), 3.6f);
					}
					Next(6s);
					break;
				case 12:
					GetJaina()->CastSpell(GetJaina(), SPELL_DISSOLVE);
					GetJaina()->CastSpell(GetJaina(), SPELL_TELEPORT, true);
					GetAethas()->CastSpell(GetAethas(), SPELL_TELEPORT, true);
					Next(1s);
					break;
				case 13:
					GetJaina()->SetVisible(false);
					GetElemental()->SetVisible(false);
					GetAethas()->SetVisible(false);
					DoOnCreatures(highmages, [this](Creature* creature)
					{
						creature->SetVisible(false);
					});
					TriggerGameEvent(EVENT_ASSIST_JAINA);
					break;
				// DELETED
				case 14:
					break;

				#pragma endregion

				// First Step
				#pragma region FIRST_STEP

				case 15:
					GetVereesa()->SetFacingToObject(GetJaina());
					GetJaina()->SetFacingToObject(GetVereesa());
					Next(2s);
					break;
				case 16:
					Talk(GetVereesa(), SAY_FIRST_STEP_VEREESA_01);
					Next(5s);
					break;
				case 17:
					Talk(GetJaina(), SAY_FIRST_STEP_JAINA_02);
					Next(4s);
					break;
				case 18:
					if (Creature* jaina = GetJaina())
					{
						Talk(jaina, SAY_FIRST_STEP_JAINA_03);
						if (Player* player = GetNearestPlayer(jaina))
							jaina->SetFacingToObject(player);
					}
					Next(2s);
					break;
				case 19:
					if (Creature* vereesa = GetVereesa())
					{
						Talk(vereesa, SAY_FIRST_STEP_VEREESA_04);
						if (Player* player = GetNearestPlayer(vereesa))
							vereesa->SetFacingToObject(player);
					}
					Next(2s);
					break;
				case 20:
					TriggerGameEvent(EVENT_FIND_JAINA_02);
					break;

				#pragma endregion

				// What happened! - Events
				#pragma region WHAT_HAPPENED_EVENTS

				case 21:
					instance->DoOnPlayers([this](Player* player)
					{
						player->NearTeleportTo(SewersPos01);
						player->CastSpell(player, SPELL_FLASHBACK_EFFECT, true);
                        for (uint8 i = 0; i < 20; i++)
                            player->CastSpell(player, SPELL_FOR_THE_HORDE);
					});
					if (Creature* surdiel = GetCreature(DATA_MAGISTER_SURDIEL))
						surdiel->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, SurdielPos02, true, SurdielPos02.GetOrientation());
					Next(1s);
					break;
				case 22:
					DoOnCreatures(extraction, [this](Creature* creature)
					{
						creature->SetVisible(false);
					});
					instance->DoOnPlayers([this](Player* player)
					{
						player->CombatStop();
						player->CastSpell(player, SPELL_HORDE_ILLUSION);
						player->CastSpell(player, SPELL_FACTION_OVERRIDE);
					});
					break;

				#pragma endregion

				// What happened! - Escort
				#pragma region WHAT_HAPPENED_ESCORT

				case 23:
					Talk(GetRommath(), SAY_INFILTRATE_ROMMATH_01);
					Next(12s);
					break;
				case 24:
					Talk(GetRommath(), SAY_INFILTRATE_ROMMATH_02);
					Next(5s);
					break;
				case 25:
					GetRommath()->GetMotionMaster()->MovePath(RommathPath01, false);
					Next(15s);
					break;
				case 26:
					Talk(GetRommath(), SAY_INFILTRATE_ROMMATH_03);
					Next(15s);
					break;
				case 27:
					if (Creature* rommath = GetRommath())
					{
						if (GameObject* portal = GetGameObject(DATA_PORTAL_TO_PRISON))
						{
							if (rommath->IsWithinDist(portal, 35.0f))
							{
                                events.CancelEvent(27);

                                if (TempSummon* dummy = portal->SummonCreature(WORLD_TRIGGER, portal->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, 10s))
                                {
                                    rommath->SetOwnerGUID(ObjectGuid::Empty);
                                    rommath->GetMotionMaster()->Clear();
                                    rommath->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_02, dummy, 5.0f);
                                }
							}
							else
							{
								events.RescheduleEvent(27, 2s);
							}
						}
					}
					break;

				#pragma endregion

				// What happened! - Prison
				#pragma region WHAT_HAPPENED_PRISON

				case 28:
                    if (CheckWipe())
                    {
                        if (Creature* aethas = GetAethas())
                        {
                            aethas->RemoveAllAuras();
                            aethas->SetImmuneToAll(true);
                            aethas->SetVisible(true);
                            aethas->NearTeleportTo(AethasPos02);
                            aethas->SetHomePosition(AethasPos02);
                        }

                        Next(2s);
                    }
					break;
				case 29:
					if (Creature* aethas = GetAethas())
					{
						aethas->AddAura(SPELL_ICY_GLARE, aethas);
						aethas->AddAura(SPELL_CHILLING_BLAST, aethas);
					}
					Talk(GetRommath(), SAY_INFILTRATE_ROMMATH_05);
					Next(8s);
					break;
				case 30:
					if (Creature* rommath = GetRommath())
					{
						Talk(rommath, SAY_INFILTRATE_ROMMATH_08);
						rommath->CastSpell(rommath, SPELL_TELEPORT_CASTER);
					}
					Next(5s);
					break;
				case 31:
					if (Creature* rommath = GetRommath())
					{
						rommath->NearTeleportTo(GuardianPos01);
						rommath->SetHomePosition(GuardianPos01);
						instance->DoOnPlayers([this](Player* player)
						{
                            const Position dest = GetRandomPosition(GuardianPos01, 4.5f);
                            player->CastSpell(dest, SPELL_TELEPORT);
						});
					}
					if (Creature* hathorel = GetCreature(DATA_MAGISTER_HATHOREL))
						hathorel->NearTeleportTo(HathorelPos01);
					Next(2s);
					break;
				case 32:
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
				case 33:
					if (Creature* rommath = GetRommath())
					{
						if (Creature* narasi = GetCreature(DATA_NARASI_SNOWDAWN))
						{
							if (rommath->IsWithinDist(narasi, 45.0f))
							{
                                events.CancelEvent(33);

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
								events.RescheduleEvent(33, 2s);
							}
						}
					}
					break;
				case 34:
                    if (Creature* narasi = GetCreature(DATA_NARASI_SNOWDAWN))
                    {
                        Talk(narasi, SAY_INFILTRATE_NARASI_01);
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
				case 35:
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
                case 36:
                    if (Creature* rommath = GetRommath())
                    {
                        Talk(rommath, SAY_INFILTRATE_ROMMATH_08);
                        rommath->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, RommathPos03);
                    }
                    break;

				#pragma endregion

				// What happened! - Escape
				#pragma region WHAT_HAPPENED_PRISON

				case 37:
					if (Creature* surdiel = GetCreature(DATA_MAGISTER_SURDIEL))
						Talk(surdiel, SAY_INFILTRATE_SURDIEL_03);
                    Next(2s);
                    break;
				case 38:
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
				case 39:
					if (Creature* narasi = GetCreature(DATA_NARASI_SNOWDAWN))
					{
						Talk(narasi, SAY_INFILTRATE_NARASI_05);
						if (Creature* surdiel = GetCreature(DATA_MAGISTER_SURDIEL))
							narasi->CastSpell(surdiel, SPELL_ARCANE_IMPRISONMENT);
					}
					Next(1s);
					break;
				case 40:
					if (Creature* hathorel = GetCreature(DATA_MAGISTER_HATHOREL))
						Talk(hathorel, SAY_INFILTRATE_HATHOREL_06);
                    if (Creature* jaina = instance->SummonCreature(NPC_JAINA_PROUDMOORE, RommathPos02))
                    {
                        jaina->SetImmuneToAll(true);
                        jaina->SetWalk(true);
                        jaina->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, RommathPos03);
                        jaina->DespawnOrUnsummon(11s);
                    }
                    Next(2s);
					break;
				case 41:
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

        bool CheckWipe()
        {
            uint8 counter = 0;
            for (TempSummon* summon : prison)
            {
                if (!summon || summon->isDead())
                    counter++;
            }

            if (counter >= prison.size())
            {
                events.CancelEvent(28);
                return true;
            }

            events.RescheduleEvent(28, 500ms);
            return false;
        }

		EventMap events;
		uint32 eventId;
		uint8 index;
		DLPPhases phase;

		GuidVector highmages;
        GuidVector patrol;
        GuidVector citizens;
        GuidVector sunreavers;
        GuidVector extraction;
        GuidVector barriers;

		std::list<TempSummon*> prison;

		ObjectGuid endPortal;

		// Accesseurs
		#pragma region ACCESSORS
		
		Creature* GetJaina() { return GetCreature(DATA_JAINA_PROUDMOORE); }
		Creature* GetVereesa() { return GetCreature(DATA_VEREESA_WINDRUNNER); }
		Creature* GetAethas() { return GetCreature(DATA_AETHAS_SUNREAVER); }
		Creature* GetElemental() { return GetCreature(DATA_SUMMONED_WATER_ELEMENTAL); }
		Creature* GetRommath() { return GetCreature(DATA_GRAND_MAGISTER_ROMMATH); }

		#pragma endregion

		// Utils
		#pragma region UTILS

		void Talk(Creature* creature, uint8 id)
		{
			creature->AI()->Talk(id);
		}

		void Next(const Milliseconds& time)
		{
			eventId++;
			events.ScheduleEvent(eventId, time);
		}

		Player* GetNearestPlayer(Creature* creature)
		{
			Player* player = instance->GetPlayers().begin()->GetSource();
			if (player && player->IsWithinDist(creature, 5.f, false))
				return player;
			return nullptr;
		}

		void FeingDeath(Creature* creature)
		{
			creature->RemoveAllAuras();
			creature->SetRegenerateHealth(false);
			creature->SetHealth(0U);
			creature->SetStandState(UNIT_STAND_STATE_DEAD);
			creature->SetUnitFlag2(UNIT_FLAG2_FEIGN_DEATH);
			creature->SetUnitFlag2(UNIT_FLAG2_PLAY_DEATH_ANIM);
			creature->SetImmuneToAll(true);
		}

		template <typename T>
		void DoOnCreatures(std::vector<ObjectGuid> guids, T&& fn)
		{
			for (ObjectGuid guid : guids)
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
