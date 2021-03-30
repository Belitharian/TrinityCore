#include "ScriptMgr.h"
#include "Unit.h"
#include "SpellAuraEffects.h"
#include "SpellMgr.h"
#include "ScriptedCreature.h"
#include "CustomAI.h"

enum Spells
{
	SPELL_FIREBALL              = 100003,
	SPELL_FIREBLAST             = 100004,
	SPELL_PYROBLAST             = 100005,
	SPELL_LIVING_BOMB           = 100080,
	SPELL_IGNITE                = 100092,
	SPELL_DRAGON_BREATH         = 37289,

	SPELL_FROSTBOLT             = 100006,
	SPELL_ICE_LANCE             = 100007,
	SPELL_ICE_BLOCK             = 100008,
	SPELL_HYPOTHERMIA           = 41425,
	SPELL_FROT_NOVA             = 71320,
	SPELL_ICE_BARRIER           = 100068,
	SPELL_FROSTBITE             = 12494,

	SPELL_ARCANE_BARRAGE        = 100009,
	SPELL_ARCANE_BARRAGES       = 100078,
	SPELL_ARCANE_BLAST          = 100010,
	AURA_ARCANE_BLAST           = 36032,
	SPELL_ARCANE_PROJECTILE     = 100012,
	SPELL_ARCANE_EXPLOSION      = 100011,
	SPELL_EVOCATION             = 100014,
	SPELL_PRISMATIC_BARRIER     = 100069,
	SPELL_ARCANE_POWER          = 100081,
	SPELL_MIRROR_IMAGE          = 100105,

	SPELL_COUNTERSPELL          = 15122,
	SPELL_BLINK                 = 57869,
	SPELL_POLYMORPH             = 61721,

	SPELL_CLONE_ME              = 45204,
	SPELL_MASTERS_THREAT_LIST   = 58838,
    SPELL_POWER_BALL_VISUAL     = 54139
};

enum Misc
{
	// NPCs
	NPC_RHONIN                  = 100005,
	NPC_MIRROR_IMAGE            = 100085,

	// Zones
	ZONE_THERAMORE              = 726,

	// Phases
	PHASE_COMBAT                = 1,
	PHASE_BLINK_SEQUENCE
};

class npc_archmage_fire : public CreatureScript
{
	public:
	npc_archmage_fire() : CreatureScript("npc_archmage_fire") {}

	struct npc_archmage_fireAI : public CustomAI
	{
		npc_archmage_fireAI(Creature* creature) : CustomAI(creature), closeTarget(false)
		{
			SetCombatMovement(false);
		}

		void Reset() override
		{
			CustomAI::Reset();

			closeTarget = false;
		}

		void SpellHitTarget(WorldObject* target, SpellInfo const* spellInfo) override
		{
			if (spellInfo->Id == SPELL_FIREBALL || spellInfo->Id == SPELL_FIREBLAST || spellInfo->Id == SPELL_PYROBLAST)
			{
				Unit* victim = target->ToUnit();
				if (victim && !victim->HasAura(SPELL_IGNITE) && roll_chance_i(40))
					DoCast(victim, SPELL_IGNITE, true);
			}
		}

		void JustEngagedWith(Unit* /*who*/) override
		{
			scheduler
				.Schedule(5ms, PHASE_COMBAT, [this](TaskContext fireball)
				{
					DoCastVictim(SPELL_FIREBALL);
					fireball.Repeat(2s);
				})
				.Schedule(1s, 5s, PHASE_COMBAT, [this](TaskContext counterspell)
				{
					if (Unit* target = DoSelectCastingUnit(SPELL_COUNTERSPELL, 35.f))
					{
						me->InterruptNonMeleeSpells(true);
						DoCast(target, SPELL_COUNTERSPELL);
						counterspell.Repeat(25s, 40s);
					}
					else
					{
						counterspell.Repeat(1s);
					}
				})
				.Schedule(5s, 8s, PHASE_COMBAT, [this](TaskContext living_bomb)
				{
					DoCastVictim(SPELL_LIVING_BOMB);
					living_bomb.Repeat(20s, 28s);
				})
				.Schedule(8s, 10s, PHASE_COMBAT, [this](TaskContext fireblast)
				{
					if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
						DoCast(target, SPELL_FIREBLAST);
					fireblast.Repeat(14s, 22s);
				})
				.Schedule(12s, 18s, PHASE_COMBAT, [this](TaskContext pyroblast)
				{
					if (Unit* target = SelectTarget(SelectTargetMethod::MaxDistance, 0))
						DoCast(target, SPELL_PYROBLAST);
					pyroblast.Repeat(22s, 35s);
				});
		}

