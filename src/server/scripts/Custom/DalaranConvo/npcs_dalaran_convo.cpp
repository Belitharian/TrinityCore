#include "AreaTriggerAI.h"
#include "Conversation.h"
#include "ConversationAI.h"
#include "Object.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "CustomAI.h"
#include "dalaran_convo.h"

struct npc_jaina_proudmoore_convo : public CustomAI
{
	enum Misc
	{
		// Spells
		SPELL_TELEPORT              = 134634,

		// Gossips
		GOSSIP_MENU_INTROCUTION     = 65006,
	};

	npc_jaina_proudmoore_convo(Creature* creature) : CustomAI(creature),
		instance(creature->GetInstanceScript()) { }

	bool OnGossipHello(Player* player) override
	{
		player->PrepareGossipMenu(me, GOSSIP_MENU_INTROCUTION, true);
		player->SendPreparedGossip(me);
		return true;
	}

	bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
	{
		ClearGossipMenuFor(player);

		switch (gossipListId)
		{
			case 0:
                me->RemoveNpcFlag(NPCFlags(UNIT_NPC_FLAG_GOSSIP));
                instance->TriggerGameEvent(EVENT_FIND_INTRODUCTION_DISCUSS);
				break;
		}

		CloseGossipMenuFor(player);
		return true;
	}

	void WaypointPathEnded(uint32 /*pointId*/, uint32 pathId) override
	{
		switch (pathId)
		{
			case 1:
				ProcTeleportVisual(me);
				me->NearTeleportTo(JainaPos01);
				me->SetHomePosition(JainaPos01);
				break;
            case 2:
                ProcTeleportVisual(me);
                me->NearTeleportTo(JainaPos02);
                me->SetHomePosition(JainaPos02);
                break;
            case 3:
                me->SetFacingTo(4.5525f);
                break;
		}
	}

	private:
	InstanceScript* instance;
};

struct npc_anduin_wrynn_convo : public CustomAI
{
	npc_anduin_wrynn_convo(Creature* creature) : CustomAI(creature) { }

    void WaypointReached(uint32 waypointId, uint32 pathId) override
    {
        if (pathId == 2 && waypointId == 1)
            me->PauseMovement(1 * IN_MILLISECONDS);
    }

	void WaypointPathEnded(uint32 /*pointId*/, uint32 pathId) override
	{
		switch (pathId)
		{
			case 1:
				ProcTeleportVisual(me);
				me->NearTeleportTo(AnduinPos01);
				me->SetHomePosition(AnduinPos01);
				break;
            case 2:
                ProcTeleportVisual(me);
                me->NearTeleportTo(AnduinPos02);
                me->SetHomePosition(AnduinPos02);
                break;
            case 3:
                me->SetFacingTo(4.5525f);
                break;
		}
	}
};

struct npc_arcanist_alec_convo : public CustomAI
{
	npc_arcanist_alec_convo(Creature* creature) : CustomAI(creature), instance(creature->GetInstanceScript()) { }

	enum Misc
	{
		// Spells
		SPELL_TELEPORT              = 134634,

		// Gossips
		GOSSIP_MENU_INTROCUTION     = 65008,
	};

	bool OnGossipHello(Player* player) override
	{
		player->PrepareGossipMenu(me, GOSSIP_MENU_INTROCUTION, true);
		player->SendPreparedGossip(me);
		return true;
	}

	bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
	{
		ClearGossipMenuFor(player);

		switch (gossipListId)
		{
			case 0:
				instance->TriggerGameEvent(EVENT_FIND_KALECGOS_SPEAK_TO_ALEC);
				break;
		}

		CloseGossipMenuFor(player);
		return true;
	}

	private:
	InstanceScript* instance;
};

struct npc_kelthuzad_vision : public CustomAI
{
    npc_kelthuzad_vision(Creature* creature) : CustomAI(creature), instance(creature->GetInstanceScript()) {}

