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

// Point d'entree du module Clans :
//  - enregistre l'IA des membres (npc_clan_member) ;
//  - un WorldScript qui charge l'etat au demarrage et fait vivre la simulation ;
//  - une commande GM ".clan info" pour observer besoins et apprentissage.

#include "ScriptMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "ClanMemberAI.h"
#include "ClanMgr.h"
#include "ClanNeeds.h"
#include "ChatPackets.h"
#include "Creature.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "GameTime.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "RBAC.h"
#include "StringFormat.h"
#include "Util.h"
#include "WorldSession.h"
#include "WowTime.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <vector>

using namespace Clan;
using namespace Trinity::ChatCommands;

namespace
{
	char const* NeedName(NeedType n)
	{
		switch (n)
		{
			case NeedType::Hunger:  return "Faim";
			case NeedType::Thirst:  return "Soif";
			case NeedType::Energy:  return "Fatigue";
			case NeedType::Repro:   return "Repro";
			default:                return "-";
		}
	}

	char const* ActionName(ActionType a)
	{
		switch (a)
		{
			case ActionType::Wander:        return "Errer";
			case ActionType::Hunt:          return "Chasser";
			case ActionType::StoreHome:     return "Rentrer deposer";
			case ActionType::Drink:     return "Boire(puits)";
			case ActionType::Sleep:         return "Dormir";
			case ActionType::SeekMate:      return "Chercher partenaire";
			case ActionType::GatherWood:    return "Ramasser bois";
			case ActionType::MineRock:      return "Miner roche";
			case ActionType::LightFire:     return "Rallumer feu";
			case ActionType::Cook:          return "Cuire";
			case ActionType::SeekDoctor:    return "Voir le medecin";
			case ActionType::HuntPredator:  return "Exterminer predateur";
			case ActionType::Remember:      return "Se souvenir";
			case ActionType::Eat:           return "Manger";
			case ActionType::Shopping:      return "Acheter";
			case ActionType::Play:          return "Jouer";
			default:                        return "Rien";
		}
	}

	char const* ClanName(ClanId c)          { return c == ClanId::ClanA ? "A" : (c == ClanId::ClanB ? "B" : "?"); }
	char const* GenderName(Clan::Gender g)  { return g == Clan::Gender::Female ? "F" : "M"; }
	char const* StageName(LifeStage s)
	{
		switch (s)
		{
			case LifeStage::Child:  return "Enfant";
			case LifeStage::Elder:  return "Ancien";
			default:                return "Adulte";
		}
	}

	// --- Parsing des arguments des commandes de debug ---------------------------------
	std::string ToLower(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
		return s;
	}

	// Besoin nomme -> NeedType. Renvoie NeedType::Count si inconnu.
	NeedType ParseNeed(std::string const& raw)
	{
		std::string s = ToLower(raw);
		if (s == "hunger" || s == "faim")                       return NeedType::Hunger;
		if (s == "thirst" || s == "soif")                       return NeedType::Thirst;
		if (s == "energy" || s == "energie" || s == "fatigue")  return NeedType::Energy;
		if (s == "repro")                                       return NeedType::Repro;
		return NeedType::Count;
	}

	// Action nommee -> ActionType. Renvoie ActionType::Count si inconnue. Accepte des alias courts.
	ActionType ParseAction(std::string const& raw)
	{
		std::string s = ToLower(raw);
		if (s == "idle")                                                 return ActionType::Idle;
		if (s == "wander"       || s == "errer")                         return ActionType::Wander;
		if (s == "hunt"         || s == "chasser")                       return ActionType::Hunt;
		if (s == "storehome"    || s == "store"     || s == "deposer")   return ActionType::StoreHome;
		if (s == "well"         || s == "puits")                         return ActionType::Drink;
		if (s == "sleep"        || s == "dormir")                        return ActionType::Sleep;
		if (s == "seekmate"     || s == "mate"      || s == "repro")     return ActionType::SeekMate;
		if (s == "gatherwood"   || s == "wood"      || s == "bois")      return ActionType::GatherWood;
		if (s == "minerock"     || s == "rock"      || s == "roche")     return ActionType::MineRock;
		if (s == "lightfire"    || s == "fire"      || s == "feu")       return ActionType::LightFire;
		if (s == "cook"         || s == "cuire")                         return ActionType::Cook;
		if (s == "seekdoctor"   || s == "doctor"    || s == "medecin")   return ActionType::SeekDoctor;
		if (s == "huntpredator" || s == "predator"  || s == "predateur") return ActionType::HuntPredator;
		if (s == "remember"     || s == "souvenir")                      return ActionType::Remember;
		if (s == "eat"          || s == "manger")                        return ActionType::Eat;
		if (s == "shopping"     || s == "acheter")                       return ActionType::Shopping;
		if (s == "play"         || s == "jouer")                         return ActionType::Play;
		return ActionType::Count;
	}
}

