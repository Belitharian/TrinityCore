#include "ScriptMgr.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "TemporarySummon.h"
#include "ScriptedCreature.h"
#include "CreatureAIImpl.h"
#include "MotionMaster.h"
#include "Weather.h"
#include "GameObject.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "affray_isle.h"

#include <iostream>

enum Phases
{
	PHASE_NONE,
	PHASE_COMBAT,
	PHASE_BLINK,
	PHASE_ICE_FALL,
};

class jaina_affray_isle : public CreatureScript
{
	public:
	jaina_affray_isle() : CreatureScript("jaina_affray_isle") { }

	struct jaina_affray_isleAI : public ScriptedAI
	{
		jaina_affray_isleAI(Creature* creature) : ScriptedAI(creature), debug(false),
			blinkIndex(0), wallsIndex(0)
		{
			Initialize();
		}

		void Initialize()
		{
			player = nullptr;
			blinkIndex= 0;
			wallsIndex = 0;
		}

		void OnSpellCastFinished(SpellInfo const* spell, SpellFinishReason reason) override
		{
			if (spell->Id == SPELL_BLINK && reason == SPELL_FINISHED_SUCCESSFUL_CAST)
			{
				if (events.IsInPhase(PHASE_BLINK))
				{
					DoCastSelf(SPELL_ARCANE_EXPLOSION);
					for (ThreatReference const* ref : me->GetThreatManager().GetUnsortedThreatList())
					{
						Unit* victim = ref->GetVictim();
						victim->StopMoving();
						victim->CastStop();
						victim->GetMotionMaster()->Clear();

						const Position pos = me->GetPosition();
						victim->GetMotionMaster()->MoveKnockbackFrom(pos.GetPositionX(), pos.GetPositionY(), 10.f, 5.f);
					}
				}
				else if (events.IsInPhase(PHASE_ICE_FALL))
				{
					DoCastVictim(SPELL_ICE_FALL);
					for (ThreatReference const* ref : me->GetThreatManager().GetUnsortedThreatList())
					{
						Unit* victim = ref->GetVictim();
						if (victim->GetTypeId() != TYPEID_PLAYER
                            && (roll_chance_i(60) || victim->GetEntry() == NPC_THRALL))
						{
							victim->CastStop();
							victim->StopMoving();
							victim->GetMotionMaster()->Clear();
							victim->GetMotionMaster()->MoveFleeing(me, 12 * IN_MILLISECONDS);
						}
					}
				}
			}

			if (spell->Id == SPELL_ICE_FALL
				&& reason == SPELL_FINISHED_CHANNELING_COMPLETE
				&& events.IsInPhase(PHASE_ICE_FALL))
			{
				me->RemoveAurasDueToSpell(SPELL_ICY_GLARE);
				me->SetControlled(false, UNIT_STATE_ROOT);
				events.SetPhase(PHASE_COMBAT);
				events.ScheduleEvent(EVENT_FROST_BOLT, 3s, 0, PHASE_COMBAT);
			}
		}

		void SetData(uint32 id, uint32 value) override
		{
			player = me->SelectNearestPlayer(25.f);

			if (value == 255 && !debug)
			{
				debug = true;

				me->NearTeleportTo(JainaFinalPos);
				player->NearTeleportTo(JainaFinalPos);

				const Position pos = GetPositionAround(me, 45.f, 5.f);
				thrall = me->SummonCreature(NPC_THRALL, pos);

				switch (id)
				{
					case 1000:
						events.ScheduleEvent(EVENT_INTRO_15, 2s);
						events.ScheduleEvent(EVENT_BATTLE_01, 5s);
						break;
				}

				return;
			}

			switch (id)
			{
				case START_AFFRAY_ISLE:
					if (debug) break;
					me->GetMap()->SetZoneWeather(17, WEATHER_STATE_FOG, 1.f);
					events.ScheduleEvent(EVENT_INTRO_01, 2s);
					break;
				case START_SUMMON_THRALL:
					if (debug) break;
					thrall = me->SummonCreature(NPC_THRALL, JainaPath02[0]);
					thrall->Mount(MOUNT_WHITE_WOLF_WOUNDED);
					thrall->SetSpeedRate(MOVE_RUN, 2.f);
					thrall->GetMotionMaster()->MoveSmoothPath(1, JainaPath02, JAINA_PATH_02_SIZE, false);
					break;
				case START_THRALL_ARRIVES:
					if (debug) break;
					thrall->SetSpeedRate(MOVE_RUN, 1.f);
					thrall->Dismount();
					thrall->SetFacingToObject(me);
					thrall->AI()->Talk(TALK_THRALL_07);
					me->RemoveAllAuras();
					me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_NONE);
					me->SetFacingToObject(thrall);
					events.ScheduleEvent(EVENT_THRALL_01, 2s);
					break;
				case START_BATTLE:
					if (debug) break;
					events.ScheduleEvent(EVENT_BATTLE_01, 5s);
					break;
				case START_POST_BATTLE:
					kalecgos = me->FindNearestCreature(NPC_KALECGOS, 80.f);
					events.ScheduleEvent(EVENT_POST_BATTLE_01, 2s);
					break;
				default:
					break;
			}
		}

