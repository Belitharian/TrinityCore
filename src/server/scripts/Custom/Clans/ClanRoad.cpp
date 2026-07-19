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

            // Resultat de la projection d'une position sur une polyligne.
            struct Projection
            {
                uint32   Segment = 0;   // index du noeud de DEBUT du segment porteur
                float    T = 0.0f;      // position sur ce segment, dans [0,1]
                float    Distance = std::numeric_limits<float>::max(); // distance 2D au point projete
                Position Point;         // le point projete lui-meme
            };

            std::unordered_map<uint32, PathData>              _paths;         // pathId -> geometrie
            std::unordered_map<uint8, std::vector<uint32>>    _pathsByAction; // ActionType -> pathIds

            // Projette 'pos' sur la polyligne et retourne le point le plus proche.
            //
            // On projette sur les SEGMENTS et pas sur les noeuds : viser le noeud le plus proche
            // ferait rejoindre la route en biais vers un piquet arbitraire, alors que la projection
            // segmentaire donne un raccordement perpendiculaire, naturel a l'oeil.
            //
            // Calcul en 2D (XY) volontairement : le Z suit le relief et fausserait la notion de
            // "proche de la route" sur un terrain en pente.
            Projection ProjectOnPath(PathData const& path, Position const& pos)
            {
                Projection best;

                for (uint32 i = 0; i + 1 < path.Nodes.size(); ++i)
                {
                    Position const& a = path.Nodes[i];
                    Position const& b = path.Nodes[i + 1];

                    float const abx = b.GetPositionX() - a.GetPositionX();
                    float const aby = b.GetPositionY() - a.GetPositionY();
                    float const lenSq = abx * abx + aby * aby;
                    if (lenSq < 0.01f) // noeuds confondus : segment degenere
                        continue;

                    // Produit scalaire normalise = position relative de la projection sur [a,b],
                    // clampee pour rester DANS le segment (sinon on projetterait sur son prolongement).
                    float t = ((pos.GetPositionX() - a.GetPositionX()) * abx
                             + (pos.GetPositionY() - a.GetPositionY()) * aby) / lenSq;
                    t = std::clamp(t, 0.0f, 1.0f);

                    Position const point(a.GetPositionX() + abx * t,
                                         a.GetPositionY() + aby * t,
                                         a.GetPositionZ() + (b.GetPositionZ() - a.GetPositionZ()) * t);

                    float const dist = pos.GetExactDist2d(point);
                    if (dist < best.Distance)
                    {
                        best.Segment = i;
                        best.T = t;
                        best.Distance = dist;
                        best.Point = point;
                    }
                }

                return best;
            }

            // Projette 'pos' sur CHAQUE segment et retourne les candidats tries par distance a
            // vol d'oiseau croissante. Sert de preselection avant l'evaluation navmesh, qui est
            // trop chere pour etre appliquee a tous les segments d'une longue route.
            std::vector<Projection> ProjectCandidates(PathData const& path, Position const& pos)
            {
                std::vector<Projection> candidates;
                candidates.reserve(path.Nodes.size());

                for (uint32 i = 0; i + 1 < path.Nodes.size(); ++i)
                {
                    Position const& a = path.Nodes[i];
                    Position const& b = path.Nodes[i + 1];

                    float const abx = b.GetPositionX() - a.GetPositionX();
                    float const aby = b.GetPositionY() - a.GetPositionY();
                    float const lenSq = abx * abx + aby * aby;
                    if (lenSq < 0.01f) // noeuds confondus : segment degenere
                        continue;

                    float t = ((pos.GetPositionX() - a.GetPositionX()) * abx
                             + (pos.GetPositionY() - a.GetPositionY()) * aby) / lenSq;
                    t = std::clamp(t, 0.0f, 1.0f);

                    Projection candidate;
                    candidate.Segment = i;
                    candidate.T = t;
                    candidate.Point = Position(a.GetPositionX() + abx * t,
                                               a.GetPositionY() + aby * t,
                                               a.GetPositionZ() + (b.GetPositionZ() - a.GetPositionZ()) * t);
                    candidate.Distance = pos.GetExactDist2d(candidate.Point);
                    candidates.push_back(candidate);
                }

                std::sort(candidates.begin(), candidates.end(),
                    [](Projection const& l, Projection const& r) { return l.Distance < r.Distance; });

                return candidates;
            }

            // Longueur du chemin REELLEMENT parcourable entre 'mover' et 'dest', en contournant
            // le decor. Retourne -1 si la destination est injoignable.
            //
            // C'est la mesure qui remplace la distance a vol d'oiseau pour choisir ou rejoindre
            // la route : elle seule sait qu'un mur separe le PNJ d'un point pourtant "proche".
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

            // Distance parcourue LE LONG de la route entre deux projections (et non a vol d'oiseau).
            float LengthAlongPath(PathData const& path, Projection const& from, Projection const& to)
            {
                auto distanceFromStart = [&path](Projection const& p)
                {
                    float d = 0.0f;
                    for (uint32 i = 0; i < p.Segment; ++i)
                        d += path.Nodes[i].GetExactDist2d(path.Nodes[i + 1]);
                    return d + path.Nodes[p.Segment].GetExactDist2d(p.Point);
                };

                return std::fabs(distanceFromStart(to) - distanceFromStart(from));
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

            auto itAction = _pathsByAction.find(uint8(action));
            if (itAction == _pathsByAction.end())
                return route; // action non liee a une route -> pathfinding standard

            Position const from = mover->GetPosition();

            // Garde-fou 1 : trajet trop court. Emprunter la route ferait faire un aller-retour
            // ridicule pour quelques metres.
            if (from.GetExactDist2d(to) < MIN_TRIP_DIST)
                return route;

            PathData const* best = nullptr;
            Projection bestEntry, bestExit;
            float bestCost = std::numeric_limits<float>::max();

            for (uint32 pathId : itAction->second)
            {
                auto itPath = _paths.find(pathId);
                if (itPath == _paths.end())
                    continue;

                PathData const& path = itPath->second;
                if (path.MapId != mover->GetMapId() || path.Nodes.size() < 2)
                    continue;

                // Le point de SORTIE reste choisi a vol d'oiseau : la destination (vendeur,
                // medecin) est par construction au bord de la route, donc sans mur entre les
                // deux. Seule l'ENTREE pose probleme, le PNJ pouvant partir de l'interieur
                // d'une maison.
                Projection const exit = ProjectOnPath(path, to);

                // Candidats d'entree : un par segment, presele ctionnes a vol d'oiseau.
                std::vector<Projection> candidates = ProjectCandidates(path, from);

                // Puis on les departage au NAVMESH. C'est ici que le point "derriere le mur"
                // se fait eliminer : son chemin reel contourne la maison, il est donc long,
                // alors que celui accessible par la porte est court.
                for (uint32 i = 0; i < candidates.size() && i < ENTRY_CANDIDATES; ++i)
                {
                    Projection const& entry = candidates[i];

                    float const joinDist = NavmeshDistance(mover, entry.Point);
                    if (joinDist < 0.0f)
                        continue; // point injoignable (PATHFIND_NOPATH)

                    // Garde-fou 2 : la route est trop loin pour etre rejointe. Mesure sur le
                    // chemin REEL, donc un point a 5 yards derriere un mur compte pour ce que
                    // sa traversee coute vraiment.
                    if (joinDist > MAX_JOIN_DIST)
                        continue;

                    // Cout total : rejoindre la route + la longer + la quitter.
                    // Aucun plafond de detour ici : on veut la route, meme si le champ est
                    // plus court.
                    float const cost = joinDist + LengthAlongPath(path, entry, exit) + exit.Distance;
                    if (cost < bestCost)
                    {
                        bestCost = cost;
                        best = &path;
                        bestEntry = entry;
                        bestExit = exit;
                    }
                }
            }

            if (!best)
                return route; // aucune route retenue -> pathfinding standard

            // Point d'entree sur la route.
            route.push_back(bestEntry.Point);

            // Noeuds intermediaires, dans le sens de parcours. Le segment i relie Nodes[i] a
            // Nodes[i+1] : en marche avant on enchaine donc Nodes[entree+1] .. Nodes[sortie],
            // en marche arriere Nodes[entree] .. Nodes[sortie+1].
            bool const forward = bestExit.Segment > bestEntry.Segment
                || (bestExit.Segment == bestEntry.Segment && bestExit.T >= bestEntry.T);

            if (forward)
                for (uint32 i = bestEntry.Segment + 1; i <= bestExit.Segment; ++i)
                    route.push_back(best->Nodes[i]);
            else
                for (uint32 i = bestEntry.Segment; i > bestExit.Segment; --i)
                    route.push_back(best->Nodes[i]);

            // Point de sortie. La destination finale n'est PAS ajoutee : l'appelant rejoue son
            // mouvement d'origine depuis ici, ce qui laisse sa machine a etats intacte.
            route.push_back(bestExit.Point);

            return route;
        }
    }
}
