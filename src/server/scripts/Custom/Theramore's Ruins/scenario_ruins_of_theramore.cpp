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
#include "ruins_of_theramore.h"

#define CREATURE_DATA_SIZE 5

const ObjectData creatureData[CREATURE_DATA_SIZE] =
{
	{ NPC_JAINA_PROUDMOORE,     DATA_JAINA_PROUDMOORE       },
	{ NPC_KALECGOS,             DATA_KALECGOS               },
	{ NPC_KINNDY_SPARKSHINE,    DATA_KINNDY_SPARKSHINE      },
	{ NPC_ROKNAH_WARLORD,       DATA_ROKNAH_WARLORD         },
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
			eventId(0), phase(RFTPhases::FindJaina_Isle)
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

		void SetData(uint32 dataId, uint32 value) override
		{
			switch (dataId)
			{
				case DATA_SCENARIO_PHASE:
					phase = (RFTPhases)value;
					break;
				case EVENT_FIND_JAINA_02:
					SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::FindJaina_Crater_Valided);
					events.ScheduleEvent(19, 500ms);
					break;
				case EVENT_BACK_TO_SENDER:
					SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::BackToSender);
					events.ScheduleEvent(25, 500ms);
					break;
			}
		}

		void OnCompletedCriteriaTree(CriteriaTree const* tree) override
		{
			switch (tree->ID)
			{
				case CRITERIA_TREE_FIND_JAINA_01:
					instance->SummonCreature(NPC_KALECGOS, KalecgosPath01[0]);
					if (Creature* kalecgos = GetKalecgos())
					{
						kalecgos->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
						kalecgos->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_NONE, KalecgosPath01, KALECGOS_PATH_01, false, false, KalecgosPath01[KALECGOS_PATH_01 - 1].GetOrientation());
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::FindJaina_Isle_Valided);
					#ifdef DEBUG
						events.ScheduleEvent(17, 1s);
					#else
						events.ScheduleEvent(1, 1s);
					#endif
					break;
				case CRITERIA_TREE_HELP_KALECGOS:
					if (Creature* jaina = GetJaina())
					{
						jaina->LoadEquipment(2);
						jaina->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
						jaina->RemoveAllAuras();
						jaina->SetStandState(UNIT_STAND_STATE_KNEEL);

						// Distance minimale pour déclencher l'event
						jaina->AI()->SetData(0U, 50U);
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::FindJaina_Crater);
					break;
				case CRITERIA_TREE_FIND_JAINA_02:
					SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::Standards);
					break;
				case CRITERIA_TREE_CLEANING:
					instance->SummonCreatureGroup(0);
					if (Creature * jaina = GetJaina())
					{
						jaina->RemoveAurasDueToSpell(SPELL_ALUNETH_DRINKS);
						jaina->AI()->Talk(SAY_IRIS_PROTECTION_JAINA_03);

						// Distance minimale pour déclencher l'event
						jaina->AI()->SetData(0U, 15U);
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::Standards_Valided);
					break;
                case CRITERIA_TREE_BACK_TO_SENDER:
                    SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::TheFinalAssault);
                    break;
                case CRITERIA_TREE_WARLORD_SLAIN:
                    if (Creature* warlord = GetWarlord())
                    {
                        if (warlord->IsAlive())
                        {
                            warlord->SetRegenerateHealth(false);
                            warlord->CombatStop();
                            warlord->SetImmuneToAll(true);
                            warlord->SetControlled(true, UNIT_STATE_ROOT);
                            warlord->SetStandState(UNIT_STAND_STATE_KNEEL);

                            if (Creature* jaina = GetJaina())
                            {
                                jaina->CombatStop();
                                jaina->SetImmuneToAll(true);
                                jaina->GetMotionMaster()->Clear();
                                jaina->SetWalk(true);
                                GameObject* brokenGlass = GetGameObject(DATA_BROKEN_GLASS);
                                if (TempSummon* trigger = instance->SummonCreature(WORLD_TRIGGER, brokenGlass->GetPosition()))
                                    jaina->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_01, trigger, 0.3f);
                            }
                        }
                    }
                    break;
                case CRITERIA_TREE_THE_LAST_STAND:
                    SetData(DATA_SCENARIO_PHASE, (uint32)RFTPhases::LeaveTheRuins);
                    break;
				default:
					break;
			}
		}

		void OnCreatureCreate(Creature* creature) override
		{
			InstanceScript::OnCreatureCreate(creature);

			creature->SetVisibilityDistanceOverride(VisibilityDistanceType::Large);

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
					creature->AddUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
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
                go->SetFlags(GO_FLAG_NOT_SELECTABLE);
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
						jaina->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
						jaina->GetMotionMaster()->MovePoint(0, JainaPoint01, true, JainaPoint01.GetOrientation());
					}
					Next(3s);
					break;
				case 2:
					if (Creature* jaina = GetJaina())
					{
						if (Creature* kalecgos = GetKalecgos())
						{
							kalecgos->AI()->Talk(SAY_AFTER_BATTLE_KALECGOS_01);
							kalecgos->SetWalk(true);
							kalecgos->SetTarget(jaina->GetGUID());

							jaina->SetTarget(kalecgos->GetGUID());
						}
					}
					Next(2s);
					break;
				case 3:
					GetJaina()->AI()->Talk(SAY_AFTER_BATTLE_JAINA_02);
					Next(6s);
					break;
				case 4:
					GetJaina()->AI()->Talk(SAY_AFTER_BATTLE_JAINA_03);
					Next(10s);
					break;
				case 5:
					if (Creature* kalecgos = GetKalecgos())
					{
						kalecgos->AI()->Talk(SAY_AFTER_BATTLE_KALECGOS_04);
						kalecgos->SetSpeedRate(MOVE_WALK, 0.6f);
						kalecgos->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, KalecgosPoint01, true, KalecgosPoint01.GetOrientation());
					}
					Next(8s);
					break;
				case 6:
					GetJaina()->AI()->Talk(SAY_AFTER_BATTLE_JAINA_05);
					Next(5s);
					break;
				case 7:
					GetKalecgos()->AI()->Talk(SAY_AFTER_BATTLE_KALECGOS_06);
					Next(4s);
					break;
				case 8:
					GetJaina()->AI()->Talk(SAY_AFTER_BATTLE_JAINA_07);
					Next(6s);
					break;
				case 9:
					GetKalecgos()->AI()->Talk(SAY_AFTER_BATTLE_KALECGOS_08);
					Next(4s);
					break;
				case 10:
					GetJaina()->AI()->Talk(SAY_AFTER_BATTLE_JAINA_09);
					Next(4s);
					break;
				case 11:
					GetKalecgos()->AI()->Talk(SAY_AFTER_BATTLE_KALECGOS_10);
					Next(6s);
					break;
				case 12:
					GetKalecgos()->AI()->Talk(SAY_AFTER_BATTLE_KALECGOS_11);
					Next(7s);
					break;
				case 13:
					GetJaina()->AI()->Talk(SAY_AFTER_BATTLE_JAINA_12);
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
					GetJaina()->AddUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
					Next(2s);
					break;
				case 17:
					GetJaina()->NearTeleportTo(JainaPoint02);
					if (Creature* kalecgos = GetKalecgos())
					{
						kalecgos->AI()->Talk(SAY_AFTER_BATTLE_KALECGOS_13);
						kalecgos->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
						if (Player* player = instance->GetPlayers().begin()->GetSource())
							kalecgos->SetFacingToObject(player);
					}
					Next(5s);
					break;
				case 18:
					DoTeleportPlayers(instance->GetPlayers(), PlayerPoint01, 12.f);
					DoCastSpellOnPlayers(SPELL_SCREEN_FX);
					DoSendScenarioEvent(EVENT_HELP_KALECGOS);
					break;

				#pragma endregion

				// Find Jaina - Crater
				#pragma region FIND_JAINA_CRATER

				case 19:
					GetJaina()->SetStandState(UNIT_STAND_STATE_STAND);
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
					GetJaina()->AI()->Talk(SAY_IRIS_PROTECTION_JAINA_01);
					Next(4s);
					break;
				case 22:
					if (Creature* jaina = GetJaina())
					{
						jaina->SetWalk(true);
						jaina->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_01, JainaPoint03, true, JainaPoint03.GetOrientation());
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
					GetJaina()->AI()->Talk(SAY_IRIS_PROTECTION_JAINA_02);
					Next(6s);
					break;
				case 24:
					DoSendScenarioEvent(EVENT_FIND_JAINA_02);
					if (Creature* dummy = instance->SummonCreature(WORLD_TRIGGER, DummyPoint01))
					{
						dummy->SetObjectScale(0.6f);
						dummy->CastSpell(GetJaina(), SPELL_ALUNETH_DRINKS);
						dummy->CastSpell(dummy, SPELL_EMPOWERED_SUMMON, true);
					}
					break;

				#pragma endregion

				// Back to sender
				#pragma region BACK_TO_SENDER

				case 25:
					if (Creature* jaina = GetJaina())
					{
						jaina->AI()->Talk(SAY_IRIS_PROTECTION_JAINA_04);
						if (Player* player = instance->GetPlayers().begin()->GetSource())
							jaina->SetFacingToObject(player);
					}
					Next(4s);
					break;
				case 26:
					if (Creature* jaina = GetJaina())
					{
						jaina->AI()->Talk(SAY_IRIS_PROTECTION_JAINA_05);
						jaina->SetFacingTo(JainaPoint03.GetOrientation());
					}
					Next(6s);
					break;
				case 27:
					GetJaina()->CastSpell(GetJaina(), SPELL_SUMMON_WATER_ELEMENTALS);
                    DoSendScenarioEvent(EVENT_BACK_TO_SENDER);
                    Next(2s);
					break;
				case 28:
					GetJaina()->CastSpell(GetJaina(), SPELL_ARCANE_CHANNELING);
					for (uint8 i = 0; i < ELEMENTALS_SIZE; ++i)
					{
						elementals[i]->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, ElementalsPoint[i].destination, true, ElementalsPoint[i].destination.GetOrientation());
						elementals[i]->SetHomePosition(ElementalsPoint[i].destination);
                        elementals[i]->SetBoundingRadius(4.f);
					}
					Next(5s);
					break;
				case 29:
					if (TempSummon* zeppelin = instance->SummonCreature(NPC_BOMBARDING_ZEPPELIN, ZeppelinPoint.spawn))
					{
						zeppelin->SetSpeedRate(MOVE_RUN, 3.f);
						zeppelin->PlayDirectSound(SOUND_ZEPPELIN_FLIGHT);
						zeppelin->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, ZeppelinPoint.destination, false);
					}
					Next(3s);
					break;
				case 30:
					instance->SummonCreatureGroup(1, &hordes);
					for (Creature* horde : hordes)
					{
						horde->SetImmuneToAll(true);
						horde->AddAura(SPELL_COSMETIC_PARACHUTE, horde);
						horde->GetMotionMaster()->MoveFall();
					}
					Next(2s);
					break;
                case 31:
                    for (Creature* horde : hordes)
                        horde->RemoveAurasDueToSpell(SPELL_COSMETIC_PARACHUTE);
                    Next(8s);
                    break;
				case 32:
					GetWarlord()->AI()->Talk(SAY_IRIS_PROTECTION_JAINA_06);
					Next(6s);
					break;
				case 33:
					GetJaina()->AI()->Talk(SAY_IRIS_PROTECTION_JAINA_07);
					Next(10s);
					break;
				case 34:
					GetWarlord()->AI()->Talk(SAY_IRIS_PROTECTION_JAINA_08);
					Next(6s);
					break;
				case 35:
					GetJaina()->AI()->Talk(SAY_IRIS_PROTECTION_JAINA_09);
					Next(11s);
					break;
				case 36:
					GetWarlord()->AI()->Talk(SAY_IRIS_PROTECTION_JAINA_10);
					Next(2s);
					break;
				case 37:
					for (Creature* horde : hordes)
					{
                        horde->SetImmuneToAll(false);
                        if (horde->GetEntry() == NPC_ROKNAH_WARLORD)
                        {
                            if (Player* player = instance->GetPlayers().begin()->GetSource())
                                horde->AI()->AttackStart(player);
                        }
                        else
                        {
                            if (horde->GetPositionY() <= -4468.18f)
                                horde->AI()->AttackStart(elementals[0]);
                            else
                                horde->AI()->AttackStart(elementals[1]);
                        }
					}
                    Next(1s);
					break;
                case 38:
                    if (Creature* jaina = GetJaina())
                    {
                        for (Creature* horde : hordes)
                        {
                            jaina->GetThreatManager().AddThreat(horde, 100000.f);
                            horde->GetThreatManager().AddThreat(jaina, 100000.f);
                        }
                    }
                    Next(2s);
                    break;
                case 39:
                {
                    uint32 membersCounter = 0;
                    uint32 deadCounter = 0;
                    for (Creature* horde : hordes)
                    {
                        if (horde->GetEntry() != NPC_ROKNAH_WARLORD)
                        {
                            ++membersCounter;
                            if (horde->isDead())
                                ++deadCounter;
                        }
                    }

                    // Quand le nombre de membres vivants est inférieur ou égal au nom de membres morts
                    if (membersCounter <= deadCounter)
                    {
                        DoSendScenarioEvent(EVENT_JAINA_PROTECTED);
                        Next(2s);
                    }
                    else
                        events.RescheduleEvent(39, 1s);

                    break;
                }

				#pragma endregion

				default:
					break;
			}
		}

		EventMap events;
		uint32 eventId;
		RFTPhases phase;
		std::vector<Creature*> elementals;
		std::list<TempSummon*> hordes;

		// Accesseurs
		#pragma region ACCESSORS
		
		Creature* GetJaina() { return GetCreature(DATA_JAINA_PROUDMOORE); }
		Creature* GetKalecgos() { return GetCreature(DATA_KALECGOS); }
		Creature* GetKinndy() { return GetCreature(DATA_KINNDY_SPARKSHINE); }
		Creature* GetWarlord() { return GetCreature(DATA_ROKNAH_WARLORD); }

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
			creature->AddUnitFlag2(UNIT_FLAG2_FEIGN_DEATH);
			creature->AddUnitFlag2(UNIT_FLAG2_PLAY_DEATH_ANIM);
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
