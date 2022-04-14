#include "Custom/AI/CustomAI.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "ScriptMgr.h"
#include "dalaran_purge.h"

struct npc_jaina_dalaran_purge : public CustomAI
{
	npc_jaina_dalaran_purge(Creature* creature) : CustomAI(creature)
	{
		Initialize();
	}

	void Initialize()
	{
		instance = me->GetInstanceScript();
	}

    enum Misc
    {
        // Gossip
        GOSSIP_MENU_DEFAULT         = 65004,
        GOSSIP_MENU_UNMASKED        = 65005,
    };

	InstanceScript* instance;

    bool OnGossipHello(Player* player) override
    {
        DLPPhases phase = (DLPPhases)instance->GetData(DATA_SCENARIO_PHASE);
        if (phase > DLPPhases::TheFinalAssault_Illusion)
            return false;

        player->PrepareGossipMenu(me, phase == DLPPhases::TheFinalAssault_Illusion ? GOSSIP_MENU_UNMASKED : GOSSIP_MENU_DEFAULT, true);
        player->SendPreparedGossip(me);
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        ClearGossipMenuFor(player);

        switch (gossipListId)
        {
            case 0:
                me->RemoveAurasDueToSpell(SPELL_CHAT_BUBBLE);
                me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                scheduler.Schedule(2s, [player, this](TaskContext context)
                {
                    switch (context.GetRepeatCounter())
                    {
                        case 0:
                            me->SetFacingToObject(player);
                            context.Repeat(2s);
                            break;
                        case 1:
                            CastHordeIllusionOnPlayers(player);
                            context.Repeat(2s);
                            break;
                        case 2:
                            instance->SetData(EVENT_SPEAK_TO_JAINA, 1U);
                            me->SetFacingTo(me->GetHomePosition().GetOrientation());
                            me->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                            break;
                    }
                });
                break;
            case 1:
                CastHordeIllusionOnPlayers(player);
                break;
        }

        CloseGossipMenuFor(player);
        return true;
    }

    void SetGUID(ObjectGuid const& guid, int32 /*id*/ = 0) override
    {
        scheduler.Schedule(2s, [guid, this](TaskContext /*context*/)
        {
            if (Player* player = ObjectAccessor::GetPlayer(*me, guid))
                player->CombatStop(true);
        });
    }

	void MoveInLineOfSight(Unit* who) override
	{
		ScriptedAI::MoveInLineOfSight(who);

		if (me->IsEngaged())
			return;

		if (who->GetTypeId() != TYPEID_PLAYER)
			return;

		if (Player* player = who->ToPlayer())
		{
			if (player->IsGameMaster())
				return;

			if (player->IsFriendlyTo(me) && player->IsWithinDist(me, 5.f))
			{
				DLPPhases phase = (DLPPhases)instance->GetData(DATA_SCENARIO_PHASE);
				switch (phase)
				{
					case DLPPhases::FindJaina01:
						instance->DoSendScenarioEvent(EVENT_FIND_JAINA_01);
						break;
                    case DLPPhases::FindJaina02:
                        instance->SetData(EVENT_FIND_JAINA_02, 1U);
                        break;
                    default:
						break;
				}
			}
		}
	}

    void CastHordeIllusionOnPlayers(Player* player)
    {
        me->HandleEmoteCommand(EMOTE_ONESHOT_CASTSTRONG, player);
        if (Map* map = me->GetMap())
        {
            map->DoOnPlayers([this](Player* player)
            {
                player->CombatStop();
                player->CastSpell(player, SPELL_FACTION_OVERRIDE);
                player->CastSpell(player, SPELL_HORDE_ILLUSION);
            });
        }
    }
};

struct npc_aethas_sunreaver_purge : public CustomAI
{
	npc_aethas_sunreaver_purge(Creature* creature) : CustomAI(creature)
	{
		Initialize();
	}

	void Initialize()
	{
		instance = me->GetInstanceScript();
	}

	InstanceScript* instance;

    void MovementInform(uint32 /*type*/, uint32 id) override
    {
        switch (id)
        {
            case MOVEMENT_INFO_POINT_01:
                instance->SetData(ACTION_AETHAS_ESCAPED, 1U);
                DoCastSelf(SPELL_TELEPORT_VISUAL_ONLY);
                me->DespawnOrUnsummon(1800ms);
                break;
        }
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id == SPELL_ARCANE_BOMBARDMENT || spellInfo->Id == SPELL_FROSTBOLT)
        {
            damage = 0;
        }
    }

	void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
	{
		if (spellInfo->Id == SPELL_FROSTBOLT)
		{
			DoCastSelf(SPELL_ICY_GLARE);
			DoCastSelf(SPELL_CHILLING_BLAST, true);
		}
	}
};

struct npc_magister_rommath_purge : public CustomAI
{
    npc_magister_rommath_purge(Creature* creature) : CustomAI(creature)
	{
		Initialize();
	}

	void Initialize()
	{
		instance = me->GetInstanceScript();
	}

    InstanceScript* instance;

    void SetData(uint32 /*id*/, uint32 /*value*/) override
    {
        instance->SetData(EVENT_INFILTRATE_THE_SUNREAVER, 1U);
    }

    void MovementInform(uint32 /*type*/, uint32 id) override
    {
        switch (id)
        {
            case MOVEMENT_INFO_POINT_01:
                me->GetMotionMaster()->MoveJump(RommathPos03, 22.0f, 22.0f, MOVEMENT_INFO_POINT_02);
                break;
            case MOVEMENT_INFO_POINT_02:
                me->SetHomePosition(RommathPath01[ROMMATH_PATH_01 - 1]);
                me->GetMotionMaster()->MoveSmoothPath(MOVEMENT_INFO_POINT_03, RommathPath01, ROMMATH_PATH_01);
                break;
            case MOVEMENT_INFO_POINT_03:
                me->GetMotionMaster()->MoveTargetedHome();
                instance->SetData(EVENT_HELP_FREE_AETHAS_SUNREAVER, 1U);
                break;
        }
    }

	void MoveInLineOfSight(Unit* who) override
	{
		ScriptedAI::MoveInLineOfSight(who);

		if (me->IsEngaged())
			return;

		if (who->GetTypeId() != TYPEID_PLAYER)
			return;

		if (Player* player = who->ToPlayer())
		{
			if (player->IsGameMaster())
				return;

			if (player->IsFriendlyTo(me) && player->IsWithinDist(me, 5.f))
			{
				DLPPhases phase = (DLPPhases)instance->GetData(DATA_SCENARIO_PHASE);
				switch (phase)
				{
					case DLPPhases::TheFinalAssault_Illusion:
                        instance->SetData(EVENT_INFILTRATE_THE_SUNREAVER, 1U);
						break;
                    default:
						break;
				}
			}
		}
	}
};

void AddSC_dalaran_purge()
{
    RegisterDalaranAI(npc_jaina_dalaran_purge);
    RegisterDalaranAI(npc_aethas_sunreaver_purge);
    RegisterDalaranAI(npc_magister_rommath_purge);
}
