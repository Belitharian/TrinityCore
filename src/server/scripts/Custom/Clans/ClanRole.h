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

// Role "metier" d'un membre de clan, derive de son sexe + de son etape de vie.
// Encapsule le repertoire d'actions autorisees propre a chaque categorie (homme, femme,
// enfant), pour coller au realisme d'epoque :
//   - hommes adultes/anciens : chasse, bois, mine (production, rapportee au stock) ;
//   - femmes adultes/anciennes : feu, cuisine, courses (foyer) ;
//   - enfants : jeu et exploration.
// Concu pour etre ETENDU (agriculture, elevage) : on ajoute des roles / methodes sans
// toucher a l'IA generique. Header-only : chaque role est un petit singleton sans etat.

#ifndef CUSTOM_CLANS_CLANROLE_H
#define CUSTOM_CLANS_CLANROLE_H

#include "ClanDefines.h"
#include "ClanMind.h" // MindState (etat decode passe a Instinct)

namespace Clan
{
    class ClanRole
    {
    public:
        virtual ~ClanRole() = default;

        virtual RoleCategory Category() const = 0;

        // L'action fait-elle partie du repertoire de ce role ? Les besoins vitaux (boire,
        // dormir, manger, se soigner, errer, se recueillir) sont partages ; seules les taches
        // de production / foyer / jeu sont reservees a une categorie.
        virtual bool IsAllowed(ActionType a) const = 0;

        // Action instinctive de ce role pour un etat donne (amorce la Q-table). Ne renvoie
        // JAMAIS qu'une action autorisee pour le role (garantit un seed coherent avec le gating).
        virtual ActionType Instinct(MindState const& s) const = 0;

    protected:
        // Actions communes a tous (survie de base + errance) : declinees par chaque role.
        static bool IsVital(ActionType a)
        {
            switch (a)
            {
                case ActionType::Idle:
                case ActionType::Wander:
                case ActionType::DrinkRiver:
                case ActionType::DrinkWell:
                case ActionType::Sleep:
                case ActionType::SeekDoctor:
                case ActionType::Eat:        // manger un repas du stock : tout le monde mange
                    return true;
                default:
                    return false;
            }
        }

        // Instinct de survie NON alimentaire, commun a tous : soin puis soif/fatigue. Renvoie
        // ActionType::Count si rien ne s'impose (faim / repro / rien d'urgent -> propre au role).
        static ActionType VitalInstinct(MindState const& s)
        {
            if (s.diseased)
                return ActionType::SeekDoctor;
            switch (s.urgentNeed)
            {
                case NeedType::Thirst: return ActionType::DrinkRiver;
                case NeedType::Energy: return ActionType::Sleep;
                default:               return ActionType::Count;
            }
        }
    };

    // Homme adulte / ancien : production (chasse, bois, mine), extermination des predateurs,
    // reproduction, tradition. Rapporte ses recoltes au stock de la maison.
    class RoleMan final : public ClanRole
    {
    public:
        RoleCategory Category() const override { return RoleCategory::Man; }
        bool IsAllowed(ActionType a) const override
        {
            if (IsVital(a))
                return true;
            switch (a)
            {
                case ActionType::Hunt:
                case ActionType::GatherWood:
                case ActionType::MineRock:
                case ActionType::HuntPredator:
                case ActionType::SeekMate:
                case ActionType::LightFire:
                case ActionType::Cook:
                case ActionType::Remember:
                    return true;
                default:
                    return false;
            }
        }
        ActionType Instinct(MindState const& s) const override
        {
            if (ActionType v = VitalInstinct(s); v != ActionType::Count)
                return v;

            switch (s.urgentNeed)
            {
                case NeedType::Hunger:
                    // On mange si un repas est pret ; sinon on chasse pour approvisionner
                    // le foyer en viande crue (ce sont les femmes qui cuisinent).
                    return s.houseHasMeal ? ActionType::Eat : ActionType::Hunt;
                case NeedType::Repro:
                    return ActionType::SeekMate;
                default: // rien d'urgent : on protege puis on remplit le stock de la maison
                    if (s.predatorNearby)      return ActionType::HuntPredator;
                    if (!s.houseHasRawFood)    return ActionType::Hunt;
                    if (!s.houseHasWood)       return ActionType::GatherWood;
                    if (!s.houseHasStone)      return ActionType::MineRock;
                    return ActionType::Wander;
            }
        }
    };

    // Femme adulte / anciennne : foyer (entretien du feu, cuisine), courses chez le vendeur,
    // reproduction, tradition. Reste a la maison (pas de production exterieure).
    class RoleWoman final : public ClanRole
    {
    public:
        RoleCategory Category() const override { return RoleCategory::Woman; }
        bool IsAllowed(ActionType a) const override
        {
            if (IsVital(a))
                return true;
            switch (a)
            {
                case ActionType::LightFire:
                case ActionType::Cook:
                case ActionType::Shopping:
                case ActionType::SeekMate:
                case ActionType::Remember:
                    return true;
                default:
                    return false;
            }
        }
        ActionType Instinct(MindState const& s) const override
        {
            if (ActionType v = VitalInstinct(s); v != ActionType::Count)
                return v;

            switch (s.urgentNeed)
            {
                case NeedType::Hunger:
                    if (s.houseHasMeal)                          return ActionType::Eat;
                    if (s.houseHasRawFood && s.houseFireLit)     return ActionType::Cook;
                    if (s.houseHasRawFood && s.houseHasWood && s.houseHasStone)
                        return ActionType::LightFire;            // pas de feu -> le rallumer pour cuire
                    return ActionType::Shopping;                 // rien a cuisiner -> acheter des repas
                case NeedType::Repro:
                    return ActionType::SeekMate;
                default: // rien d'urgent : on tient le foyer (feu + reserves de repas)
                    if (!s.houseFireLit && s.houseHasWood && s.houseHasStone)
                        return ActionType::LightFire;
                    if (s.houseHasRawFood && s.houseFireLit && !s.houseHasMeal)
                        return ActionType::Cook;
                    if (!s.houseHasMeal)
                        return ActionType::Shopping;             // pas de repas d'avance -> courses
                    return ActionType::Idle;
            }
        }
    };

    // Enfant : joue et explore. Besoins vitaux uniquement en plus (boire, dormir, manger).
    class RoleChild final : public ClanRole
    {
    public:
        RoleCategory Category() const override { return RoleCategory::Child; }
        bool IsAllowed(ActionType a) const override
        {
            if (IsVital(a))
                return true;
            return a == ActionType::Play;
        }
        ActionType Instinct(MindState const& s) const override
        {
            if (ActionType v = VitalInstinct(s); v != ActionType::Count)
                return v;

            // L'enfant ne produit ni ne cuisine : il mange si un repas existe, sinon il joue
            // (et explore). Il ne peut rien faire de plus face a la faim -> il attend en jouant.
            if (s.urgentNeed == NeedType::Hunger && s.houseHasMeal)
                return ActionType::Eat;
            return ActionType::Play;
        }
    };

    // Fabrique : role correspondant a (sexe, etape de vie). Les enfants (quel que soit le
    // sexe) ont le role enfant ; a l'age adulte, le sexe determine homme/femme.
    inline ClanRole const* RoleFor(Gender gender, LifeStage stage)
    {
        static RoleMan   man;
        static RoleWoman woman;
        static RoleChild child;

        if (stage == LifeStage::Child)
            return &child;
        return gender == Gender::Female ? static_cast<ClanRole const*>(&woman)
                                        : static_cast<ClanRole const*>(&man);
    }
}

#endif // CUSTOM_CLANS_CLANROLE_H
