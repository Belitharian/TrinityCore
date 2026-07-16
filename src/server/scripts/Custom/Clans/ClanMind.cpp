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
        SeedPriors();
    }

    void ClanMind::SeedPriors()
    {
        // Pour chaque etat, on decode ses composantes et on donne un a priori positif a
        // l'action "instinctive". L'agent demarre donc competent, puis affine par apprentissage.
        for (uint16 s = 0; s < STATE_COUNT; ++s)
        {
            uint16 idx = s;
            bool predator = (idx & 1) != 0; idx >>= 1;
            bool diseased = (idx & 1) != 0; idx >>= 1;
            bool fire     = (idx & 1) != 0; idx >>= 1;
            bool stone    = (idx & 1) != 0; idx >>= 1;
            bool wood     = (idx & 1) != 0; idx >>= 1;
            bool raw      = (idx & 1) != 0; idx >>= 1;
            idx >>= 1; // bit jour/nuit : sans influence sur l'action instinctive
            NeedType need = NeedType(idx);

            ActionType rec;
            switch (need)
            {
                case NeedType::Thirst: rec = ActionType::DrinkRiver; break;
                case NeedType::Energy: rec = ActionType::Sleep;      break;
                case NeedType::Repro:  rec = ActionType::SeekMate;   break;
                case NeedType::Hunger:
                    if (raw && fire)
                        rec = ActionType::Cook;                 // viande + feu -> cuire
                    else if (raw)                               // viande sans feu -> obtenir un feu
                        rec = (wood && stone) ? ActionType::LightFire
                            : (!wood ? ActionType::GatherWood : ActionType::MineRock);
                    else
                        rec = ActionType::Hunt;                 // pas de viande -> chasser
                    break;
                default: // aucun besoin urgent
                    if (diseased)             rec = ActionType::SeekDoctor;   // se soigner
                    else if (predator)        rec = ActionType::HuntPredator; // exterminer la menace
                    else                      rec = ActionType::Wander;
                    break;
            }

            _q[s][uint8(rec)] = Q_SEED_PRIOR;
        }

        // Instinct de combat : par defaut, se defendre plutot que fuir.
        _combatDefend = Q_SEED_PRIOR;
        _combatFlee   = 0.0f;
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