		void DamageTaken(Unit* attacker, uint32& damage) override
		{
			// Que pour la bataille de Theramore
			if (me->GetMapId() == 726)
			{
				if (attacker->GetTypeId() == TYPEID_PLAYER)
				{
					if (Player* player = attacker->ToPlayer())
					{
						if (player->IsGameMaster())
							return;
					}
				}

				if (damage >= me->GetMaxHealth())
					damage = 0;

				if (HealthBelowPct(20))
					damage = 0;
			}
		}

		void UpdateAI(uint32 diff) override
		{
			ScriptedAI::UpdateAI(diff);

            if (!UpdateVictim())
                return;

            if (me->HasBreakableByDamageCrowdControlAura())
                return;

			if (!closeTarget)
			{
				if (Unit* victim = me->GetVictim())
				{
					if (victim->IsWithinDist(me, 3.f, false))
					{
						closeTarget = true;

						scheduler.DelayGroup(PHASE_COMBAT, 2s);

						me->InterruptNonMeleeSpells(true);

						scheduler.Schedule(1s, PHASE_BLINK_SEQUENCE, [this](TaskContext context)
						{
							switch (context.GetRepeatCounter())
							{
								case 0:
									me->InterruptNonMeleeSpells(true);
									DoCastAOE(SPELL_DRAGON_BREATH, true);
									context.Repeat(3s);
									break;

								case 1:
									DoCastSelf(SPELL_BLINK, true);
									scheduler.CancelGroup(PHASE_BLINK_SEQUENCE);
									context.Repeat(500ms);
									break;

								case 2:
									closeTarget = false;
									break;
							}
						});
					}
				}
			}

			scheduler.Update(diff, [this]
			{
                DoMeleeAttackIfReady();
			});
		}

		private:
		bool closeTarget;
	};

	CreatureAI* GetAI(Creature* creature) const override
	{
		return new npc_archmage_fireAI(creature);
	}
};

class npc_archmage_arcanes : public CreatureScript
{
	public:
	npc_archmage_arcanes() : CreatureScript("npc_archmage_arcanes") {}

	struct npc_archmage_arcanesAI : public CustomAI
	{
		npc_archmage_arcanesAI(Creature* creature) : CustomAI(creature), closeTarget(false), summons(me)
		{
			SetCombatMovement(false);
		}

		void JustSummoned(Creature* summon) override
		{
			if (!me->IsInCombat())
				return;

			if (summon->GetEntry() == NPC_MIRROR_IMAGE)
			{
                summon->CastSpell(summon, SPELL_POWER_BALL_VISUAL);
				summon->Attack(me->GetVictim(), false);

                uint32 health = me->GetMaxHealth() / 1.2f;
                summon->SetHealth(health);
                summon->SetMaxHealth(health);

                summons.Summon(summon);
			}
		}

		void Reset() override
		{
			CustomAI::Reset();

			closeTarget = false;

			summons.DespawnAll();
		}

