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
    Introduction,
};

enum Spells
{
    SPELL_CLOSE_PORTAL = 203542
};

enum Talks
{
    SAY_CONVO_1         = 0,
    SAY_CONVO_2         = 0,
    SAY_CONVO_3         = 1,
    SAY_CONVO_4         = 1,
    SAY_CONVO_5         = 2,
    SAY_CONVO_6         = 0,
    SAY_CONVO_7         = 0,
    SAY_CONVO_8         = 1,
    SAY_CONVO_9         = 1,
    SAY_CONVO_9_BIS     = 2,
    SAY_CONVO_10        = 3,
    SAY_CONVO_11        = 2,
    SAY_CONVO_12        = 4,
    SAY_CONVO_13        = 3,
    SAY_CONVO_14        = 3,
    SAY_CONVO_15        = 5,
    SAY_CONVO_16        = 4,
    SAY_CONVO_17        = 5,
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

        void Reset() override
        {
            events.Reset();
            Initialize();
        }

        void MovementInform(uint32 /*type*/, uint32 id) override
        {
            switch (id)
            {
                case 0:
                    instance->DoSendScenarioEvent(EVENT_LOCALIZE_THE_FOCUSING_IRIS);
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

            if (phase == Phases::Normal && who->IsFriendlyTo(me) && who->IsWithinDist(me, 4.f))
            {
                Talk(me, SAY_CONVO_1);

                instance->DoSendScenarioEvent(EVENT_FIND_JAINA);

                tervosh = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_ARCHMAGE_TERVOSH));
                kinndy = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_KINNDY_SPARKSHINE));
                kalecgos = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_KALECGOS));

                phase = Phases::Introduction;

                tervosh->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                kinndy->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                kalecgos->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                me->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);

                events.ScheduleEvent(1, 5s);
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (phase == Phases::Introduction)
            {
                events.Update(diff);
                switch (eventId = events.ExecuteEvent())
                {
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
                        Talk(kinndy, SAY_CONVO_2);
                        SetTarget(kinndy);
                        Next(11s);
                        break;
                    case 4:
                        Talk(me, SAY_CONVO_3);
                        SetTarget(me);
                        Next(12s);
                        break;
                    case 5:
                        Talk(kinndy, SAY_CONVO_4);
                        SetTarget(kinndy);
                        Next(6s);
                        break;
                    case 6:
                        Talk(me, SAY_CONVO_5);
                        SetTarget(me);
                        Next(8s);
                        break;
                    case 7:
                        Talk(tervosh, SAY_CONVO_6);
                        SetTarget(tervosh);
                        Next(8s);
                        break;
                    case 8:
                        Talk(kalecgos, SAY_CONVO_7);
                        SetTarget(kalecgos);
                        Next(6s);
                        break;
                    case 9:
                        Talk(kalecgos, SAY_CONVO_8);
                        Next(9s);
                        break;
                    case 10:
                        Talk(tervosh, SAY_CONVO_9);
                        Next(1s);
                        break;
                    case 11:
                        Talk(kinndy, SAY_CONVO_9_BIS);
                        Next(4s);
                        break;
                    case 12:
                        Talk(me, SAY_CONVO_10);
                        SetTarget(me);
                        Next(6s);
                        break;
                    case 13:
                        Talk(kalecgos, SAY_CONVO_11);
                        SetTarget(kalecgos);
                        Next(4s);
                        break;
                    case 14:
                        Talk(me, SAY_CONVO_12);
                        SetTarget(me);
                        Next(6s);
                        break;
                    case 15:
                        Talk(kinndy, SAY_CONVO_13);
                        SetTarget(kinndy);
                        Next(6s);
                        break;
                    case 16:
                        Talk(kalecgos, SAY_CONVO_14);
                        SetTarget(kalecgos);
                        Next(7s);
                        break;
                    case 17:
                        Talk(me, SAY_CONVO_15);
                        SetTarget(me);
                        Next(4s);
                        break;
                    case 18:
                        Talk(kalecgos, SAY_CONVO_16);
                        SetTarget(kalecgos);
                        Next(4s);
                        break;
                    case 19:
                        Talk(kalecgos, SAY_CONVO_17);
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

            if (me->GetGUID() != guid)
                me->SetTarget(guid);
        }

        void ClearTarget()
        {
            kinndy->SetTarget(ObjectGuid::Empty);
            tervosh->SetTarget(ObjectGuid::Empty);
            kalecgos->SetTarget(ObjectGuid::Empty);
            me->SetTarget(ObjectGuid::Empty);
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
