#ifndef CLAN_ROAD_H
#define CLAN_ROAD_H

#include "ClanDefines.h"
#include "Position.h"
#include <vector>

class Unit;

namespace Clan
{
    // ---------------------------------------------------------------------------------------
    // Reseau de routes declare en base (custom_clan_path*).
    //
    // Le navmesh (mmaps) ne connait que la geometrie : il produit toujours le chemin le plus
    // court, qui coupe a travers champs et longe les batiments. Pour les deplacements "sociaux"
    // (aller chez le vendeur, chez le medecin) on veut au contraire que le PNJ emprunte la
    // route du village, comme un habitant.
    //
    // Principe : une polyligne declaree en base par map, liee a une ou plusieurs ActionType.
    // Le PNJ rejoint la route, la longe, puis la quitte au plus pres de sa destination.
    //
    // CHOIX DU POINT D'ENTREE : surtout PAS le point le plus proche a vol d'oiseau. Un PNJ a
    // l'interieur d'une maison a pour plus proche voisin un point situe de l'autre cote du mur ;
    // le viser le ferait sortir puis revenir sur ses pas. On presele ctionne donc les candidats
    // a vol d'oiseau (calcul negligeable) puis on les departage a la LONGUEUR REELLE DU CHEMIN
    // NAVMESH -- ce qui fait naturellement gagner le point accessible par la porte.
    //
    // Ce systeme ne REMPLACE pas le pathfinding : les points de passage sont donnes a un
    // WaypointMovementGenerator qui calcule lui-meme le chemin navmesh entre chacun. Tous les
    // noeuds etant Walk et sans delai, BuildSegments() les fusionne en UN segment continu :
    // trajet fluide, sans arret ni virage anguleux aux noeuds.
    //
    // Si aucune route ne s'applique (action non liee, route trop loin, trajet trop court), on
    // retombe silencieusement sur le comportement standard.
    // ---------------------------------------------------------------------------------------
    namespace Road
    {
        // Garde-fous. Exposes ici pour etre ajustables sans toucher a la logique.
        //
        // NOTE : il n'y a VOLONTAIREMENT aucun plafond de detour par rapport au trajet direct.
        // Un villageois emprunte la route meme quand couper par le champ serait plus court --
        // c'est precisement l'objet de ce systeme. Un tel plafond rejetterait la route dans les
        // cas ou on la veut le plus (typiquement en partant du Clan B).

        // En-deca de cette distance a vol d'oiseau, on va tout droit : trajet de proximite
        // pour lequel passer par la route serait ridicule.
        constexpr float MIN_TRIP_DIST = 8.0f;

        // Distance NAVMESH (chemin reellement parcouru, pas a vol d'oiseau) au-dela de laquelle
        // on considere que le PNJ n'est pas "sur" la route.
        constexpr float MAX_JOIN_DIST = 60.0f;

        // Nombre de points d'entree candidats soumis au pathfinder. La preselection a vol
        // d'oiseau est gratuite, l'evaluation navmesh est chere : on la limite a ces K-la.
        constexpr uint32 ENTRY_CANDIDATES = 5;

        // Charge custom_clan_path / _node / _action. Appele depuis ClanDatabase::LoadRegistries.
        void Load();

        // Construit la suite de points de passage pour aller de 'mover' a 'to' en empruntant la
        // route declaree pour 'action'.
        //
        // 'mover' est requis (et pas seulement sa position) : le choix du point d'entree
        // interroge le navmesh depuis cette unite.
        //
        // Retourne un vecteur VIDE si aucune route ne s'applique -> l'appelant doit alors faire
        // son MovePoint habituel. C'est le fallback "systeme standard".
        //
        // Le vecteur retourne ne contient PAS la destination finale : il s'arrete au point de
        // sortie de route. C'est a l'appelant d'y conduire le PNJ ensuite.
        std::vector<Position> BuildRoute(Unit const* mover, ActionType action, Position const& to);
    }
}

#endif // CLAN_ROAD_H