    enum Texts
    {
        KELTHUZAD_SAYS_01 = 0, // Encore une interruption !
        KELTHUZAD_SAYS_02 = 1, // Je refuse...
        KELTHUZAD_SAYS_03 = 2, // Ne voyez vous pas...
        KELTHUZAD_SAYS_04 = 3, // Vous ne comprendriez pas...
    };

    void DoAction(int32 param) override
    {
        if (param != ACTION_KELTHUZAD_COMBAT_READY)
            return;

        me->NearTeleportTo(KelThuzadPos01);
        me->SetHomePosition(KelThuzadPos01);
        me->AI()->Talk(KELTHUZAD_SAYS_01);
        me->RemoveUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
        me->SetFaction(FACTION_KELTHUZAD_HOSTILE);
        me->SetImmuneToAll(true);

        scheduler.Schedule(2s, [this](TaskContext context)
        {
            switch (context.GetRepeatCounter())
            {
                case 0:
                    me->AI()->Talk(KELTHUZAD_SAYS_02);
                    context.Repeat(2s);
                    break;
                case 1:
                    me->SetImmuneToAll(false);
                    context.CancelAll();
                    return;
            }
        });
    }

    void JustDied(Unit* /*killer*/) override
    {
        instance->TriggerGameEvent(EVENT_FIND_KELTHUZAD_DEFEATED);
        instance->SetData(DATA_PHASE, (uint32)Phases::KelThuzad_CanTeleport);
    }

private:
    InstanceScript* instance;
};

// 60000 - Kirin Tor Fate
class conversation_dalaran_introduction : public ConversationAI
{
public:
	conversation_dalaran_introduction(Conversation* conversation) : ConversationAI(conversation) { }

	enum TheKirinTorFate
	{
		// Lines
		CONVERSATION_LINE_INTRODUCTION_01       = 2,    // Anduin, I know exactly what he's asking. Oh - look who's arrived.

		// Actors
		CONVERSATION_ACTOR_IDX_ANDUIN_WRYNN     = 0,
		CONVERSATION_ACTOR_IDX_JAINA_POUDMOORE  = 1,
	};

	void OnCreate(Unit* creator) override
	{
		Creature* anduin = creator->FindNearestCreatureWithOptions(50.0f, { .CreatureId = NPC_ANDUIN_WRYNN, .IgnorePhases = true });
		Creature* jaina = creator->FindNearestCreatureWithOptions(50.0f, { .CreatureId = NPC_JAINA_PROUDMOORE, .IgnorePhases = true });
		if (!anduin || !jaina)
			return;

		jaina->RemoveNpcFlag(NPCFlags(UNIT_NPC_FLAG_GOSSIP));

		conversation->AddActor(CONVERSATION_INTRODUCTION, CONVERSATION_ACTOR_IDX_ANDUIN_WRYNN, anduin->GetGUID());
		conversation->AddActor(CONVERSATION_INTRODUCTION, CONVERSATION_ACTOR_IDX_JAINA_POUDMOORE, jaina->GetGUID());
		conversation->Start();
	}

	void OnStart() override
	{
		LocaleConstant privateOwnerLocale = conversation->GetPrivateObjectOwnerLocale();

		conversation->m_Events.AddEvent([conversation = conversation]()
		{
			Creature* anduin = conversation->GetActorCreature(CONVERSATION_ACTOR_IDX_ANDUIN_WRYNN);
			Creature* jaina = conversation->GetActorCreature(CONVERSATION_ACTOR_IDX_JAINA_POUDMOORE);

			if (!anduin || !jaina)
				return;

			anduin->SetFacingToObject(jaina);
			jaina->SetFacingToObject(anduin);

		}, conversation->GetLineEndTime(privateOwnerLocale, CONVERSATION_LINE_INTRODUCTION_01));

		conversation->m_Events.AddEvent([conversation = conversation]()
		{
			Player* player = ObjectAccessor::GetPlayer(*conversation, conversation->GetPrivateObjectOwner());
			if (!player)
				return;

			if (Creature* anduin = conversation->GetActorCreature(CONVERSATION_ACTOR_IDX_ANDUIN_WRYNN))
				anduin->SetFacingToObject(player);

			if (Creature* jaina = conversation->GetActorCreature(CONVERSATION_ACTOR_IDX_JAINA_POUDMOORE))
			{
				jaina->SetFacingToObject(player);
				jaina->SetNpcFlag(NPCFlags(UNIT_NPC_FLAG_GOSSIP));
			}

		}, conversation->GetLastLineEndTime(privateOwnerLocale));
	}
};

