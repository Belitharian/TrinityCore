#include "EventMap.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "KillRewarder.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "TemporarySummon.h"
#include "ruins_of_theramore.h"

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

class scenario_ruins_of_theramore : public InstanceMapScript
{
	public:
	scenario_ruins_of_theramore() : InstanceMapScript(RFTScriptName, 5001)
	{
	}

	struct scenario_ruins_of_theramore_InstanceScript : public InstanceScript
	{
        scenario_ruins_of_theramore_InstanceScript(InstanceMap* map) : InstanceScript(map),
            eventId(1), hordeCounter(0), phase(RFTPhases::FindJaina_Isle), irisDummy(ObjectGuid::Empty)
		{
			SetHeaders(DataHeader);
			LoadObjectData(creatureData, gameobjectData);
		}

		uint32 GetData(uint32 dataId) const override
		{
			if (dataId == DATA_SCENARIO_PHASE)
				return (uint32)phase;
			return 0U;
		}

		void OnPlayerEnter(Player* player) override
		{
            RFTPhases phase = (RFTPhases)GetData(DATA_SCENARIO_PHASE);
            if (phase >= RFTPhases::FindJaina_Isle_Valided)
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
					if (phase == RFTPhases::LeaveTheRuins)
						events.ScheduleEvent(40, 1s);
					break;
				case EVENT_FIND_JAINA_02:
					SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::FindJaina_Crater_Valided);
					events.ScheduleEvent(19, 500ms);
					break;
				case EVENT_BACK_TO_SENDER:
					SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::BackToSender);
					events.ScheduleEvent(25, 1s);
					break;
				case EVENT_WARLORD_ROKNAH_SLAIN:
					GetJaina()->CastSpell(GetJaina(), SPELL_EXPLOSIVE_BRAND, true);
					for (Creature* horde : hordes)
					{
						if (horde && horde->IsAlive() && horde->GetEntry() != NPC_ROKNAH_WARLORD)
							horde->CastSpell(horde, SPELL_EXPLOSIVE_BRAND_DAMAGE);
					}
					events.ScheduleEvent(39, 800ms);
					break;
			}
		}

		void OnCompletedCriteriaTree(CriteriaTree const* tree) override
		{
			switch (tree->ID)
			{
				// Retrieve Jaina
				case CRITERIA_TREE_FIND_JAINA_01:
				{
					instance->SummonCreature(NPC_KALECGOS, KalecgosPath01[0]);
					if (Creature* kalecgos = GetKalecgos())
					{
						kalecgos->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
						kalecgos->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_NONE, KalecgosPath01, KALECGOS_PATH_01, false, false, KalecgosPath01[KALECGOS_PATH_01 - 1].GetOrientation());
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::FindJaina_Isle_Valided);
					#ifdef CUSTOM_DEBUG
						events.ScheduleEvent(17, 1s);
					#else
						events.ScheduleEvent(1, 1s);
					#endif
					break;
				}
				// Help Kalecgos
				case CRITERIA_TREE_HELP_KALECGOS:
					if (Creature* jaina = GetJaina())
					{
						jaina->LoadEquipment(2);
						jaina->RemoveUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
						jaina->SetStandState(UNIT_STAND_STATE_KNEEL);
						jaina->RemoveAllAuras();
						jaina->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);

						// Distance minimale pour déclencher l'event
						jaina->AI()->SetData(0U, 50U);
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::FindJaina_Crater);
					break;
				// Return to Theramore
				case CRITERIA_TREE_FIND_JAINA_02:
					SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::Standards);
					break;
				// Cleaning
				case CRITERIA_TREE_CLEANING:
                {
                    std::list<TempSummon*> hordes;
                    instance->SummonCreatureGroup(0, &hordes);
                    hordeCounter = (uint32)hordes.size();
                    for (TempSummon* horde : hordes)
                    {
                        horde->SetMaxHealth(horde->GetMaxHealth() * 6.f);
                        horde->SetFullHealth();
                        horde->SetTempSummonType(TEMPSUMMON_TIMED_OR_DEAD_DESPAWN);

                        hordeChecker.push_back(horde->GetGUID());
                    }
                    if (Creature* jaina = GetJaina())
                    {
                        Talk(jaina, SAY_IRIS_PROTECTION_JAINA_03);
                        jaina->RemoveAurasDueToSpell(SPELL_ALUNETH_DRINKS);
                        jaina->SetHomePosition(JainaPoint04);
                        jaina->NearTeleportTo(JainaPoint04);
                    }
                    SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::Standards_Valided);
                    events.ScheduleEvent(44, 2s);
                    break;
                }
				// Back to sender
				case CRITERIA_TREE_BACK_TO_SENDER:
					SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::TheFinalAssault);
					break;
				// The last assault - Parent
				case CRITERIA_TREE_THE_LAST_STAND:
					events.ScheduleEvent(40, 1s);
					break;
				default:
					break;
			}
		}

		void OnCreatureCreate(Creature* creature) override
		{
			InstanceScript::OnCreatureCreate(creature);

			creature->SetVisibilityDistanceOverride(VisibilityDistanceType::Large);
			creature->SetPvpFlag(UNIT_BYTE2_FLAG_PVP);
			creature->SetUnitFlag(UNIT_FLAG_PVP_ENABLING);
            creature->SetBoundingRadius(36.0f);

			switch (creature->GetEntry())
			{
				case NPC_WATER_ELEMENTAL:
					elementals.push_back(creature);
					break;
				case NPC_DEAD_ROKNAH_TROOP:
					FeingDeath(creature);
					if (roll_chance_i(50))
						creature->AddAura(RAND(SPELL_GLACIAL_SPIKE_COSMETIC, SPELL_BURNING), creature);
					break;
				case NPC_GENERAL_TIRAS_ALAN:
				case NPC_ADMIRAL_AUBREY:
					creature->SetLevel(60);
				case NPC_HEDRIC_EVENCANE:
				case NPC_THERAMORE_FAITHFUL:
				case NPC_THERAMORE_ARCANIST:
				case NPC_THERAMORE_OFFICER:
				case NPC_ARCHMAGE_TERVOSH:
				case NPC_KINNDY_SPARKSHINE:
					FeingDeath(creature);
					creature->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
					creature->AddAura(SPELL_SHIMMERDUST, creature);
					creature->AddAura(SPELL_COSMETIC_PURPLE_VERTEX_STATE, creature);
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

		void Update(uint32 diff) override
		{
			events.Update(diff);
			switch (eventId = events.ExecuteEvent())
			{
				// Find Jaina - Isle
				#pragma region FIND_JAINA_ISLE

				case 1:
					if (Creature* jaina = GetJaina())
					{
						jaina->SetWalk(true);
						jaina->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
						jaina->GetMotionMaster()->MovePoint(0, JainaPoint01, true, JainaPoint01.GetOrientation());
					}
					Next(3s);
					break;
				case 2:
					if (Creature* jaina = GetJaina())
					{
						if (Creature* kalecgos = GetKalecgos())
						{
							Talk(kalecgos, SAY_AFTER_BATTLE_KALECGOS_01);
							kalecgos->SetWalk(true);
							kalecgos->SetTarget(jaina->GetGUID());

							jaina->SetTarget(kalecgos->GetGUID());
						}
					}
					Next(2s);
					break;
				case 3:
					Talk(GetJaina(), SAY_AFTER_BATTLE_JAINA_02);
					Next(6s);
					break;
				case 4:
					Talk(GetJaina(), SAY_AFTER_BATTLE_JAINA_03);
					Next(10s);
					break;
				case 5:
					if (Creature* kalecgos = GetKalecgos())
					{
						Talk(kalecgos, SAY_AFTER_BATTLE_KALECGOS_04);
						kalecgos->SetSpeedRate(MOVE_WALK, 0.6f);
						kalecgos->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, KalecgosPoint01, true, KalecgosPoint01.GetOrientation());
					}
					Next(8s);
					break;
				case 6:
					Talk(GetJaina(), SAY_AFTER_BATTLE_JAINA_05);
					Next(5s);
					break;
				case 7:
					Talk(GetKalecgos(), SAY_AFTER_BATTLE_KALECGOS_06);
					Next(4s);
					break;
				case 8:
					Talk(GetJaina(), SAY_AFTER_BATTLE_JAINA_07);
					Next(6s);
					break;
				case 9:
					Talk(GetKalecgos(), SAY_AFTER_BATTLE_KALECGOS_08);
					Next(4s);
					break;
				case 10:
					Talk(GetJaina(), SAY_AFTER_BATTLE_JAINA_09);
					Next(4s);
					break;
				case 11:
					Talk(GetKalecgos(), SAY_AFTER_BATTLE_KALECGOS_10);
					Next(6s);
					break;
				case 12:
					Talk(GetKalecgos(), SAY_AFTER_BATTLE_KALECGOS_11);
					Next(7s);
					break;
				case 13:
					Talk(GetJaina(), SAY_AFTER_BATTLE_JAINA_12);
					Next(2s);
					break;
				case 14:
					if (TempSummon* trigger = instance->SummonCreature(WORLD_TRIGGER, GetJaina()->GetPosition(), nullptr, 10 * IN_MILLISECONDS))
						trigger->CastSpell(trigger, SPELL_ECHO_OF_ALUNETH_SPAWN, true);
					Next(8s);
					break;
				case 15:
					GetKalecgos()->SetTarget(ObjectGuid::Empty);
					if (Creature* jaina = GetJaina())
					{
						jaina->SetTarget(ObjectGuid::Empty);
						jaina->CastSpell(jaina, SPELL_COSMETIC_ARCANE_DISSOLVE, true);
						if (TempSummon* trigger = instance->SummonCreature(WORLD_TRIGGER, GetJaina()->GetPosition(), nullptr, 5 * IN_MILLISECONDS))
							trigger->CastSpell(trigger, SPELL_ALUNETH_FREED_EXPLOSION, true);
					}
					Next(800ms);
					break;
				case 16:
					if (Creature* jaina = GetJaina())
						jaina->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
					Next(2s);
					break;
				case 17:
					GetJaina()->NearTeleportTo(JainaPoint02);
					if (Creature* kalecgos = GetKalecgos())
					{
						Talk(kalecgos, SAY_AFTER_BATTLE_KALECGOS_13);
						kalecgos->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
                        if (instance->GetPlayers().isEmpty())
                            return;
						if (Player* player = instance->GetPlayers().begin()->GetSource())
							kalecgos->SetFacingToObject(player);
					}
					Next(5s);
					break;
				case 18:
					DoTeleportPlayers(instance->GetPlayers(), PlayerPoint01, 12.f);
					DoRemoveAurasDueToSpellOnPlayers(SPELL_SKYBOX_EFFECT_ENTRANCE);
					DoCastSpellOnPlayers(SPELL_SKYBOX_EFFECT_RUINS);
					TriggerGameEvent(EVENT_HELP_KALECGOS);
					break;

					#pragma endregion

				// Find Jaina - Crater
				#pragma region FIND_JAINA_CRATER

				case 19:
					if (Creature* jaina = GetJaina())
						jaina->SetStandState(UNIT_STAND_STATE_STAND);
					Next(1800ms);
					break;
				case 20:
					if (Player* player = instance->GetPlayers().begin()->GetSource())
						GetJaina()->SetFacingToObject(player);
					if (Creature* kinndy = GetCreature(DATA_KINNDY_SPARKSHINE))
						kinndy->AddAura(SPELL_COSMETIC_ARCANE_DISSOLVE, kinndy);
					Next(2s);
					break;
				case 21:
					Talk(GetJaina(), SAY_IRIS_PROTECTION_JAINA_01);
					Next(4s);
					break;
				case 22:
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
					break;
				case 23:
					Talk(GetJaina(), SAY_IRIS_PROTECTION_JAINA_02);
					Next(6s);
					break;
				case 24:
					TriggerGameEvent(EVENT_FIND_JAINA_02);
					if (Creature* dummy = instance->SummonCreature(WORLD_TRIGGER, DummyPoint01))
					{
						dummy->SetObjectScale(0.6f);
						dummy->CastSpell(GetJaina(), SPELL_ALUNETH_DRINKS);
						dummy->CastSpell(dummy, SPELL_EMPOWERED_SUMMON, true);
						irisDummy = dummy->GetGUID();
					}
					break;

				#pragma endregion

				// Back to sender
				#pragma region BACK_TO_SENDER

				case 25:
					if (Creature* jaina = GetJaina())
					{
						if (Creature* dummy = instance->GetCreature(irisDummy))
						{
							dummy->RemoveAllAuras();
							dummy->SetObjectScale(5.0f);
							dummy->AddAura(SPELL_COSMETIC_ARCANE_ENERGY_1, dummy);
						}
					}
					Next(2s);
					break;
				case 26:
					if (Creature* jaina = GetJaina())
					{
						Talk(jaina, SAY_IRIS_PROTECTION_JAINA_05);
						jaina->SetFacingTo(JainaPoint03.GetOrientation());
					}
					Next(6s);
					break;
				case 27:
					GetJaina()->CastSpell(GetJaina(), SPELL_SUMMON_WATER_ELEMENTALS);
					TriggerGameEvent(EVENT_BACK_TO_SENDER);
					Next(2s);
					break;
				case 28:
					if (Creature* jaina = GetJaina())
					{
						jaina->CastSpell(GetJaina(), SPELL_ARCANE_CHANNELING);
						for (uint8 i = 0; i < ELEMENTALS_SIZE; ++i)
						{
							elementals[i]->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, ElementalsPoint[i].destination, true, ElementalsPoint[i].destination.GetOrientation());
							elementals[i]->SetHomePosition(ElementalsPoint[i].destination);
							elementals[i]->SetBoundingRadius(4.f);
						}
					}
					Next(5s);
					break;
				case 29:
					if (TempSummon* zeppelin = instance->SummonCreature(NPC_BOMBARDING_ZEPPELIN, ZeppelinPoint.spawn, nullptr, 13 * IN_MILLISECONDS))
					{
						zeppelin->SetSpeedRate(MOVE_RUN, 5.f);
						zeppelin->PlayDirectSound(SOUND_ZEPPELIN_FLIGHT);
						zeppelin->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, ZeppelinPoint.destination, false);
					}
					Next(3s);
					break;
				case 30:
					hordes.clear();
					instance->SummonCreatureGroup(1, &hordes);
					for (TempSummon* horde : hordes)
					{
                        horde->SetTempSummonType(TEMPSUMMON_TIMED_OR_DEAD_DESPAWN);
                        horde->SetImmuneToAll(true);
						horde->CastSpell(horde, SPELL_THALYSSRA_SPAWNS);
					}
					Next(4s);
					break;
				case 31:
					Talk(GetWarlord(), SAY_IRIS_PROTECTION_JAINA_06);
					Next(6s);
					break;
				case 32:
					Talk(GetJaina(), SAY_IRIS_PROTECTION_JAINA_07);
					Next(8s);
					break;
				case 33:
					Talk(GetWarlord(), SAY_IRIS_PROTECTION_JAINA_08);
					Next(6s);
					break;
				case 34:
					Talk(GetJaina(), SAY_IRIS_PROTECTION_JAINA_09);
					Next(9s);
					break;
				case 35:
					Talk(GetWarlord(), SAY_IRIS_PROTECTION_JAINA_10);
					Next(2s);
					break;
				case 36:
					for (Creature* horde : hordes)
					{
						horde->SetImmuneToAll(false);
						horde->GetMotionMaster()->Clear();
						horde->GetMotionMaster()->MoveIdle();

						if (horde->GetEntry() == NPC_ROKNAH_WARLORD)
						{
							if (Player* player = instance->GetPlayers().begin()->GetSource())
								horde->AI()->AttackStart(player);
						}
						else
						{
							if (horde->GetPositionY() <= -4468.18f)
								horde->AI()->AttackStart(elementals[1]);
							else
								horde->AI()->AttackStart(elementals[0]);
						}
					}
					Next(1s);
					break;
				case 37:
					if (Creature* jaina = GetJaina())
						jaina->SetImmuneToAll(true);
					Next(2s);
					break;
				case 38:
				{
					uint32 membersCounter = 0;
					uint32 deadCounter = 0;

					for (Creature* horde : hordes)
					{
						if (horde && horde->GetEntry() != NPC_ROKNAH_WARLORD)
						{
							++membersCounter;
							if (horde->isDead())
								++deadCounter;
						}
					}

					if (membersCounter <= deadCounter)
					{
						TriggerGameEvent(EVENT_JAINA_PROTECTED);
						events.CancelEvent(38);
					}
					else
						events.RescheduleEvent(38, 1s);

					break;
				}

				#pragma endregion

				// Leave the Ruins
				#pragma region LEAVE_THE_RUINS

				case 39:
					if (Creature* jaina = GetJaina())
					{
						for (Creature* horde : hordes)
						{
							if (horde && horde->IsAlive() && horde->GetEntry() != NPC_ROKNAH_WARLORD)
								horde->KillSelf();
						}

						if (Creature* dummy = instance->GetCreature(irisDummy))
							dummy->DespawnOrUnsummon();

						jaina->SetImmuneToAll(true);
						jaina->SetWalk(true);

						if (GameObject* brokenGlass = GetGameObject(DATA_BROKEN_GLASS))
						{
							if (TempSummon* trigger = instance->SummonCreature(WORLD_TRIGGER, brokenGlass->GetPosition()))
								jaina->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_01, trigger, 0.8f);
						}
					}
					break;
				case 40:
					if (Creature* jaina = GetJaina())
					{
						Talk(jaina, SAY_LEAVE_THE_RUINS_JAINA_01);
						if (Player* player = instance->GetPlayers().begin()->GetSource())
							jaina->SetFacingToObject(player);
					}
					Next(9s);
					break;
				case 41:
					if (Creature* jaina = GetJaina())
					{
						Talk(jaina, SAY_LEAVE_THE_RUINS_JAINA_02);
						if (Player* player = instance->GetPlayers().begin()->GetSource())
							jaina->SetFacingToObject(player);
					}
					Next(10s);
					break;
				case 42:
					if (Creature* jaina = GetJaina())
					{
						GameObject* portal = jaina->SummonGameObject(GOB_PORTAL_TO_STORMWIND, jaina->GetPosition(), QuaternionData::fromEulerAnglesZYX(jaina->GetOrientation(), 0.f, 0.f), 0s);
						if (portal)
						{
							portal->SetGoState(GO_STATE_ACTIVE);
							portal->UseDoorOrButton();
						}
					}
					Next(1s);
					break;
				case 43:
					if (Creature* jaina = GetJaina())
					{
						for (Creature* elemental : elementals)
							elemental->DespawnOrUnsummon(1s);

						jaina->CastSpell(jaina, SPELL_COSMETIC_ARCANE_DISSOLVE);
						jaina->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
					}
					break;

				#pragma endregion

				// Check dead hordes
				#pragma region HORDE_CHECKER

				case 44:
				{
					uint32 deadCounter = 0;

					if (Creature* jaina = GetJaina())
					{
						for (uint8 i = 0; i < hordeCounter; ++i)
						{
							Creature* temp = ObjectAccessor::GetCreature(*jaina, hordeChecker[i]);
							if (temp && temp->IsAlive() && !temp->IsEngaged())
								temp->AI()->AttackStart(jaina);

							if (!temp || temp->isDead())
								++deadCounter;
						}

						// Quand le nombre de membres vivants est inférieur ou égal au nombre de membres morts
						if (deadCounter >= hordeCounter)
						{
							Talk(jaina, SAY_IRIS_PROTECTION_JAINA_04);
							if (Player* player = instance->GetPlayers().begin()->GetSource())
								jaina->SetFacingToObject(player);

							jaina->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
							jaina->SetReactState(REACT_PASSIVE);

							events.CancelEvent(44);

							Next(5s);
						}
						else
							events.RescheduleEvent(44, 1s);
					}

					break;
				}
				case 45:
					if (Creature* jaina = GetJaina())
					{
						jaina->SetWalk(false);
						jaina->SetHomePosition(JainaPoint03);
						jaina->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_03, JainaPoint03, true, JainaPoint03.GetOrientation());
					}
					break;

				#pragma endregion

				default:
					break;
			}
		}

		EventMap events;
		uint32 eventId;
		uint32 hordeCounter;
		RFTPhases phase;
		ObjectGuid irisDummy;
		GuidVector hordeChecker;
		std::vector<Creature*> elementals;
		std::list<TempSummon*> hordes;

		// Accesseurs
		#pragma region ACCESSORS

		Creature* GetJaina() { return GetCreature(DATA_JAINA_PROUDMOORE); }
		Creature* GetKalecgos() { return GetCreature(DATA_KALECGOS); }
		Creature* GetKinndy() { return GetCreature(DATA_KINNDY_SPARKSHINE); }
		Creature* GetWarlord() { return GetCreature(DATA_ROKNAH_WARLORD); }
		Creature* GetZeppelin() { return GetCreature(DATA_BOMBARDING_ZEPPELIN); }

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

		#pragma endregion
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
