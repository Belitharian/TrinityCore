/*
 * Ruins of Theramore - AIs secondaires, GameObject et SpellScripts
 *
 * Regroupe tout ce qui n'est ni Jaina (ruins_of_theramore.cpp) ni
 * l'InstanceScript (scenario_ruins_of_theramore.cpp) :
 *
 *   - npc_water_elementals_theramore : elementaires invoques par Jaina,
 *     ils servent de tank aux hordes pendant la phase BackToSender
 *   - npc_roknah_warlord             : boss de fin, ne meurt pas de lui-meme
 *     (il passe a genoux a 20% PV et attend le finisher scripte de Jaina)
 *   - go_theramore_banner            : banniere ramassable pendant la phase Standards
 *   - spell_ruins_*                  : sorts dummy relayes vers leur TriggerSpell
 *   - spell_arcane_chaos             : AoE periodique de Jaina sur tous les hostiles
 *
 * Commentaires en francais sans accents (encodage TC).
 */

#include "GameObject.h"
#include "GameObjectAI.h"
#include "InstanceScript.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "Custom/CustomAI/CustomAI.h"
#include "ruins_of_theramore.h"

// =========================================================================
// npc_water_elementals_theramore - Elementaires d'eau de Jaina
// =========================================================================
// Invoques par SPELL_SUMMON_WATER_ELEMENTALS (voir npc_jaina_ruins).
// Ils restent au corps a corps rapproche et encaissent les hordes pendant la
// phase BackToSender. Un bouclier one-shot se declenche a 30% PV.
struct npc_water_elementals_theramore : public CustomAI
{
	npc_water_elementals_theramore(Creature* creature) : CustomAI(creature), shielded(false)
	{
		// Tanks statiques : pas d'errance aleatoire hors combat.
		SetCanRandomMovement(false);
	}

	enum Spells
	{
		SPELL_WATER_SPOUT           = 271287,
		SPELL_WATERY_DOME           = 258153,
		SPELL_WATER_BOLT_VOLLEY     = 290084,
		SPELL_WATER_BOLT            = 355225,
	};

	// Seuil de declenchement du bouclier defensif (en % de PV).
	static constexpr uint8 SHIELD_HEALTH_PCT = 30;
	// Distance de suivi : volontairement courte pour que les elementaires
	// restent colles aux hordes qu'ils doivent bloquer.
	static constexpr float FOLLOW_DISTANCE   = 5.0f;

	bool shielded;                          // Bouclier a 30% PV : one-shot

	float GetDistance() override
	{
		return FOLLOW_DISTANCE;
	}

	// Bouclier defensif unique lorsque l'elementaire descend sous le seuil.
	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
		if (me->HealthBelowPctDamaged(SHIELD_HEALTH_PCT, damage) && !shielded)
		{
			DoCastSelf(SPELL_WATERY_DOME);
			shielded = true;
		}
	}

	void JustEngagedWith(Unit* who) override
	{
		// Premier cast immediat pour eviter un temps mort a l'engagement.
		DoCast(who, SPELL_WATER_BOLT);

		scheduler
			// Filler : Water Bolt en boucle sur la victime courante.
			.Schedule(5ms, [this](TaskContext water_bolt)
			{
				DoCastVictim(SPELL_WATER_BOLT);
				water_bolt.Repeat(2800ms);
			})
			// Water Spout sur une cible aleatoire (interrompt le filler en cours).
			.Schedule(10s, 15s, [this](TaskContext water_spout)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
				{
					CastStop();
					DoCast(target, SPELL_WATER_SPOUT);
				}
				water_spout.Repeat(24s, 32s);
			})
			// Volee AoE : annule un Water Spout en cours pour ne pas se chevaucher.
			.Schedule(12s, 22s, [this](TaskContext water_bolt_volley)
			{
				CastStop(SPELL_WATER_SPOUT);
				DoCast(SPELL_WATER_BOLT_VOLLEY);
				water_bolt_volley.Repeat(18s, 20s);
			});
	}
};

