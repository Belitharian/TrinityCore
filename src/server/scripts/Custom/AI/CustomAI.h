#ifndef CUSTOM_CUSTOMAI_H
#define CUSTOM_CUSTOMAI_H

#include "Creature.h"
#include "CreatureAI.h"
#include "ScriptedCreature.h"
#include "DBCEnums.h"
#include "TaskScheduler.h"

enum class AI_Type
{
    None,
    Melee,
    Distance
};

class TC_API_EXPORT CustomAI : public ScriptedAI
{
    public:
        CustomAI(Creature* creature, AI_Type type = AI_Type::Distance);
        virtual ~CustomAI() { }

        void ReleaseFocus();

        virtual void Initialize();

        virtual float GetDistance() { return 20.f; };

        void JustSummoned(Creature* /*summon*/) override;
        void SummonedCreatureDespawn(Creature* /*summon*/) override;
        void SummonedCreatureDies(Creature* /*summon*/, Unit* /*killer*/) override;

        void EnterEvadeMode(EvadeReason why = EVADE_REASON_OTHER) override;

        void Reset() override;
        void AttackStart(Unit* who) override;
        void JustEngagedWith(Unit* /*who*/) override;
        void JustExitedCombat() override;
        void JustDied(Unit* /*killer*/) override;
        void UpdateAI(uint32 diff) override;

        bool CanAIAttack(Unit const* who) const override;

    protected:
        TaskScheduler scheduler;
        SummonList summons;
        AI_Type type;
};

#endif // CUSTOM_CUSTOMAI_H