// 60001 - Kirin Tor Fate Start
class conversation_dalaran_start : public ConversationAI
{
public:
	conversation_dalaran_start(Conversation* conversation) : ConversationAI(conversation), instance(nullptr) { }

	enum TheKirinTorFate
	{
		// Lines
		CONVERSATION_LINE_START_01              = 4,    // Nobody dislikes Garrosh more than me. I wrestle with my anger every day.
		CONVERSATION_LINE_START_02              = 5,    // Come with me - look around you a moment.

		// Actors
        CONVERSATION_ACTOR_IDX_JAINA_POUDMOORE  = 0,
	};

	void OnCreate(Unit* creator) override
	{
        instance = creator->GetInstanceScript();

        Creature* jaina = creator->FindNearestCreatureWithOptions(50.0f, { .CreatureId = NPC_JAINA_PROUDMOORE, .IgnorePhases = true });
        if (!jaina)
            return;

		conversation->AddActor(CONVERSATION_START, CONVERSATION_ACTOR_IDX_JAINA_POUDMOORE, jaina->GetGUID());
		conversation->Start();
	}

	void OnStart() override
	{
		LocaleConstant privateOwnerLocale = conversation->GetPrivateObjectOwnerLocale();

		conversation->m_Events.AddEvent([conversation = conversation]()
		{
			Creature* jaina = conversation->GetActorCreature(CONVERSATION_ACTOR_IDX_JAINA_POUDMOORE);
			if (!jaina)
				return;

			jaina->GetMotionMaster()->MovePath(ActorsPath01, false);

		}, conversation->GetLineEndTime(privateOwnerLocale, CONVERSATION_LINE_START_01) + 2s);

		conversation->m_Events.AddEvent([conversationAI = this, conversation = conversation]()
		{
            Creature* anduin = conversationAI->instance->GetCreature(DATA_ANDUIN);
            if (!anduin)
                return;

			anduin->GetMotionMaster()->MovePath(ActorsPath01, false);

		}, conversation->GetLineEndTime(privateOwnerLocale, CONVERSATION_LINE_START_02) + 2s);

        conversation->m_Events.AddEvent([conversationAI = this, conversation = conversation]()
		{
            conversationAI->instance->SetData(DATA_PHASE, (uint32)Phases::Start_CanTeleport);

		}, conversation->GetLastLineEndTime(privateOwnerLocale) + 2s);
	}

private:
    InstanceScript* instance;
};

// 60002 - Kirin Tor Fate Kalecgos
class conversation_dalaran_kalecgos : public ConversationAI
{
public:
    conversation_dalaran_kalecgos(Conversation* conversation) : ConversationAI(conversation), instance(nullptr) { }

	enum TheKirinTorFate
	{
		// Lines
		CONVERSATION_LINE_KALECGOS_01           = 6,    // In the aftermath of Theramore, my first instinct was to decimate Orgrimmar [..]
		CONVERSATION_LINE_KALECGOS_02           = 7,    // ...Jaina !
		CONVERSATION_LINE_KALECGOS_03           = 8,    // I'm not proud. Since then, Kalecgos and I have talked at length [...]
        CONVERSATION_LINE_KALECGOS_04           = 9,