// =========================================================================
// npc_roknah_warlord - Boss de la phase BackToSender
// =========================================================================
// Le warlord ne peut pas mourir des degats des joueurs : arrive au seuil bas
// il devient invulnerable, s'agenouille et attend que Jaina vienne l'achever
// (voir npc_jaina_ruins::MovementInform / MOVEMENT_INFO_POINT_02).
struct npc_roknah_warlord : public CustomAI
{
	npc_roknah_warlord(Creature* creature) : CustomAI(creature, AI_Type::Melee), isAlmostDead(false)
	{
		instance = creature->GetInstanceScript();
	}

	enum Spells
	{
		SPELL_EXECUTE               = 283424,
		SPELL_MORTAL_STRIKE         = 283410,
		SPELL_OVERPOWER             = 283426,
		SPELL_REND                  = 283419,
		SPELL_SLAM                  = 299995
	};

	// Seuil a partir duquel le warlord devient invulnerable et s'agenouille.
	static constexpr uint8 ALMOST_DEAD_HEALTH_PCT = 20;
	// Chance de placer un Overpower avant le Mortal Strike enchaine.
	static constexpr uint32 OVERPOWER_CHANCE      = 60;

	InstanceScript* instance;
	bool isAlmostDead;                      // Passage en etat "a genoux" : one-shot

	// Les degats sont annules sous le seuil et la sequence de finish scriptee
	// n'est declenchee qu'une seule fois.
	void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
	{
		if (me->HealthBelowPctDamaged(ALMOST_DEAD_HEALTH_PCT, damage))
		{
			damage = 0;

			if (isAlmostDead)
				return;

			isAlmostDead = true;

			// Fige completement le warlord : plus de combat, plus de regen,
			// plus d'attaquants, il reste plante la en attendant Jaina.
			me->SetHomePosition(me->GetPosition());
			me->SetReactState(REACT_PASSIVE);
			me->SetRegenerateHealth(false);
			me->SetImmuneToAll(true);
			me->RemoveAllAttackers();

			// Petit delai avant la genuflexion pour laisser respirer l'animation.
			scheduler.Schedule(2s, [this](TaskContext /*context*/)
			{
				me->SetStandState(UNIT_STAND_STATE_KNEEL);
			});

			// Jaina sort de sa rotation : on la nettoie de ses auras de combat.
			if (Creature* jaina = instance->GetCreature(DATA_JAINA_PROUDMOORE))
			{
				jaina->RemoveAllAuras();
			}

			// Signale a l'InstanceScript que la sequence du finisher peut demarrer.
			instance->SetData(EVENT_WARLORD_ROKNAH_SLAIN, 0U);
		}
	}

	void JustEngagedWith(Unit* /*who*/) override
	{
		scheduler
			.Schedule(5s, 8s, [this](TaskContext execute)
			{
				DoCastVictim(SPELL_EXECUTE);
				execute.Repeat(15s, 28s);
			})
			// Combo en deux temps : Overpower optionnel (repeat 1s) puis Mortal Strike.
			.Schedule(2s, 5s, [this](TaskContext mortal_strike)
			{
				switch (mortal_strike.GetRepeatCounter())
				{
					case 0:
						if (!me->HasAura(SPELL_OVERPOWER) && roll_chance(OVERPOWER_CHANCE))
							DoCastSelf(SPELL_OVERPOWER);
						mortal_strike.Repeat(1s);
						break;
					case 1:
						me->CastStop();
						DoCastVictim(SPELL_MORTAL_STRIKE);
						mortal_strike.Repeat(8s, 10s);
						break;
				}
			})
			.Schedule(14s, 22s, [this](TaskContext overpower)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, SPELL_REND);
				overpower.Repeat(8s, 10s);
			})
			.Schedule(25s, 32s, [this](TaskContext rend_slam)
			{
				if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
					DoCast(target, RAND(SPELL_REND, SPELL_SLAM));
				rend_slam.Repeat(2s, 8s);
			});
	}
};

// =========================================================================
// go_theramore_banner - Banniere de Theramore (phase Standards)
// =========================================================================
// Interactible uniquement pendant la phase Standards : elle applique le buff
// de banniere au joueur puis disparait. En dehors de cette phase le clic est
// ignore (return false -> comportement par defaut du GameObject).
struct go_theramore_banner : public GameObjectAI
{
	go_theramore_banner(GameObject* go) : GameObjectAI(go)
	{
		instance = go->GetInstanceScript();
	}

