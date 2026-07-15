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
    }

    ActionType ClanMind::ChooseAction(MindState const& state) const
    {
        uint16 s = state.Index();

        // Exploration : action aleatoire avec probabilite epsilon.
        if (rand_norm() < _epsilon)
            return ActionType(urand(0, ACTION_COUNT - 1));

        // Exploitation : meilleure action connue.
        return BestAction(s);
    }

    ActionType ClanMind::BestAction(uint16 stateIndex) const
    {
        if (stateIndex >= STATE_COUNT)
            stateIndex = 0;

        // Argmax DETERMINISTE : a Q-valeurs egales, on renvoie toujours la meme action
        // (la premiere). Evite que l'action "ideale" affichee scintille tant qu'un etat
        // n'a rien appris (toutes les valeurs a ~0). L'exploration reste assuree par
        // epsilon dans ChooseAction.
        auto const& row = _q[stateIndex];
        uint8 bestAction = 0;
        for (uint8 a = 1; a < ACTION_COUNT; ++a)
            if (row[a] > row[bestAction])
                bestAction = a;

        return ActionType(bestAction);
    }

    float ClanMind::ValueOf(uint16 stateIndex, ActionType action) const
    {
        if (stateIndex >= STATE_COUNT)
            return 0.0f;
        return _q[stateIndex][uint8(action)];
    }

    float ClanMind::BestValue(MindState const& state) const
    {
        auto const& row = _q[state.Index()];
        float best = row[0];
        for (uint8 a = 1; a < ACTION_COUNT; ++a)
            best = std::max(best, row[a]);
        return best;
    }

    void ClanMind::Learn(MindState const& prev, ActionType action, float reward, MindState const& next)
    {
        uint16 s = prev.Index();
        uint8 a = uint8(action);
        float nextBest = BestValue(next);

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
        return out.str();
    }

    void ClanMind::Deserialize(std::string const& data)
    {
        if (data.empty())
            return;

        std::vector<std::string_view> tokens = Trinity::Tokenize(data, ',', false);
        if (tokens.empty())
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
    }

    void ClanMind::InheritFrom(ClanMind const& a, ClanMind const& b)
    {
        for (uint8 s = 0; s < STATE_COUNT; ++s)
        {
            for (uint8 act = 0; act < ACTION_COUNT; ++act)
            {
                float blended = Q_INHERIT_MIX * a._q[s][act] + (1.0f - Q_INHERIT_MIX) * b._q[s][act];
                blended += frand(-Q_INHERIT_NOISE, Q_INHERIT_NOISE);
                _q[s][act] = blended;
            }
        }

        // Un enfant explore encore : on redonne un peu de curiosite.
        _epsilon = std::max(a._epsilon, b._epsilon);
        _epsilon = std::min(Q_EPSILON_START, _epsilon + 0.20f);
    }
}
