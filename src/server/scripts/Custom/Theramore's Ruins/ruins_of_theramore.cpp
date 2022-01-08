#include "GameObject.h"
#include "InstanceScript.h"
#include "KillRewarder.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "PassiveAI.h"
#include "Scenario.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Custom/AI/CustomAI.h"
#include "ruins_of_theramore.h"

class npc_jaina_ruins : public CreatureScript
{
	public:
	npc_jaina_ruins() : CreatureScript("npc_jaina_ruins")
	{
	}

	struct npc_jaina_ruinsAI : public CustomAI
	{
		npc_jaina_ruinsAI(Creature* creature) : CustomAI(creature),
			hasBlinked(false), distance(10.f)
		{
			Initialize();
		}

		enum Spells
		{
			SPELL_FROSTBOLT         = 284703,
			SPELL_RING_OF_ICE       = 285459,
			SPELL_COMET_BARRAGE     = 354938,
			SPELL_FRIGID_SHARD      = 354933,
			SPELL_BLINK             = 357601
		};

		void Initialize()
		{
			instance = me->GetInstanceScript();
		}

		InstanceScript* instance;
		bool hasBlinked;
		float distance;
		Position beforeBlink;

		void SetData(uint32 /*id*/, uint32 value) override
		{
			distance = (float)value;
		}

		void AttackStart(Unit* who) override
		{
			if (!who)
				return;

			if (me->Attack(who, false))
				SetCombatMovement(false);
		}

		void Reset() override
		{
			Initialize();

			hasBlinked = false;
		}

		void EnterEvadeMode(EvadeReason why) override
		{
			CustomAI::EnterEvadeMode(why);

			RFTPhases phase = (RFTPhases)instance->GetData(DATA_SCENARIO_PHASE);
			switch (phase)
			{
				case RFTPhases::Standards_Valided:
					instance->SetData(EVENT_BACK_TO_SENDER, 0U);
					break;
				default:
					break;
			}
		}

		void OnSuccessfulSpellCast(SpellInfo const* spell) override
		{
			switch (spell->Id)
			{
				case SPELL_SUMMON_WATER_ELEMENTALS:
					for (uint8 i = 0; i < ELEMENTALS_SIZE; ++i)
					{
						if (Creature* elemental = me->GetMap()->SummonCreature(NPC_WATER_ELEMENTAL, ElementalsPoint[i].spawn))
							elemental->CastSpell(elemental, SPELL_WATER_BOSS_ENTRANCE);
					}
					break;
				case SPELL_RING_OF_ICE:
					if (!hasBlinked)
						break;
					scheduler.Schedule(1s, [this](TaskContext /*blink*/)
					{
						me->CastStop();
						me->CastSpell(beforeBlink, SPELL_BLINK, true);
						hasBlinked = false;
					});
					break;
			}
		}

		void DamageTaken(Unit* attacker, uint32& damage) override
		{
			if (HealthBelowPct(10))
				damage = 0;
		}

		void JustEngagedWith(Unit* who) override
		{
			scheduler
				.Schedule(5ms, [this](TaskContext frostbolt)
				{
					DoCastVictim(SPELL_FROSTBOLT);
					frostbolt.Repeat(3s);
				})
				.Schedule(2s, [this](TaskContext frigid_shard)
				{
					if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
						DoCast(target, SPELL_FRIGID_SHARD);
					frigid_shard.Repeat(5s, 8s);
				})
				.Schedule(8s, [this](TaskContext comet_barrage)
				{
					DoCast(SPELL_COMET_BARRAGE);
					comet_barrage.Repeat(12s, 14s);
				})
				.Schedule(14s, [this](TaskContext blink)
				{
					if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
					{
						if (me->IsWithinDist(target, 15.f))
						{
							me->CastStop();
							DoCast(SPELL_RING_OF_ICE);
						}
						else
						{
							beforeBlink = me->GetPosition();

							Position dest = GetRandomPosition(target->GetPosition(), 8.f);
							me->UpdateGroundPositionZ(dest.GetPositionX(), dest.GetPositionY(), dest.m_positionZ);
							me->CastStop();
							me->CastSpell(dest, SPELL_BLINK, true);

							scheduler.Schedule(1s, [this](TaskContext /*ring_of_ice*/)
							{
								me->CastStop();
								DoCast(SPELL_RING_OF_ICE);
							});

							hasBlinked = true;
						}
					}

					blink.Repeat(1min);
				});
		}