		void JustEngagedWith(Unit* who) override
		{
			events.SetPhase(PHASE_COMBAT);
			events.ScheduleEvent(EVENT_FROST_BOLT, 5ms, 0, PHASE_COMBAT);
			events.ScheduleEvent(EVENT_SCHEDULE_PHASE_BLINK, 10s, 15s, 0, PHASE_COMBAT);
			events.ScheduleEvent(EVENT_SCHEDULE_PHASE_ICE_FALL, 35s, 45s, 0, PHASE_COMBAT);
		}

		void Reset() override
		{
			Initialize();

			events.Reset();
			events.SetPhase(PHASE_NONE);
		}

		void UpdateAI(uint32 diff) override
		{
			events.Update(diff);

			if (!UpdateVictim())
			{
				while (uint32 eventId = events.ExecuteEvent())
				{
					switch (eventId)
					{
						// Introduction
						#pragma region INTRODUCTION

						case EVENT_INTRO_01:
							Talk(TALK_JAINA_01);
							events.ScheduleEvent(EVENT_INTRO_02, 3s);
							break;
						case EVENT_INTRO_02:
							DoCast(SPELL_SIMPLE_TELEPORT);
							events.ScheduleEvent(EVENT_INTRO_03, 2s);
							break;
						case EVENT_INTRO_03:
							me->NearTeleportTo(-1710.29f, -4377.18f, 4.56f, 4.83f);
							me->SetVisible(false);
							player->NearTeleportTo(PlayerEntrancePos);
							events.ScheduleEvent(EVENT_INTRO_04, 1s);
							break;
						case EVENT_INTRO_04:
							me->SetVisible(true);
							DoCast(SPELL_VISUAL_TELEPORT);
							events.ScheduleEvent(EVENT_INTRO_05, 2s);
							break;
						case EVENT_INTRO_05:
							Talk(TALK_JAINA_02);
							events.ScheduleEvent(EVENT_INTRO_06, 3s);
							break;
						case EVENT_INTRO_06:
							me->GetMotionMaster()->MoveSmoothPath(0, JainaPath01, JAINA_PATH_01_SIZE, false);
							events.ScheduleEvent(EVENT_INTRO_07, 9s);
							break;
						case EVENT_INTRO_07:
						{
							if (klannoc = me->SummonCreature(NPC_KLANNOC_MACLEOD, KlannocPath01[0], TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10min))
							{
								me->SetFacingTo(0.32f);

								klannoc->GetMotionMaster()->MoveSmoothPath(0, KlannocPath01, KLANNOC_PATH_01_SIZE, true);
								klannoc->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
								klannoc->SetReactState(REACT_PASSIVE);
								klannoc->SetFaction(14);
							}

							Position pos = GetPositionAround(player, 180.f, 1.f);
							if (playerSpectator = me->SummonCreature(NPC_AFFRAY_SPECTATOR, pos, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10min))
							{
								playerSpectator->CastSpell(player, SPELL_STUNNED);
								player->Lock(true);

								playerSpectator->UpdateGroundPositionZ(pos.GetPositionX(), pos.GetPositionY(), pos.m_positionZ);
								playerSpectator->NearTeleportTo(pos);
								playerSpectator->SetFacingToObject(player);
								playerSpectator->CastSpell(playerSpectator, SPELL_SMOKE_REVEAL);
								playerSpectator->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
								playerSpectator->SetReactState(REACT_PASSIVE);
								playerSpectator->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_READY_THROWN);
								playerSpectator->SetFaction(14);
								playerSpectator->LoadEquipment(urand(0, 3));
							}

							float angle = 45.f;
							for (uint8 i = 0; i < SPECTATORS_MAX_NUMBER; i++)
							{
								Position pos = GetPositionAround(me, angle, 6.5f);
								if (Creature* spectator = me->SummonCreature(NPC_AFFRAY_SPECTATOR, pos, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10min))
								{
									spectator->UpdateGroundPositionZ(pos.GetPositionX(), pos.GetPositionY(), pos.m_positionZ);
									spectator->NearTeleportTo(pos);

									spectator->SetFacingToObject(me);
									spectator->CastSpell(spectator, SPELL_SMOKE_REVEAL);
									spectator->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
									spectator->SetReactState(REACT_PASSIVE);
									spectator->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_READY_THROWN);
									spectator->SetFaction(14);
									spectator->LoadEquipment(urand(0, 3));

									spectators.push_back(spectator);
								}

								angle += 45.f;
							}

							events.ScheduleEvent(EVENT_INTRO_08, 8s);
							break;
						}
						case EVENT_INTRO_08:
							me->SetFacingToObject(klannoc);
							klannoc->SetFacingToObject(me);
							klannoc->AI()->Talk(TALK_KLANNOC_03);
							events.ScheduleEvent(EVENT_INTRO_09, 3s);
							break;
						case EVENT_INTRO_09:
							Talk(TALK_JAINA_04);
							events.ScheduleEvent(EVENT_INTRO_10, 3s);
							break;
						case EVENT_INTRO_10:
							klannoc->m_Events.AddEvent(new KlannocBurning(klannoc, me), klannoc->m_Events.CalculateTime(1ms));
							events.ScheduleEvent(EVENT_INTRO_11, 3s);
							break;
						case EVENT_INTRO_11:
						{
							for (Creature* spectator : spectators)
							{
								if (roll_chance_i(30))
									spectator->AI()->Talk(TALK_SPECTATOR_FLEE);

								spectator->GetMotionMaster()->MoveFleeing(me);
								spectator->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_COWER);
							}

							playerSpectator->GetMotionMaster()->MoveFleeing(me);
							playerSpectator->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_COWER);

							uint8 randomSpectator = urand(0, SPECTATORS_MAX_NUMBER - 1);
							if (Creature* victim = spectators[randomSpectator])
							{
								victim->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);

								CastSpellExtraArgs args;
								args.AddSpellBP0(99999999);
								args.SetTriggerFlags(TRIGGERED_CAST_DIRECTLY);

								DoCast(victim, SPELL_ARCANE_BARRAGE, args);
								me->SetTarget(victim->GetGUID());
							}

