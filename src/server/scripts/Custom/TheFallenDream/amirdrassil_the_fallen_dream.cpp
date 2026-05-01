#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "Creature.h"
#include "InstanceScript.h"
#include "ScriptMgr.h"
#include "instance_amirdrassil_the_fallen_dream.h"

// 430524 - Inflorescence
class spell_inflorescence : public AuraScript
{
    enum Spells
    {
        SPELL_INFLORESCENCE_PERIODIC_DUMMY = 430525
    };

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_INFLORESCENCE_PERIODIC_DUMMY });
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        caster->CastSpell(caster, SPELL_INFLORESCENCE_PERIODIC_DUMMY, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_inflorescence::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 430525 - Inflorescence
// ID - 30892
struct at_inflorescence : AreaTriggerAI
{
    at_inflorescence(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger)
    {
        uint32 timeToTarget = areatrigger->GetTimeToTargetScale();
        _timeToTargetScale = Milliseconds(timeToTarget) - 1s;
    }

    virtual float GetScale()
    {
        return 4.0f;
    }

    void OnInitialize() override
    {
        UpdateSize(0.0f, GetScale(), 1000);

        _scheduler.Schedule(_timeToTargetScale, [this](TaskContext /*task*/)
        {
            UpdateSize(GetScale(), 0.0f, 1000);
        });
    }

    void UpdateSize(float currentScale, float targetScale, uint32 duration) const
    {
        std::array<DBCPosition2D, 2> points =
        { {
            { 0.0f, currentScale },
            { 1.0f, targetScale  }
        } };

        at->SetTimeToTargetScale(duration);
        at->SetOverrideScaleCurve(points);
    }

    void OnUpdate(uint32 diff) override
    {
        _scheduler.Update(diff);
    }

private:
    TaskScheduler _scheduler;
    Milliseconds _timeToTargetScale;
};

// 429127 - Inflorescence
// ID - 30729
struct at_inflorescence_effect : at_inflorescence
{
    enum Spells
    {
        SPELL_INFLORESCENCE = 429178
    };

    at_inflorescence_effect(AreaTrigger* areatrigger) : at_inflorescence(areatrigger) {}

    float GetScale() override
    {
        return 46.0f;
    }

    void OnUnitEnter(Unit* unit) override
    {
        Unit* caster = at->GetCaster();
        if (!caster)
            return;

        caster->CastSpell(unit, SPELL_INFLORESCENCE, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void OnUnitExit(Unit* unit, AreaTriggerExitReason /*reason*/) override
    {
        unit->RemoveAurasDueToSpell(SPELL_INFLORESCENCE);
    }
};

// Surging Growth
struct at_surging_growth : AreaTriggerAI
{
    at_surging_growth(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) {}

    void OnUpdate(uint32 diff) override
    {
        _scheduler.Update(diff);
    }

    void UpdateArea(AreaTrigger* at)
    {
        at->SetTimeToTargetScale(1000);

        float currentScale = std::min(at->CalcCurrentScale(), 0.9f);
        float targetScale = currentScale - (0.1f * GetInsidePlayersCount());

        UpdateSize(currentScale, targetScale);

        if (targetScale <= 0.01f)
            at->Remove();
    }

    void UpdateSize(float currentScale, float targetScale) const
    {
        std::array<DBCPosition2D, 2> points =
        { {
            { 0.0f, currentScale },
            { 1.0f, targetScale }
        } };

        at->SetOverrideScaleCurve(points);
    }

    uint32 GetInsidePlayersCount()
    {
        return std::ranges::count_if(at->GetInsideUnits(), [this](ObjectGuid const& guid)
            {
                Player* player = ObjectAccessor::GetPlayer(*at, guid);
                if (!player || !player->IsAlive() || player->IsGameMaster())
                    return false;
                return true;
            });
    }

protected:
    TaskScheduler _scheduler;
};

// 420977 - Surging Growth
// ID - 30302
struct at_surging_growth_effect : at_surging_growth
{
    enum Spells
    {
        SPELL_SURGING_GROWTH_VISUAL = 424481,
        SPELL_SURGING_GROWTH_DAMAGE = 425357,
    };

    at_surging_growth_effect(AreaTrigger* areatrigger) : at_surging_growth(areatrigger) {}

    void OnInitialize() override
    {
        Unit* caster = at->GetCaster();
        if (!caster)
            return;

        caster->CastSpell(at->GetPosition(), SPELL_SURGING_GROWTH_VISUAL);

        _scheduler
            .Schedule(1s, [this](TaskContext task)
            {
                UpdateArea(at);
                task.Repeat(1s);
            })
            .Schedule(2s, [this](TaskContext context)
            {
                if (Unit* caster = at->GetCaster())
                {
                    caster->CastSpell(at->GetPosition(),
                        SPELL_SURGING_GROWTH_DAMAGE,
                        TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);

                    context.Repeat(2s);
                }   
            });
    }
};

// 424481 - Surging Growth
// ID - 30346
struct at_surging_growth_visual : at_surging_growth
{
    at_surging_growth_visual(AreaTrigger* areatrigger) : at_surging_growth(areatrigger) {}

    void OnInitialize() override
    {
        Unit* caster = at->GetCaster();
        if (!caster)
            return;

        _scheduler.Schedule(1s, [this](TaskContext task)
        {
            UpdateArea(at);
            task.Repeat(1s);
        });
    }
};

void AddSC_amirdrassil_the_fallen_dream()
{
    RegisterSpellScript(spell_inflorescence);

    RegisterAreaTriggerAI(at_inflorescence);
    RegisterAreaTriggerAI(at_inflorescence_effect);

    RegisterAreaTriggerAI(at_surging_growth_effect);
    RegisterAreaTriggerAI(at_surging_growth_visual);
}
