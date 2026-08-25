#include "CriteriaHandler.h"
#include "EventMap.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "KillRewarder.h"
#include "Map.h"
#include "MotionMaster.h"
#include "MiscPackets.h"
#include "ObjectMgr.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "Scenario.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"
#include "Weather.h"
#include "battle_for_theramore.h"

uint8 const eventCreatureDataCount = 14;

const ObjectData creatureData[] =
{
	{ NPC_JAINA_PROUDMOORE,     DATA_JAINA_PROUDMOORE       },
	{ NPC_KINNDY_SPARKSHINE,    DATA_KINNDY_SPARKSHINE      },
	{ NPC_KALECGOS,             DATA_KALECGOS               },
	{ NPC_ARCHMAGE_TERVOSH,     DATA_ARCHMAGE_TERVOSH       },
	{ NPC_PAINED,               DATA_PAINED                 },
	{ NPC_PERITH_STORMHOOVE,    DATA_PERITH_STORMHOOVE      },
	{ NPC_KNIGHT_OF_THERAMORE,  DATA_KNIGHT_OF_THERAMORE    },
	{ NPC_HEDRIC_EVENCANE,      DATA_HEDRIC_EVENCANE        },
	{ NPC_RHONIN,               DATA_RHONIN                 },
	{ NPC_VEREESA_WINDRUNNER,   DATA_VEREESA_WINDRUNNER     },
	{ NPC_THALEN_SONGWEAVER,    DATA_THALEN_SONGWEAVER      },
	{ NPC_TARI_COGG,            DATA_TARI_COGG              },
	{ NPC_AMARA_LEESON,         DATA_AMARA_LEESON           },
	{ NPC_THADER_WINDERMERE,    DATA_THADER_WINDERMERE      },
	{ NPC_KALECGOS_DRAGON,      DATA_KALECGOS_DRAGON        },
	{ NPC_CAPTAIN_DROK,         DATA_CAPTAIN_DROK           },
	{ NPC_WAVE_CALLER_GRUHTA,   DATA_WAVE_CALLER_GRUHTA     },
	{ 0,                        0                           }   // END
};

const ObjectData gameobjectData[] =
{
	{ GOB_PORTAL_TO_STORMWIND,  DATA_PORTAL_TO_STORMWIND    },
	{ GOB_PORTAL_TO_DALARAN,    DATA_PORTAL_TO_DALARAN      },
	{ GOB_PORTAL_TO_ORGRIMMAR,  DATA_PORTAL_TO_ORGRIMMAR    },
	{ GOB_MYSTIC_BARRIER_01,    DATA_MYSTIC_BARRIER_01      },
	{ GOB_MYSTIC_BARRIER_02,    DATA_MYSTIC_BARRIER_02      },
	{ GOB_ENERGY_BARRIER,       DATA_ENERGY_BARRIER         },
	{ GOB_POWDER_BARREL,        DATA_POWDER_BARREL          },
	{ 0,                        0                           }   // END
};

class HordeBombardierThrowBomb : public BasicEvent
{
	public:
		HordeBombardierThrowBomb(Unit* caster) : _caster(caster) { }

		bool Execute(uint64 /*execTime*/, uint32 /*diff*/) override
		{
			_caster->CastSpell(_caster, SPELL_THROW_BOMB, TRIGGERED_FULL_MASK);
			_caster->m_Events.AddEvent(this, _caster->m_Events.CalculateTime(Seconds(urand(8, 10))));
			return false;
		}

	private:
		Unit* _caster;
};

class HordeDemolisherThrowBoulder : public BasicEvent
{
    public:
    HordeDemolisherThrowBoulder(Unit* caster) : _caster(caster)
    {
        p1 = { -3771.834717f, -4261.928711f, 7.074570f, 4.655093f };
        p2 = { -3793.766357f, -4260.671387f, 6.944610f, 4.655093f };
    }

    bool Execute(uint64 /*execTime*/, uint32 /*diff*/) override
    {
        Position randomPos = GetRandomPointOnStrip(_caster, p1, p2, 4.0f, _caster->GetMap());
        _caster->CastSpell(randomPos, SPELL_THROW_BOULDER, TRIGGERED_FULL_MASK);
        _caster->m_Events.AddEvent(this, _caster->m_Events.CalculateTime(Seconds(urand(8, 10))));
        return false;
    }

    Position GetRandomPointOnStrip(Unit* unit, Position const& p1, Position const& p2, float widthMeters, Map* map)
    {
        float dx = p2.GetPositionX() - p1.GetPositionX();
        float dy = p2.GetPositionY() - p1.GetPositionY();
        float len = std::sqrt(dx * dx + dy * dy);

        float ux = dx / len, uy = dy / len;   // direction normalisée
        float px = -uy, py = ux;              // perpendiculaire 2D

        float t = frand(0.0f, 1.0f);
        float offset = frand(-widthMeters / 2.0f, widthMeters / 2.0f);

        float x = p1.GetPositionX() + dx * t + px * offset;
        float y = p1.GetPositionY() + dy * t + py * offset;
        float z = p1.GetPositionZ() + (p2.GetPositionZ() - p1.GetPositionZ()) * t;

        // recalage sur le vrai sol plutôt que l'interpolation linéaire
        float groundZ = map->GetHeight(unit->GetPhaseShift(), x, y, z + 2.0f, true);
        if (groundZ > INVALID_HEIGHT)
            z = groundZ;

        return Position(x, y, z, 0.f);
    }

    private:
    Unit* _caster;
    Position p1, p2;
};

class scenario_battle_for_theramore : public InstanceMapScript
{
	public:
	scenario_battle_for_theramore() : InstanceMapScript(BFTScriptName, 5000)
	{
	}

	struct scenario_battle_for_theramore_InstanceScript : public InstanceScript
	{
		scenario_battle_for_theramore_InstanceScript(InstanceMap* map) : InstanceScript(map),
			phase(BFTPhases::FindJaina), eventId(1), woundedTroops(0), archmagesIndex(0),
			waves(0)
		{
			SetHeaders(DataHeader);
			LoadObjectData(creatureData, gameobjectData);
		}

		enum Spells
		{
			SPELL_WATER_BUCKET          = 42336,
			SPELL_MASS_TELEPORT         = 60516,
			SPELL_MAGIC_QUILL           = 424726,
			SPELL_TIED_UP               = 167469,
			SPELL_CLOSE_PORTAL          = 203542,
			SPELL_DISSOLVE              = 255295,
			SPELL_PRISMATIC_BARRIER     = 235450,
			SPELL_METEOR                = 276973,
			SPELL_ARCANE_CANALISATION   = 288451,
			SPELL_BLAZING_BARRIER       = 295238,
			SPELL_FROST_BREATH          = 300548,
			SPELL_CHILLING_BLAST        = 337053,
			SPELL_ICY_GLARE             = 338517,
			SPELL_VANISH                = 199483,
			SPELL_BIG_EXPLOSION         = 348750,
			SPELL_TELEPORT              = 357601,
			SPELL_SCORCHED_EARTH        = 373139,
			SPELL_ARCANIC_CELL          = 398947,
			SPELL_READING_BOOK_STANDING = 397765,
			SPELL_AREA_TRIGGER_VISUAL   = 473554,
		};

		uint32 Waves[HORDE_WAVES_COUNT] =
		{
			DATA_WAVE_WEST,
			DATA_WAVE_CITADEL,
			DATA_WAVE_DOCKS,
			DATA_WAVE_DOORS,
			DATA_WAVE_WEST,
			DATA_WAVE_CITADEL,
			DATA_WAVE_DOCKS,
			DATA_WAVE_WEST,
			DATA_WAVE_CITADEL,
			DATA_WAVE_DOORS
		};

		uint32 GetData(uint32 dataId) const override
		{
			if (dataId == DATA_SCENARIO_PHASE)
				return (uint32)phase;
			else if (dataId == DATA_WOUNDED_TROOPS)
				return woundedTroops;
			else if (dataId == DATA_WAVE_GROUP_ID)
				return Waves[waves];
			return 0;
		}

		void OnPlayerEnter(Player* /*player*/) override
		{
			ForceWeather(WEATHER_STATE_THUNDERS, true);
		}

		void SetData(uint32 dataId, uint32 value) override
		{
			if (dataId == DATA_SCENARIO_PHASE)
				phase = (BFTPhases)value;
			else if (dataId == DATA_WOUNDED_TROOPS)
				woundedTroops = value;
		}

		void OnUnitDeath(Unit* unit) override
		{
			InstanceScript::OnUnitDeath(unit);

			Creature* creature = unit->ToCreature();
			if (!creature || creature->IsPet() || creature->IsCritter())
				return;

			// Un joueur a tapé : KillRewarder a déjà crédité le criteria
			if (!creature->GetTapList().empty())
				return;

            Map::PlayerList const& players = instance->GetPlayers();
            MapReference const* reference = players.front();
            Player* player = reference->GetSource();

            if (!player)
                return;

            switch (creature->GetEntry())
            {
                case NPC_ROKNAH_GRUNT:
                case NPC_ROKNAH_LOA_SINGER:
                case NPC_ROKNAH_HAG:
                case NPC_ROKNAH_FELCASTER:
                    KillRewarder::Reward(player, creature, creature->GetCreatureTemplate()->KillCredit[0]);
                    break;
            }
		}

