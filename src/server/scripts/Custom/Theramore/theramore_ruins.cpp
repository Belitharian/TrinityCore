#include "ScriptMgr.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "GameObject.h"
#include "Player.h"
#include "CombatAI.h"
#include "ObjectMgr.h"
#include "Creature.h"
#include "TemporarySummon.h"
#include "MotionMaster.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "GridNotifiersImpl.h"
#include "theramore_ruins.h"
#include "DatabaseEnv.h"
#include "Mail.h"
#include "Vehicle.h"
#include "Group.h"

class JainaStandEvent : public BasicEvent
{
public:
    JainaStandEvent(Creature* owner, Player* player) : owner(owner), stage(0), player(player)
    {
        kinndy = owner->FindNearestCreature(NPC_KINNDY_SPARKSHINE, 40.f);
    }

    bool Execute(uint64 eventTime, uint32 /*updateTime*/) override
    {
        switch (stage)
        {
            case 0:
                owner->AI()->Talk(SAY_IRIS_1);
                owner->SetWalk(true);
                stage++;
                owner->m_Events.AddEvent(this, Milliseconds(eventTime + 3000));
                return false;
            case 1:
                owner->SetStandState(UNIT_STAND_STATE_STAND);
                stage++;
                owner->m_Events.AddEvent(this, Milliseconds(eventTime + 2000));
                return false;
            case 2:
                kinndy->SetVisible(false);
                stage++;
                owner->m_Events.AddEvent(this, Milliseconds(eventTime + 2000));
                return false;
            case 3:
                owner->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
                owner->SetFacingToObject(player);
                return true;
            default:
                break;
        }
        return true;
    }

private:
    Creature* owner;
    Creature* kinndy;
    Player* player;
    uint8 stage;
};

class npc_jaina_ruins : public CreatureScript
{
    public:
    npc_jaina_ruins() : CreatureScript("npc_jaina_ruins") {}

    struct npc_jaina_ruinsAI : public ScriptedAI
    {
        npc_jaina_ruinsAI(Creature* creature) : ScriptedAI(creature)
        {
            Initialize();
        }

        void Initialize()
        {
            canSayIntro = false;
            canSayEnd = false;
            canSayDestruction = false;

            scheduler.SetValidator([this]
            {
                return !me->HasUnitState(UNIT_STATE_CASTING);
            });
        }

        void MoveInLineOfSight(Unit* who) override
        {
            if (who->GetTypeId() != TYPEID_PLAYER)
                return;

            switch (me->GetMapId())
            {
                case 1:
                {
                    if (canSayDestruction)
                        return;

                    if (Player* player = me->SelectNearestPlayer(2000.f))
                    {
                        players.clear();

                        if (Group* group = player->GetGroup())
                        {
                            for (GroupReference* groupRef = group->GetFirstMember(); groupRef != nullptr; groupRef = groupRef->next())
                            {
                                if (Player* member = groupRef->GetSource())
                                    players.push_back(member);
                            }
                        }
                        else
                            players.push_back(player);
                    }

                    if (me->IsWithinDist(who, 30.f))
                    {
                        for (Player* player : players)
                        {
                            if (player->GetQuestStatus(QUEST_LIMIT_THE_NUKE) == QUEST_STATUS_COMPLETE)
                                continue;
                            return;
                        }

                        me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
                        me->SetStandState(UNIT_STAND_STATE_STAND);

                        Quest const* limitTheNuke = sObjectMgr->GetQuestTemplate(QUEST_LIMIT_THE_NUKE);
                        for (Player* player : players)
                            player->RewardQuest(limitTheNuke, 0, me);

                        Quest const* returnToTheramore = sObjectMgr->GetQuestTemplate(QUEST_RETURN_TO_THERAMORE);
                        for (Player* player : players)
                            player->AddQuestAndCheckCompletion(returnToTheramore, me);

                        SetData(EVENT_START_RETURN, 1U);

                        canSayDestruction = true;
                    }

                    break;
                }

                case 725:
                {
                    if (canSayIntro && canSayEnd)
                        return;

                    if (Player* player = me->SelectNearestPlayer(2000.f))
                    {
                        players.clear();

                        if (Group* group = player->GetGroup())
                        {
                            for (GroupReference* groupRef = group->GetFirstMember(); groupRef != nullptr; groupRef = groupRef->next())
                            {
                                if (Player* member = groupRef->GetSource())
                                    players.push_back(member);
                            }
                        }
                        else
                            players.push_back(player);
                    }

                    if (me->IsWithinDist(who, 6.f))
                    {
                        if (!canSayIntro)
                        {
                            for (Player* player : players)
                            {
                                if (player->GetQuestStatus(QUEST_RETURN_TO_THERAMORE) == QUEST_STATUS_COMPLETE)
                                    continue;
                                return;
                            }

                            me->m_Events.AddEvent(new JainaStandEvent(me, who->ToPlayer()), me->m_Events.CalculateTime(1s));
                            canSayIntro = true;
                        }

                        if (!canSayEnd)
                        {
                            for (Player* player : players)
                            {
                                if (player->GetQuestStatus(QUEST_DESTROY_THE_DESTROYER) == QUEST_STATUS_COMPLETE)
                                    continue;
                                return;
                            }

                            canSayEnd = true;
                            Talk(SAY_IRIS_3);
                            events.ScheduleEvent(EVENT_IRIS_4, 3s);
                        }
                    }

                    break;
                }
            }
        }

