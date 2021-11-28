#include "CriteriaHandler.h"
#include "EventMap.h"
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

#define CREATURE_DATA_SIZE 16

const ObjectData creatureData[CREATURE_DATA_SIZE] =
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
	{ NPC_KALECGOS_DRAGON,      DATA_KALECGOS_DRAGON        },
	{ 0,                        0                           }   // END
};

const ObjectData gameobjectData[] =
{
	{ GOB_PORTAL_TO_STORMWIND,  DATA_PORTAL_TO_STORMWIND    },
	{ GOB_PORTAL_TO_DALARAN,    DATA_PORTAL_TO_DALARAN      },
	{ GOB_PORTAL_TO_ORGRIMMAR,  DATA_PORTAL_TO_ORGRIMMAR    },
	{ GOB_MYSTIC_BARRIER_01,    DATA_MYSTIC_BARRIER_01      },
	{ GOB_MYSTIC_BARRIER_02,    DATA_MYSTIC_BARRIER_02      },
	{ 0,                        0                           }   // END
};

class HordeDoorsEvent : public BasicEvent
{
	public:
	HordeDoorsEvent(GameObject* owner) : owner(owner)
	{
	}

	bool Execute(uint64 eventTime, uint32 /*updateTime*/) override
	{
		float x = minPosX + (rand() % static_cast<int>(maxPosX - minPosX + 1));
		float y = -4248.63f;
		float z = 6.00f;

		if (Creature* creature = owner->SummonCreature(RAND(NPC_ROKNAH_GRUNT, NPC_ROKNAH_LOA_SINGER, NPC_ROKNAH_HAG, NPC_ROKNAH_FELCASTER),
			PortalPoint02, TEMPSUMMON_MANUAL_DESPAWN))
		{
			creature->SetImmuneToAll(true);
			creature->CastSpell(creature, SPELL_TELEPORT_DUMMY);
			creature->UpdateGroundPositionZ(x, y, z);
			creature->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_03, { x, y, z });
		}

		owner->m_Events.AddEvent(this, eventTime + urand(3 * IN_MILLISECONDS, 5 * IN_MILLISECONDS));

		return false;
	}

	private:
	GameObject* owner;
	const float minPosX = -3792.73f;
	const float maxPosX = -3772.15f;
};

class KalecgosSpellEvent : public BasicEvent
{
	public:
	KalecgosSpellEvent(Creature* owner) : owner(owner)
	{
		owner->SetReactState(REACT_PASSIVE);
	}

	bool Execute(uint64 eventTime, uint32 /*updateTime*/) override
	{
		if (roll_chance_i(20))
			owner->AI()->Talk(SAY_KALECGOS_SPELL_01);

		owner->CastSpell(owner, SPELL_FROST_BREATH);
		owner->GetThreatManager().RemoveMeFromThreatLists();
		owner->m_Events.AddEvent(this, eventTime + urand(8 * IN_MILLISECONDS, 10 * IN_MILLISECONDS));
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

		float perimeter = 2.f * float(M_PI) * KALECGOS_CIRCLE_RADIUS;
		m_loopTime = (perimeter / owner->GetSpeed(MOVE_RUN)) * IN_MILLISECONDS;
	}

	bool Execute(uint64 eventTime, uint32 /*updateTime*/) override
	{
		owner->GetMotionMaster()->MoveCirclePath(TheramorePoint01.GetPositionX(), TheramorePoint01.GetPositionY(), TheramorePoint01.GetPositionZ(), KALECGOS_CIRCLE_RADIUS, true, 16);
		owner->m_Events.AddEvent(this, eventTime + m_loopTime);
		return false;
	}

	private:
	Creature* owner;
	float m_loopTime;
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

