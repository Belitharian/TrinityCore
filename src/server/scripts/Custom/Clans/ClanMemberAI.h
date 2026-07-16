/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// IA d'un membre de clan. Combine une IA a besoins (utility) et un cerveau
// Q-learning : a chaque tick de decision, l'agent construit son etat, laisse le
// cerveau choisir une action, l'execute dans le monde (chasse reelle, boire,
// dormir, chercher un partenaire), puis apprend de la recompense obtenue.

#ifndef CUSTOM_CLANS_CLANMEMBERAI_H
#define CUSTOM_CLANS_CLANMEMBERAI_H

#include "ClanDefines.h"
#include "ClanMind.h"
#include "ObjectGuid.h"
#include "ScriptedCreature.h"
#include "TaskScheduler.h"

namespace Clan { struct MemberState; }

struct npc_clan_member : public ScriptedAI
{
    explicit npc_clan_member(Creature* creature);

    void JustAppeared() override;
    void Reset() override;
    void UpdateAI(uint32 diff) override;
    void MovementInform(uint32 type, uint32 id) override;
    void JustDied(Unit* killer) override;

    // Reflexes de combat face aux predateurs.
    void JustEngagedWith(Unit* who) override;
    void DamageTaken(Unit* attacker, uint32& damage, DamageEffectType damageType, SpellInfo const* spellInfo) override;

    // Lie explicitement un etat (utilise par ClanMgr pour les nouveau-nes).
    void BindState(Clan::MemberState* state);
    Clan::MemberState* GetState() const { return _owner; }

    // Reproduction en deux temps : l'initiateur (celui qui a choisi SeekMate) demande
    // au partenaire de venir le rejoindre a un point de rencontre. Les deux marchent
    // l'un vers l'autre ; l'accouplement ne demarre qu'une fois reunis.
    //  - ApproachMate : le partenaire met en pause sa vie et marche vers le point de RV.
    //  - ReleaseMate  : le partenaire est libere (accouplement fini ou abandonne) et reprend sa vie.
    void ApproachMate(ObjectGuid initiator, Position const& meetPos);
    void ReleaseMate();

    // Accesseurs de debug (commandes .clan info / .clan hud).
    bool HasRawFood() const { return _hasRawFood; }
    bool HasWood() const { return _hasWood; }
    bool HasStone() const { return _hasStone; }
    Clan::ActionType CurrentAction() const { return _currentAction; }
    Clan::MindState CurrentMindState() const; // = BuildState() (etat courant percu)

private:
    // Reflexe en cours (preempte la boucle de decision).
    enum class Reflex : uint8 { None, Defend, Flee };

    void DecisionTick();
    void BeginAction(Clan::ActionType action);
    // Calcule la recompense (delta de besoin + bonus de shaping) et apprend.
    void FinishAction(bool reachedGoal, float shapedReward = 0.0f);
    void ResetActionState();
    void SetFacingAction();

    // Amorces d'action (retournent false si l'action ne peut pas demarrer).
    bool StartHunt();
    bool StartDrink(Clan::ResourceType type);
    bool StartSleep();
    bool StartSeekMate();
    bool StartGatherWood();
    bool StartMineRock();
    bool StartLightFire();
    bool StartCook();
    void StartWander();
    // Choisit une destination d'errance en espace ouvert (raycast anti-mur) : evite de
    // viser un point colle a un mur ou dans un recoin trop etroit.
    Position PickWanderDestination();
    bool StartSeekDoctor();
    bool StartHuntPredator(); // traquer un animal sauvage pour l'exterminer
    bool StartRemember();     // se recueillir sur la tombe d'un ancetre (tradition)

    // Reflexes predateurs.
    void OnThreat(Unit* attacker);
    void EndReflex();

    // Applique une maladie aleatoire
    bool SetRandomDeceased(Clan::AfflictionType type, float chance);

    // Joue l'effet declare dans ActionFx pour l'action courante
    void PlayCustomEmote();
    void CastCustomSpell();
    void CastCustomSpellTarget();
    void _CastCustomSpell(Creature* creature) const;

    // Spawn une tombe quand le personnage meur
    void SpawnGravestone();

    Clan::MindState BuildState() const;
    static bool IsNightNow();

    Clan::MemberState* _owner;
    TaskScheduler      _scheduler;

    Clan::ActionType _currentAction;
    Clan::MindState  _decisionState;   // etat au moment du choix (pour l'apprentissage)
    Clan::NeedType   _targetNeed;      // besoin vise (mesure de la recompense)
    float            _needBefore;      // niveau du besoin vise avant l'action
    ObjectGuid       _actionTarget;    // proie / partenaire / gameobject / agresseur cible
    uint32           _huntTimerMs;     // garde-fou anti-chasse infinie
    bool             _busy;            // une action est en cours
    Reflex           _reflex;          // reflexe predateur en cours

    // Inventaire (en memoire, non persiste) pour la chaine cuisson / rallumage.
    bool _hasRawFood;
    bool _hasWood;
    bool _hasStone;

    uint32 _talkCdMs;       // cooldown de parole (anti-spam des phrases)
    uint32 _starveTimerMs;  // accumulateur vers le prochain tick de degats de faim
    uint32 _diseaseTimerMs; // accumulateur vers le prochain tirage de contagion
    uint32 _mateWaitMs;     // temps restant d'attente que le partenaire rejoigne le point de RV
    uint32 _rememberCdMs;   // cooldown avant qu'un nouveau recueillement soit recompense (anti-farm)
    bool   _combatLearned;  // le choix defendre/fuir courant est un choix appris (adulte)
};

inline Position GetFacingPosition(Position position, float range = 2.5f)
{
    // Recupere l'orientation du feu
    float orientation = position.GetOrientation();

    // Calcule la position a #range m devant la position selon son orientation
    float x = position.m_positionX + cos(orientation) * range;
    float y = position.m_positionY + sin(orientation) * range;
    float z = position.m_positionZ; // Garde la meme altitude

    return Position(x, y, z);
}

inline Position GetFacingPosition(WorldObject* object, float range = 2.5f)
{
    return GetFacingPosition(object->GetPosition(), range);
}

#endif // CUSTOM_CLANS_CLANMEMBERAI_H
