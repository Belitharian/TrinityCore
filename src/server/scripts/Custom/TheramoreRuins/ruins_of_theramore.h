/*
 * Ruins of Theramore - Scenario header
 *
 * Definitions partagees par le scenario "Les Ruines de Theramore" :
 *   - Constantes (IDs creatures / spells / gameobjects / events / criteres)
 *   - Positions et waypoints pour les deplacements scriptes
 *   - Phases du scenario (RFTPhases) et helper de registration d'AI
 *
 * Flow general :
 *   Phase 1 : FindJaina_Isle   -> les joueurs trouvent Jaina sur l'ilot
 *   Phase 2 : FindJaina_Crater -> dialogue au cratere de la teleportation
 *   Phase 3 : Standards        -> retour a Theramore + nettoyage + combat de la Horde
 *   Phase 4 : LeaveTheRuins    -> Jaina ouvre le portail vers Stormwind
 *
 * NOTE encodage : tous les commentaires sont en francais SANS accents
 * (l'encodage TrinityCore casse sur les caracteres non-ASCII).
 */

#ifndef RUINS_OF_THERAMORE_H_
#define RUINS_OF_THERAMORE_H_

#include "CreatureAIImpl.h"
#include "Position.h"

#define RFTScriptName "scenario_ruins_of_theramore"
#define DataHeader "RFT"

//#define CUSTOM_DEBUG

// Nombre d'elementaires d'eau invoques par Jaina pendant le combat final.
constexpr auto ELEMENTALS_SIZE = 2;

// =========================================================================
// Constantes de gameplay et cosmetiques
// =========================================================================
// Extraites des magic numbers presents dans le .cpp pour gagner en lisibilite
// et permettre un tuning rapide. Modifier ici impacte tout le scenario.

constexpr float CREATURE_BOUNDING_RADIUS        = 36.0f;     // Bounding applique a chaque NPC du scenario (range de targeting)
constexpr float ELEMENTAL_BOUNDING_RADIUS       = 4.0f;      // Plus petit pour les elementaires d'eau (le tank principal des hordes)
constexpr float KALECGOS_WALK_SPEED_RATE        = 0.6f;      // Kalecgos marche lentement pour synchroniser avec le dialogue de Jaina
constexpr float DUMMY_SCALE_SMALL               = 0.6f;      // Echelle initiale du dummy de l'iris (avant l'explosion)
constexpr float DUMMY_SCALE_LARGE               = 5.0f;      // Echelle finale pour l'effet visuel d'explosion arcanique
constexpr float ZEPPELIN_SPEED_RATE             = 5.0f;      // Vitesse du zeppelin de bombardement (passage rapide en background)
constexpr float HORDE_SPLIT_Y_THRESHOLD         = -4468.18f; // Axe Y separant les hordes attaquant l'elementaire 0 vs 1
constexpr float JAINA_TRIGGER_DISTANCE_DEFAULT  = 10.0f;     // Distance par defaut pour declencher les evenements de proximite
constexpr float JAINA_TRIGGER_DISTANCE_CRATER   = 50.0f;     // Distance elargie utilisee au cratere (visibilite a longue portee)
constexpr float TELEPORT_SPREAD_RADIUS          = 12.0f;     // Rayon de dispersion lors du teleport groupe des joueurs
constexpr float JAINA_KNEEL_APPROACH_DIST       = 1.3f;      // Distance d'arret de Jaina lorsqu'elle s'approche du warlord
constexpr float JAINA_BROKEN_GLASS_APPROACH     = 0.8f;      // Distance d'arret devant le verre brise

// =========================================================================
// Creatures (entry IDs)
// =========================================================================
enum RFTCreatures
{
	NPC_INVISIBLE_STALKER               = 32780,
	NPC_HEDRIC_EVENCANE                 = 58840,
	NPC_THERAMORE_OFFICER               = 58913,
	NPC_THERAMORE_FAITHFUL              = 59595,
	NPC_THERAMORE_ARCANIST              = 59596,
	NPC_KALECGOS                        = 64565,
	NPC_JAINA_PROUDMOORE                = 64727,
	NPC_ROKNAH_GRUNT                    = 64732,
	NPC_ROKNAH_LOA_SINGER               = 64733,
	NPC_ROKNAH_HAG                      = 64734,
	NPC_ROKNAH_WARLORD                  = 65442,
	NPC_ROKNAH_SKIRMISHER               = 65494,
	NPC_ROKNAH_FELCASTER                = 65507,
	NPC_WATER_ELEMENTAL                 = 65680,
	NPC_GENERAL_TIRAS_ALAN              = 100007,
	NPC_ADMIRAL_AUBREY                  = 121953,
	NPC_BOMBARDING_ZEPPELIN             = 136957,
	NPC_ARCHMAGE_TERVOSH                = 500000,
	NPC_KINNDY_SPARKSHINE               = 500001,
	NPC_DEAD_ROKNAH_TROOP               = 500015
};

