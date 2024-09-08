#include "AreaTriggerAI.h"
#include "ScriptMgr.h"
#include "InstanceScript.h"
#include "ObjectAccessor.h"
#include "PassiveAI.h"
#include "pit_of_saron.h"
#include "Player.h"
#include "GameObjectAI.h"
#include "ScriptedCreature.h"
#include "SpellScript.h"
#include "Vehicle.h"
#include "Custom/AI/CustomAI.h"

struct npc_elandra_koreln : public CustomAI
{
    npc_elandra_koreln(Creature* creature) : CustomAI(creature, AI_Type::Hybrid)
	{
	}

	enum Spells
	{
		SPELL_COUNTERSPELL          = 173077,
		SPELL_FROSTBITE             = 198121,
		SPELL_FROSTBOLT             = 284703,
		SPELL_FROST_NOVA            = 284879,
		SPELL_BLINK                 = 284877,
	};

	float GetDistance() override
	{
		return 25.0f;
	}

	void OnSpellCast(SpellInfo const* spellInfo) override
	{
		if (spellInfo->Id == SPELL_FROST_NOVA)
		{
			scheduler.Schedule(1s, [this](TaskContext blink)
			{
				CastStop();
				DoCast(SPELL_BLINK);
			});
		}
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		scheduler
			.Schedule(2s, [this](TaskContext fireball)
			{
				DoCastVictim(SPELL_FROSTBOLT);
				fireball.Repeat(2s);
			})
			.Schedule(10s, [this](TaskContext counterspell)
			{
				if (Unit* target = DoSelectCastingUnit(SPELL_COUNTERSPELL, 30.0f))
				{
					CastStop();
					DoCast(target, SPELL_COUNTERSPELL);
					counterspell.Repeat(15s, 30s);
				}
				else
					counterspell.Repeat(1s);
			})
			.Schedule(8s, [this](TaskContext frost_nova)
			{
				if (EnemiesInRange(12.0f) >= 2)
				{
					CastStop();
					DoCast(SPELL_FROST_NOVA);
					frost_nova.Repeat(8s, 10s);
				}
				else
					frost_nova.Repeat(1s);
			});
	}

	void SpellHitTarget(WorldObject* target, SpellInfo const* spellInfo) override
	{
		if (spellInfo->GetSchoolMask() == SPELL_SCHOOL_MASK_FROST
			&& roll_chance_i(30))
		{
			Unit* victim = target->ToUnit();
			if (victim && victim->GetGUID() != me->GetGUID())
				DoCast(victim, SPELL_FROSTBITE);
		}
	}
};

struct npc_jaina_pos : public CustomAI
{
    npc_jaina_pos(Creature* creature) : CustomAI(creature, true, AI_Type::Melee)
    {
        instance = creature->GetInstanceScript();
    }

	enum Spells
	{
        SPELL_ARCTIC_ARMOR              = 287282,
		SPELL_FIREBLAST                 = 319836,
		SPELL_FIREBALL                  = 417030,
		SPELL_BLIZZARD                  = 429749,
    };

    InstanceScript* instance;

    void EnterEvadeMode(EvadeReason why)
    {
        CustomAI::EnterEvadeMode(why);

        Phases phase = (Phases)instance->GetData(DATA_PHASE);
        if (phase == Phases::Introduction)
        {
            scheduler.Schedule(1s, [this](TaskContext context)
            {
                switch (context.GetRepeatCounter())
                {
                    case 0:
                        me->AI()->Talk(SAY_JAINA_09);
                        context.Repeat(4s);
                        break;
                    case 1:
                        me->AI()->Talk(SAY_JAINA_10);
                    break;
                }
            });

            instance->SetData(DATA_PHASE, (uint32)Phases::Paused);
        }
    }

	void JustEngagedWith(Unit* /*who*/) override
	{
        DoCastSelf(SPELL_ARCTIC_ARMOR);

		scheduler
			.Schedule(1s, [this](TaskContext fireball)
			{
				DoCastVictim(SPELL_FIREBALL);
				fireball.Repeat(2s, 8s);
			})
			.Schedule(3s, [this](TaskContext fireblast)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_FIREBLAST);
				fireblast.Repeat(8s, 14s);
			})
			.Schedule(8s, [this](TaskContext blizzard)
			{
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                {
                    DoCast(target, SPELL_BLIZZARD);
                }
				blizzard.Repeat(14s, 22s);
			});
	}
};

struct go_ball_and_chain : public GameObjectAI
{
    go_ball_and_chain(GameObject* gameobject) : GameObjectAI(gameobject)
    {
        instance = gameobject->GetInstanceScript();
    }

    bool OnGossipHello(Player* /*player*/) override
    {
        Phases phase = (Phases)instance->GetData(DATA_PHASE);
        if (phase == Phases::Paused)
        {
            instance->SetData(DATA_PHASE, (uint32)Phases::FreeMartinVictus);
            me->DespawnOrUnsummon(1ms);
        }

        return false;
    }

    InstanceScript* instance;
};

struct areatrigger_pit_of_saron_intro : public AreaTriggerAI
{
    areatrigger_pit_of_saron_intro(AreaTrigger* at) : AreaTriggerAI(at)
    {
        instance = at->GetInstanceScript();
    }

    void OnUnitEnter(Unit* unit) override
    {
        if (unit->GetTypeId() != TYPEID_PLAYER)
            return;

        Player* player = unit->ToPlayer();
        if (player && player->IsGameMaster())
            return;

        Phases phase = (Phases)instance->GetData(DATA_PHASE);
        if (phase == Phases::None)
        {
            instance->SetData(DATA_PHASE, Phases::Introduction);
        }
    }

    private:
    InstanceScript* instance;
};

struct areatrigger_pit_of_saron_tyrannus : public AreaTriggerAI
{
    areatrigger_pit_of_saron_tyrannus(AreaTrigger* at) : AreaTriggerAI(at)
    {
        instance = at->GetInstanceScript();
    }

    void OnUnitEnter(Unit* unit) override
    {
        if (unit->GetTypeId() != TYPEID_PLAYER)
            return;

        Player* player = unit->ToPlayer();
        if (player && player->IsGameMaster())
            return;

        instance->SetData(DATA_PHASE, (uint32)Phases::TyrannusIntroduction);

        //if (instance->GetBossState(DATA_TYRANNUS) != IN_PROGRESS && instance->GetBossState(DATA_TYRANNUS) != DONE)
        //    if (Creature* tyrannus = ObjectAccessor::GetCreature(*player, instance->GetGuidData(DATA_TYRANNUS)))
        //    {
        //        //tyrannus->AI()->DoAction(ACTION_START_INTRO);
        //    }
    }

    private:
    InstanceScript* instance;
};

void AddSC_pit_of_saron_custom()
{
    // Standard creatures
    RegisterCreatureAI(npc_elandra_koreln);

    // Custom creatures
    RegisterPitOfSaronCustomCreatureAI(npc_jaina_pos);

    // AreaTriggers
    RegisterAreaTriggerAI(areatrigger_pit_of_saron_intro);
    RegisterAreaTriggerAI(areatrigger_pit_of_saron_tyrannus);

    // Misc
    RegisterGameObjectAI(go_ball_and_chain);
}