							events.ScheduleEvent(EVENT_INTRO_12, 5s);
							break;
						}
						case EVENT_INTRO_12:
						{
							Talk(TALK_JAINA_05);
							for (Creature* spectator : spectators)
							{
								spectator->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
								spectator->m_Events.AddEvent(new SpectatorDeath(spectator), spectator->m_Events.CalculateTime(2s));
							}

							playerSpectator->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
							playerSpectator->m_Events.AddEvent(new SpectatorDeath(playerSpectator), playerSpectator->m_Events.CalculateTime(2s));

							player->Lock(false);

							CastSpellExtraArgs args;
							args.SetTriggerFlags(TRIGGERED_CAST_DIRECTLY);

							me->SetTarget(ObjectGuid::Empty);
							DoCastAOE(SPELL_ICE_NOVA, args);

							events.ScheduleEvent(EVENT_INTRO_13, 5s);
							break;
						}
						case EVENT_INTRO_13:
							Talk(TALK_JAINA_06);
							me->GetMotionMaster()->Clear();
							me->GetMotionMaster()->MoveSmoothPath(0, JainaPath02, JAINA_PATH_02_SIZE, false);
							events.ScheduleEvent(EVENT_INTRO_14, 13s);
							break;
						case EVENT_INTRO_14:
							me->SetWalk(true);
							me->GetMotionMaster()->MovePoint(0, JainaFinalPos, true, JainaFinalPos.GetOrientation());
							events.ScheduleEvent(EVENT_INTRO_15, Milliseconds((int)me->GetMotionMaster()->GetTime()));
							break;
						case EVENT_INTRO_15:
						{
							if (focusingIrisFx = me->SummonCreature(NPC_FOCUSING_IRIS, -1654.17f, -4246.51f, 3.51f, 3.08f))
							{
								waveFx = me->SummonCreature(NPC_INVISIBLE_STALKER, -1553.19f, -4251.73f, -1.91f, 4.48f);
								waveFx->AddAura(SPELL_WAVE_VISUAL, waveFx);

								focusingIrisFx->CastSpell(waveFx, SPELL_WAVE_CANALISATION);

								DoCast(SPELL_ARCANE_CANALISATION);

								me->SetFacingToObject(focusingIrisFx);
								me->SummonGameObject(GOB_ANTONIDAS_BOOK, -1653.03f, -4243.82f, 3.11f, 3.99f, QuaternionData(0.f, 0.f, -0.9101f, 0.4143f), 0s);
							}
							events.ScheduleEvent(EVENT_INTRO_16, 10s);
							break;
						}
						case EVENT_INTRO_16:
							SetData(START_SUMMON_THRALL, 1U);
							break;

						#pragma endregion

						// Thrall
						#pragma region THRALL

						case EVENT_THRALL_01:
							Talk(TALK_JAINA_08);
							events.ScheduleEvent(EVENT_THRALL_02, 1s);
							break;
						case EVENT_THRALL_02:
							thrall->AI()->Talk(TALK_THRALL_09);
							events.ScheduleEvent(EVENT_THRALL_03, 7s);
							break;
						case EVENT_THRALL_03:
							thrall->AI()->Talk(TALK_THRALL_10);
							events.ScheduleEvent(EVENT_THRALL_04, 9s);
							break;
						case EVENT_THRALL_04:
							thrall->AI()->Talk(TALK_THRALL_11);
							events.ScheduleEvent(EVENT_THRALL_05, 14s);
							break;
						case EVENT_THRALL_05:
							Talk(TALK_JAINA_12);
							events.ScheduleEvent(EVENT_THRALL_06, 5s);
							break;
						case EVENT_THRALL_06:
							thrall->AI()->Talk(TALK_THRALL_13);
							events.ScheduleEvent(EVENT_THRALL_07, 15s);
							break;
						case EVENT_THRALL_07:
							Talk(TALK_JAINA_14);
							events.ScheduleEvent(EVENT_THRALL_08, 14s);
							break;
						case EVENT_THRALL_08:
							Talk(TALK_JAINA_15);
							events.ScheduleEvent(EVENT_THRALL_09, 12s);
							break;
						case EVENT_THRALL_09:
							thrall->AI()->Talk(TALK_THRALL_16);
							events.ScheduleEvent(EVENT_THRALL_10, 15s);
							break;
						case EVENT_THRALL_10:
							Talk(TALK_JAINA_17);
							events.ScheduleEvent(EVENT_THRALL_11, 18s);
							break;
						case EVENT_THRALL_11:
							thrall->AI()->Talk(TALK_THRALL_18);
							events.ScheduleEvent(EVENT_THRALL_12, 2s);
							break;
						case EVENT_THRALL_12:
							me->SetFacingToObject(focusingIrisFx);
							thrall->SetFacingToObject(waveFx);
							events.ScheduleEvent(EVENT_THRALL_13, 1s);
							break;
						case EVENT_THRALL_13:
							DoCast(SPELL_ARCANE_CANALISATION);
							events.ScheduleEvent(EVENT_THRALL_14, 3s);
							break;
						case EVENT_THRALL_14:
							thrall->AI()->Talk(TALK_THRALL_19);
							thrall->AI()->DoCast(SPELL_WIND_ELEMENTAL_TOTEM);
							thrall->CastSpell(thrall, SPELL_NATURE_CANALISATION, true);
							if (windElemental = me->FindNearestCreature(NPC_WIND_ELEMENTAL, 15.f))
								windElemental->GetMotionMaster()->MovePoint(0, -1521.15f, -4250.68f, 0.f, false, 3.13f);
							events.ScheduleEvent(EVENT_THRALL_15, 12s);
							break;
						case EVENT_THRALL_15:
							thrall->AI()->Talk(TALK_THRALL_20);
							break;

						#pragma endregion

						// Battle
						#pragma region BATTLE

						case EVENT_BATTLE_01:
							Talk(TALK_JAINA_21);
							me->RemoveAllAuras();
							me->GetMap()->SetZoneOverrideLight(17, 0, 1971, 2000);
							me->GetMap()->SetZoneWeather(17, WEATHER_STATE_HEAVY_SNOW, 1.f);
							me->NearTeleportTo(JainaBattlePos);
							me->SetHomePosition(JainaBattlePos);
							me->SetRegenerateHealth(false);
							me->SetFaction(150);
							thrall->SetRegenerateHealth(false);
							thrall->SetImmuneToPC(true);
							player->NearTeleportTo(JainaBattlePos);
							events.ScheduleEvent(EVENT_BATTLE_02, 1s);
							break;
						case EVENT_BATTLE_02:
						{
							thrall->AI()->Talk(TALK_THRALL_22);

							DoCast(SPELL_FROST_CANALISATION);
							for (uint8 i = 0; i < ICE_WALLS_MAX_NUMBER; i++)
							{
								const Position pos = { IceWallsPos[i][0], IceWallsPos[i][1], IceWallsPos[i][2], IceWallsPos[i][3] };
								if (Creature* iceFx = me->SummonCreature(NPC_INVISIBLE_STALKER, pos))
								{
									iceFx->SetObjectScale(3.f);
									iceWallsFx.push_back(iceFx);
								}
							}
							events.ScheduleEvent(EVENT_BATTLE_03, 1s);
							break;
						}
						case EVENT_BATTLE_03:
						{
							if (wallsIndex >= ICE_WALLS_MAX_NUMBER)
							{
								DoCast(SPELL_SIMPLE_TELEPORT);
								events.CancelEvent(EVENT_BATTLE_03);
								events.ScheduleEvent(EVENT_BATTLE_04, 1500ms);
							}
							else
							{
								const Position pos = { IceWallsPos[wallsIndex][0], IceWallsPos[wallsIndex][1], IceWallsPos[wallsIndex][2], IceWallsPos[wallsIndex][3] };
								const QuaternionData rot = { IceWallsPos[wallsIndex][4], IceWallsPos[wallsIndex][5], IceWallsPos[wallsIndex][6], IceWallsPos[wallsIndex][7] };

								if (GameObject* icewall = me->SummonGameObject(GOB_ICE_WALL, pos, rot, 0s))
								{
									iceWallsFx[wallsIndex]->CastSpell(iceWallsFx[wallsIndex], SPELL_FROST_EXPLOSION, true);
									iceWalls.push_back(icewall);
								}

								wallsIndex++;

								events.RescheduleEvent(EVENT_BATTLE_03, 200ms, 500ms);
							}
							break;
						}
						case EVENT_BATTLE_04:
						{
							Talk(TALK_JAINA_23);
							Talk(TALK_JAINA_24);

							me->AddAura(SPELL_ARCANIC_FORM, me);

							const Position pos = me->GetRandomNearPosition(8.f);
							thrall->NearTeleportTo(pos);
							thrall->SetHomePosition(pos);
							thrall->CastSpell(thrall, SPELL_POWER_BALL_VISUAL);
							thrall->AddAura(SPELL_FORCED_TELEPORT, thrall);
							thrall->SetFaction(89);
							events.ScheduleEvent(EVENT_BATTLE_05, 2s);
							break;
						}
						case EVENT_BATTLE_05:
							thrall->SetHomePosition(thrall->GetPosition());
							me->Attack(thrall, true);
							thrall->Attack(me, true);
							break;

						#pragma endregion

						// Post-battle
						#pragma region POST_BATTLE

                        case EVENT_POST_BATTLE_01:
                            kalecgos->SetFacingToObject(me);
                            me->SetFacingToObject(kalecgos);
                            if (debug)
                            {
                                events.ScheduleEvent(EVENT_POST_BATTLE_23, 2s);
                                break;
                            }
                            events.ScheduleEvent(EVENT_POST_BATTLE_02, 2s);
                            break;
                        case EVENT_POST_BATTLE_02:
                            Talk(TALK_JAINA_27);
                            events.ScheduleEvent(EVENT_POST_BATTLE_03, 2s);
                            break;
                        case EVENT_POST_BATTLE_03:
                            kalecgos->AI()->Talk(TALK_KALECGOS_28);
                            events.ScheduleEvent(EVENT_POST_BATTLE_04, 9s);
                            break;
                        case EVENT_POST_BATTLE_04:
                            kalecgos->AI()->Talk(TALK_KALECGOS_29);
                            events.ScheduleEvent(EVENT_POST_BATTLE_05, 2s);
                            break;
                        case EVENT_POST_BATTLE_05:
                            Talk(TALK_JAINA_30);
                            kalecgos->SetWalk(true);
                            kalecgos->SetSpeedRate(MOVE_WALK, 0.3f);
                            events.ScheduleEvent(EVENT_POST_BATTLE_06, 14s);
                            break;
                        case EVENT_POST_BATTLE_06:
                            kalecgos->GetMotionMaster()->MoveCloserAndStop(POINTID_KALECGOS_04, me, 3.f);
                            kalecgos->AI()->Talk(TALK_KALECGOS_31);
                            events.ScheduleEvent(EVENT_POST_BATTLE_07, 2s);
                            break;
                        case EVENT_POST_BATTLE_07:
                            Talk(TALK_JAINA_32);
                            events.ScheduleEvent(EVENT_POST_BATTLE_08, 2s);
                            break;
                        case EVENT_POST_BATTLE_08:
                            kalecgos->AI()->Talk(TALK_KALECGOS_33);
                            events.ScheduleEvent(EVENT_POST_BATTLE_09, 10s);
                            break;
                        case EVENT_POST_BATTLE_09:
                            Talk(TALK_JAINA_34);
                            events.ScheduleEvent(EVENT_POST_BATTLE_10, 4s);
                            break;
                        case EVENT_POST_BATTLE_10:
                            kalecgos->AI()->Talk(TALK_KALECGOS_35);
                            events.ScheduleEvent(EVENT_POST_BATTLE_11, 4s);
                            break;
                        case EVENT_POST_BATTLE_11:
                            Talk(TALK_JAINA_36);
                            events.ScheduleEvent(EVENT_POST_BATTLE_12, 1s);
                            break;
                        case EVENT_POST_BATTLE_12:
                            thrall->AI()->Talk(TALK_THRALL_37);
                            thrall->SetStandState(UNIT_STAND_STATE_STAND);
                            events.ScheduleEvent(EVENT_POST_BATTLE_13, 9s);
                            break;
                        case EVENT_POST_BATTLE_13:
                            Talk(TALK_JAINA_38);
                            thrall->SetFacingToObject(me);
                            events.ScheduleEvent(EVENT_POST_BATTLE_14, 3s);
                            break;
                        case EVENT_POST_BATTLE_14:
                            kalecgos->AI()->Talk(TALK_KALECGOS_39);
                            events.ScheduleEvent(EVENT_POST_BATTLE_15, 4s);
                            break;
                        case EVENT_POST_BATTLE_15:
                            kalecgos->AI()->Talk(TALK_KALECGOS_40);
                            events.ScheduleEvent(EVENT_POST_BATTLE_16, 6s);
                            break;
                        case EVENT_POST_BATTLE_16:
                            Talk(TALK_JAINA_41);
                            me->GetMap()->SetZoneWeather(17, WEATHER_STATE_FINE, 1.f);
                            me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_NONE);
                            me->SetSheath(SHEATH_STATE_UNARMED);
                            events.ScheduleEvent(EVENT_POST_BATTLE_17, 5s);
                            break;
                        case EVENT_POST_BATTLE_17:
                            Talk(TALK_JAINA_42);
                            me->SetFacingToObject(thrall);
                            events.ScheduleEvent(EVENT_POST_BATTLE_18, 7s);
                            break;
                        case EVENT_POST_BATTLE_18:
                            thrall->AI()->Talk(TALK_THRALL_43);
                            thrall->CastSpell(thrall, SPELL_ASTRAL_RECALL);
                            thrall->DespawnOrUnsummon(11s);
                            events.ScheduleEvent(EVENT_POST_BATTLE_19, 6s);
                            break;
                        case EVENT_POST_BATTLE_19:
                            Talk(TALK_JAINA_44);
                            events.ScheduleEvent(EVENT_POST_BATTLE_20, 3s);
                            break;
                        case EVENT_POST_BATTLE_20:
                            Talk(TALK_JAINA_45);
                            me->SetFacingToObject(kalecgos);
                            events.ScheduleEvent(EVENT_POST_BATTLE_21, 3s);
                            break;
                        case EVENT_POST_BATTLE_21:
                            kalecgos->AI()->Talk(TALK_KALECGOS_46);
                            events.ScheduleEvent(EVENT_POST_BATTLE_22, 3s);
                            break;
                        case EVENT_POST_BATTLE_22:
                            Talk(TALK_JAINA_47);
                            events.ScheduleEvent(EVENT_POST_BATTLE_23, 10s);
                            break;
                        case EVENT_POST_BATTLE_23:
                            for (GameObject* iceWall : iceWalls)
                                iceWall->UseDoorOrButton();
                            kalecgos->CastSpell(kalecgos, SPELL_TRANSFORM_VISUAL);
                            kalecgos->CastSpell(kalecgos, SPELL_KALECGOS_TRANSFORM);
                            kalecgos->UpdateEntry(NPC_KALECGOS_DRAGON);
                            events.ScheduleEvent(EVENT_POST_BATTLE_24, 2s);
                            break;
                        case EVENT_POST_BATTLE_24:
                        {
                            Position pos = kalecgos->GetPosition();
                            pos.m_positionZ += 5.f;

                            if (windElemental) windElemental->DespawnOrUnsummon();
                            if (focusingIrisFx) focusingIrisFx->DespawnOrUnsummon();
                            if (waveFx) waveFx->DespawnOrUnsummon();

                            kalecgos->SetSpeedRate(MOVE_FLIGHT, .5f);
                            kalecgos->GetMotionMaster()->MovePoint(POINTID_KALECGOS_05, pos);
                            kalecgos->HandleEmoteCommand(EMOTE_ONESHOT_LIFTOFF);
                            kalecgos->SetDisableGravity(true);
                            break;
                        }

						#pragma endregion
					}
				}
			}
			else
			{
				if (me->HasUnitState(UNIT_STATE_CASTING))
					return;

				while (uint32 eventId = events.ExecuteEvent())
				{
					switch (eventId)
					{
						case EVENT_FROST_BOLT:
							DoCastVictim(SPELL_FROST_BOTL);
							events.RescheduleEvent(EVENT_FROST_BOLT, 2s, 0, PHASE_COMBAT);
							break;
						case EVENT_SCHEDULE_PHASE_BLINK:
							me->InterruptNonMeleeSpells(true);
							me->StopMoving();
							me->SetControlled(true, UNIT_STATE_ROOT);
							events.SetPhase(PHASE_BLINK);
							events.ScheduleEvent(EVENT_BLINK, 100ms, 0, PHASE_BLINK);
							events.RescheduleEvent(EVENT_SCHEDULE_PHASE_BLINK, 1min, 2min, 0, PHASE_COMBAT);
							break;
						case EVENT_BLINK:
							if (blinkIndex > 3)
							{
								blinkIndex = 0;
								me->SetControlled(false, UNIT_STATE_ROOT);
								events.SetPhase(PHASE_COMBAT);
								events.ScheduleEvent(EVENT_FROST_BOLT, 3s, 0, PHASE_COMBAT);
							}
							else
							{
								blinkIndex++;
								const Position pos = me->GetVictim()->GetRandomNearPosition(15.f);
								me->CastSpell(pos, SPELL_BLINK, true);
								events.RescheduleEvent(EVENT_BLINK, 1s, 0, PHASE_BLINK);
							}
							break;
						case EVENT_SCHEDULE_PHASE_ICE_FALL:
							me->InterruptNonMeleeSpells(true);
							me->StopMoving();
							me->SetControlled(true, UNIT_STATE_ROOT);
							events.SetPhase(PHASE_ICE_FALL);
							events.ScheduleEvent(EVENT_ICE_FALL, 100ms, 0, PHASE_ICE_FALL);
							events.RescheduleEvent(EVENT_SCHEDULE_PHASE_ICE_FALL, 45s, 1min, 0, PHASE_COMBAT);
							break;
						case EVENT_ICE_FALL:
							me->AddAura(SPELL_ICY_GLARE, me);
							me->CastSpell(JainaBattlePos, SPELL_BLINK, true);
							break;
					}

					if (me->HasUnitState(UNIT_STATE_CASTING))
						return;
				}

				if (events.IsInPhase(PHASE_COMBAT))
					DoMeleeAttackIfReady();
			}
		}

		private:
		EventMap events;
		Player* player;
		Creature* klannoc;
		Creature* playerSpectator;
		Creature* kalecgos;
		Creature* thrall;
		Creature* windElemental;
		Creature* focusingIrisFx;
		Creature* waveFx;
		std::vector<Creature*> spectators;
		std::vector<Creature*> iceWallsFx;
		std::vector<GameObject*> iceWalls;
		uint8 blinkIndex;
		uint8 wallsIndex;

		bool debug;

		const Position GetPositionAround(Unit* unit, float angle, float radius)
		{
			float ang = float(angle * (M_PI / 180.f));
			Position pos;
			pos.m_positionX = unit->GetPositionX() + radius * sinf(ang);
			pos.m_positionY = unit->GetPositionY() + radius * cosf(ang);
			pos.m_positionZ = 4.38f;
			return pos;
		}
	};

	CreatureAI* GetAI(Creature* creature) const override
	{
		return new jaina_affray_isleAI(creature);
	}
};