		// Actors
        CONVERSATION_ACTOR_IDX_JAINA_POUDMOORE  = 0,
        CONVERSATION_ACTOR_IDX_ANDUIN_WRYNN     = 1,
        CONVERSATION_ACTOR_IDX_ARCANIST_ALEC    = 2,

        // Spells
        SPELL_SPLOTLIGHT                        = 437208,
	};

	void OnCreate(Unit* creator) override
	{
        instance = creator->GetInstanceScript();

        Creature* jaina = instance->GetCreature(DATA_JAINA_PROUDMOORE);
        Creature* anduin = instance->GetCreature(DATA_ANDUIN);
        Creature* alec = instance->GetCreature(DATA_ARCANIST_ALEC);
        if (!jaina || !anduin || !alec)
            return;

        Creature* jainaEvent = instance->GetCreature(DATA_JAINA_PROUDMOORE_VISION);
        Creature* kalecEvent = instance->GetCreature(DATA_KALECGOS);
        if (!jainaEvent || !kalecEvent)
            return;

        SetTarget(jainaEvent, kalecEvent);
        SetTarget(kalecEvent, jainaEvent);

        conversation->AddActor(CONVERSATION_KALECGOS, CONVERSATION_ACTOR_IDX_JAINA_POUDMOORE, jaina->GetGUID());
        conversation->AddActor(CONVERSATION_KALECGOS, CONVERSATION_ACTOR_IDX_ANDUIN_WRYNN, anduin->GetGUID());
        conversation->AddActor(CONVERSATION_KALECGOS, CONVERSATION_ACTOR_IDX_ARCANIST_ALEC, alec->GetGUID());
        conversation->Start();
	}

	void OnStart() override
	{
		LocaleConstant privateOwnerLocale = conversation->GetPrivateObjectOwnerLocale();

		conversation->m_Events.AddEvent([conversationAI = this, conversation = conversation]()
		{
			Creature* jaina = conversation->GetActorCreature(CONVERSATION_ACTOR_IDX_JAINA_POUDMOORE);
			Creature* anduin = conversation->GetActorCreature(CONVERSATION_ACTOR_IDX_ANDUIN_WRYNN);
			if (!jaina || !anduin)
				return;

            conversationAI->SetTarget(anduin, jaina);

		}, conversation->GetLineEndTime(privateOwnerLocale, CONVERSATION_LINE_KALECGOS_02));

        conversation->m_Events.AddEvent([conversationAI = this, conversation = conversation]()
		{
			Creature* jaina = conversation->GetActorCreature(CONVERSATION_ACTOR_IDX_JAINA_POUDMOORE);
			Creature* anduin = conversation->GetActorCreature(CONVERSATION_ACTOR_IDX_ANDUIN_WRYNN);
			if (!jaina || !anduin)
				return;

            conversationAI->SetTarget(jaina, anduin);

		}, conversation->GetLineEndTime(privateOwnerLocale, CONVERSATION_LINE_KALECGOS_02) + 1s);

        conversation->m_Events.AddEvent([conversationAI = this, conversation = conversation]()
		{
			Creature* jaina = conversation->GetActorCreature(CONVERSATION_ACTOR_IDX_JAINA_POUDMOORE);
			Creature* anduin = conversation->GetActorCreature(CONVERSATION_ACTOR_IDX_ANDUIN_WRYNN);
			if (!jaina || !anduin)
				return;

            conversationAI->ClearTarget(anduin);
            conversationAI->ClearTarget(jaina);

            jaina->GetMotionMaster()->MovePath(JainaPath01, false);

		}, conversation->GetLineEndTime(privateOwnerLocale, CONVERSATION_LINE_KALECGOS_03) + 3s);

        conversation->m_Events.AddEvent([conversation = conversation]()
		{
            Creature* anduin = conversation->GetActorCreature(CONVERSATION_ACTOR_IDX_ANDUIN_WRYNN);
			if (!anduin)
				return;

            anduin->GetMotionMaster()->MovePath(AnduinPath01, false);

		}, conversation->GetLineEndTime(privateOwnerLocale, CONVERSATION_LINE_KALECGOS_03) + 5s);

        conversation->m_Events.AddEvent([conversationAI = this, conversation = conversation]()
		{
            Creature* alec = conversation->GetActorCreature(CONVERSATION_ACTOR_IDX_ARCANIST_ALEC);
			if (!alec)
				return;

            alec->SetNpcFlag(NPCFlags(UNIT_NPC_FLAG_GOSSIP));
            alec->CastSpell(alec, SPELL_SPLOTLIGHT, true);

            conversationAI->instance->TriggerGameEvent(EVENT_FIND_KALECGOS_ASSIST_JAINA);

		}, conversation->GetLastLineEndTime(privateOwnerLocale) + 2s);
	}

