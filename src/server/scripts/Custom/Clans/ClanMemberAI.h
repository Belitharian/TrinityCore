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
#include <array>
#include <vector>

class GameObject;
namespace Clan { struct MemberState; struct HouseState; class ClanRole; }

struct npc_clan_member : public ScriptedAI
{
    explicit npc_clan_member(Creature* creature);

    void JustAppeared() override;
    void Reset() override;
    void UpdateAI(uint32 diff) override;
    void WaypointReached(uint32 waypointId, uint32 pathId) override;
    void WaypointPathEnded(uint32 waypointId, uint32 pathId) override;
    void MovementInform(uint32 type, uint32 id) override;
    void JustDied(Unit* killer) override;

    // Reflexes de combat face aux predateurs.
    void JustEngagedWith(Unit* who) override;
    void DamageTaken(Unit* attacker, uint32& damage, DamageEffectType damageType, SpellInfo const* spellInfo) override;

    // Lie explicitement un etat (utilise par ClanMgr pour les nouveau-nes).
    void BindState(Clan::MemberState* state);
    // Detache l'IA de son etat AVANT que celui-ci ne soit detruit (.clan reset). Annule
    // toutes les taches planifiees : plusieurs d'entre elles dereferencent _owner (timer de
    // cuisson, d'accouplement...), et se declencheraient sur un pointeur mort.
    void UnbindState();
    Clan::MemberState* GetState() const { return _owner; }

    // Reproduction en deux temps : l'initiateur (celui qui a choisi SeekMate) demande
    // au partenaire de venir le rejoindre a un point de rencontre. Les deux marchent
    // l'un vers l'autre ; l'accouplement ne demarre qu'une fois reunis.
    //  - ApproachMate : le partenaire met en pause sa vie et marche vers le point de RV.
    //  - ReleaseMate  : le partenaire est libere (accouplement fini ou abandonne) et reprend sa vie.
    void ApproachMate(ObjectGuid initiator, Position const& meetPos);
    void ReleaseMate();
    // Le partenaire signale a l'initiateur qu'il est arrive au point de rencontre (evenementiel,
    // remplace le sondage de distance). L'initiateur accouple des que lui ET le partenaire sont la.
    void NotifyMateArrived(ObjectGuid mate);

    // Accesseurs de debug (commandes .clan info / .clan hud).
    // Quantite portee d'un type de ressource (0 si rien).
    uint32 GetItemCount(Clan::ItemType type) const;
    bool HasRawFood() const { return GetItemCount(Clan::ItemType::RawFood) > 0; }
    bool HasWood() const { return GetItemCount(Clan::ItemType::Wood) > 0; }
    bool HasStone() const { return GetItemCount(Clan::ItemType::Stone) > 0; }
    Clan::ActionType CurrentAction() const { return _currentAction; }
    Clan::MindState CurrentMindState() const; // = BuildState() (etat courant percu)
    // Role metier courant (homme/femme/enfant), derive du sexe + de l'etape de vie. Utilise
    // par le debug/monitor pour interroger la Q-table avec le bon gating d'actions.
    Clan::ClanRole const* GetRole() const;

    // Debug (.clan force) : interrompt l'action en cours et execute IMMEDIATEMENT l'action
    // demandee, en court-circuitant la selection Q-learning. Retourne true si l'action a
    // reellement demarre (prerequis reunis), false sinon. A n'utiliser qu'en test.
    bool ForceAction(Clan::ActionType action);

private:
    // Reflexe en cours (preempte la boucle de decision).
    enum class Reflex : uint8 { None, Defend, Flee };

    void DecisionTick();
    // Demarre une action. Retourne true si elle a pu s'engager, false sinon (prerequis manquants).
    bool BeginAction(Clan::ActionType action);
    // Calcule la recompense (delta de besoin + bonus de shaping) et apprend.
    void FinishAction(bool reachedGoal, float shapedReward = 0.0f);
    void ResetActionState();
    void SetFacingAction();
    void SetFacingAction(Position const& point);

    // Amorces d'action (retournent false si l'action ne peut pas demarrer).
    bool StartHunt();
    bool StartDrink();
    bool StartSleep();
    bool StartSeekMate();
    // Declenche l'accouplement des que l'initiateur ET le partenaire sont arrives au RV
    // (appele sur les deux evenements d'arrivee). Ne fait rien tant que l'un des deux manque.
    void TryBeginMating();
    bool StartGatherWood();
    bool StartMineRock();
    bool StartLightFire();
    bool StartCook();
    void StartWander();
    // Choisit une destination d'errance en espace ouvert (raycast anti-mur) : evite de
    // viser un point colle a un mur ou dans un recoin trop etroit.
    Position PickWanderDestination();
    // Destination aleatoire dans un rayon autour du foyer (jeu/errance des enfants) ; si le
    // membre est deja au-dela du rayon, renvoie le foyer lui-meme (il rentre).
    Position HomeAnchoredDestination(float radius) const;