		void OnCompletedCriteriaTree(CriteriaTree const* tree) override
		{
			switch (tree->ID)
			{
				// Step 1 : Find Jaina
				case CRITERIA_TREE_FIND_JAINA:
				{
					ClosePortal(DATA_PORTAL_TO_STORMWIND);
					GetTervosh()->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					GetKinndy()->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					GetKalec()->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					if (Creature* jaina = GetCreature(DATA_JAINA_PROUDMOORE))
					{
						Talk(jaina, SAY_REUNION_1);
						SetTarget(jaina);
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::TheCouncil);
					events.ScheduleEvent(1, 2s);
					break;
				}
				// Step 2 : The Council
				case CRITERIA_TREE_THE_COUNCIL:
				{
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::Waiting);
					events.ScheduleEvent(24, 10s);
					break;
				}
				// Step 3 : Waiting
				case CRITERIA_TREE_WAITING:
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::UnknownTauren);
					events.ScheduleEvent(25, 1s);
					break;
				// Step 4 : The Unknow Tauren
				case CRITERIA_TREE_UNKNOW_TAUREN:
				{
					if (Creature* kinndy = GetKinndy())
					{
						kinndy->SetVisible(true);
						kinndy->GetMotionMaster()->MovePath(KinndyPath02, false);
					}
					if (Creature* tervosh = GetTervosh())
					{
						tervosh->SetVisible(true);
						tervosh->GetMotionMaster()->MovePath(TervoshPath03, false);
					}
					for (ObjectGuid guid : citizens)
					{
						if (Creature* citizen = instance->GetCreature(guid))
						{
							citizen->SetNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
							citizen->SetVignette(VIGNETTE_INTERACTION);
						}
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::Evacuation);
					break;
				}
				// Step 5 : Evacuation
				case CRITERIA_TREE_EVACUATION:
				{
                    for (ObjectGuid guid : citizens)
                    {
                        if (Creature* citizen = instance->GetCreature(guid))
                        {
                            citizen->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
                            citizen->SetVignette(VIGNETTE_NONE);
                        }
                    }
					if (Creature* jaina = GetJaina())
					{
                        jaina->SetVignette(VIGNETTE_LADY_JAINA_PROUDMOORE);
						jaina->NearTeleportTo(JainaPoint02);
						jaina->SetHomePosition(JainaPoint02);
						jaina->AI()->SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::ALittleHelp);
					}
					if (Creature* tervosh = GetTervosh())
					{
						tervosh->GetMotionMaster()->Clear();
						tervosh->GetMotionMaster()->MoveIdle();
						tervosh->NearTeleportTo(TervoshPoint01);
						tervosh->SetHomePosition(TervoshPoint01);
						tervosh->CastSpell(tervosh, SPELL_COSMETIC_FIRE_LIGHT);
					}
					if (Creature* kinndy = GetKinndy())
					{
						kinndy->GetMotionMaster()->Clear();
						kinndy->GetMotionMaster()->MoveIdle();
						kinndy->NearTeleportTo(KinndyPoint02);
						kinndy->SetHomePosition(KinndyPoint02);
					}
					if (Creature* kalecgos = GetKalec())
					{
						kalecgos->GetMotionMaster()->Clear();
						kalecgos->GetMotionMaster()->MoveIdle();
						kalecgos->NearTeleportTo(KalecgosPoint01);
						kalecgos->SetHomePosition(KalecgosPoint01);
						kalecgos->RemoveUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
						kalecgos->RemoveAllAuras();
					}
					if (Creature* hedric = GetHedric())
					{
						hedric->GetMotionMaster()->Clear();
						hedric->GetMotionMaster()->MoveIdle();
						hedric->SetHomePosition(HedricPoint01);
						hedric->NearTeleportTo(HedricPoint01);
						hedric->SetVisible(false);
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::ALittleHelp);
					break;
				}
				// Step 6 : A Little Help
				case CRITERIA_TREE_A_LITTLE_HELP:
				{
					for (uint8 i = 0; i < FIRE_LOCATION; i++)
					{
						const Position pos = FireLocation[i];
						if (TempSummon* trigger = instance->SummonCreature(NPC_THERAMORE_FIRE_CREDIT, pos))
							trigger->AddAura(SPELL_COSMETIC_LARGE_FIRE, trigger);
					}
					for (ObjectGuid guid : tanks)
					{
						if (Creature* tank = instance->GetCreature(guid))
						{
							tank->SetNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
							tank->SetVignette(VIGNETTE_INTERACTION);
							tank->SetRegenerateHealth(false);
							tank->SetHealth((float)tank->GetHealth() * frand(0.15f, 0.60f));
						}
					}
					for (ObjectGuid guid : civilians)
					{
						if (Creature* citizen = instance->GetCreature(guid))
							citizen->SetVisible(false);
					}
					for (ObjectGuid guid : troops)
					{
						if (Creature* troop = instance->GetCreature(guid))
						{
							troop->SetVignette(VIGNETTE_ALLIANCE_TROOPS);
							switch (troop->GetCreatureTemplate()->unit_class)
							{
								case UNIT_CLASS_PALADIN:
									troop->SetEmoteState(EMOTE_STATE_READY2H);
									break;
								case UNIT_CLASS_MAGE:
									troop->SetEmoteState(RAND(EMOTE_STATE_READY1H, EMOTE_STATE_READY2HL));
									break;
								case UNIT_CLASS_ROGUE:
									break;
								default:
									troop->SetEmoteState(EMOTE_STATE_READY1H);
									break;
							}
						}
					}
                    GetJaina()->SummonGameObject(GOB_PORTAL_TO_ORGRIMMAR, PortalPoint02, QuaternionData::QuaternionData(), 0s);
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::Preparation);
					#ifndef CUSTOM_DEBUG
						events.ScheduleEvent(71, 1s);
					#else
						for (uint8 i = 0; i < ARCHMAGES_LOCATION; i++)
							instance->SummonCreature(archmagesLocation[i].dataId, PortalPoint01);
						events.ScheduleEvent(90, 2s);
					#endif
					break;
				}
				// Step 7 : Preparation - Parent
				case CRITERIA_TREE_PREPARATION:
                    GetJaina()->SetVignette(VIGNETTE_LADY_JAINA_PROUDMOORE);
                    SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::TheBattle);
					break;
				// Step 7 : Preparation - Troops motivated
                case CRITERIA_TREE_TOOPS_MOTIVATED:
                    for (ObjectGuid guid : troops)
                    {
                        if (Creature* troop = instance->GetCreature(guid))
                            troop->SetVignette(VIGNETTE_NONE);
                    }
                    break;
				// Step 7 : Preparation - Speak with Rhonin
				case CRITERIA_TREE_TALK_TO_RHONIN:
					EnsurePlayerHaveShield();
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::Preparation_Rhonin);
					break;
				// Step 7 : Preparation - Tanks events
				case CRITERIA_TREE_REPAIR_TANKS:
				{
					for (ObjectGuid guid : tanks)
					{
						if (Creature* tank = instance->GetCreature(guid))
						{
							if (Creature* fire = tank->FindNearestCreature(NPC_THERAMORE_FIRE_CREDIT, 5.f))
								fire->DespawnOrUnsummon();

							tank->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
							tank->SetRegenerateHealth(true);
							tank->SetHealth(tank->GetMaxHealth());
						}
					}
					break;
				}            
				// Step 8 : The Battle - Retrieve Lady Jaina Proudmoore
				case CRITERIA_TREE_RETRIEVE_JAINA:
				{
					if (Creature* jaina = GetJaina())
					{
						Talk(jaina, SAY_BATTLE_01);
						SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, jaina);
						jaina->SetBoundingRadius(20.f);
						jaina->SetRegenerateHealth(false);
					}
					if (Creature* rhonin = GetRhonin())
					{
						SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, rhonin);
						rhonin->SetRegenerateHealth(false);
					}
					if (Creature* kalecgos = GetKalecgos())
					{
						SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, kalecgos);
						kalecgos->SetRegenerateHealth(false);
					}
					if (Creature* drok = GetDrok())
					{
						drok->setActive(true);
						drok->SetVisible(true);
					}
					if (Creature* gruhta = GetGruhta())
					{
						gruhta->setActive(true);
						gruhta->SetVisible(true);
					}
					for (uint8 i = 0; i < tanks.size() - 1; i++)
					{
						if (Creature* tank = instance->GetCreature(tanks[i]))
							tank->KillSelf();
					}
                    HordeMembersInvoker(DATA_DECORATION_WEST, true);
                    SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::TheBattle_Survive);
					events.ScheduleEvent(91, 10s);
					break;
				}
				// Step 9 : The Battle - Parent
				case CRITERIA_TREE_SURVIVE_THE_BATTLE:
				{
					SpawnWoundedTroops();
					EnsurePlayerHaveBucket();
					RelocateTroops();
					GetBarrier01()->ResetDoorOrButton();
					GetBarrier02()->ResetDoorOrButton();
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::HelpTheWounded);
					events.ScheduleEvent(122, 3s);
					break;
				}
				// Step 9 : The Battle - After 10 waves
				case CRITERIA_TREE_SURVIVE_WAVES:
				{
					DespawnDummies();
					if (Creature* kalecgos = GetKalecgos())
						kalecgos->AI()->SetData(DATA_KALECGOS_CANCEL_EVENT, 0U);
                    events.CancelEvent(EVENT_WAVES_CHECKER);
                    break;
				}
                // Step 9 : The Battle - After the protection broke
                case CRITERIA_TREE_MAINTAIN_PROTECTION:
                {
                    HordeMembersInvoker(Waves[waves]);
                    waves++;
                    events.ScheduleEvent(EVENT_WAVES_CHECKER, 1s);
                    break;
                }
				// Step 10 : Help the wounded - Parent
				case CRITERIA_TREE_HELP_THE_WOUNDED:
				{
					for (uint8 i = 128; i < 141; i++)
						events.CancelEvent(i);
                    GetJaina()->SetVignette(VIGNETTE_LADY_JAINA_PROUDMOORE);
                    DoRemoveAurasDueToSpellOnPlayers(SPELL_RUNIC_SHIELD);
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::WaitForAmara);
					events.ScheduleEvent(141, 5ms);
					break;
				}
				// Step 10 : Help the wounded - Rejoin Lady Jaina Proudmoore after the attack
				case CRITERIA_TREE_FOLLOW_JAINA:
					events.ScheduleEvent(128, 3s);
					break;
				// Step 10 : Help the wounded - Help teleporting the wounded troops
				case CRITERIA_TREE_HELP_THE_TROOPS:
				{
					if (Creature* jaina = GetJaina())
					{
                        std::list<Creature*> results;
                        jaina->GetCreatureListWithEntryInGrid(results, NPC_THERAMORE_WOUNDED_TROOP, SIZE_OF_GRIDS);

						if (results.empty())
							return;

                        for (Creature* woundedTroop : results)
                        {
                            woundedTroop->SetVignette(VIGNETTE_NONE);
                            woundedTroop->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
                        }
					}
					break;
				}
				// Step 10 : Help the wounded - Extinguish the fires
				case CRITERIA_TREE_EXTINGUISH_FIRES:
					MassDespawn(NPC_THERAMORE_FIRE_CREDIT);
					DoRemoveAurasDueToSpellOnPlayers(SPELL_WATER_BUCKET);
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::HelpTheWounded_Extinguish);
					break;
				// Step 11 : Wait for Archmage Leeson returns - Parent
				case CRITERIA_TREE_WAIT_ARCHMAGE_LEESON:
					break;
				// Step 11 : Wait for Archmage Leeson returns - Rejoin Lady Jaina Proudmoore
				case CRITERIA_TREE_JOIN_JAINA:
				{
					if (Creature* kalecgos = GetKalec())
					{
						kalecgos->SetVisible(true);
						kalecgos->GetMotionMaster()->Clear();
						kalecgos->GetMotionMaster()->MoveIdle();
						kalecgos->NearTeleportTo(KalecPath02.Nodes[0].X, KalecPath02.Nodes[0].Y, KalecPath02.Nodes[0].Z, *KalecPath02.Nodes[0].Orientation);
					}
					if (Creature* rhonin = GetRhonin())
					{
						rhonin->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, RhoninPoint01, true, RhoninPoint01.GetOrientation());

						for (uint8 i = 0; i < TOWER_BARRIERS_LOCATION; i++)
						{
							rhonin->SummonGameObject(GOB_ENERGY_BARRIER_TOWER, TowerBarriers[i].position, TowerBarriers[i].quaternion, 0s);
						}
					}
					for (uint8 i = 0; i < eventCreatureDataCount; i++)
					{
						if (Creature* creature = GetCreature(creatureData[i].type))
							creature->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::WaitForAmara_JoinJaina);
					events.ScheduleEvent(142, 1s);
					break;
				}
				// Step 11 : Wait for Archmage Leeson returns - Wait for Archmage Leeson returns
				case CRITERIA_TREE_ARCHMAGE_LEESON:
					events.ScheduleEvent(156, 1s);
					break;
				// Step 12 : Retrieve Rhonin - Parent
				case CRITERIA_TREE_RETRIEVE_RHONIN:
					DoCastSpellOnPlayers(SPELL_THERAMORE_EXPLOSION_SCENE);
					break;
				// Step 12 : Retrieve Rhonin - Retrieve Rhonin at the top of the tower
				case CRITERIA_TREE_RETRIEVE:
                    GetRhonin()->SetVignette(VIGNETTE_NONE);
                    SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::RetrieveRhonin_JoinRhonin);
					events.ScheduleEvent(161, 1s);
					break;
				// Step 12 : Retrieve Rhonin - Localize the bomb
				case CRITERIA_TREE_REDUCE_EXPLOSION:
					EnsurePlayersAreInPhase(PHASE_THERAMORE_SCENE_EXPLOSION);
					break;
			}
		}

		void OnCreatureCreate(Creature* creature) override
		{
			InstanceScript::OnCreatureCreate(creature);

            creature->SetVisibilityDistanceOverride(VisibilityDistanceType::Gigantic);
            creature->SetPvpFlag(UNIT_BYTE2_FLAG_PVP);
            creature->SetUnitFlag(UNIT_FLAG_PVP_ENABLING);

			if (creature->IsCivilian())
			{
				creature->PauseMovement();
				creature->SetPvP(false);
				creature->SetImmuneToNPC(true);
				civilians.push_back(creature->GetGUID());
			}

			switch (creature->GetEntry())
			{
				case NPC_THERAMORE_CITIZEN_MALE:
				case NPC_THERAMORE_CITIZEN_FEMALE:
					creature->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
                    if (!creature->HasStringId("IgnoreClick"))
					    citizens.push_back(creature->GetGUID());
					break;
				case NPC_ARCHMAGE_TERVOSH:
					creature->SetEmoteState(EMOTE_STATE_READ_BOOK_AND_TALK);
					break;
				case NPC_UNMANNED_TANK:
					creature->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
					tanks.push_back(creature->GetGUID());
					break;
				case NPC_THERAMORE_MARKSMAN:
				case NPC_THERAMORE_FOOTMAN:
				case NPC_THERAMORE_ARCANIST:
				case NPC_THERAMORE_FAITHFUL:
				case NPC_THERAMORE_OFFICER:
					if (creature->GetWaypointPathId() || creature->IsFormationLeader() || creature->GetFormation())
						break;
					troops.push_back(creature->GetGUID());
					break;
				case NPC_CAPTAIN_DROK:
				case NPC_WAVE_CALLER_GRUHTA:
				case NPC_KALECGOS_DRAGON:
					creature->setActive(false);
					creature->SetVisible(false);
					break;
				case NPC_JAINA_PROUDMOORE:
					creature->SetVignette(VIGNETTE_LADY_JAINA_PROUDMOORE);
					break;
			}
		}

		void OnGameObjectCreate(GameObject* go) override
		{
			InstanceScript::OnGameObjectCreate(go);

			go->SetVisibilityDistanceOverride(VisibilityDistanceType::Large);

			switch (go->GetEntry())
			{
				case GOB_PORTAL_TO_DALARAN:
				case GOB_PORTAL_TO_STORMWIND:
				case GOB_PORTAL_TO_ORGRIMMAR:
					go->SetLootState(GO_READY);
					go->UseDoorOrButton();
					go->SetFlag(GO_FLAG_NOT_SELECTABLE);
					break;
				case GOB_ENERGY_BARRIER:
					go->SetFlag(GO_FLAG_NOT_SELECTABLE);
					break;
				default:
					break;
			}
		}

		void Update(uint32 diff) override
		{
			scheduler.Update(diff);

			events.Update(diff);
			switch (eventId = events.ExecuteEvent())
			{
				// The Council
				#pragma region THE_COUNCIL

				case 1:
					if (Creature* tervosh = GetTervosh())
					{
						tervosh->SetEmoteState(EMOTE_STAND_STATE_NONE);
						tervosh->GetMotionMaster()->MovePath(TervoshPath01, false);
					}
					Next(4s);
					break;
				case 2:
					if (Creature* kinndy = GetKinndy())
					{
						kinndy->SetWalk(true);
						kinndy->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, KinndyPoint01, true, 1.09f);
					}
					#ifdef CUSTOM_DEBUG
						events.ScheduleEvent(20, 2s);
					#else
						Next(5s);
					#endif
					break;
				case 3:
					Talk(GetKinndy(), SAY_REUNION_2);
					SetTarget(GetKinndy());
					Next(13s);
					break;
				case 4:
					Talk(GetJaina(), SAY_REUNION_3);
					SetTarget(GetJaina());
					Next(12s);
					break;
				case 5:
					Talk(GetKinndy(), SAY_REUNION_4);
					SetTarget(GetKinndy());
					Next(6s);
					break;
				case 6:
					Talk(GetJaina(), SAY_REUNION_5);
					SetTarget(GetJaina());
					Next(8s);
					break;
				case 7:
					Talk(GetTervosh(), SAY_REUNION_6);
					SetTarget(GetTervosh());
					Next(8s);
					break;
				case 8:
					Talk(GetKalec(), SAY_REUNION_7);
					SetTarget(GetKalec());
					Next(6s);
					break;
				case 9:
					Talk(GetKalec(), SAY_REUNION_8);
					Next(9s);
					break;
				case 10:
					Talk(GetTervosh(), SAY_REUNION_9);
					Next(1s);
					break;
				case 11:
					Talk(GetKinndy(), SAY_REUNION_9_BIS);
					Next(4s);
					break;
				case 12:
					Talk(GetJaina(), SAY_REUNION_10);
					SetTarget(GetJaina());
					Next(6s);
					break;
				case 13:
					Talk(GetKalec(), SAY_REUNION_11);
					SetTarget(GetKalec());
					Next(4s);
					break;
				case 14:
					Talk(GetJaina(), SAY_REUNION_12);
					SetTarget(GetJaina());
					Next(6s);
					break;
				case 15:
					Talk(GetKinndy(), SAY_REUNION_13);
					SetTarget(GetKinndy());
					Next(6s);
					break;
				case 16:
					Talk(GetKalec(), SAY_REUNION_14);
					SetTarget(GetKalec());
					Next(7s);
					break;
				case 17:
					Talk(GetJaina(), SAY_REUNION_15);
					SetTarget(GetJaina());
					Next(4s);
					break;
				case 18:
					Talk(GetKalec(), SAY_REUNION_16);
					SetTarget(GetKalec());
					Next(4s);
					break;
				case 19:
					Talk(GetKalec(), SAY_REUNION_17);
					Next(4s);
					break;
				case 20:
					ClearTarget();
					if (Creature* kalecgos = GetKalec())
					{
						kalecgos->SetSpeedRate(MOVE_WALK, 1.6f);
						kalecgos->GetMotionMaster()->MovePath(KalecPath01, false);
					}
					Next(2s);
					break;
				case 21:
					GetTervosh()->GetMotionMaster()->MovePath(TervoshPath02, false);
					Next(5s);
					break;
				case 22:
					GetKinndy()->GetMotionMaster()->MovePath(KinndyPath01, false);
					Next(6s);
					break;
				case 23:
					GetJaina()->SetWalk(true);
					GetJaina()->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_01, JainaPoint01, true, JainaPoint01.GetOrientation());
					break;

				#pragma endregion

				// Waiting
				#pragma region WAITING

				case 24:
					TriggerGameEvent(EVENT_WAITING);
					break;

				#pragma endregion

				// The Unknown Tauren
				#pragma region THE_UNKNOWN_TAUREN

				case 25:
					for (uint8 i = 0; i < PERITH_LOCATION; i++)
					{
						if (Creature* creature = instance->SummonCreature(perithLocation[i].dataId, perithLocation[i].position))
						{
							if (creature->GetEntry() == NPC_KNIGHT_OF_THERAMORE)
							{
								creature->SetSheath(SHEATH_STATE_UNARMED);
								creature->SetEmoteState(EMOTE_STATE_WAGUARDSTAND01);
							}

							creature->SetWalk(true);
							creature->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
							creature->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, perithLocation[i].destination, false, perithLocation[i].destination.GetOrientation());
						}
					}
					#ifdef CUSTOM_DEBUG
					{
						GetPerith()->DespawnOrUnsummon();
						GetKnight()->DespawnOrUnsummon();

						events.ScheduleEvent(70, 2s);
					}
					#else
						Next(7s);
					#endif
					break;
				case 26:
					GetJaina()->SetTarget(GetPerith()->GetGUID());
					Next(2s);
					break;
				case 27:
					Talk(GetPained(), SAY_WARN_1);
					SetTarget(GetPained());
					Next(1s);
					break;
				case 28:
					Talk(GetJaina(), SAY_WARN_2);
					SetTarget(GetJaina());
					Next(1s);
					break;
				case 29:
					Talk(GetPained(), SAY_WARN_3);
					SetTarget(GetPained());
					Next(6s);
					break;
				case 30:
					Talk(GetPained(), SAY_WARN_4);
					Next(7s);
					break;
				case 31:
					Talk(GetJaina(), SAY_WARN_5);
					SetTarget(GetJaina());
					Next(6s);
					break;
				case 32:
					Talk(GetPained(), SAY_WARN_6);
					SetTarget(GetPained());
					Next(10s);
					break;
				case 33:
					ClearTarget();
					GetPained()->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_NONE, GetJaina(), 1.8f);
					Next(2s);
					break;
				case 34:
					SetTarget(GetJaina());
					GetPained()->SetEmoteState(EMOTE_STATE_USE_STANDING);
					Next(1s);
					break;
				case 35:
					GetJaina()->SetEmoteState(EMOTE_STATE_USE_STANDING);
					GetPained()->SetEmoteState(EMOTE_STATE_NONE);
					Talk(GetPained(), SAY_WARN_7);
					Next(3s);
					break;
				case 36:
					GetJaina()->SetEmoteState(EMOTE_STATE_NONE);
					Talk(GetJaina(), SAY_WARN_8);
					Next(4s);
					break;
				case 37:
					Talk(GetJaina(), SAY_WARN_9);
					SetTarget(GetJaina());
					Next(2s);
					break;
				case 38:
					Talk(GetJaina(), SAY_WARN_10);
					ClearTarget();
					GetPained()->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_01, PainedPoint01, true, PainedPoint01.GetOrientation());
					Next(2s);
					break;
				case 39:
					GetKnight()->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_01, GetJaina(), 3.0f);
					Next(2s);
					break;
				case 40:
					Talk(GetKnight(), SAY_WARN_11);
					SetTarget(GetKnight());
					Next(4s);
					break;
				case 41:
					Talk(GetJaina(), SAY_WARN_12);
					SetTarget(GetJaina());
					Next(5s);
					break;
				case 42:
					ClearTarget();
					Talk(GetPained(), SAY_WARN_13);
					if (Creature* officer = GetKnight())
					{
						officer->GetMotionMaster()->MovePath(OfficerPath01, false);
						officer->DespawnOrUnsummon(15s);
					}
					GetPerith()->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_NONE, GetJaina(), 3.0f);
					Next(3s);
					break;
				case 43:
					Talk(GetPerith(), SAY_WARN_14);
					GetPerith()->SetTarget(GetJaina()->GetGUID());
					SetTarget(GetPerith());
					Next(10s);
					break;
				case 44:
					Talk(GetJaina(), SAY_WARN_15);
					Next(4s);
					break;
				case 45:
					Talk(GetPerith(), SAY_WARN_16);
					Next(11s);
					break;
				case 46:
					Talk(GetPerith(), SAY_WARN_17);
					Next(10s);
					break;
				case 47:
					Talk(GetPerith(), SAY_WARN_18);
					Next(11s);
					break;
				case 48:
					Talk(GetJaina(), SAY_WARN_19);
					Next(7s);
					break;
				case 49:
					Talk(GetPerith(), SAY_WARN_20);
					Next(5s);
					break;
				case 50:
					Talk(GetJaina(), SAY_WARN_21);
					Next(1s);
					break;
				case 51:
					Talk(GetPerith(), SAY_WARN_22);
					Next(15s);
					break;
				case 52:
					Talk(GetPerith(), SAY_WARN_23);
					Next(9s);
					break;
				case 53:
					Talk(GetJaina(), SAY_WARN_24);
					Next(14s);
					break;
				case 54:
					Talk(GetPerith(), SAY_WARN_25);
					Next(16s);
					break;
				case 55:
					Talk(GetJaina(), SAY_WARN_26);
					Next(5s);
					break;
				case 56:
					if (Creature* jaina = GetJaina())
					{
						jaina->SetTarget(ObjectGuid::Empty);
						jaina->SetFacingTo(3.33f);
					}
					Next(2s);
					break;
				case 57:
                    if (Creature* jaina = GetJaina())
                    {
                        Talk(jaina, SAY_WARN_27);
                        jaina->CastSpell(jaina, SPELL_MAGIC_QUILL);
                    }
					Next(10s);
					break;
				case 58:
					if (Creature* jaina = GetJaina())
					{
						jaina->RemoveAurasDueToSpell(SPELL_MAGIC_QUILL);
						jaina->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
						jaina->SetFacingToObject(GetPerith());
					}
					Next(2s);
					break;
				case 59:
					Talk(GetJaina(), SAY_WARN_28);
					Next(5s);
					break;
				case 60:
					Talk(GetPerith(), SAY_WARN_29);
					Next(5s);
					break;
				case 61:
					Talk(GetPerith(), SAY_WARN_30);
					Next(10s);
					break;
				case 62:
					Talk(GetJaina(), SAY_WARN_31);
					Next(4s);
					break;
				case 63:
					Talk(GetPerith(), SAY_WARN_32);
					Next(4s);
					break;
				case 64:
					Talk(GetJaina(), SAY_WARN_33);
					Next(4s);
					break;
				case 65:
					Talk(GetPerith(), SAY_WARN_34);
					Next(3s);
					break;
				case 66:
					ClearTarget();
					GetPained()->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					if (Creature* perith = GetPerith())
					{
						perith->GetMotionMaster()->MovePath(OfficerPath01, false);
						perith->DespawnOrUnsummon(15s);
					}
					Next(5s);
					break;
				case 67:
					Talk(GetJaina(), SAY_WARN_35);
					GetJaina()->SetFacingToObject(GetPained());
					GetPained()->SetFacingToObject(GetJaina());
					Next(7s);
					break;
				case 68:
					Talk(GetPained(), SAY_WARN_36);
					Next(3s);
					break;
				case 69:
					Talk(GetJaina(), SAY_WARN_37);
					Next(3s);
					break;
				case 70:
					GetJaina()->SetFacingTo(0.39f);
					GetPained()->GetMotionMaster()->MovePath(KinndyPath01, false);
					break;

				#pragma endregion

				// A Little Help
				#pragma region A_LITTLE_HELP

				case 71:
					Talk(GetJaina(), SAY_PRE_BATTLE_2);
					EnsurePlayerHaveShaker();
					HordeMembersInvoker(DATA_DECORATION_ENTRANCE, true);
					if (Creature* hedric = GetHedric())
					{
						hedric->SetVisible(true);
						hedric->PlayDirectSound(SOUND_FEARFUL_CROWD);
						hedric->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					}
					Next(3s);
					break;
				case 72:
					if (Creature* hedric = GetHedric())
					{
						Talk(hedric, SAY_PRE_BATTLE_1);
						hedric->GetMotionMaster()->MovePath(HedricPath01, false);
					}
					Next(2s);
					break;
				case 73:
					Talk(GetHedric(), SAY_PRE_BATTLE_3);
					Next(3s);
					break;
				case 74:
					Talk(GetJaina(), SAY_PRE_BATTLE_4);
					Next(2s);
					break;
				case 75:
					GetJaina()->SummonGameObject(GOB_PORTAL_TO_DALARAN, PortalPoint01, QuaternionData::QuaternionData(), 0s);
					if (Creature* hedric = GetHedric())
						hedric->SetFacingTo(0.461802f);
					Next(500ms);
					break;
				case 76:
					if (Creature* hedric = GetHedric())
					{
						hedric->SetWalk(true);
						hedric->GetMotionMaster()->MoveBackward(MOVEMENT_INFO_POINT_01, HedricPoint02, nullptr, 1.4f);
					}
					Next(500ms);
					break;
				case 77:
					if (archmagesIndex >= ARCHMAGES_LOCATION)
						Next(2s);
					else if (Creature* creature = instance->SummonCreature(archmagesLocation[archmagesIndex].dataId, PortalPoint01))
					{
						creature->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
						creature->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
						creature->SetSheath(SHEATH_STATE_UNARMED);
						creature->CastSpell(creature, SPELL_TELEPORT_DUMMY);
						creature->SetWalk(true);
						creature->GetMotionMaster()->Clear();
						creature->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, archmagesLocation[archmagesIndex].destination, false, archmagesLocation[archmagesIndex].destination.GetOrientation());
						archmagesIndex++;
						events.Repeat(800ms, 1s);
					}
					break;
				case 78:
					Talk(GetRhonin(), SAY_PRE_BATTLE_5);
					SetTarget(GetRhonin());
					ClosePortal(DATA_PORTAL_TO_DALARAN);
					Next(2800ms);
					break;
				case 79:
					Talk(GetJaina(), SAY_PRE_BATTLE_6);
					SetTarget(GetJaina());
					Next(11s);
					break;
				case 80:
					Talk(GetJaina(), SAY_PRE_BATTLE_7);
					Next(9s);
					break;
				case 81:
					Talk(GetThalen(), SAY_PRE_BATTLE_8);
					SetTarget(GetThalen());
					Next(7s);
					break;
				case 82:
					Talk(GetJaina(), SAY_PRE_BATTLE_9);
					SetTarget(GetJaina());
					Next(7s);
					break;
				case 83:
					Talk(GetJaina(), SAY_PRE_BATTLE_10);
					Next(6s);
					break;
				case 84:
					Talk(GetRhonin(), SAY_PRE_BATTLE_11);
					SetTarget(GetRhonin());
					Next(2s);
					break;
				case 85:
					Talk(GetJaina(), SAY_PRE_BATTLE_12);
					SetTarget(GetTervosh());
					Next(6s);
					break;
				case 86:
					Talk(GetVereesa(), SAY_PRE_BATTLE_13);
					SetTarget(GetVereesa());
					Next(10s);
					break;
				case 87:
					if (Creature* jaina = GetJaina())
					{
						Talk(jaina, SAY_PRE_BATTLE_14);
						SetTarget(jaina);
						jaina->SetTarget(ObjectGuid::Empty);
						jaina->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					}
					Next(8s);
					break;
				case 88:
					if (Creature* jaina = GetJaina())
					{
						Talk(jaina, SAY_PRE_BATTLE_15);
						if (Player* player = instance->GetPlayers().begin()->GetSource())
							SetTarget(player);
					}
					if (Creature* vereesa = GetVereesa())
					{
                        vereesa->SetWalk(true);
                        vereesa->CastSpell(vereesa, SPELL_VANISH);
                        vereesa->GetMotionMaster()->MovePoint(0, VereesaPoint01);
						vereesa->SetFaction(FACTION_FRIENDLY);
					}
					Next(5s);
					break;
				case 89:
					ClearTarget();
					GetJaina()->CastSpell(GetJaina(), SPELL_MASS_TELEPORT);
                    GetVereesa()->SetVisible(false);
					Next(4600ms);
					break;
				case 90:
					EnsureBarrierHaveDamage();
					if (Creature* kalecgos = GetKalecgos())
					{
						kalecgos->setActive(true);
						kalecgos->SetVisible(true);
						kalecgos->SetSpeed(MOVE_RUN, 25.f);
						kalecgos->AI()->SetData(DATA_KALECGOS_CIRCLE_EVENT, 0U);
					}
					for (uint8 i = 0; i < ACTORS_RELOCATION; i++)
					{
						if (Creature* creature = GetCreature(actorsRelocation[i].dataId))
						{
							creature->RemoveAllAuras();
							creature->SetTarget(ObjectGuid::Empty);
							creature->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
							creature->SetSheath(SHEATH_STATE_MELEE);
							creature->GetMotionMaster()->Clear();
							creature->GetMotionMaster()->MoveIdle();
							creature->NearTeleportTo(actorsRelocation[i].destination);
							creature->SetHomePosition(actorsRelocation[i].destination);

							switch (creature->GetEntry())
							{
								case NPC_AMARA_LEESON:
									creature->CastSpell(creature, SPELL_PORTAL_CHANNELING_03);
									break;
								case NPC_RHONIN:
									creature->SetVignette(VIGNETTE_RHONIN);
									creature->CastSpell(creature, SPELL_CHAT_BUBBLE, true);
									creature->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
									break;
								case NPC_THADER_WINDERMERE:
									creature->SetVignette(VIGNETTE_THADER_WINDERMERE);
									creature->CastSpell(creature, SPELL_CHAT_BUBBLE, true);
									creature->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
									break;
								case NPC_THALEN_SONGWEAVER:
									creature->CastSpell(creature, SPELL_PORTAL_CHANNELING_02);
									break;
								case NPC_JAINA_PROUDMOORE:
									GetBarrier01()->UseDoorOrButton();
									TeleportPlayers(GetJaina(), actorsRelocation[i].destination, 15.0f);
									break;
								case NPC_KALECGOS:
									creature->SetVisible(false);
									break;
							}
						}
					}
					break;

				#pragma endregion

				// The Battle
				#pragma region THE_BATTLE

				case 91:
					Talk(GetJaina(), SAY_BATTLE_02);
					events.ScheduleEvent(93, 10s);
					break;
				// DELETED
				//case 92:
				//    break;
				case 93:
					if (Creature* thalen = GetThalen())
					{
						thalen->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
						thalen->SetReactState(REACT_PASSIVE);
						thalen->SetFaction(FACTION_ENEMY);
						thalen->RemoveAllAuras();
						thalen->CastSpell(thalen, SPELL_BLAZING_BARRIER);

						if (Creature* trigger = thalen->SummonCreature(WORLD_TRIGGER, ExplodingPoint01, TEMPSUMMON_TIMED_DESPAWN, 5s))
							trigger->CastSpell(trigger, SPELL_METEOR);
					}
					Next(1s);
					break;
				case 94:
					if (GameObject* barrier = GetBarrier01())
					{
						scheduler.CancelGroup((uint32)BFTPhases::TheBattle);
						barrier->ResetDoorOrButton();
						if (Creature* trigger = barrier->SummonCreature(WORLD_TRIGGER, ExplodingPoint01, TEMPSUMMON_TIMED_DESPAWN, 5s))
						{
							trigger->SetFaction(FACTION_MONSTER);
							trigger->CastSpell(trigger, SPELL_BIG_EXPLOSION);
							trigger->CastSpell(trigger, SPELL_SCORCHED_EARTH, true);
						}
					}
					Next(1s);
					break;
				case 95:
					if (Creature* amara = GetAmara())
					{
						amara->RemoveAllAuras();
						amara->SetEmoteState(EMOTE_STATE_READY2HL_ALLOW_MOVEMENT);
						amara->CastSpell(amara, SPELL_PRISMATIC_BARRIER, true);
					}
					GetThalen()->HandleEmoteCommand(EMOTE_ONESHOT_LAUGH);
					GetJaina()->SetEmoteState(EMOTE_STATE_READY2HL_ALLOW_MOVEMENT);
					GetHedric()->SetEmoteState(EMOTE_STATE_READY1H_ALLOW_MOVEMENT);
					Next(1s);
					break;
				case 96:
					Talk(GetJaina(), SAY_BATTLE_03);
					if (Creature* thalen = GetThalen())
					{
						thalen->SetWalk(false);
						thalen->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, ThalenPoint01, false, ThalenPoint01.GetOrientation());
					}
					Next(2s);
					break;
				case 97:
					if (Creature* jaina = GetJaina())
					{
						jaina->CastSpell(JainaPoint04, SPELL_TELEPORT);
						Talk(jaina, SAY_BATTLE_04);
					}
					Next(1s);
					break;
				case 98:
					if (Creature* thalen = GetThalen())
					{
						thalen->CastSpell(thalen, SPELL_ICY_GLARE);
						thalen->CastSpell(thalen, SPELL_CHILLING_BLAST, true);
						thalen->StopMoving();
					}
					Next(2s);
					break;
				case 99:
					if (Creature* thalen = GetThalen())
					{
						thalen->RemoveAurasDueToSpell(SPELL_BLAZING_BARRIER);
						thalen->CastSpell(thalen, SPELL_DISSOLVE);
					}
					if (Creature* thader = GetCreature(DATA_THADER_WINDERMERE))
					{
						Talk(thader, SAY_BATTLE_05);
						thader->RemoveAllAuras();
						thader->SetRegenerateHealth(false);
						thader->SetReactState(REACT_PASSIVE);
						thader->SetStandState(UNIT_STAND_STATE_KNEEL);
						thader->SetHealth(thader->CountPctFromMaxHealth(5));
						thader->CastSpell(thader, SPELL_ARCANE_FX);

						if (Creature* kinndy = GetCreature(DATA_KINNDY_SPARKSHINE))
						{
							kinndy->RemoveAllAuras();
							kinndy->SetReactState(REACT_PASSIVE);
							kinndy->CastSpell(kinndy, SPELL_CHANNEL_BLUE_MOVING);
							kinndy->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_NONE, thader, 0.8f);
						}
					}
					GetBarrier02()->ResetDoorOrButton();
					Next(3s);
					break;
				case 100:
					HordeMembersInvoker(DATA_WAVE_BOAT);
					if (Creature* thalen = GetThalen())
					{
						thalen->RemoveAllAuras();
						thalen->NearTeleportTo(ThalenPoint02);
						thalen->SetHomePosition(ThalenPoint02);
						thalen->CastSpell(thalen, SPELL_ARCANIC_CELL, true);
						thalen->SetEmoteState(EMOTE_STATE_STUN_NO_SHEATHE);
					}
					if (Creature* kalecgos = GetKalecgos())
					{
						kalecgos->SetVisible(true);
						kalecgos->AI()->SetData(DATA_KALECGOS_COMBAT_EVENT, 0U);
					}
					GetJaina()->CastSpell(actorsRelocation[0].destination, SPELL_TELEPORT);
                    TriggerGameEvent(EVENT_MAINTAIN_THE_PROTECTION);
					break;

				#pragma endregion

                // Waves
                case EVENT_WAVES_CHECKER:
                {
                    // Quand le nombre de membres vivants est inf?rieur ou ?gal au nombre de membres morts
                    uint32 deadCounter = HordeMembersChecker();
                    if (deadCounter >= HORDE_WAVES_COUNT)
                    {
                        HordeMembersInvoker(Waves[waves]);
                        waves++;
                        events.ScheduleEvent(EVENT_WAVES_CHECKER, 2s);
                    }
                    else
                    {
                        events.RescheduleEvent(EVENT_WAVES_CHECKER, 1s);
                    }
                    break;
                }

				// Help the wounded
				#pragma region HELP_THE_WOUNDED

				// PART I
				case 122:
					if (Creature* jaina = GetJaina())
					{
						if (Creature* hedric = GetHedric())
						{
							jaina->SetTarget(hedric->GetGUID());
							jaina->SetEmoteState(EMOTE_STATE_STAND);

							hedric->SetTarget(jaina->GetGUID());
							hedric->SetEmoteState(EMOTE_STATE_STAND);
						}
					}
					Next(800ms);
					break;
				case 123:
					Talk(GetJaina(), SAY_POST_BATTLE_01);
					Next(2s);
					break;
				case 124:
					Talk(GetHedric(), SAY_POST_BATTLE_02);
					Next(4s);
					break;
				case 125:
					Talk(GetJaina(), SAY_POST_BATTLE_03);
					Next(4s);
					break;
				case 126:
					ClearTarget();
					if (Creature* jaina = GetJaina())
					{
						jaina->GetMotionMaster()->MovePath(JainaPath01, false);
						jaina->SetHomePosition(JainaPoint06);
					}
					Next(1500ms);
					break;
				case 127:
					if (Creature* hedric = GetHedric())
						hedric->GetMotionMaster()->MovePath(HedricPath02, false);
					break;

				// PART II
				case 128:
					if (Creature* jaina = GetJaina())
					{
						if (Creature* kinndy = GetKinndy())
						{
							jaina->SetTarget(kinndy->GetGUID());
							kinndy->SetTarget(jaina->GetGUID());
						}
					}
					Next(800ms);
					break;
				case 129:
					Talk(GetKinndy(), SAY_POST_BATTLE_04);
					Next(5s);
					break;
				case 130:
					Talk(GetJaina(), SAY_POST_BATTLE_05);
					Next(7s);
					break;
				case 131:
					if (Creature* kinndy = GetKinndy())
					{
						Talk(kinndy, SAY_POST_BATTLE_06);
						kinndy->SetEmoteState(EMOTE_STATE_NONE);
					}
					Next(4s);
					break;
				case 132:
					Talk(GetJaina(), SAY_POST_BATTLE_07);
					Next(4s);
					break;
				case 133:
					Talk(GetKinndy(), SAY_POST_BATTLE_08);
					Next(3s);
					break;
				case 134:
					Talk(GetJaina(), SAY_POST_BATTLE_09);
					Next(13s);
					break;
				case 135:
					Talk(GetJaina(), SAY_POST_BATTLE_10);
					Next(7s);
					break;
				case 136:
					Talk(GetKinndy(), SAY_POST_BATTLE_11);
					Next(8s);
					break;
				case 137:
					Talk(GetJaina(), SAY_POST_BATTLE_12);
					Next(6s);
					break;
				case 138:
					Talk(GetJaina(), SAY_POST_BATTLE_13);
					Next(12s);
					break;
				case 139:
					Talk(GetKinndy(), SAY_POST_BATTLE_14);
					Next(3s);
					break;
				case 140:
					Talk(GetJaina(), SAY_POST_BATTLE_15);
					Next(3s);
					break;
				case 141:
					ClearTarget();
					if (Creature* jaina = GetJaina())
					{
						jaina->SetSpeedRate(MOVE_RUN, 0.85f);

						if (Creature* kinndy = GetKinndy())
						{
							jaina->SetFacingTo(3.15f);
							kinndy->SetFacingTo(2.73f);
						}
					}
					break;

				#pragma endregion

				// Wait for Archmage Leeson returns
				#pragma region WAIT_FOR_AMARA

				// Part I
				case 142:
					GetKalec()->GetMotionMaster()->MovePath(KalecPath02, false, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
					Next(8s);
					break;
				case 143:
					SetTarget(GetKalec());
					Talk(GetKalec(), SAY_IRIS_WARN_01);
					Next(1s);
					break;
				case 144:
					SetTarget(GetJaina());
					Talk(GetJaina(), SAY_IRIS_WARN_02);
					Next(2s);
					break;
				case 145:
					SetTarget(GetKalec());
					Talk(GetKalec(), SAY_IRIS_WARN_03);
					Next(4s);
					break;
				case 146:
					SetTarget(GetJaina());
					Talk(GetJaina(), SAY_IRIS_WARN_04);
					Next(8s);
					break;
				case 147:
					SetTarget(GetKalec());
					Talk(GetKalec(), SAY_IRIS_WARN_05);
					Next(2s);
					break;
				case 148:
					SetTarget(GetJaina());
					Talk(GetJaina(), SAY_IRIS_WARN_06);
					Next(8s);
					break;
				case 149:
					Talk(GetJaina(), SAY_IRIS_WARN_07);
					Next(2s);
					break;
				case 150:
					SetTarget(GetKalec());
					Talk(GetKalec(), SAY_IRIS_WARN_08);
					Next(7s);
					break;
				case 151:
					SetTarget(GetRhonin());
					Talk(GetRhonin(), SAY_IRIS_WARN_09);
					Next(5s);
					break;
				case 152:
					ClearTarget();
					GetKalec()->GetMotionMaster()->MovePath(KalecPath03, false, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
					Next(3s);
					break;
				case 153:
                    GetRhonin()->GetMotionMaster()->MovePath(RhoninPath01, false, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
					Next(3s);
					break;
				case 154:
                    GetAmara()->GetMotionMaster()->MovePath(AmaraPath01, false, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
					Next(10s);
					break;
				case 155:
					if (Creature* amara = GetAmara())
					{
						amara->SetVisible(true);
						amara->GetMotionMaster()->MovePath(KalecPath02, false, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
					}
					break;

				// Part II
				case 156:
					if (Creature* jaina = GetJaina())
					{
						if (Creature* amara = GetAmara())
						{
							jaina->SetTarget(amara->GetGUID());
							amara->SetTarget(jaina->GetGUID());
						}
					}
					Next(1s);
					break;
				case 157:
					Talk(GetAmara(), SAY_IRIS_WARN_10);
					Next(6s);
					break;
				case 158:
					Talk(GetJaina(), SAY_IRIS_WARN_11);
					Next(4s);
					break;
				case 159:
					ClearTarget();
					GetAmara()->GetMotionMaster()->MovePath(KalecPath03, false, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
					Next(4s);
					break;
				case 160:
					GetJaina()->GetMotionMaster()->MovePath(JainaPath02, false, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
					break;

				#pragma endregion

				// Retrieve Rhonin
				#pragma region RETRIEVE_RHONIN

				case 161:
					if (Creature* jaina = GetJaina())
					{
						Talk(jaina, SAY_IRIS_XPLOSION_01);
						jaina->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					}
					Next(3s);
					break;
				case 162:
					if (Creature* jaina = GetJaina())
					{
						jaina->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
						jaina->SetFacingToObject(GetRhonin());
						jaina->RemoveAllAuras();

						if (Creature* rhonin = GetRhonin())
						{
							jaina->SetTarget(rhonin->GetGUID());

							rhonin->SetTarget(jaina->GetGUID());
							rhonin->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
						}
					}
					Next(3s);
					break;
				case 163:
					Talk(GetRhonin(), SAY_IRIS_XPLOSION_02);
					Next(4s);
					break;
				case 164:
					Talk(GetJaina(), SAY_IRIS_XPLOSION_03);
					Next(6s);
					break;
				case 165:
					Talk(GetRhonin(), SAY_IRIS_XPLOSION_04);
					Next(5s);
					break;
				case 166:
					Talk(GetRhonin(), SAY_IRIS_XPLOSION_05);
					Next(8s);
					break;
				case 167:
					Talk(GetRhonin(), SAY_IRIS_XPLOSION_06);
					Next(6s);
					break;
				case 168:
					Talk(GetJaina(), SAY_IRIS_XPLOSION_07);
					Next(6s);
					break;
				case 169:
					Talk(GetRhonin(), SAY_IRIS_XPLOSION_08);
					Next(3s);
					break;
				case 170:
					Talk(GetJaina(), SAY_IRIS_XPLOSION_09);
					Next(5s);
					break;
				case 171:
					if (Creature* rhonin = GetRhonin())
					{
						Talk(rhonin, SAY_IRIS_XPLOSION_10);
						rhonin->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
						rhonin->RemoveAllAuras();
					}
					Next(12s);
					break;
				case 172:
					TriggerGameEvent(EVENT_REDUCE_IMPACT);
					break;

				#pragma endregion
			}
		}

		EventMap events;
		TaskScheduler scheduler;
		BFTPhases phase;
		uint32 eventId;
		uint32 woundedTroops;
		uint8 archmagesIndex;
		uint8 waves;
		GuidVector citizens;
		GuidVector civilians;
		GuidVector tanks;
		GuidVector troops;
		GuidVector hordeMembers;
		GuidVector dummyMembers;

		// Accesseurs
		#pragma region ACCESSORS

		Creature* GetJaina()        { return GetCreature(DATA_JAINA_PROUDMOORE); }
		Creature* GetKinndy()       { return GetCreature(DATA_KINNDY_SPARKSHINE); }
		Creature* GetTervosh()      { return GetCreature(DATA_ARCHMAGE_TERVOSH); }
		Creature* GetKalec()        { return GetCreature(DATA_KALECGOS); }
		Creature* GetKalecgos()     { return GetCreature(DATA_KALECGOS_DRAGON); }
		Creature* GetPained()       { return GetCreature(DATA_PAINED); }
		Creature* GetPerith()       { return GetCreature(DATA_PERITH_STORMHOOVE); }
		Creature* GetKnight()       { return GetCreature(DATA_KNIGHT_OF_THERAMORE); }
		Creature* GetHedric()       { return GetCreature(DATA_HEDRIC_EVENCANE); }
		Creature* GetRhonin()       { return GetCreature(DATA_RHONIN); }
		Creature* GetVereesa()      { return GetCreature(DATA_VEREESA_WINDRUNNER); }
		Creature* GetThalen()       { return GetCreature(DATA_THALEN_SONGWEAVER); }
		Creature* GetAmara()        { return GetCreature(DATA_AMARA_LEESON); }
		Creature* GetDrok()         { return GetCreature(DATA_CAPTAIN_DROK); }
		Creature* GetGruhta()       { return GetCreature(DATA_WAVE_CALLER_GRUHTA); }

		GameObject* GetBarrier01()  { return GetGameObject(DATA_MYSTIC_BARRIER_01); }
		GameObject* GetBarrier02()  { return GetGameObject(DATA_MYSTIC_BARRIER_02); }

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

		void SetTarget(Unit* unit)
		{
			ObjectGuid guid = unit->GetGUID();
			for (uint8 i = 0; i < eventCreatureDataCount; i++)
			{
				if (Creature* creature = GetCreature(creatureData[i].type))
				{
					if (creature->IsTrigger())
						continue;

					if (creature->GetGUID() == guid)
						continue;

					if (creature->GetEntry() == NPC_HEDRIC_EVENCANE
						&& phase < BFTPhases::Preparation)
					{
						continue;
					}

					creature->SetTarget(guid);
				}
			}
		}

		void ClearTarget()
		{
			for (uint8 i = 0; i < eventCreatureDataCount; i++)
			{
				if (Creature* creature = GetCreature(creatureData[i].type))
					creature->SetTarget(ObjectGuid::Empty);
			}
		}

		void ClosePortal(uint32 dataId)
		{
			if (GameObject* portal = GetGameObject(dataId))
			{
				portal->Delete();

				CastSpellExtraArgs args;
				args.SetTriggerFlags(TRIGGERED_CAST_DIRECTLY);

				const Position pos = portal->GetPosition();
				if (Creature* special = portal->SummonTrigger(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), pos.GetOrientation(), 5s))
				{
					special->CastSpell(special, SPELL_CLOSE_PORTAL, args);
				}
			}
		}

		void TeleportPlayers(Creature* caster, const Position center, float minDist)
		{
			Position pos = caster->GetRandomPoint(center, 8.f);
			pos.m_positionZ += 3.0f;

			instance->DoOnPlayers([caster, center, minDist, pos](Player* player)
			{
				if (player->IsWithinDist(caster, minDist))
				{
					player->NearTeleportTo(pos);
				}
			});
		}

		void HordeMembersInvoker(uint32 waveId, bool dummies = false)
		{
			std::list<TempSummon*> members;

			hordeMembers.clear();

			instance->SummonCreatureGroup(waveId, &members);
			for (TempSummon* horde : members)
			{
				horde->SetRegenerateHealth(false);

				if (Unit* target = SelectNearestHostileInRange(horde))
					horde->AI()->AttackStart(target);

				hordeMembers.push_back(horde->GetGUID());

				if (dummies)
				{
					horde->SetImmuneToAll(true);

					switch (horde->GetClass())
					{
						case UNIT_CLASS_PALADIN:
							horde->SetEmoteState(EMOTE_STATE_READY2H);
							break;
						case UNIT_CLASS_MAGE:
							horde->SetEmoteState(RAND(EMOTE_STATE_READY1H, EMOTE_STATE_READY2HL));
							break;
						case UNIT_CLASS_ROGUE:
							break;
						default:
							horde->SetEmoteState(EMOTE_STATE_READY1H);
							break;
					}

					enum Spells
					{
						SPELL_CHANNEL_WATER     = 237594,
						SPELL_CHANNEL_FROST     = 1271695
					};

					switch (horde->GetEntry())
					{
                        case NPC_PORTAL_TO_ORGRIMMAR:
                            horde->SetUninteractible(true);
                            break;
						case NPC_ROKNAH_LOA_SINGER:
							horde->CastSpell(horde, SPELL_CHANNEL_WATER);
							break;
						case NPC_ROKNAH_HAG:
							horde->CastSpell(horde, SPELL_CHANNEL_FROST);
							break;
						case NPC_HORDE_BOMBARDIER:
							horde->SetWalk(false);
							horde->SetCanFly(true);
							horde->SetDisableGravity(true);
							horde->GetMotionMaster()->MoveRandom(20.0f);
							horde->SetSpeedRate(MOVE_RUN, 2.f);
							horde->SetSpeedRate(MOVE_FLIGHT, 2.f);
							horde->m_Events.AddEvent(new HordeBombardierThrowBomb(horde),
                                                     horde->m_Events.CalculateTime(Seconds(urand(2, 8))));
							break;
                        case NPC_HORDE_DEMOLISHER:
                            if (waveId == DATA_DECORATION_WEST)
                                horde->m_Events.AddEvent(new HordeDemolisherThrowBoulder(horde),
                                                        horde->m_Events.CalculateTime(Seconds(urand(2, 8))));
                            break;
                    }

					dummyMembers.push_back(horde->GetGUID());
				}
			}

			if (Creature* jaina = GetJaina())
				jaina->AI()->DoAction(waveId);
		}

        uint32 HordeMembersChecker()
        {
            uint32 deadCounter = 0;
            if (Creature* jaina = GetJaina())
            {
                for (uint8 i = 0; i < HORDE_WAVES_COUNT; ++i)
                {
                    Creature* temp = ObjectAccessor::GetCreature(*jaina, hordeMembers[i]);
                    if (!temp || temp->isDead())
                        ++deadCounter;
                }
            }

            return deadCounter;
        }

		void MassDespawn(uint32 entry)
		{
			std::list<Creature*> results;
			if (Creature* jaina = GetJaina())
			{
				jaina->GetCreatureListWithEntryInGrid(results, entry, SIZE_OF_GRIDS);
				if (results.empty())
					return;

				for (Creature* c : results)
					c->DespawnOrUnsummon();
			}
		}

		void DespawnDummies()
		{
			for (ObjectGuid guid : dummyMembers)
			{
				if (Creature* creature = instance->GetCreature(guid))
					creature->DespawnOrUnsummon();
			}
		}

		void SpawnWoundedTroops()
		{
			Creature* jaina = GetJaina();
			if (!jaina)
				return;

			for (ObjectGuid guid : troops)
			{
				Creature* troop = ObjectAccessor::GetCreature(*jaina, guid);

				if (!troop || troop->isDead())
					continue;

				if (roll_chance(80))
				{
					troop->SetVisible(false);
					if (Creature* wounded = troop->SummonCreature(NPC_THERAMORE_WOUNDED_TROOP,
                                                                  troop->GetPosition(),
                                                                  TempSummonType::TEMPSUMMON_MANUAL_DESPAWN))
					{
						uint32 health = troop->GetMaxHealth();
						Powers power = troop->GetPowerType();

						wounded->SetPowerType(power);
						wounded->SetPower(power, troop->GetPower(power));
						wounded->SetRegenerateHealth(false);
						wounded->SetMaxHealth(health);
						wounded->SetHealth(health * frand(0.15f, 0.20f));
						wounded->SetDisplayId(troop->GetDisplayId());
						wounded->SetImmuneToNPC(true);
                        wounded->AddAura(SPELL_COSMETIC_DEATH, wounded);
                        wounded->AddAura(SPELL_COSMETIC_FREEZE, wounded);
						wounded->SetVignette(VIGNETTE_ALLIANCE_TROOPS);
					}
				}
			}
		}

		void RelocateTroops()
		{
			SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, GetRhonin());

			if (Creature* jaina = GetJaina())
			{
				SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, jaina);
				jaina->CombatStop();
				jaina->SetReactState(REACT_PASSIVE);
				jaina->NearTeleportTo(JainaPoint03);
				jaina->SetHomePosition(JainaPoint03);
				jaina->SetSheath(SHEATH_STATE_UNARMED);
			}

			if (GameObject* portal = GetGameObject(DATA_PORTAL_TO_ORGRIMMAR))
				portal->Delete();

			if (Creature* kalecgos = GetKalecgos())
			{
				SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, kalecgos);
				kalecgos->SetVisible(false);
			}

			uint8 counter = 0;
			for (uint8 i = 0; i < troops.size(); i++)
			{
				Creature* creature = instance->GetCreature(troops[i]);

				if (!creature || creature->isDead())
					continue;

				counter++;
				if (counter >= ARCHMAGES_RELOCATION)
					break;

				creature->SetVisible(true);
				creature->NearTeleportTo(UnitLocation[i]);
				creature->SetHomePosition(UnitLocation[i]);
				creature->SetSheath(SHEATH_STATE_UNARMED);
				creature->SetStandState(UNIT_STAND_STATE_SIT);
				creature->SetEmoteState(EMOTE_STATE_NONE);
				creature->RemoveAllAuras();
				creature->Dismount();
				creature->AddAura(RAND(SPELL_COSMETIC_EAT_SOUP, SPELL_COSMETIC_DRINK), creature);
			}

			for (uint8 i = 0; i < ARCHMAGES_RELOCATION; i++)
			{
				if (Creature* creature = GetCreature(archmagesRelocation[i].dataId))
				{
					creature->NearTeleportTo(archmagesRelocation[i].destination);
					creature->SetHomePosition(archmagesRelocation[i].destination);
					creature->SetSheath(SHEATH_STATE_UNARMED);
					creature->SetEmoteState(EMOTE_STATE_NONE);
					creature->RemoveAllAuras();

					switch (creature->GetEntry())
					{
						case NPC_ARCHMAGE_TERVOSH:
							creature->CastSpell(creature, SPELL_SHOW_OFF_FIRE);
							break;
						case NPC_THADER_WINDERMERE:
							creature->AddAura(SPELL_STASIS, creature);
							creature->SetStandState(UNIT_STAND_STATE_STAND);
							creature->SetEmoteState(EMOTE_STATE_STUN_NO_SHEATHE);
							break;
						case NPC_TARI_COGG:
							creature->SetStandState(UNIT_STAND_STATE_SIT);
							creature->SetEmoteState(EMOTE_STATE_EAT);
							creature->SummonGameObject(GOB_LAVISH_REFRESHMENT_TABLE, TablePoint01, QuaternionData::fromEulerAnglesZYX(TablePoint01.GetOrientation(), 0.f, 0.f), 0s);
							break;
						case NPC_KINNDY_SPARKSHINE:
							creature->SetEmoteState(EMOTE_STATE_CRY);
							break;
					}
				}
			}
		}

		void EnsurePlayersAreInPhase(uint32 phaseId)
		{
			instance->DoOnPlayers([phaseId](Player* player)
			{
				PhasingHandler::AddPhase(player, phaseId, true);
			});
		}

		void EnsurePlayersHaveAura(uint32 entry)
		{
			instance->DoOnPlayers([entry](Player* player)
			{
				if (!player->HasAura(entry))
				{
					player->CastSpell(player, entry, true);
				}
			});
		}

		void EnsurePlayerHaveShield()
		{
			scheduler.Schedule(2s, [this](TaskContext shield)
			{
				if (phase >= BFTPhases::Preparation_Rhonin && phase < BFTPhases::HelpTheWounded)
				{
					EnsurePlayersHaveAura(SPELL_RUNIC_SHIELD);
					shield.Repeat(1s);
				}
			});
		}

		void EnsurePlayerHaveBucket()
		{
			scheduler.Schedule(2s, [this](TaskContext bucket)
			{
				if (phase >= BFTPhases::HelpTheWounded && phase < BFTPhases::HelpTheWounded_Extinguish)
				{
					EnsurePlayersHaveAura(SPELL_WATER_BUCKET);
					bucket.Repeat(1s);
				}
			});
		}

		void EnsurePlayerHaveShaker()
		{
			scheduler.Schedule(1s, [this](TaskContext shield)
			{
				if (phase >= BFTPhases::Preparation && phase < BFTPhases::HelpTheWounded)
				{
					DoCastSpellOnPlayers(SPELL_CAMERA_SHAKE_VOLCANO);
				}

				shield.Repeat(15s, 30s);
			});
		}

		void EnsureBarrierHaveDamage()
		{
			scheduler.Schedule(1s, (uint32)BFTPhases::TheBattle, [this](TaskContext explosion)
			{
				if (Creature* thalen = GetThalen())
				{
					if (Creature* trigger = thalen->SummonCreature(WORLD_TRIGGER, ExplodingPoint01, TEMPSUMMON_TIMED_DESPAWN, 2s))
						trigger->CastSpell(trigger, SPELL_BIG_EXPLOSION);
					explosion.Repeat(2s, 5s);
				}
			});
		}

		void ForceWeather(uint32 weatherEntry, bool apply)
		{
			instance->DoOnPlayers([weatherEntry, apply](Player* player)
			{
				if (apply)
					player->SendDirectMessage(WorldPackets::Misc::Weather(WeatherState(weatherEntry), 1.0f).Write());
				else
					player->GetMap()->SendZoneWeather(player->GetZoneId(), player);
			});
		}

		Unit* SelectNearestHostileInRange(Creature* creature) const
		{
			Unit* target = nullptr;
			Trinity::NearestHostileUnitInAggroRangeCheck check(creature, false, true);
			Trinity::UnitSearcher<Trinity::NearestHostileUnitInAggroRangeCheck> searcher(creature, target, check);
			Cell::VisitGridObjects(creature, searcher, MAX_VISIBILITY_DISTANCE);
			return target;
		}

		#pragma endregion
	};

	InstanceScript* GetInstanceScript(InstanceMap* map) const override
	{
		return new scenario_battle_for_theramore_InstanceScript(map);
	}
};

