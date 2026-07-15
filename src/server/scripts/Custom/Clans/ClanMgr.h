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
        uint32    displayId = 0;                  // modele courant (change avec l'age), persiste

        Needs     needs;
        ClanMind  mind;

        uint64    motherId = 0;                   // dbId des parents (0 = fondateur)
        uint64    fatherId = 0;
        uint32    reproCooldownDays = 0;

        bool      isBirth = false;                // vrai si apparu par reproduction
        uint32    birthEntry = 0;                 // creature_template a summon pour un nouveau-ne

        uint32    mapId = 0;
        Position  home;

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

    // Reservation d'un noeud de ressource par un membre (evite que deux PNJ visent le meme).
    struct NodeClaim
    {
        ObjectGuid by;   // membre qui a reserve le noeud
        uint32     atMs = 0; // date de la reservation (expire apres NODE_CLAIM_TTL_MS)
    };

    // Modeles (displayId) d'un couple (clan, genre) selon l'etape de vie.
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
        // Mort definitive d'un membre (tue par un ennemi) : retire du registre + de la base.
        void OnMemberKilled(MemberState* state);

        // --- Phrases par action ---
        void AddPhrase(uint8 action, std::string text);
        // Phrase aleatoire pour une action (nullptr si aucune declaree).
        std::string const* GetRandomPhrase(ActionType action) const;

        // --- Perception (utilise par l'IA) ---
        Creature*   FindNearestPrey(Creature* from) const;
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
        // Partenaire eligible pour self (adulte, rassasie, cooldown ecoule, autre clan). nullptr sinon.
        MemberState* FindMate(MemberState* self) const;
        // Declenche une naissance a partir de deux parents (applique les cooldowns).
        void Reproduce(MemberState* a, MemberState* b);

        // --- Acces divers ---
        MemberState* GetStateByLiveGuid(ObjectGuid guid) const;
        size_t GetMemberCount() const { return _states.size(); }

        // --- Registres (renseignes par ClanDatabase au chargement) ---
        void AddResourceEntry(uint32 entry, ResourceType type, ObjectKind kind);
        void AddMemberTemplate(uint32 entry, ClanId clan, Gender gender, LifeStage stage);
        void AddDisplaySet(ClanId clan, Gender gender, uint32 child, uint32 adult, uint32 elder);
        // Modele a utiliser pour (clan, genre, etape). 0 si non declare (garde le modele du template).
        uint32 GetDisplayId(ClanId clan, Gender gender, LifeStage stage) const;
        // Ajoute un etat charge depuis la base.
        MemberState* AddLoadedState(std::unique_ptr<MemberState> state);

    private:
        ClanMgr() = default;

        void AgingTick();                                 // appele une fois par "jour" simule
        void TryReproductionRound();                      // matchmaking global
        void SpawnBirth(MemberState* state, WorldObject* summoner); // summon effectif d'un nouveau-ne
        void KillMember(MemberState* state);              // mort par vieillissement (avec despawn)
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
        std::unordered_map<uint16, DisplaySet>     _displaysByClanGender; // cle = (clan << 8) | gender
        std::unordered_map<ObjectGuid, FireState>    _fires;              // feux suivis (par GUID)
        std::unordered_map<ObjectGuid, NodeClaim>    _nodeClaims;         // reservations de noeuds
        std::unordered_map<uint8, std::vector<std::string>> _phrasesByAction; // phrases par action

        uint32 _dayTimerMs = 0;     // accumulateur vers le prochain jour simule
        uint32 _saveTimerMs = 0;    // accumulateur vers la prochaine sauvegarde
        uint32 _respawnTimerMs = 0; // re-tente l'apparition des nouveau-nes en attente
        uint32 _rainTimerMs = 0;    // accumulateur vers le prochain test de pluie
        uint64 _nextBirthId = 0;    // compteur d'id de naissance
    };
}

#define sClanMgr Clan::ClanMgr::instance()

#endif // CUSTOM_CLANS_CLANMGR_H
