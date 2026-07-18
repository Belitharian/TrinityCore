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

#include "ClanMind.h"
#include "ClanRole.h"
#include "Optional.h"
#include "Random.h"
#include "StringConvert.h"
#include "Util.h"
#include <algorithm>
#include <sstream>

namespace Clan
{
    ClanMind::ClanMind() : _epsilon(Q_EPSILON_START)
    {
        for (auto& row : _q)
            row.fill(0.0f);
        // Instinct de combat : par defaut, se defendre plutot que fuir. (Le seed des ACTIONS
        // depend du role, pas encore connu a la construction : il est applique par SeedTopUp
        // au (re)spawn et aux transitions d'age.)
        _combatDefend = Q_SEED_PRIOR;
        _combatFlee   = 0.0f;
    }

    void ClanMind::SeedTopUp(ClanRole const* role)
    {
        if (!role)
            return;

        // Pour chaque etat, on remonte l'action instinctive DU ROLE a au moins Q_SEED_PRIOR si
        // elle est encore sous ce seuil. Non destructif (jamais d'abaissement) et idempotent :
        // on peut le rappeler a chaque spawn et a chaque changement d'etape sans effacer l'acquis.
        for (uint16 s = 0; s < STATE_COUNT; ++s)
        {
            ActionType instinct = role->Instinct(MindState::Decode(s));
            float& q = _q[s][uint8(instinct)];
            if (q < Q_SEED_PRIOR)
                q = Q_SEED_PRIOR;
        }
    }

    bool ClanMind::ChooseDefend() const
    {
        if (rand_norm() < COMBAT_EXPLORE)
            return roll_chance(50);        // exploration : on tente au hasard
        return _combatDefend >= _combatFlee; // sinon meilleure option (egalite -> se defendre)
    }

    void ClanMind::LearnCombat(bool defended, float reward)
    {
        float& v = defended ? _combatDefend : _combatFlee;
        v += Q_ALPHA * (reward - v);
    }

    ActionType ClanMind::ChooseAction(MindState const& state, ClanRole const* role) const
    {
        // Exploration : action aleatoire PARMI CELLES AUTORISEES par le role.
        if (rand_norm() < _epsilon)
        {
            ActionType allowed[ACTION_COUNT];
            uint8 n = 0;
            for (uint8 a = 0; a < ACTION_COUNT; ++a)
                if (!role || role->IsAllowed(ActionType(a)))
                    allowed[n++] = ActionType(a);
            if (n == 0)
                return ActionType::Idle;
            return allowed[urand(0, n - 1)];
        }

        // Exploitation : meilleure action connue (dans le repertoire du role).
        return BestAction(state.Index(), role);
    }

    ActionType ClanMind::BestAction(uint16 stateIndex, ClanRole const* role) const
    {
        if (stateIndex >= STATE_COUNT)
            stateIndex = 0;

        // Argmax DETERMINISTE parmi les actions autorisees : a Q-valeurs egales, on garde la
        // premiere (indice le plus bas). Idle (0) est vital pour tous, donc toujours un repli.
        auto const& row = _q[stateIndex];
        ActionType best = ActionType::Idle;
        float bestVal = 0.0f;
        bool found = false;
        for (uint8 a = 0; a < ACTION_COUNT; ++a)
        {
            if (role && !role->IsAllowed(ActionType(a)))
                continue;
            if (!found || row[a] > bestVal)
            {
                bestVal = row[a];
                best = ActionType(a);
                found = true;
            }
        }
        return best;
    }

    float ClanMind::ValueOf(uint16 stateIndex, ActionType action) const
    {
        if (stateIndex >= STATE_COUNT)
            return 0.0f;
        return _q[stateIndex][uint8(action)];
    }

    float ClanMind::BestValue(MindState const& state, ClanRole const* role) const
    {
        auto const& row = _q[state.Index()];
        bool found = false;
        float best = 0.0f;
        for (uint8 a = 0; a < ACTION_COUNT; ++a)
        {
            if (role && !role->IsAllowed(ActionType(a)))
                continue;
            if (!found || row[a] > best)
            {
                best = row[a];
                found = true;
            }
        }
        return found ? best : 0.0f;
    }

