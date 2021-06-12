#ifndef AFFRAY_ISLE_H
#define AFFRAY_ISLE_H

#include "Map.h"

#define PLAYER_TELEPORT_PATH_SIZE   15
#define JAINA_PATH_01_SIZE          13
#define JAINA_PATH_02_SIZE          14
#define KLANNOC_PATH_01_SIZE         5
#define SPECTATORS_MAX_NUMBER        8
#define FIRES_MAX_NUMBER            12
#define ICE_WALLS_MAX_NUMBER        10

enum NPCs
{
    // NPCs
    NPC_JAINA_PROUDMOORE        = 100066,
    NPC_KALECGOS                = 100001,
    NPC_THRALL                  = 100089,
    NPC_KLANNOC_MACLEOD         = 100067,
    NPC_AFFRAY_SPECTATOR        = 100068,
    NPC_INVISIBLE_STALKER       = 32780,
    NPC_FOCUSING_IRIS           = 100069,
    NPC_WIND_ELEMENTAL          = 100091,
    NPC_ICE_WALL_TRIGGER        = 100092,
};

enum Talks
{
    TALK_SPECTATOR_FLEE         =  0,

    TALK_JAINA_01               =  8,
    TALK_JAINA_02               =  9,
    TALK_KLANNOC_03             =  0,
    TALK_JAINA_04               = 10,
    TALK_JAINA_05               = 11,
    TALK_JAINA_06               = 12,

    TALK_THRALL_07              = 0,
    TALK_JAINA_08               = 13,
    TALK_THRALL_09              = 1,
    TALK_THRALL_10              = 2,
    TALK_THRALL_11              = 3,
    TALK_JAINA_12               = 14,
    TALK_THRALL_13              = 4,
    TALK_JAINA_14               = 15,
    TALK_JAINA_15               = 16,
    TALK_THRALL_16              = 5,
    TALK_JAINA_17               = 17,
    TALK_THRALL_18              = 6,
    TALK_THRALL_19              = 7,
    TALK_THRALL_20              = 8,
    TALK_JAINA_21               = 18,
    TALK_THRALL_22              = 9,
    TALK_JAINA_23               = 19,
    TALK_JAINA_24               = 20,
    TALK_THRALL_25              = 10
};

enum Misc
{
    // Map ID
    MAPID_KALIMDOR              = 1,

    // Phasemask
    PHASEMASK_AFFRAY            = 16,

    // Morph
    MORPH_INVISIBLE_PLAYER      = 15880,

    // Game objects
    GOB_FIRE                    = 182592,
    GOB_ANTONIDAS_BOOK          = 500018,
    GOB_ICE_WALL                = 201385,

    // Mounts
    MOUNT_WHITE_WOLF_WOUNDED    = 19042
};

enum Events
{
    // Event
    START_AFFRAY_ISLE           = 1,
    START_SUMMON_THRALL,         
    START_THRALL_ARRIVES,
    START_BATTLE,

    // Introduction
    EVENT_INTRO_01              = 1000,
    EVENT_INTRO_02,
	EVENT_INTRO_03,
	EVENT_INTRO_04,
	EVENT_INTRO_05,
	EVENT_INTRO_06,
	EVENT_INTRO_07,
	EVENT_INTRO_08,
	EVENT_INTRO_09,
	EVENT_INTRO_10,
	EVENT_INTRO_11,
	EVENT_INTRO_12,
	EVENT_INTRO_13,
	EVENT_INTRO_14,
	EVENT_INTRO_15,
	EVENT_INTRO_16,
	
	// Thrall
	EVENT_THRALL_01,
	EVENT_THRALL_02,
	EVENT_THRALL_03,
	EVENT_THRALL_04,
	EVENT_THRALL_05,
	EVENT_THRALL_06,
	EVENT_THRALL_07,
	EVENT_THRALL_08,
	EVENT_THRALL_09,
	EVENT_THRALL_10,
	EVENT_THRALL_11,
	EVENT_THRALL_12,
	EVENT_THRALL_13,
	EVENT_THRALL_14,
	EVENT_THRALL_15,
	EVENT_THRALL_16,
	
	// Battle
	EVENT_BATTLE_01,
	EVENT_BATTLE_02,
	EVENT_BATTLE_03,
	EVENT_BATTLE_04,
	EVENT_BATTLE_05,
	EVENT_BATTLE_06,
	EVENT_BATTLE_07,
	EVENT_BATTLE_08,
	EVENT_BATTLE_09,
	EVENT_BATTLE_10,

    // Phases
    EVENT_SCHEDULE_PHASE_BLINK,
    EVENT_SCHEDULE_PHASE_ICE_FALL,

    // Combat
    EVENT_FROST_BOLT,
    EVENT_BLINK,
    EVENT_ICE_FALL
};

