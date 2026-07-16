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
#include "GameTime.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "RBAC.h"
#include "StringFormat.h"
#include "WorldSession.h"
#include "WowTime.h"
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
            case NeedType::Hunger: return "Faim";
            case NeedType::Thirst: return "Soif";
            case NeedType::Energy: return "Fatigue";
            case NeedType::Repro:  return "Repro";
            default:               return "-";
        }
    }

    char const* ActionName(ActionType a)
    {
        switch (a)
        {
            case ActionType::Wander:     return "Errer";
            case ActionType::Hunt:       return "Chasser";
            case ActionType::DrinkRiver: return "Boire(riviere)";
            case ActionType::DrinkWell:  return "Boire(puits)";
            case ActionType::Sleep:      return "Dormir";
            case ActionType::SeekMate:   return "Chercher partenaire";
            case ActionType::GatherWood: return "Ramasser bois";
            case ActionType::MineRock:   return "Miner roche";
            case ActionType::LightFire:  return "Rallumer feu";
            case ActionType::Cook:       return "Cuire";
            case ActionType::SeekDoctor: return "Voir le medecin";
            case ActionType::HuntPredator: return "Exterminer predateur";
            case ActionType::Remember:   return "Se souvenir";
            default:                     return "Rien";
        }
    }

    char const* ClanName(ClanId c)  { return c == ClanId::ClanA ? "A" : (c == ClanId::ClanB ? "B" : "?"); }
    char const* GenderName(Clan::Gender g) { return g == Clan::Gender::Female ? "F" : "M"; }
    char const* StageName(LifeStage s)
    {
        switch (s)
        {
            case LifeStage::Child: return "Enfant";
            case LifeStage::Elder: return "Ancien";
            default:               return "Adulte";
        }
    }
}

// ---------------------------------------------------------------------------
// Suivi temps reel : .clan monitor streame les besoins d'un PNJ dans le chat.
// ---------------------------------------------------------------------------
namespace ClanMonitor
{
    // Prefixe des messages addon (a enregistrer cote client : ClanHUD).
    constexpr char const* ADDON_PREFIX = "CLANHUD";

    enum WatchKind : uint8 { WATCH_CHAT = 0, WATCH_ADDON = 1, WATCH_WORLD = 2, WATCH_ALL = 3 };

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
    bool Toggle(Player* player, Creature* creature, WatchKind kind)
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
    void SendLine(Player* player, npc_clan_member* ai)
    {
        MemberState const* s = ai->GetState();
        MindState cur = ai->CurrentMindState();
        ActionType best = s->mind.BestAction(cur.Index());
        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cff88ccff[suivi]|r %s%s %s | F%.0f S%.0f E%.0f R%.0f | inv r/b/p:%s/%s/%s feu:%s | eps%.2f | ideal:%s",
            ClanName(s->clan), GenderName(s->gender), StageName(s->stage),
            s->needs.hunger, s->needs.thirst, s->needs.energy, s->needs.reproUrge,
            ai->HasRawFood() ? "o" : "n", ai->HasWood() ? "o" : "n", ai->HasStone() ? "o" : "n",
            cur.litFireNearby ? "o" : "n", s->mind.GetEpsilon(), ActionName(best));
    }

    // Message addon "k=v;..." parse par l'addon ClanHUD (mode .clan hud / .clan hudall).
    // asTable = true -> prefixe "tbl=1;" pour que l'addon route la donnee vers le TABLEAU
    // global (flux "tout suivre") au lieu d'une fenetre individuelle.
    void SendAddon(Player* player, Creature* creature, npc_clan_member* ai, bool asTable = false)
    {
        MemberState const* s = ai->GetState();
        MindState cur = ai->CurrentMindState();
        ActionType best = s->mind.BestAction(cur.Index());

        // StringFormat est en style fmt ({}), contrairement a PSendSysMessage.
        // id = identifiant du PNJ (compteur de GUID) : cle de fenetre + ciblage (.clan target).
        // hp = pourcentage de vie. dis = masque d'affliction (bit0=maladie, bit1=poison, bit2=saignement).
        std::string payload = Trinity::StringFormat(
            "{}id={};n={};cl={};ge={};st={};ag={};hp={:.0f};hu={:.0f};th={:.0f};en={:.0f};re={:.0f};"
            "raw={};wo={};sto={};fi={};dis={};eps={:.2f};act={};best={}",
            asTable ? "tbl=1;" : "",
            creature->GetGUID().GetCounter(), creature->GetName(), ClanName(s->clan), GenderName(s->gender), StageName(s->stage),
            s->ageDays, creature->GetHealthPct(), s->needs.hunger, s->needs.thirst, s->needs.energy, s->needs.reproUrge,
            ai->HasRawFood() ? 1 : 0, ai->HasWood() ? 1 : 0, ai->HasStone() ? 1 : 0,
            cur.litFireNearby ? 1 : 0, sClanMgr->GetAfflictionMask(creature),
            s->mind.GetEpsilon(), ActionName(ai->CurrentAction()), ActionName(best));

        WorldPackets::Chat::Chat packet;
        packet.Initialize(CHAT_MSG_WHISPER, LANG_ADDON, creature, player, payload, 0, "", DEFAULT_LOCALE, ADDON_PREFIX);
        player->SendDirectMessage(packet.Write());
    }

    // Message addon de l'etat du monde (fenetre globale de l'addon).
    void SendWorld(Player* player)
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
            "w=1;hour={};night={};pop={};ad={};ch={};el={};sick={}",
            hour, night ? 1 : 0, w.population, w.adults, w.children, w.elders, w.sick);

        WorldPackets::Chat::Chat packet;
        packet.Initialize(CHAT_MSG_WHISPER, LANG_ADDON, player, player, payload, 0, "", DEFAULT_LOCALE, ADDON_PREFIX);
        player->SendDirectMessage(packet.Write());
    }

    // Appele chaque tick ; emet une mise a jour par seconde et nettoie les suivis invalides.
    void Update(uint32 diff)
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