		bool CanAIAttack(Unit const* who) const override
		{
			return true;
		}

		void MovementInform(uint32 type, uint32 id) override
		{
			switch (id)
			{
				case MOVEMENT_INFO_POINT_01:
					me->SetStandState(UNIT_STAND_STATE_KNEEL);
					scheduler.Schedule(2s, [this](TaskContext context)
					{
						switch (context.GetRepeatCounter())
						{
							case 0:
								me->SetStandState(UNIT_STAND_STATE_STAND);
								context.Repeat(2s);
								break;
							case 1:
                                if (Creature* warlord = instance->GetCreature(DATA_ROKNAH_WARLORD))
                                {
                                    if (!warlord->IsWithinDist(warlord, 8.f))
                                        me->SetWalk(false);

                                    me->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_02, warlord, 1.3f);
                                }
								break;
						}
					});
					break;
				case MOVEMENT_INFO_POINT_02:
					me->HandleEmoteCommand(EMOTE_ONESHOT_CASTSTRONG);
					scheduler.Schedule(1s, [this](TaskContext /*context*/)
					{
						if (Creature* warlord = instance->GetCreature(DATA_ROKNAH_WARLORD))
						{
							warlord->SetRespawnTime(5 * MINUTE * IN_MILLISECONDS);
							warlord->SetRespawnDelay(5 * MINUTE * IN_MILLISECONDS);
							warlord->KillSelf();

							instance->DoSendScenarioEvent(EVENT_WARLORD_ROKNAH_SLAIN);
						}
					});
					break;
			}
		}

		void MoveInLineOfSight(Unit* who) override
		{
			ScriptedAI::MoveInLineOfSight(who);

			if (me->IsEngaged())
				return;

			if (who->GetTypeId() != TYPEID_PLAYER)
				return;

			if (Player* player = who->ToPlayer())
			{
				if (player->IsGameMaster())
					return;

				if (player->IsFriendlyTo(me) && player->IsWithinDist(me, distance))
				{
					RFTPhases phase = (RFTPhases)instance->GetData(DATA_SCENARIO_PHASE);
					switch (phase)
					{
						case RFTPhases::FindJaina_Isle:
							instance->DoSendScenarioEvent(EVENT_FIND_JAINA_01);
							break;
						case RFTPhases::FindJaina_Crater:
							instance->SetData(EVENT_FIND_JAINA_02, 0U);
							break;
						default:
							break;
					}
				}
			}
		}
	};

	CreatureAI* GetAI(Creature* creature) const override
	{
		return GetRuinsOfTheramoreAI<npc_jaina_ruinsAI>(creature);
	}
};

class event_ruins_warlock : public CreatureScript
{
    public:
    event_ruins_warlock() : CreatureScript("event_ruins_warlock")
    {
    }

    struct event_ruins_warlockAI : public NullCreatureAI
    {
        event_ruins_warlockAI(Creature* creature) : NullCreatureAI(creature), launched(false)
        {
            instance = creature->GetInstanceScript();
        }

        TaskScheduler scheduler;
        InstanceScript* instance;
        bool launched;

        enum Misc
        {
            // Groups
            GROUP_WARLOCK,
            GROUP_HAG,

            // NPCs
            NPC_FELHUNTER               = 146795,

            // Spells
            SPELL_SHADOW_CHANNELING     = 323849,
            SPELL_SHADOWY_TEAR          = 219107,
            SPELL_FEL_DISSOLVE_IN       = 237765,
            SPELL_POLYMORPH             = 236663,
        };