enum Spells
{
    SPELL_SMOKE_REVEAL          = 10389,
    SPELL_NATURE_CANALISATION   = 28892,
    SPELL_ARCANE_CLOUD          = 39952,
    SPELL_FROST_CANALISATION    = 45846,
    SPELL_IMMOLATE              = 48150,
    SPELL_VISUAL_TELEPORT       = 51347,
    SPELL_ICE_NOVA              = 56935,
    SPELL_FORCED_TELEPORT       = 70525,
    SPELL_FROST_EXPLOSION       = 73775,
    SPELL_PYROBLAST             = 100005,
    SPELL_ICE_LANCE             = 100007,
    SPELL_ARCANE_BARRAGE        = 100009,
    SPELL_LIGHTNING_BOLT        = 100026,
    SPELL_SIMPLE_TELEPORT       = 100032,
    SPELL_METEOR                = 100054,
    SPELL_WAVE_VISUAL           = 100060,
    SPELL_ARCANE_CANALISATION   = 100064,
    SPELL_STUNNED               = 100066,
    SPELL_WIND_ELEMENTAL_TOTEM  = 100113,
    SPELL_ICE_FALL              = 100115,
    SPELL_WAVE_CANALISATION     = 100117,
    SPELL_BLINK                 = 100119,
    SPELL_FROST_BOTL            = 100121,
    SPELL_ARCANE_EXPLOSION      = 100122,
    SPELL_TORNADO               = 100124,
    SPELL_ICY_GLARE             = 100125
};

const Position PlayerEntrancePos    = { -1710.83f, -4390.23f,  4.37f, 1.28f };
const Position JainaFinalPos        = { -1655.44f, -4246.56f,  1.77f, 6.13f };
const Position JainaBattlePos       = { -1667.71f, -4335.27f,  3.55f, 0.06f };
const Position FocusingIrisPos      = { -1643.60f, -4244.23f, 10.42f, 6.25f };
const Position FocusingIrisFxPos    = { -1644.40f, -4244.29f,  8.48f, 6.25f };

const Position JainaPath01[JAINA_PATH_01_SIZE] =
{
    { -1710.29f, -4377.18f, 4.55f, 5.02f },
    { -1710.29f, -4377.18f, 4.55f, 5.01f },
    { -1708.13f, -4382.60f, 4.60f, 5.44f },
    { -1704.00f, -4383.90f, 4.69f, 0.24f },
    { -1699.90f, -4381.63f, 4.91f, 0.79f },
    { -1697.59f, -4378.65f, 4.90f, 1.00f },
    { -1693.75f, -4372.47f, 4.88f, 0.93f },
    { -1691.10f, -4369.22f, 4.90f, 0.84f },
    { -1686.12f, -4364.10f, 4.95f, 0.87f },
    { -1681.42f, -4357.16f, 5.02f, 1.06f },
    { -1679.39f, -4353.49f, 4.90f, 1.09f },
    { -1676.57f, -4346.48f, 4.23f, 1.22f },
    { -1674.08f, -4341.96f, 3.67f, 0.91f }
};

const Position JainaPath02[JAINA_PATH_02_SIZE] =
{
    { -1674.14f, -4342.04f, 3.67f, 0.34f },
    { -1666.01f, -4337.26f, 3.80f, 1.00f },
    { -1664.60f, -4330.80f, 3.63f, 1.64f },
    { -1665.77f, -4324.19f, 3.43f, 1.84f },
    { -1670.08f, -4313.56f, 3.68f, 1.98f },
    { -1673.96f, -4306.12f, 3.75f, 2.13f },
    { -1677.87f, -4299.82f, 3.61f, 2.03f },
    { -1680.33f, -4291.97f, 3.30f, 1.72f },
    { -1680.23f, -4283.77f, 2.88f, 1.31f },
    { -1675.98f, -4275.16f, 2.74f, 0.89f },
    { -1670.29f, -4269.01f, 2.90f, 0.75f },
    { -1667.21f, -4266.14f, 2.99f, 0.75f },
    { -1660.96f, -4257.84f, 3.08f, 1.15f },
    { -1658.20f, -4249.91f, 2.01f, 1.30f }
};

const Position KlannocPath01[KLANNOC_PATH_01_SIZE] =
{
    { -1642.59f, -4347.65f, 6.50f, 2.42f },
    { -1644.83f, -4345.66f, 6.52f, 2.41f },
    { -1648.16f, -4342.63f, 4.88f, 2.40f },
    { -1652.18f, -4339.98f, 4.88f, 2.73f },
    { -1657.90f, -4338.15f, 4.45f, 2.85f }
};

const Position KlannocPath02[KLANNOC_PATH_01_SIZE] =
{
    { -1657.90f, -4338.15f, 4.45f, 2.85f },
    { -1652.18f, -4339.98f, 4.88f, 2.73f },
    { -1648.16f, -4342.63f, 4.88f, 2.40f },
    { -1644.83f, -4345.66f, 6.52f, 2.41f },
    { -1642.59f, -4347.65f, 6.50f, 2.42f }
};

