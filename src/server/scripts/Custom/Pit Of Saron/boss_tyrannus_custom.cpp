#include "ScriptMgr.h"
#include "InstanceScript.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "pit_of_saron.h"
#include "Player.h"
#include "PlayerAI.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "Vehicle.h"

enum TyrannusPhases
{
    PHASE_NONE,
    PHASE_INTRO,
};

struct boss_tyrannus_custom : public BossAI
{
    boss_tyrannus_custom(Creature* creature) : BossAI(creature, DATA_TYRANNUS)
    {
    }

    void InitializeAI() override
    {
        if (instance->GetBossState(DATA_TYRANNUS) != DONE)
            Reset();
        else
            me->DespawnOrUnsummon();
    }

    void Reset() override
    {
        me->SetReactState(REACT_PASSIVE);
        me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
        instance->SetBossState(DATA_TYRANNUS, NOT_STARTED);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
    }

    void AttackStart(Unit* victim) override
    {
        if (me->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE))
            return;

        if (victim && me->Attack(victim, true) && !events.IsInPhase(PHASE_INTRO))
            me->GetMotionMaster()->MoveChase(victim);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {

    }

    void KilledUnit(Unit* /*victim*/) override
    {

    }

    void JustDied(Unit* /*killer*/) override
    {
        instance->SetBossState(DATA_TYRANNUS, DONE);

        // Prevent corpse despawning
        if (TempSummon* summ = me->ToTempSummon())
            summ->SetTempSummonType(TEMPSUMMON_DEAD_DESPAWN);
    }

    void DoAction(int32 /*actionId*/) override
    {

    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim() && !events.IsInPhase(PHASE_INTRO))
            return;

        events.Update(diff);

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                
            }

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }
    }
};

void AddSC_boss_tyrannus_custom()
{
    RegisterPitOfSaronCustomCreatureAI(boss_tyrannus_custom);
}
