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

#ifndef KillRewarder_h__
#define KillRewarder_h__

#include "Define.h"
#include "IteratorPair.h"

class Player;
class Unit;
class Group;

class TC_GAME_API KillRewarder
{
public:
    KillRewarder(Trinity::IteratorPair<Player**> killers, Unit* victim, bool isBattleGround);

    void Reward();

    static void Reward(Player* player, Unit* victim, uint32 credit = 0U)
    {
        Player* players[] = { player };
        uint32 entry = credit ? credit : victim->GetEntry();
        KillRewarder(Trinity::IteratorPair(std::begin(players), std::end(players)), victim, false)._Reward(entry);
    }

private:
    void _InitXP(Player* player, Player const* killer);
    void _InitGroupData(Player const* killer);

    void _RewardHonor(Player* player);
    void _RewardXP(Player* player, float rate);
    void _RewardReputation(Player* player, float rate);
    void _RewardKillCredit(Player* player);
    void _RewardPlayer(Player* player, bool isDungeon);
    void _RewardGroup(Group const* group, Player const* killer);

    void _Reward(uint32 entry);

    Trinity::IteratorPair<Player**> _killers;
    Unit* _victim;
    float _groupRate;
    Player* _maxNotGrayMember;
    uint32 _count;
    uint32 _sumLevel;
    uint32 _xp;
    bool _isFullXP;
    uint8 _maxLevel;
    bool _isBattleGround;
    bool _isPvP;
};

#endif // KillRewarder_h__
