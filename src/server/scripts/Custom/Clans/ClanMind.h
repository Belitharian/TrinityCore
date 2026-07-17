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
    // Etat discret percu par l'agent.
    struct MindState
    {
        NeedType urgentNeed    = NeedType::None;
        bool     night         = false;
        bool     hasRawFood    = false; // possede de la viande crue (a cuire)
        bool     hasWood       = false; // possede du bois (pour rallumer)
        bool     hasStone      = false; // possede une pierre (pour rallumer)
        bool     litFireNearby  = false; // un feu ALLUME est a portee (on peut y cuire)
        bool     diseased       = false; // porte une aura de maladie
        bool     predatorNearby = false; // un animal sauvage (predateur) est a portee
        // Un feu ETEINT est a portee. Distinct de litFireNearby : sans ce drapeau, des qu'un
        // seul feu est allume l'agent ne "voit" plus les autres eteints et ne les rallume pas.
        bool     unlitFireNearby = false;

        // Index compact dans [0, STATE_COUNT[.
        uint16 Index() const
        {
            uint16 needIdx = uint16(urgentNeed);
            if (needIdx >= NEED_STATE_COUNT)
                needIdx = 0;

            uint16 idx = needIdx;
            idx = uint16(idx * TIME_STATE_COUNT + (night ? 1 : 0));
            idx = uint16(idx * 2 + (hasRawFood ? 1 : 0));
            idx = uint16(idx * 2 + (hasWood ? 1 : 0));
            idx = uint16(idx * 2 + (hasStone ? 1 : 0));
            idx = uint16(idx * 2 + (litFireNearby ? 1 : 0));
            idx = uint16(idx * 2 + (diseased ? 1 : 0));
            idx = uint16(idx * 2 + (predatorNearby ? 1 : 0));
            idx = uint16(idx * 2 + (unlitFireNearby ? 1 : 0));
            return idx;
        }
    };

    class ClanMind
    {
    public:
        ClanMind();

        // Choisit une action pour l'etat courant (epsilon-greedy).
        ActionType ChooseAction(MindState const& state) const;

        // Met a jour la Q-table apres avoir observe une recompense et le nouvel etat.
        void Learn(MindState const& prev, ActionType action, float reward, MindState const& next);

        // Meilleure valeur Q apprise pour un etat (utile au debug).
        float BestValue(MindState const& state) const;
        // Meilleure action apprise pour un etat, hors exploration (utile au debug).
        ActionType BestAction(uint16 stateIndex) const;
        float ValueOf(uint16 stateIndex, ActionType action) const;

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
        // Donne un a priori positif a la "bonne" action de chaque etat (instinct de depart).
        void SeedPriors();

        std::array<std::array<float, ACTION_COUNT>, STATE_COUNT> _q;
        float _epsilon;
        float _combatDefend = 0.0f; // valeur apprise de l'action "se defendre"
        float _combatFlee   = 0.0f; // valeur apprise de l'action "fuir"
    };
}

#endif // CUSTOM_CLANS_CLANMIND_H