        void OnQuestReward(Player* player, Quest const* quest, uint32 /*opt*/) override
        {
            switch (quest->GetQuestId())
            {
                case QUEST_RETURN_TO_THERAMORE:
                {
                    Quest const* returnToTheramore = sObjectMgr->GetQuestTemplate(QUEST_RETURN_TO_THERAMORE);
                    for (Player* player : players)
                        player->RewardQuest(returnToTheramore, 0, me);
                    SetData(EVENT_START_IRIS, 1U);
                    break;
                }
            }
        }

        void SetData(uint32 id, uint32 /*value*/) override
        {
            if (players.empty())
            {
                if (Player* player = me->SelectNearestPlayer(2000.f))
                {
                    if (Group* group = player->GetGroup())
                    {
                        for (GroupReference* groupRef = group->GetFirstMember(); groupRef != nullptr; groupRef = groupRef->next())
                        {
                            if (Player* member = groupRef->GetSource())
                                if (member->IsInMap(player))
                                    players.push_back(player);
                        }
                    }
                    else
                        players.push_back(player);
                }
            }

            me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
            me->SetSheath(SHEATH_STATE_UNARMED);

            switch (id)
            {
                case EVENT_START_RETURN:
                    events.ScheduleEvent(EVENT_RETURN_1, 5s);
                    break;

                case EVENT_START_IRIS:
                    events.ScheduleEvent(EVENT_IRIS_1, 0s);
                    break;

                default:
                    break;
            }
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            scheduler
                .Schedule(5ms, [this](TaskContext fireball)
                {
                    DoCastVictim(SPELL_FIREBALL);
                    fireball.Repeat(2s, 5s);
                })
                .Schedule(5s, [this](TaskContext frostbolt)
                {
                    DoCastVictim(SPELL_FROSTBOLT);
                    frostbolt.Repeat(4s, 12s);
                })
                .Schedule(14s, [this](TaskContext frost_rune)
                {
                    DoCastVictim(SPELL_FROST_RUNE);
                    me->PlayDirectSound(28139);
                    frost_rune.Repeat(14s, 22s);
                });
        }

        void EnterEvadeMode(EvadeReason why) override
        {
            ScriptedAI::EnterEvadeMode(why);

            scheduler.CancelAll();
        }

        void Reset() override
        {
            me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
            scheduler.CancelAll();
        }

        void JustDied(Unit* /*killer*/) override
        {
            scheduler.CancelAll();
            events.Reset();
        }

