#include "PartyPackets.h"
#include "PhasingHandler.h"
#include "Log.h"
#include "FakeParty.h"

using namespace WorldPackets::Party;

ObjectGuid FakeParty::BuildPartyGuid() const
{
    return ObjectGuid::Create<HighGuid::Party>(FAKE_PARTY_GUID_LOW + _owner->GetEntry());
}

/// Sends a fake SMSG_PARTY_UPDATE to make the client display a party frame
/// with the player and this NPC, without creating a real Group object.
void FakeParty::SendFakePartyUpdate(Player* player)
{
    if (!_owner || !player)
        return;

    ObjectGuid const fakePartyGuid = BuildPartyGuid();

    PartyUpdate partyUpdate;
    partyUpdate.PartyFlags = GROUP_FLAG_NONE;
    partyUpdate.PartyIndex = GROUP_CATEGORY_HOME;
    partyUpdate.PartyType = GROUP_TYPE_NORMAL;
    partyUpdate.PartyGUID = fakePartyGuid;
    partyUpdate.LeaderGUID = player->GetGUID();
    partyUpdate.LeaderFactionGroup = Player::GetFactionGroupForRace(player->GetRace());
    partyUpdate.SequenceNum = player->NextGroupUpdateSequenceNumber(GROUP_CATEGORY_HOME);
    partyUpdate.MyIndex = 0;

    // Member 0 : le joueur (leader)
    {
        PartyPlayerInfo& info = partyUpdate.PlayerList.emplace_back();
        info.GUID = player->GetGUID();
        info.Name = player->GetName();
        info.Class = player->GetClass();
        info.FactionGroup = Player::GetFactionGroupForRace(player->GetRace());
        info.Connected = true;
        info.Subgroup = 0;
        info.Flags = 0;
        info.RolesAssigned = 0;
    }

    // Member 1 : le PNJ
    {
        PartyPlayerInfo& info = partyUpdate.PlayerList.emplace_back();
        info.GUID = _owner->GetGUID();
        info.Name = _owner->GetName();
        info.Class = CLASS_MAGE;
        info.FactionGroup = Player::GetFactionGroupForRace(player->GetRace());
        info.Connected = true;
        info.Subgroup = 0;
        info.Flags = 0;
        info.RolesAssigned = 0;
    }

    partyUpdate.LootSettings.emplace();
    partyUpdate.LootSettings->Method = 0;
    partyUpdate.LootSettings->Threshold = 2;

    partyUpdate.DifficultySettings.emplace();
    partyUpdate.DifficultySettings->DungeonDifficultyID = 1;
    partyUpdate.DifficultySettings->RaidDifficultyID = 14;
    partyUpdate.DifficultySettings->LegacyRaidDifficultyID = 3;

    player->SendDirectMessage(partyUpdate.Write());

    _fakePartyActive = true;

    TC_LOG_DEBUG("scripts", "FakeParty::SendFakePartyUpdate - Party frame created for player {} with creature {}",
        player->GetName(), _owner->GetEntry());
}

/// Sends SMSG_PARTY_MEMBER_FULL_STATE with the NPC's current stats
/// so the client can display health/mana/position on the party frame.
void FakeParty::SendFakePartyMemberState(Player* player)
{
    if (!_owner || !player)
        return;

    if (!_fakePartyActive)
        return;

    PartyMemberFullState packet;
    packet.ForEnemy = false;
    packet.MemberGuid = _owner->GetGUID();

    // Status
    packet.MemberStats.Status = MEMBER_STATUS_ONLINE;
    if (!_owner->IsAlive())
        packet.MemberStats.Status |= MEMBER_STATUS_DEAD;

    // Niveau
    packet.MemberStats.Level = _owner->GetLevel();

    // Vie
    packet.MemberStats.CurrentHealth = _owner->GetHealth();
    packet.MemberStats.MaxHealth = _owner->GetMaxHealth();

    // Mana
    packet.MemberStats.PowerType = POWER_MANA;
    packet.MemberStats.PowerDisplayID = 0;
    packet.MemberStats.CurrentPower = _owner->GetPower(POWER_MANA);
    packet.MemberStats.MaxPower = _owner->GetMaxPower(POWER_MANA);

    // Position
    packet.MemberStats.ZoneID = _owner->GetZoneId();
    packet.MemberStats.PositionX = int16(_owner->GetPositionX());
    packet.MemberStats.PositionY = int16(_owner->GetPositionY());
    packet.MemberStats.PositionZ = int16(_owner->GetPositionZ());

    // Type de groupe
    packet.MemberStats.PartyType[0] = GROUP_TYPE_NORMAL;
    packet.MemberStats.PartyType[1] = 0;

    // Phases
    PhasingHandler::FillPartyMemberPhase(&packet.MemberStats.Phases, _owner->GetPhaseShift());

    // Auras visibles
    for (AuraApplication const* aurApp : _owner->GetVisibleAuras())
    {
        if (!aurApp)
            continue;

        PartyMemberAuraStates& aura = packet.MemberStats.Auras.emplace_back();
        aura.SpellID = aurApp->GetBase()->GetId();
        aura.ActiveFlags = aurApp->GetEffectMask();
        aura.Flags = aurApp->GetFlags();

        if (aurApp->GetFlags() & AFLAG_SCALABLE)
        {
            for (AuraEffect const* aurEff : aurApp->GetBase()->GetAuraEffects())
            {
                if (aurEff && aurApp->HasEffect(aurEff->GetEffIndex()))
                    aura.Points.push_back(float(aurEff->GetAmount()));
            }
        }
    }

    player->SendDirectMessage(packet.Write());
}

/// Sends a SMSG_PARTY_UPDATE with GROUP_FLAG_DESTROYED to remove the fake party frame.
void FakeParty::DestroyFakeParty(Player* player)
{
    if (!_owner || !player)
        return;

    if (!_fakePartyActive)
        return;

    ObjectGuid const fakePartyGuid = BuildPartyGuid();

    PartyUpdate partyUpdate;
    partyUpdate.PartyFlags = GROUP_FLAG_DESTROYED;
    partyUpdate.PartyIndex = GROUP_CATEGORY_HOME;
    partyUpdate.PartyType = GROUP_TYPE_NONE;
    partyUpdate.PartyGUID = fakePartyGuid;
    partyUpdate.MyIndex = -1;
    partyUpdate.SequenceNum = player->NextGroupUpdateSequenceNumber(GROUP_CATEGORY_HOME);

    player->SendDirectMessage(partyUpdate.Write());

    _fakePartyActive = false;

    TC_LOG_DEBUG("scripts", "FakeParty::DestroyFakeParty - Party frame destroyed for player {} with creature {}",
        player->GetName(), _owner->GetEntry());
}

/// Periodic update to keep the party frame in sync with the NPC's state.
/// Call this from UpdateAI(uint32 diff) of the owning creature script.
void FakeParty::Update(uint32 diff, Player* player)
{
    if (!_fakePartyActive)
        return;

    _partyUpdateTimer -= static_cast<int32>(diff);
    if (_partyUpdateTimer <= 0)
    {
        SendFakePartyMemberState(player);
        _partyUpdateTimer = FAKE_PARTY_UPDATE_INTERVAL;
    }
}
