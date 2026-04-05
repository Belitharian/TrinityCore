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

/* ScriptData
Name: cast_commandscript
%Complete: 100
Comment: All cast related commands
Category: commandscripts
EndScriptData */

#include "ScriptMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "Creature.h"
#include "Language.h"
#include "Player.h"
#include "RBAC.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "WorldSession.h"

using namespace Trinity::ChatCommands;

class cast_commandscript : public CommandScript
{
public:
    cast_commandscript() : CommandScript("cast_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable castCommandTable =
        {
            { "back",   HandleCastBackCommand,  rbac::RBAC_PERM_COMMAND_CAST_BACK,   Console::No },
            { "dist",   HandleCastDistCommand,  rbac::RBAC_PERM_COMMAND_CAST_DIST,   Console::No },
            { "self",   HandleCastSelfCommand,  rbac::RBAC_PERM_COMMAND_CAST_SELF,   Console::No },
            { "target", HandleCastTargetCommad, rbac::RBAC_PERM_COMMAND_CAST_TARGET, Console::No },
            { "dest",   HandleCastDestCommand,  rbac::RBAC_PERM_COMMAND_CAST_DEST,   Console::No },

            // Custom
            { "list",   HandleCastListCommand,  rbac::RBAC_PERM_COMMAND_CAST,        Console::No },
            { "stop",   HandleCastStopCommand,  rbac::RBAC_PERM_COMMAND_CAST,        Console::No },

            { "",       HandleCastCommand,      rbac::RBAC_PERM_COMMAND_CAST,        Console::No },
        };
        static ChatCommandTable commandTable =
        {
            { "cast", castCommandTable },
        };
        return commandTable;
    }

    static bool CheckSpellExistsAndIsValid(ChatHandler* handler, SpellInfo const* spell)
    {
        if (!spell)
        {
            handler->PSendSysMessage(LANG_COMMAND_NOSPELLFOUND);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!SpellMgr::IsSpellValid(spell, handler->GetSession()->GetPlayer()))
        {
            handler->PSendSysMessage(LANG_COMMAND_SPELL_BROKEN, spell->Id);
            handler->SetSentErrorMessage(true);
            return false;
        }
        return true;
    }