// ---------------------------------------------------------------------------
// Suivi temps reel : .clan monitor streame les besoins d'un PNJ dans le chat.
// ---------------------------------------------------------------------------
namespace ClanMonitor
{
	// Prefixe des messages addon (a enregistrer cote client : ClanHUD).
	constexpr char const* ADDON_PREFIX = "CLANHUD";

	enum WatchKind : uint8
	{
		WATCH_CHAT  = 0,
		WATCH_ADDON = 1,
		WATCH_WORLD = 2,
		WATCH_ALL   = 3
	};

	struct Watch
	{
		ObjectGuid player;
		ObjectGuid creature; // vide pour WATCH_WORLD
		WatchKind  kind = WATCH_CHAT;
	};

	// Suivis actifs (un GM peut suivre plusieurs PNJ + la fenetre monde).
	std::vector<Watch> g_watchers;
	uint32 g_timer = 0;

	// Active/desactive un suivi (chat / addon PNJ / monde). Retourne true si active.
	bool static Toggle(Player* player, Creature* creature, WatchKind kind)
	{
		ObjectGuid pg = player->GetGUID();
		ObjectGuid cg = creature ? creature->GetGUID() : ObjectGuid::Empty;
		for (auto it = g_watchers.begin(); it != g_watchers.end(); ++it)
		{
			if (it->player == pg && it->creature == cg && it->kind == kind)
			{
				g_watchers.erase(it);
				return false;
			}
		}
		g_watchers.push_back({ pg, cg, kind });
		return true;
	}

	// Ligne de chat lisible (mode .clan monitor).
	void static SendLine(Player* player, npc_clan_member* ai)
	{
		MemberState const* s = ai->GetState();
		MindState cur = ai->CurrentMindState();
		ActionType best = s->mind.BestAction(cur.Index(), ai->GetRole());
		HouseState const* house = sClanMgr->GetHouseBySpawn(s->houseSpawnId);
		ChatHandler(player->GetSession()).PSendSysMessage(
			"|cff88ccff[suivi]|r %s%s %s | F%.0f S%.0f E%.0f R%.0f | maison v/b/p:%u/%u/%u repas:%u feu:%s | eps%.2f | ideal:%s",
			ClanName(s->clan), GenderName(s->gender), StageName(s->stage),
			s->needs.hunger, s->needs.thirst, s->needs.energy, s->needs.reproUrge,
			house ? house->Get(ItemType::RawFood) : 0, house ? house->Get(ItemType::Wood) : 0, house ? house->Get(ItemType::Stone) : 0,
			house ? house->meals : 0,
			cur.houseFireLit ? "o" : "n", s->mind.GetEpsilon(), ActionName(best));
	}

