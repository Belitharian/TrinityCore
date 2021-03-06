#include "ScriptMgr.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "GameObject.h"
#include "theramore.h"
#include "MoveSpline.h"
#include "WaypointManager.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "Weather.h"
#include "GridNotifiersImpl.h"
#include "Log.h"
#include "Group.h"

#define KINNDY_PATH_SIZE             6
#define ADEN_PATH_SIZE               7
#define KALECGOS_PATH_SIZE           8
#define TERVOSH_PATH_SIZE            9
#define JAINA_PATH_SIZE             14
#define JAINA_PATH_1_SIZE           43
#define KINNDY_PATH_1_SIZE          21

#define KALECGOS_CIRCLE_RADIUS      95.f

// Pathes
#pragma region EVENT_PRE_BATTLE

Position TervoshPath[TERVOSH_PATH_SIZE]
{
    { -3742.71f, -4440.83f, 30.55f, 5.00f },
    { -3741.09f, -4445.29f, 30.55f, 4.53f },
    { -3742.46f, -4450.14f, 32.38f, 4.38f },
    { -3744.65f, -4453.23f, 33.74f, 3.99f },
    { -3747.46f, -4455.76f, 35.08f, 3.45f },
    { -3751.00f, -4456.81f, 36.39f, 3.36f },
    { -3755.11f, -4457.08f, 37.85f, 3.15f },
    { -3758.88f, -4455.95f, 37.99f, 2.80f },
    { -3762.25f, -4453.26f, 37.99f, 2.36f }
};

Position KinndyPath[KINNDY_PATH_SIZE]
{
    { -3747.36f, -4434.46f, 30.55f, 1.02f },
    { -3750.84f, -4432.38f, 30.55f, 2.65f },
    { -3754.72f, -4432.04f, 32.16f, 3.12f },
    { -3758.73f, -4433.16f, 33.62f, 3.57f },
    { -3762.08f, -4435.29f, 35.02f, 3.92f },
    { -3765.23f, -4440.14f, 35.21f, 4.21f }
};

Position const AdenPath[ADEN_PATH_SIZE]
{
    { -3717.79f, -4522.24f, 25.82f, 5.16f },
    { -3714.91f, -4528.24f, 25.82f, 5.16f },
    { -3713.09f, -4532.02f, 25.82f, 5.16f },
    { -3711.22f, -4535.91f, 25.82f, 5.16f },
    { -3710.03f, -4538.38f, 25.82f, 5.16f },
    { -3712.85f, -4539.80f, 25.82f, 3.60f },
    { -3716.85f, -4541.81f, 25.82f, 3.60f }
};

Position const JainaPostBattlePath[JAINA_PATH_SIZE]
{
    { -3669.20f, -4380.69f,  9.51f, 3.89f },
    { -3672.86f, -4393.39f, 10.60f, 4.71f },
    { -3672.87f, -4398.85f, 10.64f, 4.71f },
    { -3672.87f, -4403.85f, 10.68f, 4.71f },
    { -3672.87f, -4407.60f, 10.64f, 4.71f },
    { -3671.40f, -4411.18f, 10.61f, 0.79f },
    { -3668.17f, -4412.39f, 10.73f, 6.16f },
    { -3662.70f, -4413.18f, 10.67f, 6.17f },
    { -3657.73f, -4413.71f, 10.48f, 6.17f },
    { -3652.12f, -4413.99f, 10.25f, 6.18f },
    { -3647.20f, -4414.85f, 10.06f, 5.99f },
    { -3642.43f, -4416.33f,  9.88f, 5.96f },
    { -3637.79f, -4418.19f,  9.71f, 5.79f },
    { -3633.32f, -4420.42f,  9.69f, 5.82f }
};

Position const JainaWoundedPath[JAINA_PATH_1_SIZE]
{
    { -3637.05f, -4416.78f,  9.67f, 0.21f },
    { -3640.47f, -4416.59f,  9.80f, 6.04f },
    { -3642.88f, -4415.93f,  9.89f, 6.02f },
    { -3645.31f, -4415.34f, 10.00f, 6.07f },
    { -3649.00f, -4414.67f, 10.13f, 6.10f },
    { -3652.70f, -4414.02f, 10.27f, 6.10f },
    { -3655.16f, -4413.59f, 10.38f, 6.10f },
    { -3658.86f, -4413.01f, 10.52f, 6.16f },
    { -3661.35f, -4412.73f, 10.61f, 6.18f },
    { -3665.09f, -4412.54f, 10.75f, 6.28f },
    { -3667.37f, -4412.95f, 10.82f, 0.32f },
    { -3669.74f, -4414.60f, 10.86f, 0.84f },
    { -3671.13f, -4417.32f, 11.03f, 1.24f },
    { -3671.80f, -4420.67f, 11.11f, 1.43f },
    { -3672.18f, -4424.40f, 11.18f, 1.52f },
    { -3672.29f, -4426.90f, 11.22f, 1.52f },
    { -3672.46f, -4430.64f, 11.28f, 1.52f },
    { -3672.57f, -4433.14f, 11.35f, 1.52f },
    { -3672.74f, -4436.89f, 11.35f, 1.52f },
    { -3672.85f, -4439.38f, 11.36f, 1.52f },
    { -3673.02f, -4443.13f, 11.38f, 1.52f },
    { -3673.18f, -4446.88f, 11.39f, 1.52f },
    { -3673.35f, -4450.62f, 11.40f, 1.52f },
    { -3673.52f, -4454.37f, 11.42f, 1.52f },
    { -3673.83f, -4458.10f, 11.36f, 1.43f },
    { -3674.56f, -4461.78f, 11.35f, 1.32f },
    { -3675.83f, -4465.16f, 11.41f, 1.05f },
    { -3677.86f, -4468.11f, 11.46f, 0.94f },
    { -3680.00f, -4471.19f, 11.54f, 1.01f },
    { -3681.56f, -4474.36f, 11.56f, 1.25f },
    { -3681.85f, -4477.52f, 11.48f, 1.66f },
    { -3680.67f, -4480.93f, 11.30f, 2.04f },
    { -3678.74f, -4484.14f, 11.08f, 2.14f },
    { -3676.05f, -4488.35f, 10.87f, 2.13f },
    { -3673.41f, -4492.59f, 10.69f, 2.11f },
    { -3671.56f, -4495.86f, 10.56f, 2.02f },
    { -3670.55f, -4500.86f, 10.40f, 1.69f },
    { -3670.10f, -4504.58f, 10.30f, 1.69f },
    { -3669.46f, -4509.53f, 10.15f, 1.81f },
    { -3668.26f, -4511.59f, 10.09f, 2.44f },
    { -3665.60f, -4512.60f, 10.03f, 2.97f },
    { -3663.23f, -4512.82f, 9.977f, 3.07f },
    { -3655.96f, -4513.32f, 9.463f, 3.07f }
};

Position const KinndyWoundedPath[KINNDY_PATH_1_SIZE]
{
    { -3638.61f, -4409.38f,  9.80f, 4.85f },
    { -3638.47f, -4417.07f,  9.72f, 4.69f },
    { -3646.66f, -4415.24f, 10.05f, 2.98f },
    { -3654.98f, -4414.06f, 10.37f, 3.01f },
    { -3663.34f, -4413.29f, 10.69f, 3.07f },
    { -3670.61f, -4414.45f, 10.81f, 3.60f },
    { -3671.06f, -4418.63f, 11.04f, 4.64f },
    { -3671.46f, -4431.22f, 11.26f, 4.66f },
    { -3671.94f, -4439.60f, 11.30f, 4.63f },
    { -3672.79f, -4447.96f, 11.36f, 4.60f },
    { -3673.87f, -4456.29f, 11.37f, 4.51f },
    { -3676.22f, -4464.34f, 11.43f, 4.33f },
    { -3679.93f, -4471.87f, 11.51f, 4.19f },
    { -3681.81f, -4477.32f, 11.49f, 4.76f },
    { -3680.18f, -4482.29f, 11.22f, 5.29f },
    { -3676.65f, -4488.00f, 10.90f, 5.07f },
    { -3674.23f, -4496.04f, 10.64f, 5.03f },
    { -3671.02f, -4503.80f, 10.34f, 5.16f },
    { -3668.65f, -4508.87f, 10.15f, 5.05f },
    { -3661.69f, -4514.67f, 9.88f, 0.04f },
    { -3656.59f, -4516.66f, 9.46f, 0.46f }
};

