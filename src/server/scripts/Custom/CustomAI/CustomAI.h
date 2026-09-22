#ifndef CUSTOM_CUSTOMAI_H
#define CUSTOM_CUSTOMAI_H

#include "Custom/FakeParty/FakeParty.h"
#include "Creature.h"
#include "ScriptedCreature.h"
#include "TaskScheduler.h"
#include "SpellInfo.h"
#include "Random.h"
#include <cmath>

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
        void CastStop(const std::unordered_set<uint32>& /*exceptions*/);

        //
        void StartFakeParty(Player* /*player*/);
        void StopFakeParty();

        void TalkInCombat(uint8 textId, Seconds cooldown = 10s);

        // Autorise la creature a faire des coups critiques avec ses sorts et fixe
        // sa chance de base. Le core les interdit aux PNJ (Unit::SpellCritChanceDone)
        // sauf s'ils portent le string id "can_spell_crit" : s'il manque en DB, on le
        // pose dans le slot Script (ecrase donc un eventuel SetScriptStringId).
        void SetSpellCritChance(float chance);

        void MovementInform(uint32 /*type*/, uint32 /*id*/) override;

        std::list<Unit*> DoFindMissingBuff(uint32 /*spellId*/, float /*range*/ = 40.0f);
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

        // -------------------------------------------------------------------------
        // Encerclement (anti-melee)
        // -------------------------------------------------------------------------
        // Detecte quand un caster distance est entoure de melee sans arc de fuite
        // exploitable, et declenche une reaction (sort AOE/blink cote sous-classe,
        // ou fuite generique sinon).

        // Hook appele une fois quand l'unite est detectee encerclee. Retourne true
        // si la sous-classe a gere (ex: Frost Nova + Blink), false pour utiliser
        // le fallback PanicFlee.
        virtual bool OnEncircled(Unit* /*victim*/) { return false; }

        // Hook appele en dernier recours quand meme la fuite n'a pas de direction
        // exploitable (ennemis et murs partout). La sous-classe peut switcher en
        // sorts instantanes, lancer un AOE auto-defense, etc.
        virtual void OnCornered(Unit* /*victim*/) { }

        // Distance et nombre minimal d'ennemis pour considerer l'encerclement.
        // Surchargeables par les sous-classes pour ajuster la sensibilite.
        virtual float GetEncircleRadius() const { return 8.f; }
        virtual uint32 GetEncircleMinEnemies() const { return 3; }

        // Vrai si >= GetEncircleMinEnemies() ennemis dans le radius ET qu'il
        // n'existe aucun arc libre de >= minClearArc radians (par defaut 150°).
        bool IsEncircled(float minClearArc = float(5.0 * M_PI / 6.0)) const;

        // Centre du plus grand arc vide d'ennemis, projete a `distance` via
        // MovePositionToFirstCollision. Retourne la position courante si aucun
        // ennemi n'est a portee.
        Position GetBestEscapePosition(float distance) const;

        // Interrompt le cast, MovePoint vers la direction de fuite optimale,
        // suspend le circle-kite. Si aucun arc viable: appelle OnCornered.
        void PanicFlee(float distance = 20.f);

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
        bool encircleReactOnCooldown;
        float circleAngle;

        // Schedule la boucle de detection d'encerclement (Distance/Hybrid).
        void ScheduleEncircleCheck();

        // Interrompt les sorts non-melee en cours dont l'ID ne satisfait pas le prédicat.
        void CastStopIf(const std::function<bool(uint32)>& isException);

        uint32 FriendsInRange(float distance, uint8 pct);
        uint32 FriendsInFront(float distance, uint8 pct);
        uint32 EnemiesInRange(float distance);
        uint32 EnemiesInFront(float distance);

        bool HasMechanic(SpellInfo const* spellInfo, Mechanics mechanic);

        enum MovementInformId : uint32
        {
            Jump        = 2500000,
            Move        = 2500001,
            Backped     = 2500002,
        };

        enum SchedulerGroup : uint32
        {
            RandomMovement  = 9001,
            TeleportSettle  = 9002,
            Encircle        = 9003,
        };

        static constexpr float JUMP_SPEED = 6.f;
        static constexpr float JUMP_HEIGHT = 1.5f;
        static constexpr float JUMP_DISTANCE = 5.f;
        static constexpr float JUMP_BACK_HEIGHT = 1.8f;
        static constexpr float JUMP_BACK_DISTANCE = 2.5f;
};

