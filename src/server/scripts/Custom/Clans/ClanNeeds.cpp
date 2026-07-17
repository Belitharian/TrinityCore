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

#include "ClanNeeds.h"
#include <algorithm>

namespace Clan
{
    void Needs::Decay(uint32 diffMs, bool night, bool canReproduce)
    {
        // Les taux sont exprimes en points par JOUR SIMULE : on convertit le temps reel
        // ecoule en fraction de jour simule. Les besoins suivent donc la meme echelle de temps
        // que le vieillissement -- raccourcir la duree d'un jour accelere aussi les besoins.
        float dayLenSec = float(std::max<uint32>(1, REAL_SECONDS_PER_SIM_DAY));
        float days = (diffMs / 1000.0f) / dayLenSec;

        hunger = std::min(NEED_MAX, hunger + HUNGER_RATE * days);
        thirst = std::min(NEED_MAX, thirst + THIRST_RATE * days);
        energy = std::min(NEED_MAX, energy + (night ? ENERGY_RATE_NIGHT : ENERGY_RATE_DAY) * days);

        // Seuls les adultes ressentent le besoin de se reproduire.
        if (canReproduce)
            reproUrge = std::min(NEED_MAX, reproUrge + REPRO_RATE * days);
        else
            reproUrge = 0.0f;
    }

    float Needs::Get(NeedType type) const
    {
        switch (type)
        {
            case NeedType::Hunger: return hunger;
            case NeedType::Thirst: return thirst;
            case NeedType::Energy: return energy;
            case NeedType::Repro:  return reproUrge;
            default:               return 0.0f;
        }
    }

    float Needs::Satisfy(NeedType type, float amount)
    {
        float* target = nullptr;
        switch (type)
        {
            case NeedType::Hunger: target = &hunger;    break;
            case NeedType::Thirst: target = &thirst;    break;
            case NeedType::Energy: target = &energy;    break;
            case NeedType::Repro:  target = &reproUrge; break;
            default: return 0.0f;
        }

        float before = *target;
        *target = std::max(0.0f, *target - amount);
        return before - *target;
    }

    NeedType Needs::MostUrgent() const
    {
        NeedType worst = NeedType::None;
        float worstValue = NEED_URGENT_THRESHOLD;

        auto consider = [&](NeedType type, float value)
        {
            if (value > worstValue)
            {
                worstValue = value;
                worst = type;
            }
        };

        // Ordre de priorite en cas d'egalite : soif > faim > fatigue > reproduction.
        consider(NeedType::Repro,  reproUrge);
        consider(NeedType::Energy, energy);
        consider(NeedType::Hunger, hunger);
        consider(NeedType::Thirst, thirst);
        return worst;
    }

    bool Needs::IsWellFed() const
    {
        return hunger < REPRO_READY_MAX_NEED
            && thirst < REPRO_READY_MAX_NEED
            && energy < REPRO_READY_MAX_NEED;
    }
}
