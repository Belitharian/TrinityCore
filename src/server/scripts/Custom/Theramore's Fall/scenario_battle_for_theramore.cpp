#include "CriteriaHandler.h"
#include "EventMap.h"
#include "Containers.h"
#include "GameObject.h"
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
#include "battle_for_theramore.h"

#define EVENT_CREATURE_DATA_SIZE 14

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

	// NPCs don't serve events
	{ NPC_KALECGOS_DRAGON,      DATA_KALECGOS_DRAGON        },
	{ NPC_ADMIRAL_AUBREY,       DATA_ADMIRAL_AUBREY         },
	{ NPC_CAPTAIN_DROK,         DATA_CAPTAIN_DROK           },
	{ NPC_WAVE_CALLER_GRUHTA,   DATA_WAVE_CALLER_GRUHTA     },
	{ 0,                        0                           }   // END
};

const ObjectData gameobjectData[] =
{
	{ GOB_PORTAL_TO_STORMWIND,  DATA_PORTAL_TO_STORMWIND    },
	{ GOB_PORTAL_TO_DALARAN,    DATA_PORTAL_TO_DALARAN      },
	{ GOB_PORTAL_TO_ORGRIMMAR,  DATA_PORTAL_TO_ORGRIMMAR    },
	{ GOB_MYSTIC_BARRIER_01,    DATA_MYSTIC_BARRIER_01      },
	{ GOB_MYSTIC_BARRIER_02,    DATA_MYSTIC_BARRIER_02      },
	{ GOB_ENERGY_BARRIER,       DATA_ENERGY_BARRIER         },
	{ GOB_POWDER_BARREL,        DATA_POWDER_BARREL          },
	{ 0,                        0                           }   // END
};

class KalecgosSpellEvent : public BasicEvent
{
	public:
	KalecgosSpellEvent(Creature* owner) : owner(owner)
	{
		owner->SetReactState(REACT_PASSIVE);
	}

	bool Execute(uint64 timer, uint32 /*updateTime*/) override
	{
		if (roll_chance_i(20))
			owner->AI()->Talk(SAY_KALECGOS_SPELL_01);

		owner->CastSpell(owner, SPELL_FROST_BREATH);
		owner->m_Events.AddEvent(this, Milliseconds(timer) + randtime(8s, 10s));
		return false;
	}

	private:
	Creature* owner;
};

class KalecgosLoopEvent : public BasicEvent
{
	public:
	KalecgosLoopEvent(Creature* owner) : owner(owner), m_loopTime(0.f)
	{
		owner->SetCanFly(true);
		owner->SetDisableGravity(true);
		owner->SetSpeed(MOVE_RUN, 25.f);

		float perimeter = 2.f * float(M_PI) * m_circleRadius;
		m_loopTime = (perimeter / owner->GetSpeed(MOVE_RUN)) * 1000.f;
	}

	bool Execute(uint64 timer, uint32 /*updateTime*/) override
	{
		owner->GetMotionMaster()->MoveCirclePath(TheramorePoint01.GetPositionX(), TheramorePoint01.GetPositionY(), TheramorePoint01.GetPositionZ(), m_circleRadius, true, 16);
		owner->m_Events.AddEvent(this, Milliseconds(timer + m_loopTime));
		return false;
	}

	private:
	const float m_circleRadius = 95.0f;
	Creature* owner;
	uint64 m_loopTime;
};

class scenario_battle_for_theramore : public InstanceMapScript
{
	public:
	scenario_battle_for_theramore() : InstanceMapScript(BFTScriptName, 5000)
	{
	}

	struct scenario_battle_for_theramore_InstanceScript : public InstanceScript
	{
		scenario_battle_for_theramore_InstanceScript(InstanceMap* map) : InstanceScript(map),
			phase(BFTPhases::FindJaina), eventId(1), archmagesIndex(0), waves(0), woundedTroops(0),
			wavesInvoker(WAVE_01)
		{
			SetHeaders(DataHeader);
			LoadObjectData(creatureData, gameobjectData);
		}

		enum Invokers
		{
			// Waves
			WAVE_01                     = 101,
			WAVE_01_CHECK,
			WAVE_02,
			WAVE_02_CHECK,
			WAVE_03,
			WAVE_03_CHECK,
			WAVE_04,
			WAVE_04_CHECK,
			WAVE_05,
			WAVE_05_CHECK,
			WAVE_06,
			WAVE_06_CHECK,
			WAVE_07,
			WAVE_07_CHECK,
			WAVE_08,
			WAVE_08_CHECK,
			WAVE_09,
			WAVE_09_CHECK,
			WAVE_10,
			WAVE_10_CHECK,

			WAVE_BREAKER,
			WAVE_INTRO,
		};

		enum Spells
		{
			SPELL_WATER_BUCKET          = 42336,
			SPELL_MASS_TELEPORT         = 60516,
			SPELL_MAGIC_QUILL           = 171980,
			SPELL_TIED_UP               = 167469,
			SPELL_CLOSE_PORTAL          = 203542,
			SPELL_DISSOLVE              = 255295,
			SPELL_PRISMATIC_BARRIER     = 235450,
			SPELL_METEOR                = 276973,
			SPELL_ARCANE_CANALISATION   = 288451,
			SPELL_BLAZING_BARRIER       = 295238,
			SPELL_FROST_BREATH          = 300548,
			SPELL_CHILLING_BLAST        = 337053,
			SPELL_ICY_GLARE             = 338517,
			SPELL_VANISH                = 342048,
			SPELL_BIG_EXPLOSION         = 348750,
			SPELL_TELEPORT              = 357601,
			SPELL_SCORCHED_EARTH        = 373139,
			SPELL_ARCANIC_CELL          = 398947,
			SPELL_READING_BOOK_STANDING = 397765,
		};

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
				return Waves[waves];
			return 0;
		}

		void OnPlayerEnter(Player* player) override
		{
			player->AddAura(SPELL_SKYBOX_EFFECT, player);
		}

		void OnPlayerLeave(Player* player) override
		{
			player->RemoveAurasDueToSpell(SPELL_SKYBOX_EFFECT);
		}

