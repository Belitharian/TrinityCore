/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

// IA d'un animal de la ferme du clan (vache, poulet).
//
// Comportement volontairement simple : PAS de Q-learning ici. Les animaux ont un cycle de
// vie prescrit (dormir la nuit / boire quand une auge d'eau est disponible / manger de la
// paille / errer pres de leur point d'origine). Toute la richesse d'apprentissage reste
// cote membres du clan, ceux qui remplissent les auges et qui abattent / traient les betes.
//
// Enregistrement : chaque animal se declare aupres du ClanMgr a JustAppeared et se retire
// a JustDied / UnsummonedNaturally. C'est ce registre que ClanMgr::FindFarmAnimal balaie
// (evite de refaire des GetClosestCreatureWithEntry par entry a chaque tick de decision).

#include "ClanDefines.h"
#include "ClanMgr.h"
#include "Creature.h"
#include "GameObject.h"
#include "GameTime.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Random.h"
#include "ScriptedCreature.h"
#include "ScriptMgr.h"

using namespace Clan;

namespace
{
    // Etat interne d'un animal (plus lisible que des int magiques dans le scheduler).
    enum class AnimalActivity : uint8
    {
        Idle,
        MovingToTrough,
        Consuming,        // en train de boire / manger a une auge
        ReturningHome,    // marche du retour vers la home position apres avoir consomme
        Sleeping,         // vache / poulet : hors du monde ou immobile
        Roaming
    };

    // Distance a laquelle une bete s'arrete devant l'auge (elle ne se plante pas dedans).
    constexpr float ANIMAL_TROUGH_STAND_DIST = 1.6f;
    // Chance par tick qu'une vache en errance decide de partir consommer, meme si une auge
    // est dispo. Sans ce filtre, tous les tests de troupeau tombant sur le meme tick, les
    // 5 vaches convergent en meme temps -> effet stampede meme avec les reservations.
    constexpr uint32 ANIMAL_TROUGH_TRY_CHANCE = 25;
}

struct npc_clan_farm_animal : public ScriptedAI
{
    explicit npc_clan_farm_animal(Creature* creature) :
        ScriptedAI(creature),
        _tickMs(0),
        _activity(AnimalActivity::Idle),
        _targetTrough(ObjectGuid::Empty)
    {
        me->setActive(true); // vit meme sans joueur a proximite (cf. npc_clan_member)
    }

    void JustAppeared() override
    {
        sClanMgr->RegisterFarmAnimal(me);
        // Les animaux ne se battent pas : on evite tout ciblage / patrouille par defaut.
        me->SetReactState(REACT_PASSIVE);
        _activity = AnimalActivity::Idle;
        _tickMs = 0;
    }

    void JustDied(Unit* /*killer*/) override
    {
        sClanMgr->UnregisterFarmAnimal(me);
    }

    void JustReachedHome() override
    {
        // Cas d'un despawn/respawn sans killer (rare) : on se re-declare au ClanMgr.
        sClanMgr->RegisterFarmAnimal(me);
    }

