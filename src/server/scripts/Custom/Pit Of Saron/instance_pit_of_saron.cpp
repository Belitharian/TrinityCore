#include "ScriptMgr.h"
#include "Creature.h"
#include "Containers.h"
#include "InstanceScript.h"
#include "Map.h"
#include "pit_of_saron.h"
#include "TemporarySummon.h"

// positions for Martin Victus
Position const SlaveLeaderPos       = { 689.7158f, -104.8736f, 513.7360f, 0.0f };
// position for Jaina
Position const EventLeaderPos2      = { 1054.368f, 107.14620f, 628.4467f, 0.0f };
// entrance position
Position const EntrancePos          = { 423.9543f, 212.44447f, 529.9114f, 0.0f };
// Tyrannus position away from introduction
Position const TyrannusAfterIntro   = { 843.3142f, 276.0623f, 554.7220f, 0.15f, };

const ObjectData creatureData[] =
{
	// Bosses
	{ NPC_TYRANNUS,             DATA_TYRANNUS               },
	{ NPC_RIMEFANG,             DATA_RIMEFANG               },

	// NPCs
	{ NPC_JAINA_PART1,          DATA_JAINA_1                },
	{ NPC_ELANDRA,              DATA_ELANDRA                },
	{ NPC_KORELN,               DATA_KORELN                 },
	{ NPC_MARTIN_VICTUS_1,      DATA_MARTIN_VICTUS_1        },
	{ NPC_MARTIN_VICTUS_2,      DATA_MARTIN_VICTUS_2        },
	{ 0,                        0                           }   // END
};

const ObjectData gameobjectData[] =
{
	{ GO_ARCANE_BARRIER,        DATA_ARCANE_BARRIER         },
	{ 0,                        0,                          }   // END
};

class instance_pit_of_saron_custom : public InstanceMapScript
{
	public:
	instance_pit_of_saron_custom() : InstanceMapScript(PoSScriptName, 6000) { }

		struct instance_pit_of_saron_InstanceScript : public InstanceScript
		{
			instance_pit_of_saron_InstanceScript(InstanceMap* map) : InstanceScript(map),
                eventId(1), phase(Phases::None), skeletonIndex(0), actorsIndex(0)
			{
				SetHeaders(DataHeader);
				LoadObjectData(creatureData, gameobjectData);
			}

			enum Misc
			{
                // Stages
                STAGE_INTRODUCTION          = 1000,
                STAGE_MARTIN_VICTUS         = 2000,
                STAGE_TYRANNUS              = 3000,

                // Events
                EVENT_COMET_STORM           = 200,
                EVENT_SOLDIERS_RUNNING      = 201,

                // Spells
				SPELL_NECROMANTIC_POWER     = 69347,
				SPELL_RAISE_DEAD            = 69350,
				SPELL_STRANGULATING         = 69413,
                SPELL_COMET_STORM           = 167352,
                SPELL_FROZEN_GROUND         = 387149,
                SPELL_FROST_CHANNELING_DNT  = 458255,
                SPELL_VOID_CHANNELING_DNT   = 458839,
			};

