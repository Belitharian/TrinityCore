#include "EventMap.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "TemporarySummon.h"
#include "dalaran_convo.h"

const ObjectData creatureData[] =
{
	{ NPC_JAINA_PROUDMOORE,             DATA_JAINA_PROUDMOORE           },
	{ NPC_ANDUIN,                       DATA_ANDUIN                     },
	{ NPC_KAELTHAS,                     DATA_KAELTHAS                   },
	{ NPC_KELTHUZAD,                    DATA_KELTHUZAD                  },
	{ NPC_KALECGOS,                     DATA_KALECGOS                   },
	{ NPC_SHANNON_NOEL,                 DATA_SHANNON_NOEL               },
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
		instance_dalaran_convo_InstanceScript(InstanceMap* map) : InstanceScript(map),
            eventId(1), eventIndex(EVENT_VISION_01), phase(Phases::None)
		{
			SetHeaders(DataHeader);
			LoadObjectData(creatureData, gameobjectData);
		}

        void SetData(uint32 dataId, uint32 value) override
        {
            if (dataId == PHASE_TYPE)
            {
                phase = (Phases)value;

                switch (phase)
                {
                    case Phases::Introduction:
                        events.ScheduleEvent(1, 5ms);
                        break;
                    case Phases::Conversation:
                        events.ScheduleEvent(6, 1s);
                        break;
                    case Phases::Event:
                        events.ScheduleEvent(eventIndex, 2s);
                        break;
                }
            }
        }

        uint32 GetData(uint32 dataId) const override
        {
            if (dataId == PHASE_TYPE)
                return (uint32)phase;
            return 0;
        }

		void OnCreatureCreate(Creature* creature) override
		{
			if (creature->HasUnitTypeMask(UNIT_MASK_SUMMON))
				return;

			InstanceScript::OnCreatureCreate(creature);

			switch (creature->GetEntry())
			{
                case NPC_KAELTHAS:
                    creature->AIM_Destroy();
                    creature->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                    creature->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
                    creature->CastSpell(creature, SPELL_DISSOLVE, true);
                    creature->SetObjectScale(0.38f);
                    break;
                case NPC_KELTHUZAD:
                case NPC_SHANNON_NOEL:
                    creature->AIM_Destroy();
                    creature->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                    creature->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
                    creature->CastSpell(creature, SPELL_DISSOLVE, true);
                    break;
                case NPC_JAINA_PROUDMOORE:
                    creature->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
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
                // Part I
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
                    if (Creature* jaina = GetJaina())
                    {
                        jaina->AI()->SetData(PHASE_TYPE, (uint32)Phases::Introduction);
                        jaina->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                    }
                    break;

                // Part II
                case 6:
                    //if (Creature* jaina = GetJaina())
                    //{
                    //    Talk(jaina, SAY_JAINA_CONVO_01);
                    //    jaina->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_01, ActorsPath01, ACTORS_PATH_01, true);
                    //}
                    Next(1s);
                    break;
                case 7:
                    //GetAnduin()->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_01, ActorsPath01, ACTORS_PATH_01, true);;
                    break;

                // Visions
                #pragma region VISIONS

                case EVENT_VISION_01:
                    //if (Creature* jaina = GetJaina())
                    //{
                    //    Talk(jaina, SAY_JAINA_CONVO_02);
                    //    jaina->RemoveAurasDueToSpell(SPELL_SIT_CHAIR_MED);
                    //    jaina->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_02, JainaPath02, ACTORS_PATH_02, true);
                    //}
                    Next(5s);
                    break;
                case EVENT_VISION_02:
                    //GetAnduin()->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_02, AnduinPath02, ACTORS_PATH_02, true);
                    Talk(GetJaina(), SAY_JAINA_CONVO_03);
                    Next(9s);
                    break;
                case EVENT_VISION_03:
                    Talk(GetJaina(), SAY_JAINA_CONVO_04);
                    eventIndex = EVENT_VISION_04;
                    break;
                case EVENT_VISION_04:
                    Talk(GetJaina(), SAY_JAINA_CONVO_05);
                    Next(4s);
                    break;
                case EVENT_VISION_05:
                    if (Creature* shannon = GetShannon())
                    {
                        shannon->RemoveAurasDueToSpell(SPELL_DISSOLVE);
                        shannon->CastSpell(shannon, SPELL_READING_BOOK_SITTING);
                    }
                    if (Creature* kelThuzad = GetKelThuzad())
                    {
                        kelThuzad->RemoveAurasDueToSpell(SPELL_DISSOLVE);
                        kelThuzad->CastSpell(kelThuzad, SPELL_VOID_CHANNELING);
                    }
                    Next(8s);
                    break;
                case EVENT_VISION_06:
                    if (Creature* shannon = GetShannon())
                    {
                        shannon->CastSpell(shannon, SPELL_FEIGN_DEATH);

                        if (Creature* ghoul = instance->SummonCreature(NPC_GHOUL, shannon->GetPosition()))
                        {
                            tempGUID = ghoul->GetGUID();

                            ghoul->SetWalk(true);
                            ghoul->DespawnOrUnsummon(15s);
                            ghoul->SetFaction(FACTION_FRIENDLY);
                            ghoul->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
                        }
                    }
                    Next(4s);
                    break;
                case EVENT_VISION_07:
                    if (Creature* ghoul = instance->GetCreature(tempGUID))
                        ghoul->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, GhoulPos01);
                    if (Creature* kelThuzad = GetKelThuzad())
                    {
                        kelThuzad->RemoveAurasDueToSpell(SPELL_VOID_CHANNELING);
                        kelThuzad->HandleEmoteCommand(EMOTE_ONESHOT_LAUGH);
                    }
                    Next(3s);
                    break;
                case EVENT_VISION_08:
                    if (Creature* kelThuzad = GetKelThuzad())
                    {
                        kelThuzad->HandleEmoteCommand(EMOTE_ONESHOT_NONE);
                        kelThuzad->CastSpell(kelThuzad, SPELL_TAKING_NOTES);
                    }
                    Next(5s);
                    break;
                case EVENT_VISION_09:
                    if (Creature* kelThuzad = GetKelThuzad())
                        kelThuzad->CastSpell(kelThuzad, SPELL_DISSOLVE);
                    if (Creature* shannon = GetShannon())
                        shannon->AddAura(SPELL_DISSOLVE, shannon);
                    if (Creature* ghoul = instance->GetCreature(tempGUID))
                        ghoul->CastSpell(ghoul, SPELL_DISSOLVE);
                    Next(5s);
                    break;
                case EVENT_VISION_10:
                    GetAnduin()->SetFacingTo(6.022f);
                    if (Creature* jaina = GetJaina())
                    {
                        Talk(jaina, SAY_JAINA_CONVO_06);
                        jaina->SetWalk(true);
                        jaina->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, JainaPos02, true, JainaPos02.GetOrientation());
                    }
                    Next(2s);
                    break;
                case EVENT_VISION_11:
                    if (Creature* kael = GetKaelThas())
                        kael->RemoveAurasDueToSpell(SPELL_DISSOLVE);
                    Next(5s);
                    break;
                case EVENT_VISION_12:
                    if (Creature* kael = GetKaelThas())
                    {
                        kael->SetFacingTo(2.52f);
                        kael->CastSpell(FirestrikePos01, SPELL_FIRESTRIKE, true);
                    }
                    break;

                #pragma endregion

                default:
					break;
			}
		}

		EventMap events;
		uint32 eventId;
		uint32 eventIndex;
        Phases phase;
        ObjectGuid tempGUID;

		// Accesseurs
		#pragma region ACCESSORS
		
		Creature* GetJaina() { return GetCreature(DATA_JAINA_PROUDMOORE); }
		Creature* GetAnduin() { return GetCreature(DATA_ANDUIN); }
		Creature* GetKelThuzad() { return GetCreature(DATA_KELTHUZAD); }
		Creature* GetShannon() { return GetCreature(DATA_SHANNON_NOEL); }
		Creature* GetKaelThas() { return GetCreature(DATA_KAELTHAS); }

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