    void SetTarget(Creature* creature, Creature* target)
    {
        creature->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
        creature->SetTarget(target->GetGUID());
    }

    void ClearTarget(Creature* creature)
    {
        creature->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
        creature->SetTarget(ObjectGuid::Empty);
    }

private:
    InstanceScript* instance;
};

// 60003 - Kirin Tor Fate Kel'Thuzad
class conversation_dalaran_kelthuzad : public ConversationAI
{
public:
    conversation_dalaran_kelthuzad(Conversation* conversation) : ConversationAI(conversation), instance(nullptr) { }

    enum TheKirinTorFate
    {
        // Actors
        CONVERSATION_ACTOR_IDX_JAINA_POUDMOORE  = 0,
    };

    void OnCreate(Unit* creator) override
    {
        instance = creator->GetInstanceScript();

        Creature* jaina = instance->GetCreature(DATA_JAINA_PROUDMOORE);
        if (!jaina)
            return;

        conversation->AddActor(CONVERSATION_KELTHUZAD, CONVERSATION_ACTOR_IDX_JAINA_POUDMOORE, jaina->GetGUID());
        conversation->Start();
    }

    void OnStart() override
    {
        LocaleConstant privateOwnerLocale = conversation->GetPrivateObjectOwnerLocale();

        conversation->m_Events.AddEvent([conversationAI = this, conversation = conversation]()
        {
            Creature* jaina = conversation->GetActorCreature(CONVERSATION_ACTOR_IDX_JAINA_POUDMOORE);
            Creature* anduin = conversationAI->instance->GetCreature(DATA_ANDUIN);
            if (!jaina || !anduin)
                return;

            jaina->GetMotionMaster()->MovePath(JainaPath02, false);
            anduin->GetMotionMaster()->MovePath(AnduinPath02, false);

            conversationAI->instance->SetData(DATA_PHASE, (uint32)Phases::KelThuzad_Combat_Ready);

        }, conversation->GetLastLineEndTime(privateOwnerLocale) + 2s);
    }

private:
    InstanceScript* instance;
};

struct areatrigger_dalaran : public AreaTriggerAI
{
    InstanceScript* instance;
    bool consumed;

    areatrigger_dalaran(AreaTrigger* at) : AreaTriggerAI(at), consumed(false)
    {
        instance = at->GetInstanceScript();
    }

    virtual Phases CheckPhase()
    {
        return Phases::None;
    }

    virtual Phases NextPhase()
    {
        return Phases::None;
    }

    virtual void Process(Player* player) { }

    void OnUnitEnter(Unit* unit) override
    {
        Player* player = unit->ToPlayer();
        if (!player || player->IsGameMaster())
            return;

        Phases phase = (Phases)instance->GetData(DATA_PHASE);
        if (phase != CheckPhase())
            return;

        Phases nextPhase = NextPhase();
        if (nextPhase != Phases::None)
            instance->SetData(DATA_PHASE, (uint32)nextPhase);

        Process(player);
        consumed = true;
    }

