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
    Hybrid,
    Distance,
};

class TC_API_EXPORT CustomAI : public ScriptedAI
{
    public:
        CustomAI(Creature* creature, AI_Type type = AI_Type::Distance);
        virtual ~CustomAI() { }

        virtual void Initialize();

        virtual float GetDistance() { return 20.f; };

        void JustSummoned(Creature* /*summon*/) override;
        void SummonedCreatureDespawn(Creature* /*summon*/) override;
        void SummonedCreatureDies(Creature* /*summon*/, Unit* /*killer*/) override;

        void SpellHit(WorldObject* /*caster*/, SpellInfo const* /*spellInfo*/) override;

        void EnterEvadeMode(EvadeReason why = EvadeReason::Other) override;

        void Reset() override;
        void AttackStart(Unit* /*who*/) override;
        void JustDied(Unit* /*killer*/) override;
        void UpdateAI(uint32 /*diff*/) override;

        bool CanAIAttack(Unit const* /*who*/) const override;
        void CastStop();
        void CastStop(uint32 exception);
        void CastStop(const std::vector<uint32>& exceptions);
        void SetCombatMove(bool on, float distance = 0.0f, bool stopMoving = false);

        std::list<Unit*> DoFindMissingBuff(uint32 spellId);

    protected:
        TaskScheduler scheduler;
        SummonList summons;
        AI_Type type;
        uint8 interruptCounter;
        bool canCombatMove;
        bool wasInterrupted;

        uint32 EnemiesInRange(float distance);
        uint32 EnemiesInFront(float distance);

        bool HasMechanic(SpellInfo const* spellInfo, Mechanics mechanic);
};

#endif // CUSTOM_CUSTOMAI_H