    static bool CheckSpellExistsAndIsValid(ChatHandler* handler, uint32 spell)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spell, DIFFICULTY_NONE);

        if (!spellInfo)
        {
            handler->PSendSysMessage(LANG_COMMAND_NOSPELLFOUND);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!SpellMgr::IsSpellValid(spellInfo, handler->GetSession()->GetPlayer()))
        {
            handler->PSendSysMessage(LANG_COMMAND_SPELL_BROKEN, spellInfo->Id);
            handler->SetSentErrorMessage(true);
            return false;
        }
        return true;
    }

    static Optional<TriggerCastFlags> GetTriggerFlags(Optional<std::string> triggeredStr)
    {
        if (triggeredStr)
        {
            if (StringStartsWith("triggered", *triggeredStr)) // check if "triggered" starts with *triggeredStr (e.g. "trig", "trigger", etc.)
                return TRIGGERED_FULL_DEBUG_MASK;
            else
                return std::nullopt;
        }
        return TRIGGERED_NONE;
    }

    static bool HandleCastCommand(ChatHandler* handler, SpellInfo const* spell, Optional<std::string> triggeredStr)
    {
        Unit* target = handler->getSelectedUnit();
        if (!target)
        {
            handler->SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!CheckSpellExistsAndIsValid(handler, spell))
            return false;

        Optional<TriggerCastFlags> triggerFlags = GetTriggerFlags(triggeredStr);
        if (!triggerFlags)
            return false;

        handler->GetSession()->GetPlayer()->CastSpell(target, spell->Id, *triggerFlags);

        return true;
    }

    static bool HandleCastBackCommand(ChatHandler* handler, SpellInfo const* spell, Optional<std::string> triggeredStr)
    {
        Creature* caster = handler->getSelectedCreature();
        if (!caster)
        {
            handler->SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!CheckSpellExistsAndIsValid(handler, spell))
            return false;

        Optional<TriggerCastFlags> triggerFlags = GetTriggerFlags(triggeredStr);
        if (!triggerFlags)
            return false;

        caster->CastSpell(handler->GetSession()->GetPlayer(), spell->Id, *triggerFlags);

        return true;
    }

    static bool HandleCastDistCommand(ChatHandler* handler, SpellInfo const* spell, float dist, Optional<std::string> triggeredStr)
    {
        if (!CheckSpellExistsAndIsValid(handler, spell))
            return false;

        Optional<TriggerCastFlags> triggerFlags = GetTriggerFlags(triggeredStr);
        if (!triggerFlags)
            return false;

        float x, y, z;
        handler->GetSession()->GetPlayer()->GetClosePoint(x, y, z, dist);
        handler->GetSession()->GetPlayer()->CastSpell(Position{ x, y, z }, spell->Id, *triggerFlags);

        return true;
    }

    static bool HandleCastSelfCommand(ChatHandler* handler, SpellInfo const* spell, Optional<std::string> triggeredStr)
    {
        Unit* target = handler->getSelectedUnit();
        if (!target)
        {
            handler->SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!CheckSpellExistsAndIsValid(handler, spell))
            return false;

        Optional<TriggerCastFlags> triggerFlags = GetTriggerFlags(triggeredStr);
        if (!triggerFlags)
            return false;

        target->CastSpell(target, spell->Id, *triggerFlags);

        return true;
    }

    static bool HandleCastTargetCommad(ChatHandler* handler, SpellInfo const* spell, Optional<std::string> triggeredStr)
    {
        Creature* caster = handler->getSelectedCreature();
        if (!caster)
        {
            handler->SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!caster->GetVictim())
        {
            handler->SendSysMessage(LANG_SELECTED_TARGET_NOT_HAVE_VICTIM);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!CheckSpellExistsAndIsValid(handler, spell))
            return false;

        Optional<TriggerCastFlags> triggerFlags = GetTriggerFlags(triggeredStr);
        if (!triggerFlags)
            return false;

        caster->CastSpell(caster->GetVictim(), spell->Id, *triggerFlags);

        return true;
    }

    static bool HandleCastDestCommand(ChatHandler* handler, SpellInfo const* spell, float x, float y, float z, Optional<std::string> triggeredStr)
    {
        Unit* caster = handler->getSelectedUnit();
        if (!caster)
        {
            handler->SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!CheckSpellExistsAndIsValid(handler, spell))
            return false;

        Optional<TriggerCastFlags> triggerFlags = GetTriggerFlags(triggeredStr);
        if (!triggerFlags)
            return false;

        caster->CastSpell(Position{ x, y, z }, spell->Id, *triggerFlags);

        return true;
    }

     // Événement bas-niveau exécuté par Unit::m_Events
    class CastListEvent : public BasicEvent
    {
        public:
            // Constructeur
            CastListEvent(Player* player, uint64 delay, std::vector<uint32> spells, size_t index = 0)
                : m_player(player), m_delay(delay), m_spells(std::move(spells)), m_index(index)
            {
            }

            // Exécution dans le thread monde
            bool Execute(uint64 /*execTime*/, uint32 /*diff*/) override
            {
                // Garde : caster encore valide et en vie
                if (!m_player || !m_player->IsAlive())
                {
                    return true; // on consomme l'event, pas de replanif
                }

                // Garde : index dans la liste
                if (m_index >= m_spells.size())
                {
                    return true; // séquence terminée
                }

                uint32 spellId = m_spells[m_index];

                // Par sécurité, revalider le spell (hot reload éventuel)
                if (SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE))
                {
                    // Suppresssion des auras
                    m_player->RemoveAllAuras();
                    // Cast sur soi-même, triggered pour éviter GCD/conditions inutiles
                    m_player->CastSpell(m_player, info->Id, true);
                    // Message au joueur
                    ChatHandler(m_player->GetSession()).PSendSysMessage("Cast spell with id: %u (duration: %u)", info->Id, info->CalcDuration());
                }

                // Prochaine étape
                ++m_index;

                if (m_index < m_spells.size() && m_player->IsAlive())
                {
                    // Replanifier le même event
                    m_player->m_Events.AddEvent(this, m_player->m_Events.CalculateTime(Milliseconds(m_delay)));
                    // Suivant
                    return false;
                }

                ChatHandler(m_player->GetSession()).SendSysMessage("Sequence over!");

                // Consommé
                return true;
            }

        private:
            // Pas de XML ici (private) → simple //
            // Pointeur détenu par le monde; l’EventProcessor annule les events à la destruction du Unit
            Player* m_player = nullptr;
            uint64 m_delay;
            std::vector<uint32> m_spells;
            size_t m_index = 0;
    };

    static bool HandleCastListCommand(ChatHandler* handler, uint64 delay, std::vector<uint32> spellIds)
    {
        Player* player = handler->getSelectedPlayer();
        if (!player)
        {
            handler->SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (spellIds.empty())
        {
            handler->SendSysMessage("Syntax: .cast list #spellid1 [#spellid2 ... #spellidN]");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // Valider tous les sorts AVANT de programmer quoi que ce soit
        for (uint32 id : spellIds)
        {
            if (!sSpellMgr->GetSpellInfo(id, DIFFICULTY_NONE))
            {
                handler->PSendSysMessage("Invalid spell id: %u", id);
                handler->SetSentErrorMessage(true);
                return false;
            }
        }

        // Programmer la séquence : on pousse simplement le premier event maintenant
        // les suivants se reprogrammeront eux-mêmes
        player->m_Events.AddEvent(new CastListEvent(player, delay, spellIds, 0), player->m_Events.CalculateTime(1s));

        // Message d’aide (en live, pas dans un thread. handler est valide ici, point.)
        handler->PSendSysMessage("Scheduled %zu spells on selected target. One every %u milliseconds.", spellIds.size(), delay);
        return true;
    }

    static bool HandleCastStopCommand(ChatHandler* handler)
    {
        Player* player = handler->getSelectedPlayer();
        if (!player)
        {
            handler->SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // Tue tous les events de type CastList
        player->m_Events.KillAllEvents(true);
        handler->SendSysMessage("Stopped cast list events on target.");
        return true;
    }
};

void AddSC_cast_commandscript()
{
    new cast_commandscript();
}
