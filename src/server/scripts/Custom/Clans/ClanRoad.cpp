#include "ClanRoad.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "PathGenerator.h"
#include "Unit.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace Clan
{
    namespace Road
    {
        namespace
        {
            struct PathData
            {
                uint32                Id = 0;
                uint32                MapId = 0;
                std::vector<Position> Nodes; // ordonnes par idx croissant
            };

            std::unordered_map<uint32, PathData>           _paths;         // pathId -> geometrie
            std::unordered_map<uint8, std::vector<uint32>> _pathsByAction; // ActionType -> pathIds

            // Index du noeud DECLARE le plus proche de 'pos' (a vol d'oiseau, en 2D).
            //
            // On ne projette volontairement PAS sur les segments : un point interpole en plein
            // milieu d'un troncon fait quitter la route entre deux noeuds, et le PNJ coupe alors
            // a travers le decor pour rejoindre sa destination. En se limitant aux noeuds
            // declares, il longe la route jusqu'a un point que tu as choisi toi-meme.
            //
            // 2D volontairement : le Z suit le relief et fausserait la comparaison en pente.
            uint32 NearestNode(PathData const& path, Position const& pos)
            {
                uint32 best = 0;
                float bestDist = std::numeric_limits<float>::max();

                for (uint32 i = 0; i < path.Nodes.size(); ++i)
                {
                    float const dist = pos.GetExactDist2d(path.Nodes[i]);
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        best = i;
                    }
                }

                return best;
            }

            // Indices des noeuds tries par distance croissante a 'pos'. Preselection avant
            // l'evaluation navmesh, trop chere pour etre appliquee a tous les noeuds.
            std::vector<uint32> NodesByDistance(PathData const& path, Position const& pos)
            {
                std::vector<uint32> indices(path.Nodes.size());
                for (uint32 i = 0; i < indices.size(); ++i)
                    indices[i] = i;

                std::sort(indices.begin(), indices.end(), [&](uint32 l, uint32 r)
                {
                    return pos.GetExactDist2d(path.Nodes[l]) < pos.GetExactDist2d(path.Nodes[r]);
                });

                return indices;
            }

            // Distance parcourue LE LONG de la route entre deux noeuds (et non a vol d'oiseau).
            float LengthBetweenNodes(PathData const& path, uint32 from, uint32 to)
            {
                if (from > to)
                    std::swap(from, to);

                float length = 0.0f;
                for (uint32 i = from; i < to; ++i)
                    length += path.Nodes[i].GetExactDist2d(path.Nodes[i + 1]);

                return length;
            }

            // Decalage lateral propre a un PNJ, dans [-LATERAL_OFFSET_MAX, +LATERAL_OFFSET_MAX].
            //
            // DETERMINISTE (derive du GUID) et non aleatoire : un meme membre garde ainsi
            // toujours le meme cote de la route d'un trajet a l'autre, ce qui se lit comme une
            // habitude plutot que comme du bruit.
            float LateralOffset(Unit const* mover)
            {
                uint32 const spread = uint32(mover->GetGUID().GetCounter() % 1000);
                return ((float(spread) / 999.0f) * 2.0f - 1.0f) * LATERAL_OFFSET_MAX;
            }

            // Ecarte chaque noeud perpendiculairement au SENS DE MARCHE.
            //
            // Le fait de se baser sur le sens de marche (et non sur une normale absolue) fait
            // que deux PNJ se croisant en sens inverse se decalent chacun de leur cote : le
            // comportement "on tient sa droite" apparait tout seul.
            void ApplyLateralOffset(std::vector<Position>& route, float offset)
            {
                if (route.size() < 2)
                    return;

                std::vector<Position> shifted = route;

                for (std::size_t i = 0; i < route.size(); ++i)
                {
                    // Direction locale : vers le noeud suivant ; pour le dernier, depuis le precedent.
                    std::size_t const a = (i + 1 < route.size()) ? i : i - 1;
                    std::size_t const b = (i + 1 < route.size()) ? i + 1 : i;

                    float const dx = route[b].GetPositionX() - route[a].GetPositionX();
                    float const dy = route[b].GetPositionY() - route[a].GetPositionY();
                    float const len = std::sqrt(dx * dx + dy * dy);
                    if (len < 0.01f) // noeuds confondus : pas de direction exploitable
                        continue;

                    // Perpendiculaire 2D au sens de marche.
                    shifted[i] = Position(route[i].GetPositionX() + (-dy / len) * offset,
                                          route[i].GetPositionY() + (dx / len) * offset,
                                          route[i].GetPositionZ());
                }

                route = std::move(shifted);
            }

            // Recale le Z de chaque noeud sur le sol reel.
            //
            // Deux raisons, et la premiere est structurelle :
            //  - le decalage lateral deplace X/Y mais conserve le Z du noeud declare : des que
            //    la route est en devers, le point se retrouve sous le terrain ;
            //  - un Z releve a la main en jeu est de toute facon approximatif.
            //
            // Et comme la route est parcourue SANS navmesh (ExactSplinePath), la spline suit ce
            // Z a la lettre : la moindre erreur fait passer le PNJ dans le sol. Ce recalage
            // n'est donc pas un confort, c'est ce qui rend le decalage lateral utilisable.
            void SnapToGround(Unit const* mover, std::vector<Position>& route)
            {
                for (Position& node : route)
                {
                    float z = node.GetPositionZ();
                    mover->UpdateAllowedPositionZ(node.GetPositionX(), node.GetPositionY(), z);
                    node.Relocate(node.GetPositionX(), node.GetPositionY(), z);
                }
            }

            // Longueur du chemin REELLEMENT parcourable entre 'mover' et 'dest', en contournant
            // le decor. Retourne -1 si la destination est injoignable.
            //
            // C'est la mesure qui remplace la distance a vol d'oiseau pour choisir ou rejoindre
            // la route : elle seule sait qu'un mur separe le PNJ d'un noeud pourtant "proche".
            float NavmeshDistance(Unit const* mover, Position const& dest)
            {
                PathGenerator generator(mover);
                generator.CalculatePath(dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ());

                // On ne teste pas la valeur de retour : elle vaut false quand le chemin n'avait
                // pas besoin d'etre recalcule, ce qui n'est pas une erreur. Seul le type fait foi.
                if ((generator.GetPathType() & PATHFIND_NOPATH) || generator.GetPath().size() < 2)
                    return -1.0f;

                return generator.GetPathLength();
            }
        }

        void Load()
        {
            _paths.clear();
            _pathsByAction.clear();

            // Geometrie des routes.
            if (QueryResult result = WorldDatabase.Query("SELECT id, map_id FROM custom_clan_path"))
            {
                do
                {
                    Field* f = result->Fetch();
                    uint32 const id = f[0].GetUInt32();

                    PathData& path = _paths[id];
                    path.Id = id;
                    path.MapId = f[1].GetUInt32();
                } while (result->NextRow());
            }

            // Noeuds. ORDER BY idx : l'ordre porte le sens de la polyligne, il est structurant.
            if (QueryResult result = WorldDatabase.Query(
                "SELECT path_id, pos_x, pos_y, pos_z FROM custom_clan_path_node ORDER BY path_id, idx"))
            {
                do
                {
                    Field* f = result->Fetch();
                    uint32 const pathId = f[0].GetUInt32();

                    auto itr = _paths.find(pathId);
                    if (itr == _paths.end())
                    {
                        TC_LOG_ERROR("server.loading", "custom_clan_path_node : path_id {} inconnu, noeud ignore.", pathId);
                        continue;
                    }

                    itr->second.Nodes.emplace_back(f[1].GetFloat(), f[2].GetFloat(), f[3].GetFloat());
                } while (result->NextRow());
            }

            // Liaison route <-> actions. Une meme route peut servir plusieurs actions (le vendeur
            // et le medecin sont sur la meme route) : c'est tout l'interet de la table separee,
            // la geometrie n'est declaree qu'une fois.
            if (QueryResult result = WorldDatabase.Query("SELECT path_id, action_type FROM custom_clan_path_action"))
            {
                do
                {
                    Field* f = result->Fetch();
                    uint32 const pathId = f[0].GetUInt32();

                    if (!_paths.count(pathId))
                    {
                        TC_LOG_ERROR("server.loading", "custom_clan_path_action : path_id {} inconnu, liaison ignoree.", pathId);
                        continue;
                    }

                    _pathsByAction[f[1].GetUInt8()].push_back(pathId);
                } while (result->NextRow());
            }

            // Une route d'un seul noeud n'a pas de segment : inexploitable.
            for (auto const& [id, path] : _paths)
                if (path.Nodes.size() < 2)
                    TC_LOG_ERROR("server.loading", "custom_clan_path : la route {} a moins de 2 noeuds, elle ne sera jamais empruntee.", id);

            TC_LOG_INFO("server.loading", ">> {} route(s) de clan chargee(s) pour {} action(s).",
                _paths.size(), _pathsByAction.size());
        }

        std::vector<Position> BuildRoute(Unit const* mover, ActionType action, Position const& to)
        {
            std::vector<Position> route;

            // Debug : trace toutes les tentatives. Sous-jacent TC_LOG_DEBUG -> affiche seulement
            // si la Logger "scripts" est configuree en Debug ; muet en prod. En-tete d'appel
            // repete a chaque log pour retrouver un run entier dans le fichier.
            uint64 const moverLow = mover->GetGUID().GetCounter();
            uint32 const actionId = uint32(action);

            auto itAction = _pathsByAction.find(uint8(action));
            if (itAction == _pathsByAction.end())
            {
                TC_LOG_DEBUG("scripts", "ClanRoad[{}] action={} : aucune route declaree pour cette action.",
                    moverLow, actionId);
                return route; // action non liee a une route -> pathfinding standard
            }

            Position const from = mover->GetPosition();
            float const straight = from.GetExactDist2d(to);

            // Garde-fou 1 : trajet trop court. Emprunter la route ferait faire un aller-retour
            // ridicule pour quelques metres.
            if (straight < MIN_TRIP_DIST)
            {
                TC_LOG_DEBUG("scripts", "ClanRoad[{}] action={} : trajet trop court ({:.1f} < {:.1f}) -> pathfinding direct.",
                    moverLow, actionId, straight, MIN_TRIP_DIST);
                return route;
            }

            PathData const* best = nullptr;
            uint32 bestEntry = 0;
            uint32 bestExit = 0;
            float bestCost = std::numeric_limits<float>::max();
            uint32 bestPathId = 0;
            uint32 pathsConsidered = 0;
            uint32 rejectedFar = 0;
            uint32 rejectedNoPath = 0;

            for (uint32 pathId : itAction->second)
            {
                auto itPath = _paths.find(pathId);
                if (itPath == _paths.end())
                    continue;

                PathData const& path = itPath->second;
                if (path.MapId != mover->GetMapId() || path.Nodes.size() < 2)
                    continue;
                ++pathsConsidered;

                // Point de SORTIE : le noeud declare le plus proche de la destination. C'est lui
                // qui evite de quitter la route en plein milieu d'un troncon pour couper a
                // travers champs -- le PNJ longe la route jusqu'a un point que tu as pose.
                uint32 const exitIdx = NearestNode(path, to);
                float const exitToDest = path.Nodes[exitIdx].GetExactDist2d(to);

                // Point d'ENTREE : parmi les noeuds les plus proches a vol d'oiseau, celui dont
                // le chemin navmesh est le plus court. C'est ce qui elimine le noeud "derriere
                // le mur" quand le PNJ part de l'interieur d'une maison.
                std::vector<uint32> const candidates = NodesByDistance(path, from);

                for (uint32 i = 0; i < candidates.size() && i < ENTRY_CANDIDATES; ++i)
                {
                    uint32 const entryIdx = candidates[i];

                    float const joinDist = NavmeshDistance(mover, path.Nodes[entryIdx]);
                    if (joinDist < 0.0f)
                    {
                        ++rejectedNoPath;
                        continue; // noeud injoignable (PATHFIND_NOPATH)
                    }

                    // Garde-fou 2 : trop loin pour etre rejoint. Mesure sur le chemin REEL, donc
                    // un noeud a 5 yards derriere un mur compte pour ce que le contournement
                    // coute vraiment.
                    if (joinDist > MAX_JOIN_DIST)
                    {
                        ++rejectedFar;
                        continue;
                    }

                    // Cout total : rejoindre la route + la longer + la quitter vers la
                    // destination. Aucun plafond de detour : on veut la route, meme si couper
                    // par le champ serait plus court.
                    float const cost = joinDist + LengthBetweenNodes(path, entryIdx, exitIdx) + exitToDest;
                    if (cost < bestCost)
                    {
                        bestCost = cost;
                        best = &path;
                        bestEntry = entryIdx;
                        bestExit = exitIdx;
                        bestPathId = pathId;
                    }
                }
            }

            if (!best)
            {
                TC_LOG_DEBUG("scripts",
                    "ClanRoad[{}] action={} : aucune route retenue ({} path(s) examinee(s), {} rejet(s) trop loin, {} rejet(s) navmesh).",
                    moverLow, actionId, pathsConsidered, rejectedFar, rejectedNoPath);
                return route; // aucune route retenue -> pathfinding standard
            }

            // Le trajet n'est fait que de noeuds DECLARES, entree et sortie comprises, parcourus
            // dans le sens qui va de l'une a l'autre.
            if (bestEntry <= bestExit)
                for (uint32 i = bestEntry; i <= bestExit; ++i)
                    route.push_back(best->Nodes[i]);
            else
                for (uint32 i = bestEntry + 1; i-- > bestExit; )
                    route.push_back(best->Nodes[i]);

            TC_LOG_DEBUG("scripts",
                "ClanRoad[{}] action={} : route {} retenue (entry={}, exit={}, {} noeud(s), cout={:.1f}, direct={:.1f}).",
                moverLow, actionId, bestPathId, bestEntry, bestExit, uint32(route.size()), bestCost, straight);

            // On ecarte le trace de la ligne centrale pour que deux membres empruntant la meme
            // route ne se marchent pas dessus. Applique APRES le choix des noeuds : les couts
            // se raisonnent sur la route reelle, pas sur la variante decalee.
            ApplyLateralOffset(route, LateralOffset(mover));

            // Puis, obligatoirement EN DERNIER, on repose le trace sur le sol : le decalage
            // ci-dessus a bouge X/Y en gardant le Z d'origine.
            SnapToGround(mover, route);

            return route;
        }
    }
}
