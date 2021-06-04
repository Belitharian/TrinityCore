#include "ScriptMgr.h"
#include "Map.h"
#include "GameObject.h"
#include "ObjectAccessor.h"
#include "CreatureData.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "CreatureAIImpl.h"
#include "MotionMaster.h"
#include "theramore.h"

#include <iostream>

constexpr uint8 NUMBER_OF_MEMBERS = 20;

const Position JainaHomePos = { -3658.39f, -4372.87f, 9.35f, 0.69f };

enum Invoker
{
    // Events
    EVENT_START_WAR             = 1,
    EVENT_BATTLE_1,
    EVENT_BATTLE_2,
    EVENT_BATTLE_3,
    EVENT_BATTLE_4,
    EVENT_BATTLE_5,
    EVENT_BATTLE_6,
    EVENT_BATTLE_7,
    EVENT_BATTLE_8,
    EVENT_BATTLE_9,

    // NPCs
    NPC_WARLORD_ROK_NAH         = 100039,
    NPC_ROK_NAH_GRUNT           = 100034,
    NPC_ROK_NAH_SOLDIER         = 100029,
    NPC_ROK_NAH_HAG             = 100030,
    NPC_ROK_NAH_FELCASTER       = 100031,
    NPC_ROK_NAH_LOA_SINGER      = 100032,
    NPC_THERAMORE_WAVES         = 100035,
    NPC_GATECRUSHER             = 100040,
    NPC_IRONWORK_CANNON         = 33264,

    // Textes
    JAINA_SAY_01                = 32,
    JAINA_SAY_02                = 33,
    JAINA_SAY_03                = 34,
    JAINA_SAY_04                = 35,
    JAINA_SAY_WAVE_ALERT        = 36,
    JAINA_SAY_WAVE_CITADEL      = 37,
    JAINA_SAY_WAVE_DOORS        = 38,
    JAINA_SAY_WAVE_DOCKS        = 39,

    // Wave types
    WAVE_DOORS                  = 0,
    WAVE_CITADEL                = 1,
    WAVE_DOCKS                  = 2
};

enum Waves
{
    WAVE_01                     = 100,
    WAVE_01_CHECK,
    WAVE_02,
    WAVE_02_CHECK,
    WAVE_03,
    WAVE_03_CHECK,
    WAVE_04,
    WAVE_04_CHECK,
    WAVE_05,
    WAVE_05_CHECK,
    WAVE_06,
    WAVE_06_CHECK,
    WAVE_07,
    WAVE_07_CHECK,
    WAVE_08,
    WAVE_08_CHECK,
    WAVE_09,
    WAVE_09_CHECK,
    WAVE_10,
    WAVE_10_CHECK,
    WAVE_EXIT
};

class KalecgosFlightEvent : public BasicEvent
{
    public:
    KalecgosFlightEvent(Creature* owner) : owner(owner)
    {
        spellArgs.SetTriggerFlags(TRIGGERED_CAST_DIRECTLY);
        spellArgs.SetTriggerFlags(TRIGGERED_IGNORE_SET_FACING);
        spellArgs.SetTriggerFlags(TRIGGERED_IGNORE_AURA_INTERRUPT_FLAGS);

        owner->SetReactState(REACT_PASSIVE);
    }

    bool Execute(uint64 eventTime, uint32 /*updateTime*/) override
    {
        if (roll_chance_i(20))
            owner->AI()->Talk(RAND(SAY_KALECGOS_1, SAY_KALECGOS_2));

        owner->CastSpell(owner, SPELL_FROST_BREEZE, spellArgs);
        owner->GetThreatManager().RemoveMeFromThreatLists();
        owner->m_Events.AddEvent(this, Milliseconds(eventTime + 5000));
        return false;
    }

    private:
    Creature* owner;
    CastSpellExtraArgs spellArgs;
};

class CannonDoorsEvent : public BasicEvent
{
    public:
    CannonDoorsEvent(Creature* owner, Player* playerForQuest) : owner(owner), playerForQuest(playerForQuest)
    {
    }

    bool Execute(uint64 eventTime, uint32 /*updateTime*/) override
    {
        playerForQuest->PlayDirectSound(RAND(11564, 11565, 11566, 11567));
        playerForQuest->CastSpell(playerForQuest, 12816);
        owner->CastSpell(owner, 71495);
        owner->m_Events.AddEvent(this, Milliseconds(eventTime + urand(300, 800)));
        return false;
    }