		void JustEngagedWith(Unit* /*who*/) override
		{
			if (me->GetEntry() != NPC_RHONIN)
			{
				DoCast(SPELL_PRISMATIC_BARRIER);

				scheduler
					.Schedule(30s, PHASE_COMBAT,[this](TaskContext prismatic_barrier)
					{
						if (!me->HasAura(SPELL_PRISMATIC_BARRIER))
						{
							DoCast(SPELL_PRISMATIC_BARRIER);
							prismatic_barrier.Repeat(30s);
						}
						else
						{
							prismatic_barrier.Repeat(1s);
						}
					})
					.Schedule(15s, PHASE_COMBAT, [this](TaskContext arcane_projectile)
					{
						me->InterruptNonMeleeSpells(true);
						DoCastVictim(SPELL_ARCANE_PROJECTILE);
						arcane_projectile.Repeat(28s, 40s);
					});
			}
			else
			{
				DoCast(SPELL_ARCANE_POWER);
			}

			scheduler
				.Schedule(5ms, PHASE_COMBAT, [this](TaskContext arcane_blast)
				{
					if (Aura* aura = me->GetAura(AURA_ARCANE_BLAST))
					{
						if (aura->GetStackAmount() < 4)
							DoCastVictim(SPELL_ARCANE_BLAST);
					}
					else
					{
						DoCastVictim(SPELL_ARCANE_BLAST);
					}

					arcane_blast.Repeat(500ms);
				})
				.Schedule(1s, 5s, PHASE_COMBAT, [this](TaskContext evocation)
				{
					float manaPct = me->GetPower(POWER_MANA) * 100 / me->GetMaxPower(POWER_MANA);
					if (manaPct <= 20)
					{
						me->InterruptNonMeleeSpells(true);
						if (me->GetEntry() == NPC_RHONIN)
						{
							CastSpellExtraArgs args;
							const SpellInfo* spellInfo = sSpellMgr->AssertSpellInfo(SPELL_EVOCATION);
							args.AddSpellBP0(spellInfo->Effects[EFFECT_0].BasePoints * 10);
							DoCastSelf(SPELL_EVOCATION, args);
						}
						else
						{
							DoCastSelf(SPELL_EVOCATION);
						}

						evocation.Repeat(2min);
					}
					else
					{
						evocation.Repeat(5s);
					}
				})
				.Schedule(1s, 5s, PHASE_COMBAT, [this](TaskContext counterspell)
				{
					if (Unit* target = DoSelectCastingUnit(SPELL_COUNTERSPELL, 35.f))
					{
						me->InterruptNonMeleeSpells(true);
						DoCast(target, SPELL_COUNTERSPELL);
						counterspell.Repeat(25s, 40s);
					}
					else
					{
						counterspell.Repeat(1s);
					}
				})
				.Schedule(3s, 6s, PHASE_COMBAT, [this](TaskContext arcane_barrage)
				{
					if (Aura* aura = me->GetAura(AURA_ARCANE_BLAST))
					{
						if (aura->GetStackAmount() >= 4)
						{
							me->InterruptNonMeleeSpells(true);
							me->RemoveAura(aura);
							DoCastVictim(SPELL_ARCANE_BARRAGE);
						}
					}

					arcane_barrage.Repeat(500ms);
				})
				.Schedule(8s, 12s, PHASE_COMBAT, [this](TaskContext arcane_explosion)
				{
					DoCastAOE(SPELL_ARCANE_EXPLOSION);
					arcane_explosion.Repeat(1min, 2min);
				});
		}

		void DamageTaken(Unit* attacker, uint32& damage) override
		{
			// Que pour la bataille de Theramore
			if (me->GetMapId() == 726)
			{
				if (attacker->GetTypeId() == TYPEID_PLAYER)
				{
					if (Player* player = attacker->ToPlayer())
					{
						if (player->IsGameMaster())
							return;
					}
				}

				if (damage >= me->GetMaxHealth())
					damage = 0;

				if (HealthBelowPct(20))
					damage = 0;
			}
		}

		void UpdateAI(uint32 diff) override
		{
			ScriptedAI::UpdateAI(diff);

            if (!UpdateVictim())
                return;

            if (me->HasBreakableByDamageCrowdControlAura())
                return;

			if (!closeTarget)
			{
				if (Unit* victim = me->GetVictim())
				{
					if (victim->IsWithinDist(me, 3.f, false))
					{
						closeTarget = true;

						scheduler.DelayGroup(PHASE_COMBAT, 2s);

						me->InterruptNonMeleeSpells(true);

						scheduler.Schedule(1s, PHASE_BLINK_SEQUENCE, [this](TaskContext context)
						{
							switch (context.GetRepeatCounter())
							{
								case 0:
									me->InterruptNonMeleeSpells(true);
									DoCastSelf(SPELL_MIRROR_IMAGE, true);
									context.Repeat(1s);
									break;

								case 1:
									DoCastSelf(SPELL_BLINK, true);
									scheduler.CancelGroup(PHASE_BLINK_SEQUENCE);
									context.Repeat(1min);
									break;

								case 2:
									closeTarget = false;
									break;
							}
						});
					}
				}
			}

			scheduler.Update(diff, [this]
			{
                DoMeleeAttackIfReady();
			});
		}

