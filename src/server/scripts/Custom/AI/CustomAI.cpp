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
    switch (type)
    {
        case AI_Type::Distance:
        case AI_Type::NoMovement:
            scheduler.SetValidator([this]
            {
                return !me->HasUnitState(UNIT_STATE_CASTING)
                    || !me->HasUnitState(UNIT_STATE_FLEEING)
                    || !me->HasUnitState(UNIT_STATE_FLEEING_MOVE);
            });
            break;
        default:
            scheduler.SetValidator([this]
            {
                return !me->HasUnitState(UNIT_STATE_FLEEING)
                    || !me->HasUnitState(UNIT_STATE_FLEEING_MOVE);
            });
            break;
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
                    if (unit && unit->IsAlive())
                    {
                        DoStartMovement(unit, GetDistance());
                    }
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
        switch (type)
        {
            case AI_Type::Melee:
                DoStartMovement(who);
                break;
            case AI_Type::Distance:
                DoStartMovement(who, GetDistance());
                break;
            case AI_Type::NoMovement:
                SetCombatMovement(false);
                return;
            case AI_Type::None:
            default:
                return;
        }

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
            switch (type)
            {
                case AI_Type::None:
                case AI_Type::Melee:
                    DoMeleeAttackIfReady();
                    break;
                case AI_Type::Distance:
                case AI_Type::NoMovement:
                default:
                    break;
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
