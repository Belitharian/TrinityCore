/*
 * Battle for Theramore - InstanceScript principal
 *
 * Pilote le scenario "La Chute de Theramore" de bout en bout.
 *
 * DEUX MOTEURS COMPLEMENTAIRES
 *
 *   1. OnCompletedCriteriaTree : les criteria trees valident les etapes
 *      cotees joueur (trouver Jaina, evacuer les civils, survivre...). A
 *      chaque validation on fait avancer DATA_SCENARIO_PHASE et on amorce la
 *      cinematique suivante.
 *
 *   2. Update / EventMap : la cinematique elle-meme est une chaine d'events
 *      numerotes. Next(delay) incremente eventId et planifie eventId + 1,
 *      donc l'ordre NUMERIQUE des case est l'ordre chronologique du scenario.
 *      Un event sans Next() est un point d'arret : la suite repart d'un
 *      criteria tree ou d'un TriggerGameEvent leve par une AI.
 *
 * PLAN DES EVENTS (les #pragma region suivent ce decoupage)
 *
 *      1 -  23  THE_COUNCIL          Conseil de guerre dans la tour
 *     24        WAITING              Temps mort avant l'arrivee de Perith
 *     25 -  70  THE_UNKNOWN_TAUREN   Perith annonce l'attaque de la Horde
 *     71 -  90  A_LITTLE_HELP        Arrivee des archimages, mise en place
 *     91 - 100  THE_BATTLE           Trahison de Thalen, debut de la bataille
 *    122 - 141  HELP_THE_WOUNDED     Dialogues d'apres-bataille (2 parties)
 *    142 - 160  WAIT_FOR_AMARA       Retour d'Amara Leeson (2 parties)
 *    161 - 172  RETRIEVE_RHONIN      Montee a la tour, scene de l'explosion
 *
 * Les identifiants 101 a 121 ne sont pas utilises (marge laissee libre entre
 * la bataille et l'apres-bataille).
 *
 * Les events sont nommes dans l'enum BFTEvents (voir plus bas), mais leurs
 * VALEURS restent porteuses de sens : Next() s'appuie sur eventId + 1, et
 * CRITERIA_TREE_HELP_THE_WOUNDED annule une plage entiere d'un coup via
 * EVT_WOUNDED_PART2_FIRST / EVT_WOUNDED_PART2_LAST. Renommer est libre,
 * reordonner ou inserer une valeur au milieu d'une chaine ne l'est pas.
 *
 * Commentaires en francais sans accents (encodage TC).
 */

#include "CriteriaHandler.h"
#include "CustomAI.h"          // GetRandomPosition / GetRandomPositionAroundCircle
#include "DB2Structure.h"
#include "EventMap.h"
#include "GameObject.h"
#include "InstanceScenario.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "MiscPackets.h"
#include "ObjectMgr.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "Scenario.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"
#include "Weather.h"
#include "battle_for_theramore.h"
#include "../CustomScenario.h"

// =========================================================================
// Tables de correspondance NPC / GO <-> Data ID
// =========================================================================

// Nombre d'acteurs de dialogue en tete de creatureData : SetTarget et
// ClearTarget ne parcourent que ces 14 premieres entrees, les suivantes
// (Kalecgos dragon, Drok, Gruhta) ne participent pas aux cinematiques.
uint8 const eventCreatureDataCount = 14;

const ObjectData creatureData[] =
{
	{ NPC_JAINA_PROUDMOORE,     DATA_JAINA_PROUDMOORE       },
	{ NPC_KINNDY_SPARKSHINE,    DATA_KINNDY_SPARKSHINE      },
	{ NPC_KALECGOS,             DATA_KALECGOS               },
	{ NPC_ARCHMAGE_TERVOSH,     DATA_ARCHMAGE_TERVOSH       },
	{ NPC_PAINED,               DATA_PAINED                 },
	{ NPC_PERITH_STORMHOOVE,    DATA_PERITH_STORMHOOVE      },
	{ NPC_KNIGHT_OF_THERAMORE,  DATA_KNIGHT_OF_THERAMORE    },
	{ NPC_HEDRIC_EVENCANE,      DATA_HEDRIC_EVENCANE        },
	{ NPC_RHONIN,               DATA_RHONIN                 },
	{ NPC_VEREESA_WINDRUNNER,   DATA_VEREESA_WINDRUNNER     },
	{ NPC_THALEN_SONGWEAVER,    DATA_THALEN_SONGWEAVER      },
	{ NPC_TARI_COGG,            DATA_TARI_COGG              },
	{ NPC_AMARA_LEESON,         DATA_AMARA_LEESON           },
	{ NPC_THADER_WINDERMERE,    DATA_THADER_WINDERMERE      },
	{ NPC_KALECGOS_DRAGON,      DATA_KALECGOS_DRAGON        },
	{ NPC_CAPTAIN_DROK,         DATA_CAPTAIN_DROK           },
	{ NPC_WAVE_CALLER_GRUHTA,   DATA_WAVE_CALLER_GRUHTA     },
	{ 0,                        0                           }   // END
};

const ObjectData gameobjectData[] =
{
	{ GOB_PORTAL_TO_STORMWIND,  DATA_PORTAL_TO_STORMWIND    },
	{ GOB_PORTAL_TO_DALARAN,    DATA_PORTAL_TO_DALARAN      },
	{ GOB_PORTAL_TO_ORGRIMMAR,  DATA_PORTAL_TO_ORGRIMMAR    },
	{ GOB_MYSTIC_BARRIER_01,    DATA_MYSTIC_BARRIER_01      },
	{ GOB_MYSTIC_BARRIER_02,    DATA_MYSTIC_BARRIER_02      },
	{ GOB_ENERGY_BARRIER,       DATA_ENERGY_BARRIER         },
	{ GOB_POWDER_BARREL,        DATA_POWDER_BARREL          },
	{ 0,                        0                           }   // END
};

// =========================================================================
// Identifiants des evenements internes de l'EventMap
// =========================================================================
// L'ordre numerique est SIGNIFICATIF : Next() incremente eventId de 1 et
// planifie ainsi automatiquement l'event suivant dans la sequence. Ne pas
// reordonner ni inserer une valeur au milieu d'une chaine sans verifier les
// Next() concernes.
// Les valeurs 101 a 121 restent libres (marge entre la bataille et
// l'apres-bataille) et EVT_BATTLE_UNUSED (92) est un trou historique.
enum BFTEvents : uint32
{
	// -- The Council (1 - 23) : conseil de guerre dans la tour
	EVT_COUNCIL_TERVOSH_ARRIVE          = 1,
	EVT_COUNCIL_KINNDY_ARRIVE           = 2,    // Point de saut DEBUG vers EVT_COUNCIL_KALEC_LEAVE
	EVT_COUNCIL_KINNDY_TALK_02          = 3,
	EVT_COUNCIL_JAINA_TALK_03           = 4,
	EVT_COUNCIL_KINNDY_TALK_04          = 5,
	EVT_COUNCIL_JAINA_TALK_05           = 6,
	EVT_COUNCIL_TERVOSH_TALK_06         = 7,
	EVT_COUNCIL_KALEC_TALK_07           = 8,
	EVT_COUNCIL_KALEC_TALK_08           = 9,
	EVT_COUNCIL_TERVOSH_TALK_09         = 10,
	EVT_COUNCIL_KINNDY_TALK_09_BIS      = 11,
	EVT_COUNCIL_JAINA_TALK_10           = 12,
	EVT_COUNCIL_KALEC_TALK_11           = 13,
	EVT_COUNCIL_JAINA_TALK_12           = 14,
	EVT_COUNCIL_KINNDY_TALK_13          = 15,
	EVT_COUNCIL_KALEC_TALK_14           = 16,
	EVT_COUNCIL_JAINA_TALK_15           = 17,
	EVT_COUNCIL_KALEC_TALK_16           = 18,
	EVT_COUNCIL_KALEC_TALK_17           = 19,
	EVT_COUNCIL_KALEC_LEAVE             = 20,   // Dispersion - point d'entree DEBUG
	EVT_COUNCIL_TERVOSH_LEAVE           = 21,
	EVT_COUNCIL_KINNDY_LEAVE            = 22,
	EVT_COUNCIL_JAINA_TO_TABLE          = 23,   // Point d'arret : l'AI de Jaina leve EVENT_THE_COUNCIL

	// -- Waiting (24)
	EVT_WAITING_TRIGGER                 = 24,   // EVENT_WAITING

	// -- The Unknown Tauren (25 - 70) : Perith annonce l'attaque de la Horde
	EVT_TAUREN_ESCORT_SPAWN             = 25,   // Point de saut DEBUG vers EVT_TAUREN_PAINED_LEAVE
	EVT_TAUREN_JAINA_TARGET_PERITH      = 26,
	EVT_TAUREN_PAINED_TALK_01           = 27,
	EVT_TAUREN_JAINA_TALK_02            = 28,
	EVT_TAUREN_PAINED_TALK_03           = 29,
	EVT_TAUREN_PAINED_TALK_04           = 30,
	EVT_TAUREN_JAINA_TALK_05            = 31,
	EVT_TAUREN_PAINED_TALK_06           = 32,
	EVT_TAUREN_PAINED_APPROACH          = 33,
	EVT_TAUREN_PAINED_SALUTE            = 34,
	EVT_TAUREN_PAINED_TALK_07           = 35,
	EVT_TAUREN_JAINA_TALK_08            = 36,
	EVT_TAUREN_JAINA_TALK_09            = 37,
	EVT_TAUREN_JAINA_TALK_10            = 38,
	EVT_TAUREN_KNIGHT_APPROACH          = 39,
	EVT_TAUREN_KNIGHT_TALK_11           = 40,
	EVT_TAUREN_JAINA_TALK_12            = 41,
	EVT_TAUREN_PAINED_TALK_13           = 42,   // Sortie du chevalier
	EVT_TAUREN_PERITH_TALK_14           = 43,
	EVT_TAUREN_JAINA_TALK_15            = 44,
	EVT_TAUREN_PERITH_TALK_16           = 45,
	EVT_TAUREN_PERITH_TALK_17           = 46,
	EVT_TAUREN_PERITH_TALK_18           = 47,
	EVT_TAUREN_JAINA_TALK_19            = 48,
	EVT_TAUREN_PERITH_TALK_20           = 49,
	EVT_TAUREN_JAINA_TALK_21            = 50,
	EVT_TAUREN_PERITH_TALK_22           = 51,
	EVT_TAUREN_PERITH_TALK_23           = 52,
	EVT_TAUREN_JAINA_TALK_24            = 53,
	EVT_TAUREN_PERITH_TALK_25           = 54,
	EVT_TAUREN_JAINA_TALK_26            = 55,
	EVT_TAUREN_JAINA_TURN_AWAY          = 56,
	EVT_TAUREN_JAINA_TALK_27            = 57,   // Ordre d'evacuation (SPELL_MAGIC_QUILL)
	EVT_TAUREN_JAINA_QUILL_END          = 58,
	EVT_TAUREN_JAINA_TALK_28            = 59,
	EVT_TAUREN_PERITH_TALK_29           = 60,
	EVT_TAUREN_PERITH_TALK_30           = 61,
	EVT_TAUREN_JAINA_TALK_31            = 62,
	EVT_TAUREN_PERITH_TALK_32           = 63,
	EVT_TAUREN_JAINA_TALK_33            = 64,
	EVT_TAUREN_PERITH_TALK_34           = 65,
	EVT_TAUREN_PERITH_LEAVE             = 66,
	EVT_TAUREN_JAINA_TALK_35            = 67,
	EVT_TAUREN_PAINED_TALK_36           = 68,
	EVT_TAUREN_JAINA_TALK_37            = 69,
	EVT_TAUREN_PAINED_LEAVE             = 70,   // Point d'arret : l'AI de Pained leve EVENT_THE_UNKNOWN_TAUREN

	// -- A Little Help (71 - 90) : arrivee des renforts de Dalaran
	EVT_HELP_JAINA_TALK_02              = 71,
	EVT_HELP_HEDRIC_TALK_01             = 72,
	EVT_HELP_HEDRIC_TALK_03             = 73,
	EVT_HELP_JAINA_TALK_04              = 74,
	EVT_HELP_OPEN_PORTAL                = 75,   // Portail de Dalaran
	EVT_HELP_HEDRIC_BACKSTEP            = 76,
	EVT_HELP_ARCHMAGES_ARRIVAL          = 77,   // S'auto-repete (events.Repeat) tant qu'il reste un archimage
	EVT_HELP_RHONIN_TALK_05             = 78,
	EVT_HELP_JAINA_TALK_06              = 79,
	EVT_HELP_JAINA_TALK_07              = 80,
	EVT_HELP_THALEN_TALK_08             = 81,
	EVT_HELP_JAINA_TALK_09              = 82,
	EVT_HELP_JAINA_TALK_10              = 83,
	EVT_HELP_RHONIN_TALK_11             = 84,
	EVT_HELP_JAINA_TALK_12              = 85,
	EVT_HELP_VEREESA_TALK_13            = 86,
	EVT_HELP_JAINA_TALK_14              = 87,
	EVT_HELP_JAINA_TALK_15              = 88,
	EVT_HELP_MASS_TELEPORT              = 89,
	EVT_HELP_BATTLEFIELD_SETUP          = 90,   // Point d'arret : suite via CRITERIA_TREE_RETRIEVE_JAINA

	// -- The Battle (91 - 100) : trahison de Thalen et debut de la bataille
	EVT_BATTLE_JAINA_TALK_02            = 91,
	EVT_BATTLE_UNUSED                   = 92,   // DELETED - trou conserve pour ne pas decaler la suite
	EVT_BATTLE_THALEN_BETRAYAL          = 93,
	EVT_BATTLE_BARRIER_BREAKS           = 94,
	EVT_BATTLE_ARCHMAGES_READY          = 95,
	EVT_BATTLE_JAINA_TALK_03            = 96,
	EVT_BATTLE_JAINA_TELEPORT           = 97,
	EVT_BATTLE_THALEN_FREEZE            = 98,
	EVT_BATTLE_THADER_WOUNDED           = 99,
	EVT_BATTLE_FIRST_LANDING            = 100,  // Point d'arret : EVENT_MAINTAIN_THE_PROTECTION

	// 101 - 121 : libres

