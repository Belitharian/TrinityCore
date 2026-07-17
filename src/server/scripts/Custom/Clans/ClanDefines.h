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

	// Appartenance a un clan. La reproduction est autorisee au sein d'un meme clan
	// comme entre clans differents (seuls le genre oppose et l'absence de lien
	// parent-enfant sont requis).
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
		Remember        = 13, // se recueillir sur la tombe d'un ancetre (tradition) -> recompense
		Count           = 14
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
		Fire        = 6, // feu (GameObject) : se consume avec le temps -> a rallumer
		// 7 : ancien "feu exterieur" (supprime avec la mecanique de pluie). Valeur laissee
		//     volontairement libre pour ne PAS renumeroter les types suivants, deja
		//     utilises dans custom_clan_resource.
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

	// Ressources transportables par un membre (inventaire, quantites).
	enum class ItemType : uint8
	{
		RawFood = 0, // viande crue (a cuire)
		Wood    = 1, // bois (rallumage)
		Stone   = 2, // pierre / silex (rallumage)
		Count   = 3
	};

	// Capacite de portage par type de ressource : au-dela, on ne ramasse plus.
	// (L'etat percu par la Q-table reste booleen "en possede / n'en possede pas" : la
	//  quantite ne fait donc pas exploser le nombre d'etats.)
	constexpr uint32 INVENTORY_MAX_PER_ITEM = 5;

	// Cause de la mort, gravee sur la tombe et lisible en cliquant dessus (gossip).
	enum class DeathCause : uint8
	{
		Unknown    = 0,
		Starvation = 1, // mort de faim
		Disease    = 2, // emporte par une affliction
		Predator   = 3, // tue par un animal sauvage
		OldAge     = 4, // mort de vieillesse
		Count      = 5
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
	// Etat discret = (besoin le plus urgent) x (jour/nuit) x 7 bits de contexte :
	// hasRawFood, hasWood, hasStone, litFireNearby, diseased, predatorNearby, unlitFireNearby.
	constexpr uint8  ACTION_COUNT     = uint8(ActionType::Count);   // 14
	constexpr uint8  NEED_STATE_COUNT = uint8(NeedType::Count);     // 5 (None..Repro)
	constexpr uint8  TIME_STATE_COUNT = 2;                          // jour / nuit
	constexpr uint16 FLAG_STATE_COUNT = 2 * 2 * 2 * 2 * 2 * 2 * 2; // 128 (7 booleens de contexte)
	constexpr uint16 STATE_COUNT      = uint16(NEED_STATE_COUNT * TIME_STATE_COUNT * FLAG_STATE_COUNT); // 5*2*128 = 1280

	// ---------------------------------------------------------------------
	// Parametres d'apprentissage (Q-learning) - tous ajustables
	// ---------------------------------------------------------------------
	constexpr float Q_ALPHA         = 0.20f; // taux d'apprentissage
	constexpr float Q_GAMMA         = 0.85f; // facteur d'actualisation
	// Exploration (epsilon-greedy) : % d'actions tirees au hasard plutot que "la meilleure".
	// Trop d'exploration = PNJ erratiques ; trop peu = ils ne decouvrent jamais les actions
	// non amorcees (ex. Remember). epsilon decroit a CHAQUE pas d'apprentissage.
	constexpr float Q_EPSILON_START = 0.15f;  // exploration initiale
	constexpr float Q_EPSILON_MIN   = 0.02f;  // exploration residuelle (une fois "adulte")
	constexpr float Q_EPSILON_DECAY = 0.985f; // ~130 pas pour passer de START a MIN
	// Melange des Q-tables parentales a la naissance (part du parent A).
	constexpr float Q_INHERIT_MIX   = 0.50f;
	constexpr float Q_INHERIT_NOISE = 0.10f; // bruit d'exploration ajoute a l'heritage
	// Valeur a priori donnee a la "bonne" action de chaque etat (instinct de depart).
	// Evite le cold-start ou les PNJ meurent avant d'avoir appris. Ils affinent ensuite.
	constexpr float Q_SEED_PRIOR    = 0.50f;

	// ---------------------------------------------------------------------
	// Modele de besoins (points par JOUR SIMULE) - ajustables
	// ---------------------------------------------------------------------
	// Les besoins evoluent au rythme du temps simule, PAS du temps reel : leur vitesse est
	// exprimee en points gagnes par jour simule et convertie via REAL_SECONDS_PER_SIM_DAY
	// (voir Needs::Decay). Changer la duree d'un jour accelere/ralentit ainsi besoins ET
	// vieillissement de concert (avant, un jour raccourci laissait les besoins a la traine).
	constexpr float NEED_MAX             = 100.0f;
	constexpr float HUNGER_RATE          = 12.0f; // ~4.6 jours pour devenir urgent (55), ~8 pour saturer
	constexpr float THIRST_RATE          = 18.0f; // la soif monte plus vite que la faim
	constexpr float ENERGY_RATE_DAY      = 6.0f;  // fatigue accumulee en journee
	constexpr float ENERGY_RATE_NIGHT    = 21.0f; // fatigue accumulee la nuit (pousse a dormir)
	constexpr float REPRO_RATE           = 3.0f;  // le desir de reproduction monte lentement

	// Un besoin est considere "urgent" au-dela de ce seuil.
	constexpr float NEED_URGENT_THRESHOLD = 55.0f;

	// Survie : en-dessous de ce % de PV, se nourrir devient LA priorite (le besoin percu
	// bascule sur Hunger, quel que soit le besoin reellement le plus eleve). Les PV sont
	// rendus par le sort declare pour la cuisson (custom_clan_action_fx, action 10).
	constexpr float HEALTH_LOW_PCT = 40.0f;

	// Degats de survie : famine ET maladie rongent les PV au meme tick (et peuvent tuer).
	// Regen desactivee tant que le membre a faim.
	constexpr float  HUNGER_STARVE_THRESHOLD = 70.0f;
	constexpr uint32 STARVE_TICK_MS          = 3000;  // frequence des degats de survie (faim + maladie)
	constexpr float  STARVE_DAMAGE_PCT       = 1.0f;  // % des PV max perdus par tick (faim critique)
	constexpr float  DISEASE_DAMAGE_PCT      = 1.0f;  // % des PV max perdus par tick tant qu'on est afflige

	// Maladie / poison / saignement : le membre peut en contracter, et apprend (Q-learning)
	// a aller se faire soigner par un medecin (PNJ neutre) quand il est afflige.
	constexpr uint32 DISEASE_TICK_MS       = 30000; // frequence du tirage de contagion ambiante
	constexpr float  DISEASE_CHANCE        = 5.0f;  // % de contracter une maladie (type Disease) par tirage
	constexpr float  DISEASE_CHANCE_COOK   = 3.0f;  // % de contracter une maladie (type Disease) par tirage
	constexpr float  DISEASE_CHANCE_DRINK  = 2.0f;  // % de contracter une maladie (type Disease) par tirage
	constexpr float  DISEASE_CHANCE_PRED   = 8.0f;  // % d'infliger un saignement quand un animal attaque

	// Recompense negative appliquee quand une action echoue / perd du temps.
	constexpr float REWARD_FAIL              = -0.5f;
	// Petite penalite de temps par pas de decision (encourage l'efficacite).
	constexpr float REWARD_TIME_PENALTY      = -0.05f;
	// Etre malade est tres penalisant : toute action menee en etant afflige est punie.
	// Comme se faire soigner mene a un etat sain (meilleure valeur future), l'agent apprend
	// a filer chez le medecin plutot qu'a vaquer a ses occupations en etant malade.
	constexpr float REWARD_DISEASED          = -0.60f;

	// Reward shaping : chaque etape productive de la chaine alimentaire / du feu donne
	// un gain immediat, sinon l'apprentissage (recompense finale unique) ne convergerait pas.
	constexpr float REWARD_RAWFOOD = 0.40f; // proie tuee -> viande crue
	constexpr float REWARD_WOOD    = 0.30f; // bois ramasse
	constexpr float REWARD_STONE   = 0.30f; // roche minee
	constexpr float REWARD_LIGHT   = 0.60f; // feu rallume
	constexpr float REWARD_COOK    = 1.00f; // plat cuit -> faim rassasiee
	constexpr float REWARD_CURE    = 1.00f; // maladie soignee par le medecin
	constexpr float REWARD_KILL_PREDATOR = 0.80f; // predateur extermine
	constexpr float REWARD_REMEMBER = 0.50f; // s'etre recueilli sur la tombe d'un ancetre (tradition)

	// Apprentissage du combat (choix defendre / fuir des adultes) - separe de la Q-table.
	constexpr float REWARD_DEFEND_WIN = 1.00f;  // avoir tue l'agresseur en se defendant
	constexpr float REWARD_DEFEND_HURT = -0.80f; // s'etre defendu mais fini a faible vie
	constexpr float REWARD_FLEE_SAFE  = 0.20f;  // avoir fui et atteint un lieu sur
	constexpr float DEFEND_HURT_HP_PCT = 25.0f; // en-dessous de ce % de PV, la defense est "ratee"
	constexpr float COMBAT_EXPLORE     = 0.10f; // exploration du choix combattre/fuir

	// ---------------------------------------------------------------------
	// Perception / execution
	// ---------------------------------------------------------------------
	constexpr float  RESOURCE_SEARCH_RANGE = 255.0f; // rayon de recherche des ressources
	constexpr float  INTERACT_RANGE        = 3.0f;   // distance d'interaction (boire, etc.)

	// Errance : au lieu de MoveRandom (qui vise un point navmesh parfois colle a un mur
	// ou dans un recoin etroit), on tire quelques directions et on garde la plus degagee
	// via un raycast anti-collision (destination toujours en espace ouvert / ligne de vue).
	constexpr float  WANDER_MIN_DIST   = 6.0f;   // distance min d'un saut d'errance
	constexpr float  WANDER_MAX_DIST   = 18.0f;  // distance max d'un saut d'errance
	constexpr uint8  WANDER_SAMPLES    = 4;      // nb de directions testees (on garde la plus ouverte)
	// Frequence du tick de decision. C'est aussi le temps mort MAXIMUM entre la fin d'une
	// action et le choix de la suivante : plus il est bas, plus les PNJ paraissent reactifs.
	constexpr uint32 DECISION_INTERVAL_MS  = 1500;
	constexpr uint32 INTERACT_DURATION_MS  = 4000;   // duree d'une interaction (boire/dormir)
	constexpr uint32 HUNT_TIMEOUT_MS       = 20000;  // garde-fou : abandon d'une chasse trop longue
	// Sequence de chasse : on s'approche a portee de tir, on abat la proie a l'arme a feu,
	// puis on marche jusqu'a la depouille et on s'agenouille pour prelever la viande.
	constexpr float  HUNT_SHOOT_RANGE      = 20.0f;  // distance a laquelle on ouvre le feu
	constexpr uint32 HUNT_KILL_DELAY_MS    = 800;    // temps entre le coup de feu et la mise a mort
	constexpr uint32 HUNT_SHOT_DELAY_MS    = 1000;   // temps entre le coup de feu et la mise a mort
	constexpr uint32 HUNT_LOOT_DURATION_MS = 2500;   // duree du prelevement (agenouille)
	constexpr uint32 WOOD_DURATION_MS      = 3000;   // duree de coupage du bois
	constexpr uint32 STONE_DURATION_MS     = 3000;   // duree de minage
	constexpr uint32 COOK_DURATION_MS      = 10000;  // duree de cuisson sur un feu
	constexpr uint32 SLEEP_DURATION_MS     = 10000;  // duree de sommeil
	constexpr uint32 MATE_DURATION_MS      = 3000;   // duree de l'accouplement (une fois les deux reunis)
	constexpr float  MATE_APPROACH_RANGE   = 0.8f;   // demi-distance entre les partenaires au point de rencontre (face a face, proches)
	constexpr uint32 MATE_APPROACH_TIMEOUT_MS = 15000; // attente max que le partenaire rejoigne le point de rencontre
	constexpr uint32 MATE_POLL_MS          = 400;    // frequence de verification "les deux partenaires sont-ils reunis ?"
	// Reproduction UNIQUEMENT dans la maison du couple : rayon autour du centre de la maison
	// en-deca duquel un partenaire est considere "a la maison". Au-dela, l'accouplement attend.
	constexpr float  MATE_HOUSE_RADIUS     = 10.0f;
	constexpr uint32 DOCTOR_DURATION_MS    = 8800;   // duree pour le docteur
	constexpr uint32 WANDER_DURATION_MS    = 3500;   // duree de marche / decouverte
	constexpr uint32 REMEMBER_DURATION_MS  = 3000;   // duree du recueillement sur la tombe
	constexpr uint32 REMEMBER_COOLDOWN_MS  = 60000;  // delai avant qu'un souvenir soit de nouveau recompense (anti-farm)

	// Feux et noeuds de ressource.
	// Tous les feux se consument puis s'eteignent : c'est ce qui oblige les PNJ a ramasser
	// bois + pierre et a les rallumer (et rend la cuisson non acquise).
	constexpr uint32 FIRE_BURN_DURATION_MS  = 60000;  // combustion avant extinction (1 min)
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
	constexpr uint32 REAL_SECONDS_PER_SIM_DAY = 60;
	constexpr uint32 AGE_CHILD_TO_ADULT_DAYS  = 10;
	constexpr uint32 AGE_ADULT_TO_ELDER_DAYS  = 40;
	constexpr uint32 AGE_DEATH_DAYS           = 50;
	// Fenetre (en jours simules) avant la mort ou un Ancien annonce que "la fin est proche".
	constexpr uint32 AGE_DEATH_WARNING_DAYS   = 3;
	constexpr uint32 REPRO_COOLDOWN_DAYS      = 5;
	// Un adulte doit etre suffisamment rassasie pour se reproduire.
	constexpr float  REPRO_READY_MAX_NEED     = 40.0f;
	// Echelle du modele pour un enfant.
	constexpr float  CHILD_SCALE              = 1.0f;
	// Tombes al?atoires
	constexpr uint8  GRAVESTONE_COUNT         = 3;
	constexpr uint32 GRAVESTONES[GRAVESTONE_COUNT]
        = { 2000007, 2000008, 2000009 };
	// Spot pour signaler les tombes
	constexpr uint32 GRAVESTONE_SPOT          = 1239999;
	// (Plus de gossip sur les tombes : l'epitaphe part a l'addon, qui l'affiche dans sa
	//  propre stele. Le gossip ne laissait piloter ni le fond, ni la police, ni la couleur
	//  du texte -- tout cela vit dans le client.)
    constexpr uint32 ASHES_DISPLAY_ID         = 38563;

	// Intervalle de sauvegarde periodique de l'etat (ms).
	constexpr uint32 SAVE_INTERVAL_MS = 45000;

	// Identifiants MovementInform emis par l'IA (evite les collisions avec CustomAI).
	enum ClanMovePointId : uint32
	{
		MOVE_TO_RESOURCE    = 5300000, // point d'eau (boire)
		MOVE_TO_MATE        = 5300001,
		MOVE_TO_HOME        = 5300002, // lit / maison (dormir)
		MOVE_TO_WOOD        = 5300003,
		MOVE_TO_ROCK        = 5300004,
		MOVE_TO_FIRE_LIGHT  = 5300005, // feu eteint a rallumer
		MOVE_TO_FIRE_COOK   = 5300006, // feu allume pour cuire
		MOVE_TO_FLEE        = 5300007, // point de fuite (enfants/anciens)
		MOVE_TO_DOCTOR      = 5300008, // medecin (pour se faire soigner)
		MOVE_TO_MATE_JOIN   = 5300009, // partenaire rejoignant le point de rencontre (cote mate)
		MOVE_TO_HOME_WANDER = 5300010, // retour a _home quand on ne peut pas errer (interieur)
		MOVE_TO_WANDER      = 5300011, // saut d'errance vers un point ouvert (raycast anti-mur)
		MOVE_TO_GRAVE       = 5300012, // se recueillir sur la tombe d'un ancetre
		MOVE_TO_PREY        = 5300013, // approche de la proie, a portee de tir
		MOVE_TO_CARCASS     = 5300014  // marche vers la depouille pour prelever la viande
	};

	// Nom de script attache aux gabarits des membres (creature_template.ScriptName).
	constexpr char const* MEMBER_SCRIPT_NAME = "npc_clan_member";

	// Cle de phrase reservee (custom_clan_phrase.action_type) pour le presage de mort d'un
	// Ancien. Volontairement hors de la plage des ActionType pour ne pas entrer en collision.
	constexpr uint8 PHRASE_DEATH_OMEN = 100;

	// Sort du docteur
	constexpr uint32 const SPELL_HEAL_DOCTOR = 463444;

	// Coup de feu tire sur la proie pendant la chasse.
	constexpr uint32 const SPELL_HUNT_SHOOT = 1246847;
}

#endif // CUSTOM_CLANS_CLANDEFINES_H