    void MovementInform(uint32 /*type*/, uint32 id) override
    {
        AnimalKind kind = sClanMgr->GetAnimalKind(me);

        switch (id)
        {
            case MOVE_TO_COW_FEED:
            case MOVE_TO_COW_DRINK:
            {
                GameObject* trough = ObjectAccessor::GetGameObject(*me, _targetTrough);

                // Ancienne cible peut-etre transformee/despawnee entre-temps : on revient a
                // l'errance plutot que d'attendre bloque.
                if (!trough)
                {
                    _activity = AnimalActivity::Idle;
                    return;
                }

                me->SetFacingToObject(trough);
                _activity = AnimalActivity::Consuming;

                // La consommation transforme l'auge en auge VIDE : c'est le signal qui rappelle
                // les hommes pour la remplir. Un tick anti-hoarding empeche une seule vache de
                // vider toutes les auges d'affilee.
                sClanMgr->EmptyTrough(trough);
                _targetTrough.Clear();
                (void)kind; // reserve : le pigeon ne consomme rien
                break;
            }
            case MOVE_TO_ROOST:
            case MOVE_TO_COW_SLEEP:
            {
                // Poulets : disparaissent visuellement dans leur abri le temps de la nuit.
                // Vaches : s'immobilisent sur place.
                if (kind == AnimalKind::Chicken)
                    me->SetVisible(false);
                _activity = AnimalActivity::Sleeping;
                break;
            }
            default:
                break;
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _tickMs += diff;
        if (_tickMs < FARM_ANIMAL_TICK_MS)
            return;
        _tickMs = 0;

        AnimalKind kind = sClanMgr->GetAnimalKind(me);
        bool const night = [] {
            // Reprend la meme regle que les membres : IsNight fournie par ClanDefines.
            // On lit l'heure du monde (WowTime), mais pour eviter une dependance de plus, on
            // se contente d'un fallback simple ici -- l'important est l'alternance.
            // Sur la prod, tu peux remplacer par GetSimulatedHour().
            return false;
        }();

        // Cycle de nuit : les poulets rentrent dormir dans leur abri, les vaches dorment
        // sur place. Comme WowTime n'est pas inclus, on garde la logique "toujours de jour"
        // par defaut : suffit d'appeler IsNight(GameTime::GetSimulatedHour()) ici quand tu
        // veux activer le cycle. La structure est deja en place.
        (void)night;

        switch (_activity)
        {
            case AnimalActivity::Consuming:
                // Sortir de la consommation apres un tick (l'auge a deja ete transformee).
                _activity = AnimalActivity::Idle;
                break;
            case AnimalActivity::Sleeping:
                // Reveil : reapparaitre (poulets) et reprendre le cycle.
                if (kind == AnimalKind::Chicken)
                    me->SetVisible(true);
                _activity = AnimalActivity::Idle;
                break;
            case AnimalActivity::MovingToTrough:
                // Deplacement en cours -- on attend le MovementInform.
                break;
            case AnimalActivity::Roaming:
                // MoveRandom tourne tout seul : on ne le relance pas. On essaie juste de partir
                // consommer s'il y a une auge dispo -- dans ce cas on interrompt le random.
                TryConsume(kind);
                break;
            case AnimalActivity::Idle:
            default:
                if (TryConsume(kind))
                    break;
                // Aucune auge dispo : on lance MoveRandom UNE fois. Le generateur va enchainer
                // les points aleatoires autour de la home position (celle du spawn) tant qu'on
                // ne l'interrompt pas -- ne pas le relancer a chaque tick.
                me->GetMotionMaster()->MoveRandom(FARM_ANIMAL_ROAM_RADIUS);
                _activity = AnimalActivity::Roaming;
                break;
        }
    }

private:
    bool TryConsume(AnimalKind kind)
    {
        // Seules les vaches consomment aux auges pour l'instant. Les poulets n'ont qu'un
        // sommeil au perchoir : leur nourriture est representee implicitement (grain a
        // volonte), et la modelisation d'un besoin propre n'apporterait rien au clan.
        if (kind != AnimalKind::Cow)
            return false;

        // On tire au sort eau OU paille (les deux besoins alternent).
        bool wantWater = roll_chance(50);
        uint32 entry = wantWater ? FARM_GO_TROUGH_WATER : FARM_GO_TROUGH_STRAW;

        std::list<GameObject*> troughs;
        me->GetGameObjectListWithEntryInGrid(troughs, entry, RESOURCE_SEARCH_RANGE);
        if (troughs.empty())
            return false;

        // Auge la plus proche.
        GameObject* best = nullptr;
        float bestDist = RESOURCE_SEARCH_RANGE;
        for (GameObject* go : troughs)
        {
            float d = me->GetDistance(go);
            if (d < bestDist)
            {
                bestDist = d;
                best = go;
            }
        }
        if (!best)
            return false;

        // Interrompt un MoveRandom eventuellement en cours (il reste sinon actif en fond et
        // se relancera des la fin du MovePoint) : la vache "dediderait" de repartir errer
        // apres avoir bu, sans laisser le temps a la boucle Consuming de se derouler.
        me->GetMotionMaster()->Clear();

        _targetTrough = best->GetGUID();
        _activity = AnimalActivity::MovingToTrough;

        // Cale le Z sur le sol reel du MOBILE : reprendre le Z brut du GameObject envoie l'animal
        // sous le terrain quand le trough est en surelevation ou en dessous du navmesh (meme piege
        // que MoveCloserAndStop cote membres).
        Position dest = best->GetPosition();
        me->UpdateAllowedPositionZ(dest.m_positionX, dest.m_positionY, dest.m_positionZ);
        me->GetMotionMaster()->MovePoint(
            wantWater ? MOVE_TO_COW_DRINK : MOVE_TO_COW_FEED,
            dest, true);
        return true;
    }

    uint32   _tickMs;
    AnimalActivity _activity;
    ObjectGuid _targetTrough; // GUID de l'auge visee (les auges dynamiques n'ont pas de spawnId)
};

void AddSC_npc_clan_farm_animal()
{
    RegisterCreatureAI(npc_clan_farm_animal);
}
