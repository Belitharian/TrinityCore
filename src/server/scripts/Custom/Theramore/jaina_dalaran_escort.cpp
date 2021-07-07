#include "CreatureAIImpl.h"
#include "GameObject.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedEscortAI.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"
#include "Weather.h"
#include "../AI/CustomAI.h"

#include <iostream>

#define JAINA_PATH_01   4
#define JAINA_PATH_02   18
#define JAINA_PATH_03   51

#define JAINA_BOOK_PATH_01   12

enum Misc
{
    // Groups
    GROUP_ESCORT,
    GROUP_COMBAT,
    GROUP_RETREAT,

    // NPCs
    NPC_INVISIBLE_STALKER           = 32780,
    NPC_JAINA_PROUDMOORE            = 100095,
    NPC_ARCANIC_PROTECTOR           = 100096,

    // Gameobjets
    GOB_SNOWDRIFT                   = 194173,
    GOB_SECRET_ENTRANCE_DOOR        = 500001,

    // Phases
    PHASE_NORMAL                    = 5,
    PHASE_ESCORT                    = 9,
};

enum Actions
{
    ACTION_INTRO,
    ACTION_START_ESCORT,
    ACTION_WARNING,
    ACTION_CONTINUE
};

enum Talks
{
    TALK_JAINA_1,
    TALK_JAINA_2,
    TALK_JAINA_3,
    TALK_JAINA_4,

    TALK_JAINA_BOOK_1,
    TALK_JAINA_BOOK_2,
    TALK_JAINA_BOOK_3,
    TALK_JAINA_BOOK_4,
    TALK_JAINA_BOOK_5,
    TALK_JAINA_BOOK_6,
    TALK_JAINA_BOOK_7,

    TALK_JAINA_SECRET_1,

    TALK_JAINA_RETREAT              = 20,
};

enum Spells
{
    SPELL_FROST_CANALISATION        = 45846,
    SPELL_FROST_EXPLOSION           = 73775,
    SPELL_BLINK                     = 100119,
    SPELL_FROST_BOLT                = 100121,
    SPELL_ARCANE_EXPLOSION          = 100122,
    SPELL_FORCED_TELEPORTATION      = 100142,
    SPELL_INVISIBILITY              = 100147,
};

const Position ProtectorPos01 = { 5829.84f, 619.63f, 648.17f, 2.46f };
const Position ProtectorPos02 = { 5813.28f, 632.51f, 647.40f, 1.72f };
const Position JainaBookPos01 = { 5858.63f, 708.00f, 643.27f, 2.44f };
const Position JainaBookPos02 = { 5868.28f, 704.76f, 643.27f, 5.63f };
const Position JainaBookPos03 = { 5862.49f, 713.34f, 645.70f, 4.70f };
const Position JainaSecretPos01 = { 5694.87f, 613.50f, 613.19f, 3.87f };
const Position SnowdriftPos01 = { 5691.05f, 611.67f, 612.21f, 4.00f };
const QuaternionData SnowdriftRot01 = { 0, 0, -0.90f, 0.41f };

const Position JainaPath01[JAINA_PATH_01] =
{
    { 5808.72f, 606.37f, 655.42f, 2.68f },
    { 5805.82f, 606.31f, 655.42f, 1.65f },
    { 5804.90f, 615.03f, 651.17f, 1.63f },
    { 5804.32f, 625.04f, 647.75f, 0.77f }
};

