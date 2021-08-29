#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "AreaTriggerTemplate.h"
#include "AreaTriggerAI.h"
#include "Player.h"
#include "Spell.h"
#include "SpellScript.h"
#include "AreaTrigger.h"
#include "ObjectAccessor.h"

enum Spells
{
    SPELL_CEASELESS_WINTER              = 227779,
    SPELL_CEASELESS_WINTER_DMG          = 227806,
};

class spell_shade_medivh_ceaseless_winter : public SpellScriptLoader
{
    public:
    spell_shade_medivh_ceaseless_winter() : SpellScriptLoader("spell_shade_medivh_ceaseless_winter")
    {
    }

    class spell_ceaserless_winter_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_ceaserless_winter_AuraScript);

        void OnApply(AuraEffect const* /**/, AuraEffectHandleModes /**/)
        {
            if (!GetUnitOwner())
                return;

            _ownerPos = GetUnitOwner()->GetPosition();
        }

        void OnPeriodic(AuraEffect const* /**/)
        {
            if (!GetUnitOwner())
                return;

            _ownerPos.SetOrientation(GetUnitOwner()->GetOrientation());

            if (_ownerPos != GetUnitOwner()->GetPosition())
            {
                _ownerPos = GetUnitOwner()->GetPosition();
                ModStackAmount(-1);
            }
            else
                ModStackAmount(1);
        }

        void Register() override
        {
            OnEffectPeriodic += AuraEffectPeriodicFn(spell_ceaserless_winter_AuraScript::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
            OnEffectApply += AuraEffectApplyFn(spell_ceaserless_winter_AuraScript::OnApply, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
        }

        Position _ownerPos;
    };

    AuraScript* GetAuraScript() const override
    {
        return new spell_ceaserless_winter_AuraScript();
    }
};

class at_kara_ceaseless_winter : public AreaTriggerEntityScript
{
    public:
    at_kara_ceaseless_winter() : AreaTriggerEntityScript("at_kara_ceaseless_winter")
    {
    }

    struct at_kara_ceaseless_winter_AI : public AreaTriggerAI
    {
        explicit at_kara_ceaseless_winter_AI(AreaTrigger* at) : AreaTriggerAI(at)
        {
            _timerCheck = 0;
        }

        void OnUnitEnter(Unit* target) override
        {
            if (target && target->GetTypeId() == TYPEID_PLAYER)
                target->CastSpell(target, SPELL_CEASELESS_WINTER_DMG, true);
        }

        void OnUnitExit(Unit* target) override
        {
            if (target && target->GetTypeId() == TYPEID_PLAYER)
                target->RemoveAurasDueToSpell(SPELL_CEASELESS_WINTER_DMG);
        }

        void OnUpdate(uint32 diff) override
        {
            _timerCheck += diff;

            if (_timerCheck >= IN_MILLISECONDS + 500)
            {
                _timerCheck = 0;

                for (auto& it : at->GetInsideUnits())
                {
                    if (Player* player = ObjectAccessor::GetPlayer(*at, it))
                    {
                        if (!player->HasAura(SPELL_CEASELESS_WINTER_DMG))
                            player->CastSpell(player, SPELL_CEASELESS_WINTER_DMG, true);
                    }
                }
            }
        }

        private:
        uint32 _timerCheck;
    };

    AreaTriggerAI* GetAI(AreaTrigger* at) const override
    {
        return new at_kara_ceaseless_winter_AI(at);
    }
};

void AddSC_custom_spells()
{
    new spell_shade_medivh_ceaseless_winter();
    new at_kara_ceaseless_winter();
}