const Position BuildingMeteorPos[FIRES_MAX_NUMBER] =
{
    { -1666.98f, -4368.48f, 15.09f, 0.09f },
    { -1654.66f, -4356.94f, 15.66f, 2.67f },
    { -1649.54f, -4363.14f, 18.56f, 5.10f },
    { -1645.32f, -4342.68f,  4.88f, 4.94f },
    { -1646.60f, -4353.18f, 16.78f, 0.91f },
    { -1656.34f, -4372.14f, 18.35f, 6.24f },
    { -1653.69f, -4363.88f, 17.80f, 0.02f },
    { -1632.16f, -4348.17f, 17.78f, 4.12f },
    { -1638.41f, -4354.31f, 28.32f, 4.35f },
    { -1642.21f, -4360.63f, 28.96f, 4.21f },
    { -1661.48f, -4360.98f,  4.88f, 5.34f },
    { -1667.19f, -4367.37f,  4.88f, 5.63f }
};

const float IceWallsPos[ICE_WALLS_MAX_NUMBER][8] =
{
    { -1632.79f, -4332.48f,  4.83f, 2.99f, 0.0f, 0.0f, -0.99f, -0.07f },
    { -1683.79f, -4269.71f,  2.69f, 1.58f, 0.0f, 0.0f, -0.71f, -0.70f },
    { -1634.66f, -4277.96f, -1.71f, 3.76f, 0.0f, 0.0f, -0.95f,  0.30f },
    { -1702.05f, -4380.88f,  4.73f, 4.08f, 0.0f, 0.0f, -0.89f,  0.45f },
    { -1657.94f, -4378.16f,  5.13f, 5.53f, 0.0f, 0.0f, -0.36f,  0.93f },
    { -1711.58f, -4330.22f,  4.21f, 3.01f, 0.0f, 0.0f, -0.99f, -0.06f },
    { -1718.14f, -4273.43f, -1.53f, 5.34f, 0.0f, 0.0f, -0.45f,  0.89f },
    { -1616.15f, -4297.27f, -8.34f, 3.19f, 0.0f, 0.0f, -0.99f,  0.02f },
    { -1731.68f, -4360.19f,  3.67f, 0.21f, 0.0f, 0.0f, -0.10f, -0.99f },
    { -1684.69f, -4401.76f, 18.98f, 1.41f, 0.0f, 0.0f, -0.65f, -0.75f }
};

class KlannocBurning : public BasicEvent
{
    public:
    KlannocBurning(Creature* owner, Creature* jaina) : owner(owner), jaina(jaina), stage(0)
    {
        args.SetTriggerFlags(TRIGGERED_CAST_DIRECTLY);
    }

    bool Execute(uint64 eventTime, uint32 /*updateTime*/) override
    {
        switch (stage)
        {
            case 0:
                owner->SetWalk(false);
                owner->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                return NextEvent(Milliseconds(eventTime), 500ms);
            case 1:
                jaina->CastSpell(owner, SPELL_PYROBLAST);
                return NextEvent(Milliseconds(eventTime), 1890ms);
            case 2:
                owner->RemoveAllAuras();
                owner->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                owner->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_COWER);
                owner->AddAura(SPELL_IMMOLATE, owner);
                owner->GetMotionMaster()->Clear();
                owner->GetMotionMaster()->MoveSmoothPath(0, KlannocPath02, KLANNOC_PATH_01_SIZE, false);
                return NextEvent(Milliseconds(eventTime), 3s);
            case 3:
                SummonMeteors();
                return NextEvent(Milliseconds(eventTime), 4s);
            case 4:
                owner->KillSelf();
                return true;
            default:
                break;
        }
        return true;
    }

    private:
    Creature* owner;
    Creature* jaina;
    uint8 stage;
    CastSpellExtraArgs args;

    bool NextEvent(Milliseconds eventTime, Milliseconds time)
    {
        stage++;
        owner->m_Events.AddEvent(this, eventTime + time);
        return false;
    }

    void SummonMeteors()
    {
        for (uint8 i = 0; i < FIRES_MAX_NUMBER; i++)
        {
            if (Creature* stalker = jaina->SummonCreature(NPC_INVISIBLE_STALKER, BuildingMeteorPos[i], TEMPSUMMON_TIMED_DESPAWN, 5s))
            {
                jaina->SummonGameObject(GOB_FIRE, BuildingMeteorPos[i], QuaternionData(), 0s);
                jaina->CastSpell(stalker->GetPosition(), SPELL_METEOR, args);
            }
        }
    }
};

class SpectatorDeath : public BasicEvent
{
    public:
    SpectatorDeath(Creature* owner) : owner(owner)
    {
    }

    bool Execute(uint64 eventTime, uint32 /*updateTime*/) override
    {
        owner->KillSelf();
        return true;
    }

    private:
    Creature* owner;
};

#endif