// IA des membres : attachee via creature_template.ScriptName = "npc_clan_member".
// (RegisterCreatureAI utilise le nom de la struct comme ScriptName.)

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
            { "target",  HandleClanTargetCommand,  rbac::RBAC_PERM_COMMAND_NPC_INFO, Console::No },
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

        // NB : PSendSysMessage de ce fork est en style printf (%s/%u/%.2f), pas fmt "{}".
        MemberState const* s = ai->GetState();
        handler->PSendSysMessage("|cff00ff00[Clan]|r dbId %s | clan %s | genre %s | etape %s | age %uj | cd repro %uj",
            std::to_string(s->dbId).c_str(), ClanName(s->clan), GenderName(s->gender), StageName(s->stage),
            s->ageDays, s->reproCooldownDays);
        handler->PSendSysMessage("Besoins -> Faim %.0f | Soif %.0f | Fatigue %.0f | Repro %.0f",
            s->needs.hunger, s->needs.thirst, s->needs.energy, s->needs.reproUrge);
        handler->PSendSysMessage("Apprentissage -> epsilon %.3f (bas = exploite ce qu'il a appris)", s->mind.GetEpsilon());

        // Inventaire courant (chaine cuisson / rallumage).
        MindState cur = ai->CurrentMindState();
        handler->PSendSysMessage("Inventaire -> viande crue: %s | bois: %s | pierre: %s | feu allume proche: %s",
            ai->HasRawFood() ? "oui" : "non", ai->HasWood() ? "oui" : "non",
            ai->HasStone() ? "oui" : "non", cur.litFireNearby ? "oui" : "non");

        // Meilleure action apprise pour l'etat courant (preuve d'apprentissage).
        uint16 idx = cur.Index();
        ActionType best = s->mind.BestAction(idx);
        handler->PSendSysMessage("Etat courant (besoin=%s, %s) -> meilleure action: %s (Q=%.2f)",
            NeedName(cur.urgentNeed), cur.night ? "nuit" : "jour",
            ActionName(best), s->mind.ValueOf(idx, best));
        return true;
    }

    // .clan monitor : active/desactive le suivi temps reel (1 ligne/s) du membre selectionne.
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

    // .clan hud : active/desactive l'envoi des donnees a l'addon ClanHUD (1 maj/s).
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

    // .clan hudall : active/desactive le flux "tout suivre" (tableau global de l'addon).
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

    // .clan target <counter> : cible le membre dont le GUID a ce compteur (appele par l'addon).
    static bool HandleClanTargetCommand(ChatHandler* handler, uint64 counter)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
        {
            handler->SendSysMessage("Commande utilisable uniquement en jeu.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        for (ObjectGuid guid : sClanMgr->GetLiveMemberGuids())
        {
            if (guid.GetCounter() != counter)
                continue;

            // Le PNJ doit etre visible du client pour etre reellement cible.
            if (ObjectAccessor::GetCreature(*player, guid))
            {
                player->SetSelection(guid);
                return true;
            }

            handler->SendSysMessage("Ce membre est hors de portee (trop loin pour etre cible).");
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->SendSysMessage("Membre introuvable.");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // .clan world : active/desactive l'envoi de l'etat du monde a l'addon (fenetre globale).
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
    new ClanWorldScript();
    new clan_commandscript();
}
