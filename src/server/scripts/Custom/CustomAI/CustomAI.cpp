#include "CustomAI.h"
#include "CellImpl.h"
#include "Containers.h"
#include "GridNotifiers.h"
#include "MotionMaster.h"

CustomAI::CustomAI(Creature* creature, AI_Type type) : ScriptedAI(creature),
	type(type), summons(creature), canCombatMove(true), damageReduction(false),
	textOnCooldown(false), randomMovements(false), backpedaling(false), circleClockwise(roll_chance(50)),
	encircleReactOnCooldown(false), circleAngle(0.f),
	fakeParty(creature), linkedPlayer(nullptr)
{
	if (type == AI_Type::Distance)
	{
		SetCanRandomMovement(true);
	}

	Initialize();
}

CustomAI::CustomAI(Creature* creature, bool damageReduction, AI_Type type) : ScriptedAI(creature),
	type(type), summons(creature), canCombatMove(true), damageReduction(damageReduction),
	textOnCooldown(false), randomMovements(false), backpedaling(false), circleClockwise(roll_chance(50)),
	encircleReactOnCooldown(false), circleAngle(0.f),
	fakeParty(creature), linkedPlayer(nullptr)
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
	circleClockwise = roll_chance(50);
	encircleReactOnCooldown = false;

	scheduler.SetValidator([this]
	{
		return !me->HasBreakableByDamageCrowdControlAura()
			&& !me->HasAuraType(SPELL_AURA_MOD_FEAR_2)
			&& !me->HasUnitState(UNIT_STATE_CASTING)
			&& !me->HasUnitState(UNIT_STATE_FLEEING)
			&& !me->HasUnitState(UNIT_STATE_FLEEING_MOVE);
	});
}

void CustomAI::JustSummoned(Creature* summon)
{
	summons.Summon(summon);
}

void CustomAI::SummonedCreatureDespawn(Creature* summon)
{
	summons.Despawn(summon);
}

void CustomAI::SummonedCreatureDies(Creature* summon, Unit* killer)
{
	summons.Despawn(summon);
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
			me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);

			scheduler.Schedule(5s, [this](TaskContext /*context*/)
			{
				me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_INTERRUPT_CAST, false);
				me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_STUN, false);
				me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, false);

				interruptCounter = 0;
			});
		}
	}
}

void CustomAI::EnterEvadeMode(EvadeReason why)
{
	ScriptedAI::EnterEvadeMode(why);

	if (me->GetWaypointPathId() != 0)
	{
		me->ResumeMovement();
	}
}

void CustomAI::Reset()
{
	Initialize();

	me->RemoveAllAreaTriggers();

	summons.DespawnAll();
	scheduler.CancelAll();

	// Si un joueur était lié, on détruit le frame avant de reset
	if (linkedPlayer)
	{
		fakeParty.DestroyFakeParty(linkedPlayer);
		linkedPlayer = nullptr;
	}
}