		private:
		bool closeTarget;
		SummonList summons;
	};

	CreatureAI* GetAI(Creature* creature) const override
	{
		return new npc_archmage_arcanesAI(creature);
	}
};

class npc_archmage_frost : public CreatureScript
{
	public:
	npc_archmage_frost() : CreatureScript("npc_archmage_frost") {}

	struct npc_archmage_frostAI : public CustomAI
	{
		npc_archmage_frostAI(Creature* creature) : CustomAI(creature), closeTarget(false), polymorphedGUID(ObjectGuid::Empty), polymorphTarget(false)
		{
			SetCombatMovement(false);
		}

		void SpellHitTarget(WorldObject* target, SpellInfo const* spellInfo) override
		{
			if (spellInfo->Id == SPELL_FROSTBOLT || spellInfo->Id == SPELL_ICE_LANCE)
			{
				Unit* victim = target->ToUnit();
				if (victim && !victim->HasAura(SPELL_FROSTBITE) && roll_chance_i(60))
					DoCast(victim, SPELL_FROSTBITE, true);
			}

			if (spellInfo->Id == SPELL_POLYMORPH)
			{
				polymorphedGUID = target->GetGUID();
			}
		}

		void JustEngagedWith(Unit* /*who*/) override
		{
			DoCast(SPELL_ICE_BARRIER);

			scheduler
				.Schedule(5ms, PHASE_COMBAT, [this](TaskContext polymorph)
				{
                    // Si on a moins d'une cible, on ne peut pas lancer le sheep
                    if (me->GetThreatManager().GetThreatListSize() <= 1)
                    {
                        if (polymorphTarget)
                        {
                            if (Unit* target = ObjectAccessor::GetCreature(*me, polymorphedGUID))
                            {
                                target->RemoveAurasDueToSpell(SPELL_POLYMORPH);
                                me->Attack(target, false);
                            }
                        }

                        // On tente de trouver une autre cible
                        polymorphTarget = false;
                        polymorphedGUID.Clear();
                        polymorph.Repeat(1s);
                    }
                    else
                    {
                        // Si on a déjà une cible sous sheep
                        if (polymorphTarget)
                        {
                            if (Unit* target = ObjectAccessor::GetCreature(*me, polymorphedGUID))
                            {
                                // Si la cible actuelle est morte
                                if (!target->IsAlive())
                                {
                                    // On tente de trouver une autre cible
                                    polymorphTarget = false;
                                    polymorphedGUID.Clear();
                                    polymorph.Repeat(1s);
                                }
                                else
                                {
                                    if (!target->HasBreakableByDamageCrowdControlAura())
                                    {
                                        CastPolymorph(polymorph);
                                    }
                                    else
                                    {
                                        polymorph.Repeat(1s);
                                    }
                                }
                            }
                            else
                            {
                                CastPolymorph(polymorph);
                            }
                        }
                        else
                        {
                            CastPolymorph(polymorph);
                        }
                    }
				})
				.Schedule(1s, 5s, PHASE_COMBAT, [this](TaskContext frostbotl)
				{
					DoCastVictim(SPELL_FROSTBOLT);
					frostbotl.Repeat(1500ms);
				})
				.Schedule(1s, 5s, PHASE_COMBAT, [this](TaskContext counterspell)
				{
					if (Unit* target = DoSelectCastingUnit(SPELL_COUNTERSPELL, 35.f))
					{
						me->InterruptNonMeleeSpells(true);
						DoCast(target, SPELL_COUNTERSPELL);
						counterspell.Repeat(20s, 45s);
					}
					else
					{
						counterspell.Repeat(1s);
					}
				})
				.Schedule(3s, 6s, PHASE_COMBAT, [this](TaskContext ice_lance)
				{
					me->InterruptNonMeleeSpells(false);
					if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
						DoCast(target, SPELL_ICE_LANCE);
					ice_lance.Repeat(8s, 14s);
				})
				.Schedule(30s, 50s, PHASE_COMBAT, [this](TaskContext ice_barrier)
				{
					if (!me->HasAura(SPELL_ICE_BARRIER))
					{
						DoCast(SPELL_ICE_BARRIER);
						ice_barrier.Repeat(30s);
					}
					else
					{
						ice_barrier.Repeat(1s);
					}
				});
		}