			// Types
			WAVE_DOORS                  = 1000,
			WAVE_CITADEL,
			WAVE_DOCKS
		};

		enum Spells
		{
			SPELL_CLOSE_PORTAL          = 203542,
			SPELL_MAGIC_QUILL           = 171980,
			SPELL_ARCANE_CANALISATION   = 288451,
			SPELL_VANISH                = 342048,
			SPELL_MASS_TELEPORT         = 60516,
			SPELL_FIRE_CHANNELING       = 329612,
			SPELL_METEOR                = 276973,
			SPELL_BIG_EXPLOSION         = 348750,
			SPELL_BLAZING_BARRIER       = 295238,
            SPELL_PRISMATIC_BARRIER     = 235450,
            SPELL_ICY_GLARE             = 338517,
			SPELL_DISSOLVE              = 255295,
			SPELL_TELEPORT              = 357601,
			SPELL_CHILLING_BLAST        = 337053,
			SPELL_TIED_UP               = 167469,
			SPELL_FROST_BREATH          = 300548,
			SPELL_WATER_BUCKET          = 42336
		};

		uint32 Waves[10] =
		{
			WAVE_DOORS,
			WAVE_CITADEL,
			WAVE_DOCKS,
			WAVE_DOCKS,
			WAVE_CITADEL,
			WAVE_DOORS,
			WAVE_DOCKS,
			WAVE_CITADEL,
			WAVE_DOCKS,
			WAVE_DOORS
		};

		uint32 GetData(uint32 dataId) const override
		{
            if (dataId == DATA_SCENARIO_PHASE)
                return (uint32)phase;
            else if (dataId == DATA_WOUNDED_TROOPS)
                return woundedTroops;
			return 0;
		}

		void SetData(uint32 dataId, uint32 value) override
		{
            if (dataId == DATA_SCENARIO_PHASE)
                phase = (BFTPhases)value;
            else if (dataId == DATA_WOUNDED_TROOPS)
            {
                woundedTroops = value;
                printf("woundedTroops: %u\n", woundedTroops);
            }
		}

		void OnCompletedCriteriaTree(CriteriaTree const* tree) override
		{
			switch (tree->ID)
			{
				// Step 1 : Find Jaina
				case CRITERIA_TREE_FIND_JAINA:
				{
					ClosePortal(DATA_PORTAL_TO_STORMWIND);
					GetTervosh()->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
					GetKinndy()->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
					GetKalec()->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
					if (Creature* jaina = GetCreature(DATA_JAINA_PROUDMOORE))
					{
						jaina->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
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
				{
					for (uint8 i = 0; i < PERITH_LOCATION; i++)
					{
						if (Creature* creature = GetCreature(perithLocation[i].dataId))
						{
							creature->NearTeleportTo(perithLocation[i].position);
							if (creature->GetEntry() == NPC_KNIGHT_OF_THERAMORE)
							{
								creature->SetSheath(SHEATH_STATE_UNARMED);
								creature->SetEmoteState(EMOTE_STATE_WAGUARDSTAND01);
							}
						}
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::UnknownTauren);
					events.ScheduleEvent(25, 1s);
					break;
				}
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
							citizen->SetNpcFlags(UNIT_NPC_FLAG_GOSSIP);
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
						kalecgos->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
						kalecgos->RemoveAllAuras();
					}
					for (uint8 i = 0; i < ARCHMAGES_LOCATION; i++)
					{
						if (Creature* creature = GetCreature(archmagesLocation[i].dataId))
						{
							creature->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
							creature->SetVisible(false);
							creature->NearTeleportTo(PortalPoint01);
						}
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
							trigger->CastSpell(trigger, SPELL_COSMETIC_LARGE_FIRE);
					}
					for (ObjectGuid guid : dummies)
					{
						if (Creature* dummy = instance->GetCreature(guid))
						{
							dummy->AI()->DoAction(2U);
						}
					}
					for (ObjectGuid guid : tanks)
					{
						if (Creature* tank = instance->GetCreature(guid))
						{
							tank->SetNpcFlags(UNIT_NPC_FLAG_SPELLCLICK);
							tank->SetRegenerateHealth(false);
							tank->SetHealth((float)tank->GetHealth() * frand(0.15f, 0.60f));
						}
					}
					for (ObjectGuid guid : citizens)
					{
						if (Creature* citizen = instance->GetCreature(guid))
						{
							citizen->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
							if (roll_chance_i(60))
								citizen->SetVisible(false);
							else
							{
								citizen->RemoveAllAuras();
								citizen->SetRegenerateHealth(false);
								citizen->SetHealth(0U);
								citizen->SetStandState(UNIT_STAND_STATE_DEAD);
								citizen->AddUnitFlag2(UNIT_FLAG2_FEIGN_DEATH);
								citizen->AddUnitFlag2(UNIT_FLAG2_PLAY_DEATH_ANIM);
								citizen->SetImmuneToAll(true);
							}
						}
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::Preparation);
					events.ScheduleEvent(71, 1s);
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
						SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, jaina);

						jaina->SetBoundingRadius(20.f);
						jaina->AI()->Talk(SAY_BATTLE_01);
						jaina->SetRegenerateHealth(false);

						if (GameObject* portal = jaina->SummonGameObject(GOB_PORTAL_TO_ORGRIMMAR, PortalPoint02, QuaternionData::QuaternionData(), 0))
							portal->m_Events.AddEvent(new HordeDoorsEvent(portal), portal->m_Events.CalculateTime(15 * IN_MILLISECONDS));
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
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::TheBattle_Survive);
					#ifdef DEBUG
						events.ScheduleEvent(WAVE_01, 3s);
					#else
						events.ScheduleEvent(91, 10s);
						events.ScheduleEvent(92, 1s);
					#endif
					break;
				}
				// Step 9 : The Battle - Parent
				case CRITERIA_TREE_SURVIVE_THE_BATTLE:
				{
					SpawnWoundedTroops();
					EnsurePlayerHaveBucket();
					if (Creature* jaina = GetJaina())
					{
						SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, jaina);

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
					for (uint8 i = 0; i < ARCHMAGES_RELOCATION; i++)
					{
						ObjectGuid guid = Trinity::Containers::SelectRandomContainerElement(troops);
						if (Creature* creature = instance->GetCreature(guid))
						{
							creature->NearTeleportTo(UnitLocation[i]);
							creature->SetHomePosition(UnitLocation[i]);
							creature->SetSheath(SHEATH_STATE_UNARMED);
							creature->SetStandState(UNIT_STAND_STATE_STAND);
							creature->SetEmoteState(EMOTE_STATE_NONE);
							creature->RemoveAllAuras();
							creature->Dismount();

							switch (i)
							{
								case 0:
									creature->SetStandState(UNIT_STAND_STATE_SIT);
									creature->SetEmoteState(EMOTE_STATE_EAT);
									break;
								case 1:
									creature->SetEmoteState(EMOTE_STATE_WADRUNKSTAND);
									break;
							}
						}
						if (Creature* creature = GetCreature(archmagesRelocation[i].dataId))
						{
							creature->NearTeleportTo(archmagesRelocation[i].destination);
							creature->SetHomePosition(archmagesRelocation[i].destination);
							creature->SetSheath(SHEATH_STATE_UNARMED);
							creature->RemoveAllAuras();

							if (creature->GetEntry() == NPC_TARI_COGG)
							{
								creature->SetEmoteState(EMOTE_STATE_EAT);
								creature->SetStandState(UNIT_STAND_STATE_SIT);
								creature->SummonGameObject(GOB_REFRESHMENT, TablePoint01, QuaternionData::fromEulerAnglesZYX(TablePoint01.GetOrientation(), 0.f, 0.f), 0);
							}
							else if (creature->GetEntry() == NPC_KINNDY_SPARKSHINE)
							{
								creature->SetEmoteState(EMOTE_STATE_CRY);
							}
						}
					}
					GetBarrier01()->ResetDoorOrButton();
					GetBarrier02()->ResetDoorOrButton();
					SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, GetRhonin());
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::HelpTheWounded);
					events.ScheduleEvent(122, 3s);
					break;
				}
				// Step 10 : Help the wounded - Parent
				case CRITERIA_TREE_HELP_THE_WOUNDED:
					for (uint8 i = 128; i < 141; i++)
						events.CancelEvent(i);
					DoRemoveAurasDueToSpellOnPlayers(SPELL_RUNIC_SHIELD);
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::WaitForAmara);
					events.ScheduleEvent(141, 5ms);
					break;
				// Step 10 : Help the wounded - Rejoin Lady Jaina Proudmoore after the attack
				case CRITERIA_TREE_FOLLOW_JAINA:
					events.ScheduleEvent(128, 3s);
					break;
				// Step 10 : Help the wounded - Help teleporting the wounded troops
				case CRITERIA_TREE_HELP_WOUNDED_TROOP:
					MassDespawn(NPC_THERAMORE_WOUNDED_TROOP);
					break;
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
						kalecgos->SetSpeedRate(MOVE_RUN, 0.85f);
					}
					if (Creature* rhonin = GetRhonin())
					{
						rhonin->SetSpeedRate(MOVE_RUN, 0.85f);
						rhonin->SetSpeedRate(MOVE_WALK, 0.85f);
						rhonin->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, RhoninPoint01, true, RhoninPoint01.GetOrientation());
					}
					for (uint8 i = 0; i < CREATURE_DATA_SIZE; i++)
					{
						if (Creature* creature = GetCreature(creatureData[i].type))
							creature->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
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

			creature->SetVisibilityDistanceOverride(VisibilityDistanceType::Large);

			switch (creature->GetEntry())
			{
				case NPC_ARCHMAGE_TERVOSH:
					creature->SetEmoteState(EMOTE_STATE_READ_BOOK_AND_TALK);
					break;
				case NPC_EVENT_THERAMORE_TRAINING:
				case NPC_EVENT_THERAMORE_FAITHFUL:
					dummies.push_back(creature->GetGUID());
					break;
				case NPC_THERAMORE_CITIZEN_FEMALE:
				case NPC_THERAMORE_CITIZEN_MALE:
					citizens.push_back(creature->GetGUID());
					if (phase == BFTPhases::Evacuation)
						break;
					creature->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
					break;
				case NPC_ALLIANCE_PEASANT:
					creature->SetImmuneToNPC(true);
					citizens.push_back(creature->GetGUID());
					break;
				case NPC_UNMANNED_TANK:
					creature->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
					tanks.push_back(creature->GetGUID());
					break;
				case NPC_THERAMORE_FOOTMAN:
				case NPC_THERAMORE_ARCANIST:
				case NPC_THERAMORE_FAITHFUL:
				case NPC_THERAMORE_OFFICER:
					troops.push_back(creature->GetGUID());
					break;
			}
		}

		void OnGameObjectCreate(GameObject* go) override
		{
			InstanceScript::OnGameObjectCreate(go);

			go->SetVisibilityDistanceOverride(VisibilityDistanceType::Large);

			switch (go->GetEntry())
			{
				case GOB_PORTAL_TO_DALARAN:
				case GOB_PORTAL_TO_STORMWIND:
				case GOB_PORTAL_TO_ORGRIMMAR:
					go->SetLootState(GO_READY);
					go->UseDoorOrButton();
					go->SetFlags(GO_FLAG_NOT_SELECTABLE);
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
					#ifdef DEBUG
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
					DoSendScenarioEvent(EVENT_WAITING);
					break;

				#pragma endregion

				// The Unknown Tauren
				#pragma region THE_UNKNOWN_TAUREN

				case 25:
					for (uint8 i = 0; i < PERITH_LOCATION; i++)
					{
						if (Creature* creature = GetCreature(perithLocation[i].dataId))
						{
							creature->SetWalk(true);
							creature->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
							creature->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, perithLocation[i].destination, false, perithLocation[i].destination.GetOrientation());
						}
					}
					#ifdef DEBUG
						events.ScheduleEvent(70, 2s);
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
						jaina->RemoveUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
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
					GetPained()->RemoveUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
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
					#ifdef DEBUG
						events.ScheduleEvent(77, 1s);
					#else
						EnsurePlayerHaveShaker();
						if (Creature* hedric = GetHedric())
						{
							hedric->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
							hedric->GetMotionMaster()->Clear();
							hedric->GetMotionMaster()->MoveIdle();
							hedric->NearTeleportTo(HedricPoint01);
							hedric->PlayDirectSound(SOUND_FEARFUL_CROWD);
							Talk(hedric, SAY_PRE_BATTLE_1);
						}
						Next(3s);
					#endif
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
					GetJaina()->SummonGameObject(GOB_PORTAL_TO_DALARAN, PortalPoint01, QuaternionData::QuaternionData(), 0);
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
					{
						#ifdef DEBUG
							events.ScheduleEvent(90, 1s);
						#else
							Next(2s);
						#endif
					}
					else if (Creature* creature = GetCreature(archmagesLocation[archmagesIndex].dataId))
					{
						creature->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
						creature->SetSheath(SHEATH_STATE_UNARMED);
						creature->SetVisible(true);
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
						jaina->RemoveUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
					}
					Next(2s);
					break;
				case 88:
					if (Creature* jaina = GetJaina())
					{
						Talk(jaina, SAY_PRE_BATTLE_15);
						if (Player* player = jaina->SelectNearestPlayer(150.f))
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
					for (uint8 i = 0; i < ACTORS_RELOCATION; i++)
					{
						if (Creature* creature = GetCreature(actorsRelocation[i].dataId))
						{
							creature->GetMotionMaster()->Clear();
							creature->GetMotionMaster()->MoveIdle();
							creature->NearTeleportTo(actorsRelocation[i].destination);
							creature->SetHomePosition(actorsRelocation[i].destination);
							creature->RemoveUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
							creature->SetSheath(SHEATH_STATE_MELEE);

							switch (creature->GetEntry())
							{
								case NPC_AMARA_LEESON:
									creature->CastSpell(creature, SPELL_PORTAL_CHANNELING_03);
									break;
								case NPC_THADER_WINDERMERE:
									creature->AddNpcFlag(UNIT_NPC_FLAG_GOSSIP);
									break;
								case NPC_THALEN_SONGWEAVER:
									creature->CastSpell(creature, SPELL_PORTAL_CHANNELING_02);
									break;
								case NPC_RHONIN:
									creature->AddNpcFlag(UNIT_NPC_FLAG_GOSSIP);
									break;
								case NPC_ARCHMAGE_TERVOSH:
									creature->RemoveAllAuras();
									break;
								case NPC_JAINA_PROUDMOORE:
									GetBarrier01()->UseDoorOrButton();
									TeleportPlayers(GetJaina(), actorsRelocation[i].destination, 15.0f);
									break;
								case NPC_KALECGOS_DRAGON:
									creature->m_Events.AddEvent(new KalecgosLoopEvent(creature), creature->m_Events.CalculateTime(1 * IN_MILLISECONDS));
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
					GetJaina()->AI()->Talk(SAY_BATTLE_02);
					events.ScheduleEvent(93, 10s);
					break;
				// DELETED
				//case 92:
				//    break;
				case 93:
					if (Creature* thalen = GetThalen())
					{
						thalen->AddUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
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
						if (Creature* trigger = barrier->SummonCreature(WORLD_TRIGGER, ExplodingPoint01, TEMPSUMMON_TIMED_DESPAWN, 2s))
							trigger->CastSpell(trigger, SPELL_BIG_EXPLOSION);
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
					GetJaina()->AI()->Talk(SAY_BATTLE_03);
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
						jaina->AI()->Talk(SAY_BATTLE_04);
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
					Next(3s);
					break;
				case 100:
					if (Creature* thalen = GetThalen())
					{
						thalen->RemoveAllAuras();
						thalen->NearTeleportTo(ThalenPoint02);
						thalen->SetHomePosition(ThalenPoint02);
						thalen->CastSpell(thalen, SPELL_TIED_UP, true);
					}
					if (Creature* kalecgos = GetKalecgos())
					{
						kalecgos->m_Events.AddEvent(new KalecgosSpellEvent(kalecgos), kalecgos->m_Events.CalculateTime(2 * IN_MILLISECONDS));
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
					#ifdef DEBUG
						for (uint8 i = 0; i < 10; i++)
							DoCastSpellOnPlayers(SPELL_KILL_CREDIT);
					#else
						HordeMembersInvoker(Waves[waves], hordeMembers);
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
					uint32 membersCounter = 0;
					uint32 deadCounter = 0;
					for (uint8 i = 0; i < NUMBER_OF_MEMBERS; ++i)
					{
						++membersCounter;
						Creature* temp = ObjectAccessor::GetCreature(*GetJaina(), hordeMembers[i]);
						if (!temp || temp->isDead())
							++deadCounter;
					}

					// Quand le nombre de membres vivants est inférieur ou égal au nom de membres morts
					if (membersCounter <= deadCounter)
					{
						DoCastSpellOnPlayers(SPELL_KILL_CREDIT);
						events.ScheduleEvent(++wavesInvoker, 2s);
					}
					else
						events.ScheduleEvent(wavesInvoker, 1s);

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
					GetJaina()->AI()->Talk(SAY_POST_BATTLE_01);
					Next(2s);
					break;
				case 124:
					GetHedric()->AI()->Talk(SAY_POST_BATTLE_02);
					Next(4s);
					break;
				case 125:
					GetJaina()->AI()->Talk(SAY_POST_BATTLE_03);
					Next(4s);
					break;
				case 126:
					ClearTarget();
					GetJaina()->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_02, JainaPath01, JAINA_PATH_01);
					Next(1500ms);
					break;
				case 127:
					GetHedric()->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_NONE, JainaPath01, JAINA_PATH_01);
					break;

				// PART II
				case 128:
				{
					#ifdef DEBUG
						events.ScheduleEvent(141, 2s);
					#else
						if (Creature* jaina = GetJaina())
						{
							if (Creature* kinndy = GetKinndy())
							{
								jaina->SetTarget(kinndy->GetGUID());
								kinndy->SetTarget(jaina->GetGUID());
							}
						}
						Next(800ms);
					#endif
					break;
				}
				case 129:
					GetKinndy()->AI()->Talk(SAY_POST_BATTLE_04);
					Next(5s);
					break;
				case 130:
					GetJaina()->AI()->Talk(SAY_POST_BATTLE_05);
					Next(7s);
					break;
				case 131:
					if (Creature* kinndy = GetKinndy())
					{
						kinndy->AI()->Talk(SAY_POST_BATTLE_06);
						kinndy->SetEmoteState(EMOTE_STATE_NONE);
					}
					Next(4s);
					break;
				case 132:
					GetJaina()->AI()->Talk(SAY_POST_BATTLE_07);
					Next(4s);
					break;
				case 133:
					GetKinndy()->AI()->Talk(SAY_POST_BATTLE_08);
					Next(3s);
					break;
				case 134:
					GetJaina()->AI()->Talk(SAY_POST_BATTLE_09);
					Next(13s);
					break;
				case 135:
					GetJaina()->AI()->Talk(SAY_POST_BATTLE_10);
					Next(7s);
					break;
				case 136:
					GetKinndy()->AI()->Talk(SAY_POST_BATTLE_11);
					Next(8s);
					break;
				case 137:
					GetJaina()->AI()->Talk(SAY_POST_BATTLE_12);
					Next(6s);
					break;
				case 138:
					GetJaina()->AI()->Talk(SAY_POST_BATTLE_13);
					Next(12s);
					break;
				case 139:
					GetKinndy()->AI()->Talk(SAY_POST_BATTLE_14);
					Next(3s);
					break;
				case 140:
					GetJaina()->AI()->Talk(SAY_POST_BATTLE_15);
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
						kalecgos->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_NONE, KalecgosPath02, KALECGOS_PATH_02);
					}
					Next(5s);
					break;
				case 143:
					SetTarget(GetKalec());
					GetKalec()->AI()->Talk(SAY_IRIS_WARN_01);
					Next(1s);
					break;
				case 144:
					SetTarget(GetJaina());
					GetJaina()->AI()->Talk(SAY_IRIS_WARN_02);
					Next(2s);
					break;
				case 145:
					SetTarget(GetKalec());
					GetKalec()->AI()->Talk(SAY_IRIS_WARN_03);
					Next(4s);
					break;
				case 146:
					SetTarget(GetJaina());
					GetJaina()->AI()->Talk(SAY_IRIS_WARN_04);
					Next(8s);
					break;
				case 147:
					SetTarget(GetKalec());
					GetKalec()->AI()->Talk(SAY_IRIS_WARN_05);
					Next(2s);
					break;
				case 148:
					SetTarget(GetJaina());
					GetJaina()->AI()->Talk(SAY_IRIS_WARN_06);
					Next(6s);
					break;
				case 149:
					GetJaina()->AI()->Talk(SAY_IRIS_WARN_07);
					Next(2s);
					break;
				case 150:
					SetTarget(GetKalec());
					GetKalec()->AI()->Talk(SAY_IRIS_WARN_08);
					Next(7s);
					break;
				case 151:
					SetTarget(GetRhonin());
					GetRhonin()->AI()->Talk(SAY_IRIS_WARN_09);
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
					GetAmara()->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_01, AmaraPath01, AMARA_PATH_01);
					Next(10s);
					break;
				case 155:
					if (Creature* amara = GetAmara())
					{
						amara->SetVisible(true);
						amara->SetSpeedRate(MOVE_RUN, 0.85f);
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
					GetAmara()->AI()->Talk(SAY_IRIS_WARN_10);
					Next(6s);
					break;
				case 158:
					GetJaina()->AI()->Talk(SAY_IRIS_WARN_11);
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
						jaina->AI()->Talk(SAY_IRIS_XPLOSION_01);
						jaina->RemoveUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
					}
					Next(3s);
					break;
				case 162:
					if (Creature* jaina = GetJaina())
					{
						jaina->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
						jaina->SetFacingToObject(GetRhonin());
						jaina->RemoveAllAuras();

						if (Creature* rhonin = GetRhonin())
						{
							jaina->SetTarget(rhonin->GetGUID());

							rhonin->SetTarget(jaina->GetGUID());
							rhonin->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
						}
					}
					Next(3s);
					break;
				case 163:
					GetRhonin()->AI()->Talk(SAY_IRIS_XPLOSION_02);
					Next(4s);
					break;
				case 164:
					GetJaina()->AI()->Talk(SAY_IRIS_XPLOSION_03);
					Next(6s);
					break;
				case 165:
					GetRhonin()->AI()->Talk(SAY_IRIS_XPLOSION_04);
					Next(5s);
					break;
				case 166:
					GetRhonin()->AI()->Talk(SAY_IRIS_XPLOSION_05);
					Next(8s);
					break;
				case 167:
					GetRhonin()->AI()->Talk(SAY_IRIS_XPLOSION_06);
					Next(6s);
					break;
				case 168:
					GetJaina()->AI()->Talk(SAY_IRIS_XPLOSION_07);
					Next(6s);
					break;
				case 169:
					GetRhonin()->AI()->Talk(SAY_IRIS_XPLOSION_08);
					Next(3s);
					break;
				case 170:
					GetJaina()->AI()->Talk(SAY_IRIS_XPLOSION_09);
					Next(5s);
					break;
				case 171:
					if (Creature* rhonin = GetRhonin())
					{
						rhonin->AI()->Talk(SAY_IRIS_XPLOSION_10);
						rhonin->RemoveUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
						rhonin->RemoveAllAuras();
					}
					Next(5s);
					break;
				case 172:
					DoSendScenarioEvent(EVENT_REDUCE_IMPACT);
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
		GuidVector dummies;
		GuidVector tanks;
		GuidVector troops;
		std::list<TempSummon*> hordes;
		ObjectGuid hordeMembers[NUMBER_OF_MEMBERS];

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
			for (uint8 i = 0; i < CREATURE_DATA_SIZE; i++)
			{
				if (Creature* creature = GetCreature(creatureData[i].type))
				{
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
			for (uint8 i = 0; i < CREATURE_DATA_SIZE; i++)
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
				if (Creature* special = portal->SummonTrigger(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), pos.GetOrientation(), 5 * IN_MILLISECONDS))
				{
					special->CastSpell(special, SPELL_CLOSE_PORTAL, args);
				}
			}
		}

		void TeleportPlayers(Creature* caster, const Position center, float minDist)
		{
			Map::PlayerList const& PlayerList = instance->GetPlayers();
			if (PlayerList.isEmpty())
				return;

			Position pos = GetRandomPosition(center, 8.f);
			for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
			{
				if (Player* player = i->GetSource())
				{
					if (player->IsWithinDist(caster, minDist))
					{
						float x = pos.GetPositionX();
						float y = pos.GetPositionY();
						float z = pos.GetPositionZ();

						player->UpdateGroundPositionZ(x, y, z);
						player->NearTeleportTo(x, y, z, pos.GetOrientation());
					}
				}
			}
		}

		void HordeMembersInvoker(uint32 waveId, ObjectGuid* hordes)
		{
			uint8 healers = 0, dps = 0;
			for (uint32 i = 0; i < NUMBER_OF_MEMBERS; ++i)
			{
				uint32 entry = NPC_ROKNAH_GRUNT;

				if (dps < 5)
				{
					entry = RAND(NPC_ROKNAH_FELCASTER, NPC_ROKNAH_HAG);
					dps++;
				}

				if (healers < 2)
				{
					entry = NPC_ROKNAH_LOA_SINGER;
					healers++;
				}

				Position pos = {};
				switch (waveId)
				{
					case WAVE_DOORS:
						pos = GetRandomPosition(JainaPoint03, 13.f);
						break;
					case WAVE_CITADEL:
						pos = GetRandomPosition(CitadelPoint01, 20.f);
						break;
					case WAVE_DOCKS:
						pos = GetRandomPosition(DocksPoint01, 25.f);
						break;
				}

				if (Creature* temp = instance->SummonCreature(entry, pos))
				{
					float x = pos.GetPositionX();
					float y = pos.GetPositionY();
					float z = pos.GetPositionZ();

					temp->SetBoundingRadius(20.f);
					temp->UpdateGroundPositionZ(x, y, z);
					temp->NearTeleportTo(x, y, z, pos.GetOrientation());
					temp->SetHomePosition(x, y, z, pos.GetOrientation());
					temp->CastSpell(temp, SPELL_TELEPORT_DUMMY, true);

					if (Unit * target = temp->SelectNearestHostileUnitInAggroRange())
						temp->AI()->AttackStart(target);

					hordes[i] = temp->GetGUID();
				}
			}

			SendJainaWarning(waveId);
		}

		void SendJainaWarning(uint32 spawnNumber)
		{
			uint8 groupId = 0;
			Position position;
			switch (spawnNumber)
			{
				// Portes
				case WAVE_DOORS:
					position = JainaPoint03;
					groupId = SAY_BATTLE_GATE;
					break;
				// Citadelle
				case WAVE_CITADEL:
					position = CitadelPoint01;
					groupId = SAY_BATTLE_CITADEL;
					break;
				// Docks
				case WAVE_DOCKS:
					position = DocksPoint01;
					groupId = SAY_BATTLE_DOCKS;
					break;
			}

			if (Creature* jaina = GetJaina())
			{
				jaina->NearTeleportTo(position);
				jaina->SetHomePosition(position);
				jaina->AI()->Talk(SAY_BATTLE_ALERT);
				jaina->AI()->Talk(groupId);

				Map::PlayerList const& PlayerList = instance->GetPlayers();
				if (PlayerList.isEmpty())
					return;

				for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
				{
					if (Player* player = i->GetSource())
					{
						if (player->IsWithinDist(jaina, 25.f))
						{
							Position playerPos = GetRandomPosition(position, 3.f);

							// Si le joueur est à plus de 25 mètre de la destination d'attaque
							float distance = playerPos.GetExactDist2d(player->GetPosition());
							if (distance > 25.0f)
							{
								float x = playerPos.GetPositionX();
								float y = playerPos.GetPositionY();
								float z = playerPos.GetPositionZ();

								player->UpdateGroundPositionZ(x, y, z);
								player->NearTeleportTo(playerPos);
								player->CastSpell(player, SPELL_TELEPORT_DUMMY);
							}
						}
					}
				}
			}
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
			GuidVector::iterator itr = troops.begin();
			while (itr != troops.end())
			{
				if (Creature* troop = instance->GetCreature(*itr))
				{
					if (!troop->IsFormationLeader() && roll_chance_i(60))
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

							itr = troops.erase(itr);
						}
					}
					else
					{
						itr++;
					}
				}
			}
		}

		void EnsurePlayersAreInPhase(uint32 phaseId)
		{
			Map::PlayerList const& players = instance->GetPlayers();
			if (players.isEmpty())
				return;

			for (Map::PlayerList::const_iterator i = players.begin(); i != players.end(); ++i)
			{
				if (Player* player = i->GetSource())
				{
					PhasingHandler::AddPhase(player, phaseId, true);
				}
			}
		}

		void EnsurePlayersHaveAura(uint32 entry)
		{
			Map::PlayerList const& players = instance->GetPlayers();
			if (players.isEmpty())
				return;

			for (Map::PlayerList::const_iterator i = players.begin(); i != players.end(); ++i)
			{
				if (Player* player = i->GetSource())
				{
					if (player->HasAura(entry))
						continue;

					player->CastSpell(player, entry, true);
				}
			}
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

		Position GetRandomPosition(Position center, float dist)
		{
			float alpha = 2 * float(M_PI) * float(rand_norm());
			float r = dist * sqrtf(float(rand_norm()));
			float x = r * cosf(alpha) + center.GetPositionX();
			float y = r * sinf(alpha) + center.GetPositionY();
			return { x, y, center.GetPositionZ(), 0.f };
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

	void OnSceneStart(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* sceneTemplate) override
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

void AddSC_scenario_battle_for_theramore()
{
	new scenario_battle_for_theramore();
	new scene_theramore_explosion();
}
