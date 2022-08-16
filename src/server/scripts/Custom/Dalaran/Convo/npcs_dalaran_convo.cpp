#include "Custom/AI/CustomAI.h"
#include "AreaTriggerAI.h"
#include "MotionMaster.h"
#include "Object.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"
#include "dalaran_convo.h"

struct npc_jaina_proudmoore_convo : public CustomAI
{
    enum Misc
    {
        // Spells
        SPELL_TELEPORT          = 134634,

        // Groups
        GROUP_NONE              = 0,
        GROUP_TELEPORT          = 1,

        // Gossip
        GOSSIP_MENU_DEFAULT     = 65006,
    };

	npc_jaina_proudmoore_convo(Creature* creature) : CustomAI(creature), phase(Phases::None)
	{
        instance = me->GetInstanceScript();
    }
	
    void SetData(uint32 id, uint32 value)
    {
        switch (id)
        {
            // ========== DEBUG
            case 100:
                scheduler.CancelGroup(GROUP_TELEPORT);
                SetData(PHASE_TYPE, (uint32)Phases::Progress);
                me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                instance->SetData(PHASE_TYPE, (uint32)Phases::Conversation);
                break;
            // ========== DEBUG

            case PHASE_TYPE:
                phase = (Phases)value;
                break;
        }
    }

    bool OnGossipHello(Player* player) override
    {
        player->PrepareGossipMenu(me, GOSSIP_MENU_DEFAULT, true);
        player->SendPreparedGossip(me);
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        ClearGossipMenuFor(player);

        switch (gossipListId)
        {
            case 0:
                scheduler.CancelGroup(GROUP_TELEPORT);
                SetData(PHASE_TYPE, (uint32)Phases::Progress);
                me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                instance->SetData(PHASE_TYPE, (uint32)Phases::Conversation);
                break;
        }

        CloseGossipMenuFor(player);
        return true;
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (who->GetTypeId() != TYPEID_PLAYER)
            return;

        Player* player = who->ToPlayer();
        if (!player)
            return;

        switch (phase)
        {
            case Phases::Introduction:
                scheduler.Schedule(5ms, GROUP_TELEPORT, [this, player](TaskContext teleport)
                {
                    if (player->GetPositionZ() < 920.f)
                        me->CastSpell(player, SPELL_TELEPORT, true);
                    teleport.Repeat(850ms);
                });
                break;
            case Phases::None:
                if (player->IsWithinDist(me, 5.0f))
                {
                    SetData(PHASE_TYPE, (uint32)Phases::Progress);
                    instance->SetData(PHASE_TYPE, (uint32)Phases::Introduction);
                }
                break;
            default:
            case Phases::Progress:
                break;
        }
    }

	void MovementInform(uint32 /*type*/, uint32 id) override
	{
		if (id == MOVEMENT_INFO_POINT_01)
		{
            if (Creature* trigger = me->SummonCreature(NPC_INVISIBLE_STALKER, me->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, 10s))
                trigger->CastSpell(trigger, SPELL_TELEPORT_DUMMY);

            me->NearTeleportTo(JainaPos01);
            me->SetHomePosition(JainaPos01);
            me->AddAura(SPELL_SIT_CHAIR_MED, me);
		}
        else if (id == MOVEMENT_INFO_POINT_02)
        {
            me->SetFacingTo(0.15f);
            instance->SetData(PHASE_TYPE, (uint32)Phases::Event);
        }
	}

    private:
    Phases phase;
    InstanceScript* instance;
};

struct npc_anduin_wrynn_convo : public CustomAI
{
    npc_anduin_wrynn_convo(Creature* creature) : CustomAI(creature)
    {
    }

    void MovementInform(uint32 /*type*/, uint32 id) override
    {
        if (id == MOVEMENT_INFO_POINT_01)
        {
            if (Creature* trigger = me->SummonCreature(NPC_INVISIBLE_STALKER, me->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, 10s))
                trigger->CastSpell(trigger, SPELL_TELEPORT_DUMMY);

            me->NearTeleportTo(AnduinPos01);
            me->SetHomePosition(AnduinPos01);
        }
        else if (id == MOVEMENT_INFO_POINT_02)
        {
            me->SetFacingTo(1.01f);
        }
    }
};

struct areatrigger_dalaran_teleport_unit : public AreaTriggerAI
{
    areatrigger_dalaran_teleport_unit(AreaTrigger* at) : AreaTriggerAI(at)
    {
        instance = at->GetInstanceScript();
    }

    void OnUnitEnter(Unit* unit) override
    {
        if (unit->GetTypeId() != TYPEID_PLAYER)
            return;

        Phases phase = (Phases)instance->GetData(PHASE_TYPE);
        if (phase != Phases::Conversation)
            return;

        unit->CastSpell(unit, SPELL_TELEPORT_DUMMY);
        unit->NearTeleportTo(PlayerPos01);
    }

    private:
    InstanceScript* instance;
};

struct areatrigger_dalaran_start_events : public AreaTriggerAI
{
    areatrigger_dalaran_start_events(AreaTrigger* at) : AreaTriggerAI(at)
    {
        instance = at->GetInstanceScript();
    }

    void OnUnitEnter(Unit* unit) override
    {
        if (unit->GetTypeId() != TYPEID_PLAYER)
            return;

        Phases phase = (Phases)instance->GetData(PHASE_TYPE);
        if (phase == Phases::Event)
            return;

        instance->SetData(PHASE_TYPE, (uint32)Phases::Event);
    }

    private:
    InstanceScript* instance;
};

void AddSC_npcs_dalaran_convo()
{
	RegisterConvoAI(npc_jaina_proudmoore_convo);
	RegisterConvoAI(npc_anduin_wrynn_convo);

	RegisterAreaTriggerAI(areatrigger_dalaran_teleport_unit);
	RegisterAreaTriggerAI(areatrigger_dalaran_start_events);
}
