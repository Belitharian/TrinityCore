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

// Cerveau d'apprentissage par renforcement d'un membre.
//
// Representation LINEAIRE : Q(s,a) = w[a] . phi(s), et non plus une Q-table tabulaire.
// Voir le pave FEATURE_COUNT dans ClanDefines.h pour le pourquoi (le tabulaire ne generalise
// pas, et 43 520 cases pour ~150 pas d'apprentissage par vie ne s'apprennent jamais).
//
// Regle de mise a jour : EXPECTED SARSA (on-policy) et non plus Q-learning (off-policy).
// La combinaison approximation de fonction + bootstrapping + off-policy est la "triade
// mortelle", le cas connu ou les valeurs peuvent diverger. Passer on-policy retire une patte
// du trepied, et au passage l'agent value le risque de sa propre exploration -- ce qui compte
// dans un monde ou explorer peut tuer (famine, predateurs).
//
// L'agent explore (epsilon-greedy) puis exploite. Poids et niveau d'exploration sont
// serialisables pour survivre au redemarrage et etre herites.

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
        // Niveaux de besoin CONTINUS, normalises [0,1]. Ils n'entrent PAS dans Index() (qui
        // reste discret pour le debug et l'amorce), mais alimentent les features : le lineaire
        // sait exploiter l'intensite, la ou le tabulaire devait se contenter du classement.
        // Un affame a 51% et un affame a 99% cessent d'etre le meme etat.
        float    hunger01        = 0.0f;
        float    thirst01        = 0.0f;
        float    energy01        = 0.0f;
        float    repro01         = 0.0f;
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
        // --- Drapeaux Ferme -------------------------------------------------------
        // Un animal de ferme est A PORTEE et disponible (pas reserve, en vie). Un seul bit pour
        // toutes les especes : distinguer vache/poulet doublerait les etats sans changer la
        // decision (la Q-valeur ferait quand meme la moyenne des occurrences).
        bool     farmAnimalReady = false;
        // Une auge de la ferme du clan est VIDE (a remplir : eau OU paille, cf. instinct role).
        bool     farmTroughEmpty = false;
        // La ferme a besoin d'eau (au moins une auge vide -> l'instinct pousse a remplir en
        // priorite l'auge d'eau). Redondant avec farmTroughEmpty pour l'instant : place en
        // reserve, le comportement fin peut plus tard distinguer eau/paille.
        bool     farmNeedsWater  = false;
        bool     houseHasMilk    = false; // le stock de lait est non vide (on peut Boire du lait)

        // Index compact dans [0, STATE_COUNT[. L'ordre d'empilage DOIT correspondre a Decode().
        uint32 Index() const
        {
            uint32 needIdx = uint32(urgentNeed);
            if (needIdx >= NEED_STATE_COUNT)
                needIdx = 0;

            uint32 idx = needIdx;
            idx = idx * TIME_STATE_COUNT + (night ? 1u : 0u);
            idx = idx * 2 + (houseHasMeal ? 1u : 0u);
            idx = idx * 2 + (houseHasRawFood ? 1u : 0u);
            idx = idx * 2 + (houseHasWood ? 1u : 0u);
            idx = idx * 2 + (houseHasStone ? 1u : 0u);
            idx = idx * 2 + (houseFireLit ? 1u : 0u);
            idx = idx * 2 + (diseased ? 1u : 0u);
            idx = idx * 2 + (predatorNearby ? 1u : 0u);
            idx = idx * 2 + (bagFull ? 1u : 0u);
            idx = idx * 2 + (farmAnimalReady ? 1u : 0u);
            idx = idx * 2 + (farmTroughEmpty ? 1u : 0u);
            idx = idx * 2 + (farmNeedsWater ? 1u : 0u);
            idx = idx * 2 + (houseHasMilk ? 1u : 0u);
            return idx;
        }

        // Inverse de Index() : reconstruit l'etat a partir de son indice (dernier bit empile
        // = premier lu). Sert au seed d'instinct (SeedTopUp) qui balaie tous les etats.
        static MindState Decode(uint32 index)
        {
            MindState s;
            s.houseHasMilk    = (index & 1) != 0; index >>= 1;
            s.farmNeedsWater  = (index & 1) != 0; index >>= 1;
            s.farmTroughEmpty = (index & 1) != 0; index >>= 1;
            s.farmAnimalReady = (index & 1) != 0; index >>= 1;
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

            // Les niveaux continus ne sont pas encodes dans l'index (il est discret). On donne
            // au besoin urgent un niveau representatif : sans ca, un etat decode aurait tous
            // ses niveaux a zero et l'amorce apprendrait sur un vecteur non representatif.
            switch (s.urgentNeed)
            {
                case NeedType::Hunger: s.hunger01 = 1.0f; break;
                case NeedType::Thirst: s.thirst01 = 1.0f; break;
                case NeedType::Energy: s.energy01 = 1.0f; break;
                case NeedType::Repro:  s.repro01  = 1.0f; break;
                default: break;
            }
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

        // Met a jour les poids apres avoir observe une recompense et le nouvel etat. Le role
        // borne l'estimation de la valeur future aux actions que l'agent peut reellement prendre.
        void Learn(MindState const& prev, ActionType action, float reward, MindState const& next, ClanRole const* role);

        // Meilleure valeur apprise pour un etat, parmi les actions autorisees par le role.
        float BestValue(MindState const& state, ClanRole const* role) const;
        // Valeur ESPEREE sous la politique epsilon-greedy courante : la cible d'Expected SARSA.
        //   (1-eps) * max Q  +  eps * moyenne Q   (sur les seules actions du role)
        float PolicyValue(MindState const& state, ClanRole const* role) const;

        // Meilleure action apprise pour un etat (hors exploration), parmi les actions du role.
        ActionType BestAction(MindState const& state, ClanRole const* role) const;
        // Surcharge par indice d'etat DISCRET, conservee pour les commandes de debug. Attention :
        // elle passe par Decode(), donc les niveaux de besoin continus y sont approximes.
        ActionType BestAction(uint32 stateIndex, ClanRole const* role) const;
        float ValueOf(uint32 stateIndex, ActionType action) const;

        // Amorce non destructive : pour chaque etat, remonte l'action instinctive du role vers
        // Q_SEED_PRIOR si elle est encore sous ce seuil. Appele a chaque (re)spawn et aux
        // transitions d'age (le role change avec l'etape).
        //
        // En lineaire l'amorce ne peut plus etre exacte : les poids etant PARTAGES entre etats,
        // remonter un etat en deplace legerement d'autres. On procede donc par petits pas de
        // gradient (Q_SEED_PASSES x Q_SEED_RATE) -- le resultat approche les priors sans jamais
        // ecraser brutalement ce qui a ete appris.
        void SeedTopUp(ClanRole const* role);

        float GetEpsilon() const { return _epsilon; }

        // Serialisation texte (CSV) : epsilon, puis ACTION_COUNT*FEATURE_COUNT poids, puis les
        // 2 valeurs de combat. Le garde-fou de dimension de Deserialize rejette automatiquement
        // les anciennes chaines tabulaires -- aucune migration manuelle n'est necessaire.
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
        using FeatureVector = std::array<float, FEATURE_COUNT>;

        // phi(s). L'ordre est decrit au-dessus de FEATURE_COUNT dans ClanDefines.h et DOIT y
        // rester conforme : les poids serialises sont indexes par position.
        static FeatureVector Features(MindState const& state);
        // Produit scalaire w[a] . phi(s).
        float Value(FeatureVector const& features, ActionType action) const;
        // Applique un pas de gradient sur les poids d'une action : w[a] += rate * delta * phi.
        // Le pas est normalise par ||phi||^2 : une mise a jour touche desormais des dizaines de
        // poids a la fois, un alpha brut sur-corrigerait et ferait diverger les valeurs.
        void ApplyGradient(ActionType action, FeatureVector const& features, float delta, float rate);

        // Un vecteur de poids par action (et non plus une case par couple etat/action).
        std::array<std::array<float, FEATURE_COUNT>, ACTION_COUNT> _w;
        float _epsilon;
        float _combatDefend = 0.0f; // valeur apprise de l'action "se defendre"
        float _combatFlee   = 0.0f; // valeur apprise de l'action "fuir"
    };
}

#endif // CUSTOM_CLANS_CLANMIND_H
