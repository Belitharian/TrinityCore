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
	{ NPC_CAULDRON_BUNNY,               DATA_CAULDRON_BUNNY             },
	{ 0,                                0                               }   // END
};

const ObjectData gameobjectData[] =
{
	{ GOB_ALCHEMICAL_SOLUTION,          DATA_ALCHEMICAL_SOLUTION        },
	{ GOB_SKULL,                        DATA_SKULL                      },
	{ GOB_ESSENCE_OF_DEATH,             DATA_ESSENCE_OF_DEATH           },
	{ 0,                                0                               }   // END
};

std::unordered_map<VisionType, std::list<VisionData>> visionData =
{
    {
        VISION_TYPE_JAINA,
        {
            {
                .EntryOrData = DATA_JAINA_PROUDMOORE_VISION,
                .Type        = HighGuid::Uniq,
                .Position    = RoomCenter,
                .Auras       = { SPELL_WATER_CHANNELING },
            },
            {
                .EntryOrData = NPC_INVISIBLE_STALKER,
                .Type        = HighGuid::Creature,
                .Position    = RoomCenter,
                .Auras       = { SPELL_WATER_WAVE, SPELL_LIGHTNING_AURA },
            },
        }
    },
    {
        VISION_TYPE_KALECGOS,
        {
            {
                .EntryOrData = DATA_KALECGOS,
                .Type        = HighGuid::Uniq,
                .Position    = { -846.94f, 4466.65f, 588.84f, 0.60f },
            },
            {
                .EntryOrData = DATA_JAINA_PROUDMOORE_VISION,
                .Type        = HighGuid::Uniq,
                .Position    = { -844.68f, 4470.23f, 589.15f, 4.69f },
                .Emotes      = { EMOTE_STATE_TALK_SUBDUED },
                .Auras       = { SPELL_SIT_CHAIR_MED },
            },
            {
                .EntryOrData = GOB_DALARAN_COUCH,
                .Type        = HighGuid::GameObject,
                .Position    = { -844.68f, 4470.16f, 588.84f, 4.69f },
            },
            {
                .EntryOrData = GOB_DALARAN_TABLE,
                .Type        = HighGuid::GameObject,
                .Position    = { -844.71f, 4467.60f, 588.84f, 0.09f },
            },
            {
                .EntryOrData = GOB_TOME_OF_POWER,
                .Type        = HighGuid::GameObject,
                .Position    = { -845.32f, 4466.32f, 589.78f, 1.60f },
            },
        },
    },
    {
        VISION_TYPE_KEL_THUZAD,
        {
            {
                .EntryOrData = DATA_KELTHUZAD,
                .Flags       = NoDespawn,
                .Type        = HighGuid::Uniq,
                .Position    = { -845.31f, 4465.29f, 588.84f, 1.30f },
            },
            {
                .EntryOrData = NPC_MR_BIGGLESWORTH,
                .Type        = HighGuid::Creature,
                .Position    = { -840.42f, 4469.93f, 588.84f, 3.80f },
                .Auras       = { SPELL_SLEEPING },
            },
            {
                .EntryOrData = NPC_CAULDRON_BUNNY,
                .Type        = HighGuid::Creature,
                .Position    = RoomCenter,
            },
            {
                .EntryOrData = GOB_CAULDRON,
                .Type        = HighGuid::GameObject,
                .Position    = RoomCenter,
            },
            {
                .EntryOrData = GOB_ALCHEMICAL_SOLUTION,
                .Type        = HighGuid::GameObject,
                .Position    = { -850.70f, 4464.40f, 588.84f, 0.44f },
            },
            {
                .EntryOrData = GOB_SKULL,
                .Type        = HighGuid::GameObject,
                .Position    = { -830.32f, 4468.58f, 590.65f, 4.03f },
            },
            {
                .EntryOrData = GOB_PUISUIT_TERNAL_LIFE,
                .Type        = HighGuid::GameObject,
                .Position    = { -845.53f, 4474.29f, 588.84f, 4.79f },
            },
            {
                .EntryOrData = GOB_BAG_OF_GRAIN,
                .Type        = HighGuid::GameObject,
                .Position    = { -847.10f, 4467.32f, 588.84f, 1.87f },
            },
            {
                .EntryOrData = GOB_ESSENCE_OF_DEATH,
                .Type        = HighGuid::GameObject,
                .Position    = { -855.95f, 4476.79f, 590.12f, 5.61f },
            },
        }
    }
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
                    case Phases::Visions:
                        StartConversation(CONVERSATION_VISIONS);
                        break;
                    case Phases::Visions_Jaina:
                        SpawnActors(VISION_TYPE_JAINA);
                        GetJainaVision()->SetObjectScale(2.5f);
                        break;
                    case Phases::Visions_KalecgosJaina:
                        DespawnActors(VISION_TYPE_JAINA);
                        SpawnActors(VISION_TYPE_KALECGOS);
                        break;
                    case Phases::Visions_KelThuzad:
                        DespawnActors(VISION_TYPE_KALECGOS);
                        SpawnActors(VISION_TYPE_KEL_THUZAD);
                        SetIngredientState(DATA_ALCHEMICAL_SOLUTION);
                        SetIngredientState(DATA_SKULL);
                        SetIngredientState(DATA_ESSENCE_OF_DEATH);
                        break;
                    case Phases::Visions_KelThuzad_Combat:
                        DespawnActors(VISION_TYPE_KEL_THUZAD);
                        StartKelThuzadCombat();
                        break;
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

		void OnCompletedCriteriaTree(CriteriaTree const* tree) override
		{
			switch (tree->ID)
			{
				// Find Lady Jaina Proudmoore
				case CRITERIA_TREE_01_FIND_JAINA:
					SetData(DATA_PHASE, (uint32)Phases::Introduction);
					break;
				// Discuss the fate of the Kirin Tor
				case CRITERIA_TREE_01_DISCUSS:
					SetData(DATA_PHASE, (uint32)Phases::Start);
					events.ScheduleEvent(EVENT_START, 1s);
					break;
                // Dalaran Fate - Jaina Guardian's Room (PARENT)
                case CRITERIA_TREE_02_PARENT:
                    SetData(DATA_PHASE, (uint32)Phases::Visions);
                    break;
                // Dalaran Fate - Kalecgos (PARENT)
                case CRITERIA_TREE_03_PARENT:
                    SetData(DATA_PHASE, (uint32)Phases::Visions_KelThuzad);
                    break;
                // Add the ingredients to the cauldron
                case CRITERIA_TREE_04_CAULDRON:
                    SetData(DATA_PHASE, (uint32)Phases::Visions_KelThuzad_Combat);
                    break;
			}
		}

		void OnCreatureCreate(Creature* creature) override
		{
			InstanceScript::OnCreatureCreate(creature);

			switch (creature->GetEntry())
			{
				// Generic
				case NPC_JAINA_PROUDMOORE:
					creature->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
					break;

				// Visions
				case NPC_KALECGOS:
				case NPC_JAINA_PROUDMOORE_VISION:
				case NPC_KELTHUZAD:
				case NPC_KAELTHAS:
				case NPC_AETHAS_SUNREAVER:
				case NPC_ARCHMAGE_ANTONIDAS:
                    ApplyHauntingMemoryAura(creature, true);
                    break;

                // Misc
				case NPC_MR_BIGGLESWORTH:
				case NPC_BLOOD_ELF_NOBLE:
				case NPC_SUNREAVER_CITIZEN:
				case NPC_SUNREAVER_LIEUTENANT:
				case NPC_SUNREAVER_BATTLEMAGE:
				case NPC_SUNREAVER_MAGE:
				case NPC_DALARAN_CITIZEN_01:
				case NPC_DALARAN_CITIZEN_02:
				case NPC_DALARAN_CITIZEN_03:
					ApplyHauntingMemoryAura(creature, false);
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
					go->SetFlag(GO_FLAG_NOT_SELECTABLE);
					break;

                case GOB_CAULDRON:
                case GOB_ALCHEMICAL_SOLUTION:
                case GOB_SKULL:
                case GOB_PUISUIT_TERNAL_LIFE:
                case GOB_BAG_OF_GRAIN:
                case GOB_ESSENCE_OF_DEATH:
                    ApplyHauntingMemoryAura(go, false);
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

				default:
					break;
			}
		}

		EventMap events;
		uint32 eventId;
		Phases phase;
        std::unordered_map<VisionType, std::vector<VisionGuid>> actors;

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

		void StartKelThuzadCombat()
		{
			if (Creature* kelthuzad = GetCreature(DATA_KELTHUZAD))
				kelthuzad->AI()->DoAction(ACTION_KELTHUZAD_COMBAT_READY);
		}

        void SpawnActors(VisionType type, std::function<void(const std::vector<VisionGuid>&)> onSpawn = nullptr)
        {
            Creature* jaina = GetJaina();

            for (const VisionData& data : visionData[type])
            {
                ObjectGuid guid = SpawnActor(jaina, data);
                actors[type].push_back({ &data, guid });
            }

            if (onSpawn)
                onSpawn(actors[type]);
        }

        // Retourne le GUID de l'acteur spawné, ou Empty si non applicable.
        ObjectGuid SpawnActor(Creature* jaina, const VisionData& data)
        {
            switch (data.Type)
            {
                case HighGuid::Uniq:
                {
                    Creature* summon = GetCreature(data.EntryOrData);
                    TeleportActor(summon, data.Position);
                    ApplyAuras(summon, data.Auras);
                    return ObjectGuid::Empty;
                }
                case HighGuid::Creature:
                {
                    Creature* summon = jaina->SummonCreature(data.EntryOrData, data.Position);
                    ApplyHauntingMemoryAura(summon, false);
                    ApplyAuras(summon, data.Auras);
                    return summon->GetGUID();
                }
                case HighGuid::GameObject:
                {
                    GameObject* summon = jaina->SummonGameObject(data.EntryOrData, data.Position,
                        QuaternionData::fromEulerAnglesZYX(data.Position.GetOrientation(), 0.f, 0.f), 0s);
                    ApplyHauntingMemoryAura(summon, false);
                    return summon->GetGUID();
                }
                default:
                    return ObjectGuid::Empty;
            }
        }

        void ApplyAuras(Creature* creature, const std::unordered_set<uint32>& auras)
        {
            if (auras.empty())
                return;

            for (uint32 spellId : auras)
                creature->CastSpell(creature, spellId, true);
        }

        void DespawnActors(VisionType type)
        {
            Creature* jaina = GetJaina();

            for (const VisionGuid& guid : actors[type])
            {
                if (guid.Data->HasFlags(NoDespawn))
                    continue;

                switch (guid.Data->Type)
                {
                    case HighGuid::Uniq:
                    {
                        Creature* summon = GetCreature(guid.Data->EntryOrData);
                        if (summon)
                            ResetActor(summon);
                        break;
                    }
                    case HighGuid::Creature:
                    {
                        Creature* summon = ObjectAccessor::GetCreature(*jaina, guid.Guid);
                        if (summon)
                            summon->DespawnOrUnsummon();
                        break;
                    }
                    case HighGuid::GameObject:
                    {
                        GameObject* go = ObjectAccessor::GetGameObject(*jaina, guid.Guid);
                        if (go)
                            go->Delete();
                        break;
                    }
                }
            }

            actors.erase(type);
        }

        void TeleportActor(Creature* const creature,  Position const position)
        {
            ProcTeleportVisual(creature, position, SPELL_SPAWN);
            ApplyHauntingMemoryAura(creature, false);
        }

        void ResetActor(Creature* const creature)
        {
            ProcTeleportVisual(creature, creature->GetHomePosition(), SPELL_SPAWN);
            ApplyHauntingMemoryAura(creature, true);

            creature->RemoveAllAurasException({ SPELL_HAUNTING_MEMORY, SPELL_SPOTLIGHT, SPELL_FREEZE_ANIMATION });
            creature->SetObjectScale(1.f);
        }

        void SetIngredientState(uint32 data)
        {
            GameObject* go = GetGameObject(data);
            if (!go) return;
            go->SetVignette(VIGNETTE_PURPLE);
            go->RemoveFlag(GO_FLAG_NOT_SELECTABLE);
        }

		void ApplyHauntingMemoryAura(WorldObject* const wo, bool highlight)
		{
            if (Creature* creature = wo->ToCreature())
            {
                creature->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                creature->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
                creature->SetVignette(VIGNETTE_YELLOW);
                creature->AddAura(SPELL_HAUNTING_MEMORY, creature);

                if (highlight)
                {
                    creature->AddAura(SPELL_SPOTLIGHT, creature);
                    creature->AddAura(SPELL_FREEZE_ANIMATION, creature);
                }
                else
                {
                    creature->RemoveAurasDueToSpell(SPELL_SPOTLIGHT);
                    creature->RemoveAurasDueToSpell(SPELL_FREEZE_ANIMATION);
                }
            }
            else if (GameObject* go = wo->ToGameObject())
            {
                go->SetVignette(VIGNETTE_YELLOW);
                go->SetFlag(GO_FLAG_NOT_SELECTABLE);
            }
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
