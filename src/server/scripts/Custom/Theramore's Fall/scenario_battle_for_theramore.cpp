#include "TemporarySummon.h"
#include "GameObject.h"
#include "CriteriaHandler.h"
#include "Map.h"
#include "EventMap.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "InstanceScript.h"
#include "MotionMaster.h"
#include "Player.h"
#include "battle_for_theramore.h"

#define CREATURE_DATA_SIZE 15

const ObjectData creatureData[CREATURE_DATA_SIZE] =
{
    { NPC_JAINA_PROUDMOORE,     DATA_JAINA_PROUDMOORE       },
    { NPC_KINNDY_SPARKSHINE,    DATA_KINNDY_SPARKSHINE      },
    { NPC_KALECGOS,             DATA_KALECGOS               },
    { NPC_ARCHMAGE_TERVOSH,     DATA_ARCHMAGE_TERVOSH       },
    { NPC_PAINED,               DATA_PAINED                 },
    { NPC_PERITH_STORMHOOVE,    DATA_PERITH_STORMHOOVE      },
    { NPC_THERAMORE_OFFICER,    DATA_THERAMORE_OFFICER      },
    { NPC_HEDRIC_EVENCANE,      DATA_HEDRIC_EVENCANE        },
    { NPC_RHONIN,               DATA_RHONIN                 },
    { NPC_VEREESA_WINDRUNNER,   DATA_VEREESA_WINDRUNNER     },
    { NPC_THALEN_SONGWEAVER,    DATA_THALEN_SONGWEAVER      },
    { NPC_TARI_COGG,            DATA_TARI_COGG              },
    { NPC_AMARA_LEESON,         DATA_AMARA_LEESON           },
    { NPC_THADER_WINDERMERE,    DATA_THADER_WINDERMERE      },
    { 0,                        0                           }   // END
};

const ObjectData gameobjectData[] =
{
    { GOB_PORTAL_TO_STORMWIND,  DATA_PORTAL_TO_STORMWIND    },
    { GOB_PORTAL_TO_DALARAN,    DATA_PORTAL_TO_DALARAN      },
    { 0,                        0                           }   // END
};