    private:
    Creature* owner;
    Player* playerForQuest;
};

class HordeDoorsEvent : public BasicEvent
{
    public:
    HordeDoorsEvent(Creature* owner) : owner(owner)
    {
    }

    bool Execute(uint64 eventTime, uint32 /*updateTime*/) override
    {
        Position pos;
        pos.m_positionX = minPosX + (rand() % static_cast<int>(maxPosX - minPosX + 1));
        pos.m_positionY = -4252.81f;
        pos.m_positionZ = minPosZ + (rand() % static_cast<int>(maxPosZ - minPosZ + 1));

        float ori = owner->GetPosition().GetAbsoluteAngle(pos);
        owner->SetFacingTo(ori);

        owner->CastSpell(pos, 100111, true);
        owner->m_Events.AddEvent(this, Milliseconds(eventTime + urand(1500, 2300)));

        return false;
    }

    private:
    Creature* owner;
    const float minPosX = -3790.04f;
    const float maxPosX = -3776.74f;
    const float minPosZ = 10.f;
    const float maxPosZ = 20.f;
};

class theramore_waves_invoker : public CreatureScript
{
    public:
    theramore_waves_invoker() : CreatureScript("theramore_waves_invoker") {}

    struct theramore_waves_invokerAI : public ScriptedAI
    {
        theramore_waves_invokerAI(Creature* creature) : ScriptedAI(creature)
        {
            Initialize();
        }

        void Initialize()
        {
            waves = 0;
            wavesInvoker = WAVE_01;
        }

        void SetGUID(ObjectGuid const& guid, int32 id = 0) override
        {
            if (id == TYPEID_PLAYER)
                playerForQuest = ObjectAccessor::GetPlayer(*me, guid);
        }

        void SetData(uint32 id, uint32 value) override
        {
            if (id == EVENT_START_WAR)
            {
                wavesInvoker = value == 2 ? WAVE_10 : WAVE_01;

                jaina = GetClosestCreatureWithEntry(me, NPC_JAINA_PROUDMOORE, 20.f);
                thalen = GetClosestCreatureWithEntry(me, NPC_THALEN_SONGWEAVER, 20.f);
                kalecgos = GetClosestCreatureWithEntry(me, NPC_KALECGOS_DRAGON, 2000.f);
                amara = GetClosestCreatureWithEntry(me, NPC_AMARA_LEESON, 20.f);

                jaina->AI()->Talk(JAINA_SAY_01);
                jaina->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);

                events.ScheduleEvent(EVENT_BATTLE_1, 5s);
            }
        }

        void Reset() override
        {
            events.Reset();
            Initialize();
        }

        void UpdateAI(uint32 diff) override
        {
            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                    // Event - Battle
                    case EVENT_BATTLE_1:
                        if (Creature* cannon = DoSummon(NPC_INVISIBLE_STALKER, { -3646.48f, -4362.23f, 9.57f, 0.70f }, 9s, TEMPSUMMON_TIMED_DESPAWN))
                        {
                            jaina->AI()->Talk(JAINA_SAY_02);
                            jaina->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_READY2HL);
                            cannon->m_Events.AddEvent(new CannonDoorsEvent(cannon, playerForQuest), cannon->m_Events.CalculateTime(5ms));
                        }
                        events.ScheduleEvent(EVENT_BATTLE_2, 8s);
                        break;

