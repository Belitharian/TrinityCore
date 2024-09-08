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

void AddSC_ruins_of_theramore();
void AddSC_scenario_ruins_of_theramore();
void AddSC_npcs_ruins_of_theramore();

void AddSC_dalaran_purge();
void AddSC_scenario_dalaran_purge();
void AddSC_npcs_dalaran_purge();

void AddSC_instance_dalaran_convo();
void AddSC_npcs_dalaran_convo();

void AddSC_instance_pit_of_saron_custom();
void AddSC_pit_of_saron_custom();
void AddSC_boss_tyrannus_custom();

// The name of this function should match:
// void Add${NameOfDirectory}Scripts()
void AddCustomScripts()
{
    AddSC_battle_for_theramore();
    AddSC_scenario_battle_for_theramore();
    AddSC_npcs_battle_for_theramore();

    AddSC_ruins_of_theramore();
    AddSC_scenario_ruins_of_theramore();
    AddSC_npcs_ruins_of_theramore();

    AddSC_dalaran_purge();
    AddSC_scenario_dalaran_purge();
    AddSC_npcs_dalaran_purge();

    AddSC_instance_dalaran_convo();
    AddSC_npcs_dalaran_convo();

    AddSC_instance_pit_of_saron_custom();
    AddSC_pit_of_saron_custom();
    AddSC_boss_tyrannus_custom();
}
