#include "CustomAI.h"

CustomAI::CustomAI(Creature* creature, AI_Type type) : ScriptedAI(creature),
    type(type), summons(creature)
{
    Initialize();
}

void CustomAI::ReleaseFocus()
{
    me->ReleaseFocus(nullptr, false);   // remove spellcast focus
    me->DoNotReacquireTarget();         // cancel delayed re-target
    me->SetTarget(ObjectGuid::Empty);   // drop target - dead mobs shouldn't ever target things
}

void CustomAI::Initialize()
{
    me->SetCombatReach(10.f);

    if (type == AI_Type::Distance)
    {
        scheduler.SetValidator([this]
        {
            return !me->HasUnitState(UNIT_STATE_CASTING)
                || !me->HasUnitState(UNIT_STATE_FLEEING)
                || !me->HasUnitState(UNIT_STATE_FLEEING_MOVE);
        });
    }
    else
    {
        scheduler.SetValidator([this]
        {
            return !me->HasUnitState(UNIT_STATE_FLEEING) || !me->HasUnitState(UNIT_STATE_FLEEING_MOVE);
        });
    }
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

void CustomAI::EnterEvadeMode(EvadeReason why)
{
    ReleaseFocus();

    summons.DespawnAll();
    scheduler.CancelAll();

    ScriptedAI::EnterEvadeMode(why);
}

void CustomAI::Reset()
{
    Initialize();
    ReleaseFocus();

    summons.DespawnAll();
    scheduler.CancelAll();

    ScriptedAI::Reset();
}

void CustomAI::AttackStart(Unit* who)
{
    if (!who)
        return;

    if (me->Attack(who, true))
    {
        if (type != AI_Type::Distance)
            DoStartMovement(who);
        else
            DoStartMovement(who, GetDistance());
        SetCombatMovement(true);
    }
}

void CustomAI::JustEngagedWith(Unit* who)
{
    ScriptedAI::JustEngagedWith(who);
}

void CustomAI::JustExitedCombat()
{
    ScriptedAI::JustExitedCombat();

    ReleaseFocus();
}

void CustomAI::JustDied(Unit* killer)
{
    ReleaseFocus();

    summons.DespawnAll();
    scheduler.CancelAll();

    ScriptedAI::JustDied(killer);
}

void CustomAI::UpdateAI(uint32 diff)
{
    ScriptedAI::UpdateAI(diff);

    scheduler.Update(diff, [this]
    {
        if (UpdateVictim())
        {
            if (type != AI_Type::Distance)
                DoMeleeAttackIfReady();
        }
    });
}

bool CustomAI::CanAIAttack(Unit const* who) const
{
    return who->IsAlive() && me->IsValidAttackTarget(who)
        && !who->HasBreakableByDamageCrowdControlAura()
        && ScriptedAI::CanAIAttack(who);
}
