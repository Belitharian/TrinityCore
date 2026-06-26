#ifndef SPELL_WARLOCK_H_
#define SPELL_WARLOCK_H_

#include "CreatureAIImpl.h"
#include "SpellAuras.h"
#include "Unit.h"

// 296553 - Wild Imp Aura
static struct spell_wild_imp_aura
{
    static constexpr uint32 WILD_IMP_AURA_STACK = 296553;

public:
    static void AddImp(Unit* owner, Optional<uint8> count = {})
    {
        if (count)
        {
            for (uint8 i = 0; i < *count; i++)
                owner->CastSpell(owner, WILD_IMP_AURA_STACK, true);
        }
        else
            owner->CastSpell(owner, WILD_IMP_AURA_STACK, true);
    }

    static void RemoveImp(Unit* owner, uint8 count)
    {
        if (Aura* aura = owner->GetAura(WILD_IMP_AURA_STACK))
            aura->ModStackAmount(-count);
    }

    static void RemoveImps(Unit* owner)
    {
        owner->RemoveAurasDueToSpell(WILD_IMP_AURA_STACK);
    }
};

#endif SPELL_WARLOCK_H_
