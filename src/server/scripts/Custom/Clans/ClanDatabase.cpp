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

#include "ClanDatabase.h"
#include "ClanMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Position.h"
#include "StringFormat.h"
#include <memory>

namespace Clan
{
    void ClanDatabase::LoadRegistries()
    {
        // Registre des ressources : quelles Creatures/GameObjects sont gibier/eau/bois/lit.
        if (QueryResult result = WorldDatabase.Query("SELECT entry, object_kind, resource_type FROM custom_clan_resource"))
        {
            do
            {
                Field* f = result->Fetch();
                sClanMgr->AddResourceEntry(f[0].GetUInt32(), ResourceType(f[2].GetUInt8()), ObjectKind(f[1].GetUInt8()));
            } while (result->NextRow());
        }

        // Registre des gabarits de membres : quel entry = quel clan/genre/etape.
        if (QueryResult result = WorldDatabase.Query("SELECT entry, clan_id, gender, stage FROM custom_clan_member_template"))
        {
            do
            {
                Field* f = result->Fetch();
                sClanMgr->AddMemberTemplate(f[0].GetUInt32(), ClanId(f[1].GetUInt8()), Gender(f[2].GetUInt8()), LifeStage(f[3].GetUInt8()));
            } while (result->NextRow());
        }

        // Registre des modeles (displayId) par entry selon l'etape de vie.
        if (QueryResult result = WorldDatabase.Query("SELECT entry, display_child, display_adult, display_elder FROM custom_clan_display"))
        {
            do
            {
                Field* f = result->Fetch();
                sClanMgr->AddDisplaySet(f[0].GetUInt32(), f[1].GetUInt32(), f[2].GetUInt32(), f[3].GetUInt32());
            } while (result->NextRow());
        }

        // Phrases prononcees au debut d'une action.
        if (QueryResult result = WorldDatabase.Query("SELECT action_type, text FROM custom_clan_phrase"))
        {
            do
            {
                Field* f = result->Fetch();
                sClanMgr->AddPhrase(f[0].GetUInt8(), f[1].GetString());
            } while (result->NextRow());
        }

        // Effets RP (aura / sort / emote) joues au debut d'une action.
        if (QueryResult result = WorldDatabase.Query("SELECT action_type, aura, spell, emote FROM custom_clan_action_fx"))
        {
            do
            {
                Field* f = result->Fetch();
                sClanMgr->AddActionFx(f[0].GetUInt8(), f[1].GetUInt32(), f[2].GetUInt32(), f[3].GetUInt32());
            } while (result->NextRow());
        }

        // Afflictions possibles (aura + type : maladie / poison / saignement).
        if (QueryResult result = WorldDatabase.Query("SELECT aura, type FROM custom_clan_disease"))
        {
            do
            {
                Field* f = result->Fetch();
                sClanMgr->AddDisease(f[0].GetUInt32(), f[1].GetUInt8());
            } while (result->NextRow());
        }

        // Attribution des lits : entry du membre (creature_template) -> lit (spawnId du gameobject).
        // Par entry (et non par spawnId) pour couvrir aussi les nouveau-nes. Registre en lecture
        // seule (le serveur ne l'ecrit jamais) : l'attribution survit a la mort / au respawn.
        if (QueryResult result = WorldDatabase.Query("SELECT entry, bed_spawn_id FROM custom_clan_bed"))
        {
            do
            {
                Field* f = result->Fetch();
                sClanMgr->AddBedAssignment(f[0].GetUInt32(), f[1].GetUInt64());
            } while (result->NextRow());
        }
    }

    void ClanDatabase::LoadMembers()
    {
        QueryResult result = WorldDatabase.Query(
            "SELECT db_id, clan_id, gender, stage, age_days, hunger, thirst, energy, repro_urge, "
            "mother_id, father_id, repro_cd_days, is_birth, birth_entry, map, pos_x, pos_y, pos_z, orientation, display_id, qtable "
            "FROM custom_clan_member");
        if (!result)
            return;

        do
        {
            Field* f = result->Fetch();
            auto state = std::make_unique<MemberState>();
            state->dbId             = f[0].GetUInt64();
            state->clan             = ClanId(f[1].GetUInt8());
            state->gender           = Gender(f[2].GetUInt8());
            state->stage            = LifeStage(f[3].GetUInt8());
            state->ageDays          = f[4].GetUInt32();
            state->needs.hunger     = f[5].GetFloat();
            state->needs.thirst     = f[6].GetFloat();
            state->needs.energy     = f[7].GetFloat();
            state->needs.reproUrge  = f[8].GetFloat();
            state->motherId         = f[9].GetUInt64();
            state->fatherId         = f[10].GetUInt64();
            state->reproCooldownDays = f[11].GetUInt32();
            state->isBirth          = f[12].GetBool();
            state->birthEntry       = f[13].GetUInt32();
            state->mapId            = f[14].GetUInt32();
            state->home.Relocate(f[15].GetFloat(), f[16].GetFloat(), f[17].GetFloat(), f[18].GetFloat());
            state->displayId        = f[19].GetUInt32();
            state->mind.Deserialize(f[20].GetString());
            state->dirty            = false;

            // Un nouveau-ne garde son entry (= birthEntry) ; un membre place recupere la
            // sienne quand sa creature apparait (RegisterPlacedMember).
            if (state->isBirth)
                state->entry = state->birthEntry;

            sClanMgr->AddLoadedState(std::move(state));
        } while (result->NextRow());
    }

    void ClanDatabase::SaveMember(MemberState const& state, bool direct)
    {
        std::string query = Trinity::StringFormat(
            "REPLACE INTO custom_clan_member "
            "(db_id, clan_id, gender, stage, age_days, hunger, thirst, energy, repro_urge, "
            "mother_id, father_id, repro_cd_days, is_birth, birth_entry, map, pos_x, pos_y, pos_z, orientation, display_id, qtable) "
            "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, '{}')",
            state.dbId, uint32(state.clan), uint32(state.gender), uint32(state.stage), state.ageDays,
            state.needs.hunger, state.needs.thirst, state.needs.energy, state.needs.reproUrge,
            state.motherId, state.fatherId, state.reproCooldownDays, state.isBirth ? 1u : 0u, state.birthEntry,
            state.mapId, state.home.GetPositionX(), state.home.GetPositionY(), state.home.GetPositionZ(),
            state.home.GetOrientation(), state.displayId, state.mind.Serialize());

        if (direct)
            WorldDatabase.DirectExecute(query.c_str());
        else
            WorldDatabase.Execute(query.c_str());
    }

    void ClanDatabase::DeleteMember(uint64 dbId, bool direct)
    {
        std::string query = Trinity::StringFormat("DELETE FROM custom_clan_member WHERE db_id = {}", dbId);
        if (direct)
            WorldDatabase.DirectExecute(query.c_str());
        else
            WorldDatabase.Execute(query.c_str());
    }
}