const Position KalecgosPath[KALECGOS_PATH_SIZE]
{
    { -3749.21f, -4442.37f, 30.55f, 1.29f },
    { -3748.05f, -4439.02f, 30.55f, 0.97f },
    { -3746.85f, -4437.62f, 30.55f, 0.76f },
    { -3745.72f, -4436.63f, 30.55f, 0.68f },
    { -3742.40f, -4433.60f, 30.55f, 0.78f },
    { -3738.19f, -4429.32f, 30.55f, 0.79f },
    { -3735.01f, -4426.14f, 30.55f, 0.78f },
    { -3730.72f, -4421.95f, 30.46f, 0.77f }
};

#pragma endregion

class npc_jaina_theramore : public CreatureScript
{
    public:
    npc_jaina_theramore() : CreatureScript("npc_jaina_theramore") {}

    struct npc_jaina_theramoreAI : public ScriptedAI
    {
        npc_jaina_theramoreAI(Creature* creature) : ScriptedAI(creature),
            kalecgos(nullptr), tervosh(nullptr), kinndy(nullptr), aden(nullptr),
            thalen(nullptr), rhonin(nullptr), vereesa(nullptr), pained(nullptr),
            perith(nullptr), zealous(nullptr), guard(nullptr), amara(nullptr),
            playerShaker(false), firesCount(0), npcCount(0), canBeginEnd(false),
            debug(false)
        {
            Initialize();
        }

        void Initialize()
        {
            SetCombatMovement(false);

            scheduler.SetValidator([this]
            {
                return !me->HasUnitState(UNIT_STATE_CASTING);
            });
        }

        void OnQuestAccept(Player* player, Quest const* quest) override
        {
            switch (quest->GetQuestId())
            {
                case QUEST_LOOKING_FOR_THE_ARTEFACT:
                    SetData(EVENT_START_CONVO, 0);
                    break;

                case QUEST_EVACUATION:
                    SetData(EVENT_START_EVACUATION, 0);
                    break;

                case QUEST_PREPARE_FOR_WAR:
                    SetData(EVENT_START_BATTLE, 0);
                    break;

                case QUEST_LIMIT_THE_NUKE:
                    if (Creature* wounded = GetClosestCreatureWithEntry(me, NPC_WOUNDED_DUMMY, 30.f))
                        wounded->AI()->SetData(2, 1);
                    break;
            }
        }

        void OnQuestReward(Player* player, Quest const* quest, uint32 /*opt*/) override
        {
            switch (quest->GetQuestId())
            {
                case QUEST_LOOKING_FOR_THE_ARTEFACT:
                    SetData(EVENT_END_CONVO, 0);
                    break;

                case QUEST_PREPARE_FOR_WAR:
                    SetData(EVENT_START_POST_BATTLE, 0);
                    break;
            }
        }

        void MoveInLineOfSight(Unit* who) override
        {
            if (me->GetMapId() != 726 || !canBeginEnd)
                return;

            if (who->GetTypeId() != TYPEID_PLAYER)
                return;

            if (events.GetPhaseMask() == PHASE_END)
                return;

            Player* player = who->ToPlayer();
            if (player && player->GetQuestStatus(QUEST_LIMIT_THE_NUKE) == QUEST_STATUS_INCOMPLETE && me->IsWithinDist(who, 5.f))
            {
                SetData(EVENT_SET_END, 0U);
                SetData(EVENT_START_END, 1U);
            }
        }

        void SetData(uint32 id, uint32 value) override
        {
            if (Player* player = me->SelectNearestPlayer(2000.f))
            {
                players.clear();

                if (Group* group = player->GetGroup())
                {
                    for (GroupReference* groupRef = group->GetFirstMember(); groupRef != nullptr; groupRef = groupRef->next())
                    {
                        if (Player* member = groupRef->GetSource())
                        {
                            players.push_back(member);
                        }
                    }
                }
                else
                    players.push_back(player);
            }

            me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
            me->SetSheath(SHEATH_STATE_UNARMED);

            switch (id)
            {
                case EVENT_START_CONVO:
                {
                    kalecgos = GetClosestCreatureWithEntry(me, NPC_KALECGOS, 2000.f);
                    tervosh = GetClosestCreatureWithEntry(me, NPC_ARCHMAGE_TERVOSH, 2000.f);
                    kinndy = GetClosestCreatureWithEntry(me, NPC_KINNDY_SPARKSHINE, 2000.f);

                    kinndy->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);

                    me->PlayDirectSound(28096);

                    events.SetPhase(PHASE_CONVO);
                    events.ScheduleEvent(EVENT_CONVO_1, 2s, 0, PHASE_CONVO);
                    break;
                }

                case EVENT_END_CONVO:
                {
                    Quest const* lookingForTheArtefact = sObjectMgr->GetQuestTemplate(QUEST_LOOKING_FOR_THE_ARTEFACT);
                    for (Player* player : players)
                        player->RewardQuest(lookingForTheArtefact, 0, me);
                    events.ScheduleEvent(EVENT_CONVO_23, 2s, 0, PHASE_CONVO);
                    break;
                }

                case EVENT_START_POST_BATTLE:
                {
                    Quest const* prepareForWar = sObjectMgr->GetQuestTemplate(QUEST_PREPARE_FOR_WAR);
                    for (Player* player : players)
                    {
                        player->RewardQuest(prepareForWar, 0, me);
                        player->SetPhaseMask(3, true);
                    }

                    kalecgos = GetClosestCreatureWithEntry(me, NPC_KALECGOS, 2000.f);
                    tervosh = GetClosestCreatureWithEntry(me, NPC_ARCHMAGE_TERVOSH, 2000.f);
                    kinndy = GetClosestCreatureWithEntry(me, NPC_KINNDY_SPARKSHINE, 2000.f);
                    aden = GetClosestCreatureWithEntry(me, NPC_LIEUTENANT_ADEN, 2000.f);
                    rhonin = GetClosestCreatureWithEntry(me, NPC_RHONIN, 2000.f);
                    amara = GetClosestCreatureWithEntry(me, NPC_AMARA_LEESON, 2000.f);

                    me->SetPhaseMask(3, true);
                    tervosh->SetPhaseMask(3, true);
                    kinndy->SetPhaseMask(3, true);
                    kalecgos->SetPhaseMask(3, true);
                    aden->SetPhaseMask(3, true);
                    amara->SetPhaseMask(3, true);
                    kinndy->NearTeleportTo(-3637.63f, -4408.91f, 9.79f, 4.84f);
                    kinndy->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_CRY);
                    kinndy->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);

                    rhonin->SetVisible(false);

                    std::vector<GameObject*> fires;
                    GetGameObjectListWithEntryInGrid(fires, me, GOB_FIRE_THERAMORE, 5000.f);
                    for (GameObject* fire : fires)
                        fire->Delete();

                    events.SetPhase(PHASE_POST_BATTLE);
                    events.ScheduleEvent(EVENT_POST_BATTLE_1, 2s, 0, PHASE_POST_BATTLE);
                    break;
                }

                case EVENT_START_EVACUATION:
                    kalecgos = GetClosestCreatureWithEntry(me, NPC_KALECGOS, 2000.f);
                    tervosh = GetClosestCreatureWithEntry(me, NPC_ARCHMAGE_TERVOSH, 2000.f);
                    kinndy = GetClosestCreatureWithEntry(me, NPC_KINNDY_SPARKSHINE, 2000.f);
                    aden = GetClosestCreatureWithEntry(me, NPC_LIEUTENANT_ADEN, 2000.f);
                    events.ScheduleEvent(EVENT_EVACUATION_1, Milliseconds(value ? value : 60000), 0, PHASE_CONVO);
                    break;

                case EVENT_START_WARN:
                    tervosh = GetClosestCreatureWithEntry(me, NPC_ARCHMAGE_TERVOSH, 2000.f);
                    kinndy = GetClosestCreatureWithEntry(me, NPC_KINNDY_SPARKSHINE, 2000.f);
                    kinndy->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
                    events.SetPhase(PHASE_WARN);
                    events.ScheduleEvent(EVENT_WARN_1, 2s, 0, PHASE_WARN);
                    break;

                case EVENT_START_BATTLE:
                {
                    debug = value == 2 ? true : false;

                    kalecgos = GetClosestCreatureWithEntry(me, NPC_KALECGOS, 2000.f);
                    tervosh = GetClosestCreatureWithEntry(me, NPC_ARCHMAGE_TERVOSH, 2000.f);
                    kinndy = GetClosestCreatureWithEntry(me, NPC_KINNDY_SPARKSHINE, 2000.f);
                    aden = GetClosestCreatureWithEntry(me, NPC_LIEUTENANT_ADEN, 2000.f);

                    kinndy->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);

