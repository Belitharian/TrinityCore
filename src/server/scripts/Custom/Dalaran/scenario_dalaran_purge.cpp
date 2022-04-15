#include "CriteriaHandler.h"
#include "CreatureGroups.h"
#include "EventMap.h"
#include "GameObject.h"
#include "GameTime.h"
#include "InstanceScript.h"
#include "KillRewarder.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectMgr.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
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
	{ 0,                                0                               }   // END
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

		enum Spells
		{
			SPELL_RAINY_WEATHER         = 296026,
			SPELL_FLASHBACK_EFFECT      = 279486,
		};

		void OnPlayerEnter(Player* player) override
		{
			player->CastSpell(player, SPELL_RAINY_WEATHER, true);
		}

		void OnPlayerLeave(Player* player) override
		{
			player->RemoveAurasDueToSpell(SPELL_RAINY_WEATHER);
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
						DoSendScenarioEvent(EVENT_FIND_JAINA_02);
					#endif
					SetData(DATA_SCENARIO_PHASE, (uint32)DLPPhases::FreeTheArcanist);
					events.ScheduleEvent(15, 2s);
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
					Talk(GetJaina(), SAY_SAVOR_JAINA_01);
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
					}
					if (Creature* narasi = GetCreature(DATA_NARASI_SNOWDAWN))
					{
						if (Creature* surdiel = GetCreature(DATA_MAGISTER_SURDIEL))
							narasi->CastSpell(surdiel, SPELL_ARCANE_IMPRISONMENT);
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)DLPPhases::TheEscape);
					break;
				}
				// The Final Assault - Speak to Jaina Proudmoore
				case CRITERIA_TREE_SPEAK_TO_JAINA:
				{
					if (Creature* jaina = GetJaina())
					{
						jaina->SetVisible(false);

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

					if (Creature* zuros = GetCreature(DATA_MAGE_COMMANDER_ZUROS))
						zuros->NearTeleportTo(ZurosPos01);

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
						rommath->CastSpell(rommath, SPELL_COSMETIC_YELLOW_ARROW);

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
				// The Final Assault - Find the Grand Magister Rommath
				case CRITERIA_TREE_FIND_ROMMATH:
					events.ScheduleEvent(23, 2s);
					SetData(DATA_SCENARIO_PHASE, (uint32)DLPPhases::TheEscape_Escort);
					break;
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
				case NPC_NARASI_SNOWDAWN:
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
					go->SetFlag(GO_FLAG_IN_USE | GO_FLAG_NOT_SELECTABLE | GO_FLAG_LOCKED);
					break;
				case GOB_LAMP_POST:
					go->SetLootState(GO_READY);
					go->UseDoorOrButton();
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
					Talk(GetJaina(), SAY_PURGE_JAINA_01);
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
					DoSendScenarioEvent(EVENT_ASSIST_JAINA);
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
					DoSendScenarioEvent(EVENT_FIND_JAINA_02);
					break;

				#pragma endregion

				// The Escape - Events
				#pragma region THE_ESCAPE_EVENTS

				case 21:
					instance->DoOnPlayers([this](Player* player)
					{
						player->NearTeleportTo(SewersPos01);
						player->CastSpell(player, SPELL_FLASHBACK_EFFECT, true);
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

				// The Escape - Escort
				#pragma region THE_ESCAPE_ESCORT

				case 23:
					Talk(GetRommath(), SAY_INFILTRATE_ROMMATH_01);
					Next(12s);
					break;
				case 24:
					Talk(GetRommath(), SAY_INFILTRATE_ROMMATH_02);
					Next(5s);
					break;
				case 25:
					GetRommath()->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_01, RommathPath01, ROMMATH_PATH_01);
					break;

				#pragma endregion

				default:
					break;
			}
		}

		EventMap events;
		uint32 eventId;
		uint8 index;
		DLPPhases phase;

		std::vector<ObjectGuid> highmages;
		std::vector<ObjectGuid> patrol;
		std::vector<ObjectGuid> citizens;
		std::vector<ObjectGuid> sunreavers;
		std::vector<ObjectGuid> extraction;
		std::vector<ObjectGuid> barriers;

		// Accesseurs
		#pragma region ACCESSORS
		
		Creature* GetJaina() { return GetCreature(DATA_JAINA_PROUDMOORE); }
		Creature* GetVereesa() { return GetCreature(DATA_VEREESA_WINDRUNNER); }
		Creature* GetAethas() { return GetCreature(DATA_AETHAS_SUNREAVER); }
		Creature* GetElemental() { return GetCreature(DATA_SUMMONED_WATER_ELEMENTAL); }
		Creature* GetRommath() { return GetCreature(DATA_GRAND_MAGISTER_ROMMATH); }
		Creature* GetRathaella() { return GetCreature(DATA_ARCANIST_RATHAELLA); }

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
