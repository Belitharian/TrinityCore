#include "ScriptMgr.h"
#include "battle_for_theramore.h"
#include "ObjectAccessor.h"
#include "MotionMaster.h"
#include "GameObject.h"
#include "Scenario.h"
#include "ScriptedCreature.h"
#include "InstanceScript.h"
#include "Log.h"

enum class Phases
{
    Normal,
    Started,
};

enum Spells
{
    SPELL_CLOSE_PORTAL = 203542
};

class npc_jaina_theramore : public CreatureScript
{
public:
    npc_jaina_theramore() : CreatureScript("npc_jaina_theramore") { }

    struct npc_jaina_theramoreAI: public ScriptedAI
    {
        npc_jaina_theramoreAI(Creature* creature) : ScriptedAI(creature), eventId(1)
        {
            phase = Phases::Normal;
            Initialize();
        }

        void Initialize()
        {
            instance = me->GetInstanceScript();
        }

        EventMap events;
        uint8 eventId;
        Phases phase;
        InstanceScript* instance;

        Creature* tervosh;
        Creature* kinndy;
        Creature* kalecgos;
        Creature* pained;
        Creature* perith;
        Creature* officer;

        void Reset() override
        {
            events.Reset();
            Initialize();
        }

        void SetData(uint32 id, uint32 value) override
        {
            if (id == 100)
            {
                me->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);

                ClosePortal();
                SearchCreatures();

                phase = Phases::Started;

                switch (value)
                {
                    case 23:
                        instance->DoSendScenarioEvent(EVENT_FIND_JAINA);
                        kalecgos->NearTeleportTo(KalecgosPath01[KALECGOS_PATH_01 - 1]);
                        tervosh->NearTeleportTo(TervoshPath02[TERVOSH_PATH_02 - 1]);
                        kinndy->NearTeleportTo(KinndyPath01[KINNDY_PATH_01 - 1]);
                        break;
                }

                events.ScheduleEvent(value, 1s);
            }
        }

        void MovementInform(uint32 /*type*/, uint32 id) override
        {
            switch (id)
            {
                case 0:
                    instance->DoSendScenarioEvent(EVENT_LOCALIZE_THE_FOCUSING_IRIS);
                    instance->SetData(DATA_SCENARIO_WAIT_EVENT_01, 0U);
                    break;
                default:
                    break;
            }
        }