// =========================================================================
// Data IDs (utilises par InstanceScript::GetCreature / GetGameObject)
// =========================================================================
enum RFTData
{
	DATA_JAINA_PROUDMOORE               = 1,
	DATA_KALECGOS,
	DATA_KINNDY_SPARKSHINE,
	DATA_ROKNAH_WARLORD,
	DATA_BOMBARDING_ZEPPELIN,
	DATA_BROKEN_GLASS,
	DATA_SCENARIO_PHASE                            // Lu/ecrit pour piloter les transitions de phase
};

// =========================================================================
// Talks (groupId / textId des dialogues scriptes)
// =========================================================================
// Les IDs reels (right side) correspondent aux entrees creature_text en BDD.
// Les noms (left side) suivent l'ordre du scenario.
enum RFTTalks
{
	// --- Apres bataille (Phase 1, ilot) ---
	SAY_AFTER_BATTLE_KALECGOS_01        = 10,
	SAY_AFTER_BATTLE_JAINA_02           = 0,
	SAY_AFTER_BATTLE_JAINA_03           = 1,
	SAY_AFTER_BATTLE_KALECGOS_04        = 11,
	SAY_AFTER_BATTLE_JAINA_05           = 2,
	SAY_AFTER_BATTLE_KALECGOS_06        = 12,
	SAY_AFTER_BATTLE_JAINA_07           = 3,
	SAY_AFTER_BATTLE_KALECGOS_08        = 13,
	SAY_AFTER_BATTLE_JAINA_09           = 4,
	SAY_AFTER_BATTLE_KALECGOS_10        = 14,
	SAY_AFTER_BATTLE_KALECGOS_11        = 15,
	SAY_AFTER_BATTLE_JAINA_12           = 5,
	SAY_AFTER_BATTLE_KALECGOS_13        = 16,

	// --- Protection de l'iris (Phase 2-3, cratere) ---
	SAY_IRIS_PROTECTION_JAINA_01        = 6,
	SAY_IRIS_PROTECTION_JAINA_02        = 7,
	SAY_IRIS_PROTECTION_JAINA_03        = 8,
	SAY_IRIS_PROTECTION_JAINA_04        = 9,
	SAY_IRIS_PROTECTION_JAINA_05        = 10,
	SAY_IRIS_PROTECTION_JAINA_06        = 0,     // Dit par le Warlord (groupId 0 dans sa table)
	SAY_IRIS_PROTECTION_JAINA_07        = 11,
	SAY_IRIS_PROTECTION_JAINA_08        = 1,
	SAY_IRIS_PROTECTION_JAINA_09        = 12,
	SAY_IRIS_PROTECTION_JAINA_10        = 2,

	// --- Sortie des ruines (Phase 4) ---
	SAY_LEAVE_THE_RUINS_JAINA_01        = 13,
	SAY_LEAVE_THE_RUINS_JAINA_02        = 14
};

// =========================================================================
// Sorts utilises dans le scenario (cosmetiques + combat)
// =========================================================================
enum RFTSpells
{
	// OLD
	//SPELL_SCREEN_FX                   = 337213,