	// Message addon "k=v;..." parse par l'addon ClanHUD (mode .clan hud / .clan hudall).
	// asTable = true -> prefixe "tbl=1;" pour que l'addon route la donnee vers le TABLEAU
	// global (flux "tout suivre") au lieu d'une fenetre individuelle.
	void static SendAddon(Player* player, Creature* creature, npc_clan_member* ai, bool asTable = false)
	{
		MemberState const* s = ai->GetState();
		MindState cur = ai->CurrentMindState();
		ActionType best = s->mind.BestAction(cur.Index(), ai->GetRole());

		// Conjoint : le nom vit sur la creature, pas dans MemberState. S'il n'est pas
		// apparu (hors de portee, pas encore spawn), on sait qu'il est marie sans plus.
		std::string spouseName = "-";
		if (s->spouseId)
		{
			spouseName = "?";
			if (MemberState const* sp = sClanMgr->GetStateByDbId(s->spouseId))
				if (Creature* spouse = ObjectAccessor::GetCreature(*creature, sp->liveGuid))
					spouseName = spouse->GetName();
		}

		// Stock partage de la MAISON du membre (viande/bois/pierre + repas prets).
		HouseState const* house = sClanMgr->GetHouseBySpawn(s->houseSpawnId);
		uint32 hRaw   = house ? house->Get(ItemType::RawFood) : 0;
		uint32 hWood  = house ? house->Get(ItemType::Wood)    : 0;
		uint32 hStone = house ? house->Get(ItemType::Stone)   : 0;
		uint32 hMeal  = house ? house->meals                  : 0;

		// StringFormat est en style fmt ({}), contrairement a PSendSysMessage.
		// id = identifiant du PNJ (compteur de GUID) : cle de fenetre + ciblage (.clan target).
		// hp = pourcentage de vie. dis = masque d'affliction (bit0=maladie, bit1=poison, bit2=saignement).
		// raw/wo/sto = QUANTITES portees (transit vers le stock), cap = capacite de portage,
		// bag = 1 si au moins un type est plein (le membre doit rentrer livrer avant de repartir).
		// fb = secondes restantes avant extinction du foyer (0 = eteint).
		uint32 fireBurnSec = sClanMgr->GetHouseFireBurnMs(creature, s->houseSpawnId) / 1000;

		// hraw/hwood/hstn/hmeal = STOCK de la maison ; hcap/hmmax = capacites ; fi = foyer allume ; fb = compte a rebours.
		std::string payload = Trinity::StringFormat(
			"{}id={};n={};cl={};ge={};st={};ag={};hp={:.0f};hu={:.0f};th={:.0f};en={:.0f};re={:.0f};"
			"raw={};wo={};sto={};cap={};bag={};fi={};dis={};eps={:.2f};act={};best={};sp={};"
			"hraw={};hwood={};hstn={};hmeal={};hcap={};hmmax={};fb={}",
			asTable ? "tbl=1;" : "",
			creature->GetGUID().GetCounter(), creature->GetName(), ClanName(s->clan), GenderName(s->gender), StageName(s->stage),
			s->ageDays, creature->GetHealthPct(), s->needs.hunger, s->needs.thirst, s->needs.energy, s->needs.reproUrge,
			ai->GetItemCount(ItemType::RawFood), ai->GetItemCount(ItemType::Wood), ai->GetItemCount(ItemType::Stone),
			INVENTORY_MAX_PER_ITEM, cur.bagFull ? 1 : 0,
			cur.houseFireLit ? 1 : 0, sClanMgr->GetAfflictionMask(creature),
			s->mind.GetEpsilon(), ActionName(ai->CurrentAction()), ActionName(best), spouseName,
			hRaw, hWood, hStone, hMeal, HOUSE_STOCK_MAX, HOUSE_MEALS_MAX, fireBurnSec);

		WorldPackets::Chat::Chat packet;
		packet.Initialize(CHAT_MSG_WHISPER, LANG_ADDON, creature, player, payload, 0, "", DEFAULT_LOCALE, ADDON_PREFIX);
		player->SendDirectMessage(packet.Write());
	}

	// Message addon de l'etat du monde (fenetre globale de l'addon).
	void static SendWorld(Player* player)
	{
		Clan::WorldSummary w = sClanMgr->GetWorldSummary();

		int32 hour = 12;
		bool night = false;
		if (WowTime const* t = GameTime::GetWowTime())
		{
			hour = t->GetHour();
			night = Clan::IsNight(uint8(hour));
		}

		std::string payload = Trinity::StringFormat(
			"w=1;hour={};night={};pop={};ad={};ch={};el={};sick={};fl={};ft={}",
			hour, night ? 1 : 0, w.population, w.adults, w.children, w.elders, w.sick,
			w.firesLit, w.firesTotal);

		WorldPackets::Chat::Chat packet;
		packet.Initialize(CHAT_MSG_WHISPER, LANG_ADDON, player, player, payload, 0, "", DEFAULT_LOCALE, ADDON_PREFIX);
		player->SendDirectMessage(packet.Write());
	}