        void UpdateAI(uint32 diff) override
        {
            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                    // Event - Return to Theramore
                    #pragma region THERAMORE

                    case EVENT_RETURN_1:
                        me->GetMotionMaster()->MovePoint(0, -2690.53f, -4765.79f, 13.76f, true, 2.03f);
                        events.ScheduleEvent(EVENT_RETURN_2, 6s);
                        break;

                    case EVENT_RETURN_2:
                        if (Creature * portal = me->SummonCreature(NPC_INVISIBLE_STALKER, -2678.14f, -4774.18f, 14.16f, 2.55f, TEMPSUMMON_TIMED_DESPAWN, 20s))
                        {
                            portal->AddAura(SPELL_OPENED_PORTAL, portal);
                            if (kalecgos = portal->SummonCreature(NPC_KALECGOS, -2678.14f, -4774.18f, 14.16f, 2.55f, TEMPSUMMON_MANUAL_DESPAWN))
                            {
                                kalecgos->SetWalk(true);
                                kalecgos->GetMotionMaster()->MovePoint(0, -2685.95f, -4768.93f, 13.94f, true, 2.55f);
                            }
                        }
                        events.ScheduleEvent(EVENT_RETURN_3, 4s);
                        break;

                    case EVENT_RETURN_3:
                        me->SetFacingToObject(kalecgos);
                        kalecgos->AI()->Talk(SAY_RETURN_1);
                        events.ScheduleEvent(EVENT_RETURN_4, 1s);
                        break;

                    case EVENT_RETURN_4:
                        Talk(SAY_RETURN_2);
                        events.ScheduleEvent(EVENT_RETURN_5, 7s);
                        break;

                    case EVENT_RETURN_5:
                        Talk(SAY_RETURN_3);
                        events.ScheduleEvent(EVENT_RETURN_6, 11s);
                        break;

                    case EVENT_RETURN_6:
                        kalecgos->AI()->Talk(SAY_RETURN_4);
                        events.ScheduleEvent(EVENT_RETURN_7, 15s);
                        break;

                    case EVENT_RETURN_7:
                        Talk(SAY_RETURN_5);
                        events.ScheduleEvent(EVENT_RETURN_8, 6s);
                        break;

                    case EVENT_RETURN_8:
                        kalecgos->AI()->Talk(SAY_RETURN_6);
                        events.ScheduleEvent(EVENT_RETURN_9, 4s);
                        break;

                    case EVENT_RETURN_9:
                        Talk(SAY_RETURN_7);
                        events.ScheduleEvent(EVENT_RETURN_10, 15s);
                        break;

                    case EVENT_RETURN_10:
                        kalecgos->AI()->Talk(SAY_RETURN_8);
                        events.ScheduleEvent(EVENT_RETURN_11, 5s);
                        break;

                    case EVENT_RETURN_11:
                        Talk(SAY_RETURN_9);
                        events.ScheduleEvent(EVENT_RETURN_12, 4s);
                        break;

                    case EVENT_RETURN_12:
                        kalecgos->AI()->Talk(SAY_RETURN_10);
                        events.ScheduleEvent(EVENT_RETURN_13, 7s);
                        break;

                    case EVENT_RETURN_13:
                        kalecgos->AI()->Talk(SAY_RETURN_11);
                        events.ScheduleEvent(EVENT_RETURN_14, 15s);
                        break;

                    case EVENT_RETURN_14:
                        Talk(SAY_RETURN_12);
                        events.ScheduleEvent(EVENT_RETURN_15, 14s);
                        break;

                    case EVENT_RETURN_15:
                        me->CastSpell(me, SPELL_POWER_BALL_VISUAL);
                        events.ScheduleEvent(EVENT_RETURN_16, 1s);
                        break;

                    case EVENT_RETURN_16:
                        me->SetVisible(false);
                        kalecgos->AI()->Talk(SAY_RETURN_13);
                        kalecgos->SetFacingToObject(players[0]);
                        events.ScheduleEvent(EVENT_RETURN_17, 3s);
                        break;

                    case EVENT_RETURN_17:
                        kalecgos->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_SPELL_CAST_OMNI);
                        for (Player* player : players)
                            player->CastSpell(player, SPELL_POWER_BALL_VISUAL);
                        events.ScheduleEvent(EVENT_RETURN_18, 1s);
                        break;

