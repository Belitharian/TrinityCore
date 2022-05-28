#include "EventMap.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"
#include "dalaran_convo.h"

const ObjectData creatureData[] =
{
	{ NPC_JAINA_PROUDMOORE,             DATA_JAINA_PROUDMOORE           },
	{ NPC_ANDUIN,                       DATA_ANDUIN                     },
	{ NPC_KAELTHAS,                     DATA_KAELTHAS                   },
	{ NPC_KELTHUZAD,                    DATA_KELTHUZAD                  },
	{ NPC_KALECGOS,                     DATA_KALECGOS                   },
	{ NPC_INVISIBLE_STALKER,            DATA_CAMERA                     },
	{ 0,                                0                               }   // END
};

const ObjectData gameobjectData[] =
{
	{ 0,                                0                               }   // END
};

class instance_dalaran_convo : public InstanceMapScript
{
	public:
	instance_dalaran_convo() : InstanceMapScript(DLCScriptName, 5003)
	{
	}

	struct instance_dalaran_convo_InstanceScript : public InstanceScript
	{
		instance_dalaran_convo_InstanceScript(InstanceMap* map) : InstanceScript(map), eventId(1)
		{
			SetHeaders(DataHeader);
			LoadObjectData(creatureData, gameobjectData);
		}

        void SetData(uint32 dataId, uint32 value) override
        {
            events.ScheduleEvent(1, 2s);
        }

		void OnCreatureCreate(Creature* creature) override
		{
			if (creature->HasUnitTypeMask(UNIT_MASK_SUMMON))
				return;

			InstanceScript::OnCreatureCreate(creature);

			creature->SetVisibilityDistanceOverride(VisibilityDistanceType::Large);
			
			switch (creature->GetEntry())
			{
                case NPC_CAPTURED_CIVILIAN:
                    creature->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
                    creature->SetAIAnimKitId(ANIMKIT_BEGGING);
                    break;
                case NPC_KELTHUZAD:
                    creature->AIM_Destroy();
                    creature->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
                    break;
                case NPC_INVISIBLE_STALKER:
                    creature->SetSpeedRate(MOVE_RUN, 0.7f);
                    creature->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
                    creature->NearTeleportTo(CamPath01[0].GetPosition());
                    break;
				default:
					break;
			}
		}

		void Update(uint32 diff) override
		{
			events.Update(diff);
			switch (eventId = events.ExecuteEvent())
			{
                case 1:
                    GetPlayer()->CastSpell(GetCamera(), SPELL_FIRST_PERSON_CAMERA);
                    Next(2s);
                    break;
                case 2:
                    GetCamera()->CastSpell(GetCreature(DATA_KELTHUZAD), 275736);
                    GetCamera()->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_NONE, CamPath01, CAM_PATH_1);
                    break;
				default:
					break;
			}
		}

		EventMap events;
		uint32 eventId;

		// Accesseurs
		#pragma region ACCESSORS
		
		Creature* GetJaina() { return GetCreature(DATA_JAINA_PROUDMOORE); }
		Creature* GetAnduin() { return GetCreature(DATA_ANDUIN); }
		Creature* GetCamera() { return GetCreature(DATA_CAMERA); }

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

        Player* GetPlayer()
        {
            if (Player* player = instance->GetPlayers().begin()->GetSource())
                return player;
            return nullptr;
        }

		#pragma endregion
	};

	InstanceScript* GetInstanceScript(InstanceMap* map) const override
	{
		return new instance_dalaran_convo_InstanceScript(map);
	}
};

void AddSC_instance_dalaran_convo()
{
	new instance_dalaran_convo();
}
