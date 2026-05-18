#include "EventMap.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "Player.h"
#include "dalaran_convo.h"

const ObjectData creatureData[] =
{
	{ NPC_JAINA_PROUDMOORE,             DATA_JAINA_PROUDMOORE           },
	{ NPC_ANDUIN_WRYNN,                 DATA_ANDUIN                     },
	{ NPC_KAELTHAS,                     DATA_KAELTHAS                   },
	{ NPC_KELTHUZAD,                    DATA_KELTHUZAD                  },
	{ NPC_KALECGOS,                     DATA_KALECGOS                   },
	{ NPC_JAINA_PROUDMOORE_VISION,      DATA_JAINA_PROUDMOORE_VISION    },
	{ NPC_ARCANIST_ALEC,                DATA_ARCANIST_ALEC              },
	{ NPC_CAULDRON_BUNNY,               DATA_CAULDRON_BUNNY             },
	{ 0,                                0                               }   // END
};

const ObjectData gameobjectData[] =
{
	{ GOB_FEL_BARRIER,                  DATA_FEL_BARRIER                },
	{ GOB_ALCHEMICAL_SOLUTION,          DATA_ALCHEMICAL_SOLUTION        },
	{ GOB_SKULL,                        DATA_SKULL                      },
	{ GOB_ESSENCE_OF_DEATH,             DATA_ESSENCE_OF_DEATH           },
	{ 0,                                0                               }   // END
};

std::unordered_map<std::string_view, Portals::Entry> portals =
{
	{ Portals::PortraitsRoom,  { DATA_PORTAL_PORTRAITS_ROOM,  { } } },
	{ Portals::VioletParlor,   { DATA_PORTAL_VIOLET_PARLOR,   { } } },
	{ Portals::VioletCitadel,  { DATA_PORTAL_VIOLET_CITADEL,  { } } }
};

class scenario_dalaran_convo : public InstanceMapScript
{
	public:
	scenario_dalaran_convo() : InstanceMapScript(DLCScriptName, 5003)
	{
	}

	struct scenario_dalaran_convo_InstanceScript : public InstanceScript
	{
		scenario_dalaran_convo_InstanceScript(InstanceMap* map) : InstanceScript(map),
			eventId(EVENT_NONE), phase(Phases::None)
		{
			SetHeaders(DataHeader);
			LoadObjectData(creatureData, gameobjectData);
		}

        void OnPlayerEnter(Player* player) override
        {
            player->CastSpell(player, SPELL_SKYBOX, true);
        }

        void OnPlayerLeave(Player* player) override
        {
            player->RemoveAurasDueToSpell(SPELL_SKYBOX);
        }

		void SetData(uint32 dataId, uint32 value) override
		{
			if (dataId == DATA_PHASE)
			{
				phase = (Phases)value;

				switch (phase)
				{
					case Phases::Introduction:
						StartConversation(CONVERSATION_INTRODUCTION);
						break;
					case Phases::Kalecgos:
						StartConversation(CONVERSATION_KALECGOS);
						break;
					case Phases::KelThuzad:
						StartConversation(CONVERSATION_KELTHUZAD);
						break;
					case Phases::KelThuzad_Combat:
						StartKelThuzadCombat();
						break;
                    case Phases::KelThuzad_CanTeleport:
                    {
                        if (GameObject* barrier = GetGameObject(DATA_FEL_BARRIER))
                            barrier->UseDoorOrButton();

                        DespawnVision(VISION_KEL_THUZAD);
                        break;
                    }
					default:
						break;
				}
			}
		}

		uint32 GetData(uint32 dataId) const override
		{
			if (dataId == DATA_PHASE)
				return (uint32)phase;
			return 0;
		}

		ObjectGuid GetGuidData(uint32 dataId) const override
		{
			for (auto const& [name, entry] : portals)
				if (entry.dataId == dataId)
					return entry.guid;

            return InstanceScript::GetGuidData(dataId);
		}

