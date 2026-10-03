#ifndef PET_SPAWN_CAST_H
#define PET_SPAWN_CAST_H

#include "Creature.h"
#include "ObjectAccessor.h"
#include "SpellDefines.h"

// Un sort lance des l'apparition (IsSummonedBy, JustAppeared, InitializeAI) part avant la creation
// de l'invocation, envoyee aux clients en fin de tick de la map : le client l'ignore et le visuel
// n'apparait pas. On attend un tick.
inline void CastAfterSpawn(Creature* me, ObjectGuid const& targetGuid, uint32 spellId, CastSpellExtraArgs const& args = {})
{
    me->m_Events.AddEventAtOffset([me, targetGuid, spellId, args]
    {
        Unit* target = ObjectAccessor::GetUnit(*me, targetGuid);
        if (target && target->IsAlive())
            me->CastSpell(target, spellId, args);
    }, 100ms);
}

#endif
