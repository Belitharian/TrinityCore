/*
 * Ruins of Theramore - Scenario header
 *
 * Definitions partagees par le scenario "Les Ruines de Theramore" :
 *   - Constantes (IDs creatures / spells / gameobjects / events / criteres)
 *   - Positions et trajets de Jaina
 *   - Phases du scenario (RFTPhases) et helper de registration d'AI
 *
 * Deroule (Tides of War, chapitre de la chute de Theramore) :
 *   5012 Trouver Jaina           -> l'ilot : Jaina, Kalecgos, "ne me suivez pas"
 *   5013 Ne me suivez pas        -> Jaina se teleporte seule, Kalec envoie les joueurs
 *   5014 Sur les traces de Jaina -> armurerie, rat, Peine, generaux, puis Kinndy et l'eclat
 *   5015 Les pilleurs            -> Jaina massacre les pillards, les joueurs l'aident
 *   5016 Le dernier survivant    -> Jaina acheve le chef avec l'eclat de miroir
 *   5017 Retour a l'envoyeur     -> les docks, puis l'iris au fond du cratere
 *   5018 Un regard sur le futur  -> portail vers Hurlevent
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

// =========================================================================
// Constantes de gameplay et cosmetiques
// =========================================================================
constexpr float CREATURE_BOUNDING_RADIUS        = 36.0f;     // Bounding applique a chaque NPC du scenario (range de targeting)
constexpr float KALECGOS_WALK_SPEED_RATE        = 0.6f;      // Kalecgos marche lentement pour synchroniser avec le dialogue de Jaina
constexpr float KALECGOS_BACK_OFF_DISTANCE      = 3.0f;      // Kalecgos recule quand Jaina lance sa teleportation...
constexpr float KALECGOS_BACK_OFF_SPEED         = 0.8f;      // ... lentement (m/s)
constexpr float DUMMY_SCALE_SMALL               = 0.6f;      // Echelle initiale du dummy de l'iris (avant le sceau)
constexpr float DUMMY_SCALE_LARGE               = 5.0f;      // Echelle finale : l'iris rayonne de l'essence de Jaina
constexpr float JAINA_TRIGGER_DISTANCE          = 10.0f;     // Distance de declenchement sur l'ilot
constexpr float TRAIL_WAIT_DISTANCE             = 15.0f;     // Jaina reprend sa marche quand un joueur est a cette distance
constexpr uint32 LOOTERS_REQUIRED               = 5;         // = Amount de criteria_tree 1000039 (pillards du groupe 0, chef exclu)
constexpr float TELEPORT_SPREAD_RADIUS          = 8.0f;      // Rayon de dispersion lors du teleport groupe des joueurs
constexpr int32 LOOTERS_WEAKENED_DURATION       = 5000;      // ms : les pillards sonnes par l'onde arcanique de Jaina

// =========================================================================
// Creatures (entry IDs)
// =========================================================================
enum RFTCreatures
{
	NPC_HEDRIC_EVENCANE                 = 58840,
	NPC_THERAMORE_OFFICER               = 58913,
	NPC_THERAMORE_FAITHFUL              = 59595,
	NPC_THERAMORE_ARCANIST              = 59596,
	NPC_KALECGOS                        = 64565,
	NPC_JAINA_PROUDMOORE                = 64727,
	NPC_ROKNAH_GRUNT                    = 64732,
	NPC_ROKNAH_LOA_SINGER               = 64733,
	NPC_ROKNAH_HAG                      = 64734,
	NPC_ROKNAH_WARLORD                  = 65442,     // Chef des pillards
	NPC_ROKNAH_FELCASTER                = 65507,
	NPC_GENERAL_TIRAS_ALAN              = 500041,
	NPC_ADMIRAL_AUBREY                  = 500040,
	NPC_ARCHMAGE_TERVOSH                = 500000,
	NPC_KINNDY_SPARKSHINE               = 500001,
	NPC_DEAD_ROKNAH_TROOP               = 500015,

	// Cadavres du parcours (Tides of War)
	NPC_SWIFT_WHITE_STEED               = 14560,     // Cheval tombe avec son cavalier
	NPC_THERAMORE_FOOTMAN               = 58612,
	NPC_KNIGHT_OF_THERAMORE             = 59654,     // Le soldat qui flotte (armurerie)
	NPC_PAINED                          = 500002,
	NPC_THADDUS_STOUTBLOW               = 500033,
	NPC_LIEUTENANT_ADEN                 = 500035,
	NPC_MARCUS_JONATHAN                 = 500036,
	NPC_BLACK_RAT                       = 500037,
	NPC_HORRAN_REDMANE                  = 500038     // Le second nain, tombe pres de Thaddus
};

// StringId (table creature) des cadavres qui sortent du traitement par entry
#define STRING_ID_CORPSE_BLOOD      "ruins_corpse_blood"      // Mort dans une flaque de sang rouge, sans trace arcanique
#define STRING_ID_CORPSE_FLOATING   "ruins_corpse_floating"   // Flotte a hauteur des yeux (animation de noye, sans faux-mort)

// =========================================================================
// Data IDs (utilises par InstanceScript::GetCreature / GetGameObject)
// =========================================================================
enum RFTData
{
	DATA_JAINA_PROUDMOORE               = 1,
	DATA_KALECGOS,
	DATA_KINNDY_SPARKSHINE,
	DATA_ROKNAH_WARLORD,
	DATA_BROKEN_GLASS,
	DATA_SCENARIO_PHASE,                           // Lu/ecrit pour piloter les transitions de phase
	DATA_FLOATING_KNIGHT,
	DATA_BLACK_RAT,
	DATA_PAINED,
	DATA_MARCUS_JONATHAN,
	DATA_JAINA_ARRIVED,                            // SetData : Jaina a termine le trajet <value> (RFTLegs)
	DATA_WARLORD_KNEELING                          // SetData : le chef des pillards est a genoux
};

// =========================================================================
// Talks (groupId des dialogues scriptes, table creature_text)
// =========================================================================
enum RFTTalks
{
	// --- Ilot : Kalecgos (64565) ---
	SAY_ISLE_KALECGOS_01                = 10,    // "Jaina ?"
	SAY_ISLE_KALECGOS_02                = 11,    // "Vous n'y etes pour rien..."
	SAY_ISLE_KALECGOS_03                = 12,    // "Ce n'est pas sur."
	SAY_ISLE_KALECGOS_04                = 13,    // "Cet endroit empeste l'energie arcanique..."
	SAY_ISLE_KALECGOS_05                = 14,    // "Vos cheveux ont blanchi..."
	SAY_ISLE_KALECGOS_06                = 15,    // "Il n'y a plus personne..."
	SAY_ISLE_KALECGOS_07                = 16,    // "Je ne peux pas la suivre, mais vous oui."

	// --- Ilot : Jaina (64727) ---
	SAY_ISLE_JAINA_01                   = 0,     // "Disparu... Tout a disparu."
	SAY_ISLE_JAINA_02                   = 1,     // "Mon royaume... tous les generaux..."
	SAY_ISLE_JAINA_03                   = 2,     // "Je dois y retourner."
	SAY_ISLE_JAINA_04                   = 3,     // "Sur ? Comment pouvez-vous..."
	SAY_ISLE_JAINA_05                   = 4,     // "Deja quoi ?"
	SAY_ISLE_JAINA_06                   = 5,     // "Ne me suivez pas."

	// --- Parcours ---
	SAY_TRAIL_FLOATING_OFFICER_JAINA    = 14,
	SAY_TRAIL_RAT_JAINA                 = 15,
	SAY_TRAIL_SURVIVORS_JAINA           = 16,
	SAY_TRAIL_PAINED_HAIR_EMOTE         = 11,    // Emote : les cheveux de Peine se brisent
	SAY_TRAIL_PAINED_JAINA              = 12,
	SAY_TRAIL_GENERALS_JAINA            = 13,
	SAY_TRAIL_PAGE_EMOTE                = 24,    // Emote : la page tombe en poussiere
	SAY_TRAIL_HEALER_EMOTE              = 25,    // Emote : le soigneur et son blesse
	SAY_TRAIL_RIDER_EMOTE               = 26,    // Emote : le cavalier et son cheval
	SAY_TRAIL_SOLDIERS_EMOTE            = 27,    // Emote : les soldats, l'arme au fourreau
	SAY_TRAIL_BLOOD_EMOTE               = 28,    // Emote : le soldat dans son sang (armurerie)
	SAY_TRAIL_THADDUS_EMOTE             = 29,    // Emote : Thaddus et Horran
	SAY_TRAIL_ADEN_EMOTE                = 30,    // Emote : Aden sur son toit

	// --- Kinndy ---
	SAY_KINNDY_JAINA                    = 10,    // "Kinndy... Non... Pas toi."
	SAY_KINNDY_SCREAM_EMOTE             = 17,    // Emote : Jaina hurle
	SAY_KINNDY_RAGE_JAINA               = 18,

	// --- Pillards : chef (65442) ---
	SAY_LOOTERS_WARLORD_TAUNT           = 2,     // "Petite dame... on corrige l'erreur."
	SAY_LOOTERS_WARLORD_HEAD            = 1,     // Officielle : "votre tete sur un plateau"
	SAY_FINISHER_WARLORD                = 0,     // Officielle : "je vous CRACHERAI au visage"

	// --- Le dernier survivant : Jaina ---
	SAY_FINISHER_JAINA_01               = 7,     // Officielle : "des chiens enrages"
	SAY_FINISHER_JAINA_02               = 8,     // Officielle : "Vous crachez sur la pitie ?"

	// --- Docks et cratere ---
	SAY_DOCKS_JAINA_01                  = 19,
	SAY_DOCKS_JAINA_02                  = 20,
	SAY_CRATER_RIM_JAINA                = 6,     // Officielle : "Cette aberration a tout detruit"
	SAY_IRIS_JAINA_01                   = 21,
	SAY_IRIS_JAINA_02                   = 22,
	SAY_IRIS_JAINA_03                   = 23,

	// --- Sortie ---
	SAY_LEAVE_THE_RUINS_JAINA           = 9
};

// =========================================================================
// Repliques sans voix (Sound 0 dans creature_text)
// =========================================================================
// A REDOUBLER : texte modifie, l'ancienne voix custom a ete retiree.
//   Jaina    (64727) groupe 1  - "Mon royaume... tous les generaux... Coup-Robuste, Tiras'alan, Aubrey..."   (ancien son 700103)
//   Kalecgos (64565) groupe 11 - "Non, Jaina. Vous n'y etes pour rien... la bombe etait alimentee par l'iris" (ancien son 700108)
//
// A DOUBLER : repliques sans voix d'origine.
//   Jaina    (64727) groupe 9  - "Pas un mot de l'iris a Kalecgos. Je pars pour Hurlevent..."
//   Jaina    (64727) groupe 10 - "Kinndy... Non... Pas toi. Tu avais toute la vie devant toi..."
//   Jaina    (64727) groupes 12 a 16, 18 a 23 (11, 17 et 24 a 30 sont des emotes, sans voix)
//   Chef des pillards (65442) groupe 2 - "Petite dame... sais pas comment toi survivre..."

// =========================================================================
// Sorts utilises dans le scenario (cosmetiques)
// =========================================================================
enum RFTSpells
{
	SPELL_TELEPORT_VISUAL_ONLY          = 51347,
	SPELL_ECHO_OF_ALUNETH_SPAWN         = 211768,
	SPELL_ALUNETH_FREED_EXPLOSION       = 225253,
	SPELL_ALUNETH_DRINKS                = 212220,
	SPELL_SHIMMERDUST                   = 278917,
	SPELL_SKYBOX_EFFECT_ENTRANCE        = 148137,    // Skybox de l'ilot
	SPELL_ARCANE_WAVE                   = 275167,    // "Ping Jaina" (dummy) avec le visuel de Time Stop 208944 (hotfix XSV 600000)
	SPELL_BURNING                       = 282051,
	SPELL_ARCANE_CHANNELING             = 294676,
	SPELL_EMPOWERED_SUMMON              = 303681,
	SPELL_SKYBOX_EFFECT_RUINS           = 310302,    // Skybox des ruines
	SPELL_GLACIAL_SPIKE_COSMETIC        = 346559,
    SPELL_KINNDY_DUST                   = 430829,
	SPELL_ARCANE_DEBUFF_VISUAL          = 436058,    // Aussi : pillards affaiblis par l'onde arcanique de Jaina
	SPELL_COSMETIC_ARCANE_ENERGY        = 1233488,
	SPELL_COSMETIC_PURPLE_VERTEX_STATE  = 1247717,
	SPELL_BLOOD_STAIN                   = 1289233,   // [DNT]Blood Stain : flaque de sang sous le cadavre
	SPELL_COSMETIC_ARCANE_DISSOLVE      = 1244360,
	SPELL_DROWNED_ANIM                  = 1244365,    // Animation de noye : le soldat qui flotte
};

// =========================================================================
// Constantes diverses : GO, data IDs internes, events, criteres, points
// =========================================================================
enum RFTMisc
{
	// GameObjects
	GOB_BROKEN_GLASS                    = 349872,   // L'eclat du miroir de communication de Jaina
	GOB_PORTAL_TO_STORMWIND             = 353823,
	GOB_LOST_PAGE                       = 620533,   // Pages de la bibliotheque de Jaina
	GOB_THERAMORE_BANNER                = 214965,   // Etendard pille (go_theramore_banner)

	// Data IDs internes a npc_jaina_ruins::SetData
	DATA_CANCEL_GROUP                   = 2,
	DATA_PHASE_COMBAT                   = 3,

	// Weather
	WEATHER_ARCANE_BUILD                = 182,

	// Events de scenario (TriggerGameEvent) -> criteres de type 92
	EVENT_FIND_JAINA_01                 = 65811,    // 5012 : Jaina trouvee sur l'ilot
	EVENT_HELP_KALECGOS                 = 65812,    // 5013 : Jaina partie seule
	EVENT_FIND_JAINA_02                 = 65813,    // 5014 : Jaina retrouvee aupres de Kinndy
	EVENT_WARLORD_ROKNAH_SLAIN          = 65815,    // 5016 : chef des pillards acheve
	EVENT_TRAIL_PAINED                  = 65825,    // 5014 : lieu "Peine"
	EVENT_TRAIL_GENERALS                = 65826,    // 5014 : lieu "les generaux"
	EVENT_TRAIL_FLOATING_OFFICER        = 65827,    // 5014 : lieu "l'officier qui flotte"
	EVENT_TRAIL_RAT                     = 65828,    // 5014 : lieu "le rat"
	EVENT_IRIS_SEALED                   = 65829,    // 5017 : iris scelle
	EVENT_LOOTER_SLAIN                  = 65830,    // 5015 : un pillard est mort (quel que soit le tueur)

	// Criteres (criteria tree IDs) declenchant les transitions de phase
	CRITERIA_TREE_FIND_JAINA_01         = 1000031,  // 5012
	CRITERIA_TREE_HELP_KALECGOS         = 1000033,  // 5013
	CRITERIA_TREE_FIND_JAINA_02         = 1000035,  // 5014
	CRITERIA_TREE_CLEANING              = 1000037,  // 5015
	CRITERIA_TREE_LAST_SURVIVOR         = 1000040,  // 5016
	CRITERIA_TREE_BACK_TO_SENDER        = 1000042,  // 5017

	// Point ID de MovePoint ignore par le scenario
	MOVEMENT_INFO_POINT_NONE            = 0,

	// Vignettes
	VIGNETTE_NONE                       = 0,
	VIGNETTE_JAINA_PROUDMOORE           = 50027,    // Celle de la Purge : icone de heros d'Alliance (dore)
	VIGNETTE_HORDE_TROOPS               = 50006,    // Pillards : surbrillance seule (partagee avec la Purge)
	VIGNETTE_HORDE_WARLORD              = 50007,    // Chef des pillards : icone de heros de la Horde (dore), sur la carte
	VIGNETTE_PORTAL_TO_STORMWIND        = 50030,    // Celle de la Purge
	VIGNETTE_PILLAGED_STANDARD          = 50032,    // Etendards pilles : banniere d'Alliance sur la minimap
};

// =========================================================================
// Trajets de Jaina
// =========================================================================
// Un trajet suit son WaypointPath (ci-dessous, ID PATH_JAINA_LEG_BASE + leg),
// ou va tout droit s'il n'en a pas (point fixe ou approche d'une cible, voir
// JainaLegs dans le .cpp). L'arrivee revient a l'InstanceScript par
// SetData(DATA_JAINA_ARRIVED, leg), depuis WaypointPathEnded ou MovementInform.
enum class RFTLegs : uint32
{
	TrailStart,         // Course de l'arrivee jusqu'au debut du parcours
	Armory,             // Le soldat qui flotte, pres de l'armurerie
	Rat,
	Pained,
	Generals,
	Kinndy,
	BrokenGlass,        // L'eclat de miroir, a quelques pas de Kinndy
	Looters,            // Jaina se place a portee des pillards
	Finisher,           // Jaina rejoint le chef a genoux
	Docks,
	CraterRim,
	CraterBottom,
	Max
};

constexpr uint32 PATH_JAINA_LEG_BASE            = 100;          // + RFTLegs (ID des WaypointPath)
constexpr uint32 MOVEMENT_JAINA_LEG_BASE        = 89644900;     // + RFTLegs (point IDs du repli sans chemin)

// Chaque chemin commence au point qui suit l'arret precedent.
WaypointPath const JainaPathTrailStart =
{
	PATH_JAINA_LEG_BASE + (uint32)RFTLegs::TrailStart,
	{
		{ 0, -3636.93f, -4347.62f, 1.66f, 3.55f },
		{ 1, -3642.48f, -4350.57f, 1.90f, 3.79f },
		{ 2, -3647.28f, -4356.53f, 2.76f, 4.15f },
		{ 3, -3652.11f, -4363.39f, 3.83f, 3.96f },
		{ 4, -3658.21f, -4369.15f, 5.28f, 3.83f },
		{ 5, -3665.98f, -4375.11f, 5.43f, 3.76f },
		{ 6, -3673.07f, -4379.60f, 5.41f, 3.58f }
	},
	WaypointMoveType::Run,
	WaypointPathFlags::ExactSplinePath
};

WaypointPath const JainaPathArmory =
{
	PATH_JAINA_LEG_BASE + (uint32)RFTLegs::Armory,
	{
		{ 0,  -3679.18f, -4380.44f,  5.33f, 2.95f },
		{ 1,  -3686.49f, -4378.02f,  7.11f, 2.79f },
		{ 2,  -3693.44f, -4374.77f,  9.58f, 2.58f },
		{ 3,  -3700.43f, -4367.99f, 10.60f, 2.22f },
		{ 4,  -3705.39f, -4362.19f, 10.60f, 2.69f, 2500ms },   // Pause : le cavalier et son cheval
		{ 5,  -3714.06f, -4359.62f, 10.60f, 2.57f },
		{ 6,  -3716.52f, -4349.92f, 10.60f, 1.58f },
		{ 7,  -3719.21f, -4342.92f, 10.60f, 2.18f, 3000ms },   // Pause : les soldats, l'arme au fourreau
		{ 8,  -3725.33f, -4334.40f, 10.80f, 2.23f },
		{ 9,  -3730.71f, -4327.06f,  9.89f, 2.16f, 4000ms },   // Pause : une page de sa bibliotheque
		{ 10, -3735.64f, -4319.40f,  9.34f, 2.14f },
		{ 11, -3741.48f, -4314.32f,  9.40f, 2.67f }
	},
	WaypointMoveType::Walk,
	WaypointPathFlags::ExactSplinePath
};

WaypointPath const JainaPathRat =
{
	PATH_JAINA_LEG_BASE + (uint32)RFTLegs::Rat,
	{
		{ 0, -3765.89f, -4321.76f, 9.81f, 3.20f },
		{ 1, -3762.83f, -4331.29f, 9.93f, 5.26f }
	},
	WaypointMoveType::Walk,
	WaypointPathFlags::ExactSplinePath
};

WaypointPath const JainaPathPained =
{
	PATH_JAINA_LEG_BASE + (uint32)RFTLegs::Pained,
	{
		{ 0, -3754.55f, -4346.69f, 10.64f, 5.18f },
		{ 1, -3747.09f, -4360.96f, 10.63f, 5.20f },
		{ 2, -3743.14f, -4369.14f, 10.60f, 5.07f },
		{ 3, -3743.21f, -4375.91f, 10.61f, 4.22f },
		{ 4, -3751.24f, -4375.92f, 11.00f, 1.89f }
	},
	WaypointMoveType::Run,       // Elle court : elle cherche des survivants ("Ou etes-vous ?!")
	WaypointPathFlags::ExactSplinePath
};

WaypointPath const JainaPathGenerals =
{
	PATH_JAINA_LEG_BASE + (uint32)RFTLegs::Generals,
	{
		{ 0, -3755.10f, -4382.40f, 10.65f, 3.63f },
		{ 1, -3760.86f, -4378.34f, 11.51f, 2.21f },
		{ 2, -3768.10f, -4373.21f, 12.86f, 2.82f },
		{ 3, -3775.54f, -4371.44f, 14.54f, 3.13f },
		{ 4, -3786.68f, -4370.46f, 16.76f, 3.01f },
		{ 5, -3797.53f, -4370.53f, 16.79f, 3.26f }
	},
	WaypointMoveType::Run,
	WaypointPathFlags::ExactSplinePath
};

WaypointPath const JainaPathKinndy =
{
	PATH_JAINA_LEG_BASE + (uint32)RFTLegs::Kinndy,
	{
		{ 0,  -3803.46f, -4372.60f, 16.75f, 3.29f },
		{ 1,  -3811.74f, -4373.72f, 14.16f, 3.45f },
		{ 2,  -3818.19f, -4376.36f, 13.01f, 3.73f },
		{ 3,  -3822.47f, -4383.36f, 11.59f, 4.44f },
		{ 4,  -3823.96f, -4390.85f, 11.11f, 4.74f },
		{ 5,  -3821.63f, -4397.22f, 11.44f, 5.43f },
		{ 6,  -3813.97f, -4399.52f, 11.92f, 0.05f, 5000ms },   // Pause : Thaddus et Horran, Aden sur le toit
		{ 7,  -3805.93f, -4400.83f, 11.81f, 5.71f },
		{ 8,  -3804.32f, -4406.16f, 12.19f, 4.26f },
		{ 9,  -3809.00f, -4414.76f, 12.72f, 4.25f },
		{ 10, -3812.02f, -4422.58f, 12.69f, 4.43f },
		{ 11, -3818.13f, -4430.56f, 12.61f, 3.78f }
	},
	WaypointMoveType::Run,
	WaypointPathFlags::ExactSplinePath
};

WaypointPath const JainaPathLooters =
{
	PATH_JAINA_LEG_BASE + (uint32)RFTLegs::Looters,
	{
		{ 0,  -3813.16f, -4432.73f, 12.75f, 6.11f },
		{ 1,  -3810.90f, -4439.82f, 10.64f, 4.93f },
		{ 2,  -3809.00f, -4453.65f,  2.34f, 4.84f },
		{ 3,  -3807.14f, -4463.98f,  3.32f, 4.92f },
		{ 4,  -3806.04f, -4474.40f,  4.57f, 4.76f },
		{ 5,  -3805.58f, -4484.88f,  4.99f, 4.76f },
		{ 6,  -3804.08f, -4497.35f,  5.38f, 4.99f },
		{ 7,  -3800.67f, -4508.74f,  5.54f, 5.16f },
		{ 8,  -3798.79f, -4515.34f,  6.60f, 4.57f },
		{ 9,  -3804.31f, -4521.29f, 10.05f, 3.63f },
		{ 10, -3814.56f, -4527.95f,  9.22f, 3.86f }    // Devant le chef des pillards
	},
	WaypointMoveType::Run,
	WaypointPathFlags::ExactSplinePath
};

WaypointPath const JainaPathDocks =
{
	PATH_JAINA_LEG_BASE + (uint32)RFTLegs::Docks,
	{
		{ 0,  -3818.52f, -4530.46f,  9.22f, 3.89f },
		{ 1,  -3825.81f, -4537.30f,  9.22f, 3.90f },
		{ 2,  -3831.15f, -4542.03f,  9.22f, 3.73f },
		{ 3,  -3835.86f, -4543.39f,  9.18f, 3.15f },
		{ 4,  -3842.98f, -4542.92f,  9.07f, 3.13f },
		{ 5,  -3848.24f, -4544.69f,  8.82f, 3.82f },
		{ 6,  -3851.73f, -4549.95f,  8.64f, 4.45f },
		{ 7,  -3851.56f, -4557.05f,  8.39f, 4.82f },
		{ 8,  -3854.50f, -4564.57f,  8.15f, 4.01f },
		{ 9,  -3863.96f, -4575.26f,  8.08f, 3.94f },
		{ 10, -3869.82f, -4581.09f,  9.03f, 3.94f },
		{ 11, -3876.15f, -4586.37f,  8.65f, 3.29f }     // Arret des docks (DocksPoint)
	},
	WaypointMoveType::Run,
	WaypointPathFlags::ExactSplinePath
};

WaypointPath const JainaPathCraterRim =
{
	PATH_JAINA_LEG_BASE + (uint32)RFTLegs::CraterRim,
	{
		{ 0,  -3869.64f, -4581.02f,   9.02f, 0.83f },
		{ 1,  -3855.14f, -4566.24f,   8.09f, 0.77f },
		{ 2,  -3846.26f, -4562.41f,   8.36f, 0.02f },
		{ 3,  -3840.55f, -4562.26f,   8.49f, 0.11f },
		{ 4,  -3835.44f, -4560.06f,   8.67f, 0.74f },
		{ 5,  -3831.78f, -4555.72f,   8.87f, 1.08f },
		{ 6,  -3830.26f, -4543.09f,   9.22f, 1.57f },
		{ 7,  -3807.21f, -4522.77f,   9.69f, 0.63f },
		{ 8,  -3789.09f, -4510.06f,   2.62f, 0.68f },
		{ 9,  -3780.91f, -4503.14f,   1.19f, 0.73f },
		{ 10, -3769.97f, -4492.90f,  -0.43f, 0.85f },
		{ 11, -3760.67f, -4481.17f,  -0.45f, 1.03f },
		{ 12, -3756.47f, -4481.43f,  -0.12f, 0.30f }     // Arret au bord du cratere (CraterRimPoint)
	},
	WaypointMoveType::Run,
	WaypointPathFlags::ExactSplinePath
};

WaypointPath const JainaPathCraterBottom =
{
	PATH_JAINA_LEG_BASE + (uint32)RFTLegs::CraterBottom,
	{
		{ 0,  -3756.23f, -4474.77f,  -0.70f, 0.58f },
		{ 1,  -3746.95f, -4471.29f,  -5.35f, 0.19f },
		{ 2,  -3737.07f, -4469.81f, -11.17f, 0.12f },
		{ 3,  -3721.90f, -4468.74f, -17.67f, 0.04f },
		{ 4,  -3711.64f, -4468.20f, -20.58f, 6.28f }     // Arret devant l'iris (CraterBottomPoint)
	},
	WaypointMoveType::Walk,
	WaypointPathFlags::ExactSplinePath
};

// =========================================================================
// Phases logiques du scenario
// =========================================================================
// L'ordre est important : les auras de phase (skybox) se lisent en tranches.
enum class RFTPhases
{
	FindJaina_Isle,                // Etat initial : les joueurs cherchent Jaina sur l'ilot
	FindJaina_Isle_Valided,        // Cinematique de l'ilot en cours
	Trail,                         // Dans les ruines : Jaina parcourt la ville
	Kinndy,                        // Kinndy, puis l'eclat de miroir
	Looters,                       // Combat contre les pillards (etendards ramassables)
	LastSurvivor,                  // Jaina acheve le chef
	BackToSender,                  // Docks, puis l'iris au fond du cratere
	LeaveTheRuins                  // Sortie : portail vers Hurlevent
};

// =========================================================================
// Ilot : Kalecgos rejoint Jaina apres la bataille
// =========================================================================
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
// Ilot
const Position JainaPoint01         = { -3012.72f, -4340.22f,   6.64f, 1.70f }; // Jaina sur l'ilot apres la bataille
const Position KalecgosPoint01      = { -3044.71f, -4328.60f,   7.38f, 0.64f }; // Spawn de Kalecgos a l'ilot
const Position KalecgosPoint02      = { -3013.10f, -4336.91f,   6.77f, 4.82f }; // Position finale de Kalecgos avant teleport

// Ruines
const Position JainaArrivalPoint    = { -3629.21f, -4344.32f,   2.39f, 3.58f }; // Jaina se teleporte ici (les joueurs aussi, juste apres)
const Position TrailStartPoint      = { -3672.85f, -4380.26f,   5.24f, 1.94f }; // Debut du parcours, pres du soigneur et de son blesse
const Position DocksPoint           = { -3876.15f, -4586.37f,   8.65f, 3.29f }; // Il ne reste rien des docks
const Position CraterRimPoint       = { -3756.47f, -4481.43f,  -0.12f, 0.30f }; // Bord du cratere, face a l'iris
const Position CraterBottomPoint    = { -3711.64f, -4468.20f, -20.58f, 6.28f }; // Fond du cratere, devant l'iris
const Position IrisPoint            = { -3698.69f, -4467.94f, -20.87f, 3.55f }; // Centre de l'iris (dans l'ADT) : porte le dummy des visuels

// Cadavres que Jaina regarde en passant (positions des spawns, map 5001)
const Position HealerCorpsePoint    = { -3680.34f, -4391.30f,   4.57f };        // Fidele et son blesse (debut du parcours)
const Position RiderCorpsePoint     = { -3694.79f, -4357.43f,  11.44f };        // Le cavalier et son cheval
const Position SoldiersCorpsePoint  = { -3710.80f, -4341.20f,  10.62f };        // Les soldats tombes l'arme au fourreau
const Position LostPagePoint        = { -3731.54f, -4325.81f,   9.89f };        // Page de sa bibliotheque (GO 620533)
const Position ThaddusCorpsePoint   = { -3800.50f, -4400.87f,  11.37f };        // Thaddus Coup-Robuste (Horran git plus loin)
const Position AdenCorpsePoint      = { -3836.44f, -4402.62f,  17.59f };        // Le lieutenant Aden, sur le toit

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
