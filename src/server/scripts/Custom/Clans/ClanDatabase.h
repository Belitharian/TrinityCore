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

// Couche de persistance du module Clans. Les registres statiques sont lus dans
// la base "world" (renseignes par l'admin) ; l'etat runtime des membres vit dans
// la base "characters".

#ifndef CUSTOM_CLANS_CLANDATABASE_H
#define CUSTOM_CLANS_CLANDATABASE_H

#include "Define.h"

namespace Clan
{
    struct MemberState;

    namespace ClanDatabase
    {
        // Registres (world) : ressources declarees + gabarits de membres.
        void LoadRegistries();
        // Etats persistants des membres (characters).
        void LoadMembers();

        // Ecrit / met a jour un membre. direct = execution synchrone (utile a l'arret).
        void SaveMember(MemberState const& state, bool direct);
        // Supprime un membre (mort definitive).
        void DeleteMember(uint64 dbId, bool direct);
    }
}

#endif // CUSTOM_CLANS_CLANDATABASE_H
