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

// Modele de besoins d'un membre de clan. Les besoins croissent avec le temps
// (0 = satisfait, 100 = critique) et sont apaises par les actions reussies.

#ifndef CUSTOM_CLANS_CLANNEEDS_H
#define CUSTOM_CLANS_CLANNEEDS_H

#include "ClanDefines.h"

namespace Clan
{
    struct Needs
    {
        float hunger    = 0.0f;
        float thirst    = 0.0f;
        float energy    = 0.0f; // fatigue accumulee (haute = fatigue)
        float reproUrge = 0.0f;

        // Fait croitre les besoins selon le temps ecoule (ms) et le moment de la journee.
        // canReproduce = false (enfant / ancien) -> le besoin de reproduction reste a 0.
        void Decay(uint32 diffMs, bool night, bool canReproduce);

        // Valeur courante d'un besoin donne.
        float Get(NeedType type) const;

        // Reduit un besoin d'un certain montant (borne a 0). Retourne la reduction reelle.
        float Satisfy(NeedType type, float amount);

        // Besoin le plus critique (None si aucun n'est urgent).
        NeedType MostUrgent() const;

        // Vrai si tous les besoins vitaux sont sous le seuil de "pret a se reproduire".
        bool IsWellFed() const;
    };
}

#endif // CUSTOM_CLANS_CLANNEEDS_H