                    // First get all creatures.
                    Trinity::AllFriendlyCreaturesInGrid creature_check(me);
                    Trinity::CreatureListSearcher<Trinity::AllFriendlyCreaturesInGrid> creature_searcher(me, civils, creature_check);
                    Cell::VisitGridObjects(me, creature_searcher, 2000.f);

                    me->SetWalk(false);
                    tervosh->SetWalk(false);
                    kinndy->SetWalk(false);

                    events.SetPhase(PHASE_PRE_BATTLE);
                    events.ScheduleEvent(EVENT_SHAKER, 2s, 0, PHASE_PRE_BATTLE);
                    if (!debug)
                        events.ScheduleEvent(EVENT_PRE_BATTLE_1, 2s, 0, PHASE_PRE_BATTLE);
                    else
                    {
                        me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
                        PrepareCivils();
                        events.ScheduleEvent(EVENT_EVACUATION_1, 0s, PHASE_PRE_BATTLE);
                        events.ScheduleEvent(EVENT_PRE_BATTLE_6, 5s, 0, PHASE_PRE_BATTLE);
                    }

                    break;
                }

                case EVENT_STOP_KALECGOS:
                    events.CancelEvent(EVENT_PRE_BATTLE_22);
                    break;

                case EVENT_SET_END:
                    canBeginEnd = !value ? false : true;
                    break;

                case EVENT_START_END:
                    rhonin = GetClosestCreatureWithEntry(me, NPC_RHONIN, 2000.f);
                    events.SetPhase(PHASE_END);
                    events.ScheduleEvent(EVENT_END_1, 2s, 0, PHASE_END);
                    break;
            }
        }

        void Reset() override
        {
            me->RemoveAllAuras();
            scheduler.CancelAll();
        }

        void EnterEvadeMode(EvadeReason why) override
        {
            ScriptedAI::EnterEvadeMode(why);

            scheduler.CancelAll();
        }

        void JustEngagedWith(Unit* who) override
        {
            if (who->GetEntry() == NPC_INVISIBLE_STALKER)
                return;

            if (roll_chance_i(60))
            {
                me->AI()->Talk(SAY_AGGRO_01);
            }

            scheduler
                .Schedule(5ms, [this](TaskContext fireball)
                {
                    DoCastVictim(SPELL_FIREBALL);
                    fireball.Repeat(2s);
                })
                .Schedule(5s, [this](TaskContext text)
                {
                    if (roll_chance_i(20))
                        me->AI()->Talk(RAND(SAY_CASTING_1, SAY_CASTING_2, SAY_CASTING_3));
                    text.Repeat(10s);
                })
                .Schedule(14s, [this](TaskContext blizzard)
                {
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                    {
                        DoCast(target, SPELL_BLIZZARD);
                        if (roll_chance_i(30))
                            me->AI()->Talk(RAND(SAY_BLIZZARD_1, SAY_BLIZZARD_2, SAY_BLIZZARD_3));
                    }
                    blizzard.Repeat(6s, 15s);
                });
        }

        void JustDied(Unit* /*killer*/) override
        {
            scheduler.CancelAll();
            events.Reset();

            for (Player* player : players)
                player->FailQuest(QUEST_PREPARE_FOR_WAR);
        }

        void KilledUnit(Unit* victim) override
        {
            if (victim->GetEntry() == NPC_INVISIBLE_STALKER)
                return;

            if (roll_chance_i(10))
            {
                me->AI()->Talk(RAND(SAY_SLAIN_01, SAY_SLAIN_02));
            }
        }

        void UpdateAI(uint32 diff) override
        {
            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                    // Event - Convo
                    #pragma region EVENT_CONVO

                    // D�but de la r�union
                    case EVENT_CONVO_1:
                        Talk(SAY_CONVO_1);
                        events.ScheduleEvent(EVENT_CONVO_2, 11s, 0, PHASE_CONVO);
                        break;

                    case EVENT_CONVO_2:
                        kinndy->AI()->Talk(SAY_CONVO_2);
                        events.ScheduleEvent(EVENT_CONVO_3, 10s, 0, PHASE_CONVO);
                        break;

                    case EVENT_CONVO_3:
                        Talk(SAY_CONVO_3);
                        events.ScheduleEvent(EVENT_CONVO_4, 12s, 0, PHASE_CONVO);
                        break;

                    case EVENT_CONVO_4:
                        kinndy->AI()->Talk(SAY_CONVO_4);
                        events.ScheduleEvent(EVENT_CONVO_5, 6s, 0, PHASE_CONVO);
                        break;

                    case EVENT_CONVO_5:
                        Talk(SAY_CONVO_5);
                        events.ScheduleEvent(EVENT_CONVO_6, 8s, 0, PHASE_CONVO);
                        break;

                    case EVENT_CONVO_6:
                        tervosh->AI()->Talk(SAY_CONVO_6);
                        events.ScheduleEvent(EVENT_CONVO_7, 8s, 0, PHASE_CONVO);
                        break;

                    case EVENT_CONVO_7:
                        kalecgos->AI()->Talk(SAY_CONVO_7);
                        events.ScheduleEvent(EVENT_CONVO_8, 6s, 0, PHASE_CONVO);
                        break;

                    case EVENT_CONVO_8:
                        kalecgos->AI()->Talk(SAY_CONVO_8);
                        events.ScheduleEvent(EVENT_CONVO_9, 9s, 0, PHASE_CONVO);
                        events.ScheduleEvent(EVENT_CONVO_10, 10s, 0, PHASE_CONVO);
                        events.ScheduleEvent(EVENT_CONVO_11, 14s, 0, PHASE_CONVO);
                        break;

                    case EVENT_CONVO_9:
                        kinndy->AI()->Talk(SAY_CONVO_9_BIS);
                        break;

                    case EVENT_CONVO_10:
                        tervosh->AI()->Talk(SAY_CONVO_9);
                        break;

                    case EVENT_CONVO_11:
                        Talk(SAY_CONVO_10);
                        events.ScheduleEvent(EVENT_CONVO_12, 6s, 0, PHASE_CONVO);
                        break;

                    case EVENT_CONVO_12:
                        kalecgos->AI()->Talk(SAY_CONVO_11);
                        events.ScheduleEvent(EVENT_CONVO_13, 4s, 0, PHASE_CONVO);
                        break;

                    case EVENT_CONVO_13:
                        Talk(SAY_CONVO_12);
                        events.ScheduleEvent(EVENT_CONVO_14, 6s, 0, PHASE_CONVO);
                        break;

                    case EVENT_CONVO_14:
                        kinndy->AI()->Talk(SAY_CONVO_13);
                        events.ScheduleEvent(EVENT_CONVO_15, 6s, 0, PHASE_CONVO);
                        break;

                    case EVENT_CONVO_15:
                        kalecgos->AI()->Talk(SAY_CONVO_14);
                        events.ScheduleEvent(EVENT_CONVO_16, 7s, 0, PHASE_CONVO);
                        break;

                    case EVENT_CONVO_16:
                        Talk(SAY_CONVO_15);
                        events.ScheduleEvent(EVENT_CONVO_17, 4s, 0, PHASE_CONVO);
                        break;

                    case EVENT_CONVO_17:
                        kalecgos->AI()->Talk(SAY_CONVO_16);
                        events.ScheduleEvent(EVENT_CONVO_18, 4s, 0, PHASE_CONVO);
                        break;

                    case EVENT_CONVO_18:
                        kalecgos->AI()->Talk(SAY_CONVO_17);
                        events.ScheduleEvent(EVENT_CONVO_19, 4s, 0, PHASE_CONVO);
                        break;

                    case EVENT_CONVO_19:
                        kalecgos->GetMotionMaster()->MoveSmoothPath(0, KalecgosPath, KALECGOS_PATH_SIZE, true);
                        events.ScheduleEvent(EVENT_CONVO_20, 10s, 0, PHASE_CONVO);
                        break;

                    case EVENT_CONVO_20:
                        kalecgos->SetVisible(false);
                        tervosh->GetMotionMaster()->MoveSmoothPath(0, TervoshPath, TERVOSH_PATH_SIZE, true);
                        events.ScheduleEvent(EVENT_CONVO_21, 5s, 0, PHASE_CONVO);
                        break;

                    case EVENT_CONVO_21:
                        kinndy->GetMotionMaster()->MoveSmoothPath(0, KinndyPath, KINNDY_PATH_SIZE, true);
                        events.ScheduleEvent(EVENT_CONVO_22, 16s, 0, PHASE_CONVO);
                        break;

                    case EVENT_CONVO_22:
                        for (Player* player : players)
                        {
                            player->AreaExploredOrEventHappens(QUEST_LOOKING_FOR_THE_ARTEFACT);
                            player->SetFacingToObject(me);
                        }
                        kinndy->SetFacingTo(0.62f);
                        me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
                        me->SetFacingToObject(players[0]);
                        tervosh->SetVisible(false);
                        break;

                    // Fin de la r�union
                    case EVENT_CONVO_23:
                        me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
                        me->SetWalk(true);
                        me->GetMotionMaster()->MovePoint(0, -3759.73f, -4446.66f, 30.55f, true, 0.38f);
                        events.ScheduleEvent(EVENT_CONVO_24, 15s, 0, PHASE_CONVO);
                        break;

                    case EVENT_CONVO_24:
                        SetData(EVENT_START_WARN, 0);
                        break;

                    #pragma endregion

                    // Event - Warn
                    #pragma region EVENT_WARN

                    case EVENT_WARN_1:
                    {
                        if (npcCount >= 4)
                        {
                            npcCount = 0;
                            events.ScheduleEvent(EVENT_WARN_2, 3s, 0, PHASE_WARN);
                            break;
                        }

                        if (Creature* c = me->SummonCreature(WarnLocation[npcCount].entry, WarnLocation[npcCount].position, TEMPSUMMON_MANUAL_DESPAWN))
                        {
                            switch (WarnLocation[npcCount].entry)
                            {
                                case NPC_PAINED:
                                    c->SetSheath(SHEATH_STATE_UNARMED);
                                    pained = c;
                                    break;

                                case NPC_MYSTERIOUS_TAUREN:
                                    perith = c;
                                    break;

                                case NPC_ZEALOUS_THERAMORE_GUARD:
                                    zealous = c;
                                    break;

                                case NPC_THERAMORE_GUARD:
                                    guard = c;
                                    break;
                            }

                            c->SetWalk(true);
                            c->GetMotionMaster()->MovePoint(0, WarnDestination[npcCount], false, WarnDestination[npcCount].GetOrientation());

                            npcCount++;

                            events.ScheduleEvent(EVENT_WARN_1, 1s, 0, PHASE_WARN);
                        }

                        break;
                    }

                    case EVENT_WARN_2:
                        pained->AI()->Talk(SAY_WARN_1);
                        pained->SetFacingTo(3.64f);
                        events.ScheduleEvent(EVENT_WARN_3, 1s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_3:
                        Talk(SAY_WARN_2);
                        me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_NONE);
                        me->GetMotionMaster()->MovePoint(0, -3750.87f, -4439.72f, 30.55f, true, 0.58f);
                        events.ScheduleEvent(EVENT_WARN_4, 1s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_4:
                        pained->AI()->Talk(SAY_WARN_3);
                        events.ScheduleEvent(EVENT_WARN_5, 6s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_5:
                        pained->AI()->Talk(SAY_WARN_4);
                        events.ScheduleEvent(EVENT_WARN_6, 7s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_6:
                        Talk(SAY_WARN_5);
                        events.ScheduleEvent(EVENT_WARN_7, 6s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_7:
                        pained->AI()->Talk(SAY_WARN_6);
                        events.ScheduleEvent(EVENT_WARN_8, 10s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_8:
                        pained->GetMotionMaster()->MoveCloserAndStop(0, me, 1.0f);
                        events.ScheduleEvent(EVENT_WARN_9, 2s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_9:
                        pained->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_USE_STANDING);
                        events.ScheduleEvent(EVENT_WARN_10, 1s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_10:
                        pained->AI()->Talk(SAY_WARN_7);
                        pained->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_NONE);
                        events.ScheduleEvent(EVENT_WARN_11, 2s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_11:
                        pained->SetTarget(ObjectGuid::Empty);
                        pained->GetMotionMaster()->MovePoint(0, -3746.31f, -4433.00f, 30.55f, false, 4.23f);
                        events.ScheduleEvent(EVENT_WARN_12, 3s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_12:
                        Talk(SAY_WARN_8);
                        me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_USE_STANDING);
                        events.ScheduleEvent(EVENT_WARN_13, 2s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_13:
                        Talk(SAY_WARN_9);
                        me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_NONE);
                        me->SetFacingToObject(perith);
                        events.ScheduleEvent(EVENT_WARN_14, 3s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_14:
                        pained->AI()->Talk(SAY_WARN_10);
                        perith->GetMotionMaster()->MovePoint(0, -3747.11f, -4438.24f, 30.55f, false, 3.94f);
                        events.ScheduleEvent(EVENT_WARN_15, 2s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_15:
                        Talk(SAY_WARN_11);
                        events.ScheduleEvent(EVENT_WARN_16, 3s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_16:
                        me->SetFacingToObject(zealous);
                        perith->SetFacingToObject(me);
                        zealous->AI()->Talk(0);
                        zealous->GetMotionMaster()->MovePoint(0, -3743.01f, -4433.78f, 30.55f, false, 3.93f);
                        events.ScheduleEvent(EVENT_WARN_17, 3s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_17:
                        Talk(SAY_WARN_13);
                        events.ScheduleEvent(EVENT_WARN_18, 7s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_18:
                        zealous->GetMotionMaster()->MovePoint(0, -3731.17f, -4420.68f, 30.44f, false);
                        guard->GetMotionMaster()->MovePoint(0, -3729.32f, -4422.18f, 30.44f, false);
                        me->SetFacingToObject(perith);
                        perith->UpdateEntry(NPC_PERITH_STORMHOOF);
                        perith->AI()->Talk(SAY_WARN_14);
                        events.ScheduleEvent(EVENT_WARN_19, 6s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_19:
                        Talk(SAY_WARN_15);
                        zealous->DespawnOrUnsummon();
                        guard->DespawnOrUnsummon();
                        events.ScheduleEvent(EVENT_WARN_20, 4s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_20:
                        perith->AI()->Talk(SAY_WARN_16);
                        events.ScheduleEvent(EVENT_WARN_21, 10s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_21:
                        perith->AI()->Talk(SAY_WARN_17);
                        events.ScheduleEvent(EVENT_WARN_22, 10s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_22:
                        perith->AI()->Talk(SAY_WARN_18);
                        events.ScheduleEvent(EVENT_WARN_23, 10s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_23:
                        Talk(SAY_WARN_19);
                        events.ScheduleEvent(EVENT_WARN_24, 5s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_24:
                        perith->AI()->Talk(SAY_WARN_20);
                        events.ScheduleEvent(EVENT_WARN_25, 5s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_25:
                        Talk(SAY_WARN_21);
                        events.ScheduleEvent(EVENT_WARN_26, 2s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_26:
                        perith->AI()->Talk(SAY_WARN_22);
                        events.ScheduleEvent(EVENT_WARN_27, 12s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_27:
                        perith->AI()->Talk(SAY_WARN_23);
                        events.ScheduleEvent(EVENT_WARN_28, 8s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_28:
                        Talk(SAY_WARN_24);
                        events.ScheduleEvent(EVENT_WARN_29, 10s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_29:
                        perith->AI()->Talk(SAY_WARN_25);
                        events.ScheduleEvent(EVENT_WARN_30, 11s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_30:
                        Talk(SAY_WARN_26);
                        me->SetWalk(true);
                        events.ScheduleEvent(EVENT_WARN_31, 3s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_31:
                        me->GetMotionMaster()->MovePoint(0, -3756.04f, -4437.71f, 30.55f, false, 2.08f);
                        events.ScheduleEvent(EVENT_WARN_32, 3s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_32:
                        Talk(SAY_WARN_27);
                        me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_USE_STANDING);
                        events.ScheduleEvent(EVENT_WARN_33, 3s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_33:
                        Talk(SAY_WARN_28);
                        me->GetMotionMaster()->MovePoint(0, -3750.87f, -4439.72f, 30.55f, false, 0.97f);
                        me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_NONE);
                        events.ScheduleEvent(EVENT_WARN_34, 3s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_34:
                        me->SetFacingToObject(perith);
                        perith->AI()->Talk(SAY_WARN_29);
                        events.ScheduleEvent(EVENT_WARN_35, 5s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_35:
                        perith->AI()->Talk(SAY_WARN_30);
                        events.ScheduleEvent(EVENT_WARN_36, 8s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_36:
                        Talk(SAY_WARN_31);
                        events.ScheduleEvent(EVENT_WARN_37, 5s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_37:
                        perith->AI()->Talk(SAY_WARN_32);
                        events.ScheduleEvent(EVENT_WARN_38, 4s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_38:
                        Talk(SAY_WARN_33);
                        events.ScheduleEvent(EVENT_WARN_39, 5s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_39:
                        perith->AI()->Talk(SAY_WARN_34);
                        events.ScheduleEvent(EVENT_WARN_40, 4s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_40:
                        perith->GetMotionMaster()->MovePoint(0, -3731.33f, -4422.18f, 30.47f);
                        events.ScheduleEvent(EVENT_WARN_41, 8s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_41:
                        perith->DespawnOrUnsummon();
                        me->SetFacingToObject(pained);
                        Talk(SAY_WARN_35);
                        events.ScheduleEvent(EVENT_WARN_42, 5s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_42:
                        pained->GetMotionMaster()->MoveCloserAndStop(0, me, 4.0f);
                        pained->AI()->Talk(SAY_WARN_36);
                        events.ScheduleEvent(EVENT_WARN_43, 3s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_43:
                        Talk(SAY_WARN_37);
                        events.ScheduleEvent(EVENT_WARN_44, 3s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_44:
                        pained->GetMotionMaster()->MoveSmoothPath(0, TervoshPath, TERVOSH_PATH_SIZE, true);
                        events.ScheduleEvent(EVENT_WARN_45, 16s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_45:

                        // Reverse path points
                        Reverse(TervoshPath, TERVOSH_PATH_SIZE);
                        Reverse(KinndyPath, KINNDY_PATH_SIZE);

                        me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
                        me->SetFacingTo(6.26f);
                        pained->SetVisible(false);
                        tervosh->SetVisible(true);
                        tervosh->GetMotionMaster()->MoveSmoothPath(0, TervoshPath, TERVOSH_PATH_SIZE, true);
                        kinndy->GetMotionMaster()->MoveSmoothPath(0, KinndyPath, KINNDY_PATH_SIZE, true);
                        kinndy->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
                        events.ScheduleEvent(EVENT_WARN_46, 21s, 0, PHASE_WARN);
                        break;

                    case EVENT_WARN_46:
                        me->SetFacingTo(6.25f);
                        tervosh->SetFacingTo(2.73f);
                        kinndy->SetFacingTo(4.75f);
                        break;

                    #pragma endregion

                    // Event - Evacuation
                    case EVENT_EVACUATION_1:
                        kalecgos->SetVisible(true);
                        kalecgos->NearTeleportTo(-3725.74f, -4547.68f, 25.82f, 0.46f);
                        me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
                        me->NearTeleportTo(-3725.09f, -4545.75f, 25.82f, 0.53f);
                        kinndy->NearTeleportTo(-3726.27f, -4543.99f, 25.82f, 0.47f);
                        tervosh->NearTeleportTo(-3723.67f, -4548.69f, 25.82f, 0.94f);
                        break;

                    // Event - Pre Battle
                    #pragma region EVENT_PRE_BATTLE

                    case EVENT_SHAKER:
                    {
                        if (!playerShaker)
                        {
                            for (Player* player : players)
                                player->CastSpell(player, 42910);

                            aden->AI()->Talk(SAY_EVENT_SHAKER);

                            std::list<GameObject*> theramoreGates;
                            GetGameObjectListWithEntryInGrid(theramoreGates, me, GOB_THERAMORE_GATE, 2000.f);
                            for (GameObject* gate : theramoreGates)
                                gate->UseDoorOrButton();

                            playerShaker = true;
                        }

                        uint32 sound = RAND(14340, 14341, 14338);
                        me->PlayDirectSound(sound);
                        events.ScheduleEvent(EVENT_SHAKER, 5ms, 1s, 0, PHASE_CONVO);
                        break;
                    }

                    case EVENT_PRE_BATTLE_1:
                    {
                        PrepareCivils();

                        me->PlayDirectSound(15003);
                        aden->Dismount();
                        aden->NearTeleportTo(-3717.79f, -4522.24f, 25.82f, 5.16f);
                        events.ScheduleEvent(EVENT_PRE_BATTLE_2, 1s, 0, PHASE_PRE_BATTLE);
                        break;
                    }

                    case EVENT_PRE_BATTLE_2:
                        Talk(SAY_PRE_BATTLE_1);
                        aden->GetMotionMaster()->MoveSmoothPath(0, AdenPath, ADEN_PATH_SIZE, false);
                        events.ScheduleEvent(EVENT_PRE_BATTLE_3, 3s, 0, PHASE_PRE_BATTLE);
                        break;

                    case EVENT_PRE_BATTLE_3:
                        events.CancelEvent(EVENT_SHAKER);
                        aden->AI()->Talk(SAY_PRE_BATTLE_2);
                        events.ScheduleEvent(EVENT_PRE_BATTLE_4, 4s, 0, PHASE_PRE_BATTLE);
                        break;

                    case EVENT_PRE_BATTLE_4:
                        Talk(SAY_PRE_BATTLE_3);
                        events.ScheduleEvent(EVENT_PRE_BATTLE_5, 3s, 0, PHASE_PRE_BATTLE);
                        break;

                    case EVENT_PRE_BATTLE_5:
                    {
                        aden->SetWalk(true);
                        aden->GetMotionMaster()->MovePoint(0, -3721.23f, -4549.68f, 25.82f, true, 1.32f);
                        if (GameObject* portal = me->SummonGameObject(201797, PortalPosition, QuaternionData(), 13s))
                        {
                            me->SetFacingToObject(portal);
                            portal->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NOT_SELECTABLE);
                        }
                        events.ScheduleEvent(EVENT_PRE_BATTLE_6, 8ms, 0, PHASE_PRE_BATTLE);
                        break;
                    }

                    case EVENT_PRE_BATTLE_6:
                    {
                        if (npcCount >= 6)
                        {
                            npcCount = 0;

                            events.CancelEvent(EVENT_PRE_BATTLE_6);
                            if (!debug)
                                events.ScheduleEvent(EVENT_PRE_BATTLE_7, 1s, 0, PHASE_PRE_BATTLE);
                            else
                            {
                                events.CancelEvent(EVENT_SHAKER);
                                events.ScheduleEvent(EVENT_PRE_BATTLE_19, 1s, 0, PHASE_PRE_BATTLE);
                            }

                            break;
                        }

                        if (Creature* c = me->SummonCreature(ArchmagesLocation[npcCount].entry, PortalPosition, TEMPSUMMON_MANUAL_DESPAWN))
                        {
                            switch (ArchmagesLocation[npcCount].entry)
                            {
                                case NPC_RHONIN:
                                    rhonin = c;
                                    c->AI()->Talk(SAY_PRE_BATTLE_4);
                                    me->SetFacingToObject(c);
                                    kinndy->SetFacingToObject(c);
                                    tervosh->SetFacingToObject(c);
                                    break;

                                case NPC_THALEN_SONGWEAVER:
                                    thalen = c;
                                    break;

                                case NPC_VEREESA_WINDRUNNER:
                                    vereesa = c;
                                    break;

                                default:
                                    break;
                            }

                            archmagesGUID[npcCount] = c->GetGUID();

                            c->SetWalk(true);
                            c->GetMotionMaster()->MovePoint(0, ArchmagesLocation[npcCount].position);
                            c->SetSheath(SHEATH_STATE_UNARMED);
                            c->CastSpell(c, SPELL_TELEPORT);
                            c->SetTarget(me->GetGUID());

                            npcCount++;
                        }

                        events.RescheduleEvent(EVENT_PRE_BATTLE_6, 1s, 0, PHASE_PRE_BATTLE);
                        break;
                    }

                    case EVENT_PRE_BATTLE_7:
                        Talk(SAY_PRE_BATTLE_5);
                        events.ScheduleEvent(EVENT_PRE_BATTLE_8, 11s, 0, PHASE_PRE_BATTLE);
                        break;

                    case EVENT_PRE_BATTLE_8:
                        Talk(SAY_PRE_BATTLE_6);
                        events.ScheduleEvent(EVENT_PRE_BATTLE_9, 9s, 0, PHASE_PRE_BATTLE);
                        break;

                    case EVENT_PRE_BATTLE_9:
                        thalen->AI()->Talk(SAY_PRE_BATTLE_7);
                        events.ScheduleEvent(EVENT_PRE_BATTLE_10, 7s, 0, PHASE_PRE_BATTLE);
                        break;

                    case EVENT_PRE_BATTLE_10:
                        Talk(SAY_PRE_BATTLE_8);
                        events.ScheduleEvent(EVENT_PRE_BATTLE_11, 3s, 0, PHASE_PRE_BATTLE);
                        break;

                    case EVENT_PRE_BATTLE_11:
                        Talk(SAY_PRE_BATTLE_9);
                        events.ScheduleEvent(EVENT_PRE_BATTLE_12, 7s, 0, PHASE_PRE_BATTLE);
                        break;

                    case EVENT_PRE_BATTLE_12:
                        rhonin->AI()->Talk(SAY_PRE_BATTLE_10);
                        events.ScheduleEvent(EVENT_PRE_BATTLE_13, 2s, 0, PHASE_PRE_BATTLE);
                        break;

                    case EVENT_PRE_BATTLE_13:
                        Talk(SAY_PRE_BATTLE_11);
                        me->SetFacingToObject(tervosh);
                        tervosh->SetFacingToObject(me);
                        events.ScheduleEvent(EVENT_PRE_BATTLE_14, 5s, 0, PHASE_PRE_BATTLE);
                        break;

                    case EVENT_PRE_BATTLE_14:
                        vereesa->AI()->Talk(SAY_PRE_BATTLE_12);
                        me->SetFacingToObject(vereesa);
                        tervosh->SetFacingToObject(vereesa);
                        events.ScheduleEvent(EVENT_PRE_BATTLE_15, 5s, 0, PHASE_PRE_BATTLE);
                        break;

                    case EVENT_PRE_BATTLE_15:
                        Talk(SAY_PRE_BATTLE_13);
                        events.ScheduleEvent(EVENT_PRE_BATTLE_16, 8s, 0, PHASE_PRE_BATTLE);
                        break;

                    case EVENT_PRE_BATTLE_16:
                        vereesa->CastSpell(vereesa, SPELL_VANISH);
                        vereesa->SetStandState(UNIT_STAND_STATE_KNEEL);
                        Talk(SAY_PRE_BATTLE_14);
                        me->SetFacingToObject(players[0]);
                        events.ScheduleEvent(EVENT_PRE_BATTLE_17, 2s, 0, PHASE_PRE_BATTLE);
                        break;

                    case EVENT_PRE_BATTLE_17:
                        vereesa->SetFaction(35);
                        vereesa->SetVisible(false);
                        DoCast(SPELL_SIMPLE_TELEPORT);
                        events.ScheduleEvent(EVENT_PRE_BATTLE_18, 1300ms, 0, PHASE_PRE_BATTLE);
                        break;

                    case EVENT_PRE_BATTLE_18:
                    {
                        DoCastSelf(SPELL_TELEPORT);

                        kinndy->CastSpell(kinndy, SPELL_TELEPORT);
                        tervosh->CastSpell(tervosh, SPELL_TELEPORT);
                        aden->CastSpell(aden, SPELL_TELEPORT);
                        kalecgos->CastSpell(kalecgos, SPELL_TELEPORT);

                        for (uint32 i = 0; i < 6; ++i)
                        {
                            if (Creature* c = ObjectAccessor::GetCreature(*me, archmagesGUID[i]))
                                c->CastSpell(c, SPELL_TELEPORT);
                        }

                        for (Player* player : players)
                            player->CastSpell(player, SPELL_TELEPORT);

                        events.ScheduleEvent(EVENT_PRE_BATTLE_19, 1500ms, 0, PHASE_PRE_BATTLE);
                        break;
                    }

                    case EVENT_PRE_BATTLE_19:
                    {
                        me->SetSheath(SHEATH_STATE_MELEE);

                        kalecgos->SetVisible(false);
                        kalecgos = me->SummonCreature(NPC_KALECGOS_DRAGON, -3717.96f, -4356.52f, 90.82f, 0.f);
                        kalecgos->SetDisableGravity(true);
                        events.ScheduleEvent(EVENT_PRE_BATTLE_22, 1s, 0, PHASE_PRE_BATTLE);

                        Relocate(aden, -3669.01f, -4381.60f, 9.56f, 0.69f);
                        Relocate(me, -3658.39f, -4372.87f, 9.35f, 0.69f);
                        Relocate(kinndy, -3666.15f, -4519.95f, 10.03f, 2.44f);
                        Relocate(tervosh, -3808.72f, -4541.01f, 10.68f, 3.09f);

                        for (int i = 0; i < 6; ++i)
                        {
                            if (Creature* c = ObjectAccessor::GetCreature(*me, archmagesGUID[i]))
                            {
                                c->SetTarget(ObjectGuid::Empty);
                                c->SetSheath(SHEATH_STATE_MELEE);
                                Relocate(c, ArchmagesRelocate[i][0], ArchmagesRelocate[i][1], ArchmagesRelocate[i][2], ArchmagesRelocate[i][3]);

                                uint32 entry = c->GetEntry();
                                switch (entry)
                                {
                                    case NPC_AMARA_LEESON:
                                    case NPC_THALEN_SONGWEAVER:
                                    case NPC_TARI_COGG:
                                    case NPC_THODER_WINDERMERE:
                                        c->CastSpell(c, SPELL_ARCANE_CANALISATION);
                                        break;

                                    default:
                                        break;
                                }
                            }
                        }

                        if (Creature* c = me->SummonCreature(NPC_INVISIBLE_STALKER, -3646.48f, -4362.23f, 9.57f, 0.70f, TEMPSUMMON_MANUAL_DESPAWN))
                        {
                            c->AddAura(70573, c);
                            c->SetObjectScale(3.0f);
                        }

                        if (Creature* c = me->SummonCreature(NPC_INVISIBLE_STALKER, -3782.96f, -4252.81f, 6.25f, 1.52f, TEMPSUMMON_MANUAL_DESPAWN))
                        {
                            c->AddAura(70573, c);
                            c->SetObjectScale(3.0f);
                        }

                        for (Player* player : players)
                        {
                            float dist = frand(1, 4);

                            Position pos = { -3658.39f, -4372.87f, 9.35f, 0.69f };
                            me->MovePosition(pos, dist * (float)rand_norm(), (float)rand_norm() * static_cast<float>(2 * M_PI));

                            player->NearTeleportTo(pos);
                        }

                        events.ScheduleEvent(EVENT_PRE_BATTLE_20, 4s, 0, PHASE_PRE_BATTLE);
                        break;
                    }

                    case EVENT_PRE_BATTLE_20:
                    {
                        me->GetGameObjectListWithEntryInGrid(fires, GOB_FIRE_THERAMORE, 18.f);

                        const Position start = { fires[0]->GetPositionX(), fires[0]->GetPositionY(), 13.46f };
                        if (frostTarget = me->SummonCreature(NPC_TARGET_ICE_WALL, start, TEMPSUMMON_TIMED_DESPAWN, 2min))
                        {
                            me->SetTarget(frostTarget->GetGUID());

                            frostTarget->SetSpeedRate(MOVE_WALK, 1.4f);
                        }

                        events.ScheduleEvent(EVENT_PRE_BATTLE_21, 2s, 0, PHASE_PRE_BATTLE);
                        break;
                    }

                    case EVENT_PRE_BATTLE_21:
                    {
                        if (!me->HasUnitState(UNIT_STATE_CASTING))
                        {
                            uint8 count = fires.size();

                            Position* pos = new Position[count];
                            for (uint8 i = 0; i < count; i++)
                                pos[i] = { fires[i]->GetPositionX(), fires[i]->GetPositionY(), 13.46f };

                            frostTarget->GetMotionMaster()->MoveSmoothPath(0, pos, count, false);
                            DoCast(frostTarget, 100127, true);
                        }

                        if (firesCount >= fires.size())
                        {
                            if (frostTarget) frostTarget->RemoveAllAuras();

                            me->CastStop();
                            me->SetFacingTo(0.70f);
                            me->SetTarget(ObjectGuid::Empty);

                            events.SetPhase(PHASE_BATTLE);
                            if (Creature* waves = me->SummonCreature(NPC_WAVES_INVOKER, me->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN))
                            {
                                if (debug)
                                {
                                    me->Talk("Need to launch waves manually in debug mode with SetData command.", CHAT_MSG_MONSTER_SAY, LANG_UNIVERSAL, 5.f, players[0]);
                                }
                                else
                                {
                                    waves->AI()->SetData(1, 1);
                                }
                            }

                            break;
                        }

                        if (GameObject* fire = fires[firesCount])
                        {
                            fire->Delete();
                            firesCount++;
                            events.RescheduleEvent(EVENT_PRE_BATTLE_21, 2s, 0, PHASE_PRE_BATTLE);
                        }

                        break;
                    }

                    case EVENT_PRE_BATTLE_22:
                        kalecgos->GetMotionMaster()->MoveCirclePath(-3753.48f, -4444.54f, 55.23f, KALECGOS_CIRCLE_RADIUS, true, 16);
                        HandleKalecPathRepeat(KALECGOS_CIRCLE_RADIUS);
                        break;

                    #pragma endregion

                    // Event - Post Battle
                    #pragma region EVENT_POST_BATTLE

                    case EVENT_POST_BATTLE_1:
                        me->SetWalk(true);
                        aden->Dismount();
                        me->GetMotionMaster()->MovePoint(0, -3666.88f, -4379.16f, 9.36f, true, 3.89f);
                        events.ScheduleEvent(EVENT_POST_BATTLE_2, 6s, 0, PHASE_POST_BATTLE);
                        break;

                    case EVENT_POST_BATTLE_2:
                        Talk(SAY_POST_BATTLE_1);
                        me->SetFacingToObject(aden);
                        events.ScheduleEvent(EVENT_POST_BATTLE_3, 4s, 0, PHASE_POST_BATTLE);
                        break;

                    case EVENT_POST_BATTLE_3:
                        aden->AI()->Talk(SAY_POST_BATTLE_2);
                        events.ScheduleEvent(EVENT_POST_BATTLE_4, 6s, 0, PHASE_POST_BATTLE);
                        break;

                    case EVENT_POST_BATTLE_4:
                        Talk(SAY_POST_BATTLE_3);
                        events.ScheduleEvent(EVENT_POST_BATTLE_5, 2s, 0, PHASE_POST_BATTLE);
                        break;

                    case EVENT_POST_BATTLE_5:
                        aden->SetWalk(true);
                        me->GetMotionMaster()->MoveSmoothPath(0, JainaPostBattlePath, JAINA_PATH_SIZE, true);
                        aden->GetMotionMaster()->MoveSmoothPath(0, JainaPostBattlePath, JAINA_PATH_SIZE, true);
                        events.ScheduleEvent(EVENT_POST_BATTLE_6, 26s, 0, PHASE_POST_BATTLE);
                        break;

                    case EVENT_POST_BATTLE_6:
                        amara->NearTeleportTo(-3658.80f, -4520.89f, 9.71f, 2.52f);
                        me->GetMotionMaster()->MovePoint(0, -3634.95f, -4412.57f, 9.81f, true, 2.17f);
                        aden->GetMotionMaster()->Clear();
                        aden->GetMotionMaster()->MovePoint(0, -3615.46f, -4437.43f, 13.68f, true, 1.57f);
                        events.ScheduleEvent(EVENT_POST_BATTLE_7, 3s, 0, PHASE_POST_BATTLE);
                        break;

                    case EVENT_POST_BATTLE_7:
                        kinndy->AI()->Talk(SAY_POST_BATTLE_4);
                        events.ScheduleEvent(EVENT_POST_BATTLE_8, 5s, 0, PHASE_POST_BATTLE);
                        break;

                    case EVENT_POST_BATTLE_8:
                        Talk(SAY_POST_BATTLE_5);
                        events.ScheduleEvent(EVENT_POST_BATTLE_9, 9s, 0, PHASE_POST_BATTLE);
                        break;

                    case EVENT_POST_BATTLE_9:
                        kinndy->AI()->Talk(SAY_POST_BATTLE_6);
                        kinndy->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_NONE);
                        kinndy->SetFacingToObject(me);
                        events.ScheduleEvent(EVENT_POST_BATTLE_10, 4s, 0, PHASE_POST_BATTLE);
                        break;

                    case EVENT_POST_BATTLE_10:
                        Talk(SAY_POST_BATTLE_7);
                        events.ScheduleEvent(EVENT_POST_BATTLE_11, 4s, 0, PHASE_POST_BATTLE);
                        break;

                    case EVENT_POST_BATTLE_11:
                        kinndy->AI()->Talk(SAY_POST_BATTLE_8);
                        events.ScheduleEvent(EVENT_POST_BATTLE_12, 3s, 0, PHASE_POST_BATTLE);
                        break;

                    case EVENT_POST_BATTLE_12:
                        Talk(SAY_POST_BATTLE_9);
                        events.ScheduleEvent(EVENT_POST_BATTLE_13, 16s, 0, PHASE_POST_BATTLE);
                        break;

                    case EVENT_POST_BATTLE_13:
                        Talk(SAY_POST_BATTLE_10);
                        events.ScheduleEvent(EVENT_POST_BATTLE_14, 7s, 0, PHASE_POST_BATTLE);
                        break;

                    case EVENT_POST_BATTLE_14:
                        kinndy->AI()->Talk(SAY_POST_BATTLE_11);
                        events.ScheduleEvent(EVENT_POST_BATTLE_15, 6s, 0, PHASE_POST_BATTLE);
                        break;

                    case EVENT_POST_BATTLE_15:
                        Talk(SAY_POST_BATTLE_12);
                        events.ScheduleEvent(EVENT_POST_BATTLE_16, 6s, 0, PHASE_POST_BATTLE);
                        break;

                    case EVENT_POST_BATTLE_16:
                        Talk(SAY_POST_BATTLE_13);
                        events.ScheduleEvent(EVENT_POST_BATTLE_17, 15s, 0, PHASE_POST_BATTLE);
                        break;

                    case EVENT_POST_BATTLE_17:
                        kinndy->AI()->Talk(SAY_POST_BATTLE_14);
                        events.ScheduleEvent(EVENT_POST_BATTLE_18, 3s, 0, PHASE_POST_BATTLE);
                        break;

                    case EVENT_POST_BATTLE_18:
                        Talk(SAY_POST_BATTLE_15);
                        events.ScheduleEvent(EVENT_POST_BATTLE_19, 3s, 0, PHASE_POST_BATTLE);
                        break;

                    case EVENT_POST_BATTLE_19:
                        Talk(SAY_POST_BATTLE_16);
                        kinndy->SetWalk(false);
                        kinndy->GetMotionMaster()->MoveSmoothPath(0, KinndyWoundedPath, KINNDY_PATH_1_SIZE, false);
                        me->SetWalk(false);
                        me->GetMotionMaster()->MoveSmoothPath(0, JainaWoundedPath, JAINA_PATH_1_SIZE, false);
                        events.ScheduleEvent(EVENT_POST_BATTLE_20, 23s, 0, PHASE_POST_BATTLE);
                        break;

                    case EVENT_POST_BATTLE_20:
                        kinndy->GetMotionMaster()->Clear();
                        kinndy->GetMotionMaster()->MoveIdle();
                        kinndy->SetWalk(true);
                        me->SetWalk(true);
                        if (Creature* wounded = DoSummon(NPC_WOUNDED_DUMMY, me->GetPosition()))
                            wounded->AI()->SetData(1, 1);
                        break;

                    #pragma endregion

                    // Event - End
                    #pragma region EVENT_END

                    case EVENT_END_1:
                        Talk(SAY_END_1);
                        me->PlayDirectMusic(28046);
                        events.ScheduleEvent(EVENT_END_2, 4s, 0, PHASE_END);
                        break;

                    case EVENT_END_2:
                        rhonin->AI()->Talk(SAY_END_2);
                        events.ScheduleEvent(EVENT_END_3, 3s, 0, PHASE_END);
                        break;

                    case EVENT_END_3:
                        rhonin->AI()->Talk(SAY_END_3);
                        events.ScheduleEvent(EVENT_END_4, 5s, 0, PHASE_END);
                        break;

                    case EVENT_END_4:
                        Talk(SAY_END_4);
                        Talk(SAY_END_5);
                        me->RemoveAllAuras();
                        me->SetFacingToObject(rhonin);
                        events.ScheduleEvent(EVENT_END_5, 7s, 0, PHASE_END);
                        break;

                    case EVENT_END_5:
                        rhonin->AI()->Talk(SAY_END_6);
                        events.ScheduleEvent(EVENT_END_6, 8s, 0, PHASE_END);
                        break;

                    case EVENT_END_6:
                        rhonin->AI()->Talk(SAY_END_7);
                        events.ScheduleEvent(EVENT_END_7, 7s, 0, PHASE_END);
                        break;

                    case EVENT_END_7:
                        Talk(SAY_END_8);
                        events.ScheduleEvent(EVENT_END_8, 7s, 0, PHASE_END);
                        break;

                    case EVENT_END_8:
                        rhonin->AI()->Talk(SAY_END_9);
                        events.ScheduleEvent(EVENT_END_9, 3s, 0, PHASE_END);
                        break;

                    case EVENT_END_9:
                        Talk(SAY_END_10);
                        for (Player* player : players)
                            player->CastSpell(player, 100093);
                        events.ScheduleEvent(EVENT_END_10, 6s, 0, PHASE_END);
                        break;

                    case EVENT_END_10:
                        rhonin->AI()->Talk(SAY_END_11);
                        for (Player* player : players)
                            player->PlayDirectSound(11563);
                        events.ScheduleEvent(EVENT_END_11, 9s, 0, PHASE_END);
                        break;

                    case EVENT_END_11:
                        for (Player* player : players)
                        {
                            player->PlayDirectSound(11571);
                            player->CastSpell(player, SPELL_TELEPORT);
                            player->CompleteQuest(QUEST_LIMIT_THE_NUKE);
                        }
                        events.ScheduleEvent(EVENT_END_12, 1s, 0, PHASE_END);
                        break;

                    case EVENT_END_12:
                        for (Player* player : players)
                        {
                            player->SetPhaseMask(4, true);

                            const Position center = { -2701.89f, -4702.44f, 7.86f, 4.95f };
                            const Position pos = GetRandomPosition(center, 10.f);
                            const WorldLocation dest = { 1, pos };
                            player->TeleportTo(dest);
                        }
                        break;

                    #pragma endregion

                    default:
                        break;
                }
            }

            // Combat
            if (!UpdateVictim())
                return;

            scheduler.Update(diff, [this]
            {
                DoMeleeAttackIfReady();
            });
        }

        private:

        #pragma region VARIABLES
        bool playerShaker, canBeginEnd, debug;
        uint8 firesCount;
        uint8 npcCount;
        EventMap events;
        Creature *kalecgos, *tervosh, *kinndy;
        Creature *aden, *thalen, *rhonin;
        Creature *vereesa, *pained, *perith;
        Creature *zealous, *guard, *amara, *frostTarget;
        std::vector<Player*> players;
        ObjectGuid archmagesGUID[6];
        std::vector<GameObject*> fires;
        std::list<Creature*> civils;
        TaskScheduler scheduler;
        #pragma endregion

        void PrepareCivils()
        {
            for (int i = 0; i < 35; ++i)
            {
                me->SummonGameObject(GOB_FIRE_THERAMORE, FireLocation[i][0], FireLocation[i][1], FireLocation[i][2], FireLocation[i][3], QuaternionData(), 0s);
                if (Creature* fx = me->SummonCreature(NPC_INVISIBLE_STALKER, FireLocation[i][0], FireLocation[i][1], FireLocation[i][2], 0.f, TEMPSUMMON_TIMED_DESPAWN, 5s))
                    fx->CastSpell(fx, 70444);
            }

            for (Creature* civil : civils)
            {
                switch (civil->GetEntry())
                {
                    case 4901:
                    case 4902:
                    case 6732:
                    case 100025:
                    case 100026:
                    case 100027:
                    case 100028:
                    case 100073:
                        civil->SetVisible(false);
                        break;

                    case 100012:
                        civil->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_READY1H);
                        if (civil->GetSpawnId() == 1022705 || civil->GetSpawnId() == 1022704)
                        {
                            civil->SetVisible(false);
                            civil->StopMoving();
                            civil->GetMotionMaster()->MoveIdle();
                        }
                        break;

                    case 34776:
                        if (civil->GetSpawnId() == 1022775)
                        {
                            civil->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                            civil->SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_DEAD);
                            civil->SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_FEIGN_DEATH);
                            civil->SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_PLAY_DEATH_ANIM);
                        }
                        break;

                    case 4941:
                        civil->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_COWER);
                        break;

                    case 4899:
                        civil->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_COWER);
                        civil->GetMotionMaster()->MovePoint(0, -3698.03f, -4353.46f, 11.45f, true, 1.78f);
                        break;

                    case 4900:
                        civil->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_COWER);
                        civil->GetMotionMaster()->MovePoint(0, -3699.53f, -4356.35f, 11.41f, true, 1.78f);
                        break;

                    case 4898:
                        civil->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_COWER);
                        civil->GetMotionMaster()->MovePoint(0, -3700.21f, -4351.08f, 11.41f, true, 1.86f);
                        break;

                    case 4897:
                        civil->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_COWER);
                        civil->GetMotionMaster()->MovePoint(0, -3824.76f, -4443.74f, 12.66f, true, 3.24f);
                        break;

                    case 11052:
                        civil->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_COWER);
                        civil->GetMotionMaster()->MovePoint(0, -3826.36f, -4441.66f, 12.25f, true, 3.51f);
                        break;

                    case 4885:
                    case 4888:
                    case 5405:
                    case 10047:
                    case 12375:
                    case 12376:
                    case 24005:
                    case 24006:
                    case 24007:
                        civil->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                        civil->SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_DEAD);
                        civil->SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_FEIGN_DEATH);
                        civil->SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_PLAY_DEATH_ANIM);
                        break;

                    case 4886:
                        civil->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_READY1H);
                        if (Creature* c = civil->FindNearestCreature(4888, 18.f))
                        {
                            civil->GetMotionMaster()->MoveFollow(c, 1.0f, 0.f);
                            civil->SetTarget(c->GetGUID());
                        }
                        break;
                }
            }
        }

        void Relocate(Creature* c, float x, float y, float z, float orientation)
        {
            const Position pos{ x, y, z, orientation };
            c->NearTeleportTo(pos);
            c->SetHomePosition(pos);
            c->Respawn();
        }

        void Reverse(Position array[], int size)
        {
            std::stack<Position> stack;
            for (int i = 0; i < size; ++i)
                stack.push(array[i]);

            int index = 0;
            while (!stack.empty())
            {
                array[index++] = stack.top();
                stack.pop();
            }
        }

        void HandleKalecPathRepeat(float radius)
        {
            float perimeter = 2.f * float(M_PI) * radius;
            float time = (perimeter / kalecgos->GetSpeed(MOVE_FLIGHT)) * IN_MILLISECONDS;
            events.RescheduleEvent(EVENT_PRE_BATTLE_22, Milliseconds((uint64)time), 0, PHASE_PRE_BATTLE);
        }

        Position GetRandomPosition(Position center, float dist)
        {
            float alpha = 2 * float(M_PI) * float(rand_norm());
            float r = dist * sqrtf(float(rand_norm()));
            float x = r * cosf(alpha) + center.GetPositionX();
            float y = r * sinf(alpha) + center.GetPositionY();
            return { x, y, center.GetPositionZ(), 0.f };
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_jaina_theramoreAI(creature);
    }
};

enum Civil
{
    GOSSIP_MENU_EVACUATION  = 100003,
    GOSSIP_MENU_DEFAULT     = 68
};

class npc_civil_of_theramore : public CreatureScript
{
    public:
    npc_civil_of_theramore() : CreatureScript("npc_civil_of_theramore")
    {
    }

    struct npc_civil_of_theramoreAI : public ScriptedAI
    {
        npc_civil_of_theramoreAI(Creature* creature) : ScriptedAI(creature)
        {
            Initialize();
        }

        void Initialize()
        {
        }

        void MovementInform(uint32 /*type*/, uint32 id) override
        {
            if (id == 0)
            {
                me->SetPhaseMask(32, true);
            }
        }

        bool OnGossipHello(Player* player) override
        {
            if (player->GetQuestStatus(QUEST_EVACUATION) == QUEST_STATUS_INCOMPLETE)
            {
                player->PrepareGossipMenu(me, GOSSIP_MENU_EVACUATION, true);
            }
            else
            {
                player->PrepareGossipMenu(me, GOSSIP_MENU_DEFAULT, true);
            }

            player->SendPreparedGossip(me);

            return true;
        }

        bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
        {
            ClearGossipMenuFor(player);

            switch (gossipListId)
            {
                case 0:
                {
                    me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
                    me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STAND_STATE_NONE);
                    scheduler.Schedule(5ms, [this, player](TaskContext context)
                    {
                        switch (context.GetRepeatCounter())
                        {
                            case 0:
                                me->SetFacingToObject(player);
                                me->SetWalk(false);
                                context.Repeat(1s);
                                break;
                            case 1:
                                Talk(0);
                                context.Repeat(5s);
                                break;
                            case 2:
                                if (Creature* stalker = GetClosestCreatureWithEntry(me, NPC_INVISIBLE_STALKER, 30.f))
                                    me->GetMotionMaster()->MovePoint(0, stalker->GetPosition());
                                context.Repeat(1s);
                                break;
                            case 3:
                                AddKillMonsterCredit();
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
            scheduler.CancelAll();
        }

        void UpdateAI(uint32 diff) override
        {
            scheduler.Update(diff);
        }

        private:
        ObjectGuid* stalker;
        TaskScheduler scheduler;

        void AddKillMonsterCredit()
        {
            if (Player* player = me->SelectNearestPlayer(2000.f))
            {
                if (Group* group = player->GetGroup())
                {
                    for (GroupReference* groupRef = group->GetFirstMember(); groupRef != nullptr; groupRef = groupRef->next())
                    {
                        if (Player* member = groupRef->GetSource())
                        {
                            member->KilledMonsterCredit(100074);
                        }
                    }
                }
                else
                    player->KilledMonsterCredit(100074);
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_civil_of_theramoreAI(creature);
    }
};

void AddSC_theramore()
{
    new npc_jaina_theramore();
    new npc_civil_of_theramore();
}