void CustomAI::AttackStart(Unit* who)
{
	if (!who)
		return;

    if (me->HasUnitState(UNIT_STATE_DISTRACTED))
    {
        me->ClearUnitState(UNIT_STATE_DISTRACTED);
        me->GetMotionMaster()->Clear();
    }

	switch (type)
	{
		case AI_Type::None:
		case AI_Type::Stay:
		{
			// Pas d'auto-attaque melee, pas de déplacement
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
			// Pas d'auto-attaque melee, mais on suit a distance
			if (me->Attack(who, false))
			{
				me->SetCanMelee(false, true);
				me->SetSheath(SHEATH_STATE_UNARMED);
				me->GetMotionMaster()->MoveChase(who, GetDistance());
				ScheduleRandomMovements();
				ScheduleEncircleCheck();
			}
			break;
		}
		case AI_Type::Hybrid:
		{
			// Pas d'auto-attaque melee, mais on suit a distance
			if (me->Attack(who, false))
			{
				me->SetCanMelee(false, true);
				me->SetSheath(SHEATH_STATE_RANGED);
				me->GetMotionMaster()->MoveChase(who, GetDistance());
				ScheduleEncircleCheck();
			}
			break;
		}
        case AI_Type::Melee:
        {
            if (me->Attack(who, true))
            {
                me->GetMotionMaster()->MoveChase(who);

                ObjectGuid const targetGuid = who->GetGUID();
                scheduler.Schedule(500ms, [this, targetGuid](TaskContext ctx)
                {
                    Unit* target = ObjectAccessor::GetUnit(*me, targetGuid);
                    if (!target || !target->IsAlive() || !me->IsInCombat())
                        return;

                    std::vector<Creature*> meleeBots;
                    for (Unit* a : target->getAttackers())
                    {
                        Creature* c = a->ToCreature();
                        if (!c || !c->IsAlive())
                            continue;
                        CustomAI* cai = dynamic_cast<CustomAI*>(c->AI());
                        if (!cai || cai->type != AI_Type::Melee)
                            continue;
                        meleeBots.push_back(c);
                    }

                    if (std::find(meleeBots.begin(), meleeBots.end(), me) == meleeBots.end())
                        meleeBots.push_back(me);

                    std::sort(meleeBots.begin(), meleeBots.end(), [](Creature* lhs, Creature* rhs)
                    {
                        return lhs->GetGUID() < rhs->GetGUID();
                    });

                    uint32 const total = uint32(meleeBots.size());
                    float const meleeRange = me->GetCombatReach();

                    // Arc total centre sur M_PI (face au joueur, relatif a son orientation)
                    float const maxArc = 2.f * float(M_PI) / 3.f;
                    float const totalArc = (total == 1)
                        ? 0.f
                        : std::min(maxArc, float(M_PI) / 4.f * float(total - 1));
                    float const tolerance = (total == 1)
                        ? float(M_PI) / 2.f
                        : totalArc / float(total - 1) / 2.f;

                    auto it = std::find(meleeBots.begin(), meleeBots.end(), me);
                    uint32 const myIndex = uint32(std::distance(meleeBots.begin(), it));

                    // 0 = face au joueur (dans sa direction de regard)
                    // M_PI = dans son dos
                    float const t = (total == 1) ? 0.f : (float(myIndex) / float(total - 1) - 0.5f);
                    float const angle = 0.f + t * totalArc;

                    me->GetMotionMaster()->MoveChase(target, meleeRange, ChaseAngle(angle, tolerance));

                    ctx.Repeat(500ms);
                });
            }
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

	StopFakeParty();
}

void CustomAI::UpdateAI(uint32 diff)
{
	// Mise à jour périodique du party frame (santé, mana, position)
	if (fakeParty.IsActive())
	{
		// Vérifier que le joueur est toujours valide et en range
		if (!linkedPlayer
			|| !linkedPlayer->IsInWorld()
			|| !linkedPlayer->IsWithinDistInMap(me, 100.0f))
		{
			StopFakeParty();
		}
		else
		{
			fakeParty.Update(diff, linkedPlayer);
		}
	}

	UpdateVictim();

	scheduler.Update(diff);
}

bool CustomAI::CanAIAttack(Unit const* who) const
{
	return who->IsAlive() && me->IsValidAttackTarget(who)
		&& !who->HasAuraType(SPELL_AURA_MOD_FEAR_2)
		&& !who->HasBreakableByDamageCrowdControlAura()
		&& ScriptedAI::CanAIAttack(who);
}

void CustomAI::CastStop()
{
	for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; i++)
		me->InterruptSpell(CurrentSpellTypes(i), false);
}

void CustomAI::CastStop(const std::unordered_set<uint32>& exceptions)
{
	for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
	{
		if (Spell const* spell = me->GetCurrentSpell(i))
		{
			if (exceptions.find(spell->m_spellInfo->Id) != exceptions.end())
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

uint32 CustomAI::FriendsInFront(float range, uint8 pct)
{
	std::list<Unit*> list;
	FriendlyInFront check(me, range, pct);
	Trinity::UnitListSearcher<FriendlyInFront> searcher(me, list, check);
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

void CustomAI::StartFakeParty(Player* player)
{
	if (!player || fakeParty.IsActive())
		return;

	linkedPlayer = player;
	fakeParty.SendFakePartyUpdate(player);
	fakeParty.SendFakePartyMemberState(player);
}

void CustomAI::StopFakeParty()
{
	if (!linkedPlayer || !fakeParty.IsActive())
		return;

	fakeParty.DestroyFakeParty(linkedPlayer);
	linkedPlayer = nullptr;
}

void CustomAI::TalkInCombat(uint8 textId, Seconds cooldown)
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

void CustomAI::EnterBackped(Unit* victim)
{
	if (!backpedaling)
	{
		backpedaling = true;
		OnBackpedStart(victim);
	}
	else
	{
		OnBackpedTick(victim);
	}
}

void CustomAI::ExitBackped(Unit* victim)
{
	if (backpedaling)
	{
		backpedaling = false;
		OnBackpedEnd(victim);
	}
}

void CustomAI::MovementInform(uint32 type, uint32 id)
{
	// EFFECT_MOTION_TYPE = sauts (MoveJump)
	// POINT_MOTION_TYPE = MovePoint et MoveBackward (BackwardMovementGenerator)
	if (type != EFFECT_MOTION_TYPE && type != POINT_MOTION_TYPE)
		return;

	// Vérifie que l'unité à bien une cible
	Unit* victim = me->GetVictim();
	if (!victim)
		return;

	switch (id)
	{
		case Backped:
		{
			// Fin propre de la phase de recul : on notifie les classes filles
			// puis on reprend le chase
			ExitBackped(victim);
			me->GetMotionMaster()->MoveChase(victim, GetDistance());
			break;
		}
		case Jump:
		{
			// Apres un saut, on reprend toujours le chase
			me->GetMotionMaster()->MoveChase(victim, GetDistance());
			break;
		}
		case Move:
		{
			// Petite chance d'enchainer un saut apres un deplacement (effet de vie)
			if (roll_chance(30))
			{
				MovementFacingTarget facing;
				facing = victim;

				me->GetMotionMaster()->MoveJump(Jump, GetRandomJump(), me->GetSpeed(MOVE_RUN),
					JUMP_HEIGHT, JUMP_HEIGHT,
					facing, true);
			}
			else
			{
				me->GetMotionMaster()->MoveChase(victim, GetDistance());
			}
			break;
		}
	}
}

std::list<Unit*> CustomAI::DoFindMissingBuff(uint32 spellId)
{
	const SpellInfo* info = sSpellMgr->AssertSpellInfo(spellId, DIFFICULTY_NONE);

	SpellRange range = info->GetEffect(EFFECT_0).CalcRadius();

	std::list<Unit*> list;
	FriendlyMissingBuff u_check(me, spellId, range.Max);
	Trinity::UnitListSearcher<FriendlyMissingBuff> searcher(me, list, u_check);
	Cell::VisitAllObjects(me, searcher, range.Max);
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
	if (!CanRandomMovement())
		return;

	scheduler.Schedule(5s, 10s, RandomMovement, [this](TaskContext context)
	{
		Unit* victim = me->GetVictim();

		// Conditions bloquantes : on reporte
		if (!me->IsInCombat() || !victim
			|| me->HasUnitState(UNIT_STATE_NOT_MOVE | UNIT_STATE_CONTROLLED | UNIT_STATE_JUMPING | UNIT_STATE_CHARGING)
			|| me->HasBreakableByDamageCrowdControlAura()
			|| me->IsInWater() || me->IsUnderWater()
			|| me->HasInvisibilityAura() || me->HasStealthAura()
			|| me->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
		{
			if (victim)
				ExitBackped(victim);
			context.Repeat(6s, 8s);
			return;
		}

		// Trop proche : on recule TANT QUE la cible est trop proche.
		// On alterne aleatoirement entre walk back et jump back, toujours face a la cible
		if (victim->IsWithinCombatRange(me, 8.f))
		{
			// On calcule la position de recul AVANT d'interrompre le sort
			// Si un mur bloque derriere, on ne coupe pas le cast inutilement
			bool useJump = roll_chance(30);
			Position backPos = useJump ? GetRandomBackJump() : GetRandomBackStep(10.f);
			float movedDist = me->GetExactDist2d(backPos);

			// Si on ne peut pas reculer assez (mur derriere), on garde le cast et on reessaye plus tard
			if (movedDist < 1.f)
			{
				context.Repeat(500ms);
				return;
			}

			// On peut reculer : on interrompt le sort et on recule
			CastStop();

			EnterBackped(victim);

			if (useJump)
			{
				MovementFacingTarget facing;
				facing = victim;

				// Petit hop arriere face a la cible
				me->GetMotionMaster()->MoveJump(Backped, backPos, JUMP_SPEED,
					JUMP_BACK_HEIGHT, JUMP_BACK_HEIGHT,
					facing, true);
			}
			else
			{
				me->GetMotionMaster()->MoveBackward(Backped, backPos, victim);
			}

			// Repeat court : on revient verifier rapidement, et on reculera encore si toujours trop proche
			context.Repeat(500ms);
			return;
		}

		// Hop lateral : 60% (laisse le cast finir, sinon on couperait les sorts)
		if (roll_chance(60))
		{
			if (me->HasUnitState(UNIT_STATE_CASTING))
			{
				context.Repeat(1s, 2s);
				return;
			}

			MovementFacingTarget facing;
			facing = victim;

			ExitBackped(victim);
			me->GetMotionMaster()->MoveJump(Jump, GetRandomJump(), JUMP_SPEED,
				JUMP_HEIGHT, JUMP_HEIGHT,
				facing, true);

			context.Repeat(2s, 8s);
			return;
		}

		// Mouvement : circle kite
		ExitBackped(victim);
		me->GetMotionMaster()->MovePoint(Move, GetRandomMovementsPosition());
		context.Repeat(8s, 16s);
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
	if (roll_chance(15))
		circleClockwise = !circleClockwise;

	// Appliquer le circle kiting : base + offset cumulatif
	float finalAngle = baseAngle + circleAngle;

	// Se positionner a GetDistance() de la cible
	// MovePositionToFirstCollision ajoute GetOrientation() en interne, on compense
	Position pos = victim->GetPosition();
	victim->MovePositionToFirstCollision(pos, GetDistance(), finalAngle - victim->GetOrientation());

	return pos;
}

Position CustomAI::GetRandomJump()
{
	// Petit hop lateral depuis la position actuelle (strafe-jump)
	Unit* victim = me->GetVictim();
	float perpAngle = victim->GetAbsoluteAngle(me)
		+ (roll_chance(50) ? float(M_PI / 2) : -float(M_PI / 2));

	// Compensation de l'orientation interne ajoutee par MovePositionToFirstCollision
	Position pos = me->GetPosition();
	me->MovePositionToFirstCollision(pos, JUMP_DISTANCE, perpAngle - me->GetOrientation());

	return pos;
}

Position CustomAI::GetRandomBackJump()
{
	// Petit hop arriere depuis la position actuelle, strictement aligne sur l'axe victim -> me
	Unit* victim = me->GetVictim();
	float axisAngle = victim->GetAbsoluteAngle(me);

	// Compensation de l'orientation interne ajoutee par MovePositionToFirstCollision
	Position pos = me->GetPosition();
	me->MovePositionToFirstCollision(pos, JUMP_BACK_DISTANCE, axisAngle - me->GetOrientation());

	return pos;
}

Position CustomAI::GetRandomBackStep(float distance)
{
	// Recul strictement aligne sur l'axe victim -> me
	Unit* victim = me->GetVictim();
	float axisAngle = victim->GetAbsoluteAngle(me);
	float currentDist = me->GetExactDist2d(victim);
	float backDist = currentDist + distance;

	// On part de la victime et on projete sur l'axe a une distance plus grande
	// Compensation de l'orientation interne ajoutee par MovePositionToFirstCollision
	Position pos = victim->GetPosition();
	victim->MovePositionToFirstCollision(pos, backDist, axisAngle - victim->GetOrientation());

	return pos;
}

bool CustomAI::IsEncircled(float minClearArc) const
{
	float radius = GetEncircleRadius();
	uint32 minEnemies = GetEncircleMinEnemies();

	std::vector<float> angles;
	for (ThreatReference const* ref : me->GetThreatManager().GetUnsortedThreatList())
	{
		Unit* enemy = ref->GetVictim();
		if (!enemy || !enemy->IsAlive() || !me->IsWithinDist(enemy, radius))
			continue;
		angles.push_back(me->GetAbsoluteAngle(enemy));
	}

	if (angles.size() < minEnemies)
		return false;

	std::sort(angles.begin(), angles.end());

	// Plus grand gap angulaire entre deux ennemis consecutifs (avec wrap-around).
	float largestGap = (angles.front() + 2.f * float(M_PI)) - angles.back();
	for (size_t i = 0; i + 1 < angles.size(); ++i)
	{
		float gap = angles[i + 1] - angles[i];
		if (gap > largestGap)
			largestGap = gap;
	}

	// Aucun arc libre suffisamment large -> encerclee.
	return largestGap < minClearArc;
}

Position CustomAI::GetBestEscapePosition(float distance) const
{
	float radius = GetEncircleRadius();

	std::vector<float> angles;
	for (ThreatReference const* ref : me->GetThreatManager().GetUnsortedThreatList())
	{
		Unit* enemy = ref->GetVictim();
		if (!enemy || !enemy->IsAlive() || !me->IsWithinDist(enemy, radius))
			continue;
		angles.push_back(me->GetAbsoluteAngle(enemy));
	}

	if (angles.empty())
		return me->GetPosition();

	std::sort(angles.begin(), angles.end());

	// On cherche le plus grand arc vide et son angle de depart (pour viser son centre).
	float bestGap = (angles.front() + 2.f * float(M_PI)) - angles.back();
	float bestStart = angles.back();
	for (size_t i = 0; i + 1 < angles.size(); ++i)
	{
		float gap = angles[i + 1] - angles[i];
		if (gap > bestGap)
		{
			bestGap = gap;
			bestStart = angles[i];
		}
	}

	float fleeAngle = bestStart + bestGap / 2.f;

	Position pos = me->GetPosition();
	me->MovePositionToFirstCollision(pos, distance, fleeAngle - me->GetOrientation());
	return pos;
}

void CustomAI::PanicFlee(float distance)
{
	Unit* victim = me->GetVictim();
	if (!victim)
		return;

	Position fleePos = GetBestEscapePosition(distance);
	float moved = me->GetExactDist2d(fleePos);

	// Arc de fuite bloque par un mur ou inexistant : dernier recours.
	if (moved < 3.f)
	{
		OnCornered(victim);
		return;
	}

	CastStop();
	ExitBackped(victim);
	me->GetMotionMaster()->MovePoint(Move, fleePos);

	// Suspend le circle-kite pour ne pas etre ramene vers la cible avant d'arriver.
	scheduler.DelayGroup(RandomMovement, 4s);
}

void CustomAI::ScheduleEncircleCheck()
{
	scheduler.Schedule(2s, Encircle, [this](TaskContext context)
	{
		Unit* victim = me->GetVictim();

		// Conditions bloquantes : on reporte sans toucher au cooldown.
		if (!victim || !me->IsInCombat()
			|| me->HasUnitState(UNIT_STATE_NOT_MOVE | UNIT_STATE_CONTROLLED | UNIT_STATE_JUMPING | UNIT_STATE_CHARGING)
			|| me->HasBreakableByDamageCrowdControlAura()
			|| encircleReactOnCooldown)
		{
			context.Repeat(2s);
			return;
		}

		if (!IsEncircled())
		{
			context.Repeat(1500ms);
			return;
		}

		// On bloque les nouveaux declenchements pendant que la sous-classe
		// (ou le fallback) gere la situation, et on libere le circle-kite.
		encircleReactOnCooldown = true;
		scheduler.DelayGroup(RandomMovement, 3s);

		bool handled = OnEncircled(victim);
		Milliseconds cooldown = handled ? 12s : 6s;

		if (!handled)
			PanicFlee();

		scheduler.Schedule(cooldown, TeleportSettle, [this](TaskContext /*ctx*/)
		{
			encircleReactOnCooldown = false;
		});

		context.Repeat(2s);
	});
}

void CustomAI::NotifyTeleported(Milliseconds settleDuration)
{
	// Reset eventuel d'un settle precedent encore en cours.
	scheduler.CancelGroup(TeleportSettle);

	// Repousse le circle-kite : sans ca, son MovePoint replacerait l'unite
	// a GetDistance() de la victime, annulant le gain de distance du teleport.
	scheduler.DelayGroup(RandomMovement, settleDuration);

	if (!canCombatMove)
		return;

	if (type != AI_Type::Distance && type != AI_Type::Hybrid)
		return;

	// Le teleport s'execute via un MoveJump :
	// on attend l'atterrissage avant de mesurer la distance reelle, sinon on
	// ecraserait le saut en cours par un MoveChase.
	scheduler.Schedule(500ms, TeleportSettle, [this, settleDuration](TaskContext /*context*/)
	{
		Unit* victim = me->GetVictim();
		if (!victim)
			return;

		// Re-ancre le chase sur la distance courante : tant qu'on est plus
		// loin que GetDistance(), MoveChase ne tirera plus l'unite vers la
		// cible. On garde quand meme GetDistance() comme plancher.
		float keepDist = std::max(GetDistance(), me->GetExactDist2d(victim));
		me->GetMotionMaster()->MoveChase(victim, keepDist);

		// Restaure la distance de poursuite par defaut a la fin de la fenetre.
		scheduler.Schedule(settleDuration, TeleportSettle, [this](TaskContext /*ctx*/)
		{
			if (Unit* v = me->GetVictim())
				me->GetMotionMaster()->MoveChase(v, GetDistance());
		});
	});
}

