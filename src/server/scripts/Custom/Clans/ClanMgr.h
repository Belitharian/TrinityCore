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

// Gestionnaire global du module Clans (singleton).
// Detient l'etat persistant de chaque membre (besoins, age, lignee, cerveau),
// le registre des ressources declarees, et pilote le vieillissement, la
// reproduction inter-clans et la sauvegarde. Ne fait apparaitre AUCUN membre
// place par l'admin ; il ne summon que les nouveau-nes issus de la reproduction.

#ifndef CUSTOM_CLANS_CLANMGR_H
#define CUSTOM_CLANS_CLANMGR_H

#include "ClanDefines.h"
#include "ClanMind.h"
#include "ClanNeeds.h"
#include "ObjectGuid.h"
#include "Position.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Creature;
class GameObject;
class Unit;
class WorldObject;

namespace Clan
{
    // Etat complet et persistant d'un membre, independant du cycle de spawn.
    struct MemberState
    {
        uint64    dbId = 0;                       // cle de persistance (spawnId ou id de naissance)
        ClanId    clan = ClanId::None;
        Gender    gender = Gender::Male;
        LifeStage stage = LifeStage::Adult;
        uint32    ageDays = 0;
        uint32    entry = 0;                       // creature_template du PNJ (cle des modeles par entry)
        uint32    displayId = 0;                   // modele courant (change avec l'age), persiste

        Needs     needs;
        ClanMind  mind;

        uint64    motherId = 0;                   // dbId des parents (0 = fondateur)
        uint64    fatherId = 0;
        uint32    reproCooldownDays = 0;

        bool      isBirth = false;                // vrai si apparu par reproduction
        uint32    birthEntry = 0;                 // creature_template a summon pour un nouveau-ne
        bool      deathOmenSaid = false;          // l'Ancien a deja annonce que sa fin est proche (transitoire)

        uint32    mapId = 0;
        Position  home;

        // Lit attribue (spawnId du GameObject dans la table `gameobject`). 0 = aucun :
        // le membre dort alors dans le lit le plus proche, sinon rentre a `home`.
        // Renseigne depuis le registre custom_clan_bed (par entry du membre), pas persiste
        // dans la table d'etat -> l'attribution survit a la mort / au respawn, et couvre
        // les nouveau-nes (qui partagent l'entry de leur gabarit).
        uint64    bedSpawnId = 0;

        ObjectGuid liveGuid;                      // creature vivante associee (vide si non spawnee)
        bool       dirty = true;                  // a resauvegarder

        bool IsSpawned() const { return !liveGuid.IsEmpty(); }
    };

    // Classification d'une entry declaree dans le registre de ressources.
    struct ResourceEntry
    {
        ResourceType type = ResourceType::None;
        ObjectKind   kind = ObjectKind::Creature;
    };

    // Gabarit de membre declare par l'admin (quel entry = quel clan/genre/etape).
    struct MemberTemplate
    {
        ClanId    clan = ClanId::None;
        Gender    gender = Gender::Male;
        LifeStage stage = LifeStage::Adult;
    };

    // Etat d'un feu suivi (allume/eteint, minuterie de combustion).
    struct FireState
    {
        bool   lit     = true;
        uint32 burnMs  = 0;      // temps restant avant extinction naturelle
        bool   outdoor = false;  // exterieur => eteint par la pluie
        uint32 mapId   = 0;
        uint32 zoneId  = 0;
    };

    // Effets RP joues au debut d'une action (declares dans custom_clan_action_fx).
    struct ActionFx
    {
        uint32 aura  = 0; // aura appliquee sur soi
        uint32 spell = 0; // sort lance (sur soi)
        uint32 emote = 0; // emote jouee (oneshot)
    };

    // Resume global du monde (pour la fenetre monde de l'addon).
    struct WorldSummary
    {
        uint32 population = 0;
        uint32 adults = 0;
        uint32 children = 0;
        uint32 elders = 0;
        uint32 sick = 0;
    };

    // Reservation d'un noeud de ressource par un membre (evite que deux PNJ visent le meme).
    struct NodeClaim
    {
        ObjectGuid by;   // membre qui a reserve le noeud
        uint32     atMs = 0; // date de la reservation (expire apres NODE_CLAIM_TTL_MS)
    };

    // Modeles (displayId) d'une entry de PNJ selon l'etape de vie.
    struct DisplaySet
    {
        uint32 child = 0;
        uint32 adult = 0;
        uint32 elder = 0;

