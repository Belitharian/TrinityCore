/*
 * Ruins of Theramore - AIs secondaires, GameObject et SpellScripts
 *
 * Regroupe tout ce qui n'est ni Jaina (ruins_of_theramore.cpp) ni
 * l'InstanceScript (scenario_ruins_of_theramore.cpp) :
 *
 *   - npc_water_elementals_theramore : elementaires d'eau invoques par Jaina (Bataille de
 *     Theramore, entrees 10955 / 65680 / 68956)
 *   - npc_roknah_warlord             : chef des pillards, ne meurt pas de lui-meme
 *     (il passe a genoux a 20% PV et attend le finisher scripte de Jaina)
 *   - go_theramore_banner            : etendard pille, ramassable pendant la phase Looters
 *   - spell_ruins_frigid_shards      : Frigid Shards (354933), lance par la Jaina de la Purge
 *   - spell_ruins_flaming_twisters   : Flaming Twisters (1283566) de Jaina
 *   - at_ruins_flaming_twister       : la tornade de feu (areatrigger 41584)
 *
 * Commentaires en francais sans accents (encodage TC).
 */

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "Containers.h"
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
// combat. Un bouclier one-shot se declenche a 30% PV.
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
// npc_roknah_warlord - Chef des pillards (phase Looters)
// =========================================================================
// Le chef des pillards ne peut pas mourir des degats des joueurs : arrive au seuil bas
// il devient invulnerable, s'agenouille et attend que Jaina vienne l'achever
// (voir scenario_ruins_of_theramore::TryStartFinisher).
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

			// Signale a l'InstanceScript que le chef est a genoux : Jaina viendra
			// l'achever une fois l'etape des pillards validee.
			instance->SetData(DATA_WARLORD_KNEELING, 0U);
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
// go_theramore_banner - Banniere de Theramore (phase Looters)
// =========================================================================
// Interactible uniquement pendant la phase Looters : elle applique le buff
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
		if (phase != RFTPhases::Looters)
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

// Flaming Twisters - 1283566 (Embertalon, Midnight)
// Dummy "server-side script" sur les ennemis a 60 m (TARGET_UNIT_SRC_AREA_ENEMY) :
// on n'en garde que BasePoints (3) au hasard, et sur chacun on lance le missile
// du TriggerSpell (1283561), qui pose a l'arrivee l'areatrigger 41584 (1283558).
class spell_ruins_flaming_twisters : public SpellScript
{
	bool Validate(SpellInfo const* spellInfo) override
	{
		return ValidateSpellEffect({ { spellInfo->Id, EFFECT_0 } })
			&& ValidateSpellInfo({ spellInfo->GetEffect(EFFECT_0).TriggerSpell });
	}

	void FilterTargets(std::list<WorldObject*>& targets)
	{
		int32 const count = GetEffectInfo(EFFECT_0).CalcValue(GetCaster());
		Trinity::Containers::RandomResize(targets, std::max(count, 1));
	}

	void HandleDummy(SpellEffIndex effIndex)
	{
		Unit* caster = GetCaster();
		Unit* target = GetHitUnit();
		if (!caster || !target)
			return;

		// Le missile vise une destination (TARGET_DEST_DEST) : la position de la cible.
		caster->CastSpell(target->GetPosition(), GetEffectInfo(effIndex).TriggerSpell, true);
	}

	void Register() override
	{
		OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_ruins_flaming_twisters::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
		OnEffectHitTarget += SpellEffectFn(spell_ruins_flaming_twisters::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
	}
};

// Flaming Twister - AreaTrigger 41584 (sort 1283558, 20 s)
// La tornade se balade au hasard pendant ses 20 s (spline relancee a chaque
// arrivee) et brule les ennemis de son lanceur tant qu'ils sont dedans :
// DoT 1283559 (4 % toutes les 1,5 s). Elle reste dans la zone du combat : au-dela
// de WANDER_LEASH de son point d'apparition, elle repart vers lui.
struct at_ruins_flaming_twister : AreaTriggerAI
{
	using AreaTriggerAI::AreaTriggerAI;

	enum Spells
	{
		SPELL_FLAMING_TWISTERS_DAMAGE = 1283559
	};

	static constexpr float WANDER_MIN   = 10.0f;
	static constexpr float WANDER_MAX   = 20.0f;
	static constexpr float WANDER_SPEED = 6.0f;
	static constexpr float WANDER_LEASH = 40.0f;
	static constexpr uint8 WANDER_TRIES = 6;

	Position origin;

	void OnInitialize() override
	{
		origin = at->GetPosition();
		Wander();
	}

	void OnDestinationReached() override
	{
		Wander();
	}

	// Un grand pas dans une direction au hasard, arrete par le premier obstacle.
	// Plusieurs essais : contre un mur, une autre direction finit par passer.
	void Wander()
	{
		for (uint8 i = 0; i < WANDER_TRIES; ++i)
		{
			// Trop loin de son point d'apparition : elle y revient (a peu pres).
			float angle = frand(0.0f, float(2 * M_PI));
			if (at->GetExactDist2d(origin) > WANDER_LEASH)
				angle = at->GetAbsoluteAngle(origin) + frand(-float(M_PI) / 4, float(M_PI) / 4);

			// MovePositionToFirstCollision ajoute l'orientation a l'angle : on la retranche.
			Position dest = at->GetPosition();
			at->MovePositionToFirstCollision(dest, frand(WANDER_MIN, WANDER_MAX), angle - at->GetOrientation());

			std::vector<G3D::Vector3> points;
			points.emplace_back(at->GetPositionX(), at->GetPositionY(), at->GetPositionZ());
			points.emplace_back(dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ());

			// Spline trop courte (bloquee contre un mur) : on essaie une autre direction.
			if ((points[1] - points[0]).squaredMagnitude() < 4.0f)
				continue;

			at->InitSplines(points, WANDER_SPEED);
			return;
		}
	}

	void OnUnitEnter(Unit* unit) override
	{
		Unit* caster = at->GetCaster();
		if (caster && caster->IsValidAttackTarget(unit))
			caster->CastSpell(unit, SPELL_FLAMING_TWISTERS_DAMAGE, true);
	}

	void OnUnitExit(Unit* unit, AreaTriggerExitReason /*reason*/) override
	{
		unit->RemoveAurasDueToSpell(SPELL_FLAMING_TWISTERS_DAMAGE, at->GetCasterGuid());
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

	RegisterSpellScript(spell_ruins_frigid_shards);
	RegisterSpellScript(spell_ruins_flaming_twisters);

	RegisterAreaTriggerAI(at_ruins_flaming_twister);
}