			void SetData(uint32 dataId, uint32 value) override
			{
				if (dataId == DATA_PHASE)
				{
					phase = (Phases)value;

					switch (phase)
					{
						case Phases::Introduction:
                            eventId = STAGE_INTRODUCTION;
							events.ScheduleEvent(STAGE_INTRODUCTION, 500ms);
							break;
                        case Phases::FreeMartinVictus:
                            eventId = STAGE_MARTIN_VICTUS;
                            events.ScheduleEvent(STAGE_MARTIN_VICTUS, 500ms);
                            break;
                        case Phases::TyrannusIntroduction:
                            eventId = STAGE_TYRANNUS;
                            events.ScheduleEvent(STAGE_TYRANNUS, 500ms);
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

			void OnCreatureCreate(Creature* creature) override
			{
				InstanceScript::OnCreatureCreate(creature);

				switch (creature->GetEntry())
				{
					case NPC_TYRANNUS:
					case NPC_RIMEFANG:
                        creature->SetVisibilityDistanceOverride(VisibilityDistanceType::Gigantic);
						creature->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
						break;
					case NPC_JAINA_PART1:
					case NPC_KORELN:
					case NPC_ELANDRA:
					case NPC_CHAMPION_1_ALLIANCE:
					case NPC_CHAMPION_2_ALLIANCE:
					case NPC_CHAMPION_3_ALLIANCE:
						creature->NearTeleportTo(EntrancePos);
						break;
				}
			}

            void OnGameObjectCreate(GameObject* go) override
            {
                InstanceScript::OnGameObjectCreate(go);

                switch (go->GetEntry())
                {
                    case GO_ARCANE_BARRIER:
                        go->SetFlag(GO_FLAG_NOT_SELECTABLE);
                        break;
                }
            }

			void Update(uint32 diff) override
			{
				events.Update(diff);
				switch (eventId = events.ExecuteEvent())
				{
                    // Introduction
                    #pragma region INTRODUCTION

					case STAGE_INTRODUCTION:
					{
						Creature* jaina = GetCreature(DATA_JAINA_1);
						if (!jaina)
							break;

						GetCreatureListWithEntryInGrid(actors, jaina, NPC_JAINA_PART1, 25.0f);
						GetCreatureListWithEntryInGrid(actors, jaina, NPC_KORELN, 25.0f);
						GetCreatureListWithEntryInGrid(actors, jaina, NPC_ELANDRA, 25.0f);
						GetCreatureListWithEntryInGrid(actors, jaina, NPC_CHAMPION_1_ALLIANCE, 25.0f);
						GetCreatureListWithEntryInGrid(actors, jaina, NPC_CHAMPION_2_ALLIANCE, 25.0f);
						GetCreatureListWithEntryInGrid(actors, jaina, NPC_CHAMPION_3_ALLIANCE, 25.0f);

						for (Creature* creature : actors)
						{
							switch (creature->GetEntry())
							{
								case NPC_JAINA_PART1:
								case NPC_KORELN:
								case NPC_ELANDRA:
									creature->SetEmoteState(EMOTE_STATE_READY2HL);
									break;
								default:
                                    creature->SetEmoteState(EMOTE_STATE_READY1H);
                                    creature->SetSpeedRate(MOVE_RUN, 0.95f);
									break;
							}
						}

                        // Launch Comet Storm Event
                        events.ScheduleEvent(EVENT_SOLDIERS_RUNNING, 3ms, 5ms);

						Next(2s);
						break;
					}
					case STAGE_INTRODUCTION + 1:
						Talk(DATA_TYRANNUS, SAY_TYRANNUS_01);
						Next(5s);
						break;
					case STAGE_INTRODUCTION + 2:
						Talk(DATA_TYRANNUS, SAY_TYRANNUS_02);
						Next(13s);
						break;
					case STAGE_INTRODUCTION + 3:
						Talk(DATA_JAINA_1, SAY_JAINA_03);
						Next(3s);
						break;
					case STAGE_INTRODUCTION + 4:
					{
						Creature* jaina = GetCreature(DATA_JAINA_1);
						if (!jaina)
							break;

						Talk(DATA_TYRANNUS, SAY_TYRANNUS_04);

						GetCreatureListWithEntryInGrid(champions, jaina, NPC_CHAMPION_1_ALLIANCE, 25.0f);
						GetCreatureListWithEntryInGrid(champions, jaina, NPC_CHAMPION_2_ALLIANCE, 25.0f);
						GetCreatureListWithEntryInGrid(champions, jaina, NPC_CHAMPION_3_ALLIANCE, 25.0f);

						for (Creature* champion : champions)
						{
							if (Creature* necrolyte = champion->FindNearestCreature(NPC_DEATHWHISPER_NECROLYTE, 45.0f))
							{
                                necrolyte->CastSpell(necrolyte, SPELL_VOID_CHANNELING_DNT);
                                necrolyte->SetFaction(FACTION_HOSTILE);

								champion->SetSpeedRate(MOVE_RUN, 1.0f);
                                champion->GetMotionMaster()->Clear();
                                champion->GetMotionMaster()->MoveFollow(necrolyte, 0.0f, rand_norm() * 2 * M_PI);
							}
						}

						Next(3500ms);
						break;
					}
					case STAGE_INTRODUCTION + 5:
						Talk(DATA_TYRANNUS, SAY_TYRANNUS_05);
						for (Creature* champion : champions)
						{
                            float dist = rand_norm() * 2.0f;
                            float angle = rand_norm() * 2 * M_PI;
                            float posX = champion->GetPositionX() + dist * cos(angle);
                            float posY = champion->GetPositionY() + dist * sin(angle);
                            float posZ = champion->GetPositionZ() + frand(6.0f, 8.0f);

                            champion->SetWalk(true);
                            champion->SetDisableGravity(true);
                            champion->CastSpell(champion, SPELL_STRANGULATING);
                            champion->GetMotionMaster()->Clear();
                            champion->GetMotionMaster()->MovePoint(0, posX, posY, posZ, false);
						}
						Next(6s);
						break;
					case STAGE_INTRODUCTION + 6:
						if (Creature* tyrannus = GetCreature(DATA_TYRANNUS))
						{
							tyrannus->CastSpell(tyrannus, SPELL_NECROMANTIC_POWER);
							for (Creature* champion : champions)
							{
                                champion->SetWalk(false);
                                champion->RemoveAurasDueToSpell(SPELL_STRANGULATING);
                                champion->GetMotionMaster()->Clear();
                                champion->GetMotionMaster()->MoveFall();

                                Unit::Kill(champion, champion);
							}
						}
						Next(3s);
						break;
					case STAGE_INTRODUCTION + 7:
						Talk(DATA_JAINA_1, SAY_JAINA_06);
						Next(3s);
						break;
					case STAGE_INTRODUCTION + 8:
					{
						Creature* jaina = GetCreature(DATA_JAINA_1);
						if (!jaina)
							break;

						Talk(DATA_TYRANNUS, SAY_TYRANNUS_07);
						for (Creature* champion : champions)
						{
                            champion->SetVisible(false);
                            if (Creature* skeleton = champion->SummonCreature(NPC_CORRUPTED_CHAMPION, champion->GetPosition()))
                            {
                                skeleton->CastSpell(skeleton, SPELL_RAISE_DEAD);
                                skeleton->SetFacingToObject(jaina);
                            }
						}

                        Next(4s);
                        break;
					}
                    case STAGE_INTRODUCTION + 9:
                    {
                        Creature* jaina = GetCreature(DATA_JAINA_1);
                        if (!jaina)
                            break;

                        GetCreatureListWithEntryInGrid(skeletons, jaina, NPC_CORRUPTED_CHAMPION, 45.0f);

                        for (Creature* skeleton : skeletons)
                        {
                            skeleton->SetWalk(true);
                            skeleton->GetMotionMaster()->MovePoint(0, jaina->GetPosition());
                        }

                        Next(380ms);
                        break;
                    }
                    case STAGE_INTRODUCTION + 10:
                    {
                        Creature* jaina = GetCreature(DATA_JAINA_1);
                        if (!jaina)
                            break;

                        Talk(DATA_JAINA_1, SAY_JAINA_08);

                        jaina->CastSpell(jaina, SPELL_FROST_CHANNELING_DNT);

                        if (Creature* elandra = GetCreature(DATA_ELANDRA))
                        {
                            if (Creature* necrolyte = elandra->FindNearestCreature(NPC_DEATHWHISPER_NECROLYTE, 45.0f))
                            {
                                elandra->AI()->AttackStart(necrolyte);

                                necrolyte->RemoveAllAuras();
                                necrolyte->GetMotionMaster()->MovePoint(0, elandra->GetPosition());
                            }
                        }

                        if (Creature* koreln = GetCreature(DATA_KORELN))
                        {
                            if (Creature* necrolyte = koreln->FindNearestCreature(NPC_DEATHWHISPER_NECROLYTE, 45.0f))
                            {
                                koreln->AI()->AttackStart(necrolyte);

                                necrolyte->RemoveAllAuras();
                                necrolyte->GetMotionMaster()->MovePoint(0, koreln->GetPosition());
                            }
                        }

                        // Launch Comet Storm Event
                        events.ScheduleEvent(EVENT_COMET_STORM, 850ms);

                        Next(5s);
                        break;
                    }
                    case STAGE_INTRODUCTION + 11:
                        if (Creature* rimefang = GetCreature(DATA_RIMEFANG))
                        {
                            rimefang->SetSpeedRate(MOVE_RUN, 1.8f);
                            rimefang->GetMotionMaster()->MovePoint(0, TyrannusAfterIntro, false);
                        }
                        break;

                    #pragma endregion

                    // Martin Victus Freed
                    case STAGE_MARTIN_VICTUS:
                    {
                        if (Creature* martin = GetCreature(DATA_MARTIN_VICTUS_1))
                            martin->SetStandState(UNIT_STAND_STATE_STAND);
                        break;
                    }

                    // Tyrannus Introduction
                    #pragma region TYRANNUS_INTRODUCTION

                    case STAGE_TYRANNUS:
                    {
                        if (Creature* rimefang = GetCreature(DATA_RIMEFANG))
                        {
                            rimefang->NearTeleportTo(TyrannusPoint01);
                        }

                        if (Creature* jaina = GetCreature(DATA_JAINA_1))
                        {
                            jaina->SetEmoteState(EMOTE_STATE_NONE);
                            jaina->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);

                            TeleportAndMovePath(jaina, JainaPath01);
                        }

                        if (Creature* elandra = GetCreature(DATA_ELANDRA))
                        {
                            elandra->SetEmoteState(EMOTE_STATE_FLY_READY_2HL);
                            TeleportAndMovePath(elandra, ElandraPath01);
                        }

                        if (Creature* koreln = GetCreature(DATA_KORELN))
                        {
                            koreln->SetEmoteState(EMOTE_STATE_FLY_READY_2HL);
                            TeleportAndMovePath(koreln, KorelnPath01);
                        }

                        // Si Martin Victus n'est pas libéré, il faut le spawner manuellement
                        Creature* martin = GetCreature(DATA_MARTIN_VICTUS_2);
                        if (!martin)
                            martin = instance->SummonCreature(NPC_MARTIN_VICTUS_2, { 1068.41f, 68.79f, 631.44f, 4.53f });

                        if (martin)
                        {
                            martin->SetEmoteState(EMOTE_STATE_FLY_READY_1H);
                            TeleportAndMovePath(martin, MartinPath01);
                        }

                        Next(6s);
                        break;
                    }
                    case STAGE_TYRANNUS + 1:
                        break;

                    #pragma endregion

                    // Comet Storm Event - Jaina
                    case EVENT_COMET_STORM:
                    {
                        Creature* jaina = GetCreature(DATA_JAINA_1);
                        if (!jaina)
                            break;

                        if (skeletonIndex >= 32 || skeletons.empty())
                            break;

                        Trinity::Containers::EraseIf(skeletons, [this](Creature* const creature)
                        {
                            return creature->isDead();
                        });

                        if (skeletons.empty())
                            break;

                        if (Creature* target = Trinity::Containers::SelectRandomContainerElement(skeletons))
                        {
                            Position position = target->GetRandomNearPosition(frand(5.f, 8.f));
                            jaina->CastSpell(position, SPELL_COMET_STORM, true);
                            jaina->CastSpell(position, SPELL_FROZEN_GROUND, true);
                        }

                        skeletonIndex++;
                        events.Repeat(50ms, 90ms);
                        break;
                    }

                    // Soldiers running intro - Soldiers
                    case EVENT_SOLDIERS_RUNNING:
                    {
                        if (actorsIndex >= actors.size())
                            break;

                        auto front = actors.begin();
                        std::advance(front, actorsIndex);

                        if (Creature* actor = *front)
                        {
                            actor->GetMotionMaster()->Clear();
                            actor->GetMotionMaster()->MovePoint(0, actor->GetHomePosition(), true, actor->GetHomePosition().GetOrientation());
                        }

                        actorsIndex++;
                        events.Repeat(30ms, 50ms);
                        break;
                    }
				}
			}

			private:
			Phases phase;
			uint32 eventId;
            uint8 skeletonIndex;
            uint8 actorsIndex;
			EventMap events;
			std::list<Creature*> champions;
            std::list<Creature*> skeletons;
            std::list<Creature*> actors;

			void Talk(Creature* creature, uint8 id)
			{
				creature->AI()->Talk(id);
			}

			void Talk(uint32 data, uint8 id)
			{
				if (Creature* creature = GetCreature(data))
					creature->AI()->Talk(id);
			}

			void Next(const Milliseconds& time)
			{
				eventId++;
				events.ScheduleEvent(eventId, time);
			}

            void TeleportAndMovePath(Creature* creature, const WaypointPath path)
            {
                std::array<Position, 2> positions = GetFirstAndLastPoint(path);

                creature->NearTeleportTo(positions[0]);
                creature->SetHomePosition(positions[1]);

                creature->GetMotionMaster()->MovePath(path, false);
            }

            const std::array<Position, 2> GetFirstAndLastPoint(const WaypointPath path)
            {
                Position const first = {
                    path.Nodes[0].X,
                    path.Nodes[0].Y,
                    path.Nodes[0].Z,
                    *(path.Nodes[0].Orientation)
                };

                uint8 length = path.Nodes.size() - 1;

                Position const last = {
                    path.Nodes[length].X,
                    path.Nodes[length].Y,
                    path.Nodes[length].Z,
                    *(path.Nodes[length].Orientation)
                };

                return { first, last };
            }
		};

		InstanceScript* GetInstanceScript(InstanceMap* map) const override
		{
			return new instance_pit_of_saron_InstanceScript(map);
		}
};

void AddSC_instance_pit_of_saron_custom()
{
	new instance_pit_of_saron_custom();
}