    // --- Suivi de route (custom_clan_path) --------------------------------------------
    // Emprunte la route declaree pour l'action courante via un WaypointMovementGenerator.
    // Tous les noeuds etant sans delai et de meme MoveType, BuildSegments() les fusionne en
    // UN segment continu -> trajet fluide, sans arret ni virage anguleux aux noeuds, et le
    // pathfinding reste actif entre eux.
    //
    // Retourne false si aucune route ne s'applique : l'appelant doit alors faire son
    // MovePoint habituel (fallback standard).
    //
    // La route s'arrete a son point de SORTIE, pas sur 'dest' : c'est WaypointPathEnded qui
    // relance ensuite le MovePoint d'origine avec 'pointId', de sorte que le switch de
    // MovementInform se declenche normalement et reste inchange.
    bool MoveAlongRoad(Position const& dest, uint32 pointId, bool walk = true);
    void BeginRoadTravel(); // phase 2 : longer la route (declenche sur MOVE_TO_ROAD_ENTRY)
    void ClearRoad();       // annule la route ET la retire de MOTION_SLOT_DEFAULT

    bool StartSeekDoctor();
    bool StartHuntPredator(); // traquer un animal sauvage pour l'exterminer
    bool StartRemember();     // se recueillir sur la tombe d'un ancetre (tradition)
    // Actions "realisme" (roles + stock de maison).
    bool StartEat();          // rentrer manger un repas du stock (tout membre affame)
    bool StartShopping();     // (femmes) aller chez le vendeur, rapporter des repas au stock
    bool StartPlay();         // (enfants) jouer / explorer pres de la maison
    // Rentrer livrer TOUT le sac au stock de la maison. Action de plein droit (StoreHome) :
    // tant qu'elle etait une branche cachee de Hunt/GatherWood/MineRock, une recolte pouvait
    // rester bloquee a vie dans un sac plein des que la Q-valeur de l'action porteuse baissait.
    bool StartStoreHome();
    // Met le membre en route vers sa maison pour y deposer sa recolte. Se replie sur le point
    // de foyer (_owner->home) si le GameObject maison est introuvable : un membre doit
    // TOUJOURS pouvoir livrer, sans quoi il accumule les echecs et n'ose plus rien recolter.
    bool GoDepositHome();

    // Maison du membre (etat + GameObject dans le monde). nullptr si non declaree / absente.
    Clan::HouseState* MyHouse() const;
    GameObject* MyHouseObject() const;

    // Reflexes predateurs.
    void OnThreat(Unit* attacker);
    void EndReflex();

    // Applique une maladie aleatoire
    bool SetRandomDeceased(Clan::AfflictionType type, float chance);

    // Joue l'effet declare dans ActionFx pour l'action courante
    void PlayCustomFx();
    void PlayCustomFxTarget();
    void _PlayCustomFx(Creature* creature);

    void ResetEquipment();

    void CastSpellTarget(Creature* target);

    // Spawn une tombe quand le personnage meur
    void SpawnGravestone();

    // Repose le dormeur au sol (reactive la gravite) s'il avait ete monte sur un lit.
    // Appele a tout point de sortie du sommeil (reveil OU interruption par un predateur),
    // sinon il resterait a flotter en l'air.
    void RestoreSleepPosture();

    // --- Inventaire ---
    bool IsItemFull(Clan::ItemType type) const;                    // capacite atteinte ?
    bool AddItem(Clan::ItemType type, uint32 count = 1);           // false si plein
    bool ConsumeItem(Clan::ItemType type, uint32 count = 1);       // false si quantite insuffisante
    // Depose TOUT ce qu'on porte de ce type au stock de la maison (borne par la place
    // restante). Retourne la quantite reellement deposee. Jamais de perte d'item.
    uint32 DepositCarried(Clan::ItemType type);
    // Vide le sac ENTIER au stock de la maison (les trois types en un seul voyage) et renvoie
    // la recompense de depot ponderee par la rarete de ce qui a ete depose ; 0 si rien n'a pu
    // l'etre. Abandonne au passage ce qui ne rentre plus dans un stock deja plein, faute de quoi
    // un sac plein + un stock plein bouclerait indefiniment sur le retour au foyer.
    float DepositAllCarried();
    // Au moins un type est porte au maximum (= il faut rentrer livrer).
    bool IsBagFull() const;
    // Multiplicateur de RARETE d'une ressource pour le foyer : 1.0 quand la maison n'en a pas,
    // decroissant jusqu'a REWARD_SCARCITY_FLOOR une fois HOUSE_STOCK_COMFORT atteint. Sert a
    // mieux payer ce qui MANQUE, pour que les hommes varient leurs taches d'eux-memes.
    float ScarcityMult(Clan::ItemType type) const;

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