        void DoAction(int32 param) override
        {
            if (param == 1)
            {
                Creature* warlock = GetClosestCreatureWithEntry(me, NPC_ROKNAH_FELCASTER, 30.f);
                Creature* dummy = GetClosestCreatureWithEntry(me, NPC_INVISIBLE_STALKER, 30.f);
                if (warlock && dummy)
                {
                    if (warlock->IsEngaged())
                    {
                        scheduler.CancelGroup(GROUP_WARLOCK);
                        DoAction(1U);
                        return;
                    }

                    if (!dummy->HasAura(SPELL_SHADOWY_TEAR)) dummy->AddAura(SPELL_SHADOWY_TEAR, dummy);

                    scheduler
                        .Schedule(3s, 5s, GROUP_WARLOCK,[this, warlock, dummy](TaskContext context)
                        {
                            switch (context.GetRepeatCounter())
                            {
                                case 0:
                                    warlock->CastSpell(warlock, SPELL_SHADOW_CHANNELING);
                                    context.Repeat(3s);
                                    break;
                                case 1:
                                    warlock->CastStop();
                                    if (Creature* felhunter = warlock->SummonCreature(NPC_FELHUNTER, dummy->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, 20s))
                                    {
                                        felhunter->CastSpell(felhunter, SPELL_FEL_DISSOLVE_IN);
                                        felhunter->SetMaxHealth(warlock->CountPctFromMaxHealth(80));
                                        felhunter->SetFullHealth();
                                        felhunter->SetOwnerGUID(warlock->GetGUID());
                                        felhunter->SetFaction(warlock->GetFaction());
                                        felhunter->SetImmuneToAll(true);

                                        felhunter->GetMotionMaster()->Clear();
                                        felhunter->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_NONE, FelhunterPath01, FELHUNTER_PATH_01);
                                    }
                                    context.Repeat(5s, 14s);
                                    break;
                                case 2:
                                    scheduler.CancelGroup(GROUP_WARLOCK);
                                    DoAction(1U);
                                    break;
                            }
                        });
                }
            }
            else if (param == 2)
            {
                Creature* loa = GetClosestCreatureWithEntry(me, NPC_ROKNAH_LOA_SINGER, 30.f);
                Creature* hag = GetClosestCreatureWithEntry(me, NPC_ROKNAH_HAG, 30.f);
                Creature* grunt = GetClosestCreatureWithEntry(me, NPC_ROKNAH_GRUNT, 30.f);
                if (loa && hag && grunt)
                {
                    if (loa->IsEngaged() || hag->IsEngaged() || grunt->IsEngaged())
                    {
                        scheduler.CancelGroup(GROUP_HAG);
                        DoAction(2U);
                        return;
                    }

                    scheduler
                        .Schedule(2s, GROUP_HAG, [hag, grunt, loa, this](TaskContext context)
                        {
                            switch (context.GetRepeatCounter())
                            {
                                case 0:
                                    hag->SetFacingToObject(grunt);
                                    context.Repeat(1s);
                                    break;
                                case 1:
                                    hag->CastSpell(grunt, SPELL_POLYMORPH);
                                    context.Repeat(1s);
                                    break;
                                case 2:
                                    loa->SetFacingToObject(grunt);
                                    hag->HandleEmoteCommand(EMOTE_ONESHOT_LAUGH);
                                    context.Repeat(31s);
                                    break;
                                case 3:
                                    loa->AI()->EnterEvadeMode();
                                    hag->AI()->EnterEvadeMode();
                                    grunt->AI()->EnterEvadeMode();
                                    context.Repeat(25s, 60s);
                                    break;
                                case 4:
                                    scheduler.CancelGroup(GROUP_HAG);
                                    DoAction(2U);
                                    break;
                            }
                        });
                }
            }
        }

        void Reset() override
        {
            scheduler.CancelAll();
        }

        void UpdateAI(uint32 diff) override
        {
            if (!launched)
            {
                DoAction(1U);
                DoAction(2U);

                launched = true;
            }

            scheduler.Update(diff);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetRuinsOfTheramoreAI<event_ruins_warlockAI>(creature);
    }
};

void AddSC_ruins_of_theramore()
{
	new npc_jaina_ruins();
    new event_ruins_warlock();
}
