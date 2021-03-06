#include "ScriptMgr.h"
#include "Map.h"
#include "GameObject.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "CreatureAIImpl.h"
#include "MotionMaster.h"
#include "Group.h"

#define VARIAN_PATH_SIZE 4

enum Misc
{
    // Quests
    QUEST_WHAT_HAD_TO_BE_DONE   = 80019,

    // Phasemask
    PHASEMASK_EVENT             = 4,

    // Gameobjects
    GOB_PORTAL_TO_DALARAN       = 500019
};

enum NPCs
{
    NPC_JAINA_PROUDMOORE        = 100066,
    NPC_VARIAN_WRYNN            = 100064,
    NPC_ANDUIN_WRYNN            = 100041
};

enum Spells
{
    SPELL_TELEPORT              = 51347,
    SPELL_INVISIBILITY          = 16380
};

enum Texts
{
    SAY_VARIAN_01               = 0,
    SAY_VARIAN_02,
    SAY_VARIAN_03,
    SAY_VARIAN_04,
    SAY_VARIAN_05,
    SAY_VARIAN_06,
    SAY_VARIAN_07,
    SAY_VARIAN_08,

    SAY_JAINA_01                = 0,
    SAY_JAINA_02,
    SAY_JAINA_03,
    SAY_JAINA_04,
    SAY_JAINA_05,
    SAY_JAINA_06,
    SAY_JAINA_07,
    SAY_JAINA_08,
};

enum class Phases
{
    None,
    Progress,
    Done
};

const Position varianPath[VARIAN_PATH_SIZE] =
{
    { -8441.49f, 333.27f, 122.58f, 2.24f },
    { -8443.04f, 335.23f, 122.16f, 2.28f },
    { -8449.56f, 337.40f, 121.32f, 3.60f },
    { -8455.53f, 334.51f, 120.88f, 3.16f }
};

const Position varianPathReversed[VARIAN_PATH_SIZE] =
{
    { -8455.53f, 334.51f, 120.88f, 3.16f },
    { -8449.56f, 337.40f, 121.32f, 3.60f },
    { -8443.04f, 335.23f, 122.16f, 2.28f },
    { -8441.49f, 333.27f, 122.58f, 2.24f }
};

class stormwind_final_event : public CreatureScript
{
    public:
    stormwind_final_event() : CreatureScript("stormwind_final_event") {}

    struct stormwind_final_eventAI : public ScriptedAI
    {
        stormwind_final_eventAI(Creature* creature) : ScriptedAI(creature), phase(Phases::None)
        {
            Initialize();
        }

        void Initialize() { }

        void Reset() override
        {
            events.Reset();
            Initialize();
        }

        void MoveInLineOfSight(Unit* who) override
        {
            if (who->GetTypeId() != TYPEID_PLAYER || phase != Phases::None)
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

            for (Player* player : players)
            {
                if (player->GetPhaseMask() == PHASEMASK_EVENT
                    && me->IsFriendlyTo(player)
                    && player->GetQuestStatus(QUEST_WHAT_HAD_TO_BE_DONE) == QUEST_STATUS_COMPLETE)
                {
                    continue;
                }
                return;
            }

            if (me->IsWithinDist(who, 6.f, false))
            {
                phase = Phases::Progress;

                jaina = GetClosestCreatureWithEntry(me, NPC_JAINA_PROUDMOORE, 35.f);
                varian = GetClosestCreatureWithEntry(me, NPC_VARIAN_WRYNN, 35.f);
                anduin = GetClosestCreatureWithEntry(me, NPC_ANDUIN_WRYNN, 35.f);

                jaina->SetWalk(true);
                varian->SetWalk(true);
                varian->SetSheath(SHEATH_STATE_UNARMED);
                varian->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);

                events.ScheduleEvent(1, 2s);
            }
        }