	enum Spells
	{
		SPELL_STANDARD_OF_THERAMORE = 105690
	};

	InstanceScript* instance;

	bool OnGossipHello(Player* player) override
	{
		RFTPhases phase = (RFTPhases)instance->GetData(DATA_SCENARIO_PHASE);
		if (phase != RFTPhases::Standards)
			return false;

		player->CastSpell(player, SPELL_STANDARD_OF_THERAMORE, true);
		me->DespawnOrUnsummon();
		return true;
	}
};

// =========================================================================
// SpellScripts
// =========================================================================

// Frigid Shards - 354933
// Effet dummy periodique : relaie le TriggerSpell declare en BDD depuis le
// caster vers la cible de l'aura (l'effet dummy ne le fait pas tout seul).
class spell_ruins_frigid_shards : public AuraScript
{
	void OnPeriodic(AuraEffect const* aurEff)
	{
		Unit* target = GetTarget();
		Unit* caster = GetCaster();
		if (target && caster)
		{
			uint32 triggerSpell = GetSpellInfo()->GetEffect(aurEff->GetEffIndex()).TriggerSpell;
			caster->CastSpell(target, triggerSpell, true);
		}
	}

	void Register() override
	{
		OnEffectPeriodic += AuraEffectPeriodicFn(spell_ruins_frigid_shards::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
	}
};

// Comet Barrage - 354938
// Meme principe que Frigid Shards, mais sur un effet dummy de sort direct.
class spell_ruins_comet_barrage : public SpellScript
{
	void HandleDamages(SpellEffIndex effIndex)
	{
		Unit* caster = GetCaster();
		Unit* victim = GetHitUnit();
		if (caster && victim)
		{
			uint32 triggerSpell = GetSpellInfo()->GetEffect(effIndex).TriggerSpell;
			caster->CastSpell(victim, triggerSpell, true);
		}
	}

	void Register() override
	{
		OnEffectHitTarget += SpellEffectFn(spell_ruins_comet_barrage::HandleDamages, EFFECT_0, SPELL_EFFECT_DUMMY);
	}
};

// Arcane Chaos - 406854
// Aura periodique de Jaina : a chaque tick, envoie un missile sur TOUS les
// hostiles vivants a portee. Les morts et les feign death sont filtres pour
// ne pas gaspiller de missiles sur des cibles invalides.
class spell_arcane_chaos : public AuraScript
{
	static constexpr float MAX_RANGE = 20.0f;

	enum Spells
	{
		SPELL_ARCANE_CHAOS_MISSILE = 406859,
	};

	void OnPeriodic(AuraEffect const* /*aurEff*/)
	{
		Unit* caster = GetCaster();
		if (!caster)
			return;

		// Recherche de tous les hostiles a portee autour du caster.
		std::list<Unit*> targets;
		Trinity::AnyUnfriendlyUnitInObjectRangeCheck uCheck(caster, caster, MAX_RANGE);
		Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(caster, targets, uCheck);
		Cell::VisitAllObjects(caster, searcher, MAX_RANGE);

		targets.remove_if([](Unit* unit)
		{
			return unit->isDead() || unit->HasUnitFlag2(UNIT_FLAG2_FEIGN_DEATH);
		});

		for (Unit* victim : targets)
			caster->CastSpell(victim, SPELL_ARCANE_CHAOS_MISSILE, true);
	}

	void Register() override
	{
		OnEffectPeriodic += AuraEffectPeriodicFn(spell_arcane_chaos::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
	}
};

// =========================================================================
// Registration
// =========================================================================
void AddSC_npcs_ruins_of_theramore()
{
	// Utilisable en dehors de l'instance
	RegisterCreatureAI(npc_water_elementals_theramore);

	RegisterRuinsAI(npc_roknah_warlord);

	RegisterGameObjectAI(go_theramore_banner);

	RegisterSpellScript(spell_ruins_comet_barrage);
	RegisterSpellScript(spell_ruins_frigid_shards);
	RegisterSpellScript(spell_arcane_chaos);
}
