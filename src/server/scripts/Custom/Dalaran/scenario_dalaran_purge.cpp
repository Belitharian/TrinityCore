#include "CriteriaHandler.h"
#include "CreatureGroups.h"
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
#include "dalaran_purge.h"

const ObjectData creatureData[] =
{
    { NPC_JAINA_PROUDMOORE,             DATA_JAINA_PROUDMOORE           },
    { NPC_JAINA_PROUDMOORE_PATROL,      DATA_JAINA_PROUDMOORE_PATROL    },
    { NPC_AETHAS_SUNREAVER,             DATA_AETHAS_SUNREAVER           },
    { NPC_SUMMONED_WATER_ELEMENTAL,     DATA_SUMMONED_WATER_ELEMENTAL   },
    { NPC_BOUND_WATER_ELEMENTAL,        DATA_BOUND_WATER_ELEMENTAL      },
	{ 0,                                0                               }   // END
};

const ObjectData gameobjectData[] =
{
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
            eventId(1), phase(DLPPhases::FindJaina)
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
                    phase = (DLPPhases)value;
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
                    if (Creature* aethas = instance->SummonCreature(NPC_AETHAS_SUNREAVER, AethasPoint01.spawn))
                        aethas->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, AethasPoint01.destination, true, AethasPoint01.destination.GetOrientation());
                    SetData(DATA_SCENARIO_PHASE, (uint32)DLPPhases::FindingTheThieves);
                    events.ScheduleEvent(1, 10s);
                    break;
                    // Finding the thieves
                case CRITERIA_TREE_FINDING_THE_THIEVES:
                    for (Creature* creature : patrol)
                    {
                        creature->SetFaction(FACTION_DALARAN_PATROL);
                        creature->setActive(true);
                        creature->SetVisible(true);
                    }
                    break;
                default:
                    break;
            }
        }

        void OnCreatureCreate(Creature* creature) override
        {
            InstanceScript::OnCreatureCreate(creature);

            creature->SetVisibilityDistanceOverride(VisibilityDistanceType::Large);
            creature->AddPvpFlag(UNIT_BYTE2_FLAG_PVP);
            creature->AddUnitFlag(UNIT_FLAG_PVP);

            switch (creature->GetEntry())
            {
                case NPC_ARCANIST_RATHAELLA:
                    creature->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
                    creature->CastSpell(creature, SPELL_ATTACHED);
                    break;
                case NPC_SORIN_MAGEHAND:
                    creature->SetReactState(REACT_PASSIVE);
                    break;
                case NPC_ARCHMAGE_LAN_DALOCK:
                    creature->SetImmuneToAll(true);
                    creature->CastSpell(creature, SPELL_FROST_CANALISATION);
                    break;
                case NPC_ICE_WALL:
                    creature->SetImmuneToAll(true);
                    creature->SetUnitFlags(UNIT_FLAG_NOT_SELECTABLE);
                    break;
                case NPC_JAINA_PROUDMOORE_PATROL:
                case NPC_BOUND_WATER_ELEMENTAL:
                    creature->SetFaction(FACTION_FRIENDLY);
                    creature->setActive(false);
                    creature->SetVisible(false);
                    patrol.push_back(creature);
                    break;
                case NPC_VEREESA_WINDRUNNER:
                    creature->SetNpcFlags(UNIT_NPC_FLAG_NONE);
                    break;
                case NPC_AETHAS_SUNREAVER:
                    creature->SetWalk(true);
                    creature->SetImmuneToAll(true);
                    creature->AddAura(SPELL_CASTER_READY_03, creature);
                    break;
                case NPC_HIGH_SUNREAVER_MAGE:
                    creature->AddAura(RAND(SPELL_CASTER_READY_01, SPELL_CASTER_READY_02), creature);
                    creature->SetImmuneToAll(true);
                    highmages.push_back(creature);
                    break;
                default:
                    break;
            }
        }

        void OnGameObjectCreate(GameObject* go) override
        {
            InstanceScript::OnGameObjectCreate(go);

            go->SetVisibilityDistanceOverride(VisibilityDistanceType::Large);

            switch (go->GetEntry())
            {
                case GOB_MYSTIC_BARRIER_01:
                    go->SetFlags(GO_FLAG_NOT_SELECTABLE);
                    break;
            }
        }

		void Update(uint32 diff) override
		{
			events.Update(diff);
			switch (eventId = events.ExecuteEvent())
			{
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
                    {
                        int32 val = aethas->CountPctFromMaxHealth(60);
                        CastSpellExtraArgs args(SPELLVALUE_BASE_POINT0, val);
                        args.SetTriggerFlags(TRIGGERED_DISALLOW_PROC_EVENTS);
                        GetElemental()->CastSpell(GetAethas(), SPELL_FROSTBOLT, args);
                    }
                    Next(2s);
                    break;
                case 8:
                    for (Creature* highmage : highmages)
                    {
                        highmage->SetSpeedRate(MOVE_RUN, 0.8f);
                        highmage->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_NONE, GetJaina(), 1.6f);
                    }
                    Next(760ms);
                    break;
                case 9:
                    GetJaina()->CastSpell(GetJaina(), SPELL_ARCANE_BOMBARDMENT);
                    Next(630ms);
                    break;
                case 10:
                    for (Creature* highmage : highmages)
                        highmage->KillSelf();
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
                    GetAethas()->SetVisible(false);
                    GetElemental()->SetVisible(false);
                    for (Creature* highmage : highmages)
                        highmage->SetVisible(false);
                    DoSendScenarioEvent(EVENT_ASSIST_JAINA);
                    break;
				default:
					break;
			}
		}

		EventMap events;
		uint32 eventId;
		DLPPhases phase;
        std::vector<Creature*> highmages;
        std::vector<Creature*> patrol;

		// Accesseurs
		#pragma region ACCESSORS
		
		Creature* GetJaina() { return GetCreature(DATA_JAINA_PROUDMOORE); }
		Creature* GetAethas() { return GetCreature(DATA_AETHAS_SUNREAVER); }
		Creature* GetElemental() { return GetCreature(DATA_SUMMONED_WATER_ELEMENTAL); }

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