	// Envoie l'epitaphe a l'addon, qui l'affiche dans SA fenetre (une stele dediee).
	void static SendGraveEpitaph(Player* player, std::string const& name, std::string const& epitaph)
	{
		// Le payload est decoupe sur ';' : on neutralise ceux du texte pour ne pas
		// tronquer l'epitaphe cote addon.
		std::string safe = epitaph;
		StringReplaceAll(&safe, ";", ",");

		WorldPackets::Chat::Chat packet;
		packet.Initialize(CHAT_MSG_WHISPER, LANG_ADDON, player, player,
			Trinity::StringFormat("ep={};epN={}", safe, name), 0, "", DEFAULT_LOCALE, ADDON_PREFIX);
		player->SendDirectMessage(packet.Write());
	}

	// Appele chaque tick ; emet une mise a jour par seconde et nettoie les suivis invalides.
	void static Update(uint32 diff)
	{
		if (g_watchers.empty())
			return;

		g_timer += diff;
		if (g_timer < 1000)
			return;
		g_timer = 0;

		for (auto it = g_watchers.begin(); it != g_watchers.end(); )
		{
			Player* player = ObjectAccessor::FindPlayer(it->player);
			if (!player)
			{
				it = g_watchers.erase(it);
				continue;
			}

			// Fenetre monde : pas de PNJ, juste le resume global.
			if (it->kind == WATCH_WORLD)
			{
				SendWorld(player);
				++it;
				continue;
			}

			// Tableau global : une ligne par membre vivant (flux "tout suivre").
			if (it->kind == WATCH_ALL)
			{
				for (ObjectGuid guid : sClanMgr->GetLiveMemberGuids())
				{
					Creature* c = ObjectAccessor::GetCreature(*player, guid);
					npc_clan_member* mai = c ? dynamic_cast<npc_clan_member*>(c->AI()) : nullptr;
					if (mai && mai->GetState())
						SendAddon(player, c, mai, true);
				}
				++it;
				continue;
			}

			Creature* creature = ObjectAccessor::GetCreature(*player, it->creature);
			npc_clan_member* ai = creature ? dynamic_cast<npc_clan_member*>(creature->AI()) : nullptr;
			if (!ai || !ai->GetState())
			{
				if (it->kind == WATCH_CHAT)
					ChatHandler(player->GetSession()).SendSysMessage("Suivi termine (membre indisponible).");
				it = g_watchers.erase(it);
				continue;
			}

			if (it->kind == WATCH_ADDON)
				SendAddon(player, creature, ai);
			else
				SendLine(player, ai);
			++it;
		}
	}
}

// --- Simulation globale ---
class ClanWorldScript : public WorldScript
{
public:
	ClanWorldScript() : WorldScript("ClanWorldScript") { }

	void OnStartup() override
	{
		sClanMgr->LoadFromDB();
		sClanMgr->RespawnBirths();
	}

	void OnUpdate(uint32 diff) override
	{
		sClanMgr->Update(diff);
		ClanMonitor::Update(diff);
	}

	void OnShutdown() override
	{
		sClanMgr->SaveAll(true);
	}
};

// ---------------------------------------------------------------------------
// Pierre tombale : un clic affiche l'epitaphe (nom du defunt + cause de la mort).
// ---------------------------------------------------------------------------
struct go_clan_gravestone : public GameObjectAI
{
	explicit go_clan_gravestone(GameObject* go) : GameObjectAI(go) { }

	bool OnGossipHello(Player* player) override
	{
		Clan::GraveyardSlot const* grave = sClanMgr->FindGraveByGuid(me->GetGUID());
		if (!grave)
			return false;

		ClanMonitor::SendGraveEpitaph(player, grave->deceasedName, grave->epitaph);
		return true; 
	}
};

// --- Commande de debug : .clan info (sur une creature selectionnee) ---
class clan_commandscript : public CommandScript
{
public:
	clan_commandscript() : CommandScript("clan_commandscript") { }