class scene_theramore_explosion : public SceneScript
{
	public:
		scene_theramore_explosion() : SceneScript("scene_theramore_explosion") { }

	enum Misc
	{
		MAP_THERAMORE_RUINS     = 5001,
		SPELL_DROP_BOMBE        = 128438
	};

	const Position Center = { -3002.74f, -4342.11f, 6.044930f, 3.76716f };
	const Position BombPosition = { -3819.17f, -4350.76f, 270.0f, 0.0f };

	const float Distance = 8.f;

	void OnSceneTriggerEvent(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/, std::string const& triggerName) override
	{
		if (triggerName == "DropBombServer")
		{
			if (Creature* bombModel = player->SummonCreature(WORLD_TRIGGER, BombPosition))
				bombModel->CastSpell(bombModel, SPELL_DROP_BOMBE);
		}
	}

	void OnSceneStart(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/) override
	{
		player->SetControlled(true, UNIT_STATE_ROOT);
	}

	void OnSceneComplete(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/) override
	{
		Finish(player);
	}

	void OnSceneCancel(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/) override
	{
		Finish(player);
	}

	void Finish(Player* player)
	{
		player->SetControlled(false, UNIT_STATE_ROOT);
		player->TeleportTo(GetRandomPosition(), TELE_REVIVE_AT_TELEPORT);
	}

	WorldLocation GetRandomPosition()
	{
		float alpha = 2 * float(M_PI) * float(rand_norm());
		float r = Distance * sqrtf(float(rand_norm()));
		float x = r * cosf(alpha) + Center.GetPositionX();
		float y = r * sinf(alpha) + Center.GetPositionY();
		return { MAP_THERAMORE_RUINS, { x, y, Center.GetPositionZ(), Center.GetOrientation() }};
	}
};

void AddSC_scenario_battle_for_theramore()
{
	new scenario_battle_for_theramore();
	new scene_theramore_explosion();
}
