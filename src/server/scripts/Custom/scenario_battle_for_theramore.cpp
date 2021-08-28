#include "GameObject.h"
#include "PhasingHandler.h"
#include "Map.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "InstanceScript.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "scenario_battle_for_theramore.h"

class scenario_battle_for_theramore : public InstanceMapScript
{
    public:
    scenario_battle_for_theramore() : InstanceMapScript(BfTScriptName, 1000)
    {
    }

    struct scenario_battle_for_theramore_InstanceScript : public InstanceScript
    {
        scenario_battle_for_theramore_InstanceScript(InstanceMap* map) : InstanceScript(map)
        {
   
        }

        void OnCreatureCreate(Creature* creature) override
        {
            TC_LOG_DEBUG("spells", "$s", creature->GetName().c_str());

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
                default:
                    break;
            }
        }

        ObjectGuid GetGuidData(uint32 type) const override
        {
            switch (type)
            {
                default:
                    break;
            }

            return ObjectGuid::Empty;
        }

        private:
        ObjectGuid jainaGUID;
        ObjectGuid kalecgosGUID;
        ObjectGuid tervoshGUID;
        ObjectGuid kinndyGUID;
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new scenario_battle_for_theramore_InstanceScript(map);
    }
};

void AddSC_battle_for_theramore()
{
    new scenario_battle_for_theramore();
}
