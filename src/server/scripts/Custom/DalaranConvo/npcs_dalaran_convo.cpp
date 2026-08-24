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
		// Gossips
		GOSSIP_MENU_INTROCUTION     = 65006,
	};

	npc_jaina_proudmoore_convo(Creature* creature) : CustomAI(creature),
		instance(creature->GetInstanceScript())
	{
		SetCanRandomMovement(false);
	}

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
				ProcTeleportVisual(me, JainaPos01);
				break;
		}
	}

	private:
	InstanceScript* instance;
};

struct npc_anduin_wrynn_convo : public CustomAI
{
	npc_anduin_wrynn_convo(Creature* creature) : CustomAI(creature),
		instance(creature->GetInstanceScript()) { }

	void WaypointPathEnded(uint32 /*pointId*/, uint32 pathId) override
	{
		switch (pathId)
		{
			case 1:
				ProcTeleportVisual(me, AnduinPos01);
				instance->SetData(DATA_PHASE, (uint32)Phases::Start_CanTeleport);
				break;
		}
	}

	private:
	InstanceScript* instance;
};

struct npc_kelthuzad_vision : public CustomAI
{
	npc_kelthuzad_vision(Creature* creature) : CustomAI(creature, AI_Type::Stay),
		instance(creature->GetInstanceScript()) {}

	enum Spells
	{
		SPELL_DARKSPEAKER_BLESSING  = 328507,
		SPELL_FREEZING_BLAST        = 352379,
		SPELL_DEEP_FREEZE           = 354638,
		SPELL_GLACIAL_WINDS         = 355055,
		SPELL_FROSTBOLT             = 371383,
		SPELL_FROST_BLAST           = 464527,
		SPELL_COMMAND_THE_DEAD      = 464563,
		SPELL_DEATH_BOLT            = 324589,
	};

	enum Groups
	{
		GROUP_NORMAL,
		GROUP_NECROMANCER
	};

	enum NPCs
	{
		// NPCs
		NPC_SOUL_WEAVER             = 230685,
		NPC_GHOUL_FROZEN_WASTES     = 230682,

		// DisplayId
		DISPLAY_NECOMANCER          = 6000017,
	};

	bool deepDreeze = false;
	bool darkspeakerBlessing = false;
	uint8 commandTheDead = 0;

	// Constantes
	const uint8 COMMAND_THE_DEAD_COUNT = 2;

	void Reset() override
	{
		CustomAI::Reset();

		me->AddAura(SPELL_HAUNTING_MEMORY, me);
	}

	void MovementInform(uint32 type, uint32 id) override
	{
		CustomAI::MovementInform(type, id);

		switch (id)
		{
			case MOVEMENT_INFO_POINT_01:
				me->SetHomePosition(RoomCenter);
                me->RemoveUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
                me->SetFaction(FACTION_MONSTER);
				me->SetImmuneToAll(false);
				break;
		}
	}

	void SummonedCreatureDies(Creature* summon, Unit* killer) override
	{
		CustomAI::SummonedCreatureDies(summon, killer);

		switch (summon->GetEntry())
		{
			case NPC_SOUL_WEAVER:
			case NPC_GHOUL_FROZEN_WASTES:
				commandTheDead++;
				break;
		}

		if (commandTheDead >= COMMAND_THE_DEAD_COUNT)
			me->RemoveAurasDueToSpell(SPELL_DARKSPEAKER_BLESSING);
	}

	void DoAction(int32 param) override
	{
		if (param != ACTION_KELTHUZAD_COMBAT_READY)
			return;

		if (Player* player = me->SelectNearestPlayer(40.f))
			Conversation::CreateConversation(CONVERSATION_KELTHUZAD_COMBAT, player, *player, player->GetGUID());
	}

	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
		if (!deepDreeze && me->HealthBelowPctDamaged(50, damage))
		{
			deepDreeze = true;
			for (uint8 i = 0; i < 30; ++i)
				DoCastSelf(SPELL_DEEP_FREEZE, true);
		}