	// -- Help the wounded (122 - 141) : dialogues d'apres-bataille
	// Partie I (122 - 127) : toujours jouee
	EVT_WOUNDED_JAINA_HEDRIC_FACE       = 122,
	EVT_WOUNDED_JAINA_TALK_01           = 123,
	EVT_WOUNDED_HEDRIC_TALK_02          = 124,
	EVT_WOUNDED_JAINA_TALK_03           = 125,
	EVT_WOUNDED_JAINA_WALK              = 126,
	EVT_WOUNDED_HEDRIC_WALK             = 127,
	// Partie II (128 - 140) : jouee seulement si les joueurs suivent Jaina,
	// annulee en bloc via [EVT_WOUNDED_PART2_FIRST, EVT_WOUNDED_PART2_LAST]
	// si l'etape se termine avant la fin des dialogues.
	EVT_WOUNDED_JAINA_KINNDY_FACE       = 128,
	EVT_WOUNDED_KINNDY_TALK_04          = 129,
	EVT_WOUNDED_JAINA_TALK_05           = 130,
	EVT_WOUNDED_KINNDY_TALK_06          = 131,
	EVT_WOUNDED_JAINA_TALK_07           = 132,
	EVT_WOUNDED_KINNDY_TALK_08          = 133,
	EVT_WOUNDED_JAINA_TALK_09           = 134,
	EVT_WOUNDED_JAINA_TALK_10           = 135,
	EVT_WOUNDED_KINNDY_TALK_11          = 136,
	EVT_WOUNDED_JAINA_TALK_12           = 137,
	EVT_WOUNDED_JAINA_TALK_13           = 138,
	EVT_WOUNDED_KINNDY_TALK_14          = 139,
	EVT_WOUNDED_JAINA_TALK_15           = 140,
	EVT_WOUNDED_SCENE_END               = 141,

	// Bornes de la partie II annulable (alias, pas de nouvelles valeurs)
	EVT_WOUNDED_PART2_FIRST             = EVT_WOUNDED_JAINA_KINNDY_FACE,
	EVT_WOUNDED_PART2_LAST              = EVT_WOUNDED_JAINA_TALK_15,

	// -- Wait for Amara (142 - 160) : avertissement de Kalecgos puis retour d'Amara
	EVT_AMARA_KALEC_APPROACH            = 142,
	EVT_AMARA_KALEC_TALK_01             = 143,
	EVT_AMARA_JAINA_TALK_02             = 144,
	EVT_AMARA_KALEC_TALK_03             = 145,
	EVT_AMARA_JAINA_TALK_04             = 146,
	EVT_AMARA_KALEC_TALK_05             = 147,
	EVT_AMARA_JAINA_TALK_06             = 148,
	EVT_AMARA_JAINA_TALK_07             = 149,
	EVT_AMARA_KALEC_TALK_08             = 150,
	EVT_AMARA_RHONIN_TALK_09            = 151,
	EVT_AMARA_KALEC_LEAVE               = 152,
	EVT_AMARA_RHONIN_LEAVE              = 153,
	EVT_AMARA_LEESON_PATH               = 154,
	EVT_AMARA_LEESON_RETURN             = 155,  // Point d'arret : suite via CRITERIA_TREE_ARCHMAGE_LEESON
	EVT_AMARA_JAINA_FACE                = 156,
	EVT_AMARA_LEESON_TALK_10            = 157,
	EVT_AMARA_JAINA_TALK_11             = 158,
	EVT_AMARA_LEESON_LEAVE              = 159,
	EVT_AMARA_JAINA_TO_TOWER            = 160,

	// -- Retrieve Rhonin (161 - 172) : scene finale au sommet de la tour
	EVT_RHONIN_JAINA_TALK_01            = 161,
	EVT_RHONIN_JAINA_FACE               = 162,
	EVT_RHONIN_TALK_02                  = 163,
	EVT_RHONIN_JAINA_TALK_03            = 164,
	EVT_RHONIN_TALK_04                  = 165,
	EVT_RHONIN_TALK_05                  = 166,
	EVT_RHONIN_TALK_06                  = 167,
	EVT_RHONIN_JAINA_TALK_07            = 168,
	EVT_RHONIN_TALK_08                  = 169,
	EVT_RHONIN_JAINA_TALK_09            = 170,
	EVT_RHONIN_TALK_10                  = 171,
	EVT_RHONIN_REDUCE_IMPACT            = 172   // EVENT_REDUCE_IMPACT -> scene finale
};

// =========================================================================
// Evenements d'ambiance de la Horde (bombardement de fond)
// =========================================================================
// Les deux classes suivent le meme schema : Execute lance un tir puis se
// replanifie elle-meme toutes les 8-10s. Elles renvoient toujours false pour
// que l'EventProcessor ne les detruise pas, et elles vivent aussi longtemps
// que la creature porteuse.

// Bombardier aerien : il tourne au-dessus de la ville et lache ses bombes
// sur lui-meme (le sort gere la zone d'impact).
class HordeBombardierThrowBomb : public BasicEvent
{
	public:
		HordeBombardierThrowBomb(Unit* caster) : _caster(caster) { }

		bool Execute(uint64 /*execTime*/, uint32 /*diff*/) override
		{
			_caster->CastSpell(_caster, SPELL_THROW_BOMB, TRIGGERED_FULL_MASK);
			_caster->m_Events.AddEvent(this, _caster->m_Events.CalculateTime(Seconds(urand(8, 10))));
			return false;
		}

	private:
		Unit* _caster;
};

// Demolisseur : il pilonne une bande de terrain fixe (le mur ouest). Chaque
// tir vise un point tire au hasard dans cette bande, ce qui donne un
// bombardement disperse mais toujours dans la meme zone.
class HordeDemolisherThrowBoulder : public BasicEvent
{
    public:
    HordeDemolisherThrowBoulder(Unit* caster) : _caster(caster)
    {
        // Extremites de la bande pilonnee.
        p1 = { -3771.834717f, -4261.928711f, 7.074570f, 4.655093f };
        p2 = { -3793.766357f, -4260.671387f, 6.944610f, 4.655093f };
    }

    // Largeur de la bande pilonnee, de part et d'autre de l'axe p1-p2.
    static constexpr float STRIP_WIDTH = 4.0f;

    bool Execute(uint64 /*execTime*/, uint32 /*diff*/) override
    {
        Position randomPos = GetRandomPointOnStrip(_caster, p1, p2, STRIP_WIDTH, _caster->GetMap());
        _caster->CastSpell(randomPos, SPELL_THROW_BOULDER, TRIGGERED_FULL_MASK);
        _caster->m_Events.AddEvent(this, _caster->m_Events.CalculateTime(Seconds(urand(8, 10))));
        return false;
    }

    // Tire un point au hasard dans le rectangle centre sur le segment p1-p2 :
    // on avance d'une fraction t le long du segment, puis on decale
    // lateralement d'au plus widthMeters / 2.
    Position GetRandomPointOnStrip(Unit* unit, Position const& p1, Position const& p2, float widthMeters, Map* map)
    {
        float dx = p2.GetPositionX() - p1.GetPositionX();
        float dy = p2.GetPositionY() - p1.GetPositionY();
        float len = std::sqrt(dx * dx + dy * dy);

        float ux = dx / len, uy = dy / len;   // direction normalisee
        float px = -uy, py = ux;              // perpendiculaire 2D

        float t = frand(0.0f, 1.0f);
        float offset = frand(-widthMeters / 2.0f, widthMeters / 2.0f);

        float x = p1.GetPositionX() + dx * t + px * offset;
        float y = p1.GetPositionY() + dy * t + py * offset;
        float z = p1.GetPositionZ() + (p2.GetPositionZ() - p1.GetPositionZ()) * t;

        // Recalage sur le vrai sol plutot que sur l'interpolation lineaire :
        // sans ca les rochers tombent dans le decor sur terrain accidente.
        float groundZ = map->GetHeight(unit->GetPhaseShift(), x, y, z + 2.0f, true);
        if (groundZ > INVALID_HEIGHT)
            z = groundZ;

        return Position(x, y, z, 0.f);
    }

    private:
    Unit* _caster;
    Position p1, p2;                        // Extremites de la bande pilonnee
};

// =========================================================================
// InstanceScript
// =========================================================================
class scenario_battle_for_theramore : public InstanceMapScript
{
	public:
	scenario_battle_for_theramore() : InstanceMapScript(BFTScriptName, 5000)
	{
	}

	struct scenario_battle_for_theramore_InstanceScript : public InstanceScript
	{
		// Ordre d'initialisation aligne sur l'ordre de declaration des membres
		// (voir "Etat interne" en bas de la classe) pour eviter tout -Wreorder.
		scenario_battle_for_theramore_InstanceScript(InstanceMap* map) : InstanceScript(map),
			eventId(EVT_COUNCIL_TERVOSH_ARRIVE), woundedTroops(0), archmagesIndex(0),
			waves(0), phase(BFTPhases::FindJaina)
		{
			SetHeaders(DataHeader);
			LoadObjectData(creatureData, gameobjectData);
		}

		enum Spells
		{
			SPELL_WATER_BUCKET          = 42336,
			SPELL_MASS_TELEPORT         = 60516,
			SPELL_MAGIC_QUILL           = 424726,
			SPELL_TIED_UP               = 167469,
			SPELL_CLOSE_PORTAL          = 203542,
			SPELL_DISSOLVE              = 255295,
			SPELL_PRISMATIC_BARRIER     = 235450,
			SPELL_METEOR                = 276973,
			SPELL_ARCANE_CANALISATION   = 288451,
			SPELL_BLAZING_BARRIER       = 295238,
			SPELL_FROST_BREATH          = 300548,
			SPELL_CHILLING_BLAST        = 337053,
			SPELL_ICY_GLARE             = 338517,
			SPELL_VANISH                = 199483,
			SPELL_BIG_EXPLOSION         = 348750,
			SPELL_TELEPORT              = 357601,
			SPELL_SCORCHED_EARTH        = 373139,
			SPELL_ARCANIC_CELL          = 398947,
			SPELL_READING_BOOK_STANDING = 397765,
			SPELL_AREA_TRIGGER_VISUAL   = 473554,
		};

		// Auras portees par les joueurs pendant une tranche de phases.
		// Posees et retirees par CustomScenario::SyncPhaseAuras, depuis
		// SetData(DATA_SCENARIO_PHASE) et OnPlayerEnter : plus rien a poser
		// ni a retirer a la main dans les criteria trees.
		static constexpr CustomScenario::PhaseAura PhaseAuras[] =
		{
			// Bouclier runique distribue par Rhonin, valable toute la bataille.
			{ SPELL_RUNIC_SHIELD, (uint32)BFTPhases::Preparation_Rhonin, (uint32)BFTPhases::HelpTheWounded            },
			// Seau d'eau pour eteindre les incendies d'apres-bataille.
			{ SPELL_WATER_BUCKET, (uint32)BFTPhases::HelpTheWounded,     (uint32)BFTPhases::HelpTheWounded_Extinguish }
		};

		// Ordre d'arrivee des vagues de la Horde : l'index `waves` avance d'un
		// cran a chaque appel de NextWave, declenche par OnUnitDeath. Chaque
		// valeur designe un groupe de spawn (voir HordeMembersInvoker).
		uint32 Waves[HORDE_WAVES_COUNT] =
		{
			DATA_WAVE_WEST,
			DATA_WAVE_CITADEL,
			DATA_WAVE_DOCKS,
			DATA_WAVE_DOORS,
			DATA_WAVE_WEST,
			DATA_WAVE_CITADEL,
			DATA_WAVE_DOCKS,
			DATA_WAVE_WEST,
			DATA_WAVE_CITADEL,
			DATA_WAVE_DOORS
		};

		uint32 GetData(uint32 dataId) const override
		{
			if (dataId == DATA_SCENARIO_PHASE)
				return (uint32)phase;
			else if (dataId == DATA_WOUNDED_TROOPS)
				return woundedTroops;
			else if (dataId == DATA_WAVE_GROUP_ID)
				return Waves[waves < HORDE_WAVES_COUNT ? waves : HORDE_WAVES_COUNT - 1];
			return 0;
		}

		void OnPlayerEnter(Player* player) override
		{
			// Orage permanent : ambiance de la ville assiegee.
			ForceWeather(WEATHER_STATE_THUNDERS, true);

			CustomScenario::SyncPhaseAuras(player, (uint32)phase, PhaseAuras);
		}

		void OnPlayerLeave(Player* player) override
		{
			CustomScenario::RemovePhaseAuras(player, PhaseAuras);
		}

		void SetData(uint32 dataId, uint32 value) override
		{
			if (dataId == DATA_SCENARIO_PHASE)
			{
				phase = (BFTPhases)value;
				CustomScenario::SyncPhaseAuras(instance, value, PhaseAuras);
			}
			else if (dataId == DATA_WOUNDED_TROOPS)
				woundedTroops = value;
		}

		// Comptabilite des vagues : seul et unique point de credit.
		//
		// Le chemin automatique (KillRewarder::Reward) ne credite le scenario
		// qu'avec l'entry reelle de la victime et ignore KillCredit, donc les
		// quatre entries de vague ne peuvent pas y alimenter le meme criteria.
		// Et le faire depuis npc_theramore_horde::JustDied laissait de cote
		// toutes les morts non achevees par un joueur. On credite donc
		// NPC_WAVE_MEMBER_CREDIT ici, une fois par mort, quel que soit le
		// tueur.
		//
		// Le filtre est le string id pose au spawn : sans lui, n'importe quel
		// PNJ de la Horde present sur la carte ferait avancer la barre.
		void OnUnitDeath(Unit* unit) override
		{
			InstanceScript::OnUnitDeath(unit);

			Creature* creature = unit->ToCreature();
			if (!creature || !creature->HasStringId(WaveMemberStringId))
				return;

			InstanceScenario* scenario = instance->GetInstanceScenario();
			if (!scenario)
				return;

			// CriteriaHandler::UpdateCriteria refuse un referencePlayer nul et
			// ignore les joueurs en mode MJ. Le criteria est a l'echelle du
			// scenario, pas du joueur : n'importe quel eligible fait l'affaire.
			Player* creditPlayer = GetCriteriaCreditPlayer();
			if (!creditPlayer)
			{
				TC_LOG_ERROR("scripts", "BFT: aucun joueur eligible dans l'instance, le credit de vague est perdu "
					"(mode MJ actif ?).");
				return;
			}

			scenario->UpdateCriteria(CriteriaType::KillCreature, NPC_WAVE_MEMBER_CREDIT, 1, 0, creature, creditPlayer);

			// La barre de progression appartient au criteria tree : c'est lui qui
			// decide de la fin de la bataille (CRITERIA_TREE_SURVIVE_WAVES). Le
			// script ne repond qu'a une autre question, qu'aucun criteria ne sait
			// exprimer : le terrain est-il degage pour envoyer la suite ?
			if (!IsWaveCleared())
				return;

			if (waves < HORDE_WAVES_COUNT)
				NextWave();
			else
				CompleteWaves();
		}

