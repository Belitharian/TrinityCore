#include "CombatAI.h"
#include "ScriptMgr.h"
#include "PassiveAI.h"
#include "TemporarySummon.h"

// Base commune des invocations demoniaques qui peuvent proc Demonic Core a leur despawn
// et entretiennent un sort principal en boucle tant que le maitre est en combat.
struct pet_warlock_demonic_summon : public ScriptedAI
{
    static constexpr uint32 SPELL_DEMONIC_CORE_TALENT = 267102;
    static constexpr uint32 SPELL_DEMONIC_CORE_BUFF   = 270176;

    enum Events
    {
        EVENT_PRIMARY_CAST = 1
    };

    pet_warlock_demonic_summon(Creature* creature, uint32 primarySpell)
        : ScriptedAI(creature), _primarySpell(primarySpell) {}

    void Reset() override
    {
        _events.Reset();
        _events.ScheduleEvent(EVENT_PRIMARY_CAST, 0s);
    }

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
        Unit* owner = ObjectAccessor::GetUnit(*me, _owner);
        if (!owner)
            return;

        AuraEffect const* talent = owner->GetAuraEffect(SPELL_DEMONIC_CORE_TALENT, EFFECT_0);
        if (!talent || !roll_chance(talent->GetAmount()))
            return;

        owner->CastSpell(owner, SPELL_DEMONIC_CORE_BUFF, true);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = _events.ExecuteEvent())
        {
            if (eventId == EVENT_PRIMARY_CAST)
            {
                DoCastVictim(_primarySpell);
                auto [minCd, maxCd] = GetPrimaryCooldown();
                _events.ScheduleEvent(EVENT_PRIMARY_CAST, minCd, maxCd);
            }

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }
    }

    bool CanAIAttack(Unit const* target) const override
    {
        Unit* owner = me->GetOwner();
        if (owner && !target->IsInCombatWith(owner))
            return false;
        return ScriptedAI::CanAIAttack(target);
    }

protected:
    // Hook optionnel : action declenchee au moment ou le pet apparait (ex: Pursuit sur la cible du maitre).
    virtual void OnOwnerSummon(Unit* /*owner*/) { }

    // Plage de rechargement du sort principal. Min == Max pour un cooldown fixe.
    virtual std::pair<Milliseconds, Milliseconds> GetPrimaryCooldown() const = 0;

    EventMap _events;
    ObjectGuid _owner;
    uint32 _primarySpell;
};

struct pet_wild_imp : public pet_warlock_demonic_summon
{
    static constexpr uint32 SPELL_FEL_FIREBOLT = 104318;

    pet_wild_imp(Creature* creature) : pet_warlock_demonic_summon(creature, SPELL_FEL_FIREBOLT) {}

    std::pair<Milliseconds, Milliseconds> GetPrimaryCooldown() const override
    {
        return { 2300ms, 2300ms };
    }
};

struct pet_dreadstalker : public pet_warlock_demonic_summon
{
    static constexpr uint32 SPELL_PURSUIT  = 334713;
    static constexpr uint32 SPELL_DREADBITE = 205196;

    pet_dreadstalker(Creature* creature) : pet_warlock_demonic_summon(creature, SPELL_DREADBITE) {}

    void OnOwnerSummon(Unit* owner) override
    {
        if (!owner->IsInCombat())
            return;

        if (Unit* victim = owner->GetVictim())
            DoCast(victim, SPELL_PURSUIT);
    }

    std::pair<Milliseconds, Milliseconds> GetPrimaryCooldown() const override
    {
        return { 2s, 8s };
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

    void HandleEffectPeriodic(AuraEffect const* aurEff) const
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