		if (!darkspeakerBlessing && me->HealthBelowPctDamaged(30, damage))
		{
			darkspeakerBlessing = true;

			CastStop();
			DoCastSelf(SPELL_DARKSPEAKER_BLESSING, true);

			scheduler.CancelGroup(GROUP_NORMAL);

			scheduler
				.Schedule(1s, [this](TaskContext /*context*/)
				{
					// Change Kel Thuzad display
					me->SetDisplayId(DISPLAY_NECOMANCER);

					CastStop();
					DoCastSelf(SPELL_COMMAND_THE_DEAD);
				})
				.Schedule(2s, [this](TaskContext death_bolt)
				{
					DoCastVictim(SPELL_DEATH_BOLT);
					death_bolt.Repeat(4s);
				});
		}
	}

	void JustEngagedWith(Unit* who) override
	{
		DoCast(who, SPELL_FROSTBOLT);

		scheduler
			.Schedule(2s, GROUP_NORMAL, [this](TaskContext frostbolt)
			{
				DoCastVictim(SPELL_FROSTBOLT);
				frostbolt.Repeat(2s, 3s);
			})
			.Schedule(4s, 8s, GROUP_NORMAL, [this](TaskContext frost_blast)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random))
					DoCast(target, SPELL_FROST_BLAST);
				frost_blast.Repeat(6s, 8s);
			})
			.Schedule(10s, GROUP_NORMAL, [this](TaskContext freezing_blast)
			{
				if (Unit* target = me->GetVictim())
					me->SetFacingToObject(target);

				DoCastAOE(SPELL_FREEZING_BLAST);
				freezing_blast.Repeat(18s, 22s);
			})
			.Schedule(20s, GROUP_NORMAL, [this](TaskContext glacial_winds)
			{
				DoCastSelf(SPELL_GLACIAL_WINDS);
				glacial_winds.Repeat(35s, 45s);
			});
	}

	private:
	InstanceScript* instance;
};

struct npc_soul_weaver : public CustomAI
{
	npc_soul_weaver(Creature* creature) : CustomAI(creature, AI_Type::Stay) {}

	enum Spells
	{
		SPELL_SHADOW_BOLT       = 323720,
		SPELL_FROSTBOLT_VOLLEY  = 457334
	};

	void JustEngagedWith(Unit* who) override
	{
		DoCast(who, SPELL_SHADOW_BOLT);

		scheduler
			.Schedule(2300ms, [this](TaskContext shadow_bolt)
			{
				DoCastVictim(SPELL_SHADOW_BOLT);
				shadow_bolt.Repeat(2300ms, 4s);
			})
			.Schedule(8s, 14s, [this](TaskContext frostbolt_volley)
			{
				DoCastAOE(SPELL_FROSTBOLT_VOLLEY);
				frostbolt_volley.Repeat(12s, 16s);
			});
	}
};

struct npc_ghoul_frozen_wastes : public CustomAI
{
	npc_ghoul_frozen_wastes(Creature* creature) : CustomAI(creature, AI_Type::Melee) {}

	enum Spells
	{
		SPELL_TOXIC_VAPORS      = 25786,
		SPELL_RENDING_CLAW      = 374865
	};

	void JustEngagedWith(Unit* who) override
	{
		DoCast(who, SPELL_TOXIC_VAPORS);

		scheduler.Schedule(2s, 5s, [this](TaskContext rending_claw)
		{
			DoCastVictim(SPELL_RENDING_CLAW);
			rending_claw.Repeat(8s, 14s);
		});
	}
};

/*****
* SPELLS
*****/

// Glacial Winds - 355055
class spell_glacial_winds : public SpellScript
{
	void HandleLaunch(SpellEffIndex effIndex)
	{
		Unit* caster = GetCaster();
		if (!caster)
			return;

		uint32 spellId = GetSpellInfo()->GetEffect(effIndex).BasePoints;
		caster->CastSpell(caster, spellId, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
	}

	void Register() override
	{
		OnEffectLaunch += SpellEffectFn(spell_glacial_winds::HandleLaunch, EFFECT_0, SPELL_EFFECT_DUMMY);
		OnEffectLaunch += SpellEffectFn(spell_glacial_winds::HandleLaunch, EFFECT_1, SPELL_EFFECT_DUMMY);
		OnEffectLaunch += SpellEffectFn(spell_glacial_winds::HandleLaunch, EFFECT_2, SPELL_EFFECT_DUMMY);
	}
};

// Glacial Winds - 355058
// AT@29665
struct at_glacial_winds : AreaTriggerAI
{
	enum Spells
	{
		SPELL_GLACIAL_WINDS = 355058
	};

