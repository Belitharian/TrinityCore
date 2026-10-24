/*
 * Scripts de sort custom qui ne dependent d'aucun scenario
 *
 *   - spell_custom_comet_barrage : Comet Barrage (354938), fait tomber les
 *     cometes de 354936
 *
 * Commentaires en francais sans accents (encodage TC).
 */

#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "Unit.h"

// Comet Barrage - 354938
// Incantation de 3 s dont l'effet 0 (Dummy, "Server-side script") porte le
// sort declencheur 354936 dans EffectTriggerSpell. 354936 lance le missile
// (Trigger Missile vers 354937) sur un point au sol, ou les degats de givre
// sont infliges dans un rayon de 7 m.
//
// En DB2 retail, l'effet vise un point aleatoire a 1,5 m de la cible
// (TARGET_DEST_TARGET_RANDOM) : on lance la comete sur ce point. Si un hotfix
// remplace le ciblage par une zone d'unites (ex. SRC_CASTER + UNIT_SRC_AREA_ENEMY),
// chaque ennemi touche recoit sa propre comete.
class spell_custom_comet_barrage : public SpellScript
{
	bool Validate(SpellInfo const* spellInfo) override
	{
		return ValidateSpellEffect({ { spellInfo->Id, EFFECT_0 } })
			&& ValidateSpellInfo({ spellInfo->GetEffect(EFFECT_0).TriggerSpell });
	}

	void HandleDest(SpellEffIndex /*effIndex*/)
	{
		if (GetEffectInfo().TargetA.GetObjectType() != TARGET_OBJECT_TYPE_DEST)
			return;

		if (WorldLocation const* dest = GetHitDest())
			GetCaster()->CastSpell(dest->GetPosition(),GetEffectInfo().TriggerSpell, true);
	}

	void HandleUnit(SpellEffIndex /*effIndex*/)
	{
		if (Unit* victim = GetHitUnit())
			GetCaster()->CastSpell(victim->GetPosition(), GetEffectInfo().TriggerSpell, true);
	}

	void Register() override
	{
		OnEffectHit += SpellEffectFn(spell_custom_comet_barrage::HandleDest, EFFECT_0, SPELL_EFFECT_DUMMY);
		OnEffectHitTarget += SpellEffectFn(spell_custom_comet_barrage::HandleUnit, EFFECT_0, SPELL_EFFECT_DUMMY);
	}
};

void AddSC_spell_custom()
{
	RegisterSpellScript(spell_custom_comet_barrage);
}
