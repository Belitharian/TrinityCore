#include "GameObject.h"
#include "PhasingHandler.h"
#include "Map.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "InstanceScript.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "battle_for_theramore.h"

enum class Phases
{
    Normal,
    Timed
};

class scenario_battle_for_theramore : public InstanceMapScript
{
    public:
    scenario_battle_for_theramore() : InstanceMapScript(BFTScriptName, 5000)
    {
    }

    struct scenario_battle_for_theramore_InstanceScript : public InstanceScript
    {
        scenario_battle_for_theramore_InstanceScript(InstanceMap* map) : InstanceScript(map), phase(Phases::Normal)
        {
            SetHeaders(DataHeader);
        }

        void OnGameObjectCreate(GameObject* go) override
        {
            switch (go->GetEntry())
            {
                case GOB_PORTAL_TO_STORMWIND:
                    go->SetLootState(GO_READY);
                    go->UseDoorOrButton();
                    go->SetFlags(GO_FLAG_NOT_SELECTABLE);
                    portalGUID = go->GetGUID();
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
                default:
                    break;
            }
        }

        ObjectGuid GetGuidData(uint32 type) const override
        {
            switch (type)
            {
                case DATA_PORTAL_TO_STORMWIND:
                    return portalGUID;
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
                default:
                    break;
            }

            return ObjectGuid::Empty;
        }

        void SetData(uint32 dataId, uint32 /*value*/) override
        {
            if (dataId == DATA_SCENARIO_WAIT_EVENT_01)
            {
                phase = Phases::Timed;
                scheduler.Schedule(Seconds(10s), [this](TaskContext /*context*/)
                {
                    DoSendScenarioEvent(EVENT_WAITING_SOMETHING_HAPPENING);
                    if (Creature* jaina = instance->GetCreature(jainaGUID))
                        jaina->AI()->DoAction(24);
                    phase = Phases::Normal;
                });
            }
        }

        void Update(uint32 diff) override
        {
            if (phase == Phases::Timed)
            {
                scheduler.Update(diff);
            }
        }

        private:
        TaskScheduler scheduler;
        Phases phase;
        ObjectGuid jainaGUID;
        ObjectGuid kalecgosGUID;
        ObjectGuid tervoshGUID;
        ObjectGuid kinndyGUID;
        ObjectGuid perithGUID;
        ObjectGuid painedGUID;
        ObjectGuid officerGUID;
        ObjectGuid portalGUID;
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
