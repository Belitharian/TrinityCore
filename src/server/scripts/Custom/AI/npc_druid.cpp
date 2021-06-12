#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ThreatManager.h"
#include "MotionMaster.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "SpellAuraEffects.h"
#include "CustomAI.h"

enum Spells
{
	SPELL_LIFEBLOOM                 = 100061,
	SPELL_LIFEBLOOM_FINAL_HEAL      = 100065,
	SPELL_SWIFTMEND                 = 100067,
    SPELL_REJUVENATION              = 100131,
    SPELL_REGROWTH                  = 100132,
    SPELL_HEALING_TOUCH             = 100133,
    SPELL_WRATH                     = 100134,
    SPELL_MOONFIRE                  = 100135,
    SPELL_WILD_GROWTH               = 100136,
    SPELL_SHAPESHIFT_BEAR           = 100137,
	SPELL_NATURE_SWIFTNESS          = 17116,
};

enum Phases
{
    PHASE_COMBAT,
    PHASE_HEALING,
    PHASE_BEAR,
};

class npc_druid : public CreatureScript
{
	public:
	npc_druid() : CreatureScript("npc_druid") {}

	struct npc_druidAI : public CustomAI
	{
		npc_druidAI(Creature* creature) : CustomAI(creature), shapeshifted(false)
		{
		}

        void DamageTaken(Unit* attacker, uint32& /*damage*/) override
        {
            if (!shapeshifted && !me->HasAura(SPELL_SHAPESHIFT_BEAR) && HealthBelowPct(20))
            {
                shapeshifted = true;

                //SetCombatMovement(true);

                DoCastSelf(SPELL_REJUVENATION, true);

                scheduler.DelayGroup(PHASE_COMBAT, 20s);
                scheduler.DelayGroup(PHASE_HEALING, 20s);

                scheduler
                    .Schedule(1s, PHASE_BEAR, [this, attacker](TaskContext context)
                    {
                        DoCastSelf(SPELL_SHAPESHIFT_BEAR);
                        me->GetMotionMaster()->MoveFleeing(attacker, 10 * IN_MILLISECONDS);
                    })
                    //.Schedule(10s, PHASE_BEAR, [this](TaskContext context)
                    //{
                    //    SetCombatMovement(false);
                    //})
                    .Schedule(3min, PHASE_BEAR, [this](TaskContext /*context*/)
                    {
                        shapeshifted = false;
                    });
            }
        }

		void JustEngagedWith(Unit* who) override
		{
			scheduler
				.Schedule(5ms, PHASE_COMBAT, [this](TaskContext wrath)
				{
                    DoCastVictim(SPELL_WRATH);
                    wrath.Repeat(2s);
				})
                .Schedule(8s, PHASE_COMBAT, [this](TaskContext moonfire)
                {
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                        DoCast(target, SPELL_MOONFIRE);
                    moonfire.Repeat(8s, 12s);
                })
                .Schedule(3s, PHASE_HEALING, [this](TaskContext nature_swiftness)
                {
                    if (!me->HasAura(SPELL_NATURE_SWIFTNESS))
                    {
                        me->CastStop();
                        DoCastSelf(SPELL_NATURE_SWIFTNESS);
                        nature_swiftness.Repeat(8s, 12s);
                    }
                    else
                    {
                        nature_swiftness.Repeat(2s);
                    }
                })
                .Schedule(10s, 15s, PHASE_HEALING, [this](TaskContext wild_growth)
                {
                    me->CastStop();
                    DoCastAOE(SPELL_WILD_GROWTH);
                    wild_growth.Repeat(5s, 8s);
                })
                .Schedule(1s, PHASE_HEALING, [this](TaskContext healing_touch)
                {
                    if (Unit* target = DoSelectBelowHpPctFriendly(30.0f, 50, false))
                    {
                        me->CastStop();
                        DoCast(target, SPELL_HEALING_TOUCH, HasNatureSwiftness());
                    }
                    healing_touch.Repeat(500ms);
                })
                .Schedule(1s, PHASE_HEALING, [this](TaskContext lifebloom)
                {
                    if (Unit* target = DoSelectBelowHpPctFriendly(30.0f, 20, false))
                        DoCast(target, SPELL_LIFEBLOOM);
                    lifebloom.Repeat(1s);
                })
                .Schedule(1s, PHASE_HEALING, [this](TaskContext rejuvenation)
                {
                    if (Unit* target = DoFindFriendlyMissingHot(40.0f, SPELL_REJUVENATION))
                        DoCast(target, SPELL_REJUVENATION);
                    rejuvenation.Repeat(1s);
                })
                .Schedule(1s, PHASE_HEALING, [this](TaskContext regrowth)
                {
                    if (Unit* target = DoFindFriendlyMissingHot(40.0f, SPELL_REGROWTH))
                    {
                        me->CastStop();
                        DoCast(target, SPELL_REGROWTH, HasNatureSwiftness());
                    }
                    regrowth.Repeat(1s);
                });
		}

        private:
        bool shapeshifted;

        CastSpellExtraArgs HasNatureSwiftness()
        {
            CastSpellExtraArgs args;
            if (me->HasAura(SPELL_NATURE_SWIFTNESS))
            {
                args.SetTriggerFlags(TRIGGERED_CAST_DIRECTLY);
                me->RemoveAurasDueToSpell(SPELL_NATURE_SWIFTNESS);
            }
            return args;
        }
	};

	CreatureAI* GetAI(Creature* creature) const override
	{
		return new npc_druidAI(creature);
	}
};

class spell_lifebloom : public AuraScript
{
	PrepareAuraScript(spell_lifebloom);

	bool Validate(SpellInfo const* /*spell*/) override
	{
		return ValidateSpellInfo({ SPELL_LIFEBLOOM_FINAL_HEAL });
	}

	void OnRemoveEffect(Unit* target, AuraEffect const* aurEff, uint32 stack)
	{
		CastSpellExtraArgs args(aurEff);
		args.OriginalCaster = GetCasterGUID();
		args.AddSpellBP0(aurEff->GetAmount());
		target->CastSpell(target, SPELL_LIFEBLOOM_FINAL_HEAL, args);
	}

	void AfterRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
	{
		if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
			return;

		OnRemoveEffect(GetUnitOwner(), aurEff, GetStackAmount());
	}

	void HandleDispel(DispelInfo* dispelInfo)
	{
		if (Unit* target = GetUnitOwner())
			if (AuraEffect const* aurEff = GetEffect(EFFECT_1))
				OnRemoveEffect(target, aurEff, dispelInfo->GetRemovedCharges());
	}

	void Register() override
	{
		AfterEffectRemove += AuraEffectRemoveFn(spell_lifebloom::AfterRemove, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
		AfterDispel += AuraDispelFn(spell_lifebloom::HandleDispel);
	}
};

void AddSC_npc_druid()
{
	new npc_druid();

	RegisterSpellScript(spell_lifebloom);
}