	SPELL_FEATHER_FALL                  = 130,
	SPELL_TELEPORT_VISUAL_ONLY          = 51347,
	SPELL_ECHO_OF_ALUNETH_SPAWN         = 211768,
	SPELL_ALUNETH_FREED_EXPLOSION       = 225253,
	SPELL_ALUNETH_DRINKS                = 212220,
	SPELL_WATER_BOSS_ENTRANCE           = 240261,
	SPELL_COSMETIC_ARCANE_DISSOLVE      = 254799,
	SPELL_SHIMMERDUST                   = 278917,
	SPELL_SKYBOX_EFFECT_ENTRANCE        = 148137,    // Skybox affichee avant la phase combat
	SPELL_BURNING                       = 282051,
	SPELL_ARCANE_CHANNELING             = 294676,
	SPELL_EMPOWERED_SUMMON              = 303681,
	SPELL_SKYBOX_EFFECT_RUINS           = 310302,    // Skybox affichee a partir de la phase ruins
	SPELL_GLACIAL_SPIKE_COSMETIC        = 346559,
	SPELL_THALYSSRA_SPAWNS              = 302492,
	SPELL_EXPLOSIVE_BRAND               = 374567,    // Marque appliquee aux hordes a la mort du warlord
	SPELL_EXPLOSIVE_BRAND_DAMAGE        = 374570,
	SPELL_ARCANE_DEBUFF_VISUAL          = 436058,
	SPELL_DISSOLVE_ARCANE_VISUAL        = 460434,
	SPELL_SUMMON_WATER_ELEMENTALS       = 460994,
	SPELL_COSMETIC_ARCANE_ENERGY        = 1233488,
	SPELL_COSMETIC_PURPLE_VERTEX_STATE  = 1247717
};

// =========================================================================
// Constantes diverses : GO, sons, data IDs internes, events, criteres, points
// =========================================================================
enum RFTMisc
{
	// GameObjects
	GOB_BROKEN_GLASS                    = 349872,
	GOB_PORTAL_TO_STORMWIND             = 353823,

	// Sounds
	SOUND_ZEPPELIN_FLIGHT               = 85549,

	// Data IDs internes a npc_jaina_ruins::SetData
	DATA_SET_DISTANCE                   = 1,
	DATA_CANCEL_GROUP                   = 2,
	DATA_PHASE_COMBAT                   = 3,

	// Weather
	WEATHER_ARCANE_BUILD                = 182,

	// Events de scenario (TriggerGameEvent / SetData)
	EVENT_FIND_JAINA_01                 = 65811,    // Ilot : declenche le dialogue d'apres bataille
	EVENT_HELP_KALECGOS                 = 65812,
	EVENT_FIND_JAINA_02                 = 65813,    // Cratere
	EVENT_BACK_TO_SENDER                = 65814,
	EVENT_WARLORD_ROKNAH_SLAIN          = 65815,
	EVENT_JAINA_PROTECTED               = 65816,

	// Criteres (criteria tree IDs) declenchant les transitions de phase
	CRITERIA_TREE_FIND_JAINA_01         = 1000031,  // Ilot
	CRITERIA_TREE_HELP_KALECGOS         = 1000033,
	CRITERIA_TREE_FIND_JAINA_02         = 1000035,  // Cratere
	CRITERIA_TREE_CLEANING              = 1000037,
	CRITERIA_TREE_BACK_TO_SENDER        = 1000040,
	CRITERIA_TREE_THE_LAST_STAND        = 1000042,
	CRITERIA_TREE_WARLORD_ROKNAH        = 1000043,
	CRITERIA_TREE_JAINA_PROTECTED       = 1000044,
	CRITERIA_TREE_LEAVE_THE_RUINS       = 1000045,

	// Point IDs passes a MovePoint -> recus par MovementInform
	MOVEMENT_INFO_POINT_NONE            = 0,
	MOVEMENT_INFO_POINT_01              = 89644940, // Jaina arrive devant le verre brise
	MOVEMENT_INFO_POINT_02              = 89644941, // Jaina a porte du warlord pour le finisher
	MOVEMENT_INFO_POINT_03              = 89644942, // Jaina termine sa marche, declenche back-to-sender

    // Vignettes
    VIGNETTE_NONE                       = 0,
    VIGNETTE_JAINA_PROUDMOORE           = 50003,
    VIGNETTE_HORDE_TROOPS               = 50006,
    VIGNETTE_HORDE_WARLORD              = 50007,
};

// =========================================================================
// Phases logiques du scenario
// =========================================================================
// L'ordre est important : la comparaison "phase >= ..." est utilisee dans
// OnPlayerEnter pour appliquer la bonne skybox.
enum class RFTPhases
{
	FindJaina_Isle,                // Etat initial : les joueurs cherchent Jaina sur l'ilot
	FindJaina_Isle_Valided,        // Jaina trouvee, cinematique d'apres bataille en cours
	FindJaina_Crater,              // Apres teleport, les joueurs cherchent Jaina au cratere
	FindJaina_Crater_Valided,      // Dialogue de protection de l'iris en cours
	Standards,                     // Retour a Theramore, debut du nettoyage des hordes
	Standards_Valided,             // Phase combat active (Jaina rejoint le combat)
	BackToSender,                  // Sorts arcaniques de Jaina contre les hordes
	TheFinalAssault,               // Mort du warlord, hordes finalisees
	LeaveTheRuins                  // Sortie : portail Stormwind
};

