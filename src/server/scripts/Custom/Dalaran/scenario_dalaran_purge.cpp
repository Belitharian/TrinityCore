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
	{ NPC_VEREESA_WINDRUNNER,           DATA_VEREESA_WINDRUNNER         },
	{ NPC_SUMMONED_WATER_ELEMENTAL,     DATA_SUMMONED_WATER_ELEMENTAL   },
	{ NPC_BOUND_WATER_ELEMENTAL,        DATA_BOUND_WATER_ELEMENTAL      },
	{ NPC_SORIN_MAGEHAND,               DATA_SORIN_MAGEHAND             },
	{ NPC_MAGE_COMMANDER_ZUROS,         DATA_MAGE_COMMANDER_ZUROS       },
	{ NPC_ARCHMAGE_LANDALOCK,           DATA_ARCHMAGE_LANDALOCK         },
	{ NPC_MAGISTER_HATHOREL,            DATA_MAGISTER_HATHOREL          },
	{ NPC_MAGISTER_SURDIEL,             DATA_MAGISTER_SURDIEL           },
	{ NPC_ARCANIST_RATHAELLA,           DATA_ARCANIST_RATHAELLA         },
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
			eventId(1), phase(DLPPhases::FindJaina01)
		{
			SetHeaders(DataHeader);
			LoadObjectData(creatureData, gameobjectData);
		}

		void OnPlayerEnter(Player* player) override
		{
			player->CastSpell(player, SPELL_PHASE_CITIZENS, true);
			player->CastSpell(player, SPELL_PHASE_HIGH_SUNREAVER_MAGES, true);
		}

		void OnPlayerLeave(Player* player) override
		{
			player->RemoveAurasDueToSpell(SPELL_PHASE_CITIZENS);
			player->RemoveAurasDueToSpell(SPELL_PHASE_HIGH_SUNREAVER_MAGES);
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
                case EVENT_FIND_JAINA_02:
                    #ifdef DEBUG
                        DoSendScenarioEvent(EVENT_FIND_JAINA_02);
                    #else
                        SetData(DATA_SCENARIO_PHASE, (uint32)DLPPhases::FreeTheArcanist);
                    #endif
                    events.ScheduleEvent(14, 2s);
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
                {
                    if (Creature* aethas = instance->SummonCreature(NPC_AETHAS_SUNREAVER, AethasPoint01.spawn))
                        aethas->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, AethasPoint01.destination, true, AethasPoint01.destination.GetOrientation());
                    if (Creature* surdiel = GetCreature(DATA_MAGISTER_SURDIEL))
                    {
                        if (Creature* zuros = GetCreature(DATA_MAGE_COMMANDER_ZUROS))
                        {
                            surdiel->AI()->AttackStart(zuros);
                            zuros->AI()->AttackStart(surdiel);
                    }
                }
                    SetData(DATA_SCENARIO_PHASE, (uint32)DLPPhases::FindingTheThieves);
                    #ifdef DEBUG
                        events.ScheduleEvent(13, 1s);
                    #else
                        events.ScheduleEvent(1, 10s);
                    #endif
                    break;
                }
				// Finding the thieves
				case CRITERIA_TREE_FINDING_THE_THIEVES:
                {
                    for (Creature* creature : patrol)
                    {
                        creature->SetFaction(FACTION_DALARAN_PATROL);
                        creature->setActive(true);
                        creature->SetVisible(true);
                    }
                    DoRemoveAurasDueToSpellOnPlayers(SPELL_PHASE_HIGH_SUNREAVER_MAGES, true, true);
                    break;
                }
				// A Facelift
				case CRITERIA_TREE_A_FACELIFT:
                {
                    if (Creature* jaina = GetJaina())
                    {
                        jaina->SetVisible(true);
                        jaina->SetImmuneToAll(true);
                        jaina->RemoveAllAuras();
                        jaina->NearTeleportTo(JainaPos01);
                    }
                    for (Creature* creature : patrol)
                    {
                        creature->CombatStop();
                        creature->SetFaction(FACTION_FRIENDLY);
                        creature->setActive(false);
                        creature->SetVisible(false);
                    }
                    DoRemoveAurasDueToSpellOnPlayers(SPELL_PHASE_CITIZENS, true, true);
                    SetData(DATA_SCENARIO_PHASE, (uint32)DLPPhases::FindJaina02);
                    break;
                }
                // First Step
                case CRITERIA_TREE_FIRST_STEP:
                {
                    if (Creature* rathaella = GetCreature(DATA_ARCANIST_RATHAELLA))
                        rathaella->AddNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
                    if (Creature* landalock = GetCreature(DATA_ARCHMAGE_LANDALOCK))
                    {
                        landalock->AddUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
                        landalock->SetNpcFlags(UNIT_NPC_FLAG_GOSSIP);
                        landalock->CastSpell(landalock, SPELL_CHAT_BUBBLE, true);
                    }
                    break;
                }
                // An Unfortunate Capture
                case CRITERIA_TREE_UNFORTUNATE_CAPTURE:
                {
                    DoRemoveAurasDueToSpellOnPlayers(SPELL_WAND_OF_DISPELLING);
                    DoCastSpellOnPlayers(SPELL_WAND_OF_DISPELLING);
                    SetData(DATA_SCENARIO_PHASE, (uint32)DLPPhases::FreeCitizens);
                    break;
                }
				default:
					break;
			}
		}

		void OnCreatureCreate(Creature* creature) override
		{
			InstanceScript::OnCreatureCreate(creature);

			creature->SetVisibilityDistanceOverride(VisibilityDistanceType::Large);
			creature->AddPvpFlag(UNIT_BYTE2_FLAG_PVP);
			creature->AddUnitFlag(UNIT_FLAG_PVP_ENABLING);

			switch (creature->GetEntry())
			{
                case NPC_DALARAN_CITIZEN:
                    if (roll_chance_i(30))
                        creature->SetEmoteState(EMOTE_STATE_COWER);
                    break;
                case NPC_MAGE_COMMANDER_ZUROS:
                case NPC_MAGISTER_SURDIEL:
                    creature->SetImmuneToPC(true);
                    break;
                case NPC_WANTON_HOST:
                case NPC_WANTON_HOSTESS:
                    creature->SetImmuneToAll(true);
                    creature->SetLevel(60);
                    creature->SetMaxHealth(urand(15800, 35000));
                    creature->SetFullHealth();
                    creature->SetFaction(FACTION_HORDE_GENERIC);
                    break;
				case NPC_ARCANIST_RATHAELLA:
                    creature->SetImmuneToNPC(true);
                    creature->SetStandState(UNIT_STAND_STATE_KNEEL, 14904);
					creature->CastSpell(creature, SPELL_ATTACHED);
					break;
				case NPC_MAGISTER_HATHOREL:
					creature->SetImmuneToAll(true);
					break;
				case NPC_ARCHMAGE_LANDALOCK:
					creature->SetImmuneToAll(true);
					creature->CastSpell(creature, SPELL_FROST_CANALISATION);
					break;
				case NPC_ICE_WALL:
					creature->SetImmuneToAll(true);
					creature->SetUnitFlags(UNIT_FLAG_UNINTERACTIBLE);
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
					creature->SetImmuneToAll(true);
					creature->SetWalk(true);
					creature->AddAura(SPELL_CASTER_READY_03, creature);
					break;
				case NPC_HIGH_SUNREAVER_MAGE:
					creature->SetImmuneToAll(true);
					creature->AddAura(SPELL_CASTER_READY_02, creature);
					highmages.push_back(creature);
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
                // Dalaran
                #pragma region DALARAN

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
                        GetElemental()->CastSpell(GetAethas(), SPELL_FROSTBOLT);
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

                #pragma endregion

                // First Step
                #pragma region FIRST_STEP

                case 14:
                    GetVereesa()->SetFacingToObject(GetJaina());
                    GetJaina()->SetFacingToObject(GetVereesa());
                    Next(2s);
                    break;
                case 15:
                    Talk(GetVereesa(), SAY_FIRST_STEP_VEREESA_01);
                    Next(5s);
                    break;
                case 16:
                    Talk(GetJaina(), SAY_FIRST_STEP_JAINA_02);
                    Next(4s);
                    break;
                case 17:
                    if (Creature* jaina = GetJaina())
                    {
                        Talk(jaina, SAY_FIRST_STEP_JAINA_03);
                        if (Player* player = GetNearestPlayer(jaina))
                            jaina->SetFacingToObject(player);
                    }
                    Next(2s);
                    break;
                case 18:
                    if (Creature* vereesa = GetVereesa())
                    {
                        Talk(vereesa, SAY_FIRST_STEP_VEREESA_04);
                        if (Player* player = GetNearestPlayer(vereesa))
                            vereesa->SetFacingToObject(player);
                    }
                    Next(2s);
                    break;
                case 19:
                    DoSendScenarioEvent(EVENT_FIND_JAINA_02);
                    break;

                #pragma endregion

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
		Creature* GetVereesa() { return GetCreature(DATA_VEREESA_WINDRUNNER); }
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

        Player* GetNearestPlayer(Creature* creature)
        {
            Player* player = instance->GetPlayers().begin()->GetSource();
            if (player->IsWithinDist(creature, 5.f, false))
                return player;
            return nullptr;
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