	std::span<ChatCommandBuilder const> GetCommands() const override
	{
		static ChatCommandTable clanInfoTable =
		{
			{ "info",    HandleClanInfoCommand,    rbac::RBAC_PERM_COMMAND_NPC_INFO, Console::No },
			{ "monitor", HandleClanMonitorCommand, rbac::RBAC_PERM_COMMAND_NPC_INFO, Console::No },
			{ "hud",     HandleClanHudCommand,     rbac::RBAC_PERM_COMMAND_NPC_INFO, Console::No },
			{ "hudall",  HandleClanHudAllCommand,  rbac::RBAC_PERM_COMMAND_NPC_INFO, Console::No },
			{ "world",   HandleClanWorldCommand,   rbac::RBAC_PERM_COMMAND_NPC_INFO, Console::No },
			{ "reload",  HandleClanReloadCommand,  rbac::RBAC_PERM_COMMAND_NPC_INFO, Console::Yes },
			{ "reset",   HandleClanResetCommand,   rbac::RBAC_PERM_COMMAND_NPC_INFO, Console::Yes },
			{ "need",    HandleClanNeedCommand,    rbac::RBAC_PERM_COMMAND_NPC_INFO, Console::No },
			{ "force",   HandleClanForceCommand,   rbac::RBAC_PERM_COMMAND_NPC_INFO, Console::No },
			{ "ready",   HandleClanReadyCommand,   rbac::RBAC_PERM_COMMAND_NPC_INFO, Console::No },
			{ "disease", HandleClanDiseaseCommand, rbac::RBAC_PERM_COMMAND_NPC_INFO, Console::No },
			{ "cure",    HandleClanCureCommand,    rbac::RBAC_PERM_COMMAND_NPC_INFO, Console::No },
		};
		static ChatCommandTable commandTable =
		{
			{ "clan", clanInfoTable },
		};
		return commandTable;
	}

	static bool HandleClanInfoCommand(ChatHandler* handler)
	{
		Creature* target = handler->getSelectedCreature();
		if (!target)
		{
			handler->SendSysMessage("Selectionnez une creature de clan.");
			handler->SetSentErrorMessage(true);
			return false;
		}

		npc_clan_member* ai = dynamic_cast<npc_clan_member*>(target->AI());
		if (!ai || !ai->GetState())
		{
			handler->SendSysMessage("Cette creature n'est pas un membre de clan.");
			handler->SetSentErrorMessage(true);
			return false;
		}

		MemberState const* s = ai->GetState();
		handler->PSendSysMessage("|cff00ff00[Clan]|r dbId %s | clan %s | genre %s | etape %s | age %uj | cd repro %uj",
			std::to_string(s->dbId).c_str(), ClanName(s->clan), GenderName(s->gender), StageName(s->stage),
			s->ageDays, s->reproCooldownDays);
		handler->PSendSysMessage("Maison %s | Lit %s",
			s->houseSpawnId ? std::to_string(s->houseSpawnId).c_str() : "aucune",
			s->bedSpawnId   ? std::to_string(s->bedSpawnId).c_str()   : "aucun");
		handler->PSendSysMessage("Besoins -> Faim %.0f | Soif %.0f | Fatigue %.0f | Repro %.0f",
			s->needs.hunger, s->needs.thirst, s->needs.energy, s->needs.reproUrge);
		handler->PSendSysMessage("Apprentissage -> epsilon %.3f (bas = exploite ce qu'il a appris)", s->mind.GetEpsilon());

		MindState cur = ai->CurrentMindState();
		if (HouseState const* h = sClanMgr->GetHouseBySpawn(s->houseSpawnId))
			handler->PSendSysMessage("Maison stock (max %u) -> viande: %u | bois: %u | pierre: %u | repas: %u/%u | foyer: %s",
				HOUSE_STOCK_MAX, h->Get(ItemType::RawFood), h->Get(ItemType::Wood), h->Get(ItemType::Stone),
				h->meals, HOUSE_MEALS_MAX, cur.houseFireLit ? "allume" : "eteint");
		else
			handler->PSendSysMessage("Maison stock -> (aucune maison attribuee)");
		// "SAC PLEIN" = le membre doit rentrer livrer avant de repartir en tournee (regle
		// deterministe de DecisionTick). C'est aussi le bit percu par la Q-table (MindState::bagFull).
		handler->PSendSysMessage("Porte (transit, max %u) -> viande: %u | bois: %u | pierre: %u%s",
			INVENTORY_MAX_PER_ITEM,
			ai->GetItemCount(ItemType::RawFood), ai->GetItemCount(ItemType::Wood), ai->GetItemCount(ItemType::Stone),
			cur.bagFull ? " | |cffff6060SAC PLEIN -> doit rentrer deposer|r" : "");

		uint16 idx = cur.Index();
		ActionType best = s->mind.BestAction(idx, ai->GetRole());
		handler->PSendSysMessage("Etat courant (besoin=%s, %s) -> meilleure action: %s (Q=%.2f)",
			NeedName(cur.urgentNeed), cur.night ? "nuit" : "jour",
			ActionName(best), s->mind.ValueOf(idx, best));
		return true;
	}

