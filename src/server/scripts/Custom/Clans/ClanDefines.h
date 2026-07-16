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

// Module "Clans" : PNJ autonomes a besoins + apprentissage (Q-learning).
// Ce header regroupe les enums et les constantes d'equilibrage partagees
// par tout le module. Les identifiants sont en anglais ; les commentaires
// restent en francais comme le reste du dossier Custom/.

#ifndef CUSTOM_CLANS_CLANDEFINES_H
#define CUSTOM_CLANS_CLANDEFINES_H

#include "Define.h"

namespace Clan
{
	// Genre d'un membre (utilise pour la reproduction).
	enum class Gender : uint8
	{
		Male   = 0,
		Female = 1
	};

	// Appartenance a un clan. La reproduction n'a lieu qu'entre clans differents.
	enum class ClanId : uint8
	{
		None  = 0,
		ClanA = 1,
		ClanB = 2
	};

	// Etape de vie, pilotee par le vieillissement.
	enum class LifeStage : uint8
	{
		Child = 0,
		Adult = 1,
		Elder = 2
	};

	// Besoins physiologiques suivis par chaque membre.
	// Convention : 0 = besoin satisfait, 100 = besoin critique (il croit avec le temps).
	enum class NeedType : uint8
	{
		None   = 0,
		Hunger = 1,
		Thirst = 2,
		Energy = 3, // fatigue / sommeil
		Repro  = 4,
		Count  = 5
	};

	// Actions apprises par le cerveau. La valeur numerique sert d'index dans la Q-table.
	enum class ActionType : uint8
	{
		Idle            = 0,
		Wander          = 1,
		Hunt            = 2,  // chasser une proie reelle -> viande crue
		DrinkRiver      = 3, 
		DrinkWell       = 4, 
		Sleep           = 5, 
		SeekMate        = 6, 
		GatherWood      = 7,  // ramasser du bois -> pour rallumer un feu
		MineRock        = 8,  // miner une roche -> pierre/etincelle pour rallumer
		LightFire       = 9,  // rallumer un feu eteint (consomme bois + pierre)
		Cook            = 10, // cuire la viande crue sur un feu allume -> rassasie
		SeekDoctor      = 11, // aller se faire soigner d'une maladie par le medecin
		HuntPredator    = 12, // traquer un animal sauvage (predateur) pour l'exterminer
		Count           = 13
	};

	// Types de ressources declarees dans la table custom_clan_resource.
	enum class ResourceType : uint8
	{
		None        = 0,
		Prey        = 1, // gibier (Creature attaquable)
		WaterRiver  = 2, // point d'eau "riviere" (GameObject)
		WaterWell   = 3, // puits (GameObject)
		Wood        = 4, // bois a ramasser (GameObject, epuisable)
		Bed         = 5, // lit / campement pour dormir (GameObject)
		FireIndoor  = 6, // feu de cheminee (GameObject, jamais eteint par la pluie)
		FireOutdoor = 7, // feu exterieur (GameObject, eteint par la pluie)
		Rock        = 8, // roche a miner (GameObject, epuisable)
		Doctor      = 9, // medecin (Creature neutre, hors clan) qui soigne les maladies
		Predator    = 10, // animal sauvage (Creature hostile) a exterminer
		Count       = 11
	};

	// Nature de l'objet declare dans le registre de ressources.
	enum class ObjectKind : uint8
	{
		Creature   = 0,
		GameObject = 1
	};

	// Type d'affliction (colonne "type" de custom_clan_disease).
	//   Ambiance -> Disease ; morsure/griffe d'animal sauvage -> Bleed/Poison.
	//   Le medecin soigne toutes les afflictions quel que soit le type.
	enum class AfflictionType : uint8
	{
		Disease = 0, // maladie (contagion ambiante)
		Poison  = 1, // poison
		Bleed   = 2, // saignement (attaque animale)
		Count   = 3
	};