class npc_thrall_affray : public CreatureScript
{
	public:
	npc_thrall_affray() : CreatureScript("npc_thrall_affray")
	{
	}

	enum class Phases
	{
		None,
		Battle,
		SummonKalecgos,
		End
	};

	struct npc_thrall_affrayAI : public ScriptedAI
	{
		npc_thrall_affrayAI(Creature* creature) : ScriptedAI(creature), handled(false), phase(Phases::None)
		{
			Initialize();
		}

		void Initialize()
		{
			scheduler.SetValidator([this]
			{
				return !me->HasUnitState(UNIT_STATE_CASTING);
			});
		}

		void MovementInform(uint32 type, uint32 id) override
		{
			if (type == EFFECT_MOTION_TYPE && id == 1)
			{
				if (Creature* jaina = me->FindNearestCreature(NPC_JAINA_PROUDMOORE, 35.f))
				{
					jaina->AI()->SetData(START_THRALL_ARRIVES, 1U);
				}
			}
		}

		void DamageTaken(Unit* /*attacker*/, uint32& damage) override
		{
			if (phase == Phases::Battle && HealthBelowPct(50))
			{
				handled = true;
				if (Creature* jaina = me->FindNearestCreature(NPC_JAINA_PROUDMOORE, 35.f))
				{
					phase = Phases::SummonKalecgos;
					if (Creature* kalecgos = me->SummonCreature(NPC_KALECGOS_DRAGON, -1737.87f, -4116.67f, 92.01f, 5.02f))
					{
						kalecgos->SetCanFly(true);
						kalecgos->SetDisableGravity(true);
						kalecgos->SendMovementFlagUpdate();
						kalecgos->GetMotionMaster()->MovePoint(POINTID_KALECGOS_01, -1670.11f, -4307.86f, 20.52f, false, 4.74f);
					}
				}
			}
		}

