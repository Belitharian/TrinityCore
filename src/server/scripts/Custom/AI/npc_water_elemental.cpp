#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "CustomAI.h"

enum Spells
{
	SPELL_WATERBOLT			= 100046
};

class npc_water_elemental : public CreatureScript
{
    public:
    npc_water_elemental() : CreatureScript("npc_water_elemental")
    {
    }

    struct npc_water_elementalAI : public CustomAI
    {
        npc_water_elementalAI(Creature* creature) : CustomAI(creature)
        {
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            scheduler.Schedule(1s, [this](TaskContext waterbolt)
            {
                DoCastVictim(SPELL_WATERBOLT);
                waterbolt.Repeat(1s);
            });
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_water_elementalAI(creature);
    }
};

void AddSC_npc_water_elemental()
{
    new npc_water_elemental();
}
