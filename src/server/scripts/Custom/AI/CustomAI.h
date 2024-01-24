#ifndef CUSTOM_CUSTOMAI_H
#define CUSTOM_CUSTOMAI_H

#include "Creature.h"
#include "CreatureAI.h"
#include "ScriptedCreature.h"
#include "DBCEnums.h"
#include "TaskScheduler.h"
#include "ScriptMgr.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

#define NPC_TRAINING_DUMMY 87318

enum class AI_Type
{
    None,
    Melee,
    Hybrid,
    Distance,
    Stay
};

class FriendlyMissingBuff
{
    public:
        FriendlyMissingBuff(Unit const* obj, uint32 spellid, float range) : i_obj(obj), i_spell(spellid), f_range(range) { }

        bool operator()(Unit* u) const
        {
            if (Creature* c = u->ToCreature())
            {
                if (c->IsTrigger() || c->GetFaction() == FACTION_FRIENDLY
                    || c->IsCivilian())
                {
                    return false;
                }
            }

            if (u->IsAlive() && !i_obj->IsHostileTo(u) && i_obj->IsWithinDistInMap(u, f_range, false) && !u->HasAura(i_spell))
                return true;

            return false;
        }

    private:
        Unit const* i_obj;
        uint32 i_spell;
        float f_range;
};

class TC_API_EXPORT CustomAI : public ScriptedAI
{
    public:
        CustomAI(Creature* creature, AI_Type type = AI_Type::Distance);
        CustomAI(Creature* creature, bool damageReduction, AI_Type type = AI_Type::Distance);
        virtual ~CustomAI() { }

        virtual void Initialize();

        virtual float GetDistance() { return 40.f; };

        virtual float GetDamageReductionToUnit() { return 0.08f; };
        virtual void DamageFromNPC(Unit* attacker, uint32& damage, DamageEffectType damageType);

        void JustSummoned(Creature* /*summon*/) override;
        void SummonedCreatureDespawn(Creature* /*summon*/) override;
        void SummonedCreatureDies(Creature* /*summon*/, Unit* /*killer*/) override;

        void SpellHit(WorldObject* /*caster*/, SpellInfo const* /*spellInfo*/) override;

        void EnterEvadeMode(EvadeReason why = EvadeReason::Other) override;
        void DamageTaken(Unit* /*attacker*/, uint32& /*damage*/, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override;

        void Reset() override;
        void AttackStart(Unit* /*who*/) override;
        void JustDied(Unit* /*killer*/) override;
        void UpdateAI(uint32 /*diff*/) override;

        bool CanAIAttack(Unit const* /*who*/) const override;
        void CastStop();
        void CastStop(uint32 exception);
        void CastStop(const std::vector<uint32>& exceptions);
        void SetCombatMove(bool on, float distance = 0.0f, bool stopMoving = false, bool force = false);

        void TalkInCombat(uint8 textId, uint64 cooldown = 10);

        std::list<Unit*> DoFindMissingBuff(uint32 spellId);
        Unit* SelectRandomMissingBuff(uint32 spell);

    protected:
        TaskScheduler scheduler;
        SummonList summons;
        AI_Type type;
        uint8 interruptCounter;
        bool canCombatMove;
        bool damageReduction;
        bool textOnCooldown;

        uint32 EnemiesInRange(float distance);
        uint32 EnemiesInFront(float distance);

        bool HasMechanic(SpellInfo const* spellInfo, Mechanics mechanic);
        bool ShouldTakeDamage();
};

#endif // CUSTOM_CUSTOMAI_H
