#include "ScriptMgr.h"
#include "Map.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ThreatManager.h"
#include "GridNotifiersImpl.h"
#include "CreatureAIImpl.h"
#include "CellImpl.h"
#include "SpellAuraEffects.h"
#include "CustomAI.h"

#include <iostream>

enum Spells
{
    SPELL_FLASH_HEAL            = 100015,
    SPELL_SMITE                 = 100016,
    SPELL_POWER_WORD_SHIELD     = 100017,
    SPELL_HOLY_FIRE             = 100018,
    SPELL_MASS_HEAL             = 100019,
    SPELL_PAIN_SUPPRESSION      = 100020,
    SPELL_MANA_POTION           = 100022,
    SPELL_HEALTH_POTION         = 100023,
    SPELL_REJUVENATION          = 100024,
    SPELL_DIVINE_AEGIS          = 100063,
    SPELL_PRAYER_OF_HEALING     = 100062,
    SPELL_ETERNAL_AFFECTION     = 100030,
    SPELL_SPIRIT_HEALER_VISUAL  = 70571,
    SPELL_MASS_DISPEL           = 32375,
};

enum Phase
{
    PHASE_HEALING               = 1,
    PHASE_COMBAT,
    PHASE_ENDANGERED
};

enum Misc
{
    // Talk
    SAY_HEAL                    = 0,

    // NPS
    NPC_MEDIC_HELAINA           = 100014,
    NPC_LIGHTWELL               = 100013,
    NPC_SPIRIT_HEALER           = 100015,
    NPC_INVISIBLE_STALKER       = 32780
};

class npc_priest : public CreatureScript
{
    public:
    npc_priest() : CreatureScript("npc_priest") {}

    struct npc_priestAI : public CustomAI
    {
        npc_priestAI(Creature* creature) : CustomAI(creature), hasUsedDamageReduction(false)
        {
        }

        void Initialize() override
        {
            CustomAI::Initialize();

            hasUsedDamageReduction = false;
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            SetInCombatPhase();
        }

        void JustDied(Unit* /*killer*/) override
        {
            // Que pour Medic Helaina
            if (me->GetEntry() != NPC_MEDIC_HELAINA)
                return;

            if (Creature* spiritHealder = DoSummon(NPC_SPIRIT_HEALER, me->GetPosition(), 13s, TEMPSUMMON_TIMED_DESPAWN))
            {
                if (Creature* fx = DoSummon(NPC_INVISIBLE_STALKER, me->GetPosition(), 13s, TEMPSUMMON_TIMED_DESPAWN))
                {
                    fx->AddAura(SPELL_SPIRIT_HEALER_VISUAL, fx);
                    fx->SetObjectScale(1.5f);
                }

                double angle = 72;
                for (int i = 0; i < 5; ++i)
                {
                    Position pos = GetPositionAround(spiritHealder, angle, spiritHealder->GetObjectScale() * 3.f);
                    if (Creature* lightwell = DoSummon(NPC_LIGHTWELL, pos, 13s, TEMPSUMMON_TIMED_DESPAWN))
                        lightwell->CastSpell(lightwell, SPELL_MASS_HEAL);
                    angle += 72;
                }
            }
        }

        void DamageTaken(Unit* /*attacker*/, uint32& /*damage*/) override
        {
            if (!hasUsedDamageReduction && !me->HasAura(SPELL_PAIN_SUPPRESSION) && HealthBelowPct(20))
            {
                scheduler.CancelGroup(PHASE_HEALING);
                scheduler.CancelGroup(PHASE_COMBAT);

                DoCast(SPELL_PAIN_SUPPRESSION);

                hasUsedDamageReduction = true;

                scheduler
                    .Schedule(1s, PHASE_ENDANGERED, [this](TaskContext /*context*/)
                    {
                        Talk(SAY_HEAL);
                        DoCast(SPELL_MANA_POTION);
                        DoCast(SPELL_ETERNAL_AFFECTION);
                    })
                    .Schedule(3s, PHASE_ENDANGERED, [this](TaskContext /*context*/)
                    {
                        SetInCombatPhase();
                    })
                    .Schedule(1min, PHASE_ENDANGERED, [this](TaskContext /*context*/)
                    {
                        hasUsedDamageReduction = false;
                    });
            }
        }

        void SpellHitTarget(WorldObject* target, SpellInfo const* spellInfo) override
        {
            if (Unit* hit = target->ToUnit())
            {
                if (spellInfo->HasEffect(SPELL_EFFECT_HEAL))
                {
                    int32 absorb = CalculatePct(spellInfo->Effects[EFFECT_0].BasePoints, 90);

                    if (Aura* aegis = hit->GetAura(SPELL_DIVINE_AEGIS))
                        absorb = absorb * (aegis->GetStackAmount() + 1);

                    CastSpellExtraArgs args;
                    args.AddSpellBP0(absorb);

                    hit->CastSpell(hit, SPELL_DIVINE_AEGIS, args);
                }
            }
        }