        uint32 Get(LifeStage stage) const
        {
            switch (stage)
            {
                case LifeStage::Child: return child;
                case LifeStage::Elder: return elder ? elder : adult; // repli sur adulte si non defini
                default:               return adult;
            }
        }
    };

    // Cimetiere : emplacement fixe de tombe. Un membre mort y est deplace avant de
    // faire apparaitre sa pierre tombale. `full` = emplacement deja occupe.
    // deceasedId = dbId du membre enterre ici (0 = libre) -> sert au "souvenir" des descendants.
    struct GraveyardSlot
    {
        Position position;
        bool     full = false;
        uint64   deceasedId = 0;
    };

    class ClanMgr
    {
    public:
        static ClanMgr* instance();

        // --- Cycle de vie serveur ---
        void LoadFromDB();          // registres (world) + etats persistants (characters)
        void RespawnBirths();       // re-summon des nouveau-nes sauvegardes
        void Update(uint32 diff);   // vieillissement, reproduction, sauvegarde periodique
        void SaveAll(bool direct);  // flush de tous les etats "dirty"

        // --- Enregistrement des membres places (appele par l'IA) ---
        // Retourne l'etat associe a une creature placee (spawnId != 0), en le creant
        // au besoin a partir du registre de gabarits. nullptr si l'entry est inconnue.
        MemberState* RegisterPlacedMember(Creature* creature);
        // Detache la creature vivante d'un etat (despawn/reset) sans supprimer l'etat.
        void UnbindLive(MemberState* state);
        // Mort definitive
        void KillMember(MemberState* state);

        // --- Phrases par action ---
        void AddPhrase(uint8 action, std::string text);
        // Phrase aleatoire pour une action (nullptr si aucune declaree).
        std::string const* GetRandomPhrase(ActionType action) const;

        // --- Effets RP par action (aura / sort / emote) ---
        void AddActionFx(uint8 action, uint32 aura, uint32 spell, uint32 emote);
        // Effets declares pour une action (nullptr si aucun).
        ActionFx const* GetActionFx(ActionType action) const;

        // --- Afflictions (maladie / poison / saignement) & medecin ---
        void AddDisease(uint32 aura, uint8 type);     // affliction declaree (custom_clan_disease)
        uint32 GetRandomDisease(AfflictionType type) const; // aura aleatoire d'un type (0 si aucune)
        bool IsDiseased(Unit* who) const;             // 'who' porte-t-il une affliction ?
        void CureDiseases(Unit* who) const;           // retire toutes les afflictions
        // Masque des types d'affliction actifs sur 'who' (bit0=Disease, bit1=Poison, bit2=Bleed).
        uint32 GetAfflictionMask(Unit* who) const;
        Creature* FindNearestDoctor(Creature* from) const;

        // --- Resume monde (fenetre globale de l'addon) ---
        WorldSummary GetWorldSummary() const;

        // --- Perception (utilise par l'IA) ---
        Creature*   FindNearestPrey(Creature* from) const;
        Creature*   FindNearestPredator(Creature* from) const; // animal sauvage a exterminer
        GameObject* FindNearestResourceObject(Creature* from, ResourceType type) const;
        // Bois / roche : renvoie le noeud disponible le plus proche NON reserve par un
        // autre membre, et le reserve pour 'from' (evite les trajets concurrents).
        GameObject* FindNearestAvailableNode(Creature* from, ResourceType type);
        void DepleteNode(GameObject* node, uint32 respawnMs);

        // --- Feux ---
        GameObject* FindNearestLitFire(Creature* from);
        GameObject* FindNearestUnlitFire(Creature* from);
        void LightFire(GameObject* fire);

        // --- Reproduction ---
        // Partenaire eligible pour self (adulte, rassasie, cooldown ecoule). nullptr sinon.
        MemberState* FindMate(MemberState* self) const;
        // Declenche une naissance a partir de deux parents (applique les cooldowns).
        void Reproduce(MemberState* a, MemberState* b);

        // --- Cimetiere ---
        // Renvoie un emplacement de tombe libre (et le marque occupe), nullptr si tous
        // pleins. Utilise a la mort d'un membre pour deplacer le corps avant la tombe.
        GraveyardSlot* AcquireGraveyardSlot();
        // Tombe d'un ancetre (mere/pere) du membre 'seeker', la plus proche de 'from' et a
        // portee. Renvoie true + remplit 'out' (position + orientation), false si aucune.
        bool FindAncestorGrave(MemberState const* seeker, Creature* from, Position& out) const;

