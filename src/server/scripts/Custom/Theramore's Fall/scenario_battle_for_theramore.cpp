#include "GameObject.h"
#include "PhasingHandler.h"
#include "CriteriaHandler.h"
#include "Map.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "InstanceScript.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "Weather.h"
#include "MiscPackets.h"
#include "Log.h"
#include "battle_for_theramore.h"

class scenario_battle_for_theramore : public InstanceMapScript
{
    public:
    scenario_battle_for_theramore() : InstanceMapScript(BFTScriptName, 5000)
    {
    }

    struct scenario_battle_for_theramore_InstanceScript : public InstanceScript
    {
        scenario_battle_for_theramore_InstanceScript(InstanceMap* map) : InstanceScript(map), phase(BFTPhases::Normal)
        {
            SetHeaders(DataHeader);
        }

        void OnPlayerEnter(Player* player) override
        {
            WorldPackets::Misc::Weather weather(WEATHER_STATE_THUNDERS, 1.f);
            weather.Write();

            player->SendDirectMessage(weather.GetRawPacket());
        }

        void OnCompletedCriteriaTree(CriteriaTree const* tree) override
        {
            switch (tree->ID)
            {
                case CRITERIA_FIND_JAINA:
                    for (ObjectGuid guid : events)
                    {
                        if (Creature* dummy = instance->GetCreature(guid))
                            dummy->AI()->DoAction(1U);
                    }
                    break;
                case CRITERIA_TREE_A_LITTLE_HELP:
                    for (ObjectGuid guid : events)
                    {
                        if (Creature* dummy = instance->GetCreature(guid))
                            dummy->AI()->DoAction(2U);
                    }
                    break;
                case CRITERIA_TREE_EVACUATION:
                    if (Creature* jaina = instance->GetCreature(jainaGUID))
                    {
                        jaina->NearTeleportTo(JainaPoint02);
                        jaina->SetHomePosition(JainaPoint02);
                        jaina->AI()->SetData(DATA_SCENARIO_PHASE, 2);
                    }
                    if (Creature* tervosh = instance->GetCreature(tervoshGUID))
                    {
                        tervosh->NearTeleportTo(TervoshPoint01);
                        tervosh->SetHomePosition(TervoshPoint01);
                        tervosh->CastSpell(tervosh, SPELL_COSMETIC_FIRE_LIGHT);
                    }
                    if (Creature* kinndy = instance->GetCreature(kinndyGUID))
                    {
                        kinndy->NearTeleportTo(KinndyPoint02);
                        kinndy->SetHomePosition(KinndyPoint02);
                    }
                    if (Creature* kalecgos = instance->GetCreature(kalecgosGUID))
                    {
                        kalecgos->NearTeleportTo(KalecgosPoint01);
                        kalecgos->SetHomePosition(KalecgosPoint01);
                        kalecgos->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                        kalecgos->RemoveAllAuras();
                    }
                    break;
            }
        }

        void OnGameObjectCreate(GameObject* go) override
        {
            switch (go->GetEntry())
            {
                case GOB_PORTAL_TO_STORMWIND:
                    go->SetLootState(GO_READY);
                    go->UseDoorOrButton();
                    go->SetFlags(GO_FLAG_NOT_SELECTABLE);
                    stormwindGUID = go->GetGUID();
                    break;
                case GOB_PORTAL_TO_DALARAN:
                    go->SetLootState(GO_READY);
                    go->UseDoorOrButton();
                    go->SetFlags(GO_FLAG_NOT_SELECTABLE);
                    dalaranGUID = go->GetGUID();
                    break;
                case GOB_MYSTIC_BARRIER:
                    barriers.push_back(go->GetGUID());
                    break;
                default:
                    break;
            }
        }

        void OnCreatureCreate(Creature* creature) override
        {
            switch (creature->GetEntry())
            {
                case NPC_JAINA_PROUDMOORE:
                    jainaGUID = creature->GetGUID();
                    break;
                case NPC_KALECGOS:
                    kalecgosGUID = creature->GetGUID();
                    break;
                case NPC_ARCHMAGE_TERVOSH:
                    tervoshGUID = creature->GetGUID();
                    break;
                case NPC_KINNDY_SPARKSHINE:
                    kinndyGUID = creature->GetGUID();
                    break;
                case NPC_PAINED:
                    painedGUID = creature->GetGUID();
                    break;
                case NPC_PERITH_STORMHOOVE:
                    perithGUID = creature->GetGUID();
                    break;
                case NPC_THERAMORE_OFFICER:
                    officerGUID = creature->GetGUID();
                    break;
                case NPC_RHONIN:
                    rhoninGUID = creature->GetGUID();
                    break;
                case NPC_VEREESA_WINDRUNNER:
                    vereesaGUID = creature->GetGUID();
                    break;
                case NPC_THALEN_SONGWEAVER:
                    thalenGUID = creature->GetGUID();
                    break;
                case NPC_HEDRIC_EVENCANE:
                    hedricGUID = creature->GetGUID();
                    break;
                case NPC_ALLIANCE_PEASANT:
                    peasants.push_back(creature->GetGUID());
                    break;
                case NPC_EVENT_THERAMORE_TRAINING:
                case NPC_EVENT_THERAMORE_FAITHFUL:
                    events.push_back(creature->GetGUID());
                    break;
                case NPC_THERAMORE_CITIZEN_MALE:
                case NPC_THERAMORE_CITIZEN_FEMALE:
                    if (phase == BFTPhases::Evacuation)
                        break;
                    creature->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                    citizens.push_back(creature->GetGUID());
                    break;
                default:
                    break;
            }
        }