		// =================================================================
		// Progression du scenario
		// =================================================================
		// Point d'entree principal : chaque criteria tree valide represente
		// une etape terminee par les joueurs. On y fait avancer la phase et
		// on amorce la cinematique suivante (events.ScheduleEvent).
		void OnCompletedCriteriaTree(CriteriaTree const* tree) override
		{
			switch (tree->ID)
			{
				// Step 1 : Find Jaina
				case CRITERIA_TREE_FIND_JAINA:
				{
					ClosePortal(DATA_PORTAL_TO_STORMWIND);
					GetTervosh()->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					GetKinndy()->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					GetKalecgosHuman()->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					if (Creature* jaina = GetCreature(DATA_JAINA_PROUDMOORE))
					{
						Talk(jaina, SAY_REUNION_1);
						SetTarget(jaina);
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::TheCouncil);
					events.ScheduleEvent(EVT_COUNCIL_TERVOSH_ARRIVE, 2s);
					break;
				}
				// Step 2 : The Council
				case CRITERIA_TREE_THE_COUNCIL:
				{
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::Waiting);
					events.ScheduleEvent(EVT_WAITING_TRIGGER, 10s);
					break;
				}
				// Step 3 : Waiting
				case CRITERIA_TREE_WAITING:
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::UnknownTauren);
					events.ScheduleEvent(EVT_TAUREN_ESCORT_SPAWN, 1s);
					break;
				// Step 4 : The Unknow Tauren
				case CRITERIA_TREE_UNKNOW_TAUREN:
				{
					if (Creature* kinndy = GetKinndy())
					{
						kinndy->SetVisible(true);
						kinndy->GetMotionMaster()->MovePath(KinndyPath02, false);
					}
					if (Creature* tervosh = GetTervosh())
					{
						tervosh->SetVisible(true);
						tervosh->GetMotionMaster()->MovePath(TervoshPath03, false);
					}
					for (ObjectGuid guid : citizens)
					{
						if (Creature* citizen = instance->GetCreature(guid))
						{
							citizen->SetNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
							citizen->SetVignette(VIGNETTE_INTERACTION);
						}
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::Evacuation);
					break;
				}
				// Step 5 : Evacuation
				case CRITERIA_TREE_EVACUATION:
				{
                    for (ObjectGuid guid : citizens)
                    {
                        if (Creature* citizen = instance->GetCreature(guid))
                        {
                            citizen->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
                            citizen->SetVignette(VIGNETTE_NONE);
                        }
                    }
					if (Creature* jaina = GetJaina())
					{
                        jaina->SetVignette(VIGNETTE_LADY_JAINA_PROUDMOORE);
						jaina->NearTeleportTo(JainaPoint02);
						jaina->SetHomePosition(JainaPoint02);
						jaina->AI()->SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::ALittleHelp);
					}
					if (Creature* tervosh = GetTervosh())
					{
						tervosh->GetMotionMaster()->Clear();
						tervosh->GetMotionMaster()->MoveIdle();
						tervosh->NearTeleportTo(TervoshPoint01);
						tervosh->SetHomePosition(TervoshPoint01);
						tervosh->CastSpell(tervosh, SPELL_COSMETIC_FIRE_LIGHT);
					}
					if (Creature* kinndy = GetKinndy())
					{
						kinndy->GetMotionMaster()->Clear();
						kinndy->GetMotionMaster()->MoveIdle();
						kinndy->NearTeleportTo(KinndyPoint02);
						kinndy->SetHomePosition(KinndyPoint02);
					}
					if (Creature* kalecgos = GetKalecgosHuman())
					{
						kalecgos->GetMotionMaster()->Clear();
						kalecgos->GetMotionMaster()->MoveIdle();
						kalecgos->NearTeleportTo(KalecgosPoint01);
						kalecgos->SetHomePosition(KalecgosPoint01);
						kalecgos->RemoveUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
						kalecgos->RemoveAllAuras();
					}
					if (Creature* hedric = GetHedric())
					{
						hedric->GetMotionMaster()->Clear();
						hedric->GetMotionMaster()->MoveIdle();
						hedric->SetHomePosition(HedricPoint01);
						hedric->NearTeleportTo(HedricPoint01);
						hedric->SetVisible(false);
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::ALittleHelp);
					break;
				}
				// Step 6 : A Little Help
				case CRITERIA_TREE_A_LITTLE_HELP:
				{
					for (uint8 i = 0; i < FIRE_LOCATION; i++)
					{
						const Position pos = FireLocation[i];
						if (TempSummon* trigger = instance->SummonCreature(NPC_THERAMORE_FIRE_CREDIT, pos))
							trigger->AddAura(SPELL_COSMETIC_LARGE_FIRE, trigger);
					}
					for (ObjectGuid guid : tanks)
					{
						if (Creature* tank = instance->GetCreature(guid))
						{
							tank->SetNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
							tank->SetVignette(VIGNETTE_INTERACTION);
							tank->SetRegenerateHealth(false);
							tank->SetHealth((float)tank->GetHealth() * frand(0.15f, 0.60f));
						}
					}
					for (ObjectGuid guid : civilians)
					{
						if (Creature* citizen = instance->GetCreature(guid))
							citizen->SetVisible(false);
					}
					for (ObjectGuid guid : troops)
					{
						if (Creature* troop = instance->GetCreature(guid))
						{
							troop->SetVignette(VIGNETTE_ALLIANCE_TROOPS);
							switch (troop->GetCreatureTemplate()->unit_class)
							{
								case UNIT_CLASS_PALADIN:
									troop->SetEmoteState(EMOTE_STATE_READY2H);
									break;
								case UNIT_CLASS_MAGE:
									troop->SetEmoteState(RAND(EMOTE_STATE_READY1H, EMOTE_STATE_READY2HL));
									break;
								case UNIT_CLASS_ROGUE:
									break;
								default:
									troop->SetEmoteState(EMOTE_STATE_READY1H);
									break;
							}
						}
					}
                    GetJaina()->SummonGameObject(GOB_PORTAL_TO_ORGRIMMAR, PortalPoint02, QuaternionData::QuaternionData(), 0s);
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::Preparation);
					#ifndef CUSTOM_DEBUG
						events.ScheduleEvent(EVT_HELP_JAINA_TALK_02, 1s);
					#else
						for (uint8 i = 0; i < ARCHMAGES_LOCATION; i++)
							instance->SummonCreature(archmagesLocation[i].dataId, PortalPoint01);
						events.ScheduleEvent(EVT_HELP_BATTLEFIELD_SETUP, 2s);
					#endif
					break;
				}
				// Step 7 : Preparation - Parent
				case CRITERIA_TREE_PREPARATION:
                    GetJaina()->SetVignette(VIGNETTE_LADY_JAINA_PROUDMOORE);
                    SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::TheBattle);
					break;
				// Step 7 : Preparation - Troops motivated
                case CRITERIA_TREE_TOOPS_MOTIVATED:
                    for (ObjectGuid guid : troops)
                    {
                        if (Creature* troop = instance->GetCreature(guid))
                            troop->SetVignette(VIGNETTE_NONE);
                    }
                    break;
				// Step 7 : Preparation - Speak with Rhonin
				case CRITERIA_TREE_TALK_TO_RHONIN:
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::Preparation_Rhonin);
					break;
				// Step 7 : Preparation - Tanks events
				case CRITERIA_TREE_REPAIR_TANKS:
				{
					for (ObjectGuid guid : tanks)
					{
						if (Creature* tank = instance->GetCreature(guid))
						{
							if (Creature* fire = tank->FindNearestCreature(NPC_THERAMORE_FIRE_CREDIT, 5.f))
								fire->DespawnOrUnsummon();

							tank->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
							tank->SetRegenerateHealth(true);
							tank->SetHealth(tank->GetMaxHealth());
						}
					}
					break;
				}            
				// Step 8 : The Battle - Retrieve Lady Jaina Proudmoore
				case CRITERIA_TREE_RETRIEVE_JAINA:
				{
					if (Creature* jaina = GetJaina())
					{
						Talk(jaina, SAY_BATTLE_01);
						SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, jaina);
						jaina->SetBoundingRadius(20.f);
						jaina->SetRegenerateHealth(false);
					}
					if (Creature* rhonin = GetRhonin())
					{
						SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, rhonin);
						rhonin->SetRegenerateHealth(false);
					}
					if (Creature* kalecgos = GetKalecgosDragon())
					{
						SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, kalecgos);
						kalecgos->SetRegenerateHealth(false);
					}
					if (Creature* drok = GetDrok())
					{
						drok->setActive(true);
						drok->SetVisible(true);
					}
					if (Creature* gruhta = GetGruhta())
					{
						gruhta->setActive(true);
						gruhta->SetVisible(true);
					}
					// Tous les tanks sauf le dernier sont detruits au debut de
					// l'assaut. size() etant non signe, un vecteur vide ferait
					// deborder la borne : la garde ci-dessous l'evite.
					for (uint8 i = 0; !tanks.empty() && i < tanks.size() - 1; i++)
					{
						if (Creature* tank = instance->GetCreature(tanks[i]))
							tank->KillSelf();
					}
                    HordeMembersInvoker(DATA_DECORATION_WEST, true);
                    SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::TheBattle_Survive);
					events.ScheduleEvent(EVT_BATTLE_JAINA_TALK_02, 10s);
					break;
				}
				// Step 9 : The Battle - Parent
				case CRITERIA_TREE_SURVIVE_THE_BATTLE:
				{
					SpawnWoundedTroops();
					RelocateTroops();
					GetBarrier01()->ResetDoorOrButton();
					GetBarrier02()->ResetDoorOrButton();
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::HelpTheWounded);
					events.ScheduleEvent(EVT_WOUNDED_JAINA_HEDRIC_FACE, 3s);
					break;
				}
				// Step 9 : The Battle - After 10 waves
				case CRITERIA_TREE_SURVIVE_WAVES:
				{
					DespawnDummies();
					if (Creature* kalecgos = GetKalecgosDragon())
						kalecgos->AI()->SetData(DATA_KALECGOS_CANCEL_EVENT, 0U);
                    break;
				}
                // Step 9 : The Battle - After the protection broke
                case CRITERIA_TREE_MAINTAIN_PROTECTION:
                    NextWave();
                    break;
				// Step 10 : Help the wounded - Parent
				case CRITERIA_TREE_HELP_THE_WOUNDED:
				{
					// Les joueurs ont fini avant la fin des dialogues : on
					// coupe toute la partie II d'un bloc.
					for (uint32 i = EVT_WOUNDED_PART2_FIRST; i <= EVT_WOUNDED_PART2_LAST; ++i)
						events.CancelEvent(i);
                    GetJaina()->SetVignette(VIGNETTE_LADY_JAINA_PROUDMOORE);
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::WaitForAmara);
					events.ScheduleEvent(EVT_WOUNDED_SCENE_END, 5ms);
					break;
				}
				// Step 10 : Help the wounded - Rejoin Lady Jaina Proudmoore after the attack
				case CRITERIA_TREE_FOLLOW_JAINA:
					events.ScheduleEvent(EVT_WOUNDED_JAINA_KINNDY_FACE, 3s);
					break;
				// Step 10 : Help the wounded - Help teleporting the wounded troops
				case CRITERIA_TREE_HELP_THE_TROOPS:
				{
					if (Creature* jaina = GetJaina())
					{
                        std::list<Creature*> results;
                        jaina->GetCreatureListWithEntryInGrid(results, NPC_THERAMORE_WOUNDED_TROOP, SIZE_OF_GRIDS);

						if (results.empty())
							return;

                        for (Creature* woundedTroop : results)
                        {
                            woundedTroop->SetVignette(VIGNETTE_NONE);
                            woundedTroop->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
                        }
					}
					break;
				}
				// Step 10 : Help the wounded - Extinguish the fires
				case CRITERIA_TREE_EXTINGUISH_FIRES:
					MassDespawn(NPC_THERAMORE_FIRE_CREDIT);
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::HelpTheWounded_Extinguish);
					break;
				// Step 11 : Wait for Archmage Leeson returns - Parent
				case CRITERIA_TREE_WAIT_ARCHMAGE_LEESON:
					break;
				// Step 11 : Wait for Archmage Leeson returns - Rejoin Lady Jaina Proudmoore
				case CRITERIA_TREE_JOIN_JAINA:
				{
					if (Creature* kalecgos = GetKalecgosHuman())
					{
						kalecgos->SetVisible(true);
						kalecgos->GetMotionMaster()->Clear();
						kalecgos->GetMotionMaster()->MoveIdle();
						kalecgos->NearTeleportTo(KalecPath02.Nodes[0].X, KalecPath02.Nodes[0].Y, KalecPath02.Nodes[0].Z, *KalecPath02.Nodes[0].Orientation);
					}
					if (Creature* rhonin = GetRhonin())
					{
						rhonin->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, RhoninPoint01, true, RhoninPoint01.GetOrientation());

						for (uint8 i = 0; i < TOWER_BARRIERS_LOCATION; i++)
						{
							rhonin->SummonGameObject(GOB_ENERGY_BARRIER_TOWER, TowerBarriers[i].position, TowerBarriers[i].quaternion, 0s);
						}
					}
					for (uint8 i = 0; i < eventCreatureDataCount; i++)
					{
						if (Creature* creature = GetCreature(creatureData[i].type))
							creature->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					}
					SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::WaitForAmara_JoinJaina);
					events.ScheduleEvent(EVT_AMARA_KALEC_APPROACH, 1s);
					break;
				}
				// Step 11 : Wait for Archmage Leeson returns - Wait for Archmage Leeson returns
				case CRITERIA_TREE_ARCHMAGE_LEESON:
					events.ScheduleEvent(EVT_AMARA_JAINA_FACE, 1s);
					break;
				// Step 12 : Retrieve Rhonin - Parent
				case CRITERIA_TREE_RETRIEVE_RHONIN:
					DoCastSpellOnPlayers(SPELL_THERAMORE_EXPLOSION_SCENE);
					break;
				// Step 12 : Retrieve Rhonin - Retrieve Rhonin at the top of the tower
				case CRITERIA_TREE_RETRIEVE:
                    GetRhonin()->SetVignette(VIGNETTE_NONE);
                    SetData(DATA_SCENARIO_PHASE, (uint32)BFTPhases::RetrieveRhonin_JoinRhonin);
					events.ScheduleEvent(EVT_RHONIN_JAINA_TALK_01, 1s);
					break;
			}
		}

		// Classement des creatures au spawn : chaque groupe est memorise dans
		// sa propre liste de GUID, ce qui permet ensuite de les manipuler en
		// masse (marqueurs d'objectif, teleports, despawns).
		void OnCreatureCreate(Creature* creature) override
		{
			InstanceScript::OnCreatureCreate(creature);

			// Portee de visibilite maximale : la bataille se joue a l'echelle
			// de toute la ville, on ne veut aucun pop-in.
            creature->SetVisibilityDistanceOverride(VisibilityDistanceType::Gigantic);
            creature->SetPvpFlag(UNIT_BYTE2_FLAG_PVP);
            creature->SetUnitFlag(UNIT_FLAG_PVP_ENABLING);

			if (creature->IsCivilian())
			{
				creature->PauseMovement();
				creature->SetPvP(false);
				creature->SetImmuneToNPC(true);
				civilians.push_back(creature->GetGUID());
			}

			switch (creature->GetEntry())
			{
				case NPC_THERAMORE_CITIZEN_MALE:
				case NPC_THERAMORE_CITIZEN_FEMALE:
					creature->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
                    if (!creature->HasStringId("IgnoreClick"))
					    citizens.push_back(creature->GetGUID());
					break;
				case NPC_ARCHMAGE_TERVOSH:
					creature->SetEmoteState(EMOTE_STATE_READ_BOOK_AND_TALK);
					break;
				case NPC_UNMANNED_TANK:
					creature->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
					tanks.push_back(creature->GetGUID());
					break;
				case NPC_THERAMORE_MARKSMAN:
				case NPC_THERAMORE_FOOTMAN:
				case NPC_THERAMORE_ARCANIST:
				case NPC_THERAMORE_FAITHFUL:
				case NPC_THERAMORE_OFFICER:
					// Seules les troupes statiques comptent : celles qui
					// patrouillent ou appartiennent a une formation ont deja
					// leur propre comportement et ne doivent pas etre
					// deplacees par les scripts de phase.
					if (creature->GetWaypointPathId() || creature->IsFormationLeader() || creature->GetFormation())
						break;
					troops.push_back(creature->GetGUID());
					break;
				case NPC_CAPTAIN_DROK:
				case NPC_WAVE_CALLER_GRUHTA:
				case NPC_KALECGOS_DRAGON:
					// Acteurs de la bataille : masques et inactifs jusqu'a ce
					// que le scenario les revele.
					creature->setActive(false);
					creature->SetVisible(false);
					break;
				case NPC_JAINA_PROUDMOORE:
					creature->SetVignette(VIGNETTE_LADY_JAINA_PROUDMOORE);
					break;
			}
		}

		void OnGameObjectCreate(GameObject* go) override
		{
			InstanceScript::OnGameObjectCreate(go);

			go->SetVisibilityDistanceOverride(VisibilityDistanceType::Large);

			switch (go->GetEntry())
			{
				case GOB_PORTAL_TO_DALARAN:
				case GOB_PORTAL_TO_STORMWIND:
				case GOB_PORTAL_TO_ORGRIMMAR:
					go->SetLootState(GO_READY);
					go->UseDoorOrButton();
					go->SetFlag(GO_FLAG_NOT_SELECTABLE);
					break;
				case GOB_ENERGY_BARRIER:
					go->SetFlag(GO_FLAG_NOT_SELECTABLE);
					break;
				default:
					break;
			}
		}

		// =================================================================
		// Chaine d'events cinematiques
		// =================================================================
		// L'affectation `eventId = events.ExecuteEvent()` est volontaire :
		// elle memorise l'event en cours pour que Next() puisse planifier le
		// suivant sans que chaque case ait a se nommer lui-meme.
		void Update(uint32 diff) override
		{
			scheduler.Update(diff);

			events.Update(diff);
			switch (eventId = events.ExecuteEvent())
			{
				// The Council (1 - 23)
				// Conseil de guerre dans la tour : Tervosh et Kinndy rejoignent
				// Jaina et Kalecgos, longue passe de dialogues, puis tout le
				// monde se disperse et Jaina rejoint la table (EVT_COUNCIL_JAINA_TO_TABLE, point
				// d'arret : la suite depend de MOVEMENT_INFO_POINT_01).
				#pragma region THE_COUNCIL

				case EVT_COUNCIL_TERVOSH_ARRIVE:
					if (Creature* tervosh = GetTervosh())
					{
						tervosh->SetEmoteState(EMOTE_STAND_STATE_NONE);
						tervosh->GetMotionMaster()->MovePath(TervoshPath01, false);
					}
					Next(4s);
					break;
				case EVT_COUNCIL_KINNDY_ARRIVE:
					if (Creature* kinndy = GetKinndy())
					{
						kinndy->SetWalk(true);
						kinndy->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, KinndyPoint01, true, 1.09f);
					}
					// En debug on saute les dialogues 3 a 19 et on reprend
					// directement a la dispersion des acteurs.
					#ifdef CUSTOM_DEBUG
						events.ScheduleEvent(EVT_COUNCIL_KALEC_LEAVE, 2s);
					#else
						Next(5s);
					#endif
					break;
				case EVT_COUNCIL_KINNDY_TALK_02:
					Talk(GetKinndy(), SAY_REUNION_2);
					SetTarget(GetKinndy());
					Next(13s);
					break;
				case EVT_COUNCIL_JAINA_TALK_03:
					Talk(GetJaina(), SAY_REUNION_3);
					SetTarget(GetJaina());
					Next(12s);
					break;
				case EVT_COUNCIL_KINNDY_TALK_04:
					Talk(GetKinndy(), SAY_REUNION_4);
					SetTarget(GetKinndy());
					Next(6s);
					break;
				case EVT_COUNCIL_JAINA_TALK_05:
					Talk(GetJaina(), SAY_REUNION_5);
					SetTarget(GetJaina());
					Next(8s);
					break;
				case EVT_COUNCIL_TERVOSH_TALK_06:
					Talk(GetTervosh(), SAY_REUNION_6);
					SetTarget(GetTervosh());
					Next(8s);
					break;
				case EVT_COUNCIL_KALEC_TALK_07:
					Talk(GetKalecgosHuman(), SAY_REUNION_7);
					SetTarget(GetKalecgosHuman());
					Next(6s);
					break;
				case EVT_COUNCIL_KALEC_TALK_08:
					Talk(GetKalecgosHuman(), SAY_REUNION_8);
					Next(9s);
					break;
				case EVT_COUNCIL_TERVOSH_TALK_09:
					Talk(GetTervosh(), SAY_REUNION_9);
					Next(1s);
					break;
				case EVT_COUNCIL_KINNDY_TALK_09_BIS:
					Talk(GetKinndy(), SAY_REUNION_9_BIS);
					Next(4s);
					break;
				case EVT_COUNCIL_JAINA_TALK_10:
					Talk(GetJaina(), SAY_REUNION_10);
					SetTarget(GetJaina());
					Next(6s);
					break;
				case EVT_COUNCIL_KALEC_TALK_11:
					Talk(GetKalecgosHuman(), SAY_REUNION_11);
					SetTarget(GetKalecgosHuman());
					Next(4s);
					break;
				case EVT_COUNCIL_JAINA_TALK_12:
					Talk(GetJaina(), SAY_REUNION_12);
					SetTarget(GetJaina());
					Next(6s);
					break;
				case EVT_COUNCIL_KINNDY_TALK_13:
					Talk(GetKinndy(), SAY_REUNION_13);
					SetTarget(GetKinndy());
					Next(6s);
					break;
				case EVT_COUNCIL_KALEC_TALK_14:
					Talk(GetKalecgosHuman(), SAY_REUNION_14);
					SetTarget(GetKalecgosHuman());
					Next(7s);
					break;
				case EVT_COUNCIL_JAINA_TALK_15:
					Talk(GetJaina(), SAY_REUNION_15);
					SetTarget(GetJaina());
					Next(4s);
					break;
				case EVT_COUNCIL_KALEC_TALK_16:
					Talk(GetKalecgosHuman(), SAY_REUNION_16);
					SetTarget(GetKalecgosHuman());
					Next(4s);
					break;
				case EVT_COUNCIL_KALEC_TALK_17:
					Talk(GetKalecgosHuman(), SAY_REUNION_17);
					Next(4s);
					break;
				// Dispersion : chacun repart vers son poste. Point d'entree
				// DEBUG (voir EVT_COUNCIL_KINNDY_ARRIVE).
				case EVT_COUNCIL_KALEC_LEAVE:
					ClearTarget();
					if (Creature* kalecgos = GetKalecgosHuman())
					{
						kalecgos->SetSpeedRate(MOVE_WALK, 1.6f);
						kalecgos->GetMotionMaster()->MovePath(KalecPath01, false);
					}
					Next(2s);
					break;
				case EVT_COUNCIL_TERVOSH_LEAVE:
					GetTervosh()->GetMotionMaster()->MovePath(TervoshPath02, false);
					Next(5s);
					break;
				case EVT_COUNCIL_KINNDY_LEAVE:
					GetKinndy()->GetMotionMaster()->MovePath(KinndyPath01, false);
					Next(6s);
					break;
				// Point d'arret : Jaina rejoint la table du conseil. C'est son
				// AI qui leve EVENT_THE_COUNCIL en arrivant sur le point.
				case EVT_COUNCIL_JAINA_TO_TABLE:
					GetJaina()->SetWalk(true);
					GetJaina()->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_01, JainaPoint01, true, JainaPoint01.GetOrientation());
					break;

				#pragma endregion

				// Waiting (24)
				// Simple validation d'etape : laisse aux joueurs le temps de
				// souffler avant l'arrivee de Perith.
				#pragma region WAITING

				case EVT_WAITING_TRIGGER:
					TriggerGameEvent(EVENT_WAITING);
					break;

				#pragma endregion

				// The Unknown Tauren (25 - 70)
				// Perith Stormhoove et son escorte entrent dans la tour et
				// annoncent l'attaque de la Horde. Longue scene de dialogue
				// entre Pained, Jaina, le chevalier et Perith, entrecoupee de
				// deplacements. Se termine sur EVT_TAUREN_PAINED_LEAVE (sortie de Pained).
				#pragma region THE_UNKNOWN_TAUREN

				// Spawn de l'escorte de Perith. En debug on la fait disparaitre
				// aussitot et on saute directement a la fin de la scene.
				case EVT_TAUREN_ESCORT_SPAWN:
					for (uint8 i = 0; i < PERITH_LOCATION; i++)
					{
						if (Creature* creature = instance->SummonCreature(perithLocation[i].dataId, perithLocation[i].position))
						{
							if (creature->GetEntry() == NPC_KNIGHT_OF_THERAMORE)
							{
								creature->SetSheath(SHEATH_STATE_UNARMED);
								creature->SetEmoteState(EMOTE_STATE_WAGUARDSTAND01);
							}

							creature->SetWalk(true);
							creature->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
							creature->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, perithLocation[i].destination, false, perithLocation[i].destination.GetOrientation());
						}
					}
					#ifdef CUSTOM_DEBUG
					{
						GetPerith()->DespawnOrUnsummon();
						GetKnight()->DespawnOrUnsummon();

						events.ScheduleEvent(EVT_TAUREN_PAINED_LEAVE, 2s);
					}
					#else
						Next(7s);
					#endif
					break;
				case EVT_TAUREN_JAINA_TARGET_PERITH:
					GetJaina()->SetTarget(GetPerith()->GetGUID());
					Next(2s);
					break;
				case EVT_TAUREN_PAINED_TALK_01:
					Talk(GetPained(), SAY_WARN_1);
					SetTarget(GetPained());
					Next(1s);
					break;
				case EVT_TAUREN_JAINA_TALK_02:
					Talk(GetJaina(), SAY_WARN_2);
					SetTarget(GetJaina());
					Next(1s);
					break;
				case EVT_TAUREN_PAINED_TALK_03:
					Talk(GetPained(), SAY_WARN_3);
					SetTarget(GetPained());
					Next(6s);
					break;
				case EVT_TAUREN_PAINED_TALK_04:
					Talk(GetPained(), SAY_WARN_4);
					Next(7s);
					break;
				case EVT_TAUREN_JAINA_TALK_05:
					Talk(GetJaina(), SAY_WARN_5);
					SetTarget(GetJaina());
					Next(6s);
					break;
				case EVT_TAUREN_PAINED_TALK_06:
					Talk(GetPained(), SAY_WARN_6);
					SetTarget(GetPained());
					Next(10s);
					break;
				case EVT_TAUREN_PAINED_APPROACH:
					ClearTarget();
					GetPained()->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_NONE, GetJaina(), 1.8f);
					Next(2s);
					break;
				case EVT_TAUREN_PAINED_SALUTE:
					SetTarget(GetJaina());
					GetPained()->SetEmoteState(EMOTE_STATE_USE_STANDING);
					Next(1s);
					break;
				case EVT_TAUREN_PAINED_TALK_07:
					GetJaina()->SetEmoteState(EMOTE_STATE_USE_STANDING);
					GetPained()->SetEmoteState(EMOTE_STATE_NONE);
					Talk(GetPained(), SAY_WARN_7);
					Next(3s);
					break;
				case EVT_TAUREN_JAINA_TALK_08:
					GetJaina()->SetEmoteState(EMOTE_STATE_NONE);
					Talk(GetJaina(), SAY_WARN_8);
					Next(4s);
					break;
				case EVT_TAUREN_JAINA_TALK_09:
					Talk(GetJaina(), SAY_WARN_9);
					SetTarget(GetJaina());
					Next(2s);
					break;
				case EVT_TAUREN_JAINA_TALK_10:
					Talk(GetJaina(), SAY_WARN_10);
					ClearTarget();
					GetPained()->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_01, PainedPoint01, true, PainedPoint01.GetOrientation());
					Next(2s);
					break;
				case EVT_TAUREN_KNIGHT_APPROACH:
					GetKnight()->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_01, GetJaina(), 3.0f);
					Next(2s);
					break;
				case EVT_TAUREN_KNIGHT_TALK_11:
					Talk(GetKnight(), SAY_WARN_11);
					SetTarget(GetKnight());
					Next(4s);
					break;
				case EVT_TAUREN_JAINA_TALK_12:
					Talk(GetJaina(), SAY_WARN_12);
					SetTarget(GetJaina());
					Next(5s);
					break;
				case EVT_TAUREN_PAINED_TALK_13:
					ClearTarget();
					Talk(GetPained(), SAY_WARN_13);
					if (Creature* officer = GetKnight())
					{
						officer->GetMotionMaster()->MovePath(OfficerPath01, false);
						officer->DespawnOrUnsummon(15s);
					}
					GetPerith()->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_NONE, GetJaina(), 3.0f);
					Next(3s);
					break;
				case EVT_TAUREN_PERITH_TALK_14:
					Talk(GetPerith(), SAY_WARN_14);
					GetPerith()->SetTarget(GetJaina()->GetGUID());
					SetTarget(GetPerith());
					Next(10s);
					break;
				case EVT_TAUREN_JAINA_TALK_15:
					Talk(GetJaina(), SAY_WARN_15);
					Next(4s);
					break;
				case EVT_TAUREN_PERITH_TALK_16:
					Talk(GetPerith(), SAY_WARN_16);
					Next(11s);
					break;
				case EVT_TAUREN_PERITH_TALK_17:
					Talk(GetPerith(), SAY_WARN_17);
					Next(10s);
					break;
				case EVT_TAUREN_PERITH_TALK_18:
					Talk(GetPerith(), SAY_WARN_18);
					Next(11s);
					break;
				case EVT_TAUREN_JAINA_TALK_19:
					Talk(GetJaina(), SAY_WARN_19);
					Next(7s);
					break;
				case EVT_TAUREN_PERITH_TALK_20:
					Talk(GetPerith(), SAY_WARN_20);
					Next(5s);
					break;
				case EVT_TAUREN_JAINA_TALK_21:
					Talk(GetJaina(), SAY_WARN_21);
					Next(1s);
					break;
				case EVT_TAUREN_PERITH_TALK_22:
					Talk(GetPerith(), SAY_WARN_22);
					Next(15s);
					break;
				case EVT_TAUREN_PERITH_TALK_23:
					Talk(GetPerith(), SAY_WARN_23);
					Next(9s);
					break;
				case EVT_TAUREN_JAINA_TALK_24:
					Talk(GetJaina(), SAY_WARN_24);
					Next(14s);
					break;
				case EVT_TAUREN_PERITH_TALK_25:
					Talk(GetPerith(), SAY_WARN_25);
					Next(16s);
					break;
				case EVT_TAUREN_JAINA_TALK_26:
					Talk(GetJaina(), SAY_WARN_26);
					Next(5s);
					break;
				case EVT_TAUREN_JAINA_TURN_AWAY:
					if (Creature* jaina = GetJaina())
					{
						jaina->SetTarget(ObjectGuid::Empty);
						jaina->SetFacingTo(3.33f);
					}
					Next(2s);
					break;
				// Jaina redige l'ordre d'evacuation : la plume magique est un
				// visuel de channel, retire a l'event suivant.
				case EVT_TAUREN_JAINA_TALK_27:
                    if (Creature* jaina = GetJaina())
                    {
                        Talk(jaina, SAY_WARN_27);
                        jaina->CastSpell(jaina, SPELL_MAGIC_QUILL);
                    }
					Next(10s);
					break;
				case EVT_TAUREN_JAINA_QUILL_END:
					if (Creature* jaina = GetJaina())
					{
						jaina->RemoveAurasDueToSpell(SPELL_MAGIC_QUILL);
						jaina->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
						jaina->SetFacingToObject(GetPerith());
					}
					Next(2s);
					break;
				case EVT_TAUREN_JAINA_TALK_28:
					Talk(GetJaina(), SAY_WARN_28);
					Next(5s);
					break;
				case EVT_TAUREN_PERITH_TALK_29:
					Talk(GetPerith(), SAY_WARN_29);
					Next(5s);
					break;
				case EVT_TAUREN_PERITH_TALK_30:
					Talk(GetPerith(), SAY_WARN_30);
					Next(10s);
					break;
				case EVT_TAUREN_JAINA_TALK_31:
					Talk(GetJaina(), SAY_WARN_31);
					Next(4s);
					break;
				case EVT_TAUREN_PERITH_TALK_32:
					Talk(GetPerith(), SAY_WARN_32);
					Next(4s);
					break;
				case EVT_TAUREN_JAINA_TALK_33:
					Talk(GetJaina(), SAY_WARN_33);
					Next(4s);
					break;
				case EVT_TAUREN_PERITH_TALK_34:
					Talk(GetPerith(), SAY_WARN_34);
					Next(3s);
					break;
				case EVT_TAUREN_PERITH_LEAVE:
					ClearTarget();
					GetPained()->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					if (Creature* perith = GetPerith())
					{
						perith->GetMotionMaster()->MovePath(OfficerPath01, false);
						perith->DespawnOrUnsummon(15s);
					}
					Next(5s);
					break;
				case EVT_TAUREN_JAINA_TALK_35:
					Talk(GetJaina(), SAY_WARN_35);
					GetJaina()->SetFacingToObject(GetPained());
					GetPained()->SetFacingToObject(GetJaina());
					Next(7s);
					break;
				case EVT_TAUREN_PAINED_TALK_36:
					Talk(GetPained(), SAY_WARN_36);
					Next(3s);
					break;
				case EVT_TAUREN_JAINA_TALK_37:
					Talk(GetJaina(), SAY_WARN_37);
					Next(3s);
					break;
				// Point d'arret : Pained sort de la salle. C'est son AI qui
				// leve EVENT_THE_UNKNOWN_TAUREN a la fin du chemin.
				case EVT_TAUREN_PAINED_LEAVE:
					GetJaina()->SetFacingTo(0.39f);
					GetPained()->GetMotionMaster()->MovePath(KinndyPath01, false);
					break;

				#pragma endregion

				// A Little Help (71 - 90)
				// Arrivee des renforts de Dalaran : Hedric ouvre la scene, le
				// portail s'ouvre, les six archimages en sortent un a un
				// (EVT_HELP_ARCHMAGES_ARRIVAL, qui se repete lui-meme), discours et harangue,
				// puis teleport de masse et mise en place complete du champ
				// de bataille (EVT_HELP_BATTLEFIELD_SETUP).
				#pragma region A_LITTLE_HELP

				case EVT_HELP_JAINA_TALK_02:
					Talk(GetJaina(), SAY_PRE_BATTLE_2);
					ScheduleCameraShakes();
					HordeMembersInvoker(DATA_DECORATION_ENTRANCE, true);
					if (Creature* hedric = GetHedric())
					{
						hedric->SetVisible(true);
						hedric->PlayDirectSound(SOUND_FEARFUL_CROWD);
						hedric->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					}
					Next(3s);
					break;
				case EVT_HELP_HEDRIC_TALK_01:
					if (Creature* hedric = GetHedric())
					{
						Talk(hedric, SAY_PRE_BATTLE_1);
						hedric->GetMotionMaster()->MovePath(HedricPath01, false);
					}
					Next(2s);
					break;
				case EVT_HELP_HEDRIC_TALK_03:
					Talk(GetHedric(), SAY_PRE_BATTLE_3);
					Next(3s);
					break;
				case EVT_HELP_JAINA_TALK_04:
					Talk(GetJaina(), SAY_PRE_BATTLE_4);
					Next(2s);
					break;
				case EVT_HELP_OPEN_PORTAL:
					GetJaina()->SummonGameObject(GOB_PORTAL_TO_DALARAN, PortalPoint01, QuaternionData::QuaternionData(), 0s);
					if (Creature* hedric = GetHedric())
						hedric->SetFacingTo(0.461802f);
					Next(500ms);
					break;
				case EVT_HELP_HEDRIC_BACKSTEP:
					if (Creature* hedric = GetHedric())
					{
						hedric->SetWalk(true);
						hedric->GetMotionMaster()->MoveBackward(MOVEMENT_INFO_POINT_01, HedricPoint02, nullptr, 1.4f);
					}
					Next(500ms);
					break;
				// Sortie du portail, un archimage a la fois : l'event se
				// repete toutes les ~900ms tant qu'il en reste, puis passe la
				// main au suivant une fois la table epuisee.
				case EVT_HELP_ARCHMAGES_ARRIVAL:
					if (archmagesIndex >= ARCHMAGES_LOCATION)
						Next(2s);
					else if (Creature* creature = instance->SummonCreature(archmagesLocation[archmagesIndex].dataId, PortalPoint01))
					{
						creature->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
						creature->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
						creature->SetSheath(SHEATH_STATE_UNARMED);
						creature->CastSpell(creature, SPELL_TELEPORT_DUMMY);
						creature->SetWalk(true);
						creature->GetMotionMaster()->Clear();
						creature->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, archmagesLocation[archmagesIndex].destination, false, archmagesLocation[archmagesIndex].destination.GetOrientation());
						archmagesIndex++;
						events.Repeat(800ms, 1s);
					}
					break;
				case EVT_HELP_RHONIN_TALK_05:
					Talk(GetRhonin(), SAY_PRE_BATTLE_5);
					SetTarget(GetRhonin());
					ClosePortal(DATA_PORTAL_TO_DALARAN);
					Next(2800ms);
					break;
				case EVT_HELP_JAINA_TALK_06:
					Talk(GetJaina(), SAY_PRE_BATTLE_6);
					SetTarget(GetJaina());
					Next(11s);
					break;
				case EVT_HELP_JAINA_TALK_07:
					Talk(GetJaina(), SAY_PRE_BATTLE_7);
					Next(9s);
					break;
				case EVT_HELP_THALEN_TALK_08:
					Talk(GetThalen(), SAY_PRE_BATTLE_8);
					SetTarget(GetThalen());
					Next(7s);
					break;
				case EVT_HELP_JAINA_TALK_09:
					Talk(GetJaina(), SAY_PRE_BATTLE_9);
					SetTarget(GetJaina());
					Next(7s);
					break;
				case EVT_HELP_JAINA_TALK_10:
					Talk(GetJaina(), SAY_PRE_BATTLE_10);
					Next(6s);
					break;
				case EVT_HELP_RHONIN_TALK_11:
					Talk(GetRhonin(), SAY_PRE_BATTLE_11);
					SetTarget(GetRhonin());
					Next(2s);
					break;
				case EVT_HELP_JAINA_TALK_12:
					Talk(GetJaina(), SAY_PRE_BATTLE_12);
					SetTarget(GetTervosh());
					Next(6s);
					break;
				case EVT_HELP_VEREESA_TALK_13:
					Talk(GetVereesa(), SAY_PRE_BATTLE_13);
					SetTarget(GetVereesa());
					Next(10s);
					break;
				case EVT_HELP_JAINA_TALK_14:
					if (Creature* jaina = GetJaina())
					{
						Talk(jaina, SAY_PRE_BATTLE_14);
						SetTarget(jaina);
						jaina->SetTarget(ObjectGuid::Empty);
						jaina->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					}
					Next(8s);
					break;
				case EVT_HELP_JAINA_TALK_15:
					if (Creature* jaina = GetJaina())
					{
						Talk(jaina, SAY_PRE_BATTLE_15);
						if (Player* player = GetFirstPlayer())
							SetTarget(player);
					}
					if (Creature* vereesa = GetVereesa())
					{
                        vereesa->SetWalk(true);
                        vereesa->CastSpell(vereesa, SPELL_VANISH);
                        vereesa->GetMotionMaster()->MovePoint(0, VereesaPoint01);
						vereesa->SetFaction(FACTION_FRIENDLY);
					}
					Next(5s);
					break;
				case EVT_HELP_MASS_TELEPORT:
					ClearTarget();
					GetJaina()->CastSpell(GetJaina(), SPELL_MASS_TELEPORT);
                    GetVereesa()->SetVisible(false);
					Next(4600ms);
					break;
				// Mise en place du champ de bataille : Kalecgos dragon prend
				// son vol, tous les acteurs sont teleportes a leur poste
				// (actorsRelocation) et recoivent leur role de la bataille
				// (vignette, gossip, channel). Point d'arret : la suite passe
				// par CRITERIA_TREE_RETRIEVE_JAINA.
				case EVT_HELP_BATTLEFIELD_SETUP:
					EnsureBarrierHasDamage();
					if (Creature* kalecgos = GetKalecgosDragon())
					{
						kalecgos->setActive(true);
						kalecgos->SetVisible(true);
						kalecgos->SetSpeed(MOVE_RUN, 25.f);
						kalecgos->AI()->SetData(DATA_KALECGOS_CIRCLE_EVENT, 0U);
					}
					for (uint8 i = 0; i < ACTORS_RELOCATION; i++)
					{
						if (Creature* creature = GetCreature(actorsRelocation[i].dataId))
						{
							creature->RemoveAllAuras();
							creature->SetTarget(ObjectGuid::Empty);
							creature->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
							creature->SetSheath(SHEATH_STATE_MELEE);
							creature->GetMotionMaster()->Clear();
							creature->GetMotionMaster()->MoveIdle();
							creature->NearTeleportTo(actorsRelocation[i].destination);
							creature->SetHomePosition(actorsRelocation[i].destination);

							switch (creature->GetEntry())
							{
								case NPC_AMARA_LEESON:
									creature->CastSpell(creature, SPELL_PORTAL_CHANNELING_03);
									break;
								case NPC_RHONIN:
									creature->SetVignette(VIGNETTE_RHONIN);
									creature->CastSpell(creature, SPELL_CHAT_BUBBLE, true);
									creature->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
									break;
								case NPC_THADER_WINDERMERE:
									creature->SetVignette(VIGNETTE_THADER_WINDERMERE);
									creature->CastSpell(creature, SPELL_CHAT_BUBBLE, true);
									creature->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
									break;
								case NPC_THALEN_SONGWEAVER:
									creature->CastSpell(creature, SPELL_PORTAL_CHANNELING_02);
									break;
								case NPC_JAINA_PROUDMOORE:
									GetBarrier01()->UseDoorOrButton();
									TeleportPlayers(GetJaina(), actorsRelocation[i].destination, 15.0f);
									break;
								case NPC_KALECGOS:
									creature->SetVisible(false);
									break;
							}
						}
					}
					break;

				#pragma endregion

				// The Battle (91 - 100)
				// Trahison de Thalen Songweaver : il passe cote Horde, brise
				// la barriere mystique, blesse Thader et s'enfuit. La bataille
				// commence, Kalecgos entre en combat et le premier groupe
				// debarque du bateau.
				#pragma region THE_BATTLE

				case EVT_BATTLE_JAINA_TALK_02:
					Talk(GetJaina(), SAY_BATTLE_02);
					events.ScheduleEvent(EVT_BATTLE_THALEN_BETRAYAL, 10s);
					break;
				// DELETED
				//case EVT_BATTLE_UNUSED:
				//    break;
				// Thalen bascule cote Horde et fait tomber le premier meteore.
				case EVT_BATTLE_THALEN_BETRAYAL:
					if (Creature* thalen = GetThalen())
					{
						thalen->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
						thalen->SetReactState(REACT_PASSIVE);
						thalen->SetFaction(FACTION_ENEMY);
						thalen->RemoveAllAuras();
						thalen->CastSpell(thalen, SPELL_BLAZING_BARRIER);

						if (Creature* trigger = thalen->SummonCreature(WORLD_TRIGGER, ExplodingPoint01, TEMPSUMMON_TIMED_DESPAWN, 5s))
							trigger->CastSpell(trigger, SPELL_METEOR);
					}
					Next(1s);
					break;
				case EVT_BATTLE_BARRIER_BREAKS:
					if (GameObject* barrier = GetBarrier01())
					{
						scheduler.CancelGroup((uint32)BFTPhases::TheBattle);
						barrier->ResetDoorOrButton();
						if (Creature* trigger = barrier->SummonCreature(WORLD_TRIGGER, ExplodingPoint01, TEMPSUMMON_TIMED_DESPAWN, 5s))
						{
							trigger->SetFaction(FACTION_MONSTER);
							trigger->CastSpell(trigger, SPELL_BIG_EXPLOSION);
							trigger->CastSpell(trigger, SPELL_SCORCHED_EARTH, true);
						}
					}
					Next(1s);
					break;
				case EVT_BATTLE_ARCHMAGES_READY:
					if (Creature* amara = GetAmara())
					{
						amara->RemoveAllAuras();
						amara->SetEmoteState(EMOTE_STATE_READY2HL_ALLOW_MOVEMENT);
						amara->CastSpell(amara, SPELL_PRISMATIC_BARRIER, true);
					}
					GetThalen()->HandleEmoteCommand(EMOTE_ONESHOT_LAUGH);
					GetJaina()->SetEmoteState(EMOTE_STATE_READY2HL_ALLOW_MOVEMENT);
					GetHedric()->SetEmoteState(EMOTE_STATE_READY1H_ALLOW_MOVEMENT);
					Next(1s);
					break;
				case EVT_BATTLE_JAINA_TALK_03:
					Talk(GetJaina(), SAY_BATTLE_03);
					if (Creature* thalen = GetThalen())
					{
						thalen->SetWalk(false);
						thalen->GetMotionMaster()->MovePoint(MOVEMENT_INFO_POINT_NONE, ThalenPoint01, false, ThalenPoint01.GetOrientation());
					}
					Next(2s);
					break;
				case EVT_BATTLE_JAINA_TELEPORT:
					if (Creature* jaina = GetJaina())
					{
						jaina->CastSpell(JainaPoint04, SPELL_TELEPORT);
						Talk(jaina, SAY_BATTLE_04);
					}
					Next(1s);
					break;
				case EVT_BATTLE_THALEN_FREEZE:
					if (Creature* thalen = GetThalen())
					{
						thalen->CastSpell(thalen, SPELL_ICY_GLARE);
						thalen->CastSpell(thalen, SPELL_CHILLING_BLAST, true);
						thalen->StopMoving();
					}
					Next(2s);
					break;
				// Thalen se dissout (fuite) et laisse Thader agonisant, que
				// Kinndy vient soigner : c'est ce tableau que les joueurs
				// trouvent en arrivant.
				case EVT_BATTLE_THADER_WOUNDED:
					if (Creature* thalen = GetThalen())
					{
						thalen->RemoveAurasDueToSpell(SPELL_BLAZING_BARRIER);
						thalen->CastSpell(thalen, SPELL_DISSOLVE);
					}
					if (Creature* thader = GetCreature(DATA_THADER_WINDERMERE))
					{
						Talk(thader, SAY_BATTLE_05);
						thader->RemoveAllAuras();
						thader->SetRegenerateHealth(false);
						thader->SetReactState(REACT_PASSIVE);
						thader->SetStandState(UNIT_STAND_STATE_KNEEL);
						thader->SetHealth(thader->CountPctFromMaxHealth(5));
						thader->CastSpell(thader, SPELL_ARCANE_FX);

						if (Creature* kinndy = GetCreature(DATA_KINNDY_SPARKSHINE))
						{
							kinndy->RemoveAllAuras();
							kinndy->SetReactState(REACT_PASSIVE);
							kinndy->CastSpell(kinndy, SPELL_CHANNEL_BLUE_MOVING);
							kinndy->GetMotionMaster()->MoveCloserAndStop(MOVEMENT_INFO_POINT_NONE, thader, 0.8f);
						}
					}
					GetBarrier02()->ResetDoorOrButton();
					Next(3s);
					break;
				// Premier debarquement et bascule en mode combat : la boucle
				// de vagues demarre avec EVENT_MAINTAIN_THE_PROTECTION.
				case EVT_BATTLE_FIRST_LANDING:
					HordeMembersInvoker(DATA_WAVE_BOAT);
					if (Creature* thalen = GetThalen())
					{
						thalen->RemoveAllAuras();
						thalen->NearTeleportTo(ThalenPoint02);
						thalen->SetHomePosition(ThalenPoint02);
						thalen->CastSpell(thalen, SPELL_ARCANIC_CELL, true);
						thalen->SetEmoteState(EMOTE_STATE_STUN_NO_SHEATHE);
					}
					GetJaina()->CastSpell(actorsRelocation[0].destination, SPELL_TELEPORT);
                    TriggerGameEvent(EVENT_MAINTAIN_THE_PROTECTION);
					break;

				#pragma endregion

				// Help the wounded (122 - 141)
				// Deux blocs de dialogue apres la bataille. La partie I
				// (122-127) se joue toujours ; la partie II (128-140) n'est
				// lancee que si les joueurs suivent Jaina, et elle est annulee
				// en bloc s'ils terminent l'etape avant la fin (voir
				// CRITERIA_TREE_HELP_THE_WOUNDED).
				#pragma region HELP_THE_WOUNDED

				// PART I - Jaina et Hedric constatent les degats
				case EVT_WOUNDED_JAINA_HEDRIC_FACE:
					if (Creature* jaina = GetJaina())
					{
						if (Creature* hedric = GetHedric())
						{
							jaina->SetTarget(hedric->GetGUID());
							jaina->SetEmoteState(EMOTE_STATE_STAND);

							hedric->SetTarget(jaina->GetGUID());
							hedric->SetEmoteState(EMOTE_STATE_STAND);
						}
					}
					Next(800ms);
					break;
				case EVT_WOUNDED_JAINA_TALK_01:
					Talk(GetJaina(), SAY_POST_BATTLE_01);
					Next(2s);
					break;
				case EVT_WOUNDED_HEDRIC_TALK_02:
					Talk(GetHedric(), SAY_POST_BATTLE_02);
					Next(4s);
					break;
				case EVT_WOUNDED_JAINA_TALK_03:
					Talk(GetJaina(), SAY_POST_BATTLE_03);
					Next(4s);
					break;
				case EVT_WOUNDED_JAINA_WALK:
					ClearTarget();
					if (Creature* jaina = GetJaina())
					{
						jaina->GetMotionMaster()->MovePath(JainaPath01, false);
						jaina->SetHomePosition(JainaPoint06);
					}
					Next(1500ms);
					break;
				case EVT_WOUNDED_HEDRIC_WALK:
					if (Creature* hedric = GetHedric())
						hedric->GetMotionMaster()->MovePath(HedricPath02, false);
					break;

				// PART II - Jaina et Kinndy, pendant que les joueurs soignent
				// les blesses (bloc annulable, voir plus haut)
				case EVT_WOUNDED_JAINA_KINNDY_FACE:
					if (Creature* jaina = GetJaina())
					{
						if (Creature* kinndy = GetKinndy())
						{
							jaina->SetTarget(kinndy->GetGUID());
							kinndy->SetTarget(jaina->GetGUID());
						}
					}
					Next(800ms);
					break;
				case EVT_WOUNDED_KINNDY_TALK_04:
					Talk(GetKinndy(), SAY_POST_BATTLE_04);
					Next(5s);
					break;
				case EVT_WOUNDED_JAINA_TALK_05:
					Talk(GetJaina(), SAY_POST_BATTLE_05);
					Next(7s);
					break;
				case EVT_WOUNDED_KINNDY_TALK_06:
					if (Creature* kinndy = GetKinndy())
					{
						Talk(kinndy, SAY_POST_BATTLE_06);
						kinndy->SetEmoteState(EMOTE_STATE_NONE);
					}
					Next(4s);
					break;
				case EVT_WOUNDED_JAINA_TALK_07:
					Talk(GetJaina(), SAY_POST_BATTLE_07);
					Next(4s);
					break;
				case EVT_WOUNDED_KINNDY_TALK_08:
					Talk(GetKinndy(), SAY_POST_BATTLE_08);
					Next(3s);
					break;
				case EVT_WOUNDED_JAINA_TALK_09:
					Talk(GetJaina(), SAY_POST_BATTLE_09);
					Next(13s);
					break;
				case EVT_WOUNDED_JAINA_TALK_10:
					Talk(GetJaina(), SAY_POST_BATTLE_10);
					Next(7s);
					break;
				case EVT_WOUNDED_KINNDY_TALK_11:
					Talk(GetKinndy(), SAY_POST_BATTLE_11);
					Next(8s);
					break;
				case EVT_WOUNDED_JAINA_TALK_12:
					Talk(GetJaina(), SAY_POST_BATTLE_12);
					Next(6s);
					break;
				case EVT_WOUNDED_JAINA_TALK_13:
					Talk(GetJaina(), SAY_POST_BATTLE_13);
					Next(12s);
					break;
				case EVT_WOUNDED_KINNDY_TALK_14:
					Talk(GetKinndy(), SAY_POST_BATTLE_14);
					Next(3s);
					break;
				case EVT_WOUNDED_JAINA_TALK_15:
					Talk(GetJaina(), SAY_POST_BATTLE_15);
					Next(3s);
					break;
				case EVT_WOUNDED_SCENE_END:
					ClearTarget();
					if (Creature* jaina = GetJaina())
					{
						jaina->SetSpeedRate(MOVE_RUN, 0.85f);

						if (Creature* kinndy = GetKinndy())
						{
							jaina->SetFacingTo(3.15f);
							kinndy->SetFacingTo(2.73f);
						}
					}
					break;

				#pragma endregion

				// Wait for Archmage Leeson returns (142 - 160)
				// Partie I (142-155) : Kalecgos previent Jaina du danger que
				// represente l'iris, puis tout le monde regagne la table de
				// banquet et Amara revient par le portail.
				// Partie II (156-160) : dialogue avec Amara, puis Jaina se
				// leve, ce qui enchaine sur la montee a la tour.
				#pragma region WAIT_FOR_AMARA

				// Part I - Avertissement de Kalecgos
				case EVT_AMARA_KALEC_APPROACH:
					GetKalecgosHuman()->GetMotionMaster()->MovePath(KalecPath02, false, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
					Next(8s);
					break;
				case EVT_AMARA_KALEC_TALK_01:
					SetTarget(GetKalecgosHuman());
					Talk(GetKalecgosHuman(), SAY_IRIS_WARN_01);
					Next(1s);
					break;
				case EVT_AMARA_JAINA_TALK_02:
					SetTarget(GetJaina());
					Talk(GetJaina(), SAY_IRIS_WARN_02);
					Next(2s);
					break;
				case EVT_AMARA_KALEC_TALK_03:
					SetTarget(GetKalecgosHuman());
					Talk(GetKalecgosHuman(), SAY_IRIS_WARN_03);
					Next(4s);
					break;
				case EVT_AMARA_JAINA_TALK_04:
					SetTarget(GetJaina());
					Talk(GetJaina(), SAY_IRIS_WARN_04);
					Next(8s);
					break;
				case EVT_AMARA_KALEC_TALK_05:
					SetTarget(GetKalecgosHuman());
					Talk(GetKalecgosHuman(), SAY_IRIS_WARN_05);
					Next(2s);
					break;
				case EVT_AMARA_JAINA_TALK_06:
					SetTarget(GetJaina());
					Talk(GetJaina(), SAY_IRIS_WARN_06);
					Next(8s);
					break;
				case EVT_AMARA_JAINA_TALK_07:
					Talk(GetJaina(), SAY_IRIS_WARN_07);
					Next(2s);
					break;
				case EVT_AMARA_KALEC_TALK_08:
					SetTarget(GetKalecgosHuman());
					Talk(GetKalecgosHuman(), SAY_IRIS_WARN_08);
					Next(7s);
					break;
				case EVT_AMARA_RHONIN_TALK_09:
					SetTarget(GetRhonin());
					Talk(GetRhonin(), SAY_IRIS_WARN_09);
					Next(5s);
					break;
				case EVT_AMARA_KALEC_LEAVE:
					ClearTarget();
					GetKalecgosHuman()->GetMotionMaster()->MovePath(KalecPath03, false, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
					Next(3s);
					break;
				case EVT_AMARA_RHONIN_LEAVE:
                    GetRhonin()->GetMotionMaster()->MovePath(RhoninPath01, false, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
					Next(3s);
					break;
				case EVT_AMARA_LEESON_PATH:
                    GetAmara()->GetMotionMaster()->MovePath(AmaraPath01, false, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
					Next(10s);
					break;
				case EVT_AMARA_LEESON_RETURN:
					if (Creature* amara = GetAmara())
					{
						amara->SetVisible(true);
						amara->GetMotionMaster()->MovePath(KalecPath02, false, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
					}
					break;

				// Part II - Retour d'Amara Leeson
				case EVT_AMARA_JAINA_FACE:
					if (Creature* jaina = GetJaina())
					{
						if (Creature* amara = GetAmara())
						{
							jaina->SetTarget(amara->GetGUID());
							amara->SetTarget(jaina->GetGUID());
						}
					}
					Next(1s);
					break;
				case EVT_AMARA_LEESON_TALK_10:
					Talk(GetAmara(), SAY_IRIS_WARN_10);
					Next(6s);
					break;
				case EVT_AMARA_JAINA_TALK_11:
					Talk(GetJaina(), SAY_IRIS_WARN_11);
					Next(4s);
					break;
				case EVT_AMARA_LEESON_LEAVE:
					ClearTarget();
					GetAmara()->GetMotionMaster()->MovePath(KalecPath03, false, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
					Next(4s);
					break;
				case EVT_AMARA_JAINA_TO_TOWER:
					GetJaina()->GetMotionMaster()->MovePath(JainaPath02, false, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
					break;

				#pragma endregion

				// Retrieve Rhonin (161 - 172)
				// Dernier acte au sommet de la tour : Jaina et Rhonin
				// comprennent que la bombe va exploser. L'event 172 leve
				// EVENT_REDUCE_IMPACT, qui bascule les joueurs dans la scene
				// finale (voir scene_theramore_explosion en bas de fichier).
				#pragma region RETRIEVE_RHONIN

				case EVT_RHONIN_JAINA_TALK_01:
					if (Creature* jaina = GetJaina())
					{
						Talk(jaina, SAY_IRIS_XPLOSION_01);
						jaina->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
					}
					Next(3s);
					break;
				case EVT_RHONIN_JAINA_FACE:
					if (Creature* jaina = GetJaina())
					{
						jaina->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
						jaina->SetFacingToObject(GetRhonin());
						jaina->RemoveAllAuras();

						if (Creature* rhonin = GetRhonin())
						{
							jaina->SetTarget(rhonin->GetGUID());

							rhonin->SetTarget(jaina->GetGUID());
							rhonin->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
						}
					}
					Next(3s);
					break;
				case EVT_RHONIN_TALK_02:
					Talk(GetRhonin(), SAY_IRIS_XPLOSION_02);
					Next(4s);
					break;
				case EVT_RHONIN_JAINA_TALK_03:
					Talk(GetJaina(), SAY_IRIS_XPLOSION_03);
					Next(6s);
					break;
				case EVT_RHONIN_TALK_04:
					Talk(GetRhonin(), SAY_IRIS_XPLOSION_04);
					Next(5s);
					break;
				case EVT_RHONIN_TALK_05:
					Talk(GetRhonin(), SAY_IRIS_XPLOSION_05);
					Next(8s);
					break;
				case EVT_RHONIN_TALK_06:
					Talk(GetRhonin(), SAY_IRIS_XPLOSION_06);
					Next(6s);
					break;
				case EVT_RHONIN_JAINA_TALK_07:
					Talk(GetJaina(), SAY_IRIS_XPLOSION_07);
					Next(6s);
					break;
				case EVT_RHONIN_TALK_08:
					Talk(GetRhonin(), SAY_IRIS_XPLOSION_08);
					Next(3s);
					break;
				case EVT_RHONIN_JAINA_TALK_09:
					Talk(GetJaina(), SAY_IRIS_XPLOSION_09);
					Next(5s);
					break;
				case EVT_RHONIN_TALK_10:
					if (Creature* rhonin = GetRhonin())
					{
						Talk(rhonin, SAY_IRIS_XPLOSION_10);
						rhonin->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
						rhonin->RemoveAllAuras();
					}
					Next(12s);
					break;
				case EVT_RHONIN_REDUCE_IMPACT:
					TriggerGameEvent(EVENT_REDUCE_IMPACT);
					break;

				#pragma endregion
			}
		}

		// =================================================================
		// Etat interne
		// =================================================================
		EventMap events;                  // Chaine des events cinematiques
		TaskScheduler scheduler;          // Taches recurrentes (auras, explosions d'ambiance)
		uint32 eventId;                   // Dernier event execute (sert a Next() pour planifier eventId + 1)
		uint32 woundedTroops;             // Blesses deja evacues par les joueurs
		uint8 archmagesIndex;             // Prochain archimage a faire sortir du portail (EVT_HELP_ARCHMAGES_ARRIVAL)
		uint8 waves;                      // Index de la prochaine vague dans Waves[]
		BFTPhases phase;                  // Phase courante du scenario

		// Listes de GUID constituees dans OnCreatureCreate, manipulees en
		// masse par les transitions de phase.
		GuidVector citizens;              // Civils cliquables pendant Evacuation
		GuidVector civilians;             // Tous les civils (masques au debut de la bataille)
		GuidVector tanks;                 // Tanks a reparer pendant ALittleHelp
		GuidVector troops;                // Troupes statiques (motivation, blesses, banquet)
		GuidVector dummyMembers;          // Figurants Horde cosmetiques, despawnes en fin de bataille
		GuidVector waveMembers;           // Combattants de la vague en cours (voir IsWaveCleared)

		// =================================================================
		// Accesseurs
		// =================================================================
		// Raccourcis de lecture sur les acteurs du scenario. Ils peuvent
		// renvoyer nullptr : les appels directs sans test (GetJaina()->...)
		// supposent que l'acteur est vivant a ce stade du scenario.
		#pragma region ACCESSORS

		// Kalecgos existe en deux exemplaires distincts : la forme humanoide
		// qui participe aux dialogues, et le dragon qui survole la bataille.
		// Les deux accesseurs sont explicites pour qu'on ne les confonde pas.
		Creature* GetJaina()            { return GetCreature(DATA_JAINA_PROUDMOORE); }
		Creature* GetKinndy()           { return GetCreature(DATA_KINNDY_SPARKSHINE); }
		Creature* GetTervosh()          { return GetCreature(DATA_ARCHMAGE_TERVOSH); }
		Creature* GetKalecgosHuman()    { return GetCreature(DATA_KALECGOS); }
		Creature* GetKalecgosDragon()   { return GetCreature(DATA_KALECGOS_DRAGON); }
		Creature* GetPained()           { return GetCreature(DATA_PAINED); }
		Creature* GetPerith()           { return GetCreature(DATA_PERITH_STORMHOOVE); }
		Creature* GetKnight()           { return GetCreature(DATA_KNIGHT_OF_THERAMORE); }
		Creature* GetHedric()           { return GetCreature(DATA_HEDRIC_EVENCANE); }
		Creature* GetRhonin()           { return GetCreature(DATA_RHONIN); }
		Creature* GetVereesa()          { return GetCreature(DATA_VEREESA_WINDRUNNER); }
		Creature* GetThalen()           { return GetCreature(DATA_THALEN_SONGWEAVER); }
		Creature* GetAmara()            { return GetCreature(DATA_AMARA_LEESON); }
		Creature* GetDrok()             { return GetCreature(DATA_CAPTAIN_DROK); }
		Creature* GetGruhta()           { return GetCreature(DATA_WAVE_CALLER_GRUHTA); }

		GameObject* GetBarrier01()      { return GetGameObject(DATA_MYSTIC_BARRIER_01); }
		GameObject* GetBarrier02()      { return GetGameObject(DATA_MYSTIC_BARRIER_02); }

		#pragma endregion

		// =================================================================
		// Utilitaires
		// =================================================================
		#pragma region UTILS

		// Rayon de dispersion et decalage vertical du teleport groupe.
		static constexpr float TELEPORT_SPREAD_RADIUS = 8.0f;
		static constexpr float TELEPORT_Z_OFFSET      = 3.0f;
		// Proportion des troupes survivantes qui laissent un blesse a evacuer.
		static constexpr uint32 WOUNDED_SPAWN_CHANCE  = 80;

		// Talk null-safe : un acteur despawne en cours de scene ne doit pas
		// faire tomber le serveur, la replique est simplement perdue.
		void Talk(Creature* creature, uint8 textId)
		{
			if (creature)
				creature->AI()->Talk(textId);
		}

		// Joueur au nom duquel crediter un criteria de scenario.
		//
		// CriteriaHandler::UpdateCriteria sort immediatement si referencePlayer
		// est nul, et ignore aussi les joueurs en mode MJ : passer le premier
		// venu suffit a perdre silencieusement tous les credits pendant un test.
		// On prefere donc un joueur hors mode MJ, avec repli sur le premier.
		Player* GetCriteriaCreditPlayer() const
		{
			Player* fallback = nullptr;
			for (MapReference const& reference : instance->GetPlayers())
			{
				Player* player = reference.GetSource();
				if (!player)
					continue;

				if (!player->IsGameMaster())
					return player;

				if (!fallback)
					fallback = player;
			}

			return fallback;
		}

		// Retourne le premier joueur encore en jeu dans l'instance, ou nullptr.
		// Centralise le pattern instance->GetPlayers().begin()->GetSource(),
		// qui n'est pas null-safe : la liste est vide des que le dernier
		// joueur a quitte l'instance.
		Player* GetFirstPlayer() const
		{
			auto const& playerList = instance->GetPlayers();
			if (playerList.empty())
				return nullptr;
			return playerList.begin()->GetSource();
		}

		// Enchaine sur l'event suivant de la chaine. C'est ce +1 qui rend
		// l'ordre numerique des case significatif.
		void Next(const Milliseconds& time)
		{
			eventId++;
			events.ScheduleEvent(eventId, time);
		}

		// Met en scene un dialogue : tous les acteurs presents se tournent
		// vers celui qui parle. Le locuteur lui-meme est exclu pour ne pas se
		// cibler soi-meme.
		void SetTarget(Unit* unit)
		{
			ObjectGuid guid = unit->GetGUID();
			for (uint8 i = 0; i < eventCreatureDataCount; i++)
			{
				if (Creature* creature = GetCreature(creatureData[i].type))
				{
					if (creature->IsTrigger())
						continue;

					if (creature->GetGUID() == guid)
						continue;

					// Hedric n'entre en scene qu'a partir de la preparation :
					// avant, il est cense etre ailleurs et ne doit pas suivre
					// les dialogues du regard.
					if (creature->GetEntry() == NPC_HEDRIC_EVENCANE
						&& phase < BFTPhases::Preparation)
					{
						continue;
					}

					creature->SetTarget(guid);
				}
			}
		}

		// Fin de dialogue : tout le monde relache sa cible.
		void ClearTarget()
		{
			for (uint8 i = 0; i < eventCreatureDataCount; i++)
			{
				if (Creature* creature = GetCreature(creatureData[i].type))
					creature->SetTarget(ObjectGuid::Empty);
			}
		}

		// Ferme un portail proprement : on supprime le GameObject et on laisse
		// un trigger jouer l'effet visuel de fermeture a sa place.
		void ClosePortal(uint32 dataId)
		{
			if (GameObject* portal = GetGameObject(dataId))
			{
				portal->Delete();

				CastSpellExtraArgs args;
				args.SetTriggerFlags(TRIGGERED_CAST_DIRECTLY);

				const Position pos = portal->GetPosition();
				if (Creature* special = portal->SummonTrigger(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), pos.GetOrientation(), 5s))
				{
					special->CastSpell(special, SPELL_CLOSE_PORTAL, args);
				}
			}
		}

		// Teleport groupe : seuls les joueurs a moins de maxDist du caster
		// suivent. La destination est tiree au hasard autour du centre et
		// remontee de quelques metres pour eviter de faire apparaitre
		// quelqu'un dans le sol.
		void TeleportPlayers(Creature* caster, Position const& center, float maxDist)
		{
			Position pos = GetRandomPosition(caster, center, TELEPORT_SPREAD_RADIUS);
			pos.m_positionZ += TELEPORT_Z_OFFSET;

			instance->DoOnPlayers([caster, maxDist, pos](Player* player)
			{
				if (player->IsWithinDist(caster, maxDist))
				{
					player->NearTeleportTo(pos);
				}
			});
		}

		// Fait apparaitre un groupe de spawn de la Horde.
		//   waveId  : identifiant du groupe (DATA_WAVE_* / DATA_DECORATION_*)
		//   dummies : true pour un groupe purement decoratif (invulnerable,
		//             en posture de combat, sans jamais attaquer) - c'est ce
		//             qui peuple l'horizon pendant la bataille
		// Seuls les groupes listes dans Waves[] sont comptabilises : ils portent
		// WaveMemberStringId, qui est ce qui les fait crediter la barre de
		// progression (voir OnUnitDeath), et ils remplacent le contenu de
		// waveMembers - seule la vague en cours retient l'enchainement, un
		// trainard d'une vague precedente ne bloque rien. Le debarquement du
		// bateau et les figurants restent hors du decompte.
		void HordeMembersInvoker(uint32 waveId, bool dummies = false)
		{
			std::list<TempSummon*> members;

			bool const scored = std::ranges::find(Waves, waveId) != std::ranges::end(Waves);
			if (scored)
				waveMembers.clear();

			instance->SummonCreatureGroup(waveId, &members);
			for (TempSummon* horde : members)
			{
                if (Unit* target = SelectNearestHostileInRange(horde))
                    horde->AI()->AttackStart(target);

				horde->SetRegenerateHealth(false);

				if (scored)
				{
					horde->SetScriptStringId(WaveMemberStringId);
					waveMembers.push_back(horde->GetGUID());
				}

				if (dummies)
				{
					horde->SetImmuneToAll(true);

					switch (horde->GetClass())
					{
						case UNIT_CLASS_PALADIN:
							horde->SetEmoteState(EMOTE_STATE_READY2H);
							break;
						case UNIT_CLASS_MAGE:
							horde->SetEmoteState(RAND(EMOTE_STATE_READY1H, EMOTE_STATE_READY2HL));
							break;
						case UNIT_CLASS_ROGUE:
							break;
						default:
							horde->SetEmoteState(EMOTE_STATE_READY1H);
							break;
					}

					enum Spells
					{
						SPELL_CHANNEL_WATER     = 237594,
						SPELL_CHANNEL_FROST     = 1271695
					};

					switch (horde->GetEntry())
					{
                        case NPC_PORTAL_TO_ORGRIMMAR:
                            horde->SetUninteractible(true);
                            break;
						case NPC_ROKNAH_LOA_SINGER:
							horde->CastSpell(horde, SPELL_CHANNEL_WATER);
							break;
						case NPC_ROKNAH_HAG:
							horde->CastSpell(horde, SPELL_CHANNEL_FROST);
							break;
						case NPC_HORDE_BOMBARDIER:
							horde->SetWalk(false);
							horde->SetCanFly(true);
							horde->SetDisableGravity(true);
							horde->GetMotionMaster()->MoveRandom(20.0f);
							horde->SetSpeedRate(MOVE_RUN, 2.f);
							horde->SetSpeedRate(MOVE_FLIGHT, 2.f);
							horde->m_Events.AddEvent(new HordeBombardierThrowBomb(horde),
                                                     horde->m_Events.CalculateTime(Seconds(urand(2, 8))));
							break;
                        case NPC_HORDE_DEMOLISHER:
                            if (waveId == DATA_DECORATION_WEST)
                                horde->m_Events.AddEvent(new HordeDemolisherThrowBoulder(horde),
                                                        horde->m_Events.CalculateTime(Seconds(urand(2, 8))));
                            break;
                    }

					dummyMembers.push_back(horde->GetGUID());
				}
			}

			// Jaina annonce vocalement le point d'arrivee de la vague.
			if (Creature* jaina = GetJaina())
				jaina->AI()->DoAction(waveId);

			// Chaque vague doit compter HORDE_WAVE_SIZE membres : c'est ce qui
			// fait tomber le total sur l'Amount du noeud ProgressBar. Un groupe
			// de spawn incomplet plafonnerait la barre sous les 100 %.
			if (scored && waveMembers.size() != HORDE_WAVE_SIZE)
				TC_LOG_ERROR("scripts", "BFT: le groupe de spawn {} a fait apparaitre {} membres au lieu de {}.",
					waveId, waveMembers.size(), HORDE_WAVE_SIZE);
		}

		// La vague en cours est nettoyee quand plus aucun de ses membres n'est
		// vivant.
		//
		// Une creature introuvable est lachee elle aussi : elle ne peut plus
		// etre tuee, la retenir bloquerait la bataille pour toujours. Mais elle
		// n'est jamais passee par OnUnitDeath, donc elle n'a rien credite : ce
		// cas est le seul par lequel la barre peut perdre un point, on le trace.
		bool IsWaveCleared()
		{
			std::erase_if(waveMembers, [this](ObjectGuid const& guid)
			{
				Creature* member = instance->GetCreature(guid);
				if (!member)
				{
					TC_LOG_ERROR("scripts", "BFT: membre de vague {} disparu sans mourir, credit perdu.", guid.ToString());
					return true;
				}

				return !member->IsAlive();
			});

			return waveMembers.empty();
		}

		// Envoie la vague suivante.
		void NextWave()
		{
			if (waves >= HORDE_WAVES_COUNT)
				return;

			HordeMembersInvoker(Waves[waves]);
			waves++;
		}

		// Dixieme vague nettoyee : la bataille est finie, quoi qu'affiche la
		// barre. HORDE_WAVES_COUNT * HORDE_WAVE_SIZE doit tomber exactement sur
		// l'Amount du noeud ProgressBar, donc un ecart ici est un bug, pas une
		// tolerance : on le comble pour ne pas bloquer les joueurs, mais on le
		// signale. Les TC_LOG_ERROR de HordeMembersInvoker et IsWaveCleared
		// disent lequel des deux cas s'est produit.
		void CompleteWaves()
		{
			InstanceScenario* scenario = instance->GetInstanceScenario();
			if (!scenario)
				return;

			CriteriaTree const* tree = sCriteriaMgr->GetCriteriaTree(CRITERIA_TREE_SURVIVE_WAVES);
			if (!tree)
				return;

			uint64 const progress = scenario->GetCriteriaProgressCounter(CRITERIA_SURVIVE_WAVES);
			if (progress >= tree->Entry->Amount)
				return;

			TC_LOG_ERROR("scripts", "BFT: les {} vagues sont nettoyees mais la barre est a {}/{}. {} credits ont fuite.",
				HORDE_WAVES_COUNT, progress, tree->Entry->Amount, tree->Entry->Amount - progress);

			// miscValue2 est l'increment applique au criteria : un seul appel
			// suffit pour combler l'ecart.
			scenario->UpdateCriteria(CriteriaType::KillCreature, NPC_WAVE_MEMBER_CREDIT,
				tree->Entry->Amount - progress, 0);
		}

		// Supprime toutes les creatures d'une entry donnee sur la grille
		// (utilise pour nettoyer les credits d'incendie en fin d'etape).
		void MassDespawn(uint32 entry)
		{
			std::list<Creature*> results;
			if (Creature* jaina = GetJaina())
			{
				jaina->GetCreatureListWithEntryInGrid(results, entry, SIZE_OF_GRIDS);
				if (results.empty())
					return;

				for (Creature* c : results)
					c->DespawnOrUnsummon();
			}
		}

		// Retire les figurants Horde cosmetiques une fois la bataille finie.
		void DespawnDummies()
		{
			for (ObjectGuid guid : dummyMembers)
			{
				if (Creature* creature = instance->GetCreature(guid))
					creature->DespawnOrUnsummon();
			}
		}

		// Prepare l'etape HelpTheWounded : une bonne partie des troupes
		// survivantes est remplacee par un clone "blesse" a leur place. Le
		// clone reprend l'apparence et les ressources de l'original, mais avec
		// tres peu de PV et les auras cosmetiques d'agonie ; l'original est
		// simplement masque et reapparaitra au banquet.
		void SpawnWoundedTroops()
		{
			Creature* jaina = GetJaina();
			if (!jaina)
				return;

			for (ObjectGuid guid : troops)
			{
				Creature* troop = ObjectAccessor::GetCreature(*jaina, guid);

				if (!troop || troop->isDead())
					continue;

				if (roll_chance(WOUNDED_SPAWN_CHANCE))
				{
					troop->SetVisible(false);
					if (Creature* wounded = troop->SummonCreature(NPC_THERAMORE_WOUNDED_TROOP,
                                                                  troop->GetPosition(),
                                                                  TempSummonType::TEMPSUMMON_MANUAL_DESPAWN))
					{
						uint32 health = troop->GetMaxHealth();
						Powers power = troop->GetPowerType();

						wounded->SetPowerType(power);
						wounded->SetPower(power, troop->GetPower(power));
						wounded->SetRegenerateHealth(false);
						wounded->SetMaxHealth(health);
						wounded->SetHealth(health * frand(0.15f, 0.20f));
						wounded->SetDisplayId(troop->GetDisplayId());
						wounded->SetImmuneToNPC(true);
                        wounded->AddAura(SPELL_COSMETIC_DEATH, wounded);
                        wounded->AddAura(SPELL_COSMETIC_FREEZE, wounded);
						wounded->SetVignette(VIGNETTE_ALLIANCE_TROOPS);
					}
				}
			}
		}

		// Bascule de la ville en mode "apres-bataille" : on sort tout le monde
		// du combat, on ferme le portail de la Horde, et on reinstalle les
		// survivants autour de la table de banquet (troupes qui mangent,
		// archimages a leur place, Thader fige, Kinndy en pleurs).
		void RelocateTroops()
		{
			SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, GetRhonin());

			if (Creature* jaina = GetJaina())
			{
				SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, jaina);
				jaina->CombatStop();
				jaina->SetReactState(REACT_PASSIVE);
				jaina->NearTeleportTo(JainaPoint03);
				jaina->SetHomePosition(JainaPoint03);
				jaina->SetSheath(SHEATH_STATE_UNARMED);
			}

			if (GameObject* portal = GetGameObject(DATA_PORTAL_TO_ORGRIMMAR))
				portal->Delete();

			if (Creature* kalecgos = GetKalecgosDragon())
			{
				SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, kalecgos);
				kalecgos->SetVisible(false);
			}

			uint8 slot = 0;
			for (ObjectGuid const& guid : troops)
			{
				if (slot >= ARCHMAGES_RELOCATION)
					break;

				Creature* creature = instance->GetCreature(guid);

				if (!creature || creature->isDead())
					continue;

				creature->SetVisible(true);
				creature->NearTeleportTo(UnitLocation[slot]);
				creature->SetHomePosition(UnitLocation[slot]);
				creature->SetSheath(SHEATH_STATE_UNARMED);
				creature->SetStandState(UNIT_STAND_STATE_SIT);
				creature->SetEmoteState(EMOTE_STATE_NONE);
				creature->RemoveAllAuras();
				creature->Dismount();
				creature->AddAura(RAND(SPELL_COSMETIC_EAT_SOUP, SPELL_COSMETIC_DRINK), creature);

				slot++;
			}

			for (uint8 i = 0; i < ARCHMAGES_RELOCATION; i++)
			{
				if (Creature* creature = GetCreature(archmagesRelocation[i].dataId))
				{
					creature->NearTeleportTo(archmagesRelocation[i].destination);
					creature->SetHomePosition(archmagesRelocation[i].destination);
					creature->SetSheath(SHEATH_STATE_UNARMED);
					creature->SetEmoteState(EMOTE_STATE_NONE);
					creature->RemoveAllAuras();

					switch (creature->GetEntry())
					{
						case NPC_ARCHMAGE_TERVOSH:
							creature->CastSpell(creature, SPELL_SHOW_OFF_FIRE);
							break;
						case NPC_THADER_WINDERMERE:
							creature->AddAura(SPELL_STASIS, creature);
							creature->SetStandState(UNIT_STAND_STATE_STAND);
							creature->SetEmoteState(EMOTE_STATE_STUN_NO_SHEATHE);
							break;
						case NPC_TARI_COGG:
							creature->SetStandState(UNIT_STAND_STATE_SIT);
							creature->SetEmoteState(EMOTE_STATE_EAT);
							creature->SummonGameObject(GOB_LAVISH_REFRESHMENT_TABLE, TablePoint01, QuaternionData::fromEulerAnglesZYX(TablePoint01.GetOrientation(), 0.f, 0.f), 0s);
							break;
						case NPC_KINNDY_SPARKSHINE:
							creature->SetEmoteState(EMOTE_STATE_CRY);
							break;
					}
				}
			}
		}

		// Secousses de camera pendant le siege. Contrairement aux auras de
		// phase, c'est un effet reellement periodique : il doit etre rejoue
		// regulierement, il ne s'agit pas de maintenir un etat.
		void ScheduleCameraShakes()
		{
			scheduler.Schedule(1s, [this](TaskContext shake)
			{
				if (phase >= BFTPhases::Preparation && phase < BFTPhases::HelpTheWounded)
				{
					DoCastSpellOnPlayers(SPELL_CAMERA_SHAKE_VOLCANO);
				}

				shake.Repeat(15s, 30s);
			});
		}

		// Explosions d'ambiance sur la barriere pendant toute la bataille.
		// Taggee (uint32)BFTPhases::TheBattle pour pouvoir etre annulee en bloc
		// quand la barriere cede (EVT_BATTLE_BARRIER_BREAKS).
		void EnsureBarrierHasDamage()
		{
			scheduler.Schedule(1s, (uint32)BFTPhases::TheBattle, [this](TaskContext explosion)
			{
				if (Creature* thalen = GetThalen())
				{
					if (Creature* trigger = thalen->SummonCreature(WORLD_TRIGGER, ExplodingPoint01, TEMPSUMMON_TIMED_DESPAWN, 2s))
						trigger->CastSpell(trigger, SPELL_BIG_EXPLOSION);
					explosion.Repeat(2s, 5s);
				}
			});
		}

		// Force une meteo cote client, ou rend la main a la meteo de zone.
		void ForceWeather(uint32 weatherEntry, bool apply)
		{
			instance->DoOnPlayers([weatherEntry, apply](Player* player)
			{
				if (apply)
					player->SendDirectMessage(WorldPackets::Misc::Weather(WeatherState(weatherEntry), 1.0f).Write());
				else
					player->GetMap()->SendZoneWeather(player->GetZoneId(), player);
			});
		}

		// Cible d'ouverture d'une horde qui vient de spawner : sans ca elle
		// resterait plantee a son point d'arrivee.
		Unit* SelectNearestHostileInRange(Creature* creature) const
		{
			Unit* target = nullptr;
			Trinity::NearestHostileUnitInAggroRangeCheck check(creature, false, true);
			Trinity::UnitSearcher<Trinity::NearestHostileUnitInAggroRangeCheck> searcher(creature, target, check);
			Cell::VisitGridObjects(creature, searcher, MAX_VISIBILITY_DISTANCE);
			return target;
		}

		#pragma endregion
	};

	InstanceScript* GetInstanceScript(InstanceMap* map) const override
	{
		return new scenario_battle_for_theramore_InstanceScript(map);
	}
};

