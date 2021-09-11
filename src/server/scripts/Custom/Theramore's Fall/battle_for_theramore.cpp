#include "boost/range/algorithm/reverse.hpp"
#include "ScriptMgr.h"
#include "battle_for_theramore.h"
#include "ObjectAccessor.h"
#include "MotionMaster.h"
#include "GameObject.h"
#include "Scenario.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "SpellHistory.h"
#include "InstanceScript.h"
#include "KillRewarder.h"
#include "Log.h"

class npc_jaina_theramore : public CreatureScript
{
public:
    npc_jaina_theramore() : CreatureScript("npc_jaina_theramore") { }

    struct npc_jaina_theramoreAI: public ScriptedAI
    {
        enum class Phases
        {
            Normal,
            Started,
            Preparation,
            Battle
        };

        enum Spells
        {
            SPELL_TELEPORT_DUMMY        = 51347,
            SPELL_CLOSE_PORTAL          = 203542,
            SPELL_MAGIC_QUILL           = 171980,
            SPELL_ARCANE_CANALISATION   = 288451,
            SPELL_VANISH                = 342048,
            SPELL_MASS_TELEPORT         = 60516,
        };

        npc_jaina_theramoreAI(Creature* creature) : ScriptedAI(creature), eventId(1), archmagesIndex(0)
        {
            phase = Phases::Normal;
            tervosh = nullptr;
            kinndy = nullptr;
            kalecgos = nullptr;
            pained = nullptr;
            perith = nullptr;
            officer = nullptr;

            Initialize();
        }

        void Initialize()
        {
            instance = me->GetInstanceScript();
        }

        EventMap events;
        uint8 eventId;
        uint8 archmagesIndex;
        Phases phase;
        InstanceScript* instance;

        Creature* tervosh;
        Creature* kinndy;
        Creature* kalecgos;
        Creature* pained;
        Creature* perith;
        Creature* officer;
        Creature* rhonin;
        Creature* vereesa;
        Creature* thalen;
        Creature* hedric;
        std::vector<Creature*> archmages;

        void Reset() override
        {
            events.Reset();
            Initialize();
        }

        void OnSuccessfulSpellCast(SpellInfo const* spell) override
        {
            if (spell->Id == SPELL_MASS_TELEPORT)
            {
                kinndy->CastSpell(kinndy, SPELL_TELEPORT_DUMMY);
                tervosh->CastSpell(tervosh, SPELL_TELEPORT_DUMMY);
                kalecgos->CastSpell(kalecgos, SPELL_TELEPORT_DUMMY);
                hedric->CastSpell(hedric, SPELL_TELEPORT_DUMMY);
                rhonin->CastSpell(hedric, SPELL_TELEPORT_DUMMY);
                vereesa->CastSpell(vereesa, SPELL_TELEPORT_DUMMY);
                thalen->CastSpell(thalen, SPELL_TELEPORT_DUMMY);
                for (Creature* archmage : archmages)
                    archmage->CastSpell(archmage, SPELL_TELEPORT_DUMMY);

                events.ScheduleEvent(90, 1800ms);
            }
        }

        void SetData(uint32 id, uint32 value) override
        {
            if (id == 100)
            {
                me->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);

                ClosePortal(DATA_PORTAL_TO_STORMWIND);
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
                    case 70:
                        instance->DoSendScenarioEvent(EVENT_FIND_JAINA);
                        instance->DoSendScenarioEvent(EVENT_LOCALIZE_THE_FOCUSING_IRIS);
                        instance->DoSendScenarioEvent(EVENT_WAITING_SOMETHING_HAPPENING);
                        break;
                }