                    case EVENT_RETURN_18:
                        kalecgos->DespawnOrUnsummon(3s);
                        for (Player* player : players)
                        {
                            player->CompleteQuest(QUEST_RETURN_TO_THERAMORE);
                            player->SetPhaseMask(1, true);

                            const float radius = 10.f;

                            Position pos = { -3755.96f, -4479.34f, 0.018f, 0.25f };
                            player->MovePosition(pos, radius * (float)rand_norm(), (float)rand_norm() * static_cast<float>(2 * M_PI));

                            const WorldLocation destination = { 725, pos };

                            player->TeleportTo(destination);
                        }
                        break;

                        #pragma endregion

                    // Event - Protect the artefact
                    #pragma region ARTEFACT

                    case EVENT_IRIS_1:
                        me->GetMotionMaster()->MovePoint(0, -3710.42f, -4468.33f, -20.67f, true, 0.02f);
                        events.ScheduleEvent(EVENT_IRIS_2, 8s);
                        break;

                    case EVENT_IRIS_2:
                        Talk(SAY_IRIS_2);
                        events.ScheduleEvent(EVENT_IRIS_3, 10s);
                        break;

                    case EVENT_IRIS_3:
                    {
                        Quest const* quest = sObjectMgr->GetQuestTemplate(QUEST_DESTROY_THE_DESTROYER);
                        for (Player* player : players)
                        {
                            if (player->CanAddQuest(quest, true))
                                player->AddQuestAndCheckCompletion(quest, me);
                        }

                        if (Creature * dummy = me->SummonCreature(NPC_INVISIBLE_STALKER, -3698.69f, -4467.94f, -20.87f, 3.55f, TEMPSUMMON_MANUAL_DESPAWN))
                        {
                            me->CastSpell(dummy, SPELL_CANALISATION);
                            channelTarget = dummy;
                        }

                        break;
                    }

                    case EVENT_IRIS_4:
                    {
                        Quest const* destroyTheDestroyer = sObjectMgr->GetQuestTemplate(QUEST_DESTROY_THE_DESTROYER);
                        Quest const* protectTheArtefact = sObjectMgr->GetQuestTemplate(QUEST_PROTECT_THE_ARTEFACT);
                        for (Player* player : players)
                        {
                            player->RewardQuest(destroyTheDestroyer, 0, me);
                            if (player->CanAddQuest(protectTheArtefact, true))
                                player->AddQuestAndCheckCompletion(protectTheArtefact, me);
                        }

                        channelTarget->DespawnOrUnsummon();
                        me->RemoveAllAuras();

                        events.ScheduleEvent(EVENT_IRIS_5, 3s);
                        break;
                    }

                    case EVENT_IRIS_5:
                        me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_SPELL_CAST_OMNI);
                        me->CastSpell(me, 62849, true);
                        for (uint8 i = 0; i < 2; ++i)
                            elementals[i] = me->SummonCreature(NPC_WATER_ELEMENTAL, ElementalsPos[i], TEMPSUMMON_CORPSE_TIMED_DESPAWN, 3s);
                        events.ScheduleEvent(EVENT_IRIS_6, 1s);
                        break;