        void MoveInLineOfSight(Unit* who) override
        {
            ScriptedAI::MoveInLineOfSight(who);

            if (who->GetTypeId() != TYPEID_PLAYER)
                return;

            if (Player* player = who->ToPlayer())
            {
                if (player->IsGameMaster())
                    return;
            }

            if (phase == Phases::Normal && who->IsFriendlyTo(me) && who->IsWithinDist(me, 4.f))
            {
                Talk(me, SAY_REUNION_1);

                instance->DoSendScenarioEvent(EVENT_FIND_JAINA);

                SearchCreatures();

                phase = Phases::Started;

                tervosh->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                kinndy->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                kalecgos->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                me->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);

                events.ScheduleEvent(1, 5s);
            }
        }

        void DoAction(int32 param) override
        {
            events.ScheduleEvent(param, 1s);
        }

        void UpdateAI(uint32 diff) override
        {
            if (phase == Phases::Started)
            {
                events.Update(diff);
                switch (eventId = events.ExecuteEvent())
                {
                    // Localize Focusing Iris
                    #pragma region LOCALIZE_FOCUSING_IRIS

                    case 1:
                        ClosePortal();
                        tervosh->SetControlled(false, UNIT_STATE_ROOT);
                        tervosh->SetEmoteState(EMOTE_STAND_STATE_NONE);
                        tervosh->GetMotionMaster()->MoveSmoothPath(0, TervoshPath01, TERVOSH_PATH_01, true);
                        Next(5s);
                        break;
                    case 2:
                        kinndy->SetControlled(false, UNIT_STATE_ROOT);
                        kinndy->SetWalk(true);
                        kinndy->GetMotionMaster()->MovePoint(0, KinndyPoint01, true, 1.09f);
                        Next(6s);
                        break;
                    case 3:
                        Talk(kinndy, SAY_REUNION_2);
                        SetTarget(kinndy);
                        Next(11s);
                        break;
                    case 4:
                        Talk(me, SAY_REUNION_3);
                        SetTarget(me);
                        Next(12s);
                        break;
                    case 5:
                        Talk(kinndy, SAY_REUNION_4);
                        SetTarget(kinndy);
                        Next(6s);
                        break;
                    case 6:
                        Talk(me, SAY_REUNION_5);
                        SetTarget(me);
                        Next(8s);
                        break;
                    case 7:
                        Talk(tervosh, SAY_REUNION_6);
                        SetTarget(tervosh);
                        Next(8s);
                        break;
                    case 8:
                        Talk(kalecgos, SAY_REUNION_7);
                        SetTarget(kalecgos);
                        Next(6s);
                        break;
                    case 9:
                        Talk(kalecgos, SAY_REUNION_8);
                        Next(9s);
                        break;
                    case 10:
                        Talk(tervosh, SAY_REUNION_9);
                        Next(1s);
                        break;
                    case 11:
                        Talk(kinndy, SAY_REUNION_9_BIS);
                        Next(4s);
                        break;
                    case 12:
                        Talk(me, SAY_REUNION_10);
                        SetTarget(me);
                        Next(6s);
                        break;
                    case 13:
                        Talk(kalecgos, SAY_REUNION_11);
                        SetTarget(kalecgos);
                        Next(4s);
                        break;
                    case 14:
                        Talk(me, SAY_REUNION_12);
                        SetTarget(me);
                        Next(6s);
                        break;
                    case 15:
                        Talk(kinndy, SAY_REUNION_13);
                        SetTarget(kinndy);
                        Next(6s);
                        break;
                    case 16:
                        Talk(kalecgos, SAY_REUNION_14);
                        SetTarget(kalecgos);
                        Next(7s);
                        break;
                    case 17:
                        Talk(me, SAY_REUNION_15);
                        SetTarget(me);
                        Next(4s);
                        break;
                    case 18:
                        Talk(kalecgos, SAY_REUNION_16);
                        SetTarget(kalecgos);
                        Next(4s);
                        break;
                    case 19:
                        Talk(kalecgos, SAY_REUNION_17);
                        Next(4s);
                        break;
                    case 20:
                        ClearTarget();
                        kalecgos->SetSpeedRate(MOVE_WALK, 1.6f);
                        kalecgos->SetControlled(false, UNIT_STATE_ROOT);
                        kalecgos->GetMotionMaster()->MoveSmoothPath(0, KalecgosPath01, KALECGOS_PATH_01, true);
                        Next(2s);
                        break;
                    case 21:
                        tervosh->GetMotionMaster()->MoveSmoothPath(1, TervoshPath02, TERVOSH_PATH_02, true);
                        Next(5s);
                        break;
                    case 22:
                        kinndy->GetMotionMaster()->MoveSmoothPath(1, KinndyPath01, KINNDY_PATH_01, true);
                        Next(6s);
                        break;
                    case 23:
                        me->SetWalk(true);
                        me->GetMotionMaster()->MovePoint(0, JainaPoint01, true, JainaPoint01.GetOrientation());
                        break;

                    #pragma endregion

                    case 24:
                        for (uint8 i = 0; i < PERITH_LOCATION; i++)
                        {
                            if (Creature* summon = DoSummon(perithLocation[i].entry, perithLocation[i].position))
                            {
                                switch (perithLocation[i].entry)
                                {
                                    case NPC_PAINED:
                                        pained = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_PAINED));
                                        break;
                                    case NPC_PERITH_STORMHOOVE:
                                        perith = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_PERITH_STORMHOOVE));
                                        break;
                                    case NPC_THERAMORE_OFFICER:
                                        officer = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_THERAMORE_OFFICER));
                                        break;
                                    default:
                                        break;
                                }

                                summon->SetWalk(true);
                                summon->GetMotionMaster()->MovePoint(0, perithLocation[i].destination, true, perithLocation[i].destination.GetOrientation());
                            }
                        }
                        Next(7s);
                        break;
                    case 25:
                        pained->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                        perith->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                        officer->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                        me->SetTarget(perith->GetGUID());
                        Next(2s);
                        break;
                    case 26:
                        officer->SetSheath(SHEATH_STATE_UNARMED);
                        officer->SetEmoteState(EMOTE_STATE_WAGUARDSTAND01);
                        Talk(pained, SAY_WARN_1);
                        SetTarget(pained);
                        Next(1s);
                        break;
                    case 27:
                        Talk(me, SAY_WARN_2);
                        SetTarget(me);
                        Next(1s);
                        break;
                    case 28:
                        Talk(pained, SAY_WARN_3);
                        SetTarget(pained);
                        Next(6s);
                        break;
                    case 29:
                        Talk(pained, SAY_WARN_4);
                        SetTarget(pained);
                        Next(7s);
                        break;
                    case 30:
                        Talk(me, SAY_WARN_5);
                        SetTarget(me);
                        Next(6s);
                        break;
                    case 31:
                        Talk(pained, SAY_WARN_6);
                        SetTarget(pained);
                        Next(10s);
                        break;
                    case 32:
                        ClearTarget();
                        pained->GetMotionMaster()->MoveCloserAndStop(0, me, 1.8f);
                        Next(2s);
                        break;
                    case 33:
                        SetTarget(me);
                        pained->SetEmoteState(EMOTE_STATE_USE_STANDING);
                        Next(1s);
                        break;
                    case 34:
                        me->SetEmoteState(EMOTE_STATE_USE_STANDING);
                        pained->SetEmoteState(EMOTE_STATE_NONE);
                        Talk(pained, SAY_WARN_7);
                        Next(3s);
                        break;
                    case 35:
                        me->SetEmoteState(EMOTE_STATE_NONE);
                        Talk(me, SAY_WARN_8);
                        Next(4s);
                        break;
                    case 36:
                        Talk(me, SAY_WARN_9);
                        SetTarget(me);
                        Next(2s);
                        break;
                    case 37:
                        Talk(me, SAY_WARN_10);
                        ClearTarget();
                        pained->GetMotionMaster()->MovePoint(0, PainedPoint01, true, PainedPoint01.GetOrientation());
                        Next(2s);
                        break;
                    case 38:
                        Talk(pained, SAY_WARN_11);
                        officer->GetMotionMaster()->MoveCloserAndStop(0, me, 3.0f);
                        Next(2s);
                        break;
                    case 39:
                        Talk(officer, SAY_WARN_12);
                        SetTarget(officer);
                        Next(3s);
                        break;
                    case 40:
                        Talk(me, SAY_WARN_13);
                        SetTarget(me);
                        Next(5s);
                        break;
                    case 41:
                        ClearTarget();
                        officer->GetMotionMaster()->MoveSmoothPath(0, OfficerPath01, OFFICER_PATH_01, true);
                        officer->DespawnOrUnsummon(10s);
                        perith->GetMotionMaster()->MoveCloserAndStop(0, me, 3.0f);
                        Next(3s);
                        break;
                    case 42:
                        Talk(perith, SAY_WARN_14);
                        SetTarget(perith);
                        Next(5s);
                        break;
                    default:
                        break;
                }
            }

            //Return since we have no target
            if (!UpdateVictim())
                return;

            events.Update(diff);

            DoMeleeAttackIfReady();
        }

        void Talk(Creature* creature, uint8 id)
        {
            creature->AI()->Talk(id);
        }

        void Next(const Milliseconds& time)
        {
            eventId++;
            events.ScheduleEvent(eventId, time);
        }

        void SetTarget(Creature* creature)
        {
            ObjectGuid guid = creature->GetGUID();

            if (kinndy->GetGUID() != guid)
                kinndy->SetTarget(guid);

            if (tervosh->GetGUID() != guid)
                tervosh->SetTarget(guid);

            if (kalecgos->GetGUID() != guid)
                kalecgos->SetTarget(guid);

            if (pained && pained->GetGUID() != guid)
                pained->SetTarget(guid);

            if (perith && perith->GetGUID() != guid)
                perith->SetTarget(guid);

            if (officer && officer->GetGUID() != guid)
                officer->SetTarget(guid);

            if (me->GetGUID() != guid)
                me->SetTarget(guid);
        }

        void ClearTarget()
        {
            kinndy->SetTarget(ObjectGuid::Empty);
            tervosh->SetTarget(ObjectGuid::Empty);
            kalecgos->SetTarget(ObjectGuid::Empty);
            me->SetTarget(ObjectGuid::Empty);

            if (pained)
                pained->SetTarget(ObjectGuid::Empty);

            if (perith)
                perith->SetTarget(ObjectGuid::Empty);

            if (officer)
                officer->SetTarget(ObjectGuid::Empty);
        }

        void ClosePortal()
        {
            if (GameObject* portal = ObjectAccessor::GetGameObject(*me, instance->GetGuidData(DATA_PORTAL_TO_STORMWIND)))
            {
                portal->Delete();

                CastSpellExtraArgs args;
                args.SetTriggerFlags(TRIGGERED_CAST_DIRECTLY);

                if (Creature* fx = DoSummon(NPC_INVISIBLE_STALKER, portal->GetPosition(), 5 * IN_MILLISECONDS, TEMPSUMMON_TIMED_DESPAWN))
                    fx->CastSpell(fx, SPELL_CLOSE_PORTAL, args);
            }
        }

        void SearchCreatures()
        {
            tervosh = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_ARCHMAGE_TERVOSH));
            kinndy = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_KINNDY_SPARKSHINE));
            kalecgos = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_KALECGOS));
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<npc_jaina_theramoreAI>(creature);
    }
};

void AddSC_battle_for_theramore()
{
    new npc_jaina_theramore();
}