// =========================================================================
// scene_theramore_explosion - Scene finale (destruction de Theramore)
// =========================================================================
// Cinematique jouee cote client. Le serveur n'intervient que sur trois
// points : il fige le joueur pendant la scene, il fait apparaitre le modele
// de la bombe au moment ou la scene le demande ("DropBombServer"), et il
// teleporte le joueur vers les Ruines de Theramore a la fin - que la scene
// se termine normalement ou qu'elle soit annulee.
class scene_theramore_explosion : public SceneScript
{
	public:
		scene_theramore_explosion() : SceneScript("scene_theramore_explosion") { }

	enum Misc
	{
		MAP_THERAMORE_RUINS     = 5001,
	};

	// Centre de la zone d'arrivee dans les Ruines de Theramore.
	const Position Center = { -3002.74f, -4342.11f, 6.044930f, 3.76716f };

	// Rayon de dispersion des joueurs a l'arrivee.
	const float Distance = 8.f;

	void OnSceneComplete(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/) override
	{
		Finish(player);
	}

	void OnSceneCancel(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/) override
	{
		Finish(player);
	}

	void Finish(Player* player)
	{
		player->TeleportTo(GetRevivePosition(player), TELE_REVIVE_AT_TELEPORT);
	}

	// Point de reapparition apres la scene : tirage uniforme dans un disque de
	// rayon Distance autour de Center. Nom distinct du helper libre
	// GetRandomPosition (CustomAI.h) auquel il delegue, pour qu'on ne confonde
	// pas les deux a la lecture.
	WorldLocation GetRevivePosition(Player const* player)
	{
		Position dest = GetRandomPosition(player, Center, Distance);
		dest.SetOrientation(Center.GetOrientation());
		return { MAP_THERAMORE_RUINS, dest };
	}
};

// =========================================================================
// Registration
// =========================================================================
void AddSC_scenario_battle_for_theramore()
{
	new scenario_battle_for_theramore();
	new scene_theramore_explosion();
}