struct FriendlyInFront
{
    Unit const* me;
    float range;
    uint8 pct;

    FriendlyInFront(Unit const* me, float range, uint8 pct)
        : me(me), range(range), pct(pct) {
    }

    bool operator()(Unit* u) const
    {
        return u->IsAlive() && u->IsWithinDist(me, range)
            && u->HealthBelowPct(pct) && me->isInFront(u)
            && me->IsValidAssistTarget(u);
    }
};

// ===========================================================================
// Positions aleatoires
// ===========================================================================
// Les trois helpers delegent le calcul au core plutot que de refaire la
// trigonometrie a la main. On y gagne, gratuitement et partout :
//   - le Z accroche au terrain (UpdateAllowedPositionZ : gere le vol, le
//     hover et les liquides, la ou UpdateGroundPositionZ colle au sol) ;
//   - la normalisation des coordonnees de carte ;
//   - les collisions statiques (vmaps) et dynamiques (gameobjects), avec
//     recul de CONTACT_DISTANCE au point d'impact ;
//   - un repli sur la hauteur de grille quand il n'y a pas de sol dessous.
//
// Ce que le core ne fait pas et qu'on garde ici : l'orientation tournee vers
// le centre, et une garde sur les pointeurs nuls.
//
// ATTENTION AUX ANGLES : les angles de CES helpers sont ABSOLUS, alors que
// MovePosition / MovePositionToFirstCollision ajoutent l'orientation de
// l'objet au leur. On la retranche donc avant l'appel. Sans ca, un cercle
// construit par pas reguliers (slice * index) tournerait avec le personnage.

// Point sur un cercle de rayon `radius` autour de `target`, a un angle ABSOLU
// impose, en s'arretant a la premiere collision rencontree en chemin.
inline Position GetRandomPositionAroundCircle(WorldObject const* target, float angle, float radius)
{
    if (!target)
        return Position();

    Position result = target->GetPosition();
    if (radius <= 0.0f)
        return result;

    target->MovePositionToFirstCollision(result, radius, angle - target->GetOrientation());
    result.SetOrientation(result.GetAbsoluteAngle(target));

    return result;
}

// Position aleatoire autour de `target`, en s'arretant a la premiere collision.
//   fill = true  : tirage uniforme dans le disque de rayon `dist` (la racine
//                  carree sur le rayon evite l'agglutinement au centre)
//   fill = false : sur le cercle exact de rayon `dist`
inline Position GetRandomPosition(WorldObject const* target, float dist, bool fill = true)
{
    if (!target)
        return Position();

    float const angle = frand(0.0f, 2.0f * float(M_PI));
    float const radius = fill ? dist * std::sqrt(float(rand_norm())) : dist;

    return GetRandomPositionAroundCircle(target, angle, radius);
}

// Position aleatoire dans le disque de rayon `radius` autour d'un centre libre,
// avec une distance minimale optionnelle.
//
// `reference` n'est pas le centre : c'est l'objet qui sert a resoudre le
// terrain et la phase (n'importe quel objet deja present sur la bonne carte -
// le lanceur, le joueur teleporte, me...). Sans lui, impossible d'accrocher le
// Z au sol : c'est exactement ce qui manquait a l'ancienne version, qui
// recopiait le Z du centre pour tout le monde.
inline Position GetRandomPosition(WorldObject const* reference, Position const& center, float radius, float minRadius = 0.0f)
{
    if (!reference)
        return center;

    Position result = reference->GetRandomPoint(center, radius, minRadius);
    result.SetOrientation(result.GetAbsoluteAngle(center));

    return result;
}

inline void FeignDeath(Creature* creature)
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