    // Mouvement a rejouer une fois la route parcourue (0 = aucune route en cours). La route
    // s'arretant a son point de sortie, c'est WaypointPathEnded qui conduit le PNJ jusqu'a sa
    // destination reelle avec l'id d'origine.
    uint32                _roadPendingId;
    Position              _roadPendingDest;
    // Noeuds de la route, retenus entre la phase 1 (rejoindre l'entree, avec pathfinding) et
    // la phase 2 (parcourir la route en spline exacte).
    std::vector<Position> _roadNodes;
    // Allure choisie a l'appel de MoveAlongRoad. Doit etre retenue : les 3 phases sont
    // desynchronisees dans le temps et un changement d'allure entre elles ferait relancer la
    // spline (MOVEMENTGENERATOR_FLAG_SPEED_UPDATE_PENDING) en plein trajet.
    bool                  _roadWalk;
    // Garde-fou GENERIQUE : temps ecoule depuis le debut de l'action en cours. Si un
    // MovementInform n'arrive jamais (navmesh, GameObject despawne), _busy resterait vrai a vie
    // et le membre serait gele pour de bon. Au-dela de ACTION_TIMEOUT_MS on solde en echec.
    uint32           _actionTimerMs;
    // Vrai des que la proie est abattue (entre le tir et le prelevement sur la depouille).
    // Une chasse ainsi "engagee" ne doit plus etre gaspillee : le garde-fou de temps
    // recupere la viande au lieu d'echouer, sinon le PNJ tue une proie sans jamais manger.
    bool             _huntPreyKilled;
    // L'action en cours a-t-elle reellement demarre ? Distingue "conditions non reunies" (rien
    // n'a ete tente -> REWARD_UNAVAILABLE) de "engagee puis ratee" (du temps perdu -> REWARD_FAIL).
    bool             _actionEngaged;
    bool             _busy;            // une action est en cours
    Reflex           _reflex;          // reflexe predateur en cours
    Position         _gravePoint;      // point de la tombe d'un ancetre dans le cimetiere

    // Inventaire (en memoire, non persiste) : quantite portee par type de ressource.
    // Sert la chaine cuisson / rallumage et permet de faire des reserves.
    std::array<uint32, uint8(Clan::ItemType::Count)> _inventory;

    uint32 _talkCdMs;       // cooldown de parole (anti-spam des phrases)
    uint32 _starveTimerMs;  // accumulateur vers le prochain tick de degats de faim
    uint32 _diseaseTimerMs; // accumulateur vers le prochain tirage de contagion
    uint32 _rememberCdMs;   // cooldown avant qu'un nouveau recueillement soit recompense (anti-farm)
    uint32 _shopCdMs;       // cooldown avant de refaire les courses (anti-farm)
    // Anti-boucle : derniere action soldee et nombre de fois qu'elle s'est enchainee a
    // l'identique. Au-dela de REPEAT_TOLERANCE, la recompense recoit un malus croissant
    // plafonne : le membre se lasse et va voir ailleurs, sans que l'action soit condamnee.
    // Volontairement NON persiste : c'est une humeur de session, pas un acquis.
    Clan::ActionType _lastAction;
    uint8            _repeatStreak;
    bool   _equipDirty;     // un equipement d'action est affiche : il faudra le reposer
    bool   _sleepElevated;  // le dormeur a ete monte sur un lit (gravite coupee) : a redescendre
    bool   _combatLearned;  // le choix defendre/fuir courant est un choix appris (adulte)
    // Derniere etape de vie pour laquelle la Q-table a ete amorcee (SeedTopUp). Quand l'age
    // change de categorie (enfant->adulte...), le role change : on re-amorce les nouvelles actions.
    Clan::LifeStage _lastSeededStage;
    // Rendez-vous d'accouplement (evenementiel) : chacun bascule a true a l'arrivee du
    // partenaire concerne ; TryBeginMating accouple quand les deux sont vrais.
    bool   _selfAtMeet;     // l'initiateur est arrive a son point de rencontre
    bool   _mateAtMeet;     // le partenaire a signale son arrivee (via NotifyMateArrived)
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