        void UpdateAI(uint32 diff) override
        {
            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                    case 1:
                        jaina->RemoveAllAuras();
                        varian->GetMotionMaster()->MoveSmoothPath(0, varianPath, VARIAN_PATH_SIZE, true);
                        events.ScheduleEvent(2, 200ms);
                        break;

                    case 2:
                        jaina->CastSpell(jaina, SPELL_TELEPORT);
                        events.ScheduleEvent(3, 2s);
                        break;

                    case 3:
                        varian->AI()->Talk(SAY_VARIAN_01);
                        jaina->GetMotionMaster()->MovePoint(0, -8461.18f, 333.342f, 120.88f, true, 6.15f);
                        events.ScheduleEvent(4, 4s);
                        break;

                    case 4:
                        jaina->AI()->Talk(SAY_JAINA_01);
                        jaina->SetFacingToObject(varian);
                        varian->SetFacingToObject(jaina);
                        anduin->SetFacingToObject(jaina);
                        events.ScheduleEvent(5, 3s);
                        break;

                    case 5:
                        varian->AI()->Talk(SAY_VARIAN_02);
                        events.ScheduleEvent(6, 1s);
                        break;

                    case 6:
                        jaina->AI()->Talk(SAY_JAINA_02);
                        events.ScheduleEvent(7, 7s);
                        break;

                    case 7:
                        varian->AI()->Talk(SAY_VARIAN_03);
                        events.ScheduleEvent(8, 1s);
                        break;

                    case 8:
                        jaina->AI()->Talk(SAY_JAINA_03);
                        events.ScheduleEvent(9, 3s);
                        break;

                    case 9:
                        varian->AI()->Talk(SAY_VARIAN_04);
                        events.ScheduleEvent(10, 3s);
                        break;

                    case 10:
                        jaina->AI()->Talk(SAY_JAINA_04);
                        events.ScheduleEvent(11, 6s);
                        break;

                    case 11:
                        varian->AI()->Talk(SAY_VARIAN_05);
                        events.ScheduleEvent(12, 3s);
                        break;

                    case 12:
                        jaina->AI()->Talk(SAY_JAINA_05);
                        events.ScheduleEvent(13, 3s);
                        break;

                    case 13:
                        varian->AI()->Talk(SAY_VARIAN_06);
                        events.ScheduleEvent(14, 9s);
                        break;

                    case 14:
                        jaina->AI()->Talk(SAY_JAINA_06);
                        events.ScheduleEvent(15, 1s);
                        break;

                    case 15:
                        varian->AI()->Talk(SAY_VARIAN_07);
                        events.ScheduleEvent(16, 3s);
                        break;

                    case 16:
                        jaina->AI()->Talk(SAY_JAINA_07);
                        events.ScheduleEvent(17, 7s);
                        break;

                    case 17:
                        varian->AI()->Talk(SAY_VARIAN_08);
                        anduin->SetFacingToObject(varian);
                        events.ScheduleEvent(18, 6s);
                        break;

                    case 18:
                        jaina->AI()->Talk(SAY_JAINA_08);
                        events.ScheduleEvent(19, 2s);
                        break;

                    case 19:
                        varian->GetMotionMaster()->MoveSmoothPath(0, varianPathReversed, VARIAN_PATH_SIZE, true);
                        jaina->GetMotionMaster()->MovePoint(0, -8464.64f, 333.68f, 120.88f, true, 3.01f);
                        events.ScheduleEvent(20, 2s);
                        break;

                    case 20:
                        jaina->CastSpell(jaina, SPELL_TELEPORT);
                        events.ScheduleEvent(21, 2s);
                        break;

                    case 21:
                        jaina->AddAura(SPELL_INVISIBILITY, jaina);
                        if (GameObject* portal = GetClosestGameObjectWithEntry(me, GOB_PORTAL_TO_DALARAN, 45.f))
                            portal->Delete();
                        varian->SetFacingTo(2.24f);
                        varian->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
                        anduin->SetFacingToObject(varian);
                        break;

                    default:
                        break;
                }
            }
        }

        private:
        EventMap events;
        Phases phase;
        std::vector<Player*> players;
        Creature* jaina;
        Creature* varian;
        Creature* anduin;
    };

    CreatureAI* GetAI(Creature * creature) const override
    {
        return new stormwind_final_eventAI(creature);
    }
};

class npc_varian_wrynn : public CreatureScript
{
    public:
    npc_varian_wrynn() : CreatureScript("npc_varian_wrynn")
    {
    }

    struct npc_varian_wrynnAI : public ScriptedAI
    {
        npc_varian_wrynnAI(Creature* creature) : ScriptedAI(creature)
        {
        }

        void OnQuestReward(Player* player, Quest const* quest, uint32 /*opt*/) override
        {
            switch (quest->GetQuestId())
            {
                case QUEST_WHAT_HAD_TO_BE_DONE:
                    player->SetPhaseMask(PHASEMASK_NORMAL, true);
                    break;
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_varian_wrynnAI(creature);
    }
};

void AddSC_stormwind_final_event()
{
    new stormwind_final_event();
    new npc_varian_wrynn();
}
