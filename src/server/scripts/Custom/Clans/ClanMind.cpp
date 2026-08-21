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
#include "Errors.h"
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
        for (auto& row : _w)
            row.fill(0.0f);
        // Instinct de combat : par defaut, se defendre plutot que fuir. (Le seed des ACTIONS
        // depend du role, pas encore connu a la construction : il est applique par SeedTopUp
        // au (re)spawn et aux transitions d'age.)
        _combatDefend = Q_SEED_PRIOR;
        _combatFlee   = 0.0f;
    }

    ClanMind::FeatureVector ClanMind::Features(MindState const& state)
    {
        FeatureVector f{};
        f.fill(0.0f);

        size_t i = 0;

        // Biais : permet a une action d'avoir une valeur de fond independante du contexte.
        f[i++] = 1.0f;

        // Niveaux de besoin continus. Bornes [0,1] : des features non bornees desequilibreraient
        // le pas de gradient entre dimensions.
        f[i++] = std::clamp(state.hunger01, 0.0f, 1.0f);
        f[i++] = std::clamp(state.thirst01, 0.0f, 1.0f);
        f[i++] = std::clamp(state.energy01, 0.0f, 1.0f);
        f[i++] = std::clamp(state.repro01,  0.0f, 1.0f);

        // Besoin urgent en one-hot : garde le signal discret a cote des niveaux continus.
        uint8 needIdx = uint8(state.urgentNeed);
        if (needIdx >= NEED_STATE_COUNT)
            needIdx = 0;
        size_t const needBase = i;
        f[i + needIdx] = 1.0f;
        i += NEED_STATE_COUNT;

        f[i++] = state.night ? 1.0f : 0.0f;

        // Drapeaux de contexte. L'ordre est fige : il indexe aussi les conjonctions ci-dessous.
        bool const flags[FEATURE_FLAG_COUNT] = {
            state.houseHasMeal, state.houseHasRawFood, state.houseHasWood, state.houseHasStone,
            state.houseFireLit, state.diseased, state.predatorNearby, state.bagFull,
            // Ferme : ordre fige, en fin de liste (les nouvelles features ne doivent pas
            // se glisser AVANT les anciennes, sinon les poids serialises se decaleraient tous).
            state.farmAnimalReady, state.farmTroughEmpty, state.farmNeedsWater, state.houseHasMilk
        };

        size_t const flagBase = i;
        for (uint8 k = 0; k < FEATURE_FLAG_COUNT; ++k)
            f[i++] = flags[k] ? 1.0f : 0.0f;

        // Conjonctions besoin x drapeau. Sans elles, le lineaire ne peut pas distinguer
        // "affame ET un repas dispo" de la simple somme de ses deux termes -- or c'est
        // exactement ce genre de combinaison qui decide de l'action.
        for (uint8 n = 0; n < NEED_STATE_COUNT; ++n)
            for (uint8 k = 0; k < FEATURE_FLAG_COUNT; ++k)
                f[i++] = f[needBase + n] * f[flagBase + k];

        // Filet de securite : un ecart entre ce qu'ecrit cette fonction et FEATURE_COUNT
        // decalerait silencieusement TOUS les poids (chaque feature serait multipliee par le
        // poids d'une autre). Aucune erreur visible, un apprentissage simplement absurde.
        // A relever tout de suite plutot qu'a deboguer plus tard.
        ASSERT(i == FEATURE_COUNT, "ClanMind::Features a ecrit %zu features, FEATURE_COUNT vaut %u", i, uint32(FEATURE_COUNT));

        return f;
    }

    float ClanMind::Value(FeatureVector const& features, ActionType action) const
    {
        uint8 const a = uint8(action);
        if (a >= ACTION_COUNT)
            return 0.0f;

        auto const& w = _w[a];
        float sum = 0.0f;
        for (size_t i = 0; i < FEATURE_COUNT; ++i)
            sum += w[i] * features[i];
        return sum;
    }

    void ClanMind::ApplyGradient(ActionType action, FeatureVector const& features, float delta, float rate)
    {
        uint8 const a = uint8(action);
        if (a >= ACTION_COUNT)
            return;

        // Normalisation par ||phi||^2 : la mise a jour touche desormais des dizaines de poids
        // simultanement. Sans ce facteur, un pas "brut" corrigerait l'erreur autant de fois
        // qu'il y a de features actives -- sur-correction, puis oscillation ou divergence.
        float norm = 0.0f;
        for (float x : features)
            norm += x * x;
        float const step = rate / std::max(1.0f, norm);

        auto& w = _w[a];
        for (size_t i = 0; i < FEATURE_COUNT; ++i)
            w[i] += step * delta * features[i];
    }

    void ClanMind::SeedTopUp(ClanRole const* role)
    {
        if (!role)
            return;

        // On echantillonne des etats DISCRETS et on pousse l'action instinctive du role vers
        // Q_SEED_PRIOR la ou elle est encore en dessous.
        //
        // En tabulaire c'etait une simple affectation. Ici les poids sont partages, donc on
        // procede par petits pas de gradient repetes : chaque etat tire les poids dans sa
        // direction, et l'ensemble converge vers un compromis qui respecte les priors sans
        // ecraser l'acquis.
        //
        // Balayage complet ou echantillon ? Tant que STATE_COUNT reste petit (<= Q_SEED_SAMPLES),
        // on balaie tous les etats : c'est exhaustif et deterministe. Au-dela, on echantillonne
        // -- le seed est paye a CHAQUE spawn et transition d'age, et le cout serait multiplie
        // par 16 chaque fois qu'on ajoute un drapeau de contexte. Les poids etant partages, un
        // tirage aleatoire converge vers le meme compromis a cout constant.
        uint32 const total = STATE_COUNT <= Q_SEED_SAMPLES ? STATE_COUNT : Q_SEED_SAMPLES;
        bool   const sample = STATE_COUNT > Q_SEED_SAMPLES;

        for (uint8 pass = 0; pass < Q_SEED_PASSES; ++pass)
        {
            for (uint32 k = 0; k < total; ++k)
            {
                uint32 const s = sample ? urand(0, STATE_COUNT - 1) : k;
                MindState const state = MindState::Decode(s);
                FeatureVector const f = Features(state);

                ActionType const instinct = role->Instinct(state);
                float const current = Value(f, instinct);
                if (current >= Q_SEED_PRIOR)
                    continue; // deja au niveau : on ne redescend jamais un acquis

                ApplyGradient(instinct, f, Q_SEED_PRIOR - current, Q_SEED_RATE);
            }
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

        // Exploitation : meilleure action connue (dans le repertoire du role). On passe l'etat
        // COMPLET et non son index : l'index est discret et perdrait les niveaux continus.
        return BestAction(state, role);
    }

    ActionType ClanMind::BestAction(MindState const& state, ClanRole const* role) const
    {
        FeatureVector const f = Features(state);

        // Argmax DETERMINISTE parmi les actions autorisees : a valeurs egales, on garde la
        // premiere (indice le plus bas). Idle (0) est vital pour tous, donc toujours un repli.
        ActionType best = ActionType::Idle;
        float bestVal = 0.0f;
        bool found = false;
        for (uint8 a = 0; a < ACTION_COUNT; ++a)
        {
            if (role && !role->IsAllowed(ActionType(a)))
                continue;

            float const v = Value(f, ActionType(a));
            if (!found || v > bestVal)
            {
                bestVal = v;
                best = ActionType(a);
                found = true;
            }
        }
        return best;
    }

    ActionType ClanMind::BestAction(uint32 stateIndex, ClanRole const* role) const
    {
        if (stateIndex >= STATE_COUNT)
            stateIndex = 0;
        return BestAction(MindState::Decode(stateIndex), role);
    }

    float ClanMind::ValueOf(uint32 stateIndex, ActionType action) const
    {
        if (stateIndex >= STATE_COUNT)
            return 0.0f;
        return Value(Features(MindState::Decode(stateIndex)), action);
    }

    float ClanMind::BestValue(MindState const& state, ClanRole const* role) const
    {
        FeatureVector const f = Features(state);
        bool found = false;
        float best = 0.0f;
        for (uint8 a = 0; a < ACTION_COUNT; ++a)
        {
            if (role && !role->IsAllowed(ActionType(a)))
                continue;

            float const v = Value(f, ActionType(a));
            if (!found || v > best)
            {
                best = v;
                found = true;
            }
        }
        return found ? best : 0.0f;
    }

    float ClanMind::PolicyValue(MindState const& state, ClanRole const* role) const
    {
        FeatureVector const f = Features(state);

        bool found = false;
        float best = 0.0f;
        float sum = 0.0f;
        uint8 count = 0;

        for (uint8 a = 0; a < ACTION_COUNT; ++a)
        {
            if (role && !role->IsAllowed(ActionType(a)))
                continue;

            float const v = Value(f, ActionType(a));
            sum += v;
            ++count;
            if (!found || v > best)
            {
                best = v;
                found = true;
            }
        }

        if (!count)
            return 0.0f;

        // Esperance sous la politique epsilon-greedy, exactement celle qu'applique ChooseAction :
        // avec proba (1-eps) l'action gloutonne, avec proba eps une uniforme parmi les autorisees.
        return (1.0f - _epsilon) * best + _epsilon * (sum / float(count));
    }

    void ClanMind::Learn(MindState const& prev, ActionType action, float reward, MindState const& next, ClanRole const* role)
    {
        FeatureVector const f = Features(prev);

        // Expected SARSA : la cible utilise la valeur ESPEREE sous la politique courante, et non
        // le max (Q-learning). Deux raisons :
        //  - on-policy : retire une patte de la "triade mortelle" (approximation + bootstrap +
        //    off-policy), le cas ou les valeurs peuvent diverger avec des poids partages ;
        //  - l'agent value le risque de sa propre exploration, ce qui compte dans un monde ou
        //    explorer peut tuer (famine, predateurs).
        // Par rapport a SARSA echantillonne, prendre l'esperance evite le bruit du tirage de a'
        // -- et surtout ne demande pas de connaitre l'action suivante, donc aucun remaniement
        // du site d'appel dans FinishAction.
        float const target = reward + Q_GAMMA * PolicyValue(next, role);
        float const delta = target - Value(f, action);

        ApplyGradient(action, f, delta, Q_ALPHA);

        // Decroissance de l'exploration : l'agent exploite de plus en plus ce qu'il apprend.
        _epsilon = std::max(Q_EPSILON_MIN, _epsilon * Q_EPSILON_DECAY);
    }

    std::string ClanMind::Serialize() const
    {
        std::ostringstream out;
        out << _epsilon;
        for (auto const& row : _w)
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
        // epsilon + ACTION_COUNT*FEATURE_COUNT poids + les 2 valeurs de combat. Si la taille ne
        // correspond pas, on ignore les donnees et on garde les poids par defaut : le cerveau se
        // reinitialise proprement au lieu d'etre lu de travers (donnees mal alignees).
        //
        // C'est ce meme garde-fou qui absorbe le passage du tabulaire au lineaire : les anciennes
        // chaines (43 520 valeurs) ne font pas la bonne taille et sont donc rejetees d'office.
        // Aucun TRUNCATE manuel de custom_clan_member n'est necessaire.
        size_t const expected = 1 + size_t(ACTION_COUNT) * FEATURE_COUNT + 2;
        if (tokens.size() != expected)
            return;

        size_t idx = 0;
        if (Optional<float> eps = Trinity::StringTo<float>(tokens[idx]))
            _epsilon = *eps;
        ++idx;

        for (auto& row : _w)
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
        // Melange des POIDS parentaux. Nettement plus porteur qu'avec le tabulaire : moyenner
        // deux Q-tables creuses revenait surtout a moyenner des zeros (moins de 0.5% des cases
        // etaient renseignees), alors que chaque poids ici resume l'experience accumulee sur
        // tous les etats ou sa feature etait active. La transmission entre generations devient
        // reellement porteuse de sens.
        // Q_INHERIT_NOISE est exprime en ESPACE DE VALEUR : c'est de combien on veut perturber
        // Q(s,a), pas chaque poids. En tabulaire les deux se confondaient (une valeur = une
        // case). Ici une valeur est la somme de ~FEATURE_TYPICAL_ACTIVE poids, et des bruits
        // independants s'additionnent en sqrt(N) : il faut donc diviser par sqrt(N) pour
        // retrouver l'amplitude voulue.
        //
        // Sans cette division, le bruit d'heritage atteignait un ecart-type de ~0.20 sur les
        // valeurs, soit l'ordre de grandeur de Q_SEED_PRIOR : l'instinct amorce etait noye et
        // c'est le hasard qui choisissait l'action dans les etats encore peu appris.
        float const weightNoise = Q_INHERIT_NOISE / std::sqrt(float(FEATURE_TYPICAL_ACTIVE));

        for (uint8 act = 0; act < ACTION_COUNT; ++act)
        {
            for (uint16 i = 0; i < FEATURE_COUNT; ++i)
            {
                float blended = Q_INHERIT_MIX * a._w[act][i] + (1.0f - Q_INHERIT_MIX) * b._w[act][i];
                blended += frand(-weightNoise, weightNoise);
                _w[act][i] = blended;
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