		void Reset() override
		{
			CustomAI::Reset();

			closeTarget = false;
			polymorphTarget = false;
			polymorphedGUID = ObjectGuid::Empty;

			me->RemoveAurasDueToSpell(SPELL_HYPOTHERMIA);
		}

		void DamageTaken(Unit* attacker, uint32& damage) override
		{
			// Que pour la bataille de Theramore
			#pragma region THERAMORE
			if (me->GetMapId() == 726)
			{
				if (attacker->GetTypeId() == TYPEID_PLAYER)
				{
					if (Player* player = attacker->ToPlayer())
					{
						if (player->IsGameMaster())
							return;
					}
				}

				if (damage >= me->GetMaxHealth())
					damage = 0;

				if (HealthBelowPct(20))
					damage = 0;
			}
			#pragma endregion

			// Séquence Bloc de glace
			if (!me->HasAura(SPELL_HYPOTHERMIA) && HealthBelowPct(30))
			{
				damage = 0;

				scheduler.DelayGroup(PHASE_COMBAT, 6s);

				me->InterruptNonMeleeSpells(true);

				DoCastSelf(SPELL_ICE_BLOCK);
				DoCastSelf(SPELL_HYPOTHERMIA, true);

                CastFleeSequence(5s);
			}
		}

		void UpdateAI(uint32 diff) override
		{
			ScriptedAI::UpdateAI(diff);

			if (!UpdateVictim())
				return;

            if (me->HasBreakableByDamageCrowdControlAura())
                return;

            if (!me->HasAura(SPELL_ICE_BLOCK) && !closeTarget)
			{
				if (Unit* victim = me->GetVictim())
				{
					if (victim->IsWithinDist(me, 3.f, false))
					{
						closeTarget = true;
						scheduler.DelayGroup(PHASE_COMBAT, 2s);
						me->InterruptNonMeleeSpells(true);
                        CastFleeSequence(1s);
					}
				}
			}

			scheduler.Update(diff, [this]
			{
				DoMeleeAttackIfReady();
			});
		}

		private:
		bool closeTarget;
		bool polymorphTarget;
		ObjectGuid polymorphedGUID;

		void CastFleeSequence(const std::chrono::seconds start)
		{
			scheduler.Schedule(start, PHASE_BLINK_SEQUENCE, [this](TaskContext context)
			{
				switch (context.GetRepeatCounter())
				{
					case 0:
						me->InterruptNonMeleeSpells(true);
						DoCastSelf(SPELL_FROT_NOVA, true);
						context.Repeat(500ms);
						break;

					case 1:
						DoCastSelf(SPELL_BLINK, true);
						scheduler.CancelGroup(PHASE_BLINK_SEQUENCE);
						 context.Repeat(500ms);
						break;

					case 2:
						closeTarget = false;
						break;
				}
			});
		}

		void CastPolymorph(TaskContext polymorph)
		{
            // La cible qui utilise de la mana
            if (Unit* target = DoSelectTargetByPowerEnemy(45.f, POWER_MANA))
            {
                if (!target->HasBreakableByDamageCrowdControlAura())
                {
                    me->InterruptNonMeleeSpells(true);
                    DoCast(target, SPELL_POLYMORPH);
                    polymorphTarget = true;
                    polymorph.Repeat(2s);
                }
                else
                {
                    polymorph.Repeat(1s);
                }
            }
            // La cible qui a le plus de vie
            else if (Unit* target = DoSelectLowestHpEnemy(45.f, 0))
            {
                if (!target->HasBreakableByDamageCrowdControlAura())
                {
                    me->InterruptNonMeleeSpells(true);
                    DoCast(target, SPELL_POLYMORPH);
                    polymorphTarget = true;
                    polymorph.Repeat(2s);
                }
                else
                {
                    polymorph.Repeat(1s);
                }
            }
            else
            {
                polymorph.Repeat(1s);
            }
		}
	};

