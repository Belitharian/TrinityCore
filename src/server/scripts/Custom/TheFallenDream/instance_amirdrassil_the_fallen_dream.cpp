#include "Creature.h"
#include "InstanceScript.h"
#include "ScriptMgr.h"
#include "instance_amirdrassil_the_fallen_dream.h"
				
ObjectData const creatureData[] =
{
	{ BOSS_ROOT_GUARDIAN, 					DATA_ROOT_GUARDIAN   	},
	{ BOSS_ASHEN_PRIESTESS, 				DATA_ASHEN_PRIESTESS	},
	{ BOSS_FALLEN_FLAME_LORD, 				DATA_FALLEN_FLAME_LORD	},
	{ BOSS_QUEEN_FRANCESCA, 				DATA_QUEEN_FRANCESCA	},
	{ BOSS_VOID_ENTITY, 					DATA_VOID_ENTITY  		},
	{ 0,									0                       }	// END
};

DungeonEncounterData const encounters[] =
{
    { DATA_ROOT_GUARDIAN,      				{ { 10000 } } 			},
    { DATA_ASHEN_PRIESTESS,    				{ { 10001 } } 			},
    { DATA_FALLEN_FLAME_LORD,   			{ { 10002 } } 			},
    { DATA_QUEEN_FRANCESCA,       			{ { 10003 } } 			},
    { DATA_VOID_ENTITY,            			{ { 10004 } } 			}	// END
};

ObjectData const objectData[] =
{
    { 0,                                    0                       }  // END
};

class instance_amirdrassil_the_fallen_dream : public InstanceMapScript
{
public:
    instance_amirdrassil_the_fallen_dream() : InstanceMapScript(ATFDScriptName, 5004) { }

    struct instance_amirdrassil_the_fallen_dream_InstanceMapScript: public InstanceScript
    {
        instance_amirdrassil_the_fallen_dream_InstanceMapScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders(DataHeader);
            SetBossNumber(EncounterCount);
            LoadObjectData(creatureData, objectData);
            LoadDungeonEncounterData(encounters);
        }

        //uint32 GetData(uint32 dataId) const override
        //{
        //    switch (dataId)
        //    {
        //        default:
        //            break;
        //    }
        //    return 0;
        //}

        //void SetData(uint32 dataId, uint32 /*value*/) override
        //{
        //    switch (dataId)
        //    {
        //        default:
        //            break;
        //    }
        //}

        void OnCreatureCreate(Creature* creature) override
        {
            InstanceScript::OnCreatureCreate(creature);
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_amirdrassil_the_fallen_dream_InstanceMapScript(map);
    }
};

void AddSC_instance_amirdrassil_the_fallen_dream()
{
    new instance_amirdrassil_the_fallen_dream();
}