	at_glacial_winds(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) {}

	void OnUnitEnter(Unit* unit) override
	{
		Unit* caster = at->GetCaster();
		if (!caster)
			return;

		if (!caster->IsValidAttackTarget(unit))
			return;

		caster->CastSpell(unit, SPELL_GLACIAL_WINDS, true);
	}
};

// Deep Freeze - 354638
// AT@23187
struct at_deep_freeze : AreaTriggerAI
{
	enum Spells
	{
		SPELL_DEEP_FREEZE = 354639
	};

	at_deep_freeze(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) {}

	void OnUnitEnter(Unit* unit) override
	{
		Unit* caster = at->GetCaster();
		if (!caster)
			return;

		if (!caster->IsValidAttackTarget(unit))
			return;

		caster->CastSpell(unit, SPELL_DEEP_FREEZE, true);
	}

	void OnUnitExit(Unit* unit, AreaTriggerExitReason /*reason*/) override
	{
		unit->RemoveAurasDueToSpell(SPELL_DEEP_FREEZE);
	}
};

// Freezing Blast - 352381
class spell_freezing_blast : public SpellScript
{
	enum Spells
	{
		SPELL_FREEZING_BLAST_MISSILE = 352380
	};

    void OnPrecast() override
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        _orientation = caster->GetOrientation();
    }

	void HandleDamage(SpellEffIndex /*effIndex*/) const
	{
		Unit* caster = GetCaster();
		if (!caster)
			return;

		// 3 positions en ligne droite devant, espacées de 6y
		for (int i = 1; i <= 3; ++i)
		{
			float dist = 6.0f * i;

			Position dest;
			dest.m_positionX = caster->GetPositionX() + dist * std::cos(_orientation);
			dest.m_positionY = caster->GetPositionY() + dist * std::sin(_orientation);
			dest.m_positionZ = caster->GetPositionZ();
			dest.SetOrientation(_orientation);

			// Ajuster le Z au sol
			caster->UpdateAllowedPositionZ(dest.m_positionX, dest.m_positionY, dest.m_positionZ);

			CastSpellExtraArgs args(TRIGGERED_DONT_REPORT_CAST_ERROR | TRIGGERED_IGNORE_CAST_IN_PROGRESS);
			args.SetOriginalCaster(caster->GetGUID());

			caster->CastSpell(dest, SPELL_FREEZING_BLAST_MISSILE, args);
		}
	}

	void Register() override
	{
		OnEffectLaunchTarget += SpellEffectFn(spell_freezing_blast::HandleDamage, EFFECT_0, SPELL_EFFECT_DUMMY);
	}

    private:
    float _orientation = 0.f;
};

/*****
* CONVERSATIONS
*****/

// 60000 - Kirin Tor's Fate Introduction
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

// 60001 - Kirin Tor's Fate Start
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

		}, conversation->GetLineEndTime(privateOwnerLocale, CONVERSATION_LINE_START_01));

		conversation->m_Events.AddEvent([ai = this, conversation = conversation]()
		{
			Creature* anduin = ai->instance->GetCreature(DATA_ANDUIN);
			if (!anduin)
				return;

			anduin->GetMotionMaster()->MovePath(ActorsPath01, false);

		}, conversation->GetLineEndTime(privateOwnerLocale, CONVERSATION_LINE_START_02));
	}

	private:
	InstanceScript* instance;
};

// 60002 - 
class conversation_dalaran_part01 : public ConversationAI
{
	public:
		conversation_dalaran_part01(Conversation* conversation) : ConversationAI(conversation), instance(nullptr) { }