const Position JainaPath02[JAINA_PATH_02] =
{
    { 5804.32f, 625.04f, 647.75f, 0.77f },
    { 5807.63f, 627.63f, 647.29f, 0.68f },
    { 5811.86f, 631.99f, 647.41f, 1.02f },
    { 5813.06f, 636.50f, 647.44f, 1.62f },
    { 5812.73f, 639.52f, 647.48f, 1.73f },
    { 5811.36f, 646.53f, 647.42f, 1.44f },
    { 5813.00f, 650.74f, 647.40f, 1.00f },
    { 5814.77f, 652.97f, 647.39f, 0.80f },
    { 5820.46f, 659.14f, 647.27f, 0.89f },
    { 5830.88f, 672.32f, 644.65f, 0.89f },
    { 5833.49f, 675.61f, 643.73f, 0.89f },
    { 5837.56f, 680.33f, 643.37f, 0.73f },
    { 5841.61f, 683.33f, 643.22f, 0.52f },
    { 5845.28f, 685.38f, 643.12f, 0.51f },
    { 5849.24f, 688.48f, 642.84f, 0.84f },
    { 5854.54f, 695.00f, 641.93f, 0.88f },
    { 5857.20f, 698.25f, 643.00f, 0.87f },
    { 5861.75f, 703.95f, 643.27f, 0.91f }
};

const Position JainaPath03[JAINA_PATH_03] =
{
    { 5861.75f, 703.95f, 643.27f, 4.07f },
    { 5859.04f, 700.74f, 643.29f, 4.00f },
    { 5853.60f, 694.34f, 641.61f, 4.00f },
    { 5847.97f, 689.78f, 642.76f, 4.17f },
    { 5844.27f, 685.56f, 643.09f, 3.45f },
    { 5840.31f, 686.16f, 643.01f, 2.47f },
    { 5837.29f, 689.95f, 642.70f, 2.14f },
    { 5832.80f, 697.05f, 642.14f, 2.18f },
    { 5827.34f, 703.42f, 641.67f, 2.34f },
    { 5822.35f, 708.87f, 641.22f, 2.13f },
    { 5820.53f, 712.57f, 641.22f, 1.92f },
    { 5818.90f, 719.15f, 641.25f, 1.73f },
    { 5817.64f, 731.68f, 640.91f, 1.65f },
    { 5816.65f, 748.45f, 640.64f, 1.62f },
    { 5816.22f, 756.84f, 640.34f, 1.62f },
    { 5815.86f, 761.61f, 640.62f, 1.68f },
    { 5814.47f, 769.89f, 639.24f, 1.77f },
    { 5812.99f, 778.16f, 635.38f, 1.71f },
    { 5812.56f, 786.40f, 632.58f, 1.33f },
    { 5814.82f, 789.61f, 632.58f, 0.54f },
    { 5817.28f, 789.66f, 632.58f, 5.48f },
    { 5818.79f, 786.06f, 632.58f, 4.79f },
    { 5820.17f, 778.37f, 628.64f, 4.97f },
    { 5822.93f, 766.09f, 625.31f, 4.85f },
    { 5824.14f, 757.78f, 624.78f, 4.83f },
    { 5824.31f, 750.87f, 623.85f, 4.62f },
    { 5821.53f, 739.76f, 623.31f, 4.36f },
    { 5818.43f, 731.96f, 623.74f, 4.24f },
    { 5814.04f, 724.80f, 624.15f, 4.13f },
    { 5811.70f, 721.32f, 623.25f, 4.10f },
    { 5806.80f, 713.83f, 619.03f, 4.30f },
    { 5805.43f, 709.87f, 619.03f, 4.39f },
    { 5804.02f, 706.41f, 618.59f, 4.24f },
    { 5799.56f, 699.30f, 618.59f, 4.07f },
    { 5793.66f, 693.19f, 618.59f, 3.59f },
    { 5789.87f, 692.51f, 619.47f, 3.02f },
    { 5784.98f, 694.64f, 619.56f, 2.48f },
    { 5780.98f, 698.82f, 618.86f, 2.28f },
    { 5775.42f, 704.31f, 618.79f, 2.55f },
    { 5772.62f, 705.51f, 618.76f, 3.02f },
    { 5769.25f, 704.99f, 618.64f, 3.60f },
    { 5765.87f, 702.52f, 618.58f, 3.92f },
    { 5760.39f, 696.16f, 618.57f, 4.00f },
    { 5753.88f, 688.12f, 618.57f, 4.16f },
    { 5750.84f, 682.54f, 618.61f, 2.81f },
    { 5741.80f, 689.50f, 613.24f, 2.47f },
    { 5739.27f, 686.07f, 613.24f, 4.24f },
    { 5736.62f, 679.89f, 613.24f, 4.22f },
    { 5732.90f, 674.93f, 613.24f, 3.91f },
    { 5729.83f, 672.06f, 613.24f, 3.88f },
    { 5725.54f, 667.29f, 613.24f, 4.33f }
};