// =========================================================================
// Helpers de spawn (paire spawn -> destination)
// =========================================================================
struct SpawnPoint
{
	SpawnPoint(Position spawn, Position destination) : spawn(spawn), destination(destination)
	{
	}

	const Position spawn;
	const Position destination;
};

// Positions de spawn et destination des deux elementaires d'eau
// (index 0 = haut sur Y, index 1 = bas sur Y, voir HORDE_SPLIT_Y_THRESHOLD)
const SpawnPoint ElementalsPoint[ELEMENTALS_SIZE] =
{
	{
		{ -3714.59f, -4465.40f, -19.87f, 6.26f },
		{ -3699.80f, -4453.12f, -20.12f, 6.26f },
	},
	{
		{ -3714.30f, -4472.51f, -20.15f, 6.26f },
		{ -3698.08f, -4482.82f, -19.68f, 6.26f }
	}
};

static_assert(sizeof(ElementalsPoint) / sizeof(ElementalsPoint[0]) == ELEMENTALS_SIZE,
	"ElementalsPoint count must match ELEMENTALS_SIZE");

// Trajet du zeppelin de bombardement traversant la zone
const SpawnPoint ZeppelinPoint =
{
	{ -3681.30f, -4568.94f, 52.12f, 1.09f },
	{ -3479.77f, -4314.78f, 37.16f, 0.89f }
};

// Chemin de Kalecgos lorsqu'il rejoint Jaina sur l'ilot apres la bataille
WaypointPath const KalecgosPath01 =
{
	1,
	{
		{ 0, -3044.71f, -4328.60f, 7.38f, 0.64f },
		{ 1, -3040.14f, -4325.38f, 6.72f, 0.55f },
		{ 2, -3031.74f, -4324.43f, 7.66f, 5.94f },
		{ 3, -3024.75f, -4327.67f, 7.68f, 5.77f },
		{ 4, -3013.87f, -4332.47f, 7.10f, 4.84f }
	},
	WaypointMoveType::Run
};

// =========================================================================
// Positions clefs du scenario
// =========================================================================
const Position PlayerPoint01    = { -3878.20f, -4589.90f,   8.67f, 0.78f }; // Centre du teleport groupe vers Theramore
const Position JainaPoint01     = { -3012.72f, -4340.22f,   6.64f, 1.70f }; // Position de Jaina sur l'ilot apres la bataille
const Position JainaPointBack   = { -3012.06f, -4346.65f,   6.60f, 1.69f }; // Recul de Jaina pour declencher l'echo d'Aluneth
const Position JainaPoint02     = { -3698.21f, -4457.47f, -20.88f, 1.27f }; // Position de Jaina au cratere (apres teleport)
const Position JainaPoint03     = { -3711.41f, -4467.89f, -20.54f, 0.02f }; // Position de Jaina pendant la protection de l'iris
const Position JainaPoint04     = { -3825.77f, -4537.44f,   9.21f, 0.73f }; // Position de Jaina pour la phase nettoyage / Standards
const Position KalecgosPoint01  = { -3044.71f, -4328.60f,   7.38f, 0.64f }; // Spawn de Kalecgos a l'ilot
const Position KalecgosPoint02  = { -3013.10f, -4336.91f,   6.77f, 4.82f }; // Position finale de Kalecgos avant teleport
const Position DummyPoint01     = { -3698.69f, -4467.94f, -20.87f, 3.55f }; // Spawn du dummy invisible qui porte les sorts cosmetiques de l'iris

// =========================================================================
// Helper de registration pour les AIs des creatures du scenario
// =========================================================================
template <class AI>
class RuinsCreatureScript : public CreatureScript
{
	public:
	RuinsCreatureScript(char const* name) : CreatureScript(name)
	{
	}
	CreatureAI* GetAI(Creature* creature) const override
	{
		return GetInstanceAI<AI>(creature, RFTScriptName);
	}
};

#define RegisterRuinsAI(ai_name) new RuinsCreatureScript<ai_name>(#ai_name);

#endif // RUINS_OF_THERAMORE_H_