	CreatureAI* GetAI(Creature* creature) const override
	{
		return new npc_archmage_frostAI(creature);
	}
};

class npc_mirror_image : public CreatureScript
{
	public:
	npc_mirror_image() : CreatureScript("npc_mirror_image")
	{
	}

	struct npc_mirror_imageAI : CustomAI
	{
		const float CHASE_DISTANCE = 35.0f;

		npc_mirror_imageAI(Creature* creature) : CustomAI(creature)
		{
			SetCombatMovement(false);
		}

		void InitializeAI() override
		{
			Unit* owner = me->GetOwner();
			if (!owner)
				return;

			owner->CastSpell(me, SPELL_CLONE_ME, true);
		}

		void JustEngagedWith(Unit* /*who*/) override
		{
			scheduler
				.Schedule(1s, 5s, PHASE_COMBAT, [this](TaskContext arcane_blast)
				{
					if (Aura* aura = me->GetAura(AURA_ARCANE_BLAST))
					{
						if (aura->GetStackAmount() < 4)
							DoCastVictim(SPELL_ARCANE_BLAST);
					}
					else
					{
						DoCastVictim(SPELL_ARCANE_BLAST);
					}

					arcane_blast.Repeat(500ms);
				})
				.Schedule(3s, 6s, PHASE_COMBAT, [this](TaskContext arcane_barrage)
				{
					if (Aura* aura = me->GetAura(AURA_ARCANE_BLAST))
					{
						if (aura->GetStackAmount() >= 4)
						{
							me->InterruptNonMeleeSpells(true);
							me->RemoveAura(aura);
							DoCastVictim(SPELL_ARCANE_BARRAGE);
						}
					}

					arcane_barrage.Repeat(500ms);
				})
				.Schedule(8s, 12s, PHASE_COMBAT, [this](TaskContext arcane_explosion)
				{
					DoCastAOE(SPELL_ARCANE_EXPLOSION);
					arcane_explosion.Repeat(1min, 2min);
				});
		}

		void UpdateAI(uint32 diff) override
		{
			Unit* owner = me->GetOwner();
			if (!owner)
			{
				me->DespawnOrUnsummon();
				return;
			}

			if (!UpdateVictim())
				return;

            if (me->HasBreakableByDamageCrowdControlAura())
                return;

			scheduler.Update(diff, [this]
			{
				DoMeleeAttackIfReady();
			});
		}

		bool CanAIAttack(Unit const* who) const override
		{
			Unit* owner = me->GetOwner();
			return owner && who->IsAlive() && me->IsValidAttackTarget(who) &&
				!who->HasBreakableByDamageCrowdControlAura() &&
				who->IsInCombatWith(owner) && ScriptedAI::CanAIAttack(who);
		}

		void EnterEvadeMode(EvadeReason why) override
		{
			if (me->IsInEvadeMode() || !me->IsAlive())
				return;

			Unit* owner = me->GetCharmerOrOwner();

			me->CombatStop(true);
			if (owner && !me->HasUnitState(UNIT_STATE_FOLLOW))
			{
				me->GetMotionMaster()->Clear();
				me->GetMotionMaster()->MoveFollow(owner, PET_FOLLOW_DIST, me->GetFollowAngle());
			}

			CustomAI::EnterEvadeMode(why);
		}
	};

	CreatureAI* GetAI(Creature* creature) const override
	{
		return new npc_mirror_imageAI(creature);
	}
};

void AddSC_npc_archmages()
{
	new npc_archmage_fire();
	new npc_archmage_arcanes();
	new npc_archmage_frost();
	new npc_mirror_image();
}