const Position JainaBookPath1[JAINA_BOOK_PATH_01] =
{
    { 5863.42f, 713.12f, 645.70f, 5.83f },
    { 5864.70f, 712.52f, 645.24f, 5.73f },
    { 5865.86f, 711.60f, 643.57f, 5.26f },
    { 5866.09f, 711.08f, 643.27f, 5.04f },
    { 5866.24f, 710.60f, 643.27f, 4.94f },
    { 5866.51f, 708.92f, 643.27f, 4.02f },
    { 5868.77f, 706.85f, 643.27f, 4.51f },
    { 5868.25f, 704.69f, 643.27f, 4.35f },
    { 5867.06f, 703.08f, 643.27f, 3.63f },
    { 5865.41f, 702.68f, 643.27f, 3.19f },
    { 5863.38f, 702.78f, 643.30f, 3.06f },
    { 5861.82f, 703.97f, 643.27f, 4.04f }
};

class dalaran_jaina_escort : public CreatureScript
{
    public:
    dalaran_jaina_escort() : CreatureScript("dalaran_jaina_escort")
    {
    }

    struct dalaran_jaina_escortAI : public ScriptedAI
    {
        dalaran_jaina_escortAI(Creature* creature) : ScriptedAI(creature)
        {
            Initialize();
        }

        void Initialize()
        {
            me->m_CombatDistance = 4.f;
            me->m_SightDistance = 4.f;

            scheduler.SetValidator([this]
            {
                return !me->HasUnitState(UNIT_STATE_CASTING);
            });
        }

        void Reset() override
        {
            scheduler.CancelGroup(GROUP_COMBAT);
            scheduler.CancelGroup(GROUP_RETREAT);
        }

        void JustDied(Unit* killer) override
        {
            scheduler.CancelGroup(GROUP_COMBAT);
            scheduler.CancelGroup(GROUP_RETREAT);
        }

        void JustEngagedWith(Unit* who) override
        {
            scheduler
                .Schedule(5ms, GROUP_COMBAT, [this](TaskContext fireball)
                {
                    DoCastVictim(SPELL_FROST_BOLT);
                    fireball.Repeat(8s, 15s);
                })
                .Schedule(15s, GROUP_RETREAT, [this, who](TaskContext retreat)
                {
                    switch (retreat.GetRepeatCounter())
                    {
                        case 0:
                            scheduler.CancelGroup(GROUP_COMBAT);
                            if (Player* player = GetPlayerForEscort())
                                Talk(TALK_JAINA_RETREAT, player);
                            retreat.Repeat(2s);
                            break;
                        case 1:
                            me->CombatStop();
                            me->SetReactState(REACT_PASSIVE);
                            if (Player* player = GetPlayerForEscort())
                                player->CombatStop();
                            retreat.Repeat(500ms);
                            break;
                        case 2:
                            if (Player* player = GetPlayerForEscort())
                                DoCast(player, SPELL_INVISIBILITY);
                            break;
                    }


                });
        }