    void ClanMind::Learn(MindState const& prev, ActionType action, float reward, MindState const& next, ClanRole const* role)
    {
        uint16 s = prev.Index();
        uint8 a = uint8(action);
        float nextBest = BestValue(next, role);

        // Q(s,a) <- Q(s,a) + alpha * [r + gamma * max_a' Q(s',a') - Q(s,a)]
        float& q = _q[s][a];
        q += Q_ALPHA * (reward + Q_GAMMA * nextBest - q);

        // Decroissance de l'exploration : l'agent exploite de plus en plus ce qu'il apprend.
        _epsilon = std::max(Q_EPSILON_MIN, _epsilon * Q_EPSILON_DECAY);
    }

    std::string ClanMind::Serialize() const
    {
        std::ostringstream out;
        out << _epsilon;
        for (auto const& row : _q)
            for (float v : row)
                out << ',' << v;
        // Valeurs de combat en fin de chaine.
        out << ',' << _combatDefend << ',' << _combatFlee;
        return out.str();
    }

    void ClanMind::Deserialize(std::string const& data)
    {
        if (data.empty())
            return;

        std::vector<std::string_view> tokens = Trinity::Tokenize(data, ',', false);
        if (tokens.empty())
            return;

        // Garde-fou de dimension : la chaine doit contenir exactement
        // epsilon + STATE_COUNT*ACTION_COUNT valeurs + les 2 valeurs de combat. Si la taille ne
        // correspond pas (ex. ACTION_COUNT a change avec l'ajout d'une action), on ignore les
        // donnees et on garde la table par defaut deja "seedee" par le constructeur : la Q-table
        // se reinitialise proprement au lieu d'etre lue de travers (donnees mal alignees).
        size_t const expected = 1 + size_t(STATE_COUNT) * ACTION_COUNT + 2;
        if (tokens.size() != expected)
            return;

        size_t idx = 0;
        if (Optional<float> eps = Trinity::StringTo<float>(tokens[idx]))
            _epsilon = *eps;
        ++idx;

        for (auto& row : _q)
        {
            for (float& v : row)
            {
                if (idx >= tokens.size())
                    return;
                if (Optional<float> value = Trinity::StringTo<float>(tokens[idx]))
                    v = *value;
                ++idx;
            }
        }

        // Valeurs de combat (fin de chaine).
        if (idx < tokens.size())
            if (Optional<float> value = Trinity::StringTo<float>(tokens[idx]))
                _combatDefend = *value;
        ++idx;
        if (idx < tokens.size())
            if (Optional<float> value = Trinity::StringTo<float>(tokens[idx]))
                _combatFlee = *value;
    }

    void ClanMind::InheritFrom(ClanMind const& a, ClanMind const& b)
    {
        for (uint16 s = 0; s < STATE_COUNT; ++s)
        {
            for (uint8 act = 0; act < ACTION_COUNT; ++act)
            {
                float blended = Q_INHERIT_MIX * a._q[s][act] + (1.0f - Q_INHERIT_MIX) * b._q[s][act];
                blended += frand(-Q_INHERIT_NOISE, Q_INHERIT_NOISE);
                _q[s][act] = blended;
            }
        }

        // Heritage de l'instinct de combat.
        _combatDefend = Q_INHERIT_MIX * a._combatDefend + (1.0f - Q_INHERIT_MIX) * b._combatDefend;
        _combatFlee   = Q_INHERIT_MIX * a._combatFlee   + (1.0f - Q_INHERIT_MIX) * b._combatFlee;

        // Un enfant explore encore : on redonne un peu de curiosite.
        _epsilon = std::max(a._epsilon, b._epsilon);
        _epsilon = std::min(Q_EPSILON_START, _epsilon + 0.20f);
    }
}