        private:
        bool hasUsedDamageReduction;

        Position GetPositionAround(Unit* target, double angle, float radius)
        {
            double ang = angle * (M_PI / 180);
            Position pos;
            pos.m_positionX = target->GetPositionX() + radius * sin(ang);
            pos.m_positionY = target->GetPositionY() + radius * cos(ang);
            pos.m_positionZ = target->GetPositionZ();
            return pos;
        }

        std::list<Unit*> DoFindFriendlySuffering(float range)
        {
            std::list<Unit*> result;

            // First get all friendly creatures
            std::list<WorldObject*> objects;
            Trinity::AllWorldObjectsInRange check(me, range);
            Trinity::UnitListSearcher<Trinity::AllWorldObjectsInRange> searcher(me, objects, check);
            Cell::VisitGridObjects(me, searcher, range);

            for (WorldObject* object : objects)
            {
                if (Unit* unit = object->ToUnit())
                {
                    DispelChargesList dispelList;
                    unit->GetDispellableAuraList(unit, DISPEL_ALL_MASK, dispelList);
                    if (dispelList.empty())
                        continue;

                    result.push_back(unit);
                }
            }

            return result;
        }

        void SetInCombatPhase()
        {
            scheduler
                .Schedule(5ms, PHASE_HEALING, [this](TaskContext rejuvenation)
                {
                    if (Unit* target = DoSelectBelowHpPctFriendly(30.0f, 80, false))
                    {
                        if (!target->HasAura(SPELL_REJUVENATION))
                        {
                            me->CastStop();
                            DoCast(target, SPELL_REJUVENATION);
                            rejuvenation.Repeat(12s);
                        }
                        else
                        {
                            rejuvenation.Repeat(1s);
                        }
                    }
                    else
                    {
                        rejuvenation.Repeat(1s);
                    }
                })
                .Schedule(5ms, PHASE_HEALING, [this](TaskContext heal)
                {
                    if (Unit* target = DoSelectBelowHpPctFriendly(30.0f, 50, false))
                    {
                        me->CastStop();
                        DoCast(target, SPELL_FLASH_HEAL);
                    }
                    heal.Repeat(500ms);
                })
                .Schedule(20s, 28s, PHASE_HEALING, [this](TaskContext prayer_of_healing)
                {
                    me->CastStop();
                    DoCastAOE(SPELL_PRAYER_OF_HEALING);
                    prayer_of_healing.Repeat(5s, 8s);
                })
                .Schedule(5ms, PHASE_HEALING, [this](TaskContext power_word_shield)
                {
                    if (Unit* target = DoSelectBelowHpPctFriendly(30.0f, 20, false))
                    {
                        me->CastStop();
                        DoCast(target, SPELL_POWER_WORD_SHIELD);
                    }
                    power_word_shield.Repeat(2s);
                })
                .Schedule(5ms, PHASE_COMBAT, [this](TaskContext smite)
                {
                    DoCastVictim(SPELL_SMITE);
                    smite.Repeat(8s);
                })
                .Schedule(8s, PHASE_COMBAT, [this](TaskContext holy_fire)
                {
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                        DoCast(target, SPELL_HOLY_FIRE);
                    holy_fire.Repeat(10s, 20s);
                })
                .Schedule(2s, PHASE_HEALING, [this](TaskContext mass_dispel)
                {
                    std::list<Unit*> targets = DoFindFriendlySuffering(20.0f);
                    if (!targets.empty() && targets.size() > 3)
                    {
                        if (Unit* victim = me->GetVictim())
                        {
                            me->CastStop();
                            me->CastSpell(victim->GetPosition(), SPELL_MASS_DISPEL);
                            mass_dispel.Repeat(25s, 30s);
                        }
                    }
                    else
                    {
                        mass_dispel.Repeat(2s);
                    }
                });
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_priestAI(creature);
    }
};

class spell_power_word_shield : public SpellScriptLoader
{
    public:
    spell_power_word_shield() : SpellScriptLoader("spell_power_word_shield")
    {
    }

    class spell_power_word_shield_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_power_word_shield_SpellScript);

        bool Validate(SpellInfo const* spellInfo) override
        {
            return ValidateSpellInfo({ spellInfo->ExcludeTargetAuraSpell });
        }

        void WeakenSoul()
        {
            if (Unit* target = GetHitUnit())
                GetCaster()->CastSpell(target, GetSpellInfo()->ExcludeTargetAuraSpell, true);
        }

        void Register() override
        {
            AfterHit += SpellHitFn(spell_power_word_shield_SpellScript::WeakenSoul);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_power_word_shield_SpellScript();
    }
};

void AddSC_npc_priest()
{
    new npc_priest();
    new spell_power_word_shield();
}
