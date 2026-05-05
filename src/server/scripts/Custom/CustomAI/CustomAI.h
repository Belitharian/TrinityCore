#ifndef CUSTOM_CUSTOMAI_H
#define CUSTOM_CUSTOMAI_H

#include "Custom/FakeParty/FakeParty.h"
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

        virtual float GetDistance() { return 15.f; };

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
        void CastStop(uint32 /*exception*/);
        void CastStop(const std::vector<uint32>& /*exceptions*/);

        //
        void StartFakeParty(Player* /*player*/);
        void StopFakeParty();

        void TalkInCombat(uint8 textId, Seconds cooldown = 10s);

        void MovementInform(uint32 /*type*/, uint32 /*id*/) override;

        std::list<Unit*> DoFindMissingBuff(uint32 /*spellId*/);
        Unit* SelectRandomMissingBuff(uint32 /*spell*/);

        void SetCanRandomMovement(bool apply) { randomMovements = apply; }
        bool CanRandomMovement() const { return randomMovements; }
        void ScheduleRandomMovements();
        Position GetRandomMovementsPosition();
        Position GetRandomJump();
        Position GetRandomBackStep(float /*distance*/);
        Position GetRandomBackJump();

        // A appeler apres tout teleport / saut volontaire (Blink, leap, ...) :
        // - repousse le scheduler de mouvement aleatoire pour qu'il ne ramene
        //   pas l'unite vers la position d'avant le teleport ;
        // - re-ancre MoveChase sur la nouvelle distance afin que la poursuite
        //   ne tire pas mecaniquement l'unite en arriere. La distance par
        //   defaut (GetDistance()) est restauree apres `settleDuration`.
        void NotifyTeleported(Milliseconds settleDuration = 2s);

        // Hook appele une seule fois quand une unite entre dans la phase de recul
        // (transition false -> true). Override pour reagir a l'entree dans la phase.
        virtual void OnBackpedStart(Unit* /*victim*/) { }

        // Hook appele a chaque tick du scheduler tant que l'unite est en phase de recul.
        // Override pour lancer des sorts instantanes pendant le kite (le cast normal
        // est interrompu par CastStop pendant cette phase).
        virtual void OnBackpedTick(Unit* /*victim*/) { }

        // Hook appele une seule fois quand l'unite sort de la phase de recul
        // (mouvement Backped termine et plus en train de kiter).
        virtual void OnBackpedEnd(Unit* /*victim*/) { }

        bool IsBackpedaling() const { return backpedaling; }

    private:
        // Transition logic pour la phase de recul. EnterBackped declenche
        // OnBackpedStart au passage false->true, sinon OnBackpedTick.
        // ExitBackped declenche OnBackpedEnd au passage true->false.
        void EnterBackped(Unit* victim);
        void ExitBackped(Unit* victim);

    protected:
        TaskScheduler scheduler;
        AI_Type type;
        SummonList summons;
        uint8 interruptCounter;
        FakeParty fakeParty;
        Player* linkedPlayer;
        bool canCombatMove;
        bool damageReduction;
        bool textOnCooldown;
        bool randomMovements;
        bool backpedaling;
        bool circleClockwise;
        float circleAngle;

        uint32 FriendsInRange(float distance, uint8 pct);
        uint32 EnemiesInRange(float distance);
        uint32 EnemiesInFront(float distance);

        bool HasMechanic(SpellInfo const* spellInfo, Mechanics mechanic);

        enum MovementInformId : uint32
        {
            Jump        = 2500000,
            Move        = 2500001,
            Backped     = 2500002,
        };

        // Groupes de scheduler internes a CustomAI. Choisis dans une plage
        // haute pour ne pas entrer en collision avec les enums de groupes
        // definis par les sous-classes (qui partent generalement de 0).
        static constexpr uint32 GROUP_RANDOM_MOVEMENT = 9001;
        static constexpr uint32 GROUP_TELEPORT_SETTLE = 9002;

        static constexpr float JUMP_SPEED = 6.f;

        static constexpr float JUMP_HEIGHT = 1.5f;
        static constexpr float JUMP_DISTANCE = 5.f;

        static constexpr float JUMP_BACK_HEIGHT = 1.8f;
        static constexpr float JUMP_BACK_DISTANCE = 2.5f;
};

inline Position const GetRandomPosition(Position center, float dist)
{
    float alpha = 2 * float(M_PI) * float(rand_norm());
    float r = dist * sqrtf(float(rand_norm()));
    float x = r * cosf(alpha) + center.GetPositionX();
    float y = r * sinf(alpha) + center.GetPositionY();

    Position result = { x, y, center.GetPositionZ(), 0.f };

    float o = result.GetAbsoluteAngle(center);
    result.SetOrientation(o);

    return result;
}

inline Position const GetRandomPosition(Unit* target, float dist, bool fill = true)
{
    // Get center position
    Position center = target->GetPosition();

    // Random angle
    float alpha = 2 * float(M_PI) * float(rand_norm());

    // Random radius
    float r = fill
        ? dist * sqrtf(float(rand_norm()))
        : dist;

    // Move to first collision
    target->MovePositionToFirstCollision(center, r, alpha);

    // Get orientation angle
    float o = center.GetAbsoluteAngle(target);

    // Set final position
    return { center.m_positionX, center.m_positionY, center.m_positionZ, o };
}

inline Position const GetRandomPositionAroundCircle(Unit* target, float angle, float radius)
{
    // Get center position
    const Position center = target->GetPosition();

    // Get X and Y position around the center with radius
    float x = radius * cosf(angle) + center.GetPositionX();
    float y = radius * sinf(angle) + center.GetPositionY();

    // Get height map Z position
    float z = center.GetPositionZ();

    Trinity::NormalizeMapCoord(x);
    Trinity::NormalizeMapCoord(y);
    target->UpdateGroundPositionZ(x, y, z);

    // Get orientation angle
    const Position position = { x, y, z };
    float o = position.GetAbsoluteAngle(center);

    // Set final position
    return { x, y, z, o };
}

inline void FeingDeath(Creature* creature)
{
    creature->RemoveAllAuras();
    creature->SetRegenerateHealth(false);
    creature->SetHealth(0U);
    creature->SetStandState(UNIT_STAND_STATE_DEAD);
    creature->SetUnitFlag2(UNIT_FLAG2_FEIGN_DEATH);
    creature->SetUnitFlag2(UNIT_FLAG2_PLAY_DEATH_ANIM);
    creature->SetImmuneToAll(true);
}

#endif // CUSTOM_CUSTOMAI_H