		void OnCompletedCriteriaTree(CriteriaTree const* tree) override
		{
			switch (tree->ID)
			{
				// Find Lady Jaina Proudmoore
				case CRITERIA_TREE_INTRODUCTION_FIND_JAINA:
					SetData(DATA_PHASE, (uint32)Phases::Introduction);
					break;
				// Discuss the fate of the Kirin Tor
				case CRITERIA_TREE_INTRODUCTION_DISCUSS:
					SetData(DATA_PHASE, (uint32)Phases::Start);
					events.ScheduleEvent(EVENT_START, 1s);
					break;
				// Assist Lady Jaina Proudmoore
				case CRITERIA_TREE_KALECGOS_ASSIST_JAINA:
					SetData(DATA_PHASE, (uint32)Phases::Kalecgos_CanTeleport);
					DespawnVision(VISION_KALECGOS);
					break;
				// Speak to Arcanist Alec
				case CRITERIA_TREE_KALECGOS_SPEAK_TO_ALEC:
					// Phase transition handled by areatrigger_dalaran_kelthuzad
					break;
				// Witness Kel'Thuzad vision
                case CRITERIA_TREE_KELTHUZAD_WITNESS:
                    RemoveGameObjectFlags(DATA_ALCHEMICAL_SOLUTION, GO_FLAG_NOT_SELECTABLE);
                    RemoveGameObjectFlags(DATA_SKULL, GO_FLAG_NOT_SELECTABLE);
                    RemoveGameObjectFlags(DATA_ESSENCE_OF_DEATH, GO_FLAG_NOT_SELECTABLE);
                    break;
				// Defeat Kel'Thuzad vision
				case CRITERIA_TREE_KELTHUZAD_DEFEAT:
					// Phase transition handled by npc_kelthuzad_vision::JustDied
					break;
				// Toss ingredients in cauldron
				case CRITERIA_TREE_KELTHUZAD_CAULDRON:
				{
					if (Creature* bunny = GetCreature(DATA_CAULDRON_BUNNY))
					{
						bunny->CastSpell(bunny, SPELL_SLIME_BURST);
						bunny->DespawnOrUnsummon(2s);
					}

                    SetGameObjectFlags(DATA_ALCHEMICAL_SOLUTION, GO_FLAG_NOT_SELECTABLE);
                    SetGameObjectFlags(DATA_SKULL, GO_FLAG_NOT_SELECTABLE);
                    SetGameObjectFlags(DATA_ESSENCE_OF_DEATH, GO_FLAG_NOT_SELECTABLE);

                    events.ScheduleEvent(EVENT_KELTHUZAD_01, 1s);
					break;
				}
			}
		}

		void OnCreatureCreate(Creature* creature) override
		{
			if (creature->HasUnitTypeMask(UNIT_MASK_SUMMON))
				return;

			InstanceScript::OnCreatureCreate(creature);

			switch (creature->GetEntry())
			{
				// Generic
				case NPC_JAINA_PROUDMOORE:
				case NPC_ARCANIST_ALEC:
					creature->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
					break;

				// Visions
				case NPC_KALECGOS:
				case NPC_JAINA_PROUDMOORE_VISION:
					visions[VISION_KALECGOS].push_back(creature->GetGUID());
					ApplyHauntingMemoryAura(creature);
					break;

				case NPC_KELTHUZAD:
				case NPC_MR_BIGGLESWORTH:
					visions[VISION_KEL_THUZAD].push_back(creature->GetGUID());
					ApplyHauntingMemoryAura(creature);
					break;

				case NPC_KAELTHAS:
				case NPC_BLOOD_ELF_NOBLE:
					visions[VISION_KAEL_THAS].push_back(creature->GetGUID());
					ApplyHauntingMemoryAura(creature);
					break;

				case NPC_AETHAS_SUNREAVER:
				case NPC_SUNREAVER_CITIZEN:
				case NPC_SUNREAVER_LIEUTENANT:
				case NPC_SUNREAVER_BATTLEMAGE:
				case NPC_SUNREAVER_MAGE:
					visions[VISION_SUNREAVERS].push_back(creature->GetGUID());
					ApplyHauntingMemoryAura(creature);
					break;

				case NPC_ARCHMAGE_ANTONIDAS:
				case NPC_DALARAN_CITIZEN_01:
				case NPC_DALARAN_CITIZEN_02:
				case NPC_DALARAN_CITIZEN_03:
					visions[VISION_ANTONIDAS].push_back(creature->GetGUID());
					ApplyHauntingMemoryAura(creature);
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
				{
                    go->SetFlag(GO_FLAG_NOT_SELECTABLE);

					std::string_view spawnId = go->GetStringId(StringIdType::Spawn);
					auto it = portals.find(spawnId);
					if (it != portals.end())
						it->second.guid = go->GetGUID();
					break;
				}
				case GOB_TOME_OF_POWER:
					visions[VISION_KALECGOS].push_back(go->GetGUID());
					break;
                case GOB_SKULL:
                case GOB_ESSENCE_OF_DEATH:
                case GOB_ALCHEMICAL_SOLUTION:
					go->SetFlag(GO_FLAG_NOT_SELECTABLE);
					break;
                case GOB_LAMP_POST:
                case GOB_FEL_BARRIER:
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
				case EVENT_START:
					StartConversation(CONVERSATION_START);
					break;

                case EVENT_KELTHUZAD_01:
                    GetJaina()->GetMotionMaster()->MovePath(JainaPath02, false);
                    events.ScheduleEvent(EVENT_KELTHUZAD_02, 2s);
                    break;
                case EVENT_KELTHUZAD_02:
                    GetAnduin()->GetMotionMaster()->MovePath(AnduinPath02, false);
                    break;

				default:
					break;
			}
		}