                events.ScheduleEvent(value, 1s);
            }
            else if (id == DATA_SCENARIO_PHASE)
            {
                phase = (Phases)value;
            }
        }

        void MovementInform(uint32 /*type*/, uint32 id) override
        {
            switch (id)
            {
                case 0:
                    instance->DoSendScenarioEvent(EVENT_LOCALIZE_THE_FOCUSING_IRIS);
                    instance->SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::Timed);
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

            if (who->IsFriendlyTo(me) && who->IsWithinDist(me, 4.f))
            {
                switch (phase)
                {
                    case Phases::Preparation:
                        phase = Phases::Battle;
                        instance->DoSendScenarioEvent(EVENT_PREPARING_THE_DEFENSES);
                        instance->SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::Battle);
                        events.ScheduleEvent(71, 5s);
                        break;
                    case Phases::Normal:
                        phase = Phases::Started;
                        Talk(me, SAY_REUNION_1);
                        instance->DoSendScenarioEvent(EVENT_FIND_JAINA);
                        SearchCreatures();
                        tervosh->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                        kinndy->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                        kalecgos->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                        me->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                        events.ScheduleEvent(1, 5s);
                        break;
                }
            }
        }

        void DoAction(int32 param) override
        {
            events.ScheduleEvent(param, 1s);
        }

        void UpdateAI(uint32 diff) override
        {
            if (phase == Phases::Started || phase == Phases::Battle)
            {
                events.Update(diff);
                switch (eventId = events.ExecuteEvent())
                {
                    // Localize Focusing Iris
                    #pragma region LOCALIZE_FOCUSING_IRIS

                    case 1:
                        ClosePortal(DATA_PORTAL_TO_STORMWIND);
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

                    // The Unknow Tauren
                    #pragma region THE_UNKNOW_TAUREN

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
                        pained->GetMotionMaster()->MovePoint(1, PainedPoint01, true, PainedPoint01.GetOrientation());
                        Next(2s);
                        break;
                    case 38:
                        Talk(pained, SAY_WARN_11);
                        officer->GetMotionMaster()->MoveCloserAndStop(1, me, 3.0f);
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
                        officer->GetMotionMaster()->MoveSmoothPath(2, OfficerPath01, OFFICER_PATH_01, true);
                        officer->DespawnOrUnsummon(15s);
                        perith->GetMotionMaster()->MoveCloserAndStop(0, me, 3.0f);
                        Next(3s);
                        break;
                    case 42:
                        Talk(perith, SAY_WARN_14);
                        perith->SetTarget(me->GetGUID());
                        SetTarget(perith);
                        Next(5s);
                        break;
                    case 43:
                        Talk(me, SAY_WARN_15);
                        Next(4s);
                        break;
                    case 44:
                        Talk(perith, SAY_WARN_16);
                        Next(10s);
                        break;
                    case 45:
                        Talk(perith, SAY_WARN_17);
                        Next(10s);
                        break;
                    case 46:
                        Talk(perith, SAY_WARN_18);
                        Next(10s);
                        break;
                    case 47:
                        Talk(me, SAY_WARN_19);
                        Next(5s);
                        break;
                    case 48:
                        Talk(perith, SAY_WARN_20);
                        Next(5s);
                        break;
                    case 49:
                        Talk(me, SAY_WARN_21);
                        Next(2s);
                        break;
                    case 50:
                        Talk(perith, SAY_WARN_22);
                        Next(12s);
                        break;
                    case 51:
                        Talk(perith, SAY_WARN_23);
                        Next(8s);
                        break;
                    case 52:
                        Talk(me, SAY_WARN_24);
                        Next(10s);
                        break;
                    case 53:
                        Talk(perith, SAY_WARN_25);
                        Next(11s);
                        break;
                    case 54:
                        Talk(perith, SAY_WARN_26);
                        Next(3s);
                        break;
                    case 55:
                        me->SetTarget(ObjectGuid::Empty);
                        me->SetFacingTo(3.33f);
                        Next(2s);
                        break;
                    case 56:
                        Talk(me, SAY_WARN_27);
                        DoCast(SPELL_ARCANE_CANALISATION);
                        if (Creature* quill = me->SummonCreature(NPC_INVISIBLE_STALKER, QuillPoint01, TEMPSUMMON_TIMED_DESPAWN, 10s))
                            quill->AddAura(SPELL_MAGIC_QUILL, quill);
                        Next(10s);
                        break;
                    case 57:
                        me->RemoveAurasDueToSpell(SPELL_ARCANE_CANALISATION);
                        me->RemoveUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                        me->SetFacingToObject(perith);
                        Next(2s);
                        break;
                    case 58:
                        Talk(me, SAY_WARN_28);
                        Next(3s);
                        break;
                    case 59:
                        Talk(perith, SAY_WARN_29);
                        Next(5s);
                        break;
                    case 60:
                        Talk(perith, SAY_WARN_30);
                        Next(8s);
                        break;
                    case 61:
                        Talk(me, SAY_WARN_31);
                        Next(5s);
                        break;
                    case 62:
                        Talk(perith, SAY_WARN_32);
                        Next(4s);
                        break;
                    case 63:
                        Talk(me, SAY_WARN_33);
                        Next(5s);
                        break;
                    case 64:
                        Talk(perith, SAY_WARN_34);
                        Next(4s);
                        break;
                    case 65:
                        ClearTarget();
                        pained->RemoveUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                        perith->GetMotionMaster()->MoveSmoothPath(1, OfficerPath01, OFFICER_PATH_01, true);
                        perith->DespawnOrUnsummon(15s);
                        Next(5s);
                        break;
                    case 66:
                        Talk(me, SAY_WARN_35);
                        me->SetFacingToObject(pained);
                        pained->SetFacingToObject(me);
                        Next(5s);
                        break;
                    case 67:
                        Talk(pained, SAY_WARN_36);
                        Next(3s);
                        break;
                    case 68:
                        Talk(me, SAY_WARN_37);
                        Next(3s);
                        break;
                    case 69:
                        me->SetFacingTo(0.39f);
                        pained->GetMotionMaster()->MoveSmoothPath(2, KinndyPath01, KINNDY_PATH_01, true);
                        break;
                    case 70:
                        instance->DoSendScenarioEvent(EVENT_UNKNOWN_TAUREN);
                        instance->SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::Evacuation);
                        kinndy->SetVisible(true);
                        kinndy->GetMotionMaster()->MoveSmoothPath(2, KinndyPath02, KINNDY_PATH_02, true);
                        tervosh->SetVisible(true);
                        tervosh->GetMotionMaster()->MoveSmoothPath(2, TervoshPath03, TERVOSH_PATH_03, true);
                        break;

                    #pragma endregion

                    // Preparing the defenses
                    #pragma region PREPARING_THE_DEFENSES

                    case 71:
                        instance->DoCastSpellOnPlayers(SPELL_CAMERA_SHAKE_VOLCANO);
                        hedric = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_HEDRIC_EVENCANE));
                        hedric->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                        hedric->GetMotionMaster()->Clear();
                        hedric->GetMotionMaster()->MoveIdle();
                        hedric->NearTeleportTo(HedricPoint01);
                        hedric->PlayDirectSound(SOUND_FEARFUL_CROWD);
                        Talk(hedric, SAY_PRE_BATTLE_1);
                        Next(3s);
                        break;
                    case 72:
                        Talk(me, SAY_PRE_BATTLE_2);
                        hedric->GetMotionMaster()->MoveSmoothPath(0, HedricPath01, HEDRIC_PATH_01);
                        Next(2s);
                        break;
                    case 73:
                        Talk(hedric, SAY_PRE_BATTLE_3);
                        Next(3s);
                        break;
                    case 74:
                        Talk(me, SAY_PRE_BATTLE_4);
                        Next(4s);
                        break;
                    case 75:
                        me->SummonGameObject(GOB_PORTAL_TO_DALARAN, PortalPoint01, QuaternionData::QuaternionData(), 0);
                        Next(1s);
                        break;
                    case 76:
                        hedric->SetWalk(true);
                        hedric->GetMotionMaster()->MovePoint(0, HedricPoint02, true, HedricPoint02.GetOrientation());
                        Next(1s);
                        break;
                    case 77:
                        if (archmagesIndex >= ARCHMAGES_LOCATION)
                        {
                            Next(2s);
                        }
                        else if (Creature* archmage = me->SummonCreature(archmagesLocation[archmagesIndex].entry, PortalPoint01))
                        {
                            archmage->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                            archmage->SetSheath(SHEATH_STATE_UNARMED);
                            archmage->SetWalk(true);
                            archmage->GetMotionMaster()->MovePoint(0, archmagesLocation[archmagesIndex].destination, true, archmagesLocation[archmagesIndex].destination.GetOrientation());
                            archmage->CastSpell(archmage, SPELL_TELEPORT_DUMMY);

                            switch (archmagesLocation[archmagesIndex].entry)
                            {
                                case NPC_RHONIN:
                                    rhonin = archmage;
                                    Talk(rhonin, SAY_PRE_BATTLE_5);
                                    break;
                                case NPC_VEREESA_WINDRUNNER:
                                    vereesa = archmage;
                                    break;
                                case NPC_THALEN_SONGWEAVER:
                                    thalen = archmage;
                                    break;
                                default:
                                    archmages.push_back(archmage);
                                    break;
                            }

                            archmagesIndex++;

                            events.ScheduleEvent(77, 800ms, 1s);
                        }
                        break;
                    case 78:
                        SetTarget(rhonin);
                        ClosePortal(DATA_PORTAL_TO_DALARAN);
                        Next(2s);
                        break;
                    case 79:
                        Talk(me, SAY_PRE_BATTLE_6);
                        SetTarget(me);
                        Next(11s);
                        break;
                    case 80:
                        Talk(me, SAY_PRE_BATTLE_7);
                        Next(9s);
                        break;
                    case 81:
                        Talk(thalen, SAY_PRE_BATTLE_8);
                        SetTarget(thalen);
                        Next(7s);
                        break;
                    case 82:
                        Talk(me, SAY_PRE_BATTLE_9);
                        SetTarget(me);
                        Next(3s);
                        break;
                    case 83:
                        Talk(me, SAY_PRE_BATTLE_10);
                        Next(7s);
                        break;
                    case 84:
                        Talk(rhonin, SAY_PRE_BATTLE_11);
                        SetTarget(rhonin);
                        Next(2s);
                        break;
                    case 85:
                        Talk(me, SAY_PRE_BATTLE_12);
                        SetTarget(tervosh);
                        Next(2s);
                        break;
                    case 86:
                        Talk(vereesa, SAY_PRE_BATTLE_13);
                        SetTarget(vereesa);
                        Next(2s);
                        break;
                    case 87:
                        vereesa->CastSpell(vereesa, SPELL_VANISH);
                        Talk(me, SAY_PRE_BATTLE_14);
                        SetTarget(me);
                        Next(2s);
                        break;
                    case 88:
                        Talk(me, SAY_PRE_BATTLE_15);
                        if (Player* player = me->SelectNearestPlayer(150.f))
                            me->SetFacingToObject(player);
                        vereesa->SetFaction(35);
                        vereesa->SetVisible(false);
                        Next(5s);
                        break;
                    case 89:
                        ClearTarget();
                        DoCast(SPELL_MASS_TELEPORT);
                        break;
                    case 90:
                        instance->HandleGameObject(instance->GetGuidData(DATA_MYSTIC_BARRIER_01), true);
                        instance->HandleGameObject(instance->GetGuidData(DATA_MYSTIC_BARRIER_02), true);
                        me->SetVisible(false);
                        kinndy->SetVisible(false);
                        tervosh->SetVisible(false);
                        kalecgos->SetVisible(false);
                        hedric->SetVisible(false);
                        rhonin->SetVisible(false);
                        vereesa->SetVisible(false);
                        thalen->SetVisible(false);
                        for (Creature* archmage : archmages)
                            archmage->SetVisible(false);
                        break;

                    #pragma endregion

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

        void SetTarget(Unit* creature)
        {
            ObjectGuid guid = creature->GetGUID();

            if (me->GetGUID() != guid) me->SetTarget(guid);

            if (Creature* kinndy = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_KINNDY_SPARKSHINE)))
            {
                if (kinndy->GetGUID() != guid) kinndy->SetTarget(guid);
            }

            if (Creature* tervosh = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_ARCHMAGE_TERVOSH)))
            {
                if (tervosh->GetGUID() != guid) tervosh->SetTarget(guid);
            }

            if (Creature* kalecgos = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_KALECGOS)))
            {
                if (kalecgos->GetGUID() != guid) kalecgos->SetTarget(guid);
            }

            if (Creature* pained = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_PAINED)))
            {
                if (pained->GetGUID() != guid) pained->SetTarget(guid);
            }

            if (Creature* perith = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_PERITH_STORMHOOVE)))
            {
                if (perith->GetGUID() != guid) perith->SetTarget(guid);
            }

            if (Creature* officer = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_THERAMORE_OFFICER)))
            {
                if (officer->GetGUID() != guid) officer->SetTarget(guid);
            }

            if (Creature* hedric = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_HEDRIC_EVENCANE)))
            {
                if (hedric->GetGUID() != guid) hedric->SetTarget(guid);
            }

            if (Creature* rhonin = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_RHONIN)))
            {
                if (rhonin->GetGUID() != guid) rhonin->SetTarget(guid);
            }

            if (Creature* vereesa = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_VEREESA_WINDRUNNER)))
            {
                if (vereesa->GetGUID() != guid) vereesa->SetTarget(guid);
            }

            if (Creature* thalen = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_THALEN_SONGWEAVER)))
            {
                if (thalen->GetGUID() != guid) thalen->SetTarget(guid);
            }

            if (!archmages.empty())
            {
                for (Creature* archmage : archmages)
                    if (archmage->GetGUID() != guid) archmage->SetTarget(guid);
            }
        }

        void ClearTarget()
        {
            me->SetTarget(ObjectGuid::Empty);

            if (Creature* kinndy = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_KINNDY_SPARKSHINE)))
                kinndy->SetTarget(ObjectGuid::Empty);

            if (Creature* tervosh = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_ARCHMAGE_TERVOSH)))
                tervosh->SetTarget(ObjectGuid::Empty);

            if (Creature* kalecgos = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_KALECGOS)))
                kalecgos->SetTarget(ObjectGuid::Empty);

            if (Creature* pained = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_PAINED)))
                pained->SetTarget(ObjectGuid::Empty);

            if (Creature* perith = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_PERITH_STORMHOOVE)))
                perith->SetTarget(ObjectGuid::Empty);

            if (Creature* officer = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_THERAMORE_OFFICER)))
                officer->SetTarget(ObjectGuid::Empty);

            if (Creature* hedric = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_HEDRIC_EVENCANE)))
                hedric->SetTarget(ObjectGuid::Empty);

            if (Creature* rhonin = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_RHONIN)))
                rhonin->SetTarget(ObjectGuid::Empty);

            if (Creature* vereesa = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_VEREESA_WINDRUNNER)))
                vereesa->SetTarget(ObjectGuid::Empty);

            if (Creature* thalen = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_THALEN_SONGWEAVER)))
                thalen->SetTarget(ObjectGuid::Empty);

            if (!archmages.empty())
            {
                for (Creature* archmage : archmages)
                    archmage->SetTarget(ObjectGuid::Empty);
            }
        }

        void ClosePortal(uint32 dataId)
        {
            if (GameObject* portal = ObjectAccessor::GetGameObject(*me, instance->GetGuidData(dataId)))
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

class npc_pained : public CreatureScript
{
    public:
    npc_pained() : CreatureScript("npc_pained")
    {
    }

    struct npc_painedAI : public ScriptedAI
    {
        npc_painedAI(Creature* creature) : ScriptedAI(creature)
        {
            instance = me->GetInstanceScript();
        }

        InstanceScript* instance;

        void MovementInform(uint32 /*type*/, uint32 id) override
        {
            switch (id)
            {
                case 2:
                    me->SetVisible(false);
                    if (Creature* jaina = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_JAINA_PROUDMOORE)))
                        jaina->AI()->DoAction(70);
                    break;
                default:
                    break;
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<npc_painedAI>(creature);
    }
};

class npc_theramore_citizen : public CreatureScript
{
    public:
    npc_theramore_citizen() : CreatureScript("npc_theramore_citizen")
    {
    }

    struct npc_theramore_citizenAI : public ScriptedAI
    {
        enum Misc
        {
            GOSSIP_MENU_DEFAULT             = 65000,
            NPC_THERAMORE_CITIZEN_CREDIT    = 500005
        };

        npc_theramore_citizenAI(Creature* creature) : ScriptedAI(creature)
        {
            instance = creature->GetInstanceScript();
        }

        TaskScheduler scheduler;
        InstanceScript* instance;

        void MovementInform(uint32 /*type*/, uint32 id) override
        {
            if (id == 0)
            {
                me->SetVisible(false);
            }
        }

        bool GossipHello(Player* player) override
        {
            player->PrepareGossipMenu(me, GOSSIP_MENU_DEFAULT, true);
            player->SendPreparedGossip(me);
            return true;
        }

        bool GossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
        {
            ClearGossipMenuFor(player);

            switch (gossipListId)
            {
                case 0:
                {
                    me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                    me->SetEmoteState(EMOTE_STATE_NONE);
                    scheduler.Schedule(5ms, [this, player](TaskContext context)
                    {
                        switch (context.GetRepeatCounter())
                        {
                            case 0:
                                me->SetTarget(player->GetGUID());
                                me->SetWalk(false);
                                context.Repeat(1s);
                                break;
                            case 1:
                                Talk(0);
                                context.Repeat(5s);
                                break;
                            case 2:
                                me->SetTarget(ObjectGuid::Empty);
                                if (Creature* stalker = GetClosestCreatureWithEntry(me, NPC_INVISIBLE_STALKER, 35.f))
                                    me->GetMotionMaster()->MovePoint(0, stalker->GetPosition());
                                context.Repeat(1s);
                                break;
                            case 3:
                                KillRewarder(player, me, false).Reward(NPC_THERAMORE_CITIZEN_CREDIT);
                                break;
                            default:
                                break;
                        }
                    });
                    break;
                }
            }

            CloseGossipMenuFor(player);
            return true;
        }

        void Reset() override
        {
            me->SetVisible(true);
            scheduler.CancelAll();
        }

        void UpdateAI(uint32 diff) override
        {
            scheduler.Update(diff);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<npc_theramore_citizenAI>(creature);
    }
};

class event_theramore_training : public CreatureScript
{
    public:
    event_theramore_training() : CreatureScript("event_theramore_training")
    {
    }

    struct event_theramore_trainingAI : public ScriptedAI
    {
        event_theramore_trainingAI(Creature* creature) : ScriptedAI(creature)
        {
            instance = creature->GetInstanceScript();
        }

        TaskScheduler scheduler;
        InstanceScript* instance;

        enum Misc
        {
            // NPCs
            NPC_TRAINING_DUMMY              = 87318,

            // Spells
            SPELL_FLASH_HEAL                = 314655,
            SPELL_HEAL                      = 332706,
            SPELL_POWER_WORD_SHIELD         = 318158,
            SPELL_ARCANE_PROJECTILES        = 5143,
            SPELL_EVOCATION                 = 243070,
            SPELL_SUPERNOVA                 = 157980,
            SPELL_ARCANE_BLAST              = 291316,
            SPELL_ARCANE_BARRAGE            = 291318,
        };

        void Initialize(Creature* creature, Emote emote, Creature* target)
        {
            uint64 health = creature->GetMaxHealth() * 0.3f;
            creature->SetEmoteState(emote);
            creature->SetRegenerateHealth(false);
            creature->SetHealth(health);

            if (target)
                creature->SetTarget(target->GetGUID());
        }

        void DoAction(int32 param) override
        {
            std::vector<Creature*> footmen;
            GetCreatureListWithEntryInGrid(footmen, me, NPC_THERAMORE_FOOTMAN, 15.f);
            if (footmen.empty())
                return;

            Creature* faithful = GetClosestCreatureWithEntry(me, NPC_THERAMORE_FAITHFUL, 15.f);
            Creature* arcanist = GetClosestCreatureWithEntry(me, NPC_THERAMORE_ARCANIST, 15.f);
            Creature* training = GetClosestCreatureWithEntry(me, NPC_TRAINING_DUMMY, 15.f);

            if (faithful && arcanist && training)
            {
                if (param == 2)
                {
                    footmen[0]->SetVisible(false);
                    footmen[1]->SetVisible(false);
                    faithful->SetVisible(false);
                    arcanist->SetVisible(false);
                    training->SetVisible(false);
                }
                else
                {
                    Initialize(footmen[0], EMOTE_STATE_ATTACK1H, footmen[1]);
                    Initialize(footmen[1], EMOTE_STATE_BLOCK_SHIELD, footmen[0]);

                    footmen[0]->SetReactState(REACT_PASSIVE);
                    footmen[1]->SetReactState(REACT_PASSIVE);
                    faithful->SetReactState(REACT_PASSIVE);
                    arcanist->SetReactState(REACT_PASSIVE);

                    training->SetImmuneToPC(true);

                    scheduler
                        .Schedule(1s, [faithful, footmen](TaskContext context)
                        {
                            Creature* victim = footmen[urand(0, 1)];
                            faithful->SetTarget(victim->GetGUID());
                            faithful->CastSpell(victim, RAND(SPELL_FLASH_HEAL, SPELL_HEAL, SPELL_POWER_WORD_SHIELD));
                            context.Repeat(8s);
                        })
                        .Schedule(1s, [footmen](TaskContext context)
                        {
                            if (!footmen[0]->HasAura(SPELL_POWER_WORD_SHIELD))
                                footmen[0]->DealDamage(footmen[1], footmen[0], urand(150, 200));

                            if (!footmen[1]->HasAura(SPELL_POWER_WORD_SHIELD))
                                footmen[1]->DealDamage(footmen[0], footmen[1], urand(150, 200));

                            context.Repeat(8s);
                        })
                        .Schedule(1s, [arcanist, training](TaskContext context)
                        {
                            if (arcanist->GetPowerPct(POWER_MANA) <= 20)
                            {
                                const SpellInfo* info = sSpellMgr->AssertSpellInfo(SPELL_EVOCATION, DIFFICULTY_NONE);

                                arcanist->CastSpell(arcanist, SPELL_EVOCATION);
                                arcanist->GetSpellHistory()->ResetCooldown(info->Id, true);
                                arcanist->GetSpellHistory()->RestoreCharge(info->ChargeCategoryId);

                                context.Repeat(7s);
                            }
                            else
                            {
                                uint32 spellId = SPELL_ARCANE_BLAST;
                                if (roll_chance_i(30))
                                {
                                    spellId = SPELL_ARCANE_PROJECTILES;
                                }
                                else if (roll_chance_i(40))
                                {
                                    spellId = SPELL_ARCANE_BARRAGE;
                                }
                                else if (roll_chance_i(20))
                                {
                                    spellId = SPELL_SUPERNOVA;
                                }

                                const SpellInfo* info = sSpellMgr->AssertSpellInfo(spellId, DIFFICULTY_NONE);
                                Milliseconds ms = Milliseconds(info->CalcCastTime());

                                arcanist->CastSpell(training, spellId);

                                arcanist->GetSpellHistory()->ResetCooldown(info->Id, true);
                                arcanist->GetSpellHistory()->RestoreCharge(info->ChargeCategoryId);

                                if (info->IsChanneled())
                                    ms = Milliseconds(info->CalcDuration(arcanist));

                                context.Repeat(ms + 500ms);
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
            scheduler.Update(diff);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<event_theramore_trainingAI>(creature);
    }
};

class event_theramore_faithful : public CreatureScript
{
    public:
    event_theramore_faithful() : CreatureScript("event_theramore_faithful")
    {
    }

    struct event_theramore_faithfulAI : public ScriptedAI
    {
        event_theramore_faithfulAI(Creature* creature) : ScriptedAI(creature)
        {
            instance = creature->GetInstanceScript();
        }

        TaskScheduler scheduler;
        InstanceScript* instance;

        enum Misc
        {
            // Spells
            SPELL_HOLY_CHANNELING           = 235056,
            SPELL_RENEW                     = 294342,
        };

        void DoAction(int32 param) override
        {
            std::vector<Creature*> citizens;
            GetCreatureListWithEntryInGrid(citizens, me, NPC_THERAMORE_CITIZEN_MALE, 5.f);
            GetCreatureListWithEntryInGrid(citizens, me, NPC_THERAMORE_CITIZEN_FEMALE, 5.f);
            if (citizens.empty())
                return;

            Creature* faithful = GetClosestCreatureWithEntry(me, NPC_THERAMORE_FAITHFUL, 5.f);

            if (param == 2)
            {
                if (citizens.empty())
                    return;

                for (Creature* citizen : citizens)
                    citizen->SetVisible(false);

                faithful->SetVisible(false);
            }
            else
            {
                if (faithful)
                {
                    for (Creature* citizen : citizens)
                        citizen->SetFacingToObject(faithful);

                    faithful->CastSpell(faithful, SPELL_HOLY_CHANNELING);

                    scheduler.Schedule(2s, [citizens](TaskContext context)
                    {
                        uint32 random = urand(0, citizens.size() - 1);
                        citizens[random]->AddAura(SPELL_RENEW, citizens[random]);
                        context.Repeat(5s, 8s);
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
            scheduler.Update(diff);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBattleForTheramoreAI<event_theramore_faithfulAI>(creature);
    }
};

void AddSC_battle_for_theramore()
{
    new npc_jaina_theramore();
    new npc_pained();
    new npc_theramore_citizen();
    new event_theramore_training();
    new event_theramore_faithful();
}