                    case EVENT_IRIS_6:
                        me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_NONE);
                        events.ScheduleEvent(EVENT_IRIS_7, 2s);
                        break;

                    case EVENT_IRIS_7:
                        me->SetReactState(REACT_PASSIVE);
                        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                        if (Creature* dummy = me->SummonCreature(NPC_INVISIBLE_STALKER, -3698.69f, -4467.94f, -22.15f, 3.55f, TEMPSUMMON_MANUAL_DESPAWN))
                        {
                            me->CastSpell(dummy, SPELL_ARCANE_CANALISATION, true);
                            channelTarget = dummy;
                        }
                        for (uint8 i = 0; i < 2; ++i)
                        {
                            elementals[i]->GetMotionMaster()->MovePoint(0, ElementalsPos[i + 2], false, ElementalsPos[i + 2].GetOrientation());
                            elementals[i]->SetHomePosition(ElementalsPos[i + 2]);
                        }
                        events.ScheduleEvent(EVENT_IRIS_8, 5s);
                        break;

                    case EVENT_IRIS_8:
                        for (int i = 0; i < 12; ++i)
                        {
                            uint32 entry = Soldiers[i].entry;
                            if (!entry)
                                entry = RAND(NPC_ROK_NAH_GRUNT, NPC_ROK_NAH_SOLDIER, NPC_ROK_NAH_HAG, NPC_ROK_NAH_FELCASTER, NPC_ROK_NAH_LOA_SINGER);

                            if (Creature * temp = me->SummonCreature(entry, Soldiers[i].position, TEMPSUMMON_MANUAL_DESPAWN))
                            {
                                temp->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                                temp->SetReactState(REACT_PASSIVE);

                                if (entry == NPC_WARLORD_ROK_NAH)
                                    warlord = temp;
                                else
                                    soldiers.push_back(temp);
                            }
                        }
                        events.ScheduleEvent(EVENT_IRIS_9, 2s);
                        break;

                    case EVENT_IRIS_9:
                        warlord->AI()->Talk(SAY_IRIS_4);
                        events.ScheduleEvent(EVENT_IRIS_10, 7s);
                        break;

                    case EVENT_IRIS_10:
                        Talk(SAY_IRIS_5);
                        events.ScheduleEvent(EVENT_IRIS_11, 9s);
                        break;

                    case EVENT_IRIS_11:
                        warlord->AI()->Talk(SAY_IRIS_6);
                        events.ScheduleEvent(EVENT_IRIS_12, 6s);
                        break;

                    case EVENT_IRIS_12:
                        Talk(SAY_IRIS_7);
                        events.ScheduleEvent(EVENT_IRIS_13, 9s);
                        break;

                    case EVENT_IRIS_13:
                        warlord->AI()->Talk(SAY_IRIS_8);
                        events.ScheduleEvent(EVENT_IRIS_14, 3s);
                        break;

                    case EVENT_IRIS_14:
                    {
                        warlord->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                        warlord->SetReactState(REACT_AGGRESSIVE);
                        warlord->Attack(players[0], true);
                        warlord->GetMotionMaster()->MoveChase(players[0], 1.f);

                        for (Creature* soldier : soldiers)
                        {
                            soldier->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                            soldier->SetReactState(REACT_AGGRESSIVE);

                            if (soldier->GetPositionY() <= -4468.18f)
                            {
                                soldier->Attack(elementals[1], true);
                                soldier->GetMotionMaster()->MoveChase(elementals[1], 1.f);
                            }
                            else
                            {
                                soldier->Attack(elementals[0], true);
                                soldier->GetMotionMaster()->MoveChase(elementals[0], 1.f);
                            }
                        }

                        events.ScheduleEvent(EVENT_IRIS_15, 5s);
                        break;
                    }

                    case EVENT_IRIS_15:
                    {
                        if (warlord->GetHealthPct() < 10.f || players[0]->GetHealthPct() <= 20.f)
                        {
                            warlord->RemoveAllAuras();
                            warlord->SetImmuneToAll(true);
                            warlord->SetRegenerateHealth(false);
                            warlord->SetControlled(true, UNIT_STATE_ROOT);

                            float currentHealth = warlord->GetHealth() * 0.5f;
                            warlord->SetHealth((uint32)currentHealth);

                            events.CancelEvent(EVENT_IRIS_15);
                            events.ScheduleEvent(EVENT_IRIS_16, 1s);
                        }
                        else
                            events.RescheduleEvent(EVENT_IRIS_15, 1s);
                        break;
                    }

                    case EVENT_IRIS_16:
                    {
                        for (Player* player : players)
                            player->ResurrectPlayer(10.f);

                        warlord->CombatStop(true);
                        warlord->GetMotionMaster()->Clear();
                        warlord->GetMotionMaster()->MoveIdle();
                        warlord->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                        warlord->SetReactState(REACT_PASSIVE);
                        warlord->SetStandState(UNIT_STAND_STATE_KNEEL);

                        channelTarget->DespawnOrUnsummon();
                        me->RemoveAllAuras();

                        CastSpellExtraArgs args;
                        args.SetTriggerFlags(TRIGGERED_CAST_DIRECTLY);
                        me->CastSpell(me, SPELL_ICE_NOVA, args);
                        me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_SPELL_CAST_OMNI);

                        for (Creature* soldier : soldiers)
                        {
                            if (!soldier || (soldier && !soldier->IsAlive()))
                                continue;

                            Unit::Kill(me, soldier);
                        }

                        for (uint8 i = 0; i < 2; ++i)
                        {
                            if (!elementals[i] || (elementals[i] && !elementals[i]->IsAlive()))
                                continue;

                            elementals[i]->GetMotionMaster()->Clear();
                            elementals[i]->GetMotionMaster()->MovePoint(0, ElementalsPos[i], false, ElementalsPos[i].GetOrientation());
                            elementals[i]->SetHomePosition(ElementalsPos[i]);
                        }

                        events.ScheduleEvent(EVENT_IRIS_17, 2s);
                        break;
                    }

                    case EVENT_IRIS_17:
                        me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_NONE);
                        if (GameObject * temp = GetClosestGameObjectWithEntry(me, GAMEOBJECT_CRYSTAL, 100))
                        {
                            float distanceToTravel = me->GetExactDist2d(temp->GetPosition()) - 1.5f;
                            float angle = me->GetAbsoluteAngle(temp);
                            float destx = me->GetPositionX() + distanceToTravel * cosf(angle);
                            float desty = me->GetPositionY() + distanceToTravel * sinf(angle);

                            me->GetMotionMaster()->MovePoint(0, destx, desty, temp->GetPositionZ());
                            mirror = temp;
                        }
                        events.ScheduleEvent(EVENT_IRIS_18, 7s);
                        break;

                    case EVENT_IRIS_18:
                        me->SetStandState(UNIT_STAND_STATE_KNEEL);
                        events.ScheduleEvent(EVENT_IRIS_19, 2s);
                        break;

                    case EVENT_IRIS_19:
                        mirror->Delete();
                        me->SetStandState(UNIT_STAND_STATE_STAND);
                        events.ScheduleEvent(EVENT_IRIS_20, 2s);
                        break;

                    case EVENT_IRIS_20:
                    {
                        float distanceToTravel = me->GetExactDist2d(warlord->GetPosition()) - 1.5f;
                        float angle = me->GetAbsoluteAngle(warlord);
                        float destx = me->GetPositionX() + distanceToTravel * cosf(angle);
                        float desty = me->GetPositionY() + distanceToTravel * sinf(angle);

                        me->GetMotionMaster()->MovePoint(0, destx, desty, warlord->GetPositionZ());

                        events.ScheduleEvent(EVENT_IRIS_21, Milliseconds((uint64)((distanceToTravel / me->GetSpeed(MOVE_WALK)) + 2) * IN_MILLISECONDS));
                        break;
                    }

                    case EVENT_IRIS_21:
                    {
                        Quest const* protectTheArtefact = sObjectMgr->GetQuestTemplate(QUEST_PROTECT_THE_ARTEFACT);
                        for (Player* player : players)
                        {
                            player->CompleteQuest(QUEST_PROTECT_THE_ARTEFACT);
                            player->RewardQuest(protectTheArtefact, 0, me);
                        }

                        Unit::Kill(me, warlord);

                        me->HandleEmoteCommand(EMOTE_ONESHOT_ATTACK1H);
                        events.ScheduleEvent(EVENT_IRIS_22, 2s);
                        break;
                    }

                    case EVENT_IRIS_22:
                        Talk(SAY_IRIS_9);
                        me->SetFacingToObject(players[0]);
                        events.ScheduleEvent(EVENT_IRIS_23, 8s);
                        break;

                    case EVENT_IRIS_23:
                        Talk(SAY_IRIS_10);
                        events.ScheduleEvent(EVENT_IRIS_24, 12s);
                        break;

                    case EVENT_IRIS_24:
                        if (elementals[0] && elementals[0]->IsAlive()) elementals[0]->DespawnOrUnsummon();
                        if (elementals[1] && elementals[1]->IsAlive()) elementals[1]->DespawnOrUnsummon();
                        me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_SPELL_CAST_OMNI);
                        me->CastSpell(me, SPELL_POWER_BALL_VISUAL);
                        events.ScheduleEvent(EVENT_IRIS_25, 1830ms);
                        break;

                    case EVENT_IRIS_25:
                        for (Player* player : players)
                            SendMailToPlayer(player);
                        me->SetVisible(false);
                        me->SummonGameObject(500015, me->GetPosition(), QuaternionData(), 0s);
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
        EventMap events;
        std::vector<Creature*> soldiers;
        Creature* kalecgos;
        Creature* warlord;
        Creature* channelTarget;
        Creature* elementals[2];
        std::vector<Player*> players;
        GameObject* mirror;
        TaskScheduler scheduler;
        bool canSayIntro, canSayEnd, canSayDestruction;
        #pragma endregion

        void SendMailToPlayer(Player* player) const
        {
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            int16 deliverDelay = irand(MAIL_DELIVER_DELAY_MIN, MAIL_DELIVER_DELAY_MAX);
            MailDraft(MAIL_TIDES_ENTRY, true).SendMailTo(trans, MailReceiver(player), MailSender(MAIL_CREATURE, me->GetEntry()), MAIL_CHECK_MASK_NONE, deliverDelay);
            CharacterDatabase.CommitTransaction(trans);
        }
    };

    CreatureAI* GetAI(Creature * creature) const override
    {
        return new npc_jaina_ruinsAI(creature);
    }
};