		EventMap events;
		uint32 eventId;
		Phases phase;
		std::unordered_map<uint32, std::vector<ObjectGuid>> visions = {};
		std::array<ObjectGuid, MAX_PORTAL_DATA> m_portalGuids = {};

		// Accesseurs
		#pragma region ACCESSORS
		
		Creature* GetJaina()
		{
			Creature* creature = GetCreature(DATA_JAINA_PROUDMOORE);
			ASSERT(creature);
			return creature;
		}

		Creature* GetJainaVision()
		{
			Creature* creature = GetCreature(DATA_JAINA_PROUDMOORE_VISION);
			ASSERT(creature);
			return creature;
		}

		Creature* GetKalecgos()
		{
			Creature* creature = GetCreature(DATA_KALECGOS);
			ASSERT(creature);
			return creature;
		}

		Creature* GetAnduin()
		{
			Creature* creature = GetCreature(DATA_ANDUIN);
			ASSERT(creature);
			return creature;
		}

		Creature* GetKelThuzad()
		{
			Creature* creature = GetCreature(DATA_KELTHUZAD);
			ASSERT(creature);
			return creature;
		}

		Creature* GetKaelThas()
		{
			Creature* creature = GetCreature(DATA_KAELTHAS);
			ASSERT(creature);
			return creature;
		}

		#pragma endregion

		// Utils
		#pragma region UTILS

		Player* GetPlayer()
		{
			Map::PlayerList const& players = instance->GetPlayers();
			if (players.empty())
				return nullptr;

			return players.begin()->GetSource();
		}

		void DespawnVision(Visions type)
		{
			Player* player = GetPlayer();
			if (!player)
				return;

			for (auto& guid : visions[type])
			{
				if (guid.IsCreature())
				{
					if (Creature* creature = ObjectAccessor::GetCreature(*player, guid))
					{
						creature->SetVisible(false);
						creature->DespawnOrUnsummon(2s);
					}
				}
				else if (guid.IsGameObject())
				{
					if (GameObject* gob = ObjectAccessor::GetGameObject(*player, guid))
						gob->Delete();
				}
			}

			visions[type].clear();
		}

		void StartKelThuzadCombat()
		{
            if (GameObject* barrier = GetGameObject(DATA_FEL_BARRIER))
                barrier->ResetDoorOrButton();

			if (Creature* kelthuzad = GetCreature(DATA_KELTHUZAD))
				kelthuzad->AI()->DoAction(ACTION_KELTHUZAD_COMBAT_READY);
		}

        void SetGameObjectFlags(uint32 type, GameObjectFlags flags)
        {
            if (GameObject* go = GetGameObject(type))
            {
                go->SetFlag(flags);
                go->SetVignette(0);
            }
        }

        void RemoveGameObjectFlags(uint32 type, GameObjectFlags flags)
        {
            if (GameObject* go = GetGameObject(type))
            {
                go->RemoveFlag(flags);
                go->SetVignette(VIGNETTE_USABLE_INGREDIENTS);
            }
        }

		void ApplyHauntingMemoryAura(Creature* const creature)
		{
			creature->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
			creature->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
			creature->AddAura(SPELL_HAUNTING_MEMORY, creature);
            creature->SetVignette(VIGNETTE_USABLE_INGREDIENTS);
		}

		void StartConversation(uint32 entry)
		{
			Player* player = GetPlayer();
			ASSERT(player);

			Conversation::CreateConversation(entry, player, *player, player->GetGUID());
		}

		#pragma endregion
	};

	InstanceScript* GetInstanceScript(InstanceMap* map) const override
	{
		return new scenario_dalaran_convo_InstanceScript(map);
	}
};

void AddSC_scenario_dalaran_convo()
{
	new scenario_dalaran_convo();
}