	enum TheKirinTorFate
	{
		// Lines
		CONVERSATION_LINE_07                    = 7, // ...Jaina !
		CONVERSATION_LINE_08                    = 8, // I'm not proud. Since then, Kalecgos  [...]
		CONVERSATION_LINE_09                    = 9, // The Kirin-Tor has a legacy of abuse. [...]

		// Actors
		CONVERSATION_ACTOR_IDX_JAINA_PROUDMOORE = 0,
		CONVERSATION_ACTOR_IDX_ANDUIN_WRYNN     = 1,
	};

	void OnCreate(Unit* creator) override
	{
		instance = creator->GetInstanceScript();

		Creature* jaina = creator->FindNearestCreatureWithOptions(50.0f, { .CreatureId = NPC_JAINA_PROUDMOORE, .IgnorePhases = true });
		Creature* anduin = creator->FindNearestCreatureWithOptions(50.0f, { .CreatureId = NPC_ANDUIN_WRYNN, .IgnorePhases = true });
		if (!jaina || !anduin)
			return;

		conversation->AddActor(CONVERSATION_INTRODUCTION, CONVERSATION_ACTOR_IDX_JAINA_PROUDMOORE, jaina->GetGUID());
		conversation->AddActor(CONVERSATION_INTRODUCTION, CONVERSATION_ACTOR_IDX_ANDUIN_WRYNN, anduin->GetGUID());
		conversation->Start();
	}

	void OnStart() override
	{
		instance->SetData(DATA_PHASE, (uint32)Phases::Visions_Jaina);

        LocaleConstant privateOwnerLocale = conversation->GetPrivateObjectOwnerLocale();
    
        conversation->m_Events.AddEvent([ai = this, conversation = conversation]()
		{
            ai->instance->SetData(DATA_PHASE, (uint32)Phases::Visions_KalecgosJaina);

		}, conversation->GetLineEndTime(privateOwnerLocale, CONVERSATION_LINE_07));

        conversation->m_Events.AddEvent([ai = this, conversation = conversation]()
		{
            ai->instance->TriggerGameEvent(EVENT_FIND_KALECGOS_ASSIST_JAINA);

		}, conversation->GetLineEndTime(privateOwnerLocale, CONVERSATION_LINE_09));
	}

	private:
	InstanceScript* instance;
};

// 60003 - 
class conversation_dalaran_kelthuzad : public ConversationAI
{
	public:
		conversation_dalaran_kelthuzad(Conversation* conversation) : ConversationAI(conversation), instance(nullptr) { }

	enum TheKirinTorFate
	{
        // Lines
        CONVERSATION_LINE_20                = 20,

		// Actors
		CONVERSATION_ACTOR_IDX_KELTHUZAD    = 0,
	};

	void OnCreate(Unit* creator) override
	{
		instance = creator->GetInstanceScript();

		Creature* kelthuzad = creator->FindNearestCreatureWithOptions(50.0f, { .CreatureId = NPC_KELTHUZAD, .IgnorePhases = true });
		if (!kelthuzad)
			return;

		conversation->AddActor(CONVERSATION_START, CONVERSATION_ACTOR_IDX_KELTHUZAD, kelthuzad->GetGUID());
		conversation->Start();
	}

	void OnStart() override
	{
		LocaleConstant privateOwnerLocale = conversation->GetPrivateObjectOwnerLocale();

        conversation->m_Events.AddEvent([ai = this, conversation = conversation]()
		{
            Player* player = ObjectAccessor::GetPlayer(*conversation, conversation->GetPrivateObjectOwner());
            Creature* kelthuzad = ai->instance->GetCreature(DATA_KELTHUZAD);

            if (!player || !kelthuzad)
                return;

            kelthuzad->SetFacingToObject(player);

		}, conversation->GetLineEndTime(privateOwnerLocale, CONVERSATION_LINE_20));

		conversation->m_Events.AddEvent([ai = this, conversation = conversation]()
		{
            Creature* kelthuzad = ai->instance->GetCreature(DATA_KELTHUZAD);
            if (!kelthuzad)
                return;

            kelthuzad->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_01, RoomCenter, true, 5.61f, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);

		}, conversation->GetLastLineEndTime(privateOwnerLocale));
	}

	private:
	InstanceScript* instance;
};

