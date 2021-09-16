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
void AddSC_battle_for_theramore();
void AddSC_scenario_battle_for_theramore();
void AddSC_npcs_battle_for_theramore();

void AddSC_npc_archmage_fire();
void AddSC_npc_archmage_frost();
void AddSC_npc_archmage_arcane();
void AddSC_npc_priest();

// The name of this function should match:
// void Add${NameOfDirectory}Scripts()
void AddCustomScripts()
{
    AddSC_battle_for_theramore();
    AddSC_scenario_battle_for_theramore();
    AddSC_npcs_battle_for_theramore();

    AddSC_npc_archmage_fire();
    AddSC_npc_archmage_frost();
    AddSC_npc_archmage_arcane();
    AddSC_npc_priest();
}
