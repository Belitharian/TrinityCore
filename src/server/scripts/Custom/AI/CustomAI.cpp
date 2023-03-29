#include "CustomAI.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"

CustomAI::CustomAI(Creature* creature, AI_Type type) : ScriptedAI(creature),
	type(type), summons(creature), wasInterrupted(false), canCombatMove(true)
{
	Initialize();
}

void CustomAI::Initialize()
{
    interruptCounter = 0;

    scheduler.SetValidator([this]
    {
        return !me->HasUnitState(UNIT_STATE_CASTING) || !me->HasUnitState(UNIT_STATE_FLEEING) || !me->HasUnitState(UNIT_STATE_FLEEING_MOVE);
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
    if (caster->GetGUID() == me->GetGUID())
        return;

    if (spellInfo->HasEffect(SPELL_EFFECT_INTERRUPT_CAST) || spellInfo->HasEffect(SPELL_EFFECT_KNOCK_BACK))
    {
        wasInterrupted = true;

        SetCombatMove(true);

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

        if (type == AI_Type::Distance)
        {
            int32 duration = spellInfo->GetDuration() / IN_MILLISECONDS;
            scheduler.Schedule(Seconds(duration), [this](TaskContext /*context*/)
            {
                wasInterrupted = false;

                SetCombatMove(false, GetDistance());
            });
        }
    }
}

void CustomAI::EnterEvadeMode(EvadeReason why)
{
	summons.DespawnAll();
	scheduler.CancelAll();

	ScriptedAI::EnterEvadeMode(why);
}

void CustomAI::Reset()
{
	Initialize();

	summons.DespawnAll();
	scheduler.CancelAll();

	ScriptedAI::Reset();
}

void CustomAI::AttackStart(Unit* who)
{
	if (!who)
		return;

    if (who && me->Attack(who, true))
    {
        me->GetMotionMaster()->Clear(MOTION_PRIORITY_NORMAL);
        me->PauseMovement();

        if (type == AI_Type::Distance)
        {
            me->SetCanMelee(true);
            me->SetSheath(SHEATH_STATE_UNARMED);
        }
        else if (type == AI_Type::Melee || type == AI_Type::Hybrid)
        {
            me->SetCanMelee(true);
            me->GetMotionMaster()->MoveChase(who);
        }
        else
        {
            me->SetCanMelee(false);
            me->SetSheath(SHEATH_STATE_UNARMED);
        }
    }
}

void CustomAI::JustDied(Unit* killer)
{
	summons.DespawnAll();
	scheduler.CancelAll();

	ScriptedAI::JustDied(killer);
}

void CustomAI::UpdateAI(uint32 diff)
{
	scheduler.Update(diff, [this]
	{
		if (UpdateVictim())
		{
            DoMeleeAttackIfReady();

            if (type == AI_Type::Distance || type == AI_Type::None)
            {
                if (Unit* target = me->GetVictim())
                {
                    if (!me->IsWithinLOSInMap(target))
                    {
                        SetCombatMove(true, GetDistance());
                    }
                    else
                    {
                        if (me->IsInRange(target, me->GetCombatReach(), GetDistance()))
                        {
                            me->SetCanMelee(false);
                            SetCombatMove(false);
                        }
                        else
                        {
                            me->SetCanMelee(true);
                            SetCombatMove(true, GetDistance());
                        }
                    }
                }
            }
		}
	});
}

bool CustomAI::CanAIAttack(Unit const* who) const
{
	return who->IsAlive() && me->IsValidAttackTarget(who)
		&& !who->HasAuraType(SPELL_AURA_MOD_FEAR_2)
		&& !who->HasBreakableByDamageCrowdControlAura(me)
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

void CustomAI::SetCombatMove(bool on, float distance, bool stopMoving)
{
    if (canCombatMove == on)
        return;

    canCombatMove = on;

    if (distance)
        distance = me->GetCombatReach();

    if (me->IsEngaged())
    {
        if (on)
        {
            if (!me->HasReactState(REACT_PASSIVE) && me->GetVictim() && !me->GetMotionMaster()->HasMovementGenerator([](MovementGenerator const* movement) -> bool
            {
                return movement->GetMovementGeneratorType() == CHASE_MOTION_TYPE && movement->Mode == MOTION_MODE_DEFAULT && movement->Priority == MOTION_PRIORITY_NORMAL;
            }))
            {
                me->GetMotionMaster()->MoveChase(me->GetVictim(), distance);
            }
        }
        else if (MovementGenerator* movement = me->GetMotionMaster()->GetMovementGenerator([](MovementGenerator const* a) -> bool
        {
            return a->GetMovementGeneratorType() == CHASE_MOTION_TYPE && a->Mode == MOTION_MODE_DEFAULT && a->Priority == MOTION_PRIORITY_NORMAL;
        }))
        {
            me->GetMotionMaster()->Remove(movement);
            if (stopMoving)
                me->StopMoving();
        }
    }
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

std::list<Unit*> CustomAI::DoFindMissingBuff(uint32 spellId)
{
	std::list<Unit*> list;
	Trinity::FriendlyMissingBuff u_check(me, spellId);
	Trinity::CreatureListSearcher<Trinity::FriendlyMissingBuff> searcher(me, list, u_check);
	Cell::VisitAllObjects(me, searcher, SIZE_OF_GRIDS);
	return list;
}