		void SetData(uint32 dataId, uint32 value) override
		{
			if (dataId == DATA_SCENARIO_PHASE)
				phase = (BFTPhases)value;
			else if (dataId == DATA_WOUNDED_TROOPS)
				woundedTroops = value;
		}

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
					GetKalec()->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					if (Creature* jaina = GetCreature(DATA_JAINA_PROUDMOORE))
					{
						jaina->SetSheath(SHEATH_STATE_UNARMED);
						jaina->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
						Talk(jaina, SAY_REUNION_1);
						SetTarget(jaina);
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::TheCouncil);
					events.ScheduleEvent(1, 2s);
					break;
				}
				// Step 2 : The Council
				case CRITERIA_TREE_THE_COUNCIL:
				{
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::Waiting);
					events.ScheduleEvent(24, 10s);
					break;
				}
				// Step 3 : Waiting
				case CRITERIA_TREE_WAITING:
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::UnknownTauren);
					events.ScheduleEvent(25, 1s);
					break;
				// Step 4 : The Unknow Tauren
				case CRITERIA_TREE_UNKNOW_TAUREN:
				{
					for (ObjectGuid guid : citizens)
					{
						if (Creature* citizen = instance->GetCreature(guid))
						{
							if (Creature* faithful = citizen->FindNearestCreature(NPC_THERAMORE_FAITHFUL, 10.f))
								continue;
							if (citizen->GetEntry() == NPC_ALLIANCE_PEASANT)
								continue;
							citizen->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
						}
					}
					if (Creature* kinndy = GetKinndy())
					{
						kinndy->SetVisible(true);
						kinndy->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_02, KinndyPath02, KINNDY_PATH_02, true);
					}
					if (Creature* tervosh = GetTervosh())
					{
						tervosh->SetVisible(true);
						tervosh->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_03, TervoshPath03, TERVOSH_PATH_03, true);
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::Evacuation);
					break;
				}
				// Step 5 : Evacuation
				case CRITERIA_TREE_EVACUATION:
				{
					if (Creature* jaina = GetJaina())
					{
						for (ObjectGuid guid : citizens)
						{
							if (Creature* citizen = instance->GetCreature(guid))
								citizen->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
						}

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
					if (Creature* kalecgos = GetKalec())
					{
						kalecgos->GetMotionMaster()->Clear();
						kalecgos->GetMotionMaster()->MoveIdle();
						kalecgos->NearTeleportTo(KalecgosPoint01);
						kalecgos->SetHomePosition(KalecgosPoint01);
						kalecgos->RemoveUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
						kalecgos->RemoveAllAuras();
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::ALittleHelp);
					break;
				}
				// Step 6 : A Little Help
				case CRITERIA_TREE_A_LITTLE_HELP:
				{
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
							tank->SetRegenerateHealth(false);
							tank->SetHealth((float)tank->GetHealth() * frand(0.15f, 0.60f));
						}
					}
					for (ObjectGuid guid : citizens)
					{
						if (Creature* citizen = instance->GetCreature(guid))
						{
							citizen->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
							citizen->SetVisible(false);
						}
					}
					for (ObjectGuid guid : troops)
					{
						if (Creature* troop = instance->GetCreature(guid))
						{
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
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::Preparation);
					#ifndef CUSTOM_DEBUG
						events.ScheduleEvent(71, 1s);
					#else
						for (uint8 i = 0; i < ARCHMAGES_LOCATION; i++)
							instance->SummonCreature(archmagesLocation[i].dataId, PortalPoint01);
						events.ScheduleEvent(90, 2s);
					#endif
					break;
				}
				// Step 7 : Preparation - Parent
				case CRITERIA_TREE_PREPARATION:
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::TheBattle);
					break;
				// Step 7 : Preparation - Speak with Rhonin
				case CRITERIA_TREE_TALK_TO_RHONIN:
					EnsurePlayerHaveShield();
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
						jaina->SetBoundingRadius(20.f);
						jaina->SetRegenerateHealth(false);
						jaina->SummonGameObject(GOB_PORTAL_TO_ORGRIMMAR, PortalPoint02, QuaternionData::QuaternionData(), 0s);
					}
					if (Creature* rhonin = GetRhonin())
					{
						SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, rhonin);

						rhonin->SetRegenerateHealth(false);
					}
					if (Creature* kalecgos = GetKalecgos())
					{
						SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, kalecgos);

						kalecgos->SetRegenerateHealth(false);
					}
					if (Creature* drok = GetDrok())
					{
						drok->SetVisible(true);
					}
					if (Creature* gruhta = GetGruhta())
					{
						gruhta->SetVisible(true);
					}
					for (uint8 i = 0; i < tanks.size() - 1; i++)
					{
						if (Creature* tank = instance->GetCreature(tanks[i]))
						{
							tank->KillSelf();
						}
					}
                    instance->DoOnPlayers([this](Player* player)
                    {
                        for (uint8 i = 0; i < 20; i++)
                            player->CastSpell(player, SPELL_FOR_THE_ALLIANCE);
                    });
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::TheBattle_Survive);
					events.ScheduleEvent(91, 10s);
					break;
				}
				// Step 9 : The Battle - Parent
				case CRITERIA_TREE_SURVIVE_THE_BATTLE:
				{
					SpawnWoundedTroops();
					EnsurePlayerHaveBucket();
					RelocateTroops();
					GetBarrier01()->ResetDoorOrButton();
					GetBarrier02()->ResetDoorOrButton();
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::HelpTheWounded);
					events.ScheduleEvent(122, 3s);
					break;
				}
				// Step 10 : Help the wounded - Parent
				case CRITERIA_TREE_HELP_THE_WOUNDED:
				{
					for (uint8 i = 128; i < 141; i++)
						events.CancelEvent(i);
					DoRemoveAurasDueToSpellOnPlayers(SPELL_RUNIC_SHIELD);
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::WaitForAmara);
					events.ScheduleEvent(141, 5ms);
					break;
				}
				// Step 10 : Help the wounded - Rejoin Lady Jaina Proudmoore after the attack
				case CRITERIA_TREE_FOLLOW_JAINA:
					events.ScheduleEvent(128, 3s);
					break;
				// Step 10 : Help the wounded - Help teleporting the wounded troops
				case CRITERIA_TREE_HELP_THE_TROOPS:
				{
					std::list<Creature*> results;
					if (Creature* jaina = GetJaina())
					{
						jaina->GetCreatureListWithEntryInGrid(results, NPC_THERAMORE_WOUNDED_TROOP, SIZE_OF_GRIDS);
						if (results.empty())
							return;

						for (Creature* c : results)
							c->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
					}
					break;
				}
				// Step 10 : Help the wounded - Extinguish the fires
				case CRITERIA_TREE_EXTINGUISH_FIRES:
					MassDespawn(NPC_THERAMORE_FIRE_CREDIT);
					DoRemoveAurasDueToSpellOnPlayers(SPELL_WATER_BUCKET);
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::HelpTheWounded_Extinguish);
					break;
				// Step 11 : Wait for Archmage Leeson returns - Parent
				case CRITERIA_TREE_WAIT_ARCHMAGE_LEESON:
					break;
				// Step 11 : Wait for Archmage Leeson returns - Rejoin Lady Jaina Proudmoore
				case CRITERIA_TREE_JOIN_JAINA:
				{
					if (Creature* kalecgos = GetKalec())
					{
						kalecgos->NearTeleportTo(KalecgosPath02[0]);
						kalecgos->SetSpeedRate(MOVE_WALK, 0.85f);
						kalecgos->SetSpeedRate(MOVE_RUN, 0.85f);
					}
					if (Creature* rhonin = GetRhonin())
					{
						rhonin->SetSpeedRate(MOVE_RUN, 0.85f);
						rhonin->SetSpeedRate(MOVE_WALK, 0.85f);
						rhonin->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, RhoninPoint01, true, RhoninPoint01.GetOrientation());
					}
					for (uint8 i = 0; i < EVENT_CREATURE_DATA_SIZE; i++)
					{
						if (Creature* creature = GetCreature(creatureData[i].type))
							creature->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::WaitForAmara_JoinJaina);
					events.ScheduleEvent(142, 500ms);
					break;
				}
				// Step 11 : Wait for Archmage Leeson returns - Wait for Archmage Leeson returns
				case CRITERIA_TREE_ARCHMAGE_LEESON:
					events.ScheduleEvent(156, 1s);
					break;
				// Step 12 : Retrieve Rhonin - Parent
				case CRITERIA_TREE_RETRIEVE_RHONIN:
					DoCastSpellOnPlayers(SPELL_THERAMORE_EXPLOSION_SCENE);
					break;
				// Step 12 : Retrieve Rhonin - Retrieve Rhonin at the top of the tower
				case CRITERIA_TREE_RETRIEVE:
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::RetrieveRhonin_JoinRhonin);
					events.ScheduleEvent(161, 1s);
					break;
				// Step 12 : Retrieve Rhonin - Localize the bomb
				case CRITERIA_TREE_REDUCE_EXPLOSION:
					EnsurePlayersAreInPhase(PHASE_THERAMORE_SCENE_EXPLOSION);
					break;
			}
		}

		void OnCreatureCreate(Creature* creature) override
		{
			InstanceScript::OnCreatureCreate(creature);

			creature->SetVisibilityDistanceOverride(VisibilityDistanceType::Gigantic);
			creature->SetPvpFlag(UNIT_BYTE2_FLAG_PVP);
			creature->SetUnitFlag(UNIT_FLAG_PVP_ENABLING);

			switch (creature->GetEntry())
			{
				case NPC_ARCHMAGE_TERVOSH:
					creature->SetEmoteState(EMOTE_STATE_READ_BOOK_AND_TALK);
					break;
				case NPC_THERAMORE_CITIZEN_FEMALE:
				case NPC_THERAMORE_CITIZEN_MALE:
				case NPC_TRAINING_DUMMY:
					creature->RemovePvpFlag(UNIT_BYTE2_FLAG_PVP);
					creature->RemoveUnitFlag(UNIT_FLAG_PVP_ENABLING);
					citizens.push_back(creature->GetGUID());
					if (phase == BFTPhases::Evacuation)
						break;
					creature->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
					break;
				case NPC_BISHOP_DELAVEY:
					creature->SetImmuneToNPC(true);
					creature->CastSpell(creature, SPELL_READING_BOOK_STANDING);
					citizens.push_back(creature->GetGUID());
					break;
				case NPC_ALLIANCE_PEASANT:
					creature->RemovePvpFlag(UNIT_BYTE2_FLAG_PVP);
					creature->RemoveUnitFlag(UNIT_FLAG_PVP_ENABLING);
					creature->SetImmuneToNPC(true);
					citizens.push_back(creature->GetGUID());
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
                    if (creature->GetWaypointPath() || creature->IsFormationLeader() || creature->GetFormation())
                        break;
					troops.push_back(creature->GetGUID());
					break;
				case NPC_ADMIRAL_AUBREY:
				case NPC_CAPTAIN_DROK:
				case NPC_WAVE_CALLER_GRUHTA:
					creature->SetVisible(false);
					break;
			}
		}

		void OnGameObjectCreate(GameObject* go) override
		{
			InstanceScript::OnGameObjectCreate(go);

			go->SetVisibilityDistanceOverride(VisibilityDistanceType::Gigantic);

			switch (go->GetEntry())
			{
				case GOB_PORTAL_TO_DALARAN:
				case GOB_PORTAL_TO_STORMWIND:
				case GOB_PORTAL_TO_ORGRIMMAR:
					go->SetLootState(GO_READY);
					go->UseDoorOrButton();
					go->SetFlag(GO_FLAG_NOT_SELECTABLE);
					break;
				case GOB_ENERGY_BARRIER:
					go->SetFlag(GO_FLAG_NOT_SELECTABLE);
					break;
				default:
					break;
			}
		}

		void Update(uint32 diff) override
		{
			scheduler.Update(diff);

			events.Update(diff);
			switch (eventId = events.ExecuteEvent())
			{
				// The Council
				#pragma region THE_COUNCIL

				case 1:
					if (Creature* tervosh = GetTervosh())
					{
						tervosh->SetEmoteState(EMOTE_STAND_STATE_NONE);
						tervosh->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_01, TervoshPath01, TERVOSH_PATH_01, true);
					}
					Next(5s);
					break;
				case 2:
					if (Creature* kinndy = GetKinndy())
					{
						kinndy->SetWalk(true);
						kinndy->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, KinndyPoint01, true, 1.09f);
					}
					#ifdef CUSTOM_DEBUG
						events.ScheduleEvent(20, 2s);
					#else
						Next(6s);
					#endif
					break;
				case 3:
					Talk(GetKinndy(), SAY_REUNION_2);
					SetTarget(GetKinndy());
					Next(11s);
					break;
				case 4:
					Talk(GetJaina(), SAY_REUNION_3);
					SetTarget(GetJaina());
					Next(12s);
					break;
				case 5:
					Talk(GetKinndy(), SAY_REUNION_4);
					SetTarget(GetKinndy());
					Next(6s);
					break;
				case 6:
					Talk(GetJaina(), SAY_REUNION_5);
					SetTarget(GetJaina());
					Next(8s);
					break;
				case 7:
					Talk(GetTervosh(), SAY_REUNION_6);
					SetTarget(GetTervosh());
					Next(8s);
					break;
				case 8:
					Talk(GetKalec(), SAY_REUNION_7);
					SetTarget(GetKalec());
					Next(6s);
					break;
				case 9:
					Talk(GetKalec(), SAY_REUNION_8);
					Next(9s);
					break;
				case 10:
					Talk(GetTervosh(), SAY_REUNION_9);
					Next(1s);
					break;
				case 11:
					Talk(GetKinndy(), SAY_REUNION_9_BIS);
					Next(4s);
					break;
				case 12:
					Talk(GetJaina(), SAY_REUNION_10);
					SetTarget(GetJaina());
					Next(6s);
					break;
				case 13:
					Talk(GetKalec(), SAY_REUNION_11);
					SetTarget(GetKalec());
					Next(4s);
					break;
				case 14:
					Talk(GetJaina(), SAY_REUNION_12);
					SetTarget(GetJaina());
					Next(6s);
					break;
				case 15:
					Talk(GetKinndy(), SAY_REUNION_13);
					SetTarget(GetKinndy());
					Next(6s);
					break;
				case 16:
					Talk(GetKalec(), SAY_REUNION_14);
					SetTarget(GetKalec());
					Next(7s);
					break;
				case 17:
					Talk(GetJaina(), SAY_REUNION_15);
					SetTarget(GetJaina());
					Next(4s);
					break;
				case 18:
					Talk(GetKalec(), SAY_REUNION_16);
					SetTarget(GetKalec());
					Next(4s);
					break;
				case 19:
					Talk(GetKalec(), SAY_REUNION_17);
					Next(4s);
					break;
				case 20:
					ClearTarget();
					if (Creature* kalecgos = GetKalec())
					{
						kalecgos->SetSpeedRate(MOVE_WALK, 1.6f);
						kalecgos->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_01, KalecgosPath01, KALECGOS_PATH_01, true);
					}
					Next(2s);
					break;
				case 21:
					GetTervosh()->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_02, TervoshPath02, TERVOSH_PATH_02, true);
					Next(5s);
					break;
				case 22:
					GetKinndy()->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_01, KinndyPath01, KINNDY_PATH_01, true);
					Next(6s);
					break;
				case 23:
					GetJaina()->SetWalk(true);
					GetJaina()->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_01, JainaPoint01, true, JainaPoint01.GetOrientation());
					break;

				#pragma endregion

				// Waiting
				#pragma region WAITING

				case 24:
					TriggerGameEvent(EVENT_WAITING);
					break;

				#pragma endregion

				// The Unknown Tauren
				#pragma region THE_UNKNOWN_TAUREN

				case 25:
					for (uint8 i = 0; i < PERITH_LOCATION; i++)
					{
						if (Creature* creature = instance->SummonCreature(perithLocation[i].dataId, perithLocation[i].position))
						{
							if (creature->GetEntry() == NPC_KNIGHT_OF_THERAMORE)
							{
								creature->SetSheath(SHEATH_STATE_UNARMED);
								creature->SetEmoteState(EMOTE_STATE_WAGUARDSTAND01);
							}

							creature->SetWalk(true);
							creature->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
							creature->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, perithLocation[i].destination, false, perithLocation[i].destination.GetOrientation());
						}
					}
					#ifdef CUSTOM_DEBUG
					{
						GetPerith()->DespawnOrUnsummon();
						GetKnight()->DespawnOrUnsummon();

						events.ScheduleEvent(70, 2s);
					}
					#else
						Next(7s);
					#endif
					break;
				case 26:
					GetJaina()->SetTarget(GetPerith()->GetGUID());
					Next(2s);
					break;
				case 27:
					Talk(GetPained(), SAY_WARN_1);
					SetTarget(GetPained());
					Next(1s);
					break;
				case 28:
					Talk(GetJaina(), SAY_WARN_2);
					SetTarget(GetJaina());
					Next(1s);
					break;
				case 29:
					Talk(GetPained(), SAY_WARN_3);
					SetTarget(GetPained());
					Next(6s);
					break;
				case 30:
					Talk(GetPained(), SAY_WARN_4);
					Next(7s);
					break;
				case 31:
					Talk(GetJaina(), SAY_WARN_5);
					SetTarget(GetJaina());
					Next(6s);
					break;
				case 32:
					Talk(GetPained(), SAY_WARN_6);
					SetTarget(GetPained());
					Next(10s);
					break;
				case 33:
					ClearTarget();
					GetPained()->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_NONE, GetJaina(), 1.8f);
					Next(2s);
					break;
				case 34:
					SetTarget(GetJaina());
					GetPained()->SetEmoteState(EMOTE_STATE_USE_STANDING);
					Next(1s);
					break;
				case 35:
					GetJaina()->SetEmoteState(EMOTE_STATE_USE_STANDING);
					GetPained()->SetEmoteState(EMOTE_STATE_NONE);
					Talk(GetPained(), SAY_WARN_7);
					Next(3s);
					break;
				case 36:
					GetJaina()->SetEmoteState(EMOTE_STATE_NONE);
					Talk(GetJaina(), SAY_WARN_8);
					Next(4s);
					break;
				case 37:
					Talk(GetJaina(), SAY_WARN_9);
					SetTarget(GetJaina());
					Next(2s);
					break;
				case 38:
					Talk(GetJaina(), SAY_WARN_10);
					ClearTarget();
					GetPained()->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_01, PainedPoint01, true, PainedPoint01.GetOrientation());
					Next(2s);
					break;
				case 39:
					GetKnight()->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_01, GetJaina(), 3.0f);
					Next(2s);
					break;
				case 40:
					Talk(GetKnight(), SAY_WARN_11);
					SetTarget(GetKnight());
					Next(3s);
					break;
				case 41:
					Talk(GetJaina(), SAY_WARN_12);
					SetTarget(GetJaina());
					Next(5s);
					break;
				case 42:
					ClearTarget();
					Talk(GetPained(), SAY_WARN_13);
					if (Creature* officer = GetKnight())
					{
						officer->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_02, OfficerPath01, OFFICER_PATH_01, true);
						officer->DespawnOrUnsummon(15s);
					}
					GetPerith()->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_NONE, GetJaina(), 3.0f);
					Next(3s);
					break;
				case 43:
					Talk(GetPerith(), SAY_WARN_14);
					GetPerith()->SetTarget(GetJaina()->GetGUID());
					SetTarget(GetPerith());
					Next(5s);
					break;
				case 44:
					Talk(GetJaina(), SAY_WARN_15);
					Next(4s);
					break;
				case 45:
					Talk(GetPerith(), SAY_WARN_16);
					Next(10s);
					break;
				case 46:
					Talk(GetPerith(), SAY_WARN_17);
					Next(10s);
					break;
				case 47:
					Talk(GetPerith(), SAY_WARN_18);
					Next(10s);
					break;
				case 48:
					Talk(GetJaina(), SAY_WARN_19);
					Next(5s);
					break;
				case 49:
					Talk(GetPerith(), SAY_WARN_20);
					Next(5s);
					break;
				case 50:
					Talk(GetJaina(), SAY_WARN_21);
					Next(2s);
					break;
				case 51:
					Talk(GetPerith(), SAY_WARN_22);
					Next(12s);
					break;
				case 52:
					Talk(GetPerith(), SAY_WARN_23);
					Next(8s);
					break;
				case 53:
					Talk(GetJaina(), SAY_WARN_24);
					Next(10s);
					break;
				case 54:
					Talk(GetPerith(), SAY_WARN_25);
					Next(11s);
					break;
				case 55:
					Talk(GetJaina(), SAY_WARN_26);
					Next(3s);
					break;
				case 56:
					if (Creature* jaina = GetJaina())
					{
						jaina->SetTarget(ObjectGuid::Empty);
						jaina->SetFacingTo(3.33f);
					}
					Next(2s);
					break;
				case 57:
					Talk(GetJaina(), SAY_WARN_27);
					GetJaina()->AI()->DoCast(SPELL_ARCANE_CANALISATION);
					if (Creature* quill = GetJaina()->SummonCreature(NPC_INVISIBLE_STALKER, QuillPoint01, TEMPSUMMON_TIMED_DESPAWN, 10s))
						quill->AddAura(SPELL_MAGIC_QUILL, quill);
					Next(10s);
					break;
				case 58:
					if (Creature* jaina = GetJaina())
					{
						jaina->RemoveAurasDueToSpell(SPELL_ARCANE_CANALISATION);
						jaina->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
						jaina->SetFacingToObject(GetPerith());
					}
					Next(2s);
					break;
				case 59:
					Talk(GetJaina(), SAY_WARN_28);
					Next(3s);
					break;
				case 60:
					Talk(GetPerith(), SAY_WARN_29);
					Next(5s);
					break;
				case 61:
					Talk(GetPerith(), SAY_WARN_30);
					Next(8s);
					break;
				case 62:
					Talk(GetJaina(), SAY_WARN_31);
					Next(5s);
					break;
				case 63:
					Talk(GetPerith(), SAY_WARN_32);
					Next(4s);
					break;
				case 64:
					Talk(GetJaina(), SAY_WARN_33);
					Next(5s);
					break;
				case 65:
					Talk(GetPerith(), SAY_WARN_34);
					Next(4s);
					break;
				case 66:
					ClearTarget();
					GetPained()->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					if (Creature* perith = GetPerith())
					{
						perith->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_01, OfficerPath01, OFFICER_PATH_01, true);
						perith->DespawnOrUnsummon(15s);
					}
					Next(5s);
					break;
				case 67:
					Talk(GetJaina(), SAY_WARN_35);
					GetJaina()->SetFacingToObject(GetPained());
					GetPained()->SetFacingToObject(GetJaina());
					Next(5s);
					break;
				case 68:
					Talk(GetPained(), SAY_WARN_36);
					Next(3s);
					break;
				case 69:
					Talk(GetJaina(), SAY_WARN_37);
					Next(3s);
					break;
				case 70:
					GetJaina()->SetFacingTo(0.39f);
					GetPained()->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_02, KinndyPath01, KINNDY_PATH_01, true);
					break;

				#pragma endregion

				// A Little Help
				#pragma region A_LITTLE_HELP

				case 71:
					EnsurePlayerHaveShaker();
					if (Creature* hedric = GetHedric())
					{
						hedric->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
						hedric->GetMotionMaster()->Clear();
						hedric->GetMotionMaster()->MoveIdle();
						hedric->NearTeleportTo(HedricPoint01);
						hedric->PlayDirectSound(SOUND_FEARFUL_CROWD);
						Talk(hedric, SAY_PRE_BATTLE_1);
					}
					Next(3s);
					break;
				case 72:
					Talk(GetJaina(), SAY_PRE_BATTLE_2);
					GetHedric()->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_NONE, HedricPath01, HEDRIC_PATH_01);
					Next(2s);
					break;
				case 73:
					Talk(GetHedric(), SAY_PRE_BATTLE_3);
					Next(3s);
					break;
				case 74:
					Talk(GetJaina(), SAY_PRE_BATTLE_4);
					Next(4s);
					break;
				case 75:
					GetJaina()->SummonGameObject(GOB_PORTAL_TO_DALARAN, PortalPoint01, QuaternionData::QuaternionData(), 0s);
					Next(500ms);
					break;
				case 76:
					if (Creature* hedric = GetHedric())
					{
						hedric->SetWalk(true);
						hedric->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, HedricPoint02, false, HedricPoint02.GetOrientation());
					}
					Next(500ms);
					break;
				case 77:
					if (archmagesIndex >= ARCHMAGES_LOCATION)
						Next(2s);
					else if (Creature* creature = instance->SummonCreature(archmagesLocation[archmagesIndex].dataId, PortalPoint01))
					{
						creature->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
						creature->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
						creature->SetSheath(SHEATH_STATE_UNARMED);
						creature->CastSpell(creature, SPELL_TELEPORT_DUMMY);
						creature->SetWalk(true);
						creature->GetMotionMaster()->Clear();
						creature->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, archmagesLocation[archmagesIndex].destination, false, archmagesLocation[archmagesIndex].destination.GetOrientation());
						archmagesIndex++;
						events.Repeat(800ms, 1s);
					}
					break;
				case 78:
					Talk(GetRhonin(), SAY_PRE_BATTLE_5);
					SetTarget(GetRhonin());
					ClosePortal(DATA_PORTAL_TO_DALARAN);
					Next(2s);
					break;
				case 79:
					Talk(GetJaina(), SAY_PRE_BATTLE_6);
					SetTarget(GetJaina());
					Next(11s);
					break;
				case 80:
					Talk(GetJaina(), SAY_PRE_BATTLE_7);
					Next(9s);
					break;
				case 81:
					Talk(GetThalen(), SAY_PRE_BATTLE_8);
					SetTarget(GetThalen());
					Next(7s);
					break;
				case 82:
					Talk(GetJaina(), SAY_PRE_BATTLE_9);
					SetTarget(GetJaina());
					Next(3s);
					break;
				case 83:
					Talk(GetJaina(), SAY_PRE_BATTLE_10);
					Next(7s);
					break;
				case 84:
					Talk(GetRhonin(), SAY_PRE_BATTLE_11);
					SetTarget(GetRhonin());
					Next(2s);
					break;
				case 85:
					Talk(GetJaina(), SAY_PRE_BATTLE_12);
					SetTarget(GetTervosh());
					Next(2s);
					break;
				case 86:
					Talk(GetVereesa(), SAY_PRE_BATTLE_13);
					SetTarget(GetVereesa());
					Next(2s);
					break;
				case 87:
					if (Creature* jaina = GetJaina())
					{
						Talk(jaina, SAY_PRE_BATTLE_14);
						SetTarget(jaina);
						jaina->SetTarget(ObjectGuid::Empty);
						jaina->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					}
					Next(2s);
					break;
				case 88:
					if (Creature* jaina = GetJaina())
					{
						Talk(jaina, SAY_PRE_BATTLE_15);
						if (Player* player = instance->GetPlayers().begin()->GetSource())
							SetTarget(player);
					}
					if (Creature* vereesa = GetVereesa())
					{
						vereesa->CastSpell(vereesa, SPELL_VANISH);
						vereesa->SetFaction(FACTION_FRIENDLY);
						vereesa->SetVisible(false);
					}
					Next(5s);
					break;
				case 89:
					ClearTarget();
					GetJaina()->AI()->DoCast(SPELL_MASS_TELEPORT);
					Next(4600ms);
					break;
				case 90:
					EnsureBarrierHaveDamage();
					if (Creature* kalecgos = instance->SummonCreature(NPC_KALECGOS_DRAGON, KalecgosPoint02))
					{
						kalecgos->GetMotionMaster()->Clear();
						kalecgos->GetMotionMaster()->MoveIdle();

						kalecgos->m_Events.AddEvent(new KalecgosLoopEvent(kalecgos),
							kalecgos->m_Events.CalculateTime(1s));
					}
					for (uint8 i = 0; i < ACTORS_RELOCATION; i++)
					{
						if (Creature* creature = GetCreature(actorsRelocation[i].dataId))
						{
							creature->RemoveAllAuras();
							creature->SetTarget(ObjectGuid::Empty);
							creature->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
							creature->SetSheath(SHEATH_STATE_MELEE);
							creature->GetMotionMaster()->Clear();
							creature->GetMotionMaster()->MoveIdle();
							creature->NearTeleportTo(actorsRelocation[i].destination);
							creature->SetHomePosition(actorsRelocation[i].destination);

							switch (creature->GetEntry())
							{
								case NPC_AMARA_LEESON:
									creature->CastSpell(creature, SPELL_PORTAL_CHANNELING_03);
									break;
								case NPC_RHONIN:
								case NPC_THADER_WINDERMERE:
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
									creature->SetVisible(false);
									break;
							}
						}
					}
					break;

				#pragma endregion

				// The Battle
				#pragma region THE_BATTLE

				case 91:
					Talk(GetJaina(), SAY_BATTLE_02);
					events.ScheduleEvent(93, 10s);
					break;
				// DELETED
				//case 92:
				//    break;
				case 93:
					if (Creature* thalen = GetThalen())
					{
						thalen->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
						thalen->SetReactState(REACT_PASSIVE);
						thalen->SetFaction(FACTION_ENEMY);
						thalen->RemoveAllAuras();
						thalen->CastSpell(thalen, SPELL_BLAZING_BARRIER);

						if (Creature* trigger = thalen->SummonCreature(WORLD_TRIGGER, ExplodingPoint01, TEMPSUMMON_TIMED_DESPAWN, 5s))
							trigger->CastSpell(trigger, SPELL_METEOR);
					}
					Next(1s);
					break;
				case 94:
					if (GameObject* barrier = GetBarrier01())
					{
						scheduler.CancelGroup((uint32)BFTPhases::TheBattle);
						barrier->ResetDoorOrButton();
						if (Creature* trigger = barrier->SummonCreature(WORLD_TRIGGER, ExplodingPoint01, TEMPSUMMON_TIMED_DESPAWN, 5s))
						{
							trigger->SetFaction(FACTION_MONSTER);
							trigger->CastSpell(trigger, SPELL_BIG_EXPLOSION);
							trigger->CastSpell(trigger, SPELL_SCORCHED_EARTH, true);
						}
					}
					Next(1s);
					break;
				case 95:
					if (Creature* amara = GetAmara())
					{
						amara->RemoveAllAuras();
						amara->SetEmoteState(EMOTE_STATE_READY2HL_ALLOW_MOVEMENT);
						amara->CastSpell(amara, SPELL_PRISMATIC_BARRIER, true);
					}
					GetThalen()->HandleEmoteCommand(EMOTE_ONESHOT_LAUGH);
					GetJaina()->SetEmoteState(EMOTE_STATE_READY2HL_ALLOW_MOVEMENT);
					GetHedric()->SetEmoteState(EMOTE_STATE_READY1H_ALLOW_MOVEMENT);
					Next(1s);
					break;
				case 96:
					Talk(GetJaina(), SAY_BATTLE_03);
					if (Creature* thalen = GetThalen())
					{
						thalen->SetWalk(false);
						thalen->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, ThalenPoint01, false, ThalenPoint01.GetOrientation());
					}
					Next(2s);
					break;
				case 97:
					if (Creature* jaina = GetJaina())
					{
						jaina->CastSpell(JainaPoint04, SPELL_TELEPORT);
						Talk(jaina, SAY_BATTLE_04);
					}
					Next(1s);
					break;
				case 98:
					if (Creature* thalen = GetThalen())
					{
						thalen->CastSpell(thalen, SPELL_ICY_GLARE);
						thalen->CastSpell(thalen, SPELL_CHILLING_BLAST, true);
						thalen->StopMoving();
					}
					Next(2s);
					break;
				case 99:
					if (Creature* thalen = GetThalen())
					{
						thalen->RemoveAurasDueToSpell(SPELL_BLAZING_BARRIER);
						thalen->CastSpell(thalen, SPELL_DISSOLVE);
					}
					if (Creature* thader = GetCreature(DATA_THADER_WINDERMERE))
					{
						Talk(thader, SAY_BATTLE_05);
						thader->RemoveAllAuras();
						thader->SetRegenerateHealth(false);
						thader->SetReactState(REACT_PASSIVE);
						thader->SetStandState(UNIT_STAND_STATE_KNEEL);
						thader->SetHealth(thader->CountPctFromMaxHealth(5));
						thader->CastSpell(thader, SPELL_ARCANE_FX);

						if (Creature* tari = GetCreature(DATA_TARI_COGG))
						{
							Talk(tari, SAY_BATTLE_06);
							tari->RemoveAllAuras();
							tari->SetReactState(REACT_PASSIVE);
							tari->CastSpell(tari, SPELL_CHANNEL_BLUE_MOVING);
							tari->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_NONE, thader, 0.8f);
						}
					}
					GetBarrier02()->ResetDoorOrButton();
					Next(3s);
					break;
				case 100:
					HordeMembersInvoker(DATA_WAVE_BOAT);
					if (Creature* thalen = GetThalen())
					{
						thalen->RemoveAllAuras();
						thalen->NearTeleportTo(ThalenPoint02);
						thalen->SetHomePosition(ThalenPoint02);
						thalen->CastSpell(thalen, SPELL_ARCANIC_CELL, true);
						thalen->SetEmoteState(EMOTE_STATE_STUN_NO_SHEATHE);
					}
					if (Creature* kalecgos = GetKalecgos())
					{
						kalecgos->m_Events.AddEvent(new KalecgosSpellEvent(kalecgos), kalecgos->m_Events.CalculateTime(2s));
					}
					GetJaina()->CastSpell(actorsRelocation[0].destination, SPELL_TELEPORT);
					Next(2s);
					break;

				#pragma endregion

				// For the Alliance
				#pragma region FOR_THE_ALLIANCE

				case WAVE_01:
				case WAVE_02:
				case WAVE_03:
				case WAVE_04:
				case WAVE_05:
				case WAVE_06:
				case WAVE_07:
				case WAVE_08:
				case WAVE_09:
				case WAVE_10:
					#ifdef CUSTOM_DEBUG
						for (uint8 i = 0; i < HORDE_WAVES_COUNT; i++)
						{
							DoCastSpellOnPlayers(SPELL_KILL_CREDIT);
							events.ScheduleEvent(WAVE_BREAKER, 1s);
						}
					#else
						HordeMembersInvoker(Waves[waves]);
						waves++;
						events.ScheduleEvent(++wavesInvoker, 1s);
					#endif
					break;

				case WAVE_01_CHECK:
				case WAVE_02_CHECK:
				case WAVE_03_CHECK:
				case WAVE_04_CHECK:
				case WAVE_05_CHECK:
				case WAVE_06_CHECK:
				case WAVE_07_CHECK:
				case WAVE_08_CHECK:
				case WAVE_09_CHECK:
				case WAVE_10_CHECK:
				{
					uint32 deadCounter = 0;
					if (Creature* jaina = GetJaina())
					{
						for (uint8 i = 0; i < HORDE_WAVES_COUNT; ++i)
						{
							Creature* temp = ObjectAccessor::GetCreature(*jaina, hordeMembers[i]);
							if (!temp || temp->isDead())
								++deadCounter;
						}
					}
						
					// Quand le nombre de membres vivants est inférieur ou égal au nombre de membres morts
					if (deadCounter >= HORDE_WAVES_COUNT)
					{
						DoCastSpellOnPlayers(SPELL_KILL_CREDIT);
						events.ScheduleEvent(++wavesInvoker, 2s);
					}
					else
					{
						events.ScheduleEvent(wavesInvoker, 1s);
					}

					break;
				}

				case WAVE_BREAKER:
					break;

				#pragma endregion

				// Help the wounded
				#pragma region HELP_THE_WOUNDED

				// PART I
				case 122:
					if (Creature* jaina = GetJaina())
					{
						if (Creature* hedric = GetHedric())
						{
							jaina->SetTarget(hedric->GetGUID());
							jaina->SetEmoteState(EMOTE_STATE_STAND);

							hedric->SetTarget(jaina->GetGUID());
							hedric->SetEmoteState(EMOTE_STATE_STAND);
						}
					}
					Next(800ms);
					break;
				case 123:
					Talk(GetJaina(), SAY_POST_BATTLE_01);
					Next(2s);
					break;
				case 124:
					Talk(GetHedric(), SAY_POST_BATTLE_02);
					Next(4s);
					break;
				case 125:
					Talk(GetJaina(), SAY_POST_BATTLE_03);
					Next(4s);
					break;
				case 126:
					ClearTarget();
                    if (Creature* jaina = GetJaina())
                    {
                        jaina->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_02, JainaPath01, JAINA_PATH_01);
                        jaina->SetHomePosition(JainaPath01[JAINA_PATH_01 - 1]);
                    }
					Next(1500ms);
					break;
				case 127:
                    if (Creature* hedric = GetHedric())
                    {
                        hedric->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_NONE, JainaPath01, JAINA_PATH_01);
                        hedric->SetHomePosition(JainaPath01[JAINA_PATH_01 - 1]);
                    }
                    break;

				// PART II
				case 128:
					if (Creature* jaina = GetJaina())
					{
						if (Creature* kinndy = GetKinndy())
						{
							jaina->SetTarget(kinndy->GetGUID());
							kinndy->SetTarget(jaina->GetGUID());
						}
					}
					Next(800ms);
					break;
				case 129:
					Talk(GetKinndy(), SAY_POST_BATTLE_04);
					Next(5s);
					break;
				case 130:
					Talk(GetJaina(), SAY_POST_BATTLE_05);
					Next(7s);
					break;
				case 131:
					if (Creature* kinndy = GetKinndy())
					{
						Talk(kinndy, SAY_POST_BATTLE_06);
						kinndy->SetEmoteState(EMOTE_STATE_NONE);
					}
					Next(4s);
					break;
				case 132:
					Talk(GetJaina(), SAY_POST_BATTLE_07);
					Next(4s);
					break;
				case 133:
					Talk(GetKinndy(), SAY_POST_BATTLE_08);
					Next(3s);
					break;
				case 134:
					Talk(GetJaina(), SAY_POST_BATTLE_09);
					Next(13s);
					break;
				case 135:
					Talk(GetJaina(), SAY_POST_BATTLE_10);
					Next(7s);
					break;
				case 136:
					Talk(GetKinndy(), SAY_POST_BATTLE_11);
					Next(8s);
					break;
				case 137:
					Talk(GetJaina(), SAY_POST_BATTLE_12);
					Next(6s);
					break;
				case 138:
					Talk(GetJaina(), SAY_POST_BATTLE_13);
					Next(12s);
					break;
				case 139:
					Talk(GetKinndy(), SAY_POST_BATTLE_14);
					Next(3s);
					break;
				case 140:
					Talk(GetJaina(), SAY_POST_BATTLE_15);
					Next(3s);
					break;
				case 141:
					ClearTarget();
					if (Creature* jaina = GetJaina())
					{
						jaina->SetSpeedRate(MOVE_RUN, 0.85f);

						if (Creature* kinndy = GetKinndy())
						{
							jaina->SetFacingTo(3.15f);
							kinndy->SetFacingTo(2.73f);
						}
					}
					break;

				#pragma endregion

				// Wait for Archmage Leeson returns
				#pragma region WAIT_FOR_AMARA

				// Part I
				case 142:
					if (Creature* kalecgos = GetKalec())
					{
						kalecgos->SetVisible(true);
						kalecgos->SetSpeedRate(MOVE_RUN, 0.85f);
						kalecgos->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_NONE, KalecgosPath02, KALECGOS_PATH_02);
					}
					Next(5s);
					break;
				case 143:
					SetTarget(GetKalec());
					Talk(GetKalec(), SAY_IRIS_WARN_01);
					Next(1s);
					break;
				case 144:
					SetTarget(GetJaina());
					Talk(GetJaina(), SAY_IRIS_WARN_02);
					Next(2s);
					break;
				case 145:
					SetTarget(GetKalec());
					Talk(GetKalec(), SAY_IRIS_WARN_03);
					Next(4s);
					break;
				case 146:
					SetTarget(GetJaina());
					Talk(GetJaina(), SAY_IRIS_WARN_04);
					Next(8s);
					break;
				case 147:
					SetTarget(GetKalec());
					Talk(GetKalec(), SAY_IRIS_WARN_05);
					Next(2s);
					break;
				case 148:
					SetTarget(GetJaina());
					Talk(GetJaina(), SAY_IRIS_WARN_06);
					Next(6s);
					break;
				case 149:
					Talk(GetJaina(), SAY_IRIS_WARN_07);
					Next(2s);
					break;
				case 150:
					SetTarget(GetKalec());
					Talk(GetKalec(), SAY_IRIS_WARN_08);
					Next(7s);
					break;
				case 151:
					SetTarget(GetRhonin());
					Talk(GetRhonin(), SAY_IRIS_WARN_09);
					Next(5s);
					break;
				case 152:
					ClearTarget();
					GetKalec()->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_02, KalecgosPath03, KALECGOS_PATH_03);
					Next(2s);
					break;
				case 153:
					GetRhonin()->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_01, RhoninPath01, RHONIN_PATH_01);
					Next(2s);
					break;
				case 154:
					if (Creature* amara = GetAmara())
					{
						amara->SetSpeedRate(MOVE_RUN, 0.85f);
						amara->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_01, AmaraPath01, AMARA_PATH_01);
					}
					Next(10s);
					break;
				case 155:
					if (Creature* amara = GetAmara())
					{
						amara->SetVisible(true);
						amara->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_02, KalecgosPath02, KALECGOS_PATH_02);
					}
					break;

				// Part II
				case 156:
					if (Creature* jaina = GetJaina())
					{
						if (Creature* amara = GetAmara())
						{
							jaina->SetTarget(amara->GetGUID());
							amara->SetTarget(jaina->GetGUID());
						}
					}
					Next(1s);
					break;
				case 157:
					Talk(GetAmara(), SAY_IRIS_WARN_10);
					Next(6s);
					break;
				case 158:
					Talk(GetJaina(), SAY_IRIS_WARN_11);
					Next(4s);
					break;
				case 159:
					ClearTarget();
					GetAmara()->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_03, KalecgosPath03, KALECGOS_PATH_03);
					Next(2s);
					break;
				case 160:
					GetJaina()->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_03, RhoninPath01, RHONIN_PATH_01);
					break;

				#pragma endregion

				// Retrieve Rhonin
				#pragma region RETRIEVE_RHONIN

				case 161:
					if (Creature* jaina = GetJaina())
					{
						Talk(jaina, SAY_IRIS_XPLOSION_01);
						jaina->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					}
					Next(3s);
					break;
				case 162:
					if (Creature* jaina = GetJaina())
					{
						jaina->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
						jaina->SetFacingToObject(GetRhonin());
						jaina->RemoveAllAuras();

						if (Creature* rhonin = GetRhonin())
						{
							jaina->SetTarget(rhonin->GetGUID());

							rhonin->SetTarget(jaina->GetGUID());
							rhonin->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
						}
					}
					Next(3s);
					break;
				case 163:
					Talk(GetRhonin(), SAY_IRIS_XPLOSION_02);
					Next(4s);
					break;
				case 164:
					Talk(GetJaina(), SAY_IRIS_XPLOSION_03);
					Next(6s);
					break;
				case 165:
					Talk(GetRhonin(), SAY_IRIS_XPLOSION_04);
					Next(5s);
					break;
				case 166:
					Talk(GetRhonin(), SAY_IRIS_XPLOSION_05);
					Next(8s);
					break;
				case 167:
					Talk(GetRhonin(), SAY_IRIS_XPLOSION_06);
					Next(6s);
					break;
				case 168:
					Talk(GetJaina(), SAY_IRIS_XPLOSION_07);
					Next(6s);
					break;
				case 169:
					Talk(GetRhonin(), SAY_IRIS_XPLOSION_08);
					Next(3s);
					break;
				case 170:
					Talk(GetJaina(), SAY_IRIS_XPLOSION_09);
					Next(5s);
					break;
				case 171:
					if (Creature* rhonin = GetRhonin())
					{
						Talk(rhonin, SAY_IRIS_XPLOSION_10);
						rhonin->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
						rhonin->RemoveAllAuras();
					}
					Next(5s);
					break;
				case 172:
					TriggerGameEvent(EVENT_REDUCE_IMPACT);
					break;

				#pragma endregion
			}
		}

		EventMap events;
		TaskScheduler scheduler;
		BFTPhases phase;
		uint32 wavesInvoker;
		uint32 eventId;
		uint32 woundedTroops;
		uint8 archmagesIndex;
		uint8 waves;
		GuidVector citizens;
		GuidVector tanks;
		GuidVector troops;
		GuidVector hordeMembers;

		// Accesseurs
		#pragma region ACCESSORS

		Creature* GetJaina()        { return GetCreature(DATA_JAINA_PROUDMOORE); }
		Creature* GetKinndy()       { return GetCreature(DATA_KINNDY_SPARKSHINE); }
		Creature* GetTervosh()      { return GetCreature(DATA_ARCHMAGE_TERVOSH); }
		Creature* GetKalec()        { return GetCreature(DATA_KALECGOS); }
		Creature* GetKalecgos()     { return GetCreature(DATA_KALECGOS_DRAGON); }
		Creature* GetPained()       { return GetCreature(DATA_PAINED); }
		Creature* GetPerith()       { return GetCreature(DATA_PERITH_STORMHOOVE); }
		Creature* GetKnight()       { return GetCreature(DATA_KNIGHT_OF_THERAMORE); }
		Creature* GetHedric()       { return GetCreature(DATA_HEDRIC_EVENCANE); }
		Creature* GetRhonin()       { return GetCreature(DATA_RHONIN); }
		Creature* GetVereesa()      { return GetCreature(DATA_VEREESA_WINDRUNNER); }
		Creature* GetThalen()       { return GetCreature(DATA_THALEN_SONGWEAVER); }
		Creature* GetAmara()        { return GetCreature(DATA_AMARA_LEESON); }
		Creature* GetDrok()         { return GetCreature(DATA_CAPTAIN_DROK); }
		Creature* GetGruhta()       { return GetCreature(DATA_WAVE_CALLER_GRUHTA); }

		GameObject* GetBarrier01()  { return GetGameObject(DATA_MYSTIC_BARRIER_01); }
		GameObject* GetBarrier02()  { return GetGameObject(DATA_MYSTIC_BARRIER_02); }

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

		void SetTarget(Unit* unit)
		{
			ObjectGuid guid = unit->GetGUID();
			for (uint8 i = 0; i < EVENT_CREATURE_DATA_SIZE; i++)
			{
				if (Creature* creature = GetCreature(creatureData[i].type))
				{
					if (creature->IsTrigger())
						continue;

					if (creature->GetGUID() == guid)
						continue;

					if (creature->GetEntry() == NPC_HEDRIC_EVENCANE
						&& phase < BFTPhases::Preparation)
					{
						continue;
					}

					creature->SetTarget(guid);
				}
			}
		}

		void ClearTarget()
		{
			for (uint8 i = 0; i < EVENT_CREATURE_DATA_SIZE; i++)
			{
				if (Creature* creature = GetCreature(creatureData[i].type))
					creature->SetTarget(ObjectGuid::Empty);
			}
		}

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

		void TeleportPlayers(Creature* caster, const Position center, float minDist)
		{
			Position pos = caster->GetRandomPoint(center, 8.f);
			pos.m_positionZ += 3.0f;

			instance->DoOnPlayers([caster, center, minDist, pos](Player* player)
			{
				if (player->IsWithinDist(caster, minDist))
				{
					player->NearTeleportTo(pos);
				}
			});
		}

		void HordeMembersInvoker(uint32 waveId)
		{
			std::list<TempSummon*> members;

			hordeMembers.clear();

			instance->SummonCreatureGroup(waveId, &members);
			for (TempSummon* horde : members)
			{
				if (Unit* target = SelectNearestHostileInRange(horde))
					horde->AI()->AttackStart(target);

				hordeMembers.push_back(horde->GetGUID());
			}

			if (Creature* jaina = GetJaina())
				jaina->AI()->DoAction(waveId);
		}

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

				if (roll_chance_i(80))
				{
					troop->SetVisible(false);
					if (Creature* wounded = troop->SummonCreature(NPC_THERAMORE_WOUNDED_TROOP, troop->GetPosition(), TempSummonType::TEMPSUMMON_MANUAL_DESPAWN))
					{
						uint32 health = troop->GetMaxHealth();
						Powers power = troop->GetPowerType();

						wounded->SetPowerType(power);
						wounded->SetPower(power, troop->GetPower(power));
						wounded->SetRegenerateHealth(false);
						wounded->SetMaxHealth(health);
						wounded->SetHealth(health * frand(0.15f, 0.20f));
						wounded->SetStandState(UNIT_STAND_STATE_SLEEP);
						wounded->SetDisplayId(troop->GetDisplayId());
						wounded->SetImmuneToNPC(true);
					}
				}
			}
		}

		void RelocateTroops()
		{
			SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, GetRhonin());

            instance->DoOnPlayers([this](Player* player)
            {
                player->RemoveAurasDueToSpell(SPELL_FOR_THE_ALLIANCE);
            });

			if (Creature* jaina = GetJaina())
			{
				SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, jaina);
				jaina->CombatStop();
				jaina->SetReactState(REACT_PASSIVE);
				jaina->NearTeleportTo(JainaPoint03);
				jaina->SetHomePosition(JainaPoint03);
				jaina->SetSheath(SHEATH_STATE_UNARMED);
			}

			if (GameObject* portal = GetGameObject(DATA_PORTAL_TO_ORGRIMMAR))
			{
				portal->m_Events.KillAllEvents(true);
				portal->Delete();
			}

			if (Creature* kalecgos = GetKalecgos())
			{
				SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, kalecgos);

				kalecgos->m_Events.KillAllEvents(true);
				kalecgos->GetMotionMaster()->Clear();
				kalecgos->GetMotionMaster()->MoveIdle();
				kalecgos->SetVisible(false);
			}

			uint8 counter = 0;
			for (uint8 i = 0; i < troops.size(); i++)
			{
				Creature* creature = instance->GetCreature(troops[i]);

				if (!creature || creature->isDead())
					continue;

				counter++;
				if (counter >= ARCHMAGES_RELOCATION)
					break;

				creature->SetVisible(true);
				creature->NearTeleportTo(UnitLocation[i]);
				creature->SetHomePosition(UnitLocation[i]);
				creature->SetSheath(SHEATH_STATE_UNARMED);
				creature->SetStandState(UNIT_STAND_STATE_STAND);
				creature->SetEmoteState(EMOTE_STATE_NONE);
				creature->RemoveAllAuras();
				creature->Dismount();

				switch (urand(0, 2))
				{
					case 0:
						creature->SetStandState(UNIT_STAND_STATE_SIT);
						creature->SetEmoteState(EMOTE_STATE_EAT);
						break;
					case 1:
						creature->SetEmoteState(EMOTE_STATE_WADRUNKSTAND);
						break;
					default:
						break;
				}
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
		}

		void EnsurePlayersAreInPhase(uint32 phaseId)
		{
			instance->DoOnPlayers([phaseId](Player* player)
			{
				PhasingHandler::AddPhase(player, phaseId, true);
			});
		}

		void EnsurePlayersHaveAura(uint32 entry)
		{
			instance->DoOnPlayers([entry](Player* player)
			{
				if (!player->HasAura(entry))
				{
					player->CastSpell(player, entry, true);
				}
			});
		}

		void EnsurePlayerHaveShield()
		{
			scheduler.Schedule(2s, [this](TaskContext shield)
			{
				if (phase >= BFTPhases::Preparation_Rhonin && phase < BFTPhases::HelpTheWounded)
				{
					EnsurePlayersHaveAura(SPELL_RUNIC_SHIELD);
					shield.Repeat(1s);
				}
			});
		}

		void EnsurePlayerHaveBucket()
		{
			scheduler.Schedule(2s, [this](TaskContext bucket)
			{
				if (phase >= BFTPhases::HelpTheWounded && phase < BFTPhases::HelpTheWounded_Extinguish)
				{
					EnsurePlayersHaveAura(SPELL_WATER_BUCKET);
					bucket.Repeat(1s);
				}
			});
		}

		void EnsurePlayerHaveShaker()
		{
			scheduler.Schedule(1s, [this](TaskContext shield)
			{
				if (phase >= BFTPhases::Preparation && phase < BFTPhases::HelpTheWounded)
				{
					DoCastSpellOnPlayers(SPELL_CAMERA_SHAKE_VOLCANO);
				}

				shield.Repeat(15s, 30s);
			});
		}

		void EnsureBarrierHaveDamage()
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

