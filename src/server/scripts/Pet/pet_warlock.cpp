#include "CombatAI.h"
#include "CustomAI.h"
#include "PassiveAI.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"
#include "spell_warlock.h"

// Base commune des invocations demoniaques qui peuvent proc Demonic Core a leur despawn
// et entretiennent un sort principal en boucle tant que le maitre est en combat.
struct pet_warlock_demonic_summon : public CustomAI
{
    enum Spells
    {
        SPELL_DEMONIC_CORE_TALENT   = 267102,
        SPELL_DEMONIC_CORE_BUFF     = 270171
    };

    pet_warlock_demonic_summon(Creature* creature, AI_Type type)
        : CustomAI(creature, type) {}

    void IsSummonedBy(WorldObject* summoner) override
    {
        Unit* caster = summoner->ToUnit();
        if (!caster)
            return;

        _owner = caster->GetGUID();

        TempSummon* summon = me->ToTempSummon();
        if (summon && summon->IsGuardian())
            static_cast<Guardian*>(me)->SetBonusDamage(caster->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_MAGIC));

        OnOwnerSummon(caster);
    }

    void OnDespawn() override
    {
        BuffOwner();
        OnDisappear();
    }

    void JustDied(Unit* /*killer*/) override
    {
        BuffOwner();
        OnDisappear();
    }

    bool CanAIAttack(Unit const* target) const override
    {
        Unit* owner = me->GetOwner();
        if (owner && !target->IsInCombatWith(owner))
            return false;
        return CustomAI::CanAIAttack(target);
    }

protected:
    // Hook optionnel : action declenchee au moment ou le pet apparait (ex: Pursuit sur la cible du maitre).
    virtual void OnOwnerSummon(Unit* /*owner*/) {}

    // Event quand l'invocation disparait / meurt
    virtual void OnDisappear() {};

    // Retourne l'index a prendre en compte.
    virtual SpellEffIndex GetEffIndex() const = 0;

    ObjectGuid _owner;

private:
    // Retourne la chance de proc de Demonic Core.
    bool GetAuraEffect(Unit* const owner) const
    {
        AuraEffect const* talent = owner->GetAuraEffect(SPELL_DEMONIC_CORE_TALENT, GetEffIndex());
        if (!talent || !roll_chance(talent->GetAmount()))
            return false;

        return true;
    }

    // Lance le buff Demonic Core.
    void BuffOwner()
    {
        Unit* owner = ObjectAccessor::GetUnit(*me, _owner);
        if (!owner)
            return;

        if (!GetAuraEffect(owner))
            return;

        owner->CastSpell(owner, SPELL_DEMONIC_CORE_BUFF, true);
    }
};

struct pet_wild_imp : public pet_warlock_demonic_summon
{
    enum Spells
    {
        SPELL_FEL_FIREBOLT          = 104318,
        SPELL_IMPLOSION_DAMAGE      = 196278
    };

    pet_wild_imp(Creature* creature) : pet_warlock_demonic_summon(creature, AI_Type::Stay) {}

    SpellEffIndex GetEffIndex() const override
    {
        return EFFECT_0;
    }

    void SpellHitTarget(WorldObject* /*object*/, SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_IMPLOSION_DAMAGE)
            me->DespawnOrUnsummon();
    }

    void OnDisappear() override
    {
        Unit* owner = ObjectAccessor::GetUnit(*me, _owner);
        if (!owner)
            return;

        spell_wild_imp_aura::RemoveImp(owner, 1);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        scheduler.Schedule(0s, [this](TaskContext primarySpell)
        {
            DoCastVictim(SPELL_FEL_FIREBOLT);
            primarySpell.Repeat(2300ms, 2300ms);
        });
    }
};

struct pet_dreadstalker : public pet_warlock_demonic_summon
{
    pet_dreadstalker(Creature* creature) : pet_warlock_demonic_summon(creature, AI_Type::Melee) {}