class npc_warlord_roknah : public CreatureScript
{
    public:
    npc_warlord_roknah() : CreatureScript("npc_warlord_roknah") {}

    struct npc_warlord_roknahAI : public ScriptedAI
    {
        npc_warlord_roknahAI(Creature* creature) : ScriptedAI(creature)
        {
            Initialize();
        }

        void Initialize() { }

        void Reset() override { }

        void DamageTaken(Unit* /*attacker*/, uint32& damage) override
        {
            if (damage >= me->GetMaxHealth() || HealthBelowPct(10))
                damage = 0;
        }

        void UpdateAI(uint32 diff) override
        {
            // Combat
            if (!UpdateVictim())
                return;

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature * creature) const override
    {
        return new npc_warlord_roknahAI(creature);
    }
};

class npc_kruntog_gatecrusher : public CreatureScript
{
public:
    npc_kruntog_gatecrusher() : CreatureScript("npc_kruntog_gatecrusher") { }

    struct npc_kruntog_gatecrusherAI : public VehicleAI
    {
        npc_kruntog_gatecrusherAI(Creature* creature) : VehicleAI(creature)
        {
            Initialize();
        }

        void Initialize()
        {

        }

        void Reset() override
        {
            VehicleAI::Reset();

            Initialize();
        }

        void AttackStart(Unit* who) override
        {
            if (!who)
                return;

            if (me->Attack(who, true))
            {
                me->GetMotionMaster()->MoveChase(who, 5.f);
            }
        }

        void UpdateAI(uint32 diff) override
        {

        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_kruntog_gatecrusherAI(creature);
    }
};

void AddSC_theramore_ruins()
{
    new npc_jaina_ruins();
    new npc_warlord_roknah();
    new npc_kruntog_gatecrusher();
}
