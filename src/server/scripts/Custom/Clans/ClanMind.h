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

// Cerveau d'apprentissage par renforcement (Q-learning tabulaire) d'un membre.
// Chaque membre possede sa propre Q-table : etat discret -> valeur de chaque action.
// L'agent explore (epsilon-greedy) puis exploite ce qu'il a appris. La table et le
// niveau d'exploration sont serialisables pour survivre au redemarrage et etre herites.

#ifndef CUSTOM_CLANS_CLANMIND_H
#define CUSTOM_CLANS_CLANMIND_H

#include "ClanDefines.h"
#include <array>
#include <string>

namespace Clan
{
    class ClanRole; // role metier (homme/femme/enfant) : gate les actions et fournit l'instinct

    // Etat discret percu par l'agent. Les drapeaux de ressources refletent desormais le STOCK
    // DE LA MAISON (partage), pas l'inventaire individuel : les decisions du clan tournent
    // autour de ce que contient le foyer (repas prets, matieres, etat du feu).
    struct MindState
    {
        NeedType urgentNeed      = NeedType::None;
        bool     night           = false;
        bool     houseHasMeal    = false; // un repas cuisine est dispo au stock (on peut manger)
        bool     houseHasRawFood = false; // de la viande crue est au stock (a cuisiner)
        bool     houseHasWood    = false; // du bois est au stock (pour rallumer)
        bool     houseHasStone   = false; // de la pierre est au stock (pour rallumer)
        bool     houseFireLit    = false; // le foyer de la maison est allume (on peut cuire)
        bool     diseased        = false; // porte une aura de maladie
        bool     predatorNearby  = false; // un animal sauvage (predateur) est a portee
        // Seul bit portant sur l'inventaire INDIVIDUEL : au moins un type de ressource est porte
        // au maximum (INVENTORY_MAX_PER_ITEM). Il rend le retour au foyer (StoreHome) apprenable :
        // sans lui, "chasser" recouvrait deux situations opposees (partir chasser / rentrer livrer)
        // et une recolte pouvait rester bloquee a vie dans un sac plein.
        bool     bagFull         = false;

        // Index compact dans [0, STATE_COUNT[. L'ordre d'empilage DOIT correspondre a Decode().
        uint16 Index() const
        {
            uint16 needIdx = uint16(urgentNeed);
            if (needIdx >= NEED_STATE_COUNT)
                needIdx = 0;

            uint16 idx = needIdx;
            idx = uint16(idx * TIME_STATE_COUNT + (night ? 1 : 0));
            idx = uint16(idx * 2 + (houseHasMeal ? 1 : 0));
            idx = uint16(idx * 2 + (houseHasRawFood ? 1 : 0));
            idx = uint16(idx * 2 + (houseHasWood ? 1 : 0));
            idx = uint16(idx * 2 + (houseHasStone ? 1 : 0));
            idx = uint16(idx * 2 + (houseFireLit ? 1 : 0));
            idx = uint16(idx * 2 + (diseased ? 1 : 0));
            idx = uint16(idx * 2 + (predatorNearby ? 1 : 0));
            idx = uint16(idx * 2 + (bagFull ? 1 : 0));
            return idx;
        }

        // Inverse de Index() : reconstruit l'etat a partir de son indice (dernier bit empile
        // = premier lu). Sert au seed d'instinct (SeedTopUp) qui balaie tous les etats.
        static MindState Decode(uint16 index)
        {
            MindState s;
            s.bagFull         = (index & 1) != 0; index >>= 1;
            s.predatorNearby  = (index & 1) != 0; index >>= 1;
            s.diseased        = (index & 1) != 0; index >>= 1;
            s.houseFireLit    = (index & 1) != 0; index >>= 1;
            s.houseHasStone   = (index & 1) != 0; index >>= 1;
            s.houseHasWood    = (index & 1) != 0; index >>= 1;
            s.houseHasRawFood = (index & 1) != 0; index >>= 1;
            s.houseHasMeal    = (index & 1) != 0; index >>= 1;
            s.night           = (index & 1) != 0; index >>= 1;
            s.urgentNeed      = NeedType(index % NEED_STATE_COUNT);
            return s;
        }
    };

    class ClanMind
    {
    public:
        ClanMind();

        // Choisit une action pour l'etat courant (epsilon-greedy), RESTREINTE aux actions
        // autorisees par le role (homme/femme/enfant).
        ActionType ChooseAction(MindState const& state, ClanRole const* role) const;

        // Met a jour la Q-table apres avoir observe une recompense et le nouvel etat. Le role
        // borne l'estimation de la valeur future aux actions que l'agent peut reellement prendre.
        void Learn(MindState const& prev, ActionType action, float reward, MindState const& next, ClanRole const* role);

        // Meilleure valeur Q apprise pour un etat, parmi les actions autorisees par le role.
        float BestValue(MindState const& state, ClanRole const* role) const;
        // Meilleure action apprise pour un etat (hors exploration), parmi les actions du role.
        ActionType BestAction(uint16 stateIndex, ClanRole const* role) const;
        float ValueOf(uint16 stateIndex, ActionType action) const;

        // Amorce non destructive : pour chaque etat, remonte l'action instinctive du role a au
        // moins Q_SEED_PRIOR si elle est encore sous ce seuil (n'abaisse jamais un acquis).
        // Appele a chaque (re)spawn et aux transitions d'age (le role change avec l'etape).
        void SeedTopUp(ClanRole const* role);

        float GetEpsilon() const { return _epsilon; }

        // Serialisation texte (CSV) : epsilon suivi de STATE_COUNT*ACTION_COUNT valeurs.
        std::string Serialize() const;
        void Deserialize(std::string const& data);

        // Initialise la table d'un enfant a partir de celles de ses deux parents.
        void InheritFrom(ClanMind const& a, ClanMind const& b);

        // --- Apprentissage du combat (choix defendre / fuir), separe de la Q-table ---
        bool ChooseDefend() const;               // true = defendre, false = fuir (epsilon-greedy)
        void LearnCombat(bool defended, float reward); // met a jour la preference combat
        float GetDefendValue() const { return _combatDefend; }
        float GetFleeValue() const { return _combatFlee; }

    private:
        std::array<std::array<float, ACTION_COUNT>, STATE_COUNT> _q;
        float _epsilon;
        float _combatDefend = 0.0f; // valeur apprise de l'action "se defendre"
        float _combatFlee   = 0.0f; // valeur apprise de l'action "fuir"
    };
}

#endif // CUSTOM_CLANS_CLANMIND_H
