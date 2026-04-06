#include "CustomAI.h"
#include "CellImpl.h"
#include "Containers.h"
#include "GridNotifiers.h"

CustomAI::CustomAI(Creature* creature, AI_Type type) : ScriptedAI(creature),
	type(type), summons(creature), canCombatMove(true), damageReduction(false),
    textOnCooldown(false), randomMovements(false), circleClockwise(roll_chance_i(50)), circleAngle(0.f)
{
    if (type == AI_Type::Distance)
    {
        SetCanRandomMovement(true);
    }

	Initialize();
}

CustomAI::CustomAI(Creature* creature, bool damageReduction, AI_Type type) : ScriptedAI(creature),
	type(type), summons(creature), canCombatMove(true), damageReduction(damageReduction),
    textOnCooldown(false), randomMovements(false), circleClockwise(roll_chance_i(50)), circleAngle(0.f)
{
    if (type == AI_Type::Distance)
    {
        SetCanRandomMovement(true);
    }

	Initialize();
}

void CustomAI::Initialize()
{
	interruptCounter = 0;
	circleAngle = 0.f;
	circleClockwise = roll_chance_i(50);

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

		if (type == AI_Type::Distance)
		{
			if (Unit* unitCaster = caster->ToUnit())
			{
                int32 duration = spellInfo->CalcDuration();
				float angle = unitCaster->GetAbsoluteAngle(me) ;
				Position pos = me->GetPosition();

				me->MovePositionToFirstCollision(pos, GetDistance(), angle);
				me->GetMotionMaster()->MovePoint(0, pos);

				scheduler.Schedule(Milliseconds(duration), [this](TaskContext /*context*/)
				{
					if (Unit* victim = me->GetVictim())
						me->GetMotionMaster()->MoveChase(victim, GetDistance());
				});
			}
		}

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

	ScriptedAI::Reset();
}

void CustomAI::AttackStart(Unit* who)
{
    if (!who)
        return;

    switch (type)
    {
        case AI_Type::Stay:
        {
            // Pas d'auto-attaque m�l�e, pas de d�placement
            if (me->Attack(who, false))
            {
                me->SetCanMelee(false, false);
                me->SetSheath(SHEATH_STATE_UNARMED);
                SetCombatMovement(false);
            }
            break;
        }
        case AI_Type::Distance:
        {
            // Pas d'auto-attaque m�l�e, mais on suit � distance
            if (me->Attack(who, false))
            {
                me->SetCanMelee(false, true);
                me->SetSheath(SHEATH_STATE_UNARMED);
                me->GetMotionMaster()->MoveChase(who, GetDistance());
                ScheduleRandomMovements();
            }
            break;
        }
        case AI_Type::Hybrid:
        {
            // Pas d'auto-attaque m�l�e, mais on suit � distance
            if (me->Attack(who, false))
            {
                me->SetCanMelee(false, true);
                me->SetSheath(SHEATH_STATE_RANGED);
                me->GetMotionMaster()->MoveChase(who, GetDistance());
            }
            break;
        }
        case AI_Type::Melee:
        {
            // Auto-attaque m�l�e + poursuite rapproch�e
            if (me->Attack(who, true))
                me->GetMotionMaster()->MoveChase(who);
            break;
        }
        default:
            break;
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

void CustomAI::MovementInform(uint32 type, uint32 id)
{
    // Reprendre le chase apres le déplacement
    if (type == EFFECT_MOTION_TYPE)
    {
        switch (id)
        {
            case Jump:
            {
                if (Unit* victim = me->GetVictim())
                    me->GetMotionMaster()->MoveChase(victim, GetDistance());
                break;
            }
            case Move:
            {
                if (roll_chance_i(60))
                {
                    Position pos = GetRandomJump();
                    me->GetMotionMaster()->MoveJump(Jump, pos, me->GetSpeed(MOVE_RUN), 1.f);
                }
                else
                {
                    if (Unit* victim = me->GetVictim())
                        me->GetMotionMaster()->MoveChase(victim, GetDistance());
                }
                break;
            }
        }
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

void CustomAI::ScheduleRandomMovements()
{
    // Si les mouvements aléatoires sont désactivés
    if (!CanRandomMovement())
        return;

	scheduler.Schedule(5s, 10s, [this](TaskContext context)
	{
		if (!me->IsInCombat() || !me->GetVictim()
			|| me->HasBreakableByDamageCrowdControlAura()
            || me->HasRootAura() || me->IsFeared() || me->IsPolymorphed() || me->IsFrozen()
            || me->IsInWater() || me->IsUnderWater()
            || me->HasInvisibilityAura() || me->HasStealthAura()
			|| me->HasUnitState(UNIT_STATE_FLEEING)
			|| me->HasUnitState(UNIT_STATE_FLEEING_MOVE)
			|| me->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
		{
			context.Repeat(3s, 6s);
			return;
		}

		// Stutter-step : interrompre le cast en cours (hors canalisations) avant de bouger
		if (me->HasUnitState(UNIT_STATE_CASTING))
			CastStop();

		Unit* victim = me->GetVictim();
		bool tooClose = victim->IsWithinCombatRange(me, GetDistance());

		if (tooClose)
		{
			// Kite : s'eloigner immediatement au max range
			Position pos = GetRandomMovementsPosition();
			me->GetMotionMaster()->MovePoint(Move, pos);
			context.Repeat(3s, 5s);
		}
		else if (roll_chance_i(60))
		{
			// Petit hop lateral
			Position pos = GetRandomJump();
			me->GetMotionMaster()->MoveJump(Jump, pos, me->GetSpeed(MOVE_RUN), 1.f);
			context.Repeat(2s, 8s);
		}
		else
		{
			// Repositionnement au max range
			Position pos = GetRandomMovementsPosition();
			me->GetMotionMaster()->MovePoint(Move, pos);
			context.Repeat(8s, 16s);
		}
	});
}

Position CustomAI::GetRandomMovementsPosition()
{
	Unit* victim = me->GetVictim();

	// Direction actuelle (victim -> me)
	float baseAngle = victim->GetAbsoluteAngle(me);

	// Incrementer l'angle de circle kiting (40-70° par step)
	float step = frand(float(M_PI / 4.5f), float(M_PI / 2.5f));
	circleAngle += circleClockwise ? step : -step;

	// Changer de direction aleatoirement (~15% de chance)
	if (roll_chance_i(15))
		circleClockwise = !circleClockwise;

	// Appliquer le circle kiting : base + offset cumulatif
	float finalAngle = baseAngle + circleAngle;

	// Se positionner a GetDistance() de la cible
	Position pos = victim->GetPosition();
	victim->MovePositionToFirstCollision(pos, GetDistance(), finalAngle);

	return pos;
}

Position CustomAI::GetRandomJump()
{
    // Petit hop lateral depuis la position actuelle (strafe-jump)
    Unit* victim = me->GetVictim();
    float perpAngle = victim->GetAbsoluteAngle(me)
        + (roll_chance_f(50) ? float(M_PI / 2) : -float(M_PI / 2));

    Position pos = me->GetPosition();
    me->MovePositionToFirstCollision(pos, 5.f, perpAngle);

    return pos;
}
