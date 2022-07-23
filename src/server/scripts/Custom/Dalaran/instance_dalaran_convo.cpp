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
		instance_dalaran_convo_InstanceScript(InstanceMap* map) : InstanceScript(map), eventId(1), phase(Phases::None)
		{
			SetHeaders(DataHeader);
			LoadObjectData(creatureData, gameobjectData);
		}

        enum Misc
        {
            // NPCs
            NPC_GHOUL                   = 146835,

            // Spells
            SPELL_VOID_DISSOLVE_OUT     = 277050,
        };

        void SetData(uint32 dataId, uint32 /*value*/) override
        {
            switch ((Phases)dataId)
            {
                case Phases::Introduction:
                    events.ScheduleEvent(1, 2s);
                    break;
                case Phases::Conversation:
                    events.ScheduleEvent(100, 2s);
                    break;
            }
        }

		void OnCreatureCreate(Creature* creature) override
		{
			if (creature->HasUnitTypeMask(UNIT_MASK_SUMMON))
				return;

			InstanceScript::OnCreatureCreate(creature);

			switch (creature->GetEntry())
			{
                case NPC_SHANNON_NOEL:
                    creature->AIM_Destroy();
                    creature->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                    creature->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
                    creature->CastSpell(creature, SPELL_READING_BOOK_SITTING);
                    creature->CastSpell(creature, SPELL_DISSOLVE, true);
                    break;
                case NPC_KELTHUZAD:
                    creature->AIM_Destroy();
                    creature->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                    creature->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
                    creature->CastSpell(creature, SPELL_DISSOLVE, true);
                    break;
				default:
					break;
			}
		}

        void OnGameObjectCreate(GameObject* go) override
        {
            InstanceScript::OnGameObjectCreate(go);

            switch (go->GetEntry())
            {
                case GOB_PORTAL_TO_DALARAN:
                    go->SetLootState(GO_READY);
                    go->UseDoorOrButton();
                    go->SetFlag(GO_FLAG_NOT_SELECTABLE);
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
                    GetAnduin()->SetFacingToObject(GetJaina());
                    GetJaina()->SetFacingToObject(GetAnduin());
                    Next(2s);
                    break;
                case 2:
                    GetAnduin()->AI()->Talk(SAY_ANDUIN_INTRO_01);
                    Next(6s);
                    break;
                case 3:
                    GetJaina()->AI()->Talk(SAY_JAINA_INTRO_02);
                    Next(4s);
                    break;
                case 4:
                    GetAnduin()->SetFacingToObject(GetPlayer());
                    GetJaina()->SetFacingToObject(GetPlayer());
                    Next(1s);
                    break;
                case 5:
                    GetJaina()->AI()->SetData(PHASE_TYPE, (uint32)Phases::Introduction);
                    break;

                /** TEST **/

                case 100:
                    if (Creature* jaina = GetJaina())
                    {
                        jaina->NearTeleportTo(JainaPos01);
                        jaina->AddAura(SPELL_SIT_CHAIR_MED, jaina);
                    }
                    GetAnduin()->NearTeleportTo(AnduinPos01);
                    GetPlayer()->NearTeleportTo(PlayerPos01);
                    Next(1s);
                    break;
                case 101:
                    break;
                default:
					break;
			}
		}

		EventMap events;
		uint32 eventId;
        Phases phase;

		// Accesseurs
		#pragma region ACCESSORS
		
		Creature* GetJaina() { return GetCreature(DATA_JAINA_PROUDMOORE); }
		Creature* GetAnduin() { return GetCreature(DATA_ANDUIN); }

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
