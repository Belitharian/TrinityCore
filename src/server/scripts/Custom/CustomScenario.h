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

#include "Creature.h"
#include "CreatureTextMgr.h"
#include "Define.h"
#include "Duration.h"
#include "GameTime.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Optional.h"
#include "Player.h"
#include "SceneMgr.h"
#include "ScriptMgr.h"
#include <algorithm>
#include <charconv>
#include <functional>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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

    // TEMPS DE PAROLE
    //
    // Les cinematiques enchainent leurs repliques avec des delais ecrits a la
    // main (Next(8s)...). Des qu'un son est regenere, sa duree change et le
    // delai coupe la replique. DialogueClock fait du delai ecrit un minimum :
    // l'event suivant attend au moins la fin de la replique qui vient d'etre
    // dite, d'apres la colonne Duration de creature_text (en ms, renseignee
    // par _Tools/UpdateCreatureTextDurations.ps1 a partir des mp3).
    //
    //   void Talk(Creature* c, uint8 id)    { dialogue.Say(c, id); }
    //   void Next(Milliseconds const& time) { ...ScheduleEvent(eventId, dialogue.Consume(time)); }
    //   Update() : dialogue.Reset() avant d'executer l'event
    struct DialogueClock
    {
        // Respiration laissee entre deux repliques.
        static constexpr Milliseconds Pause = 700ms;

        void Say(Creature* creature, uint8 textGroup)
        {
            if (!creature)
                return;

            // CreatureAI::Talk ne fait que ce SendChat : on garde la duree.
            if (uint32 duration = sCreatureTextMgr->SendChat(creature, textGroup))
                _pending = std::max(_pending, Milliseconds(duration) + Pause);
        }

        // Delai a utiliser pour l'event suivant : le plus long entre le
        // delai ecrit et le temps de parole en cours.
        Milliseconds Consume(Milliseconds scripted)
        {
            Milliseconds const delay = std::max(scripted, _pending);
            _pending = 0ms;
            return delay;
        }

        // Un event sans Next() ne doit pas allonger l'enchainement suivant.
        void Reset() { _pending = 0ms; }

    private:
        Milliseconds _pending = 0ms;
    };

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

    // SCENES DE DIALOGUE
    //
    // Les dialogues scriptes sont joues par des scenes client (SceneScript
    // Lua, voir sql/custom/scenes) : la scene fait apparaitre ses propres
    // acteurs, qui parlent en bulle, se regardent, marchent et lancent leurs
    // visuels. Le serveur ne fait plus que trois choses :
    //   - au signal "Ready", masquer les vrais PNJ que les acteurs remplacent ;
    //   - aux signaux propres a la scene, agir sur le monde (portail, barriere...) ;
    //   - quand tous les joueurs ont fini ("Done", fin ou annulation de la
    //     scene), remettre les vrais PNJ dans l'etat ou la scene les laisse et
    //     reprendre le scenario.
    // Chaque signal n'est traite qu'une fois, quel que soit le nombre de
    // joueurs qui regardent la scene.
    //
    //   static constexpr char const* HelpCues[] = { "Portal", "ClosePortal" };
    //   static constexpr CustomScenario::DialogueScene HelpScene = { SCENE_HELP, HelpCues, 5min };
    //
    //   scenes.Play(instance, HelpScene, [this](std::string_view cue) { ... }, [this]() { ... }, skip);
    //
    // Avec skip (CUSTOM_DEBUG), la scene n'est pas jouee : ses signaux sont
    // rejoues dans l'ordre de DialogueScene::cues, puis vient la fin.
    namespace SceneCue
    {
        constexpr std::string_view Ready = "Ready";
        constexpr std::string_view Done  = "Done";
        constexpr std::string_view Step  = "Step";  // "Step<n>" : n Next() passes (framework.lua)
    }

    struct DialogueScene
    {
        uint32 sceneId;                             // scene_template
        std::span<char const* const> cues;          // signaux propres a la scene, dans l'ordre
        Milliseconds timeout;                       // filet si un client ne repond plus
    };

    class DialogueSceneDirector
    {
    public:
        using CueHandler = std::function<void(std::string_view cue)>;
        using EndHandler = std::function<void()>;

        // World state lu par la scene a son debut (RESUME_WORLD_STATE de
        // framework.lua) : le nombre de Next() a passer en accelere. 0 pour
        // une scene jouee depuis le debut.
        static constexpr uint32 ResumeWorldState = 900001;

        // Joue la scene a tous les joueurs de la carte. Sans spectateur (skip,
        // instance vide), elle se deroule d'un coup.
        void Play(Map* map, DialogueScene const& scene, CueHandler onCue, EndHandler onEnd, bool skip)
        {
            if (_running.contains(scene.sceneId))
                return;

            Running running;
            running.onCue = std::move(onCue);
            running.onEnd = std::move(onEnd);
            running.remaining = scene.timeout;
            running.timeout = scene.timeout;

            if (!skip && map)
            {
                map->DoOnPlayers([&running, &scene](Player* player)
                {
                    player->SendUpdateWorldState(ResumeWorldState, 0);
                    if (player->GetSceneMgr().PlayScene(scene.sceneId))
                        running.pending.insert(player->GetGUID());
                });
            }

            if (running.pending.empty())
            {
                for (char const* cue : scene.cues)
                    if (running.onCue)
                        running.onCue(cue);
                if (running.onEnd)
                    running.onEnd();
                return;
            }

            _running.emplace(scene.sceneId, std::move(running));
        }

        void OnCue(Player* player, uint32 sceneId, std::string_view cue)
        {
            auto itr = _running.find(sceneId);
            if (itr == _running.end())
                return;

            if (cue == SceneCue::Done)
            {
                OnPlayerDone(player, sceneId);
                return;
            }

            // Point de reprise : le dernier Next() passe.
            if (cue.starts_with(SceneCue::Step))
            {
                uint32 step = 0;
                std::string_view const number = cue.substr(SceneCue::Step.size());
                std::from_chars(number.data(), number.data() + number.size(), step);
                itr->second.step = std::max(itr->second.step, step);
                return;
            }

            if (!itr->second.fired.emplace(cue).second)
                return;

            // Copie : le signal peut terminer ou annuler la scene elle-meme.
            CueHandler handler = itr->second.onCue;
            if (handler)
                handler(cue);
        }

        // "Done", fin ou annulation de la scene pour ce joueur.
        void OnPlayerDone(Player* player, uint32 sceneId)
        {
            auto itr = _running.find(sceneId);
            if (itr == _running.end())
                return;

            itr->second.pending.erase(player->GetGUID());
            if (itr->second.pending.empty())
                Finish(sceneId);
        }

        // Un joueur qui quitte l'instance ne doit pas bloquer la suite des
        // autres. S'il etait le dernier sur la carte (deconnexion en solo),
        // la scene reste en cours : a son retour, OnPlayerEnter la lui
        // rejoue. Personne ne revient : le timeout la termine.
        void OnPlayerLeave(Player* player)
        {
            // Le joueur compte encore parmi ceux de la carte.
            bool const othersOnMap = player->GetMap()->GetPlayers().size() > 1;
            std::vector<uint32> finished;
            for (auto& [sceneId, running] : _running)
                if (running.pending.erase(player->GetGUID()) && running.pending.empty() && othersOnMap)
                    finished.push_back(sceneId);

            for (uint32 sceneId : finished)
                Finish(sceneId);
        }

        // Un joueur arrive (reconnexion, ou entree en cours de scene) : la
        // scene reprend pour lui au dernier Next() passe, ce qui precede
        // defilant en accelere (ResumeWorldState). Sans elle, les vrais PNJ,
        // deja masques au signal "Ready", seraient invisibles pour lui. Les
        // signaux deja traites ne sont pas rejoues cote serveur.
        void OnPlayerEnter(Player* player)
        {
            for (auto& [sceneId, running] : _running)
            {
                if (running.pending.contains(player->GetGUID()))
                    continue;

                player->SendUpdateWorldState(ResumeWorldState, running.step);
                if (!player->GetSceneMgr().PlayScene(sceneId))
                    continue;

                running.pending.insert(player->GetGUID());
                running.remaining = running.timeout;
            }
        }

        // Coupe la scene chez tous les joueurs et passe directement a la fin.
        void Cancel(Map* map, uint32 sceneId)
        {
            auto itr = _running.find(sceneId);
            if (itr == _running.end())
                return;

            for (ObjectGuid const& guid : itr->second.pending)
                if (Player* player = ObjectAccessor::GetPlayer(map, guid))
                    player->GetSceneMgr().CancelSceneBySceneId(sceneId);

            Finish(sceneId);
        }

        void Update(Map* map, Milliseconds diff)
        {
            std::vector<uint32> expired;
            for (auto& [sceneId, running] : _running)
            {
                running.remaining -= diff;
                if (running.remaining <= 0ms)
                    expired.push_back(sceneId);
            }

            for (uint32 sceneId : expired)
                Cancel(map, sceneId);
        }

        bool IsPlaying(uint32 sceneId) const { return _running.contains(sceneId); }

        // Une scene se joue, ou s'est terminee il y a moins de `margin`.
        bool IsBusy(Milliseconds margin) const
        {
            return !_running.empty() || GameTime::Now() < _lastFinish + margin;
        }

    private:
        struct Running
        {
            CueHandler onCue;
            EndHandler onEnd;
            GuidUnorderedSet pending;               // joueurs qui regardent encore
            std::unordered_set<std::string> fired;  // signaux deja traites
            Milliseconds remaining = 0ms;
            Milliseconds timeout = 0ms;             // duree complete, rendue a chaque joueur qui la rejoint
            uint32 step = 0;                        // dernier "Step<n>" recu : point de reprise
        };

        // La scene est retiree avant l'appel : la fin peut en lancer une autre.
        void Finish(uint32 sceneId)
        {
            auto node = _running.extract(sceneId);
            if (node.empty())
                return;

            _lastFinish = GameTime::Now();
            if (node.mapped().onEnd)
                node.mapped().onEnd();
        }

        std::unordered_map<uint32, Running> _running;
        TimePoint _lastFinish = {};
    };

    // VOIX D'AMBIANCE
    //
    // Les PNJ d'ambiance (crieur, civils...) parlent chacun de leur cote : ils
    // finissent par se couvrir, et par couper les scenes de dialogue, que le
    // client joue en stereo. AmbientVoices ne laisse passer qu'une voix a la
    // fois a portee d'oreille, et aucune pendant une scene ni juste apres.
    // Une voix refusee n'est pas mise en attente : a l'appelant de reessayer
    // ou de s'en passer.
    //
    //   if (Optional<Milliseconds> duration = CustomScenario::AmbientVoices::Talk(me, SAY_X, 4s))
    //       ...; // parle pendant *duration, puis 4 s de silence autour de lui
    class AmbientVoices
    {
    public:
        // Deux voix plus proches que ca se couvrent.
        static constexpr float HearingRange = 50.0f;
        // Silence laisse apres une scene de dialogue.
        static constexpr Milliseconds SceneMargin = 1500ms;

        explicit AmbientVoices(DialogueSceneDirector const& scenes) : _scenes(scenes) { }

        // `range` : portee de la voix qui demande a parler. Deux voix se
        // couvrent quand l'une porte jusqu'a l'autre.
        bool CanTalk(WorldObject const* speaker, float range = HearingRange)
        {
            if (_scenes.IsBusy(SceneMargin))
                return false;

            TimePoint const now = GameTime::Now();
            std::erase_if(_voices, [now](Voice const& voice) { return voice.until <= now; });
            return std::none_of(_voices.begin(), _voices.end(), [speaker, range](Voice const& voice)
            {
                return speaker->GetExactDist(voice.position) < std::max(range, voice.range);
            });
        }

        // Fait dire au PNJ une replique du groupe si personne ne parle a
        // portee. `gap` : silence impose autour de lui apres sa replique.
        // `range` : jusqu'ou porte sa voix (Everywhere pour un PNJ qu'on
        // entend dans toute la zone, comme le crieur).
        // Renvoie la duree de la replique, rien si elle est refusee.
        Optional<Milliseconds> TryTalk(Creature* speaker, uint8 textGroup, Milliseconds gap, float range = HearingRange)
        {
            if (!CanTalk(speaker, range))
                return {};

            Milliseconds const duration(sCreatureTextMgr->SendChat(speaker, textGroup));
            _voices.push_back({ speaker->GetPosition(), range, GameTime::Now() + duration + gap });
            return duration;
        }

        // Comme TryTalk, depuis l'instance du PNJ. Hors d'une instance qui
        // arbitre ses voix, la replique est toujours dite.
        static Optional<Milliseconds> Talk(Creature* speaker, uint8 textGroup, Milliseconds gap, float range = HearingRange);

        static constexpr float Everywhere = std::numeric_limits<float>::max();

    private:
        struct Voice
        {
            Position position;
            float range;
            TimePoint until;
        };

        DialogueSceneDirector const& _scenes;
        std::vector<Voice> _voices;
    };

    // Implementee par l'InstanceScript qui joue des scenes de dialogue.
    class DialogueSceneHost
    {
    public:
        virtual DialogueSceneDirector& GetDialogueScenes() = 0;

    protected:
        ~DialogueSceneHost() = default;
    };

    // Implementee par l'InstanceScript qui arbitre les voix d'ambiance.
    class AmbientVoiceHost
    {
    public:
        virtual AmbientVoices& GetAmbientVoices() = 0;

    protected:
        ~AmbientVoiceHost() = default;
    };

    inline Optional<Milliseconds> AmbientVoices::Talk(Creature* speaker, uint8 textGroup, Milliseconds gap, float range)
    {
        if (AmbientVoiceHost* host = dynamic_cast<AmbientVoiceHost*>(speaker->GetInstanceScript()))
            return host->GetAmbientVoices().TryTalk(speaker, textGroup, gap, range);

        return Milliseconds(sCreatureTextMgr->SendChat(speaker, textGroup));
    }

    // SceneScript commun a toutes les scenes de dialogue (scene_template.ScriptName) :
    // il relaie les signaux et la fin de scene au DialogueSceneDirector de l'instance.
    class DialogueSceneScript : public SceneScript
    {
    public:
        explicit DialogueSceneScript(char const* name) : SceneScript(name) { }

        void OnSceneTriggerEvent(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* sceneTemplate, std::string const& triggerName) override
        {
            if (DialogueSceneDirector* scenes = GetDirector(player))
                scenes->OnCue(player, sceneTemplate->SceneId, triggerName);
        }

        void OnSceneCancel(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* sceneTemplate) override
        {
            if (DialogueSceneDirector* scenes = GetDirector(player))
                scenes->OnPlayerDone(player, sceneTemplate->SceneId);
        }

        void OnSceneComplete(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* sceneTemplate) override
        {
            if (DialogueSceneDirector* scenes = GetDirector(player))
                scenes->OnPlayerDone(player, sceneTemplate->SceneId);
        }

    private:
        static DialogueSceneDirector* GetDirector(Player* player)
        {
            DialogueSceneHost* host = dynamic_cast<DialogueSceneHost*>(player->GetInstanceScript());
            return host ? &host->GetDialogueScenes() : nullptr;
        }
    };
}

#endif // CUSTOM_SCENARIO_H