        // --- Acces divers ---
        MemberState* GetStateByLiveGuid(ObjectGuid guid) const;
        size_t GetMemberCount() const { return _states.size(); }
        // GUID de tous les membres actuellement vivants (flux "tout suivre" + ciblage).
        std::vector<ObjectGuid> GetLiveMemberGuids() const;

        // --- Registres (renseignes par ClanDatabase au chargement) ---
        void AddResourceEntry(uint32 entry, ResourceType type, ObjectKind kind);
        void AddMemberTemplate(uint32 entry, ClanId clan, Gender gender, LifeStage stage);
        // Attribution d'un lit : entry du membre (creature_template) -> spawnId du lit.
        // Par entry (et non par spawnId) pour que les nouveau-nes, qui partagent l'entry
        // de leur gabarit, heritent eux aussi d'un lit.
        void AddBedAssignment(uint32 entry, uint64 bedSpawnId);
        // Lit attribue a une entry de membre. 0 si aucun.
        uint64 GetAssignedBed(uint32 entry) const;
        void AddDisplaySet(uint32 entry, uint32 child, uint32 adult, uint32 elder);
        // Modele a utiliser pour (entry, etape). 0 si non declare (garde le modele du creature_template).
        uint32 GetDisplayId(uint32 entry, LifeStage stage) const;
        // Ajoute un etat charge depuis la base.
        MemberState* AddLoadedState(std::unique_ptr<MemberState> state);

    private:
        ClanMgr() = default;

        void AgingTick();                                 // appele une fois par "jour" simule
        void TryReproductionRound();                      // matchmaking global
        void SpawnBirth(MemberState* state, WorldObject* summoner); // summon effectif d'un nouveau-ne
        void RemoveMemberState(uint64 dbId);              // retire l'etat du registre + de la base
        uint32 PickBirthEntry(ClanId clan, Gender gender) const; // entry enfant declaree pour (clan,genre)
        uint64 AllocateBirthId();
        Creature* ResolveLive(MemberState const* state) const;   // creature vivante d'un etat (ou nullptr)

        GameObject* FindNearestFire(Creature* from, bool wantLit); // enregistre + renvoie le feu le plus proche
        FireState& RegisterFire(GameObject* fire, bool outdoor);   // enregistre un feu (allume par defaut)
        void UpdateFires(uint32 diff);                             // combustion + extinction par la pluie
        // Applique l'apparence allumee/eteinte a un feu (unique endroit a ajuster).
        void ApplyFireVisual(GameObject* fire, bool lit) const;

        std::vector<std::unique_ptr<MemberState>> _states;
        std::unordered_map<uint64, MemberState*>  _byDbId;         // dbId -> etat
        std::unordered_map<uint64, MemberState*>  _pendingBySpawn; // spawnId -> etat non encore lie

        std::unordered_map<uint32, ResourceEntry>  _resourceByEntry;
        std::unordered_map<uint32, MemberTemplate> _memberTemplates;
        std::unordered_map<uint32, DisplaySet>     _displaysByEntry;      // cle = creature_template entry
        std::unordered_map<ObjectGuid, FireState>    _fires;              // feux suivis (par GUID)
        std::unordered_map<ObjectGuid, NodeClaim>    _nodeClaims;         // reservations de noeuds
        std::unordered_map<uint8, std::vector<std::string>> _phrasesByAction; // phrases par action
        std::unordered_map<uint8, ActionFx>          _actionFx;           // effets RP par action
        std::vector<uint32>                          _allDiseases;        // toutes les auras d'affliction (check/soin)
        std::unordered_map<uint8, std::vector<uint32>> _diseasesByType;   // type -> auras (contagion ciblee)
        std::vector<GraveyardSlot>                   _graveyardSlots;     // emplacements de tombes (cimetiere)
        std::unordered_map<uint32, uint64>           _bedByEntry;         // entry membre -> spawnId lit attribue

        uint32 _dayTimerMs = 0;     // accumulateur vers le prochain jour simule
        uint32 _saveTimerMs = 0;    // accumulateur vers la prochaine sauvegarde
        uint32 _respawnTimerMs = 0; // re-tente l'apparition des nouveau-nes en attente
        uint32 _rainTimerMs = 0;    // accumulateur vers le prochain test de pluie
        uint64 _nextBirthId = 0;    // compteur d'id de naissance
    };
}

#define sClanMgr Clan::ClanMgr::instance()

#endif // CUSTOM_CLANS_CLANMGR_H
