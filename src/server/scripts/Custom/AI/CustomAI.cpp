#include "CustomAI.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"

CustomAI::CustomAI(Creature* creature, AI_Type type) : ScriptedAI(creature),
    type(type), summons(creature)
{
    Initialize();
}

void CustomAI::Initialize()
{
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
            return !me->HasUnitState(UNIT_STATE_FLEEING)
                || !me->HasUnitState(UNIT_STATE_FLEEING_MOVE);
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

void CustomAI::SpellHit(WorldObject* caster, SpellInfo const* spellInfo)
{
    if (type == AI_Type::Distance)
    {
        if (Unit* unit = caster->ToUnit())
        {
            if (spellInfo->HasEffect(SPELL_EFFECT_INTERRUPT_CAST))
            {
                int32 duration = spellInfo->GetDuration() / IN_MILLISECONDS;
                DoStartMovement(unit);
                scheduler.Schedule(Seconds(duration), [unit, this](TaskContext /*context*/)
                {
                    DoStartMovement(unit, GetDistance());
                });
            }
        }
    }

    ScriptedAI::SpellHit(caster, spellInfo);
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

    if (me->Attack(who, true))
    {
        if (type == AI_Type::None)
            return;

        if (type != AI_Type::Distance)
            DoStartMovement(who);
        else
            DoStartMovement(who, GetDistance());

        SetCombatMovement(true);
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
        && !who->HasAuraType(SPELL_AURA_EFFECT_IMMUNITY)
        && !who->HasAuraType(SPELL_AURA_STATE_IMMUNITY)
        && !who->HasAuraType(SPELL_AURA_SCHOOL_IMMUNITY)
        && !who->HasAuraType(SPELL_AURA_DAMAGE_IMMUNITY)
        && !who->HasAuraType(SPELL_AURA_DISPEL_IMMUNITY)
        && !who->HasAuraType(SPELL_AURA_MOD_FEAR_2)
        && !who->HasBreakableByDamageCrowdControlAura(me)
        && ScriptedAI::CanAIAttack(who);
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
