#include "CustomAI.h"
#include "CellImpl.h"
#include "Containers.h"
#include "GridNotifiers.h"

CustomAI::CustomAI(Creature* creature, AI_Type type) : ScriptedAI(creature),
	type(type), summons(creature), canCombatMove(true), damageReduction(false), textOnCooldown(false)
{
	Initialize();
}

CustomAI::CustomAI(Creature* creature, bool damageReduction, AI_Type type) : ScriptedAI(creature),
    type(type), summons(creature), canCombatMove(true), damageReduction(damageReduction), textOnCooldown(false)
{
    Initialize();
}

void CustomAI::Initialize()
{
    interruptCounter = 0;

    scheduler.SetValidator([this]
    {
        return !me->HasBreakableByDamageCrowdControlAura()
            || !me->HasAuraType(SPELL_AURA_MOD_FEAR_2)
            || !me->HasUnitState(UNIT_STATE_CASTING)
            || !me->HasUnitState(UNIT_STATE_FLEEING)
            || !me->HasUnitState(UNIT_STATE_FLEEING_MOVE);
    });
}

void CustomAI::JustSummoned(Creature* summon)
{
	summons.Summon(summon);

	ScriptedAI::JustSummoned(summon);
}

void CustomAI::SummonedCreatureDespawn(Creature* summon)
{
	summons.Despawn(summon);

	ScriptedAI::SummonedCreatureDespawn(summon);
}

void CustomAI::SummonedCreatureDies(Creature* summon, Unit* killer)
{
	summons.Despawn(summon);

	ScriptedAI::SummonedCreatureDies(summon, killer);
}

void CustomAI::SpellHit(WorldObject* caster, SpellInfo const* spellInfo)
{
    if (caster->GetGUID() != me->GetGUID()
        && (spellInfo->HasEffect(SPELL_EFFECT_INTERRUPT_CAST) || spellInfo->HasEffect(SPELL_EFFECT_KNOCK_BACK)))
    {
        interruptCounter++;
        if (interruptCounter >= 3)
        {
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_INTERRUPT_CAST, true);
            me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_STUN, true);
            me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_EFFECT_KNOCK_BACK, true);

            scheduler.Schedule(5s, [this](TaskContext /*context*/)
            {
                me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_INTERRUPT_CAST, false);
                me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_STUN, false);
                me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_EFFECT_KNOCK_BACK, false);

                interruptCounter = 0;
            });
        }
    }
}

void CustomAI::EnterEvadeMode(EvadeReason why)
{
    if (me->GetWaypointPathId() != 0)
    {
        me->ResumeMovement();
    }

    me->RemoveAllAreaTriggers();

	summons.DespawnAll();
	scheduler.CancelAll();

	ScriptedAI::EnterEvadeMode(why);
}

void CustomAI::Reset()
{
	Initialize();

    me->RemoveAllAreaTriggers();

	summons.DespawnAll();
	scheduler.CancelAll();

    switch (type)
    {
        case AI_Type::Distance:
        case AI_Type::Stay:
            me->SetSheath(SHEATH_STATE_UNARMED);
            break;
    }

	ScriptedAI::Reset();
}

void CustomAI::AttackStart(Unit* who)
{
	if (!who)
		return;

    if (type == AI_Type::Stay && me->Attack(who, false))
    {
        me->SetSheath(SHEATH_STATE_UNARMED);
        SetCombatMovement(false);
        return;
    }

    if (me->Attack(who, true))
    {
        switch (type)
        {
            case AI_Type::Distance:
                me->GetMotionMaster()->MoveChase(who, GetDistance());
                break;
            case AI_Type::Melee:
            case AI_Type::Hybrid:
                me->GetMotionMaster()->MoveChase(who);
                break;
            default:
                break;
        }
    }
}

void CustomAI::JustDied(Unit* killer)
{
	summons.DespawnAll();
	scheduler.CancelAll();

    me->RemoveAllAreaTriggers();

	ScriptedAI::JustDied(killer);
}

void CustomAI::UpdateAI(uint32 diff)
{
    UpdateVictim();
    scheduler.Update(diff);
}

bool CustomAI::CanAIAttack(Unit const* who) const
{
	return who->IsAlive() && me->IsValidAttackTarget(who)
		&& !who->HasAuraType(SPELL_AURA_MOD_FEAR_2)
		&& !who->HasBreakableByDamageCrowdControlAura()
        && who->GetEntry() != NPC_TRAINING_DUMMY
		&& ScriptedAI::CanAIAttack(who);
}

void CustomAI::CastStop()
{
	for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; i++)
		me->InterruptSpell(CurrentSpellTypes(i), false);
}

void CustomAI::CastStop(const std::vector<uint32>& exceptions)
{
	for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; i++)
	{
		if (const Spell* spell = me->GetCurrentSpell(i))
		{
			if (std::find(exceptions.begin(), exceptions.end(), spell->m_spellInfo->Id) != exceptions.end())
				continue;

			me->InterruptSpell(CurrentSpellTypes(i), false);
		}
	}
}

void CustomAI::CastStop(uint32 exception)
{
	for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; i++)
	{
		if (const Spell* spell = me->GetCurrentSpell(i))
		{
			if (spell->m_spellInfo->Id == exception)
				continue;

			me->InterruptSpell(CurrentSpellTypes(i), false);
		}
	}
}


uint32 CustomAI::FriendsInRange(float range, uint8 pct)
{
    std::list<Unit*> list;
    Trinity::FriendlyBelowHpPctInRange u_check(me, range, pct);
    Trinity::UnitListSearcher<Trinity::FriendlyBelowHpPctInRange> searcher(me, list, u_check);
    Cell::VisitAllObjects(me, searcher, range);
    return list.size();
}

uint32 CustomAI::EnemiesInRange(float distance)
{
	uint32 count = 0;
	for (ThreatReference const* ref : me->GetThreatManager().GetUnsortedThreatList())
		if (me->IsWithinDist(ref->GetVictim(), distance))
			++count;
	return count;
}

uint32 CustomAI::EnemiesInFront(float distance)
{
	uint32 count = 0;
	for (ThreatReference const* ref : me->GetThreatManager().GetUnsortedThreatList())
		if (me->isInFrontInMap(ref->GetVictim(), distance))
			++count;
	return count;
}

bool CustomAI::HasMechanic(SpellInfo const* spellInfo, Mechanics mechanic)
{
	return spellInfo->GetAllEffectsMechanicMask() & (UI64LIT(1) << mechanic);
}

void CustomAI::TalkInCombat(uint8 textId, uint64 cooldown)
{
    if (!textOnCooldown)
    {
        me->AI()->Talk(textId);

        textOnCooldown = true;
        scheduler.Schedule(Seconds(cooldown), [this](TaskContext /*context*/)
        {
            textOnCooldown = false;
        });
    }
}

std::list<Unit*> CustomAI::DoFindMissingBuff(uint32 spellId)
{
    const SpellInfo* info = sSpellMgr->AssertSpellInfo(spellId, DIFFICULTY_NONE);

    float range = info->GetEffect(EFFECT_0).CalcRadius();

	std::list<Unit*> list;
    FriendlyMissingBuff u_check(me, spellId, range);
	Trinity::UnitListSearcher<FriendlyMissingBuff> searcher(me, list, u_check);
	Cell::VisitAllObjects(me, searcher, range);
	return list;
}

Unit* CustomAI::SelectRandomMissingBuff(uint32 spell)
{
    std::list<Unit*> list = DoFindMissingBuff(spell);
    if (list.empty())
        return nullptr;
    return Trinity::Containers::SelectRandomContainerElement(list);
}