	// ---------------------------------------------------------------------
	// Dimensions de la Q-table
	// ---------------------------------------------------------------------
	// Etat discret = (besoin le plus urgent) x (jour/nuit) x 6 bits de contexte :
	// hasRawFood, hasWood, hasStone, litFireNearby, diseased, predatorNearby.
	constexpr uint8  ACTION_COUNT     = uint8(ActionType::Count);   // 13
	constexpr uint8  NEED_STATE_COUNT = uint8(NeedType::Count);     // 5 (None..Repro)
	constexpr uint8  TIME_STATE_COUNT = 2;                          // jour / nuit
	constexpr uint8  FLAG_STATE_COUNT = 2 * 2 * 2 * 2 * 2 * 2;     // 64 (6 booleens de contexte)
	constexpr uint16 STATE_COUNT      = uint16(NEED_STATE_COUNT * TIME_STATE_COUNT * FLAG_STATE_COUNT); // 5*2*64 = 640

	// ---------------------------------------------------------------------
	// Parametres d'apprentissage (Q-learning) - tous ajustables
	// ---------------------------------------------------------------------
	constexpr float Q_ALPHA         = 0.20f; // taux d'apprentissage
	constexpr float Q_GAMMA         = 0.85f; // facteur d'actualisation
	constexpr float Q_EPSILON_START = 0.20f; // exploration initiale (basse : instincts amorces, on affine)
	constexpr float Q_EPSILON_MIN   = 0.05f; // exploration residuelle
	constexpr float Q_EPSILON_DECAY = 0.995f; // decroissance par pas d'apprentissage
	// Melange des Q-tables parentales a la naissance (part du parent A).
	constexpr float Q_INHERIT_MIX   = 0.50f;
	constexpr float Q_INHERIT_NOISE = 0.10f; // bruit d'exploration ajoute a l'heritage
	// Valeur a priori donnee a la "bonne" action de chaque etat (instinct de depart).
	// Evite le cold-start ou les PNJ meurent avant d'avoir appris. Ils affinent ensuite.
	constexpr float Q_SEED_PRIOR    = 0.50f;

	// ---------------------------------------------------------------------
	// Modele de besoins (points par seconde reelle) - ajustables
	// ---------------------------------------------------------------------
	constexpr float NEED_MAX             = 100.0f;
	constexpr float HUNGER_RATE          = 0.20f;
	constexpr float THIRST_RATE          = 0.30f;
	constexpr float ENERGY_RATE_DAY      = 0.10f;
	constexpr float ENERGY_RATE_NIGHT    = 0.35f;
	constexpr float REPRO_RATE           = 0.05f;

	// Un besoin est considere "urgent" au-dela de ce seuil.
	constexpr float NEED_URGENT_THRESHOLD = 55.0f;

	// Famine : au-dela de ce seuil de faim, le membre perd des PV a intervalle regulier
	// (et finit par mourir de faim s'il ne mange pas). Regen desactivee tant qu'il a faim.
	constexpr float  HUNGER_STARVE_THRESHOLD = 85.0f;
	constexpr uint32 STARVE_TICK_MS          = 5000;  // frequence des degats de faim
	constexpr float  STARVE_DAMAGE_PCT       = 2.0f;  // % des PV max perdus par tick

	// Maladie / poison / saignement : le membre peut en contracter, et apprend (Q-learning)
	// a aller se faire soigner par un medecin (PNJ neutre) quand il est afflige.
	constexpr uint32 DISEASE_TICK_MS       = 30000;  // frequence du tirage de contagion ambiante
	constexpr float  DISEASE_CHANCE        = 15.0f;  // % de contracter une maladie (type Disease) par tirage
	constexpr float  DISEASE_CHANCE_COOK   = 8.0f;   // % de contracter une maladie (type Disease) par tirage
	constexpr float  DISEASE_CHANCE_DRINK  = 5.0f;   // % de contracter une maladie (type Disease) par tirage
	constexpr float  DISEASE_CHANCE_PRED   = 15.0f;  // % d'infliger un saignement quand un animal attaque