		void JustEngagedWith(Unit* who) override
		{
			phase = Phases::Battle;

			scheduler
				.Schedule(5ms, [this](TaskContext lightning_bolt)
				{
					DoCastVictim(SPELL_LIGHTNING_BOLT);
					lightning_bolt.Repeat(8s);
				})
				.Schedule(12s, [this](TaskContext lightning_chain)
				{
					DoCastVictim(SPELL_LIGHTNING_CHAIN);
					lightning_chain.Repeat(5s, 10s);
				})
				.Schedule(25s, [this](TaskContext fire_elemental_totem)
				{
					DoCast(SPELL_FIRE_ELEMENTAL_TOTEM);
					fire_elemental_totem.Repeat(25s, 32s);
				})
				.Schedule(3s, [this](TaskContext healing_wave)
				{
					if (Unit* target = DoSelectBelowHpPctFriendly(40.0f, 80, false))
					{
						me->InterruptNonMeleeSpells(true);
						DoCast(target, SPELL_HEALING_WAVE);
						healing_wave.Repeat(15s, 32s);
					}
					else
					{
						healing_wave.Repeat(1s);
					}
				});
		}

		void Reset() override
		{
			scheduler.CancelAll();
			Initialize();
		}

		void UpdateAI(uint32 diff) override
		{
			// Combat
			if (!UpdateVictim())
				return;

			if (me->HasUnitState(UNIT_STATE_FLEEING)
				|| me->HasUnitState(UNIT_STATE_FLEEING_MOVE))
			{
				return;
			}

			scheduler.Update(diff, [this]
			{
				DoMeleeAttackIfReady();
			});
		}