                    case EVENT_BATTLE_2:
                    {
                        playerForQuest->PlayDirectSound(11563);

                        if (Creature* barrier = GetClosestCreatureWithEntry(me, NPC_INVISIBLE_STALKER, 20.f))
                        {
                            barrier->CastSpell(barrier, 70444);
                            barrier->RemoveAllAuras();
                            barrier->DespawnOrUnsummon(2s);
                        }

                        if (GameObject* gate = GetClosestGameObjectWithEntry(me, GOB_THERAMORE_GATE, 35.f))
                        {
                            gate->SetLootState(GO_READY);
                            gate->UseDoorOrButton();
                        }

                        if (gatecrusher = me->SummonCreature(NPC_GATECRUSHER, -3790.50f, -4165.00f, 7.57f, 4.78f, TEMPSUMMON_MANUAL_DESPAWN))
                        {
                            gatecrusher->SetImmuneToNPC(true);
                            gatecrusher->SetImmuneToPC(true);

                            if (cannon = me->SummonCreature(NPC_IRONWORK_CANNON, -3783.45f, -4190.19f, 7.87f, 4.67f, TEMPSUMMON_MANUAL_DESPAWN))
                            {
                                cannon->SetDisplayId(25723);
                                cannon->SetImmuneToNPC(true);
                                cannon->SetImmuneToPC(true);
                                cannon->m_Events.AddEvent(new HordeDoorsEvent(cannon), cannon->m_Events.CalculateTime(5s));
                            }

                            if (warlord = me->SummonCreature(NPC_WARLORD_ROK_NAH, -3777.98f, -4178.72f, 7.86f, 4.65f, TEMPSUMMON_MANUAL_DESPAWN))
                            {
                                warlord->SetImmuneToNPC(true);
                                warlord->SetImmuneToPC(true);
                            }

                            for (uint8 i = 0; i < 10; i++)
                            {
                                uint32 entry = RAND(NPC_ROK_NAH_GRUNT, NPC_ROK_NAH_SOLDIER, NPC_ROK_NAH_FELCASTER, NPC_ROK_NAH_HAG, NPC_ROK_NAH_LOA_SINGER);
                                if (Creature* hordeMember = me->SummonCreature(entry, SoldiersLocation[i].position, TEMPSUMMON_MANUAL_DESPAWN))
                                {
                                    hordeMember->SetImmuneToNPC(true);
                                    hordeMember->SetImmuneToPC(true);

                                    switch (entry)
                                    {
                                        case NPC_ROK_NAH_FELCASTER:
                                        case NPC_ROK_NAH_HAG:
                                            hordeMember->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_READY2HL);
                                            break;
                                        default:
                                            hordeMember->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_READY1H);
                                            break;
                                    }

                                    tempMembers.push_back(hordeMember);
                                }
                            }
                        }

                        amara->RemoveAllAuras();
                        amara->CastSpell(amara, 54899);
                        playerForQuest->CastSpell(playerForQuest, 54899);
                        thalen->SetWalk(false);
                        thalen->RemoveAllAuras();
                        thalen->CastSpell(thalen, 32404);
                        thalen->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_LAUGH);
                        events.ScheduleEvent(EVENT_BATTLE_3, 3s);
                        break;
                    }

                    case EVENT_BATTLE_3:
                        jaina->AI()->Talk(JAINA_SAY_03);
                        jaina->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_NONE);
                        amara->GetMotionMaster()->MoveTargetedHome();
                        thalen->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_NONE);
                        thalen->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                        thalen->SetReactState(REACT_PASSIVE);
                        thalen->SetFaction(85);
                        thalen->GetMotionMaster()->MovePoint(0, -3609.83f, -4333.48f, 10.29f, false, 3.82f);
                        events.ScheduleEvent(EVENT_BATTLE_4, 6s);
                        break;

                    case EVENT_BATTLE_4:
                        jaina->AI()->Talk(JAINA_SAY_04);
                        if (Creature* fx = DoSummon(NPC_INVISIBLE_STALKER, jaina->GetPosition(), 5s, TEMPSUMMON_TIMED_DESPAWN))
                            fx->CastSpell(fx, SPELL_TELEPORT, true);
                        jaina->NearTeleportTo(-3612.43f, -4335.63f, 10.29f, 0.72f);
                        events.ScheduleEvent(EVENT_BATTLE_5, 1s);
                        break;

                    case EVENT_BATTLE_5:
                    {
                        CastSpellExtraArgs args;
                        args.SetTriggerFlags(TRIGGERED_CAST_DIRECTLY);
                        jaina->CastSpell(thalen, SPELL_ICE_NOVA, args);
                        thalen->AddAura(SPELL_ICE_BLOCK, thalen);
                        events.ScheduleEvent(EVENT_BATTLE_6, 2s);
                        break;
                    }

                    case EVENT_BATTLE_6:
                        jaina->CastSpell(jaina, SPELL_SIMPLE_TELEPORT);
                        events.ScheduleEvent(EVENT_BATTLE_7, 2s);
                        break;

                    case EVENT_BATTLE_7:
                        thalen->CastSpell(thalen, SPELL_POWER_BALL_VISUAL);
                        events.ScheduleEvent(EVENT_BATTLE_8, 2s);
                        break;

                    case EVENT_BATTLE_8:
                        thalen->NearTeleportTo(-3727.50f, -4555.78f, 4.74f, 2.82f);
                        events.ScheduleEvent(EVENT_BATTLE_9, 2s);
                        break;

                    case EVENT_BATTLE_9:
                        thalen->RemoveAllAuras();
                        if (Creature* medic = DoSummon(NPC_THERAMORE_MEDIC, { -3736.40f, -4553.58f, 4.74f, 5.99f }, 0s, TEMPSUMMON_MANUAL_DESPAWN))
                            medic->CastSpell(thalen, SPELL_CHAINS);
                        thalen->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_STRANGULATE);
                        jaina->CastSpell(jaina, SPELL_TELEPORT, true);
                        jaina->NearTeleportTo(-3658.39f, -4372.87f, 9.35f, 0.69f);
                        kalecgos->m_Events.AddEvent(new KalecgosFlightEvent(kalecgos), kalecgos->m_Events.CalculateTime(3s));
                        events.ScheduleEvent(WAVE_01, 5s);
                        break;

                    // Event - Invoker
                    case WAVE_01:
                    case WAVE_02:
                    case WAVE_03:
                    case WAVE_04:
                    case WAVE_05:
                    case WAVE_06:
                    case WAVE_07:
                    case WAVE_08:
                    case WAVE_09:
                    case WAVE_10:
                        HordeMembersInvoker(waves, hordeMembers);
                        waves = RAND(WAVE_DOORS, WAVE_CITADEL, WAVE_DOCKS);
                        events.ScheduleEvent(++wavesInvoker, 1s);
                        break;

                    case WAVE_01_CHECK:
                    case WAVE_02_CHECK:
                    case WAVE_03_CHECK:
                    case WAVE_04_CHECK:
                    case WAVE_05_CHECK:
                    case WAVE_06_CHECK:
                    case WAVE_07_CHECK:
                    case WAVE_08_CHECK:
                    case WAVE_09_CHECK:
                    case WAVE_10_CHECK:
                    {
                        uint32 membersCounter = 0;
                        uint32 deadCounter = 0;
                        for (uint8 i = 0; i < NUMBER_OF_MEMBERS; ++i)
                        {
                            ++membersCounter;
                            Creature* temp = ObjectAccessor::GetCreature(*me, hordeMembers[i]);
                            if (!temp || temp->isDead())
                                ++deadCounter;
                        }

                        // Quand le nombre de membres vivants est inférieur ou égal au nom de membres morts
                        if (membersCounter <= deadCounter)
                        {
                            playerForQuest->KilledMonsterCredit(NPC_THERAMORE_WAVES);
                            events.ScheduleEvent(++wavesInvoker, 2s);
                        }
                        else
                            events.ScheduleEvent(wavesInvoker, 1s);
                        break;
                    }

                    case WAVE_EXIT:
                    {
                        playerForQuest->CompleteQuest(QUEST_PREPARE_FOR_WAR);
                        jaina->NearTeleportTo(JainaHomePos);
                        jaina->SetHomePosition(JainaHomePos);
                        jaina->AI()->SetData(EVENT_STOP_KALECGOS, 1U);
                        jaina->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
                        kalecgos->DespawnOrUnsummon();

                        if (gatecrusher) gatecrusher->DespawnOrUnsummon();
                        if (warlord) warlord->DespawnOrUnsummon();
                        if (cannon) cannon->DespawnOrUnsummon();

                        if (!tempMembers.empty())
                        {
                            for (Creature* member : tempMembers)
                                member->DespawnOrUnsummon();
                        }

                        if (playerForQuest && playerForQuest->IsWithinDist(jaina, 25.f))
                        {
                            Position playerPos = GetRandomPosition(JainaHomePos, 3.f);

                            // Si le joueur est à plus de 25 mètre de la destination d'attaque
                            float distance = playerPos.GetExactDist2d(playerForQuest->GetPosition());
                            if (distance > 25.0f)
                            {
                                playerForQuest->NearTeleportTo(playerPos);
                            }
                        }

                        break;
                    }

                    default:
                        break;
                }
            }
        }

        private:
        EventMap events;
        Creature* jaina;
        Creature* thalen;
        Creature* amara;
        Creature* kalecgos;
        Creature* gatecrusher;
        Creature* warlord;
        Creature* cannon;
        std::vector<Creature*> tempMembers;
        Player* playerForQuest;
        ObjectGuid hordeMembers[NUMBER_OF_MEMBERS];
        uint32 waves;
        uint32 wavesInvoker;

        Position GetPositionAround(Unit* target, double angle, float radius)
        {
            double ang = angle * (M_PI / 180);
            Position pos;
            pos.m_positionX = target->GetPositionX() + radius * sin(ang);
            pos.m_positionY = target->GetPositionY() + radius * cos(ang);
            pos.m_positionZ = target->GetPositionZ();
            return pos;
        }

        void HordeMembersInvoker(uint32 waveId, ObjectGuid* hordes)
        {
            for (uint32 i = 0; i < NUMBER_OF_MEMBERS; ++i)
            {
                uint32 entry = RAND(NPC_ROK_NAH_GRUNT, NPC_ROK_NAH_SOLDIER, NPC_ROK_NAH_FELCASTER, NPC_ROK_NAH_HAG, NPC_ROK_NAH_LOA_SINGER);
                Position pos;

                switch (waveId)
                {
                    case WAVE_DOORS:
                        pos = GetRandomPosition({ -3608.66f, -4332.66f, 11.62f, 3.83f }, 2.f);
                        break;

                    case WAVE_CITADEL:
                        pos = GetRandomPosition({ -3669.10f, -4507.06f, 11.62f, 0.f }, 20.f);
                        break;

                    case WAVE_DOCKS:
                        pos = GetRandomPosition({ -3823.47f, -4536.42f, 11.62f, 0.f }, 25.f);
                        break;
                }

                if (Creature* temp = DoSummon(entry, pos, 15min, TEMPSUMMON_CORPSE_TIMED_DESPAWN))
                {
                    if (waveId == WAVE_DOORS)
                    {
                        Position dest = GetRandomPosition(JainaHomePos, 5.f);
                        temp->GetMotionMaster()->MovePoint(0, dest);
                    }

                    float x, y, z;
                    temp->GetPosition(x, y, z);
                    temp->UpdateGroundPositionZ(x, y, z);
                    temp->UpdatePosition(x, y, z, 0);

                    hordes[i] = temp->GetGUID();
                }
            }

            SendJainaWarning(waveId);
        }

        void SendJainaWarning(uint8 spawnNumber)
        {
            uint8 groupId = 0;
            Position position;
            switch (spawnNumber)
            {
                // Portes
                case WAVE_DOORS:
                    position = JainaHomePos;
                    groupId = JAINA_SAY_WAVE_DOORS;
                    break;

                // Citadelle
                case WAVE_CITADEL:
                    position = { -3668.74f, -4511.64f, 10.09f, 1.54f };
                    groupId = JAINA_SAY_WAVE_CITADEL;
                    break;

                // Docks
                case WAVE_DOCKS:
                    position = { -3826.84f, -4539.05f, 9.21f };
                    groupId = JAINA_SAY_WAVE_DOCKS;
                    break;
            }

            jaina->NearTeleportTo(position);
            jaina->SetHomePosition(position);
            jaina->AI()->Talk(JAINA_SAY_WAVE_ALERT);
            jaina->AI()->Talk(groupId);

            if (playerForQuest && playerForQuest->IsWithinDist(jaina, 25.f))
            {
                Position playerPos = GetRandomPosition(position, 3.f);

                // Si le joueur est à plus de 25 mètre de la destination d'attaque
                float distance = playerPos.GetExactDist2d(playerForQuest->GetPosition());
                if (distance > 25.0f)
                {
                    playerForQuest->NearTeleportTo(playerPos);
                    playerForQuest->CastSpell(playerForQuest, SPELL_TELEPORT);
                }
            }
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
        return new theramore_waves_invokerAI(creature);
    }
};

void AddSC_theramore_waves_invoker()
{
    new theramore_waves_invoker();
}