// 60004 - 
class conversation_dalaran_part02 : public ConversationAI
{
	public:
		conversation_dalaran_part02(Conversation* conversation) : ConversationAI(conversation), instance(nullptr) { }

	enum TheKirinTorFate
	{
		// Actors
		CONVERSATION_ACTOR_IDX_JAINA_PROUDMOORE = 0,
		CONVERSATION_ACTOR_IDX_ANDUIN_WRYNN     = 1,
	};

	void OnCreate(Unit* creator) override
	{
		instance = creator->GetInstanceScript();

		Creature* jaina = creator->FindNearestCreatureWithOptions(50.0f, { .CreatureId = NPC_JAINA_PROUDMOORE, .IgnorePhases = true });
		Creature* anduin = creator->FindNearestCreatureWithOptions(50.0f, { .CreatureId = NPC_ANDUIN_WRYNN, .IgnorePhases = true });
		if (!jaina || !anduin)
			return;

		conversation->AddActor(CONVERSATION_INTRODUCTION, CONVERSATION_ACTOR_IDX_JAINA_PROUDMOORE, jaina->GetGUID());
		conversation->AddActor(CONVERSATION_INTRODUCTION, CONVERSATION_ACTOR_IDX_ANDUIN_WRYNN, anduin->GetGUID());
		conversation->Start();
	}

	void OnStart() override
	{
		LocaleConstant privateOwnerLocale = conversation->GetPrivateObjectOwnerLocale();

		conversation->m_Events.AddEvent([conversation = conversation]()
		{
			/* TODO */

		}, conversation->GetLastLineEndTime(privateOwnerLocale));
	}

	private:
	InstanceScript* instance;
};

/*****
* AREA TRIGGERS
*****/

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

	virtual void Process(Player* /*player*/) { }

	void OnUnitEnter(Unit* unit) override
	{
        if (consumed)
            return;

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

struct areatrigger_dalaran_visions : public areatrigger_dalaran
{
	areatrigger_dalaran_visions(AreaTrigger* at) : areatrigger_dalaran(at) {}

	Phases CheckPhase() override
	{
		return Phases::Start_CanTeleport;
	}

	Phases NextPhase() override
	{
		return Phases::Visions;
	}

	void Process(Player* /*player*/) override
	{
        instance->TriggerGameEvent(EVENT_FIND_GUARDIAN_FIND_JAINA);
	}
};

struct areatrigger_dalaran_teleport_guardian : public areatrigger_dalaran
{
	areatrigger_dalaran_teleport_guardian(AreaTrigger* at) : areatrigger_dalaran(at) { }

	Phases CheckPhase() override
	{
		return Phases::Start_CanTeleport;
	}

	void Process(Player* player) override
	{
		player->CastSpell(PlayerPos01, SPELL_TELEPORT);
	}
};

void AddSC_npcs_dalaran_convo()
{
	// NPCs
	RegisterConvoAI(npc_jaina_proudmoore_convo);
	RegisterConvoAI(npc_anduin_wrynn_convo);
	RegisterConvoAI(npc_kelthuzad_vision);
	RegisterConvoAI(npc_soul_weaver);
	RegisterConvoAI(npc_ghoul_frozen_wastes);

	// Spells
	RegisterSpellScript(spell_glacial_winds);
	RegisterSpellScript(spell_freezing_blast);
	RegisterAreaTriggerAI(at_glacial_winds);
	RegisterAreaTriggerAI(at_deep_freeze);

	// Conversations
	RegisterConversationAI(conversation_dalaran_introduction);
	RegisterConversationAI(conversation_dalaran_start);

	// Visions
	RegisterConversationAI(conversation_dalaran_part01);
	RegisterConversationAI(conversation_dalaran_part02);

		// Combat avec Kel Thuzad
		RegisterConversationAI(conversation_dalaran_kelthuzad);

	RegisterAreaTriggerAI(areatrigger_dalaran_introduction);
	RegisterAreaTriggerAI(areatrigger_dalaran_visions);
	RegisterAreaTriggerAI(areatrigger_dalaran_teleport_guardian);
}