		private:
		TaskScheduler scheduler;
		bool handled;
		Phases phase;
	};

	CreatureAI* GetAI(Creature* creature) const override
	{
		return new npc_thrall_affrayAI(creature);
	}
};

class npc_wind_elemental_affray : public CreatureScript
{
	public:
	npc_wind_elemental_affray() : CreatureScript("npc_wind_elemental_affray")
	{
	}

	struct npc_wind_elemental_affrayAI : public ScriptedAI
	{
		npc_wind_elemental_affrayAI(Creature* creature) : ScriptedAI(creature)
		{
			Initialize();
		}

		void Initialize()
		{
		}

		void MovementInform(uint32 type, uint32 id) override
		{
			if (type == POINT_MOTION_TYPE && id == 0)
			{
				scheduler
					.Schedule(2s, [this](TaskContext /*context*/)
					{
						DoCast(SPELL_TORNADO);
						me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_SPELL_CHANNEL_DIRECTED);
						me->SetObjectScale(10.f);
					})
					.Schedule(5s, [this](TaskContext /*context*/)
					{
						if (Creature* jaina = me->FindNearestCreature(NPC_JAINA_PROUDMOORE, 1500.f))
						{
							jaina->AI()->SetData(START_BATTLE, 1U);
						}
					});
			}
		}