        void MovementInform(uint32 type, uint32 id) override
        {
            me->GetMotionMaster()->Clear();

            switch (id)
            {
                case 1:
                    scheduler.Schedule(100ms, [this](TaskContext context)
                    {
                        switch (context.GetRepeatCounter())
                        {
                            case 0:
                                me->SetFacingTo(0.62f);
                                context.Repeat(1s);
                                break;
                            case 1:
                                Talk(TALK_JAINA_3);
                                context.Repeat(3s);
                                break;
                            case 2:
                                if (Creature* protector = ObjectAccessor::GetCreature(*me, protectorGUID))
                                    protector->GetMotionMaster()->MovePoint(0, ProtectorPos01, true, ProtectorPos01.GetOrientation());
                                context.Repeat(3s);
                                break;
                            case 3:
                                Talk(TALK_JAINA_4);
                                me->GetMotionMaster()->MoveSmoothPath(2, JainaPath02, JAINA_PATH_02, false);
                                me->SetHomePosition(JainaPath02[JAINA_PATH_02 - 1]);
                                break;
                        }
                    });
                    break;
                case 2:
                    me->GetMotionMaster()->MoveSmoothPath(3, JainaPath03, JAINA_PATH_03, false);
                    me->SetHomePosition(JainaPath03[JAINA_PATH_03 - 1]);
                    /*
                    Talk(TALK_JAINA_BOOK_1);
                    me->SetWalk(true);
                    scheduler.Schedule(3s, [this](TaskContext context)
                    {
                        switch (context.GetRepeatCounter())
                        {
                            case 0:
                                me->GetMotionMaster()->MovePoint(0, JainaBookPos01, true, JainaBookPos01.GetOrientation());
                                context.Repeat(4s);
                                break;
                            case 1:
                                Talk(TALK_JAINA_BOOK_2);
                                context.Repeat(3s);
                                break;
                            case 2:
                                me->CastSpell(JainaBookPos02, SPELL_BLINK);
                                context.Repeat(3s);
                                break;
                            case 3:
                                Talk(TALK_JAINA_BOOK_3);
                                context.Repeat(3s);
                                break;
                            case 4:
                                me->CastSpell(JainaBookPos03, SPELL_BLINK);
                                context.Repeat(3s);
                                break;
                            case 5:
                                Talk(TALK_JAINA_BOOK_4);
                                context.Repeat(3s);
                                break;
                            case 6:
                                me->GetMotionMaster()->MoveSmoothPath(0, JainaBookPath1, JAINA_BOOK_PATH_01, true, 4.02f);
                                me->SetHomePosition(JainaBookPath1[JAINA_BOOK_PATH_01 - 1]);
                                context.Repeat(6s);
                                break;
                            case 7:
                                Talk(TALK_JAINA_BOOK_5);
                                context.Repeat(3s);
                                break;
                            case 8:
                                Talk(TALK_JAINA_BOOK_6);
                                context.Repeat(9s);
                                break;
                            case 9:
                                Talk(TALK_JAINA_BOOK_7);
                                context.Repeat(3s);
                                break;
                            case 10:
                                me->SetWalk(false);
                                me->GetMotionMaster()->MoveSmoothPath(3, JainaPath03, JAINA_PATH_03, false);
                                me->SetHomePosition(JainaPath03[JAINA_PATH_03 - 1]);
                                break;
                        }
                    });
                    */
                    break;
                case 3:
                    Talk(TALK_JAINA_SECRET_1);
                    scheduler.Schedule(1s, [this](TaskContext context)
                    {
                        switch (context.GetRepeatCounter())
                        {
                            case 0:
                                me->SummonGameObject(GOB_SNOWDRIFT, SnowdriftPos01, SnowdriftRot01, 0s);
                                if (Creature* fx = me->SummonCreature(NPC_INVISIBLE_STALKER, SnowdriftPos01, TEMPSUMMON_TIMED_DESPAWN, 5s))
                                    fx->CastSpell(fx, SPELL_FROST_EXPLOSION);
                                context.Repeat(3s);
                                break;
                            case 1:
                                me->CastSpell(JainaSecretPos01, SPELL_BLINK, true);
                                me->RemoveAurasDueToSpell(SPELL_INVISIBILITY);
                                me->NearTeleportTo(JainaSecretPos01);
                                me->SetHomePosition(JainaSecretPos01);
                                context.Repeat(2s);
                                break;
                            case 2:
                                if (Player* player = ObjectAccessor::GetPlayer(*me, playerGUID))
                                {
                                    player->RemoveAurasDueToSpell(SPELL_INVISIBILITY);
                                    DoCast(player, SPELL_FORCED_TELEPORTATION);
                                }
                                context.Repeat(2s);
                                break;
                            case 3:
                                if (GameObject* door = me->FindNearestGameObject(GOB_SECRET_ENTRANCE_DOOR, 30.f))
                                    door->UseDoorOrButton();
                                DoCastAOE(SPELL_ARCANE_EXPLOSION);
                                break;
                        }
                    });
                    break;
                default:
                    break;
            }
        }

