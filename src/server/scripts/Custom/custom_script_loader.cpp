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

// This is where scripts' loading functions should be declared:
// Theramore
void AddSC_theramore();
void AddSC_theramore_waves_invoker();
void AddSC_npc_priest();
void AddSC_npc_archmages();
void AddSC_npc_shaman();
void AddSC_npc_felcaster();
void AddSC_npc_hag();
void AddSC_theramore_wounded_event();
void AddSC_npc_warrior();
void AddSC_npc_paladin();
void AddSC_theramore_ruins();
void AddSC_dalaran_jaina_anduin();
void AddSC_dalaran_jaina_purge();
void AddSC_npcs_sunreaver();
void AddSC_action_update_phase();
void AddSC_jaina_affray_isle();
void AddSC_stormwind_final_event();
void AddSC_npc_water_elemental();

// Tristam Catacombs
void AddSC_instance_tristam_catacombs();
void AddSC_tristam_catacombs();
void AddSC_npc_antonn_grave();
void AddSC_npc_skeletons();

// The name of this function should match:
void AddCustomScripts()
{
    // Theramore
    AddSC_theramore();
    AddSC_theramore_waves_invoker();
    AddSC_npc_priest();
	AddSC_npc_archmages();
    AddSC_npc_shaman();
    AddSC_npc_felcaster();
    AddSC_npc_hag();
    AddSC_theramore_wounded_event();
    AddSC_npc_warrior();
    AddSC_npc_paladin();
    AddSC_theramore_ruins();
    AddSC_dalaran_jaina_anduin();
    AddSC_dalaran_jaina_purge();
    AddSC_npcs_sunreaver();
    AddSC_action_update_phase();
    AddSC_jaina_affray_isle();
    AddSC_stormwind_final_event();
    AddSC_npc_water_elemental();

    // Tristam Catacombs
    AddSC_instance_tristam_catacombs();
    AddSC_tristam_catacombs();
    AddSC_npc_antonn_grave();
    AddSC_npc_skeletons();
}