	// Recompense negative appliquee quand une action echoue / perd du temps.
	constexpr float REWARD_FAIL              = -0.5f;
	// Petite penalite de temps par pas de decision (encourage l'efficacite).
	constexpr float REWARD_TIME_PENALTY      = -0.05f;

	// Reward shaping : chaque etape productive de la chaine alimentaire / du feu donne
	// un gain immediat, sinon l'apprentissage (recompense finale unique) ne convergerait pas.
	constexpr float REWARD_RAWFOOD = 0.40f; // proie tuee -> viande crue
	constexpr float REWARD_WOOD    = 0.30f; // bois ramasse
	constexpr float REWARD_STONE   = 0.30f; // roche minee
	constexpr float REWARD_LIGHT   = 0.60f; // feu rallume
	constexpr float REWARD_COOK    = 1.00f; // plat cuit -> faim rassasiee
	constexpr float REWARD_CURE    = 1.00f; // maladie soignee par le medecin
	constexpr float REWARD_KILL_PREDATOR = 0.80f; // predateur extermine

	// Apprentissage du combat (choix defendre / fuir des adultes) - separe de la Q-table.
	constexpr float REWARD_DEFEND_WIN = 1.00f;  // avoir tue l'agresseur en se defendant
	constexpr float REWARD_DEFEND_HURT = -0.80f; // s'etre defendu mais fini a faible vie
	constexpr float REWARD_FLEE_SAFE  = 0.20f;  // avoir fui et atteint un lieu sur
	constexpr float DEFEND_HURT_HP_PCT = 25.0f; // en-dessous de ce % de PV, la defense est "ratee"
	constexpr float COMBAT_EXPLORE     = 0.10f; // exploration du choix combattre/fuir

	// ---------------------------------------------------------------------
	// Perception / execution
	// ---------------------------------------------------------------------
	constexpr float  RESOURCE_SEARCH_RANGE = 100.0f;    // rayon de recherche des ressources
	constexpr float  INTERACT_RANGE        = 3.0f;      // distance d'interaction (boire, etc.)
	constexpr uint32 DECISION_INTERVAL_MS  = 3000;      // frequence du tick de decision
	constexpr uint32 INTERACT_DURATION_MS  = 4000;      // duree d'une interaction (boire/dormir)
	constexpr uint32 HUNT_TIMEOUT_MS       = 20000;     // abandon d'une chasse trop longue
	constexpr uint32 WOOD_DURATION_MS      = 3000;      // duree de coupage du bois
	constexpr uint32 STONE_DURATION_MS     = 3000;      // duree de minage
	constexpr uint32 COOK_DURATION_MS      = 10000;     // duree de cuisson sur un feu
	constexpr uint32 SLEEP_DURATION_MS     = 10000;     // duree de sommeil
	constexpr uint32 MATE_DURATION_MS      = 3000;      // duree de rencontre
	constexpr uint32 DOCTOR_DURATION_MS    = 2800;      // duree pour le docteur
	constexpr Milliseconds WANDER_DURATION_MS = 3500ms; // duree de marche / decouverte

	// Feux et noeuds de ressource.
	constexpr uint32 FIRE_BURN_DURATION_MS  = 60000;  // combustion avant extinction (1 min)
	constexpr uint32 RAIN_CHECK_INTERVAL_MS = 10000;  // test de pluie sur les feux exterieurs
	constexpr uint32 WOOD_RESPAWN_MS        = 30000;  // respawn d'un noeud de bois epuise
	constexpr uint32 ROCK_RESPAWN_MS        = 40000;  // respawn d'un noeud de roche epuise
	// Duree pendant laquelle un noeud cible par un membre est "reserve" (les autres
	// l'ignorent). Expire d'elle-meme si le membre n'y arrive jamais (mort, fuite...).
	constexpr uint32 NODE_CLAIM_TTL_MS      = 15000;