    void OnUnitExit(Unit* unit, AreaTriggerExitReason reason) override
    {
        if (reason != AreaTriggerExitReason::NotInside)
            return;

        Player* player = unit->ToPlayer();
        if (!player || player->IsGameMaster())
            return;

        if (!consumed)
            return;

        at->Remove();
    }
};

struct areatrigger_dalaran_introduction : public areatrigger_dalaran
{
    areatrigger_dalaran_introduction(AreaTrigger* at) : areatrigger_dalaran(at) { }

    void Process(Player* /*player*/) override
    {
        instance->TriggerGameEvent(EVENT_FIND_INTRODUCTION_FIND_JAINA);
    }
};

struct areatrigger_dalaran_kalecgos : public areatrigger_dalaran
{
    areatrigger_dalaran_kalecgos(AreaTrigger* at) : areatrigger_dalaran(at) { }

    Phases CheckPhase() override
    {
        return Phases::Start_CanTeleport;
    }

    Phases NextPhase() override
    {
        return Phases::Kalecgos;
    }
};

struct areatrigger_dalaran_teleport_legerdemain : public areatrigger_dalaran
{
    areatrigger_dalaran_teleport_legerdemain(AreaTrigger* at) : areatrigger_dalaran(at) { }

    Phases CheckPhase() override
    {
        return Phases::Start_CanTeleport;
    }

    void Process(Player* player) override
    {
        player->CastSpell(PlayerPos01, SPELL_TELEPORT);
    }
};

struct areatrigger_dalaran_kelthuzad : public areatrigger_dalaran
{
    areatrigger_dalaran_kelthuzad(AreaTrigger* at) : areatrigger_dalaran(at) { }

    Phases CheckPhase() override
    {
        return Phases::Kalecgos_CanTeleport;
    }

    Phases NextPhase() override
    {
        return Phases::KelThuzad;
    }

    void Process(Player* /*player*/) override
    {
        instance->TriggerGameEvent(EVENT_FIND_KELTHUZAD_WITNESS);
    }
};

struct areatrigger_dalaran_kelthuzad_combat : public areatrigger_dalaran
{
    areatrigger_dalaran_kelthuzad_combat(AreaTrigger* at) : areatrigger_dalaran(at) { }

    Phases CheckPhase() override
    {
        return Phases::KelThuzad_Combat_Ready;
    }

    Phases NextPhase() override
    {
        return Phases::KelThuzad_Combat;
    }
};

struct areatrigger_dalaran_teleport_kelthuzad : public areatrigger_dalaran
{
    areatrigger_dalaran_teleport_kelthuzad(AreaTrigger* at) : areatrigger_dalaran(at) { }

    Phases CheckPhase() override
    {
        return Phases::KelThuzad_CanTeleport;
    }

    void Process(Player* player) override
    {
        player->CastSpell(PlayerPos02, SPELL_TELEPORT);
    }
};

void AddSC_npcs_dalaran_convo()
{
	RegisterConvoAI(npc_jaina_proudmoore_convo);
	RegisterConvoAI(npc_anduin_wrynn_convo);
	RegisterConvoAI(npc_arcanist_alec_convo);
	RegisterConvoAI(npc_kelthuzad_vision);

	RegisterConversationAI(conversation_dalaran_introduction);
	RegisterConversationAI(conversation_dalaran_start);
	RegisterConversationAI(conversation_dalaran_kalecgos);
	RegisterConversationAI(conversation_dalaran_kelthuzad);

	RegisterAreaTriggerAI(areatrigger_dalaran_introduction);
	RegisterAreaTriggerAI(areatrigger_dalaran_kalecgos);
	RegisterAreaTriggerAI(areatrigger_dalaran_kelthuzad);
	RegisterAreaTriggerAI(areatrigger_dalaran_kelthuzad_combat);

    // Portals
	RegisterAreaTriggerAI(areatrigger_dalaran_teleport_legerdemain);
	RegisterAreaTriggerAI(areatrigger_dalaran_teleport_kelthuzad);
}
