#include "CustomAI.h"
#include "CellImpl.h"
#include "Containers.h"
#include "GridNotifiers.h"

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
    if (caster->GetGUID() != me->GetGUID()
        && (spellInfo->HasEffect(SPELL_EFFECT_INTERRUPT_CAST) || spellInfo->HasEffect(SPELL_EFFECT_KNOCK_BACK)))
    {
        wasInterrupted = true;

        if (type != AI_Type::Stay)
        {
            me->SetCanMelee(true);
            SetCombatMove(true, 0.0f, false, true);
        }

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

                me->SetCanMelee(false);

                SetCombatMove(false, GetDistance());
            });
        }
    }
}

void CustomAI::EnterEvadeMode(EvadeReason why)
{
    if (me->GetWaypointPath() != 0)
    {
        me->ResumeMovement();
    }

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

    if (type == AI_Type::Stay && me->Attack(who, false))
    {
        me->SetCanMelee(false);
        me->SetSheath(SHEATH_STATE_UNARMED);
        SetCombatMovement(false);
        return;
    }

    if (me->Attack(who, true))
    {
        if (me->GetWaypointPath() != 0)
        {
            me->GetMotionMaster()->Clear(MOTION_PRIORITY_NORMAL);
        }

        me->PauseMovement();

        switch (type)
        {
            case AI_Type::Distance:
                me->SetCanMelee(true);
                me->SetSheath(SHEATH_STATE_UNARMED);
                break;
            case AI_Type::Melee:
                me->SetCanMelee(true);
                me->GetMotionMaster()->MoveChase(who);
                break;
            case AI_Type::Hybrid:
                me->SetCanMelee(true);
                me->GetMotionMaster()->MoveChase(who);
                break;
            default:
                me->SetCanMelee(false);
                me->SetSheath(SHEATH_STATE_UNARMED);
                break;
        }
    }
}

void CustomAI::JustDied(Unit* killer)
{
	summons.DespawnAll();
	scheduler.CancelAll();

    me->RemoveAllDynObjects();
    me->RemoveAllAreaTriggers();

	ScriptedAI::JustDied(killer);
}

void CustomAI::UpdateAI(uint32 diff)
{
	scheduler.Update(diff, [this]
	{
		if (UpdateVictim())
		{
            DoMeleeAttackIfReady();

            if (type == AI_Type::Hybrid || type == AI_Type::Distance)
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

void CustomAI::SetCombatMove(bool on, float distance, bool stopMoving, bool force)
{
    if (distance) distance = me->GetCombatReach();

    if (me->IsEngaged())
    {
        if (force)
        {
            me->ResumeMovement();

            me->GetMotionMaster()->Clear();
            me->GetMotionMaster()->Remove(CHASE_MOTION_TYPE);
            me->GetMotionMaster()->MoveChase(me->GetVictim(), distance);
        }
        else
        {
            if (canCombatMove == on)
                return;

            canCombatMove = on;

            me->GetMotionMaster()->Clear();
            me->GetMotionMaster()->Remove(CHASE_MOTION_TYPE);

            if (on)
            {
                me->GetMotionMaster()->MoveChase(me->GetVictim(), distance);
            }
            else
            {
                if (stopMoving)
                    me->StopMoving();
            }
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

bool CustomAI::ShouldTakeDamage()
{
    return me->GetHealthPct() > me->GetSparringHealthPct();
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