class scene_theramore_explosion : public SceneScript
{
	public:
		scene_theramore_explosion() : SceneScript("scene_theramore_explosion") { }

	enum Misc
	{
		MAP_THERAMORE_RUINS     = 5001,
		SPELL_DROP_BOMBE        = 128438
	};

	const Position Center = { -3009.70f, -4334.41f, 6.73f, 4.24f };
	const float Distance = 8.f;

	void OnSceneStart(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/) override
	{
		player->SetControlled(true, UNIT_STATE_ROOT);
	}

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
		player->SetControlled(false, UNIT_STATE_ROOT);
		player->TeleportTo(GetRandomPosition(), TELE_REVIVE_AT_TELEPORT);
	}

	WorldLocation GetRandomPosition()
	{
		float alpha = 2 * float(M_PI) * float(rand_norm());
		float r = Distance * sqrtf(float(rand_norm()));
		float x = r * cosf(alpha) + Center.GetPositionX();
		float y = r * sinf(alpha) + Center.GetPositionY();
		return { MAP_THERAMORE_RUINS, { x, y, Center.GetPositionZ(), Center.GetOrientation() }};
	}
};

class NearestHostileUnitInRange
{
	public:
		explicit NearestHostileUnitInRange(Creature const* creature) : me(creature) { }

		bool operator()(Unit* u) const
		{
			if (!u->IsHostileTo(me))
				return false;

			if (!u->IsWithinDist(me, MAX_VISIBILITY_DISTANCE))
				return false;

			if (!me->IsValidAttackTarget(u))
				return false;

			if (!u->IsWithinLOSInMap(me))
				return false;

			return true;
		}

	private:
		Creature const* me;
		NearestHostileUnitInRange(NearestHostileUnitInRange const&) = delete;
};

void AddSC_scenario_battle_for_theramore()
{
	new scenario_battle_for_theramore();
	new scene_theramore_explosion();
}
