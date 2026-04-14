#ifndef FAKE_PARTY_H
#define FAKE_PARTY_H

#include "Group.h"
#include "GroupMgr.h"
#include "PartyPackets.h"

class Player;
class Unit;

class FakeParty
{
    static constexpr uint32 FAKE_PARTY_UPDATE_INTERVAL = 3000;
    static constexpr uint64 FAKE_PARTY_GUID_LOW = 0xFFFFF000;

public:
    /// Constructs a FakeParty tied to the given NPC owner.
    /// @param owner  The NPC whose stats will be displayed in the party frame.
    explicit FakeParty(Unit* owner)
        : _owner(owner)
        , _fakePartyActive(false)
        , _partyUpdateTimer(FAKE_PARTY_UPDATE_INTERVAL)
    {
    }

    ~FakeParty() = default;

    FakeParty(FakeParty const&) = delete;
    FakeParty& operator=(FakeParty const&) = delete;

    /// Sends SMSG_PARTY_UPDATE to create the fake party frame on the client.
    void SendFakePartyUpdate(Player* player);

    /// Sends SMSG_PARTY_MEMBER_FULL_STATE with the NPC's current stats.
    void SendFakePartyMemberState(Player* player);

    /// Sends SMSG_PARTY_UPDATE with GROUP_FLAG_DESTROYED to remove the frame.
    void DestroyFakeParty(Player* player);

    /// Periodic update — call from the AI's UpdateAI(uint32 diff).
    /// Sends MemberState at fixed interval to keep the frame in sync.
    void Update(uint32 diff, Player* player);

    /// Returns true if the fake party frame is currently active.
    bool IsActive() const { return _fakePartyActive; }

private:
    // Builds the deterministic party GUID from the creature entry.
    ObjectGuid BuildPartyGuid() const;

    Unit* _owner;
    bool _fakePartyActive;
    int32 _partyUpdateTimer;
};

#endif // FAKE_PARTY_H