        ObjectGuid GetGuidData(uint32 type) const override
        {
            switch (type)
            {
                case DATA_PORTAL_TO_STORMWIND:
                    return stormwindGUID;
                case DATA_PORTAL_TO_DALARAN:
                    return dalaranGUID;
                case DATA_JAINA_PROUDMOORE:
                    return jainaGUID;
                case DATA_KALECGOS:
                    return kalecgosGUID;
                case DATA_ARCHMAGE_TERVOSH:
                    return tervoshGUID;
                case DATA_KINNDY_SPARKSHINE:
                    return kinndyGUID;
                case DATA_PAINED:
                    return painedGUID;
                case DATA_PERITH_STORMHOOVE:
                    return perithGUID;
                case DATA_THERAMORE_OFFICER:
                    return officerGUID;
                case DATA_RHONIN:
                    return rhoninGUID;
                case DATA_VEREESA_WINDRUNNER:
                    return vereesaGUID;
                case DATA_THALEN_SONGWEAVER:
                    return thalenGUID;
                case DATA_HEDRIC_EVENCANE:
                    return hedricGUID;
                case DATA_MYSTIC_BARRIER_01:
                    return barriers[0];
                case DATA_MYSTIC_BARRIER_02:
                    return barriers[1];
                default:
                    break;
            }

            return ObjectGuid::Empty;
        }

        void SetData(uint32 dataId, uint32 value) override
        {
            if (dataId == DATA_SCENARIO_PHASE)
            {
                phase = (BFTPhases)value;
                switch (phase)
                {
                    case BFTPhases::Timed:
                        scheduler.Schedule(10s, [this](TaskContext /*context*/)
                        {
                            DoSendScenarioEvent(EVENT_WAITING_SOMETHING_HAPPENING);
                            if (Creature* jaina = instance->GetCreature(jainaGUID))
                                jaina->AI()->DoAction(24);
                            phase = BFTPhases::Normal;
                        });
                        break;
                    case BFTPhases::Evacuation:
                        for (uint8 i = 0; i < citizens.size(); i++)
                        {
                            if (Creature* citizen = instance->GetCreature(citizens[i]))
                            {
                                if (Creature* faithful = citizen->FindNearestCreature(NPC_THERAMORE_FAITHFUL, 15.f))
                                    continue;

                                citizen->SetNpcFlags(UNIT_NPC_FLAG_GOSSIP);
                            }
                        }
                        break;
                    case BFTPhases::Battle:
                        for (uint8 i = 0; i < citizens.size(); i++)
                        {
                            if (Creature* citizen = instance->GetCreature(citizens[i]))
                                citizen->SetVisible(false);
                        }
                        for (uint8 i = 0; i < peasants.size(); i++)
                        {
                            if (Creature* peasant = instance->GetCreature(peasants[i]))
                            {
                                if (roll_chance_i(30))
                                    peasant->SetVisible(false);
                                else
                                {
                                    peasant->AddUnitFlag(UNIT_FLAG_IMMUNE_TO_NPC);
                                    peasant->AddUnitFlag(UNIT_FLAG_IMMUNE_TO_PC);
                                    peasant->AddUnitFlag2(UNIT_FLAG2_FEIGN_DEATH);
                                    peasant->AddUnitFlag2(UNIT_FLAG2_PLAY_DEATH_ANIM);
                                }
                            }
                        }
                        scheduler.Schedule(10s, [this](TaskContext camera_shake)
                        {
                            DoCastSpellOnPlayers(SPELL_CAMERA_SHAKE_VOLCANO);
                            camera_shake.Repeat(8s, 32s);
                        });
                        break;
                    default:
                        break;
                }
            }
        }

        uint32 GetData(uint32 dataId) const override
        {
            if (dataId == DATA_SCENARIO_PHASE)
            {
                return (uint32)phase;
            }

            return (uint32)0;
        }

        void Update(uint32 diff) override
        {
            if (phase == BFTPhases::Timed || phase == BFTPhases::Battle)
            {
                scheduler.Update(diff);
            }
        }

        private:
        TaskScheduler scheduler;
        BFTPhases phase;
        ObjectGuid jainaGUID;
        ObjectGuid kalecgosGUID;
        ObjectGuid tervoshGUID;
        ObjectGuid kinndyGUID;
        ObjectGuid perithGUID;
        ObjectGuid painedGUID;
        ObjectGuid officerGUID;
        ObjectGuid rhoninGUID;
        ObjectGuid vereesaGUID;
        ObjectGuid thalenGUID;
        ObjectGuid hedricGUID;
        ObjectGuid stormwindGUID;
        ObjectGuid dalaranGUID;
        std::vector<ObjectGuid> barriers;
        std::vector<ObjectGuid> citizens;
        std::vector<ObjectGuid> peasants;
        std::vector<ObjectGuid> events;
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new scenario_battle_for_theramore_InstanceScript(map);
    }
};

void AddSC_scenario_battle_for_theramore()
{
    new scenario_battle_for_theramore();
}