	static bool HandleClanMonitorCommand(ChatHandler* handler)
	{
		Creature* target = handler->getSelectedCreature();
		if (!target)
		{
			handler->SendSysMessage("Selectionnez une creature de clan.");
			handler->SetSentErrorMessage(true);
			return false;
		}

		npc_clan_member* ai = dynamic_cast<npc_clan_member*>(target->AI());
		if (!ai || !ai->GetState())
		{
			handler->SendSysMessage("Cette creature n'est pas un membre de clan.");
			handler->SetSentErrorMessage(true);
			return false;
		}

		Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
		if (!player)
		{
			handler->SendSysMessage("Commande utilisable uniquement en jeu.");
			handler->SetSentErrorMessage(true);
			return false;
		}

		bool on = ClanMonitor::Toggle(player, target, ClanMonitor::WATCH_CHAT);
		handler->PSendSysMessage("Suivi temps reel (chat) : %s.", on ? "ACTIVE (1 ligne/s)" : "desactive");
		return true;
	}

	static bool HandleClanHudCommand(ChatHandler* handler)
	{
		Creature* target = handler->getSelectedCreature();
		if (!target)
		{
			handler->SendSysMessage("Selectionnez une creature de clan.");
			handler->SetSentErrorMessage(true);
			return false;
		}

		npc_clan_member* ai = dynamic_cast<npc_clan_member*>(target->AI());
		if (!ai || !ai->GetState())
		{
			handler->SendSysMessage("Cette creature n'est pas un membre de clan.");
			handler->SetSentErrorMessage(true);
			return false;
		}

		Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
		if (!player)
		{
			handler->SendSysMessage("Commande utilisable uniquement en jeu.");
			handler->SetSentErrorMessage(true);
			return false;
		}

		bool on = ClanMonitor::Toggle(player, target, ClanMonitor::WATCH_ADDON);
		handler->PSendSysMessage("Addon ClanHUD : %s.", on ? "ACTIVE (donnees envoyees 1x/s)" : "desactive");
		return true;
	}

	static bool HandleClanHudAllCommand(ChatHandler* handler)
	{
		Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
		if (!player)
		{
			handler->SendSysMessage("Commande utilisable uniquement en jeu.");
			handler->SetSentErrorMessage(true);
			return false;
		}

		bool on = ClanMonitor::Toggle(player, nullptr, ClanMonitor::WATCH_ALL);
		handler->PSendSysMessage("Tableau ClanHUD (tout suivre) : %s.", on ? "ACTIVE (tous les membres, 1x/s)" : "desactive");
		return true;
	}

	static bool HandleClanReloadCommand(ChatHandler* handler)
	{
		sClanMgr->ReloadRegistries();
		handler->SendSysMessage("Registres Clans recharges (phrases, fx/items, epitaphes, ressources, gabarits, lits).");
		return true;
	}

	static bool HandleClanResetCommand(ChatHandler* handler)
	{
		sClanMgr->ResetAll();
		handler->PSendSysMessage("Simulation Clans remise a zero : %u membre(s) en jeu.", uint32(sClanMgr->GetMemberCount()));
		return true;
	}

	static npc_clan_member* GetSelectedMember(ChatHandler* handler)
	{
		Creature* target = handler->getSelectedCreature();
		if (!target)
		{
			handler->SendSysMessage("Selectionnez une creature de clan.");
			handler->SetSentErrorMessage(true);
			return nullptr;
		}

		npc_clan_member* ai = dynamic_cast<npc_clan_member*>(target->AI());
		if (!ai || !ai->GetState())
		{
			handler->SendSysMessage("Cette creature n'est pas un membre de clan.");
			handler->SetSentErrorMessage(true);
			return nullptr;
		}
		return ai;
	}