        void UpdateAI(uint32 diff) override
        {
            scheduler.Update(diff, [this]
            {
                if (UpdateVictim())
                    DoMeleeAttackIfReady();
            });
        }

        bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
        {
            ClearGossipMenuFor(player);
            CloseGossipMenuFor(player);

            playerGUID = player->GetGUID();

            switch (gossipListId)
            {
                case 0:
                    me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
                    DoAction(ACTION_INTRO);
                    break;
                default:
                    break;
            }

            return true;
        }

        void DoAction(int32 actionId) override
        {
            switch (actionId)
            {
                case ACTION_INTRO:
                    scheduler.Schedule(1s, [this](TaskContext context)
                    {
                        switch (context.GetRepeatCounter())
                        {
                            case 0:
                                Talk(TALK_JAINA_1);
                                context.Repeat(3s);
                                break;
                            case 1:
                                if (Player* player = GetPlayerForEscort())
                                {
                                    player->SetPhaseMask(PHASE_ESCORT, true);
                                    me->SetPhaseMask(PHASE_ESCORT, true);

                                    DoCast(player, SPELL_INVISIBILITY);
                                }
                                context.Repeat(2s);
                                break;
                            case 2:
                                Talk(TALK_JAINA_2);
                                context.Repeat(2s);
                                break;
                            case 3:
                                me->GetMotionMaster()->MoveSmoothPath(1, JainaPath01, JAINA_PATH_01, false);
                                me->SetHomePosition(JainaPath01[JAINA_PATH_01 - 1]);
                                if (Creature* protector = me->SummonCreature(NPC_ARCANIC_PROTECTOR, ProtectorPos01))
                                {
                                    protectorGUID = protector->GetGUID();
                                    protector->SetWalk(true);
                                    protector->SetHomePosition(ProtectorPos02);
                                    protector->GetMotionMaster()->MovePoint(0, ProtectorPos02, true, ProtectorPos02.GetOrientation());
                                }
                                break;
                        }
                    });
                    break;
                default:
                    break;
            }
        }

        private:
        TaskScheduler scheduler;
        ObjectGuid protectorGUID;
        ObjectGuid playerGUID;

        Player* GetPlayerForEscort()
        {
            return ObjectAccessor::GetPlayer(*me, playerGUID);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new dalaran_jaina_escortAI(creature);
    }
};

class npc_arcanic_protector : public CreatureScript
{
    public:
    npc_arcanic_protector() : CreatureScript("npc_arcanic_protector")
    {
    }

    struct npc_arcanic_protectorAI : public CustomAI
    {
        npc_arcanic_protectorAI(Creature* creature) : CustomAI(creature, AI_Type::Melee)
        {
            me->m_CombatDistance = 5.f;
            me->m_SightDistance = 5.f;
        }

        void JustEngagedWith(Unit* who) override
        {
            if (Player* player = who->ToPlayer())
            {
                player->RemoveAurasDueToSpell(SPELL_INVISIBILITY);
                if (Creature* jaina = GetClosestCreatureWithEntry(me, NPC_JAINA_PROUDMOORE, 3000.f))
                {
                    jaina->RemoveAurasDueToSpell(SPELL_INVISIBILITY);
                    jaina->EngageWithTarget(me);
                }
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_arcanic_protectorAI(creature);
    }
};

void AddSC_dalaran_jaina_escort()
{
    new dalaran_jaina_escort();
    new npc_arcanic_protector();
}