		void Reset() override
		{
			scheduler.CancelAll();
			Initialize();
		}

		void UpdateAI(uint32 diff) override
		{
			scheduler.Update(diff);
		}

		private:
		TaskScheduler scheduler;
	};

	CreatureAI* GetAI(Creature* creature) const override
	{
		return new npc_wind_elemental_affrayAI(creature);
	}
};

class npc_kalecgos_affray : public CreatureScript
{
	public:
	npc_kalecgos_affray() : CreatureScript("npc_kalecgos_affray")
	{
	}

	struct npc_kalecgos_affrayAI : public ScriptedAI
	{
		npc_kalecgos_affrayAI(Creature* creature) : ScriptedAI(creature)
		{
			Initialize();
		}

		void Initialize()
		{
		}

		void MovementInform(uint32 type, uint32 id) override
		{
			if (type == POINT_MOTION_TYPE)
			{
                switch (id)
                {
                    case POINTID_KALECGOS_01:
                        scheduler
                            .Schedule(1ms, [this](TaskContext /*context*/)
                            {
                                me->SetSpeedRate(MOVE_FLIGHT, .5f);
                                me->GetMotionMaster()->MovePoint(POINTID_KALECGOS_02, -1670.11f, -4307.86f, 3.77f);
                                me->HandleEmoteCommand(EMOTE_ONESHOT_LAND);
                                me->SetDisableGravity(false);
                            })
                            .Schedule(3s, [this](TaskContext /*context*/)
                            {
                                DoCast(me, SPELL_TRANSFORM_VISUAL);
                                DoCast(me, SPELL_KALECGOS_TRANSFORM);
                                me->UpdateEntry(NPC_KALECGOS);
                            })
                            .Schedule(6s, [this](TaskContext /*context*/)
                            {
                                if (Creature* thrall = me->FindNearestCreature(NPC_THRALL, 80.f))
                                {
                                    thrall->SetControlled(true, UNIT_STATE_ROOT);
                                    thrall->SetImmuneToNPC(true);
                                    thrall->RemoveAllAuras();
                                    thrall->UnsummonAllTotems();
                                    thrall->CombatStop();
                                    thrall->SetStandState(UNIT_STAND_STATE_KNEEL);
                                }

                                if (Creature* jaina = me->FindNearestCreature(NPC_JAINA_PROUDMOORE, 80.f))
                                {
                                    jaina->SetControlled(true, UNIT_STATE_ROOT);
                                    jaina->CombatStop();
                                    jaina->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_READY2HL);

                                    float distance = 6.f;
                                    float distanceToTravel = me->GetExactDist2d(jaina) - distance;
                                    float angle = me->GetAbsoluteAngle(jaina);
                                    float destx = me->GetPositionX() + distanceToTravel * std::cos(angle);
                                    float desty = me->GetPositionY() + distanceToTravel * std::sin(angle);
                                    me->GetMotionMaster()->MovePoint(POINTID_KALECGOS_03, destx, desty, jaina->GetPositionZ());

                                    jainaGUID = jaina->GetGUID();
                                }
                            });
                        break;
                    case POINTID_KALECGOS_03:
                        Talk(TALK_KALECGOS_26);
                        if (Creature* jaina = ObjectAccessor::GetCreature(*me, jainaGUID))
                            jaina->AI()->SetData(START_POST_BATTLE, 1U);
                        break;
                    case POINTID_KALECGOS_05:
                        if (Creature* jaina = ObjectAccessor::GetCreature(*me, jainaGUID))
                        {
                            jaina->GetMotionMaster()->MoveJump(me->GetPosition(), 12.f, 8.f);
                            jaina->DespawnOrUnsummon(2s);
                            scheduler
                                .Schedule(4s, [this](TaskContext /*context*/)
                                {
                                    me->GetMotionMaster()->MovePoint(POINTID_KALECGOS_06, -1737.87f, -4116.67f, 92.01f);
                                })
                                .Schedule(8s, [this](TaskContext /*context*/)
                                {
                                    if (Player* player = me->SelectNearestPlayer(80.f))
                                    {
                                        if (Group* group = player->GetGroup())
                                        {
                                            for (GroupReference* groupRef = group->GetFirstMember();
                                                 groupRef != nullptr;
                                                 groupRef = groupRef->next())
                                            {
                                                if (Player* member = groupRef->GetSource())
                                                    member->SetPhaseMask(1, true);
                                            }
                                        }
                                        else
                                            player->SetPhaseMask(1, true);
                                    }
                                });
                        }
                        break;
				}
			}
		}

		void Reset() override
		{
			scheduler.CancelAll();
			Initialize();
		}

		void UpdateAI(uint32 diff) override
		{
			scheduler.Update(diff);
		}

		private:
		TaskScheduler scheduler;
		ObjectGuid jainaGUID;
	};

	CreatureAI* GetAI(Creature* creature) const override
	{
		return new npc_kalecgos_affrayAI(creature);
	}
};

void AddSC_jaina_affray_isle()
{
	new jaina_affray_isle();
	new npc_thrall_affray();
	new npc_kalecgos_affray();
	new npc_wind_elemental_affray();
}