	static bool HandleClanNeedCommand(ChatHandler* handler, std::string const& need, uint32 value)
	{
		npc_clan_member* ai = GetSelectedMember(handler);
		if (!ai)
			return false;

		NeedType type = ParseNeed(need);
		if (type == NeedType::Count)
		{
			handler->SendSysMessage("Besoin inconnu. Utilisez : hunger | thirst | energy | repro.");
			handler->SetSentErrorMessage(true);
			return false;
		}

		float v = float(std::min<uint32>(value, 100));
		MemberState* s = ai->GetState();
		switch (type)
		{
			case NeedType::Hunger: s->needs.hunger    = v; break;
			case NeedType::Thirst: s->needs.thirst    = v; break;
			case NeedType::Energy: s->needs.energy    = v; break;
			case NeedType::Repro:  s->needs.reproUrge = v; break;
			default: break;
		}
		s->dirty = true;
		handler->PSendSysMessage("%s regle a %.0f pour ce membre.", NeedName(type), v);
		return true;
	}

	static bool HandleClanForceCommand(ChatHandler* handler, std::string const& action)
	{
		npc_clan_member* ai = GetSelectedMember(handler);
		if (!ai)
			return false;

		ActionType act = ParseAction(action);
		if (act == ActionType::Count)
		{
			handler->SendSysMessage("Action inconnue. Ex : hunt, cook, sleep, seekmate, lightfire, "
				"gatherwood, minerock, drinkriver, drinkwell, seekdoctor, huntpredator, remember, wander.");
			handler->SetSentErrorMessage(true);
			return false;
		}

		bool ok = ai->ForceAction(act);
		handler->PSendSysMessage("Action forcee '%s' : %s.", ActionName(act),
			ok ? "demarree" : "n'a PAS pu demarrer (prerequis manquants : ressource absente, pas de maison, etc.)");
		return true;
	}

	static bool HandleClanReadyCommand(ChatHandler* handler)
	{
		npc_clan_member* ai = GetSelectedMember(handler);
		if (!ai)
			return false;

		MemberState* s = ai->GetState();
		s->needs.hunger      = 0.0f;
		s->needs.thirst      = 0.0f;
		s->needs.energy      = 0.0f;
		s->needs.reproUrge   = 90.0f;
		s->reproCooldownDays = 0;
		s->dirty             = true;

		std::string warn;
		if (s->stage != LifeStage::Adult)
			warn += " [PAS adulte -> ne se reproduira pas]";
		if (!s->houseSpawnId)
			warn += " [AUCUNE maison attribuee -> SeekMate echouera]";

		handler->PSendSysMessage("Membre pret a la reproduction (besoins vitaux 0, envie repro 90, cooldown 0).%s", warn.c_str());
		return true;
	}

	static bool HandleClanDiseaseCommand(ChatHandler* handler)
	{
		npc_clan_member* ai = GetSelectedMember(handler);
		if (!ai)
			return false;

		Creature* target = handler->getSelectedCreature();
		uint32 aura = sClanMgr->GetRandomDisease(AfflictionType::Disease);
		if (!aura)
		{
			handler->SendSysMessage("Aucune affliction declaree (custom_clan_disease).");
			handler->SetSentErrorMessage(true);
			return false;
		}

		target->AddAura(aura, target);
		handler->PSendSysMessage("Affliction %u appliquee (le membre devrait apprendre a aller voir le medecin).", aura);
		return true;
	}

	static bool HandleClanCureCommand(ChatHandler* handler)
	{
		npc_clan_member* ai = GetSelectedMember(handler);
		if (!ai)
			return false;

		Creature* target = handler->getSelectedCreature();
		sClanMgr->CureDiseases(target);
		handler->SendSysMessage("Afflictions retirees.");
		return true;
	}

	static bool HandleClanWorldCommand(ChatHandler* handler)
	{
		Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
		if (!player)
		{
			handler->SendSysMessage("Commande utilisable uniquement en jeu.");
			handler->SetSentErrorMessage(true);
			return false;
		}

		bool on = ClanMonitor::Toggle(player, nullptr, ClanMonitor::WATCH_WORLD);
		handler->PSendSysMessage("Fenetre monde ClanHUD : %s.", on ? "ACTIVE (etat du monde 1x/s)" : "desactive");
		return true;
	}
};

void AddSC_npcs_clan()
{
	RegisterCreatureAI(npc_clan_member);
	RegisterGameObjectAI(go_clan_gravestone);
	new ClanWorldScript();
	new clan_commandscript();
}