const char* debugPhaseNames[] =
{
    "FindJaina",
    "TheCouncil",
    "Waiting",
    "TheUnknownTauren",
    "Evacuation",
    "ALittleHelp",
    "Preparation",
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
            phase(BFTPhases::FindJaina), eventId(1), archmagesIndex(0)
        {
            SetHeaders(DataHeader);
            LoadObjectData(creatureData, gameobjectData);
        }

        enum Spells
        {
            SPELL_TELEPORT_DUMMY        = 51347,
            SPELL_CLOSE_PORTAL          = 203542,
            SPELL_MAGIC_QUILL           = 171980,
            SPELL_ARCANE_CANALISATION   = 288451,
            SPELL_PORTAL_CHANNELING_01  = 286636,
            SPELL_PORTAL_CHANNELING_02  = 287432,
            SPELL_VANISH                = 342048,
            SPELL_MASS_TELEPORT         = 60516,
        };

        void OnCompletedCriteriaTree(CriteriaTree const* tree) override
        {
            switch (tree->ID)
            {
                case CRITERIA_TREE_EVACUATION:
                    SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::ALittleHelp);
                    if (Creature* jaina = GetJaina())
                    {
                        jaina->NearTeleportTo(JainaPoint02);
                        jaina->SetHomePosition(JainaPoint02);
                    }
                    if (Creature* tervosh = GetTervosh())
                    {
                        tervosh->NearTeleportTo(TervoshPoint01);
                        tervosh->SetHomePosition(TervoshPoint01);
                        tervosh->CastSpell(GetTervosh(), SPELL_COSMETIC_FIRE_LIGHT);
                    }
                    if (Creature* kinndy = GetKinndy())
                    {
                        kinndy->NearTeleportTo(KinndyPoint02);
                        kinndy->SetHomePosition(KinndyPoint02);
                    }
                    if (Creature* kalecgos = GetKalecgos())
                    {
                        kalecgos->NearTeleportTo(KalecgosPoint01);
                        kalecgos->SetHomePosition(KalecgosPoint01);
                        kalecgos->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                        kalecgos->RemoveAllAuras();
                    }
                    for (uint8 i = 0; i < ARCHMAGES_LOCATION; i++)
                    {
                        if (Creature* creature = GetCreature(archmagesLocation[i].dataId))
                        {
                            creature->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                            creature->SetVisible(false);
                            creature->NearTeleportTo(PortalPoint01);
                        }
                    }
                    break;
            }
        }

        void OnCreatureCreate(Creature* creature) override
        {
            InstanceScript::OnCreatureCreate(creature);

            switch (creature->GetEntry())
            {
                case NPC_EVENT_THERAMORE_TRAINING:
                case NPC_EVENT_THERAMORE_FAITHFUL:
                    dummies.push_back(creature);
                    break;
                case NPC_THERAMORE_CITIZEN_FEMALE:
                case NPC_THERAMORE_CITIZEN_MALE:
                    if (phase == BFTPhases::Evacuation)
                        break;
                    creature->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                    citizens.push_back(creature);
                    break;
                case NPC_ALLIANCE_PEASANT:
                    citizens.push_back(creature);
                    break;
            }
        }

        void OnGameObjectCreate(GameObject* go) override
        {
            InstanceScript::OnGameObjectCreate(go);

            switch (go->GetEntry())
            {
                case GOB_MYSTIC_BARRIER:
                    go->SetLootState(GO_NOT_READY);
                    go->SetFlags(GO_FLAG_NOT_SELECTABLE);
                    barriers.push_back(go);
                    break;
                case GOB_PORTAL_TO_DALARAN:
                case GOB_PORTAL_TO_STORMWIND:
                    go->SetLootState(GO_READY);
                    go->UseDoorOrButton();
                    go->SetFlags(GO_FLAG_NOT_SELECTABLE);
                    break;
                default:
                    break;
            }
        }

        uint32 GetData(uint32 dataId) const override
        {
            if (dataId == DATA_SCENARIO_PHASE)
                return (uint32)phase;
            return 0;
        }

        void SetData(uint32 dataId, uint32 value) override
        {
            if (dataId == DATA_SCENARIO_PHASE)
            {
                printf("Change scenario phase: %s", debugPhaseNames[value]);

                phase = (BFTPhases)value;
                switch (phase)
                {
                    // 1ère phase : Localiser Jaina
                    case BFTPhases::FindJaina:
                    // 6e phase : Un coup de pouce
                    case BFTPhases::ALittleHelp:
                    default:
                        break;
                    // 2d phase : Le Conseil
                    // Joue les mini évènements pour décorer
                    case BFTPhases::TheCouncil:
                        ClosePortal(DATA_PORTAL_TO_STORMWIND);
                        GetTervosh()->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                        GetKinndy()->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                        GetKalecgos()->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                        if (Creature* jaina = GetCreature(DATA_JAINA_PROUDMOORE))
                        {
                            jaina->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                            Talk(jaina, SAY_REUNION_1);
                            SetTarget(jaina);
                        }
                        for (Creature* dummy : dummies)
                            dummy->AI()->DoAction(1U);
                        events.ScheduleEvent(1, 2s);
                        break;
                    // 3e phase : Attendre l'action
                    case BFTPhases::Waiting:
                        events.ScheduleEvent(24, 10s);
                        break;
                    // 4e phase : Un Mystérieux Tauren
                    case BFTPhases::TheUnknownTauren:
                        for (uint8 i = 0; i < PERITH_LOCATION; i++)
                        {
                            if (Creature* creature = GetCreature(perithLocation[i].dataId))
                                creature->NearTeleportTo(perithLocation[i].position);
                        }
                        GetOfficer()->SetSheath(SHEATH_STATE_UNARMED);
                        GetOfficer()->SetEmoteState(EMOTE_STATE_WAGUARDSTAND01);
                        events.ScheduleEvent(25, 1s);
                        break;
                    // 5e phase : L'Evacuation
                    case BFTPhases::Evacuation:
                        for (Creature* citizen : citizens)
                        {
                            if (Creature* faithful = citizen->FindNearestCreature(NPC_THERAMORE_FAITHFUL, 10.f))
                                continue;
                            if (citizen->GetEntry() == NPC_ALLIANCE_PEASANT)
                                continue;
                            citizen->SetNpcFlags(UNIT_NPC_FLAG_GOSSIP);
                        }
                        if (Creature* kinndy = GetKinndy())
                        {
                            kinndy->SetVisible(true);
                            kinndy->GetMotionMaster()->MoveSmoothPath(2, KinndyPath02, KINNDY_PATH_02, true);
                        }
                        if (Creature* tervosh = GetTervosh())
                        {
                            tervosh->SetVisible(true);
                            tervosh->GetMotionMaster()->MoveSmoothPath(2, TervoshPath03, TERVOSH_PATH_03, true);
                        }
                        break;
                    // 7e phase : Préparer la bataille
                    // Supprime les mini évènements avant la bataille
                    case BFTPhases::Preparation:
                        if (Creature* jaina = GetCreature(DATA_JAINA_PROUDMOORE))
                        {
                            for (uint8 i = 0; i < FIRE_LOCATION; i++)
                            {
                                const Position pos = FireLocation[i];
                                if (Creature* trigger = jaina->SummonTrigger(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), 0.0f, 0U))
                                    trigger->CastSpell(trigger, SPELL_COSMETIC_LARGE_FIRE);
                            }
                        }
                        for (Creature* dummy : dummies)
                            dummy->AI()->DoAction(2U);
                        for (Creature* citizen : citizens)
                        {
                            citizen->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                            if (roll_chance_i(60))
                                citizen->SetVisible(false);
                            else
                            {
                                citizen->SetRegenerateHealth(false);
                                citizen->SetStandState(UNIT_STAND_STATE_DEAD);
                                citizen->AddUnitFlag2(UNIT_FLAG2_FEIGN_DEATH);
                                citizen->AddUnitFlag2(UNIT_FLAG2_PLAY_DEATH_ANIM);
                            }
                        }
                        events.ScheduleEvent(71, 1s);
                        break;
                }
            }
        }

        void Update(uint32 diff) override
        {
            events.Update(diff);
            switch (eventId = events.ExecuteEvent())
            {
                // The Council
                #pragma region THE_COUNCIL

                case 1:
                    if (Creature* tervosh = GetTervosh())
                    {
                        tervosh->SetControlled(false, UNIT_STATE_ROOT);
                        tervosh->SetEmoteState(EMOTE_STAND_STATE_NONE);
                        tervosh->GetMotionMaster()->MoveSmoothPath(0, TervoshPath01, TERVOSH_PATH_01, true);
                    }
                    Next(5s);
                    break;
                case 2:
                    if (Creature* kinndy = GetKinndy())
                    {
                        kinndy->SetControlled(false, UNIT_STATE_ROOT);
                        kinndy->SetWalk(true);
                        kinndy->GetMotionMaster()->MovePoint(0, KinndyPoint01, true, 1.09f);
                    }
                    Next(6s);
                    break;
                case 3:
                    Talk(GetKinndy(), SAY_REUNION_2);
                    SetTarget(GetKinndy());
                    Next(11s);
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
                    Talk(GetKalecgos(), SAY_REUNION_7);
                    SetTarget(GetKalecgos());
                    Next(6s);
                    break;
                case 9:
                    Talk(GetKalecgos(), SAY_REUNION_8);
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
                    Talk(GetKalecgos(), SAY_REUNION_11);
                    SetTarget(GetKalecgos());
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
                    Talk(GetKalecgos(), SAY_REUNION_14);
                    SetTarget(GetKalecgos());
                    Next(7s);
                    break;
                case 17:
                    Talk(GetJaina(), SAY_REUNION_15);
                    SetTarget(GetJaina());
                    Next(4s);
                    break;
                case 18:
                    Talk(GetKalecgos(), SAY_REUNION_16);
                    SetTarget(GetKalecgos());
                    Next(4s);
                    break;
                case 19:
                    Talk(GetKalecgos(), SAY_REUNION_17);
                    Next(4s);
                    break;
                case 20:
                    ClearTarget();
                    if (Creature* kalecgos = GetKalecgos())
                    {
                        kalecgos->SetSpeedRate(MOVE_WALK, 1.6f);
                        kalecgos->SetControlled(false, UNIT_STATE_ROOT);
                        kalecgos->GetMotionMaster()->MoveSmoothPath(0, KalecgosPath01, KALECGOS_PATH_01, true);
                    }
                    Next(2s);
                    break;
                case 21:
                    GetTervosh()->GetMotionMaster()->MoveSmoothPath(1, TervoshPath02, TERVOSH_PATH_02, true);
                    Next(5s);
                    break;
                case 22:
                    GetKinndy()->GetMotionMaster()->MoveSmoothPath(1, KinndyPath01, KINNDY_PATH_01, true);
                    Next(6s);
                    break;
                case 23:
                    GetJaina()->SetWalk(true);
                    GetJaina()->GetMotionMaster()->MovePoint(0, JainaPoint01, true, JainaPoint01.GetOrientation());
                    break;

                #pragma endregion

                // Waiting
                #pragma region WAITING

                case 24:
                    DoSendScenarioEvent(EVENT_WAITING);
                    SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::TheUnknownTauren);
                    break;

                #pragma endregion

                // The Unknown Tauren
                #pragma region THE_UNKNOWN_TAUREN

                case 25:
                    for (uint8 i = 0; i < PERITH_LOCATION; i++)
                    {
                        if (Creature* creature = GetCreature(perithLocation[i].dataId))
                        {
                            creature->SetWalk(true);
                            creature->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                            creature->GetMotionMaster()->MovePoint(0, perithLocation[i].destination, true, perithLocation[i].destination.GetOrientation());
                        }
                    }
                    Next(7s);
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
                    GetPained()->GetMotionMaster()->MoveCloserAndStop(0, GetJaina(), 1.8f);
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
                    GetPained()->GetMotionMaster()->MovePoint(1, PainedPoint01, true, PainedPoint01.GetOrientation());
                    Next(2s);
                    break;
                case 39:
                    GetOfficer()->GetMotionMaster()->MoveCloserAndStop(1, GetJaina(), 3.0f);
                    Next(2s);
                    break;
                case 40:
                    Talk(GetOfficer(), SAY_WARN_11);
                    SetTarget(GetOfficer());
                    Next(3s);
                    break;
                case 41:
                    Talk(GetJaina(), SAY_WARN_12);
                    SetTarget(GetJaina());
                    Next(5s);
                    break;
                case 42:
                    ClearTarget();
                    Talk(GetPained(), SAY_WARN_13);
                    if (Creature* officer = GetOfficer())
                    {
                        officer->GetMotionMaster()->MoveSmoothPath(2, OfficerPath01, OFFICER_PATH_01, true);
                        officer->DespawnOrUnsummon(15s);
                    }
                    GetPerith()->GetMotionMaster()->MoveCloserAndStop(0, GetJaina(), 3.0f);
                    Next(3s);
                    break;
                case 43:
                    Talk(GetPerith(), SAY_WARN_14);
                    GetPerith()->SetTarget(GetJaina()->GetGUID());
                    SetTarget(GetPerith());
                    Next(5s);
                    break;
                case 44:
                    Talk(GetJaina(), SAY_WARN_15);
                    Next(4s);
                    break;
                case 45:
                    Talk(GetPerith(), SAY_WARN_16);
                    Next(10s);
                    break;
                case 46:
                    Talk(GetPerith(), SAY_WARN_17);
                    Next(10s);
                    break;
                case 47:
                    Talk(GetPerith(), SAY_WARN_18);
                    Next(10s);
                    break;
                case 48:
                    Talk(GetJaina(), SAY_WARN_19);
                    Next(5s);
                    break;
                case 49:
                    Talk(GetPerith(), SAY_WARN_20);
                    Next(5s);
                    break;
                case 50:
                    Talk(GetJaina(), SAY_WARN_21);
                    Next(2s);
                    break;
                case 51:
                    Talk(GetPerith(), SAY_WARN_22);
                    Next(12s);
                    break;
                case 52:
                    Talk(GetPerith(), SAY_WARN_23);
                    Next(8s);
                    break;
                case 53:
                    Talk(GetJaina(), SAY_WARN_24);
                    Next(10s);
                    break;
                case 54:
                    Talk(GetPerith(), SAY_WARN_25);
                    Next(11s);
                    break;
                case 55:
                    Talk(GetJaina(), SAY_WARN_26);
                    Next(3s);
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
                    Talk(GetJaina(), SAY_WARN_27);
                    GetJaina()->AI()->DoCast(SPELL_ARCANE_CANALISATION);
                    if (Creature* quill = GetJaina()->SummonCreature(NPC_INVISIBLE_STALKER, QuillPoint01, TEMPSUMMON_TIMED_DESPAWN, 10s))
                        quill->AddAura(SPELL_MAGIC_QUILL, quill);
                    Next(10s);
                    break;
                case 58:
                    if (Creature* jaina = GetJaina())
                    {
                        jaina->RemoveAurasDueToSpell(SPELL_ARCANE_CANALISATION);
                        jaina->RemoveUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                        jaina->SetFacingToObject(GetPerith());
                    }
                    Next(2s);
                    break;
                case 59:
                    Talk(GetJaina(), SAY_WARN_28);
                    Next(3s);
                    break;
                case 60:
                    Talk(GetPerith(), SAY_WARN_29);
                    Next(5s);
                    break;
                case 61:
                    Talk(GetPerith(), SAY_WARN_30);
                    Next(8s);
                    break;
                case 62:
                    Talk(GetJaina(), SAY_WARN_31);
                    Next(5s);
                    break;
                case 63:
                    Talk(GetPerith(), SAY_WARN_32);
                    Next(4s);
                    break;
                case 64:
                    Talk(GetJaina(), SAY_WARN_33);
                    Next(5s);
                    break;
                case 65:
                    Talk(GetPerith(), SAY_WARN_34);
                    Next(4s);
                    break;
                case 66:
                    ClearTarget();
                    GetPained()->RemoveUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                    if (Creature* perith = GetPerith())
                    {
                        perith->GetMotionMaster()->MoveSmoothPath(1, OfficerPath01, OFFICER_PATH_01, true);
                        perith->DespawnOrUnsummon(15s);
                    }
                    Next(5s);
                    break;
                case 67:
                    Talk(GetJaina(), SAY_WARN_35);
                    GetJaina()->SetFacingToObject(GetPained());
                    GetPained()->SetFacingToObject(GetJaina());
                    Next(5s);
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
                    GetPained()->GetMotionMaster()->MoveSmoothPath(2, KinndyPath01, KINNDY_PATH_01, true);
                    break;

                #pragma endregion

                // A Little Help
                #pragma region A_LITTLE_HELP

                case 71:
                    DoCastSpellOnPlayers(SPELL_CAMERA_SHAKE_VOLCANO);
                    if (Creature* hedric = GetHedric())
                    {
                        hedric->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                        hedric->GetMotionMaster()->Clear();
                        hedric->GetMotionMaster()->MoveIdle();
                        hedric->NearTeleportTo(HedricPoint01);
                        hedric->PlayDirectSound(SOUND_FEARFUL_CROWD);
                        Talk(hedric, SAY_PRE_BATTLE_1);
                    }
                    Next(3s);
                    break;
                case 72:
                    Talk(GetJaina(), SAY_PRE_BATTLE_2);
                    GetHedric()->GetMotionMaster()->MoveSmoothPath(0, HedricPath01, HEDRIC_PATH_01);
                    Next(2s);
                    break;
                case 73:
                    Talk(GetHedric(), SAY_PRE_BATTLE_3);
                    Next(3s);
                    break;
                case 74:
                    Talk(GetJaina(), SAY_PRE_BATTLE_4);
                    Next(4s);
                    break;
                case 75:
                    GetJaina()->SummonGameObject(GOB_PORTAL_TO_DALARAN, PortalPoint01, QuaternionData::QuaternionData(), 0);
                    Next(500ms);
                    break;
                case 76:
                    GetHedric()->SetWalk(true);
                    GetHedric()->GetMotionMaster()->MovePoint(0, HedricPoint02, true, HedricPoint02.GetOrientation());
                    Next(500ms);
                    break;
                case 77:
                    if (archmagesIndex >= ARCHMAGES_LOCATION)
                    {
                        Next(2s);
                    }
                    else if (Creature* creature = GetCreature(archmagesLocation[archmagesIndex].dataId))
                    {
                        creature->AddUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                        creature->SetSheath(SHEATH_STATE_UNARMED);
                        creature->SetVisible(true);
                        creature->CastSpell(creature, SPELL_TELEPORT_DUMMY);
                        creature->SetWalk(true);
                        creature->GetMotionMaster()->MovePoint(0, archmagesLocation[archmagesIndex].destination, true, archmagesLocation[archmagesIndex].destination.GetOrientation());

                        archmagesIndex++;

                        events.Repeat(800ms, 1s);
                    }
                    break;
                case 78:
                    SetTarget(GetRhonin());
                    ClosePortal(DATA_PORTAL_TO_DALARAN);
                    Next(2s);
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
                    Next(3s);
                    break;
                case 83:
                    Talk(GetJaina(), SAY_PRE_BATTLE_10);
                    Next(7s);
                    break;
                case 84:
                    Talk(GetRhonin(), SAY_PRE_BATTLE_11);
                    SetTarget(GetRhonin());
                    Next(2s);
                    break;
                case 85:
                    Talk(GetJaina(), SAY_PRE_BATTLE_12);
                    SetTarget(GetTervosh());
                    Next(2s);
                    break;
                case 86:
                    Talk(GetVereesa(), SAY_PRE_BATTLE_13);
                    SetTarget(GetVereesa());
                    Next(2s);
                    break;
                case 87:
                    if (Creature* vereesa = GetVereesa())
                        vereesa->CastSpell(vereesa, SPELL_VANISH);
                    Talk(GetJaina(), SAY_PRE_BATTLE_14);
                    SetTarget(GetJaina());
                    Next(2s);
                    break;
                case 88:
                    if (Creature* jaina = GetJaina())
                    {
                        Talk(jaina, SAY_PRE_BATTLE_15);
                        if (Player* player = jaina->SelectNearestPlayer(150.f))
                        {
                            jaina->RemoveUnitFlag2(UNIT_FLAG2_DISABLE_TURN);
                            jaina->SetFacingToObject(player);
                        }
                    }
                    if (Creature* vereesa = GetVereesa())
                    {
                        vereesa->SetFaction(FACTION_FRIENDLY);
                        vereesa->SetVisible(false);
                    }
                    Next(5s);
                    break;
                case 89:
                    ClearTarget();
                    GetJaina()->AI()->DoCast(SPELL_MASS_TELEPORT);
                    Next(4600ms);
                    break;
                case 90:
                    GetKalecgos()->SetVisible(false);
                    for (GameObject* barrier : barriers)
                    {
                        barrier->SetLootState(GO_READY);
                        barrier->UseDoorOrButton();
                    }
                    for (uint8 i = 0; i < ACTORS_RELOCATION; i++)
                    {
                        if (Creature* creature = GetCreature(actorsRelocation[i].dataId))
                        {
                            switch (creature->GetEntry())
                            {
                                case NPC_AMARA_LEESON:
                                case NPC_TARI_COGG:
                                    creature->CastSpell(creature, SPELL_PORTAL_CHANNELING_01);
                                    break;
                                case NPC_THADER_WINDERMERE:
                                    creature->AddNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                                    creature->CastSpell(creature, SPELL_PORTAL_CHANNELING_01);
                                    break;
                                case NPC_THALEN_SONGWEAVER:
                                    creature->CastSpell(creature, SPELL_PORTAL_CHANNELING_02);
                                    break;
                                case NPC_RHONIN:
                                    creature->AddNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                                    break;
                            }

                            creature->NearTeleportTo(actorsRelocation[i].destination);
                            creature->SetHomePosition(actorsRelocation[i].destination);
                            creature->SetSheath(SHEATH_STATE_MELEE);
                        }
                    }
                    Next(600ms);
                    break;
                case 91:
                    TeleportPlayers();
                    break;

                #pragma endregion
            }
        }

        EventMap events;
        BFTPhases phase;
        uint32 eventId;
        uint8 archmagesIndex;
        std::vector<GameObject*> barriers;
        std::vector<Creature*> citizens;
        std::vector<Creature*> dummies;

        Creature* GetJaina()    { return GetCreature(DATA_JAINA_PROUDMOORE); }
        Creature* GetKinndy()   { return GetCreature(DATA_KINNDY_SPARKSHINE); }
        Creature* GetTervosh()  { return GetCreature(DATA_ARCHMAGE_TERVOSH); }
        Creature* GetKalecgos() { return GetCreature(DATA_KALECGOS); }
        Creature* GetPained()   { return GetCreature(DATA_PAINED); }
        Creature* GetPerith()   { return GetCreature(DATA_PERITH_STORMHOOVE); }
        Creature* GetOfficer()  { return GetCreature(DATA_THERAMORE_OFFICER); }
        Creature* GetHedric()   { return GetCreature(DATA_HEDRIC_EVENCANE); }
        Creature* GetRhonin()   { return GetCreature(DATA_RHONIN); }
        Creature* GetVereesa()  { return GetCreature(DATA_VEREESA_WINDRUNNER); }
        Creature* GetThalen()   { return GetCreature(DATA_THALEN_SONGWEAVER); }

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
            for (uint8 i = 0; i < CREATURE_DATA_SIZE; i++)
            {
                if (Creature* creature = GetCreature(creatureData[i].type))
                {
                    if (creature->GetGUID() == guid)
                        continue;

                    creature->SetTarget(guid);
                }
            }
        }

        void ClearTarget()
        {
            for (uint8 i = 0; i < CREATURE_DATA_SIZE; i++)
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
                if (Creature* special = portal->SummonTrigger(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), pos.GetOrientation(), 5 * IN_MILLISECONDS))
                {
                    special->CastSpell(special, SPELL_CLOSE_PORTAL, args);
                }
            }
        }

        void TeleportPlayers()
        {
            Map::PlayerList const& players = instance->GetPlayers();

            if (players.isEmpty())
                return;

            for (Map::PlayerList::const_iterator i = players.begin(); i != players.end(); ++i)
            {
                if (Player* player = i->GetSource())
                {
                    if (Creature* jaina = GetCreature(DATA_JAINA_PROUDMOORE))
                        player->NearTeleportTo(jaina->GetRandomNearPosition(5.f));
                }
            }
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new scenario_battle_for_theramore_InstanceScript(map);
    }
};

void AddSC_scenario_battle_for_theramore()
{
    new scenario_battle_for_theramore();
}