    SpellEffIndex GetEffIndex() const override
    {
        return EFFECT_1;
    }
};

struct pet_mother_of_chaos : public PassiveAI
{
    enum Spells
    {
        SPELL_CHAOS_SALVO       = 432569
    };

    pet_mother_of_chaos(Creature* creature) : PassiveAI(creature) {}

    void IsSummonedBy(WorldObject* summoner) override
    {
        Unit* caster = summoner->ToUnit();
        if (!caster)
            return;

        TempSummon* summon = me->ToTempSummon();
        if (summon && summon->IsGuardian())
            static_cast<Guardian*>(me)->SetBonusDamage(caster->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_MAGIC));

        if (!caster->IsInCombat())
            return;

        if (Unit* victim = caster->GetVictim())
            DoCast(victim, SPELL_CHAOS_SALVO);
    }

    void OnSpellCast(SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_CHAOS_SALVO)
            me->DespawnOrUnsummon(static_cast<Milliseconds>(spell->CalcDuration()));
    }
};

struct pet_overlord : public PassiveAI
{
    enum Spells
    {
        SPELL_CHARGE            = 432113,
        SPELL_WICKED_CLEAVE     = 432120,
    };

    pet_overlord(Creature* creature) : PassiveAI(creature) {}

    void IsSummonedBy(WorldObject* summoner) override
    {
        Unit* caster = summoner->ToUnit();
        if (!caster)
            return;

        TempSummon* summon = me->ToTempSummon();
        if (summon && summon->IsGuardian())
            static_cast<Guardian*>(me)->SetBonusDamage(caster->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_MAGIC));

        if (!caster->IsInCombat())
            return;

        if (Unit* victim = caster->GetVictim())
            DoCast(victim, SPELL_CHARGE);
    }

    void SpellHitTarget(WorldObject* /*object*/, SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_WICKED_CLEAVE)
            me->DespawnOrUnsummon();
    }
};

struct pet_pit_lord : public PassiveAI
{
    enum Spells
    {
        SPELL_FELSEEKER         = 438973,
        SPELL_PIT_LORD_PORTAL   = 439562,
    };

    pet_pit_lord(Creature* creature) : PassiveAI(creature) {}

    void IsSummonedBy(WorldObject* summoner) override
    {
        Unit* caster = summoner->ToUnit();
        if (!caster)
            return;

        TempSummon* summon = me->ToTempSummon();
        if (summon && summon->IsGuardian())
            static_cast<Guardian*>(me)->SetBonusDamage(caster->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_MAGIC));

        if (!caster->IsInCombat())
            return;

        DoCastSelf(SPELL_PIT_LORD_PORTAL, true);

        if (Unit* victim = caster->GetVictim())
            DoCast(victim, SPELL_FELSEEKER);
    }

    void OnSpellCast(SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_FELSEEKER)
            me->DespawnOrUnsummon(static_cast<Milliseconds>(spell->CalcDuration()));
    }
};

// 432569 - Chaos Salvo
class spell_chaos_salvo : public AuraScript
{
    enum Spells
    {
        SPELL_CHAOS_SALVO_PERIODIC = 432596,
    };

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CHAOS_SALVO_PERIODIC });
    }

    void HandleEffectPeriodic(AuraEffect const* /*aurEff*/) const
    {
        if (Unit* target = GetTarget())
        {
            GetCaster()->CastSpell(target, SPELL_CHAOS_SALVO_PERIODIC,
                TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_chaos_salvo::HandleEffectPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

void AddSC_warlock_pet_scripts()
{
    RegisterCreatureAI(pet_wild_imp);
    RegisterCreatureAI(pet_dreadstalker);
    RegisterCreatureAI(pet_mother_of_chaos);
    RegisterCreatureAI(pet_overlord);
    RegisterCreatureAI(pet_pit_lord);

    RegisterSpellScript(spell_chaos_salvo);
}
