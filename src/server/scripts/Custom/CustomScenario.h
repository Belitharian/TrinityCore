/*
 * Outils communs aux scenarios custom.
 *
 * AURAS PILOTEES PAR LA PHASE
 *
 * Un scenario a presque toujours des auras qui ne valent que pendant une
 * tranche de phases : un buff d'etape, une illusion, une skybox. Sans support
 * dedie, chaque scenario finit par les poser a la main dans ses criteria trees,
 * les retirer ailleurs, et les reposer dans OnPlayerEnter pour couvrir les
 * reconnexions - trois endroits qui divergent des que la sequence bouge.
 *
 * Cote donnees, rien ne permet de faire ca : ScenarioStep.db2 n'a aucun champ
 * de sort, PlayerCondition.db2 ne sait que *tester* une aura, et spell_area ne
 * connait ni scenario ni phase. La logique appartient donc au serveur - c'est
 * aussi le cas chez Blizzard, dans du code qui n'est pas distribue.
 *
 * Ce fichier centralise le motif : chaque scenario declare une table
 * "aura <-> tranche de phases" et appelle Sync a deux endroits.
 *
 *   static constexpr CustomScenario::PhaseAura MyAuras[] =
 *   {
 *       { SPELL_BUFF,     (uint32)MyPhases::Etape02, (uint32)MyPhases::Etape05 },
 *       { SPELL_AMBIANCE, 0,                         CustomScenario::PhaseAura::ToEnd }
 *   };
 *
 *   void OnPlayerEnter(Player* player) override
 *   {
 *       CustomScenario::SyncPhaseAuras(player, (uint32)phase, MyAuras);
 *   }
 *
 *   void OnPlayerLeave(Player* player) override
 *   {
 *       CustomScenario::RemovePhaseAuras(player, MyAuras);
 *   }
 *
 *   // dans SetData(DATA_SCENARIO_PHASE, value)
 *   phase = (MyPhases)value;
 *   CustomScenario::SyncPhaseAuras(instance, (uint32)phase, MyAuras);
 *
 * Commentaires en francais sans accents (encodage TC).
 */

#ifndef CUSTOM_SCENARIO_H
#define CUSTOM_SCENARIO_H

#include "Define.h"
#include "Map.h"
#include "Player.h"
#include <limits>
#include <span>

namespace CustomScenario
{
    // Qui est responsable de la pose de l'aura.
    enum class PhaseAuraMode : uint8
    {
        // La tranche de phases fait foi : l'aura est posee en entrant dans la
        // tranche et retiree en en sortant. C'est le cas par defaut.
        Managed,

        // La cinematique pose l'aura elle-meme, au moment precis qui convient
        // (apres un fondu, un teleport...). La tranche ne sert alors qu'a la
        // restaurer pour un joueur qui arrive ou se reconnecte en plein milieu :
        // un changement de phase ne la pose ni ne la retire jamais.
        RestoreOnly
    };

    // Association d'un sort a une tranche de phases [from, to[.
    //
    // Les bornes sont des uint32 et non l'enum de phases du scenario : chaque
    // scenario a le sien, et ils transitent deja en uint32 via SetData /
    // GetData(DATA_SCENARIO_PHASE). Les enums de phases etant ordonnes par
    // progression, une tranche se lit directement dans la declaration.
    struct PhaseAura
    {
        // Borne haute pour une aura valable jusqu'a la fin du scenario.
        static constexpr uint32 ToEnd = std::numeric_limits<uint32>::max();

        uint32 spellId;
        uint32 from;                                // phase incluse
        uint32 to;                                  // phase exclue
        PhaseAuraMode mode = PhaseAuraMode::Managed;

        bool CoversPhase(uint32 phase) const { return phase >= from && phase < to; }
    };

    using PhaseAuraTable = std::span<PhaseAura const>;

    // Aligne les auras d'un joueur sur la phase courante.
    //
    // A appeler depuis OnPlayerEnter : couvre aussi bien le joueur qui rejoint
    // une instance deja lancee que celui qui se reconnecte. Les deux modes sont
    // poses ; seul Managed est retire hors de sa tranche, une aura RestoreOnly
    // pouvant avoir ete retiree volontairement par la cinematique.
    inline void SyncPhaseAuras(Player* player, uint32 phase, PhaseAuraTable auras)
    {
        if (!player)
            return;

        for (PhaseAura const& entry : auras)
        {
            if (entry.CoversPhase(phase))
            {
                if (!player->HasAura(entry.spellId))
                    player->CastSpell(player, entry.spellId, true);
            }
            else if (entry.mode == PhaseAuraMode::Managed)
                player->RemoveAurasDueToSpell(entry.spellId);
        }
    }

    // Aligne les auras Managed de tous les joueurs de l'instance.
    //
    // A appeler depuis SetData(DATA_SCENARIO_PHASE) : c'est le seul entonnoir
    // par lequel passe un changement de phase, donc le seul endroit ou poser et
    // retirer. Les auras RestoreOnly sont volontairement ignorees ici pour ne
    // pas court-circuiter le minutage d'une cinematique.
    inline void SyncPhaseAuras(Map* map, uint32 phase, PhaseAuraTable auras)
    {
        if (!map)
            return;

        map->DoOnPlayers([phase, auras](Player* player)
        {
            for (PhaseAura const& entry : auras)
            {
                if (entry.mode != PhaseAuraMode::Managed)
                    continue;

                if (entry.CoversPhase(phase))
                {
                    if (!player->HasAura(entry.spellId))
                        player->CastSpell(player, entry.spellId, true);
                }
                else
                    player->RemoveAurasDueToSpell(entry.spellId);
            }
        });
    }

    // Retire toutes les auras de la table, quel que soit le mode.
    // A appeler depuis OnPlayerLeave : rien de tout ceci ne doit suivre le
    // joueur hors de l'instance.
    inline void RemovePhaseAuras(Player* player, PhaseAuraTable auras)
    {
        if (!player)
            return;

        for (PhaseAura const& entry : auras)
            player->RemoveAurasDueToSpell(entry.spellId);
    }
}

#endif // CUSTOM_SCENARIO_H
