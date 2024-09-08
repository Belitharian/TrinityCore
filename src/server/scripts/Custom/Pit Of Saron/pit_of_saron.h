#ifndef PIT_OF_SARON_CUSTOM_H_
#define PIT_OF_SARON_CUSTOM_H_

#include "CreatureAIImpl.h"
#include "EventProcessor.h"
#include "Position.h"

#define PoSScriptName "instance_pit_of_saron_custom"
#define DataHeader "PSC"

uint32 const EncounterCount = 3;

enum Datas
{
    // Encounter states and GUIDs
    DATA_GARFROST,
    DATA_ICK,
    DATA_TYRANNUS,

    // GUIDs
    DATA_RIMEFANG,
    DATA_KRICK,
    DATA_JAINA_1,
    DATA_JAINA_2,
    DATA_ELANDRA,
    DATA_KORELN,
    DATA_MARTIN_VICTUS_1,
    DATA_MARTIN_VICTUS_2,
    DATA_TYRANNUS_EVENT,
    DATA_CAVERN_ACTIVE,

    // Gobs
    DATA_ARCANE_BARRIER,

    // Phases
    DATA_PHASE,
};

enum Talks
{
    // Introduction
    SAY_TYRANNUS_01                         = 15,
    SAY_TYRANNUS_02                         = 16,
    SAY_JAINA_03                            = 6,
    SAY_TYRANNUS_04                         = 17,
    SAY_TYRANNUS_05                         = 18,
    SAY_JAINA_06                            = 7,
    SAY_TYRANNUS_07                         = 19,
    SAY_JAINA_08                            = 8,
    SAY_JAINA_09                            = 9,
    SAY_JAINA_10                            = 10,
};

enum Creatures
{
    NPC_GARFROST                            = 36494,
    NPC_KRICK                               = 36477,
    NPC_ICK                                 = 36476,
    NPC_TYRANNUS                            = 36658,
    NPC_RIMEFANG                            = 36661,

    NPC_JAINA_PART1                         = 36993,
    NPC_JAINA_PART2                         = 38188,
    NPC_ELANDRA                             = 37774,
    NPC_KORELN                              = 37582,
    NPC_CHAMPION_1_ALLIANCE                 = 37496,
    NPC_CHAMPION_2_ALLIANCE                 = 37497,
    NPC_CHAMPION_3_ALLIANCE                 = 37498,
    NPC_CORRUPTED_CHAMPION                  = 36796,

    NPC_DEATHWHISPER_NECROLYTE              = 36788,

    NPC_ALLIANCE_SLAVE_1                    = 36764,
    NPC_ALLIANCE_SLAVE_2                    = 36765,
    NPC_ALLIANCE_SLAVE_3                    = 36766,
    NPC_ALLIANCE_SLAVE_4                    = 36767,
    NPC_FREED_SLAVE_1_ALLIANCE              = 37575,
    NPC_FREED_SLAVE_2_ALLIANCE              = 37572,
    NPC_FREED_SLAVE_3_ALLIANCE              = 37576,
    NPC_RESCUED_SLAVE_ALLIANCE              = 36888,
    NPC_MARTIN_VICTUS_1                     = 37591,
    NPC_MARTIN_VICTUS_2                     = 37580,

    NPC_CAVERN_EVENT_TRIGGER                = 32780
};

enum Misc
{
    // GameObjects
    GO_SARONITE_ROCK                        = 196485,
    GO_ICE_WALL                             = 201885,
    GO_HALLS_OF_REFLECTION_PORTCULLIS       = 201848,
    GO_ARCANE_BARRIER                       = 254046,

    // Factions
    FACTION_HOSTILE                         = 1771,
};

enum Phases
{
    None,
    Paused,
    Introduction,
    FreeMartinVictus,
    TyrannusIntroduction
};

// Positions
static Position const TyrannusPoint01   = { 1018.28f, 179.12f, 651.67f, 5.23f };
static Position const JainaPoint01      = { 1065.57f,  65.33f, 631.96f, 1.39f };
static Position const ElandraPoint01    = { 1071.14f,  63.69f, 631.85f, 1.53f };
static Position const KorelnPoint01     = { 1061.23f,  64.26f, 631.76f, 1.40f };

// Paths

WaypointPath const JainaPath01 =
{
    1,
    {
        { 1, 1065.57f, 65.33f, 631.96f, 1.39f },
        { 2, 1067.21f,  74.28f, 630.88f, 1.37f },
        { 3, 1068.81f,  84.22f, 631.26f, 1.59f },
        { 4, 1067.23f,  92.15f, 631.31f, 1.89f },
        { 5, 1063.90f,  99.53f, 630.99f, 2.10f },
        { 6, 1059.20f, 106.76f, 629.34f, 2.17f }
    },
    WaypointMoveType::Run,
    WaypointPathFlags::ExactSplinePath
};

WaypointPath const ElandraPath01 =
{
    1,
    {
        { 1, 1071.14f,  63.69f, 631.85f, 1.53f },
        { 2, 1071.89f,  71.77f, 630.97f, 1.40f },
        { 3, 1073.08f,  80.18f, 631.00f, 1.53f },
        { 4, 1072.31f,  88.22f, 631.45f, 1.75f },
        { 5, 1069.86f,  96.88f, 631.01f, 1.92f },
        { 6, 1063.52f, 109.13f, 629.18f, 2.11f }
    },
    WaypointMoveType::Run,
    WaypointPathFlags::ExactSplinePath
};

WaypointPath const KorelnPath01 =
{
    1,
    {
        { 1, 1061.23f,  64.26f, 631.76f, 1.40f },
        { 2, 1062.31f,  70.46f, 631.11f, 1.35f },
        { 3, 1063.62f,  79.27f, 631.01f, 1.56f },
        { 4, 1062.95f,  85.54f, 631.35f, 1.83f },
        { 5, 1060.09f,  92.54f, 630.64f, 2.05f },
        { 6, 1056.61f, 105.54f, 628.92f, 2.14f }
    },
    WaypointMoveType::Run,
    WaypointPathFlags::ExactSplinePath
};

WaypointPath const MartinPath01 =
{
    1,
    {
        { 1, 1068.41f,  68.79f, 631.44f, 4.53f },
        { 2, 1069.50f,  77.94f, 630.95f, 4.73f },
        { 3, 1068.33f,  90.24f, 631.54f, 4.92f },
        { 4, 1065.50f,  98.93f, 630.99f, 5.09f },
        { 5, 1061.92f, 106.96f, 629.45f, 2.02f }
    },
    WaypointMoveType::Run,
    WaypointPathFlags::ExactSplinePath
};

class Creature;

template <class AI, class T>
inline AI* GetPitOfSaronCustomAI(T* obj)
{
    return GetInstanceAI<AI>(obj, PoSScriptName);
}

#define RegisterPitOfSaronCustomCreatureAI(ai_name) RegisterCreatureAIWithFactory(ai_name, GetPitOfSaronCustomAI)

#endif // PIT_OF_SARON_CUSTOM_H_