	// Etat visuel (GOState) d'un GameObject de feu allume / eteint.
	// Valeurs = GOState : 0 = GO_STATE_ACTIVE, 1 = GO_STATE_READY.
	// Si TON feu s'affiche a l'envers (allume alors qu'il devrait etre eteint), inverse ces deux valeurs.
	constexpr uint8 FIRE_GOSTATE_LIT = 1;
	constexpr uint8 FIRE_GOSTATE_OUT = 0;

	// Fuite des enfants / anciens face a un predateur.
	constexpr float  FLEE_DISTANCE          = 30.0f;

	// Delai minimum entre deux phrases prononcees par un membre (anti-spam).
	constexpr uint32 TALK_COOLDOWN_MS       = 8000;

	// ---------------------------------------------------------------------
	// Cycle jour / nuit (heure de jeu WoW, 0..23)
	// ---------------------------------------------------------------------
	constexpr uint8 DAY_START_HOUR   = 6;
	constexpr uint8 NIGHT_START_HOUR = 20;

	inline bool IsNight(uint8 hour)
	{
		return hour < DAY_START_HOUR || hour >= NIGHT_START_HOUR;
	}

	// ---------------------------------------------------------------------
	// Vieillissement / reproduction (echelle de temps simulee) - ajustables
	// ---------------------------------------------------------------------
	// Duree reelle d'un "jour" simule. 60s => 1 minute reelle = 1 jour de vie.
	constexpr uint32 REAL_SECONDS_PER_SIM_DAY = 10;
	constexpr uint32 AGE_CHILD_TO_ADULT_DAYS  = 10;
	constexpr uint32 AGE_ADULT_TO_ELDER_DAYS  = 30;
	constexpr uint32 AGE_DEATH_DAYS           = 50;
	constexpr uint32 REPRO_COOLDOWN_DAYS      = 10;
	// Un adulte doit etre suffisamment rassasie pour se reproduire.
	constexpr float  REPRO_READY_MAX_NEED     = 40.0f;
	// Echelle du modele pour un enfant.
	constexpr float  CHILD_SCALE              = 1.0f;
	// Tombes aléatoires
	constexpr uint8  GRAVESTONE_COUNT         = 3;
	constexpr uint32 GRAVESTONES[GRAVESTONE_COUNT]
        = { 2000007, 2000008, 2000009 };
	// Spot pour signaler les tombes
	constexpr uint32 GRAVESTONE_SPOT          = 1239999;
    constexpr uint32 ASHES_DISPLAY_ID         = 38563;

	// Intervalle de sauvegarde periodique de l'etat (ms).
	constexpr uint32 SAVE_INTERVAL_MS = 2 * 60 * 1000;

	// Identifiants MovementInform emis par l'IA (evite les collisions avec CustomAI).
	enum ClanMovePointId : uint32
	{
		MOVE_TO_RESOURCE   = 5300000, // point d'eau (boire)
		MOVE_TO_MATE       = 5300001,
		MOVE_TO_HOME       = 5300002, // lit / maison (dormir)
		MOVE_TO_WOOD       = 5300003,
		MOVE_TO_ROCK       = 5300004,
		MOVE_TO_FIRE_LIGHT = 5300005, // feu eteint a rallumer
		MOVE_TO_FIRE_COOK  = 5300006, // feu allume pour cuire
		MOVE_TO_FLEE       = 5300007, // point de fuite (enfants/anciens)
		MOVE_TO_DOCTOR     = 5300008  // medecin (pour se faire soigner)
	};

	// Nom de script attache aux gabarits des membres (creature_template.ScriptName).
	constexpr char const* MEMBER_SCRIPT_NAME = "npc_clan_member";

	// Sort du docteur
	constexpr uint32 const SPELL_HEAL_DOCTOR = 29170;
}

#endif // CUSTOM_CLANS_CLANDEFINES_H
