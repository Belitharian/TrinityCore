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

#include "ClanMemberAI.h"
#include "ClanMgr.h"
#include "ClanNeeds.h"
#include "ClanRoad.h"
#include "ClanRole.h"
#include "Creature.h"
#include "GameObject.h"
#include "GameTime.h"
#include "Map.h"
#include "MotionMaster.h"
#include "MovementDefines.h"
#include "WaypointDefines.h"
#include "ObjectAccessor.h"
#include "PhasingHandler.h"
#include "Random.h"
#include "SpellDefines.h"
#include "SpellInfo.h"
#include "WowTime.h"
#include <algorithm>
#include <string>

using namespace Clan;

namespace
{
    // Groupe de la boucle de decision (repetitif, ne doit pas etre annule par un reflexe).
    constexpr uint32 GROUP_DECISION = 1;
    // Groupe des interactions ponctuelles (boire/dormir/cuire/errer) : annulable par un reflexe.
    constexpr uint32 GROUP_ACTION = 2;

    // Entretien du foyer qu'un HOMME doit assurer faute de femme au clan (les deux actions
    // font partie de son repertoire autorise). Miroir de la logique de RoleWoman, limite a
    // ce qu'un homme peut faire (rallumer + cuisiner ; pas de courses). Renvoie
    // ActionType::Count si rien a tenir OU si un besoin vital prime -> on laisse alors le
    // choix normal (Q-table) gerer la survie de l'individu.
    Clan::ActionType HearthFallback(Clan::MindState const& s)
    {
        using Clan::ActionType;
        using Clan::NeedType;

        if (s.diseased || s.urgentNeed == NeedType::Thirst || s.urgentNeed == NeedType::Energy)
            return ActionType::Count;

        if (!s.houseFireLit && s.houseHasWood && s.houseHasStone)
            return ActionType::LightFire;               // foyer eteint : le rallumer
        if (s.houseHasRawFood && s.houseFireLit && !s.houseHasMeal)
            return ActionType::Cook;                    // viande + feu, pas de repas : cuisiner
        return ActionType::Count;
    }
}

npc_clan_member::npc_clan_member(Creature* creature) : ScriptedAI(creature),
_owner(nullptr), _currentAction(ActionType::Idle), _targetNeed(NeedType::None),
_needBefore(0.0f), _huntTimerMs(0), _roadPendingId(0), _roadWalk(true), _roadMountSpell(0),
_actionTimerMs(0), _huntPreyKilled(false), _actionEngaged(false),
_busy(false), _reflex(Reflex::None),
_inventory {}, _talkCdMs(0), _starveTimerMs(0), _diseaseTimerMs(0), _rememberCdMs(0), _shopCdMs(0),
_milkCdMs(0), _fillCdMs(0),
_lastAction(ActionType::Count), _repeatStreak(0),
_equipDirty(false), _sleepElevated(false), _combatLearned(false), _lastSeededStage(Clan::LifeStage::Adult),
_selfAtMeet(false), _mateAtMeet(false)
{
}

Clan::ClanRole const* npc_clan_member::GetRole() const
{
    // Role derive du sexe + de l'etape de vie ; toujours en phase avec le vieillissement
    // (RoleFor renvoie un singleton, pas d'allocation). _owner peut etre nul avant BindState.
    return _owner ? RoleFor(_owner->gender, _owner->stage) : nullptr;
}

Clan::HouseState* npc_clan_member::MyHouse() const
{
    return _owner ? sClanMgr->GetHouseBySpawn(_owner->houseSpawnId) : nullptr;
}

GameObject* npc_clan_member::MyHouseObject() const
{
    if (!_owner || !_owner->houseSpawnId)
        return nullptr;

    return me->GetMap()->GetGameObjectBySpawnId(_owner->houseSpawnId);
}

// ---------------------------------------------------------------------------
// Inventaire
// ---------------------------------------------------------------------------

uint32 npc_clan_member::GetItemCount(ItemType type) const
{
    if (type >= ItemType::Count)
        return 0;

    return _inventory[uint8(type)];
}

bool npc_clan_member::IsItemFull(ItemType type) const
{
    return GetItemCount(type) >= INVENTORY_MAX_PER_ITEM;
}

bool npc_clan_member::IsBagFull() const
{
    // Un seul type sature suffit a imposer le retour : on ne repart pas en tournee les bras
    // charges. C'est ce booleen que la Q-table percoit (MindState::bagFull).
    for (uint8 t = 0; t < uint8(ItemType::Count); ++t)
        if (IsItemFull(ItemType(t)))
            return true;
    return false;
}

float npc_clan_member::ScarcityMult(ItemType type) const
{
    HouseState const* house = MyHouse();
    if (!house)
        return 1.0f; // Pas de foyer connu : tout manque

    uint32 stock = house->Get(type);
    if (stock >= HOUSE_STOCK_COMFORT)
        return REWARD_SCARCITY_FLOOR;

    // Decroissance lineaire de 1.0 (stock nul) jusqu'au plancher (niveau de confort atteint).
    float ratio = float(stock) / float(HOUSE_STOCK_COMFORT);
    return 1.0f - (1.0f - REWARD_SCARCITY_FLOOR) * ratio;
}

bool npc_clan_member::AddItem(ItemType type, uint32 count)
{
    if (type >= ItemType::Count)
        return false;

    uint32& slot = _inventory[uint8(type)];
    if (slot + count > INVENTORY_MAX_PER_ITEM)
        return false; // On ne porte pas plus que la capacite

    slot += count;
    return true;
}

bool npc_clan_member::ConsumeItem(ItemType type, uint32 count)
{
    if (type >= ItemType::Count)
        return false;

    uint32& slot = _inventory[uint8(type)];
    if (slot < count)
        return false;

    slot -= count;
    return true;
}

uint32 npc_clan_member::DepositCarried(ItemType type)
{
    HouseState* house = MyHouse();
    if (!house)
        return 0;

    uint32 carried = GetItemCount(type);
    uint32 space   = HOUSE_STOCK_MAX - std::min<uint32>(HOUSE_STOCK_MAX, house->Get(type));
    uint32 n       = std::min(carried, space);
    if (n > 0)
    {
        house->Add(type, n);
        ConsumeItem(type, n);
    }
    return n;
}

float npc_clan_member::DepositAllCarried()
{
    HouseState* house = MyHouse();
    if (!house)
        return 0.0f;

    uint32 total    = 0;
    float  weighted = 0.0f;

    for (uint8 t = 0; t < uint8(ItemType::Count); ++t)
    {
        ItemType type = ItemType(t);

        // Rarete evaluee AVANT le depot : c'est le manque au moment ou l'on rentre qui fait la
        // valeur de la livraison (sinon deposer comblerait le manque et annulerait sa propre prime).
        float mult = ScarcityMult(type);
        if (uint32 n = DepositCarried(type))
        {
            total += n;
            weighted += float(n) * mult;
        }

        // Le stock deborde et on porte encore de ce type : on l'abandonne sur place. Sans cette
        // purge, "sac plein + stock plein" ferait boucler la regle deterministe de retour au
        // foyer sur un depot toujours vide -- exactement le genre de boucle qu'on veut bannir.
        if (house->Get(type) >= HOUSE_STOCK_MAX && GetItemCount(type) > 0)
            ConsumeItem(type, GetItemCount(type));
    }

    if (total == 0)
        return 0.0f;

    // La prime de recolte a deja ete versee au ramassage : la livraison ne vaut que REWARD_STORE,
    // module par la rarete moyenne de ce qu'on vient de rapporter.
    return REWARD_STORE * (weighted / float(total));
}

void npc_clan_member::JustAppeared()
{
    // Objet ACTIF : la grille qui porte le membre reste mise a jour meme sans joueur a
    // proximite.
    //
    // C'est indispensable ici : tout ce module repose sur des PNJ qui vivent leur vie, que
    // quelqu'un les regarde ou non.
    me->setActive(true);
    me->SetFarVisible(true);
    me->SetVisibilityDistanceOverride(VisibilityDistanceType::Infinite);

    // Bloque la regeneration de la vie.
    me->SetRegenerateHealth(false);

    // Les nouveau-nes recoivent leur etat via BindState (appele par ClanMgr) ;
    // les membres places par l'admin s'enregistrent ici.
    if (!_owner && me->GetSpawnId())
        BindState(sClanMgr->RegisterPlacedMember(me));
    else if (_owner)
        ResetActionState();
}

void npc_clan_member::BindState(MemberState* state)
{
    _owner = state;
    if (!_owner)
        return;

    _owner->liveGuid = me->GetGUID();

    // Rendu visuel de l'etape de vie : modele (displayId) par entry + echelle.
    _owner->entry = me->GetEntry(); // entry reelle du PNJ (cle des modeles)

    // Lit et maison (registres custom_clan_bed / custom_clan_house) : resolus a chaque
    // (re)spawn, donc survivent a la mort du membre.
    _owner->bedSpawnId = sClanMgr->GetAssignedBed(_owner->entry);
    _owner->houseSpawnId = sClanMgr->GetMemberHouse(_owner->entry, _owner->clan);

    if (!_owner->displayId)
        _owner->displayId = sClanMgr->GetDisplayId(_owner->entry, _owner->stage);

    if (_owner->displayId)
        me->SetDisplayId(_owner->displayId);

    me->SetObjectScale(_owner->stage == LifeStage::Child ? CHILD_SCALE : 1.0f);

    // Le module pilote entierement les deplacements : on neutralise le mouvement
    // aleatoire par defaut du spawn (MovementType en base), sinon le PNJ se remet
    // a errer a l'arrivee et casse les interactions (dormir/boire/cuire...).
    me->SetDefaultMovementType(IDLE_MOTION_TYPE);
    me->GetMotionMaster()->Clear();
    me->GetMotionMaster()->MoveIdle();

    // Decouverte proactive des feux alentour : ils sont ainsi allumes des le depart, et
    // surtout enregistres (RegisterFire) avec leur rattachement de maison (custom_clan_fire).
    sClanMgr->FindNearestLitFire(me);

    // Amorce de la Q-table pour le ROLE courant (homme/femme/enfant). Non destructif : ne
    // remonte que les instincts encore sous le seuil, donc n'ecrase ni l'acquis, ni l'heritage.
    _owner->mind.SeedTopUp(GetRole());
    _lastSeededStage = _owner->stage;

    ResetActionState();

    // (Re)arme la boucle de decision.
    _scheduler.CancelGroup(GROUP_DECISION);
    _scheduler.Schedule(Milliseconds(DECISION_INTERVAL_MS), GROUP_DECISION, [this](TaskContext task)
    {
        DecisionTick();
        task.Repeat(Milliseconds(DECISION_INTERVAL_MS));
    });
}

void npc_clan_member::UnbindState()
{
    // L'ordre compte : on tue d'abord TOUTES les taches (y compris la boucle de decision),
    // car leurs lambdas capturent this et lisent _owner. Ensuite seulement on lache l'etat.
    _scheduler.CancelAll();
    ResetActionState();
    ResetEquipment();
    RestoreSleepPosture(); // si detache en plein sommeil sur un lit

    me->SetEmoteState(EMOTE_STATE_NONE);
    if (me->GetStandState() != UNIT_STAND_STATE_STAND)
        me->SetStandState(UNIT_STAND_STATE_STAND);

    me->GetMotionMaster()->MoveIdle();
    _owner = nullptr; // A partir d'ici UpdateAI et DecisionTick sortent immediatement
}

void npc_clan_member::Reset()
{
    ResetActionState();
}

void npc_clan_member::ResetActionState()
{
    _busy = false;
    _currentAction = ActionType::Idle;
    _targetNeed = NeedType::None;
    _huntTimerMs = 0;
    _actionTimerMs = 0;
    _huntPreyKilled = false;
    _actionEngaged = false;
    _reflex = Reflex::None;
    _combatLearned = false;
    _selfAtMeet = false;
    _mateAtMeet = false;
    _needBefore = 0.0f;
    _actionTarget.Clear();

    // Sans cela, un membre interrompu en pleine route (fuite, combat, .clan force) garderait sa
    // route dans MOTION_SLOT_DEFAULT et repartirait la longer des que l'action suivante rend la
    // main -- c'est ce qui faisait que forcer une action n'annulait pas le trajet en cours.
    ClearRoad();
}

void npc_clan_member::SetFacingAction()
{
    if (_actionTarget.IsCreature())
    {
        Creature* creature = ObjectAccessor::GetCreature(*me, _actionTarget);
        if (!creature)
            return;

        me->SetFacingToObject(creature);

        if (creature->IsAlive())
            creature->SetFacingToObject(me);
    }
    else if (WorldObject* actionTarget = ObjectAccessor::GetWorldObject(*me, _actionTarget))
    {
        me->SetFacingToObject(actionTarget);
    }
}

void npc_clan_member::SetFacingAction(Position const& point)
{
    me->SetFacingToPoint(point);
}

bool npc_clan_member::IsNightNow()
{
    if (WowTime const* t = GameTime::GetWowTime())
        return Clan::IsNight(uint8(t->GetHour()));

    return false;
}

MindState npc_clan_member::CurrentMindState() const
{
    if (!_owner)
        return MindState();

    return BuildState();
}

bool npc_clan_member::SetRandomDeceased(Clan::AfflictionType type, float chance)
{
    if (!sClanMgr->IsDiseased(me) && roll_chance(chance))
    {
        if (uint32 aura = sClanMgr->GetRandomDisease(type))
        {
            me->AddAura(aura, me);
            return true;
        }
    }

    return false;
}

void npc_clan_member::ResetEquipment()
{
    if (!_equipDirty)
        return; // Rien d'emprunte : on evite d'envoyer des champs pour rien

    SetEquipmentSlots(true); // Recharge l'equipement d'origine du gabarit
    _equipDirty = false;
}

void npc_clan_member::PlayCustomFx()
{
    _PlayCustomFx(me);
}

void npc_clan_member::PlayCustomFxTarget()
{
    if (!_actionTarget.IsCreature())
        return;

    Creature* actionTarget = ObjectAccessor::GetCreature(*me, _actionTarget);
    if (!actionTarget)
        return;

    _PlayCustomFx(actionTarget);
}

/// <param name="target">Ne concerne que le type SPELL</param>
void npc_clan_member::CastSpellTarget(Creature* target)
{
    if (!target)
        return;

    Clan::ActionFx const* fx = sClanMgr->GetActionFx(_currentAction);
    if (!fx)
        return;

    if (!_actionTarget.IsCreature())
        return;

    Creature* actionTarget = ObjectAccessor::GetCreature(*me, _actionTarget);
    if (!actionTarget)
        return;

    if (fx->spell)
    {
        actionTarget->CastSpell(target, fx->spell, CastSpellExtraArgs(
            TRIGGERED_IGNORE_CAST_IN_PROGRESS |
            TRIGGERED_IGNORE_POWER_COST |
            TRIGGERED_IGNORE_CASTER_AURASTATE |
            TRIGGERED_DONT_REPORT_CAST_ERROR)
        );
    }
}

void npc_clan_member::_PlayCustomFx(Creature* creature)
{
    Clan::ActionFx const* fx = sClanMgr->GetActionFx(_currentAction);
    if (!fx)
        return;

    if (fx->emote)
        creature->SetEmoteState(Emote(fx->emote));

    if (fx->spell)
    {
        creature->CastSpell(creature, fx->spell, CastSpellExtraArgs(
            TRIGGERED_IGNORE_CAST_IN_PROGRESS |
            TRIGGERED_IGNORE_POWER_COST |
            TRIGGERED_IGNORE_CASTER_AURASTATE |
            TRIGGERED_DONT_REPORT_CAST_ERROR)
        );
    }

    if (fx->aura)
        creature->AddAura(fx->aura, creature);

    if (fx->item)
    {
        // SetEquipmentSlots -> SetVirtualItem : equipement au runtime
        switch (fx->itemSlot)
        {
            case 1:
                SetEquipmentSlots(false, EQUIP_NO_CHANGE, int32(fx->item), EQUIP_NO_CHANGE);
                break;
            case 2:
                SetEquipmentSlots(false, EQUIP_NO_CHANGE, EQUIP_NO_CHANGE, int32(fx->item));
                break;
            default:
                SetEquipmentSlots(false, int32(fx->item), EQUIP_NO_CHANGE, EQUIP_NO_CHANGE);
                break;
        }

        _equipDirty = true;
    }

    if (fx->sound_male && creature->GetGender() == GENDER_MALE)
        creature->PlayDistanceSound(fx->sound_male);

    if (fx->sound_female && creature->GetGender() == GENDER_FEMALE)
        creature->PlayDistanceSound(fx->sound_female);
}

void npc_clan_member::SpawnGravestone()
{
    Position dest = me->GetPosition();
    GraveyardSlot* slot = sClanMgr->AcquireGraveyardSlot();

    if (slot)
    {
        dest = slot->position;
        // Estampille la tombe : identite du defunt (ses descendants viendront s'y recueillir,
        // action "Remember") et epitaphe (lue au clic). _owner est encore valide ici (JustDied).
        if (_owner)
        {
            slot->deceasedId = _owner->dbId;
            slot->deceasedName = me->GetName();
            slot->cause = _owner->deathCause;
            slot->ageDays = _owner->ageDays;
            // Texte grave une fois pour toutes (modele de custom_clan_epitaph + jetons).
            slot->epitaph = sClanMgr->BuildEpitaph(_owner->deathCause, me->GetName(), _owner->ageDays);
        }
    }

    uint32 randomEntry = GRAVESTONES[urand(0, GRAVESTONE_COUNT - 1)];
    QuaternionData rot = QuaternionData::fromEulerAnglesZYX(dest.GetOrientation(), 0.0f, 0.0f);

    // La tombe appartient au MONDE, pas au defunt : on la cree directement sur la map, sans
    // invocateur.
    //
    // me->SummonGameObject() la rattachait a 'me' via ToUnit()->AddGameObject() -- la pierre
    // tombale etait donc detruite avec le cadavre qu'elle commemore, ce qui la faisait
    // disparaitre a la decomposition du corps.
    //
    // Sans proprietaire ni delai de respawn, rien ne peut la faire despawn :
    //   - la branche d'expiration de GameObject::Update est gardee par m_respawnTime > 0 ;
    //   - isSummonedAndExpired vaut (GetOwner() || GetSpellId()) && ... , donc faux ici.
    // C'est aussi ce qui evite le recours a une duree de vie artificiellement enorme.
    Map* map = me->GetMap();
    GameObject* gravestone = GameObject::CreateGameObject(randomEntry, map, dest, rot, 255, GO_STATE_READY);
    if (gravestone)
    {
        // Meme phase que le defunt, sinon la tombe pourrait lui etre invisible.
        PhasingHandler::InheritPhaseShift(gravestone, me);

        if (!map->AddToMap(gravestone))
        {
            delete gravestone; // AddToMap n'en prend possession qu'en cas de succes
            gravestone = nullptr;
        }
    }

    if (gravestone)
    {
        // Lien pierre -> emplacement : c'est ainsi que le gossip retrouve l'epitaphe.
        if (slot)
            slot->graveGuid = gravestone->GetGUID();

        // Effet visuel sur la tombe. Pas de 'return' en cas d'echec : ce n'est qu'un
        // accessoire, et sortir ici sauterait la suppression du corps ci-dessous -- le
        // cadavre resterait alors sur place indefiniment.
        if (Creature* worldFx = gravestone->SummonCreature(WORLD_TRIGGER, gravestone->GetPosition()))
            worldFx->CastSpell(worldFx, GRAVESTONE_SPOT);
    }

    // Supprime le corps.
    me->DespawnOrUnsummon(3s);
}

MindState npc_clan_member::BuildState() const
{
    MindState s;
    s.urgentNeed = _owner->needs.MostUrgent();

    // Vie critique : se nourrir passe devant tout le reste -- MAIS seulement si le membre a
    // deja une vraie faim (EAT_HUNGER_MIN). Sinon on forcerait "Hunger" -> Eat, qui echoue
    // (ventre plein) -> boucle sterile, et surtout on viderait le stock pour se soigner.
    // Le blesse rassasie se soignera a son prochain cycle de faim naturel.
    if (me->GetHealthPct() <= HEALTH_LOW_PCT && _owner->needs.hunger >= EAT_HUNGER_MIN)
        s.urgentNeed = NeedType::Hunger;

    // Niveaux CONTINUS normalises, en plus du besoin urgent discret : le cerveau lineaire sait
    // exploiter l'intensite. Un affame a 51% et un affame a 99% cessaient d'etre distinguables
    // une fois reduits au seul "besoin le plus urgent".
    s.hunger01 = _owner->needs.hunger / NEED_MAX;
    s.thirst01 = _owner->needs.thirst / NEED_MAX;
    s.energy01 = _owner->needs.energy / NEED_MAX;
    s.repro01  = _owner->needs.reproUrge / NEED_MAX;

    s.night = IsNightNow();

    // Etat percu centre sur le STOCK DE LA MAISON (partage) : c'est autour de lui que
    // s'organise la division du travail. Booleens "en possede / n'en possede pas".
    if (HouseState const* h = MyHouse())
    {
        s.houseHasMeal = h->meals > 0;
        s.houseHasRawFood = h->Get(ItemType::RawFood) > 0;
        s.houseHasWood = h->Get(ItemType::Wood) > 0;
        s.houseHasStone = h->Get(ItemType::Stone) > 0;
        s.houseHasMilk = h->Get(ItemType::Milk) > 0;
    }

    // Ferme : disponibilite du gibier domestique et etat des auges. On n'interroge le
    // gestionnaire que si le role peut en tirer parti, pour ne pas payer les balayages a
    // chaque tick pour tous les enfants du monde.
    RoleCategory cat = _owner->stage == LifeStage::Child ? RoleCategory::Child
                        : (_owner->gender == Clan::Gender::Female ? RoleCategory::Woman : RoleCategory::Man);
    if (cat == RoleCategory::Man)
    {
        s.farmAnimalReady = sClanMgr->FarmHasAnimal(me, false);
        s.farmTroughEmpty = sClanMgr->FarmHasEmptyTrough(me);
        s.farmNeedsWater  = s.farmTroughEmpty; // pour l'instant, meme signal (cf. AnimalKind::Pig)
    }
    else if (cat == RoleCategory::Woman)
    {
        s.farmAnimalReady = sClanMgr->FarmHasAnimal(me, true); // seules les vaches nous interessent
    }

    // Le foyer de la maison est-il allume ? (feu rattache a la maison via custom_clan_fire.)
    s.houseFireLit = _owner->houseSpawnId && sClanMgr->FindHouseFire(me, _owner->houseSpawnId, true) != nullptr;
    s.diseased = sClanMgr->IsDiseased(me);
    s.predatorNearby = sClanMgr->FindNearestPredator(me) != nullptr;

    // Seul aspect de l'inventaire PORTE que l'agent percoit : "j'ai les bras pleins, il faut
    // livrer". Sans ce bit, la meme case de Q-table melangeait "partir recolter" et "rentrer
    // deposer", et une recolte pouvait dormir a vie dans un sac.
    s.bagFull = IsBagFull();

    return s;
}

void npc_clan_member::UpdateAI(uint32 diff)
{
    _scheduler.Update(diff);

    if (!_owner)
        return;

    // Les besoins croissent en continu (seuls les adultes ont un besoin de reproduction).
    _owner->needs.Decay(diff, IsNightNow(), _owner->stage == LifeStage::Adult);

    // Cooldown de parole.
    if (_talkCdMs > diff)
        _talkCdMs -= diff;
    else
        _talkCdMs = 0;

    // Cooldown du recueillement (anti-farm de la recompense "tradition").
    if (_rememberCdMs > diff)
        _rememberCdMs -= diff;
    else
        _rememberCdMs = 0;

    // Cooldown des courses (anti-farm de repas achetes).
    if (_shopCdMs > diff)
        _shopCdMs -= diff;
    else
        _shopCdMs = 0;

    // Cooldowns de la ferme (anti-farm des taches d'auge et de traite).
    if (_milkCdMs > diff) _milkCdMs -= diff; else _milkCdMs = 0;
    if (_fillCdMs > diff) _fillCdMs -= diff; else _fillCdMs = 0;

    // Degats de survie (famine ET maladie, meme tick) : les deux rongent les PV et peuvent
    // tuer. La regeneration est suspendue tant que le membre a faim.
    _starveTimerMs += diff;
    if (_starveTimerMs >= STARVE_TICK_MS)
    {
        _starveTimerMs = 0;

        float pct = 0.0f;
        if (_owner->needs.hunger >= HUNGER_STARVE_THRESHOLD)
            pct += STARVE_DAMAGE_PCT;   // Faim critique
        if (sClanMgr->IsDiseased(me))
            pct += DISEASE_DAMAGE_PCT;  // Affliction en cours (maladie / poison / saignement)

        if (pct > 0.0f && me->IsAlive())
        {
            uint64 dmg = me->CountPctFromMaxHealth(pct);
            if (!dmg)
                dmg = 1;

            if (me->GetHealth() > dmg)
            {
                me->SetHealth(me->GetHealth() - dmg);
            }
            else
            {
                // Cause gravee sur la tombe : la faim prime si les deux sevissent.
                _owner->deathCause = (_owner->needs.hunger >= HUNGER_STARVE_THRESHOLD)
                    ? DeathCause::Starvation : DeathCause::Disease;
                me->KillSelf(); // Mort de faim / de maladie -> JustDied
                return;         // _state est desormais libere : on ne touche plus a rien
            }
        }
    }

    // Contagion ambiante : petite chance de contracter une maladie s'il n'est pas deja afflige.
    _diseaseTimerMs += diff;
    if (_diseaseTimerMs >= DISEASE_TICK_MS)
    {
        _diseaseTimerMs = 0;
        SetRandomDeceased(AfflictionType::Disease, DISEASE_CHANCE);
    }

    // Reflexe predateur : un adulte se defend jusqu'a la fin du combat.
    if (_reflex == Reflex::Defend)
    {
        Unit* foe = ObjectAccessor::GetUnit(*me, _actionTarget);
        if (!foe || !foe->IsAlive() || !me->IsInCombat())
        {
            EndReflex();
            return;
        }
        me->DoMeleeAttackIfReady();
        return;
    }

    // En fuite : on attend l'arrivee au point sur (gere par MovementInform).
    if (_reflex == Reflex::Flee)
        return;

    // Extermination d'un predateur : combat au corps a corps jusqu'a la mort de la cible.
    if (_busy && _currentAction == ActionType::HuntPredator)
    {
        Creature* target = ObjectAccessor::GetCreature(*me, _actionTarget);
        if (!target || !target->IsAlive())
        {
            if (target && !target->IsAlive())
                FinishAction(true, REWARD_KILL_PREDATOR);
            else
                FinishAction(false);
            return;
        }

        me->DoMeleeAttackIfReady();

        if (_huntTimerMs <= diff)
            FinishAction(false);
        else
            _huntTimerMs -= diff;
    }
    // Chasse au gibier : la sequence (approche / tir / prelevement) est pilotee par
    // MovementInform + scheduler. Ici on ne tient que le garde-fou anti-blocage.
    else if (_busy && _currentAction == ActionType::Hunt)
    {
        if (_huntTimerMs <= diff)
        {
            if (_huntPreyKilled)
            {
                // La proie a bien ete abattue mais on n'a pas pu rejoindre / prelever la depouille
                // a temps (chemin bloque, distance...). On ne gaspille PAS un kill : la viande est
                // recuperee malgre tout, sinon le PNJ tuerait des proies sans jamais manger.
                _scheduler.CancelGroup(GROUP_ACTION);
                if (Creature* prey = ObjectAccessor::GetCreature(*me, _actionTarget))
                    prey->DespawnOrUnsummon();

                // Depot direct au stock de la maison (le garde-fou n'orchestre pas de trajet retour).
                float mult = ScarcityMult(ItemType::RawFood);
                if (HouseState* h = MyHouse())
                    h->Add(ItemType::RawFood, 1);

                FinishAction(true, (REWARD_RAWFOOD + REWARD_STORE) * mult);
            }
            else
            {
                FinishAction(false);
            }
        }
        else
        {
            _huntTimerMs -= diff;
        }
    }
    // Garde-fou GENERIQUE pour toutes les autres actions. Chasse et extermination ont deja le
    // leur (_huntTimerMs) ; partout ailleurs, un MovementInform qui n'arrive jamais (chemin
    // impraticable, GameObject despawne, cible hors d'atteinte) laissait _busy a vrai pour
    // toujours -- DecisionTick rendait alors la main immediatement et le membre restait fige
    // definitivement, sa recolte avec lui. On solde en echec pour le liberer.
    else if (_busy)
    {
        _actionTimerMs += diff;
        if (_actionTimerMs >= ACTION_TIMEOUT_MS)
        {
            _scheduler.CancelGroup(GROUP_ACTION);
            FinishAction(false);
        }
    }
}

void npc_clan_member::DecisionTick()
{
    if (!_owner || _busy || _reflex != Reflex::None)
        return;

    // Le vieillissement a pu changer de categorie (enfant->adulte...) : le role change, on
    // amorce alors les instincts des actions nouvellement accessibles (non destructif).
    if (_owner->stage != _lastSeededStage)
    {
        _owner->mind.SeedTopUp(GetRole());
        _lastSeededStage = _owner->stage;
    }

    _decisionState = BuildState();
    ActionType action = _owner->mind.ChooseAction(_decisionState, GetRole());

    // "Plus de femme au clan" : un homme adulte/ancien doit alors tenir le foyer lui-meme
    // (rallumer + cuisiner), tache normalement devolue aux femmes. Regle DETERMINISTE et non
    // apprise : condition globale rare, hors de l'etat Q-table pour ne pas doubler les etats.
    if (_owner->gender == Clan::Gender::Male && _owner->stage != LifeStage::Child
        && !sClanMgr->HasLivingWoman())
    {
        if (ActionType hearth = HearthFallback(_decisionState); hearth != ActionType::Count)
            action = hearth;
    }

    // Sac plein : on rentre livrer, point. Regle DETERMINISTE -- le Q-learning decide QUOI
    // recolter, jamais s'il faut rapporter. Une recolte immobilisee dans un sac gele toute la
    // chaine du foyer (rien a cuire -> feu eteint -> aucun repas), et l'apprentissage seul ne
    // garantit pas le retour : il suffisait que l'action porteuse perde la tete du classement
    // pour que les 5 unites y restent a vie.
    if (_decisionState.bagFull)
    {
        ClanRole const* role = GetRole();
        if (role && role->IsAllowed(ActionType::StoreHome))
            action = ActionType::StoreHome;
    }

    // Epuisement : au-dela du seuil, on DOIT dormir, quel que soit l'appat de la production.
    // Regle DETERMINISTE (la fatigue n'a pas de cout chiffre, l'apprentissage seul ne suffit
    // pas). On cede toutefois le pas a une urgence vitale plus grave : maladie a soigner ou
    // faim critique (risque de mort). Le sommeil, lui, ne tue jamais.
    if (_owner->needs.energy >= EXHAUSTION_THRESHOLD
        && !sClanMgr->IsDiseased(me)
        && _owner->needs.hunger < HUNGER_STARVE_THRESHOLD)
    {
        action = ActionType::Sleep;
    }

    BeginAction(action);
}

bool npc_clan_member::BeginAction(ActionType action)
{
    _busy = true;
    _currentAction = action;
    _targetNeed = _decisionState.urgentNeed;
    _needBefore = _owner->needs.Get(_targetNeed);

    // On tente d'abord de demarrer l'action ; on ne parle qu'ensuite, si elle a
    // reellement commence (evite d'annoncer "je cuisine" alors que le feu est eteint).
    bool started = false;
    switch (action)
    {
        case ActionType::Hunt:          started = StartHunt();                          break;
        case ActionType::StoreHome:     started = StartStoreHome();                     break;
        case ActionType::Drink:         started = StartDrink();                         break;
        case ActionType::Sleep:         started = StartSleep();                         break;
        case ActionType::SeekMate:      started = StartSeekMate();                      break;
        case ActionType::GatherWood:    started = StartGatherWood();                    break;
        case ActionType::MineRock:      started = StartMineRock();                      break;
        case ActionType::LightFire:     started = StartLightFire();                     break;
        case ActionType::Cook:          started = StartCook();                          break;
        case ActionType::SeekDoctor:    started = StartSeekDoctor();                    break;
        case ActionType::HuntPredator:  started = StartHuntPredator();                  break;
        case ActionType::Remember:      started = StartRemember();                      break;
        case ActionType::Eat:           started = StartEat();                           break;
        case ActionType::Shopping:      started = StartShopping();                      break;
        case ActionType::Play:          started = StartPlay();                          break;
        case ActionType::FillWater:     started = StartFillTrough(true);                break;
        case ActionType::FillStraw:     started = StartFillTrough(false);               break;
        case ActionType::Butcher:       started = StartButcher();                       break;
        case ActionType::MilkCow:       started = StartMilkCow();                       break;
        case ActionType::DrinkMilk:     started = StartDrinkMilk();                     break;
        case ActionType::Wander:
            StartWander();
            started = true;
            break;
        case ActionType::Idle:
        default:
            started = true;
            break;
    }

    if (!started)
    {
        // Conditions non reunies : rien n'a ete tente, AUCUNE phrase prononcee. FinishAction
        // sanctionne au tarif "indisponible" (_actionEngaged est reste faux), pas au tarif echec.
        FinishAction(false);
        return false;
    }

    // A partir d'ici l'action est reellement engagee : un echec ulterieur (cible disparue, feu
    // eteint en cours de cuisson, trajet impossible) aura coute du temps et sera puni comme tel.
    _actionEngaged = true;

    // L'action est bel et bien engagee : le membre l'annonce (avec cooldown).
    if (_talkCdMs == 0)
    {
        if (std::string const* phrase = sClanMgr->GetRandomPhrase(action))
        {
            me->Say(*phrase, LANG_UNIVERSAL);
            _talkCdMs = TALK_COOLDOWN_MS;
        }
    }

    // Idle n'a aucun deroulement asynchrone : on le termine immediatement.
    if (action == ActionType::Idle)
        FinishAction(true);

    return true;
}

bool npc_clan_member::ForceAction(ActionType action)
{
    if (!_owner)
        return false;

    // Interrompt proprement l'action / le reflexe en cours (comme le ferait une nouvelle
    // decision) : on annule les taches planifiees et on stoppe tout mouvement residuel.
    _scheduler.CancelGroup(GROUP_ACTION);
    me->GetMotionMaster()->MoveIdle();
    ResetActionState();

    // On construit l'etat percu (pour que la mesure de recompense reste coherente), puis on
    // court-circuite la selection Q-learning pour executer directement l'action demandee.
    _decisionState = BuildState();
    return BeginAction(action);
}

void npc_clan_member::RestoreSleepPosture()
{
    if (!_sleepElevated)
        return;

    _sleepElevated = false;
    me->SetDisableGravity(false);
}

// La lassitude ne s'applique qu'aux taches CHOISIES librement. Les actions dictees par un besoin
// (dormir, boire, manger, se soigner) ou par une regle deterministe (livrer sa recolte) doivent
// pouvoir s'enchainer autant que necessaire : les penaliser reviendrait a punir la survie.
static bool IsRepetitionPenalized(ActionType action)
{
    switch (action)
    {
        case ActionType::Idle:
        case ActionType::Sleep:
        case ActionType::Drink:
        case ActionType::Eat:
        case ActionType::SeekDoctor:
        case ActionType::StoreHome:
            return false;
        default:
            return true;
    }
}

void npc_clan_member::FinishAction(bool reachedGoal, float shapedReward)
{
    float reward;
    if (reachedGoal)
        reward = (_needBefore - _owner->needs.Get(_targetNeed)) / NEED_MAX + shapedReward + REWARD_TIME_PENALTY;
    else
        // Une action jamais engagee (conditions non reunies) n'a rien coute d'autre qu'un tick :
        // la punir comme un vrai echec revenait a condamner des taches parfaitement utiles dont
        // le moment n'etait simplement pas venu (courses en cooldown, feu deja allume...).
        reward = _actionEngaged ? REWARD_FAIL : REWARD_UNAVAILABLE;

    // Toute action menee en etant afflige est lourdement punie. Se faire soigner mene a un
    // etat sain (donc a une meilleure valeur future) : l'agent apprend ainsi a aller voir le
    // medecin en priorite. Note : SeekDoctor soigne AVANT d'appeler FinishAction, donc il
    // echappe a cette penalite et encaisse REWARD_CURE.
    if (sClanMgr->IsDiseased(me))
        reward += REWARD_DISEASED;

    // Lassitude : s'acharner sur la meme action devient progressivement moins payant. Sans ce
    // frein, un homme pouvait chasser en boucle sans jamais aller chercher le bois ni la pierre
    // qui manquaient au foyer. Le malus est PLAFONNE et la serie repart de zero des qu'il fait
    // autre chose : la boucle se casse, l'action ne se condamne jamais.
    // Exemptions : les actions dictees par un besoin ou par une regle deterministe (dormir,
    // boire, manger, se soigner, livrer) doivent pouvoir s'enchainer sans etre punies.
    // La lassitude ne compte que les actions ABOUTIES : s'ent?ter sur une action qui n'a pas pu
    // demarrer porte deja sa propre sanction, et cumuler les deux condamnerait la tache (une
    // femme qui tente vingt fois les courses pendant leur cooldown desapprendrait de les faire).
    if (reachedGoal && IsRepetitionPenalized(_currentAction))
    {
        if (_currentAction == _lastAction)
        {
            if (_repeatStreak < 0xFF)
                ++_repeatStreak;
        }
        else
            _repeatStreak = 1;

        if (_repeatStreak > REPEAT_TOLERANCE)
        {
            uint8 steps = std::min<uint8>(_repeatStreak - REPEAT_TOLERANCE, REPEAT_PENALTY_STEPS);
            reward += REWARD_REPEAT_PENALTY * float(steps);
        }
    }
    else if (reachedGoal)
        _repeatStreak = 0; // action exemptee menee a bien : la serie repart de zero

    if (reachedGoal)
        _lastAction = _currentAction;

    // Le cerveau apprend de l'experience (valeur future bornee au repertoire du role).
    _owner->mind.Learn(_decisionState, _currentAction, reward, BuildState(), GetRole());
    _owner->dirty = true;

    // Nettoyage : sortir du combat, arreter le mouvement d'action, se relever.
    if (me->IsInCombat())
        me->CombatStop(true);

    me->AttackStop();
    me->StopMoving();

    // Coupe une eventuelle emote de travail (bucheron/mineur)
    me->SetEmoteState(EMOTE_STATE_NONE);

    RestoreSleepPosture();  // Si on dormait sur un lit : redescendre au sol
    ResetEquipment();       // Repose l'outil de l'action qui vient de finir

    // IMPORTANT : on arrete tout mouvement residuel (notamment le MoveRandom du Wander,
    // qui tournerait sinon indefiniment). Le PNJ ne bouge que pour une action deliberee.
    me->GetMotionMaster()->Clear();
    me->GetMotionMaster()->MoveIdle();

    ResetActionState();
}

bool npc_clan_member::StartHunt()
{
    // Sac plein de viande : plus rien a prelever. Le retour au foyer n'est PAS traite ici --
    // c'est l'action StoreHome, forcee par DecisionTick. (Auparavant le depot etait une branche
    // cachee de cette fonction : il fallait re-choisir Hunt pour livrer, et la recolte restait
    // bloquee des que Hunt cessait d'etre la meilleure action.)
    if (IsItemFull(ItemType::RawFood))
        return false;

    // La chasse approvisionne le STOCK DE LA MAISON en viande crue (ce sont les femmes qui
    // cuisinent). Inutile de chasser sans maison ou porter la recolte, ou si le stock de
    // viande est deja plein (l'agent apprend alors a faire autre chose).
    HouseState* house = MyHouse();
    if (!house || house->Get(ItemType::RawFood) >= HOUSE_STOCK_MAX)
        return false;

    Creature* prey = sClanMgr->FindNearestPrey(me);
    if (!prey)
        return false;

    // Sequence : approche a portee de tir (MOVE_TO_PREY) -> coup de feu -> marche vers la
    // depouille (MOVE_TO_CARCASS) -> prelevement agenouille. Pas de corps a corps : le
    // suivi de combat de UpdateAI ne concerne plus que HuntPredator.
    _actionTarget = prey->GetGUID();
    _huntTimerMs = HUNT_TIMEOUT_MS; // Garde-fou decompte dans UpdateAI

    MoveCloserTo(MOVE_TO_PREY, prey, HUNT_SHOOT_RANGE);
    return true;
}

bool npc_clan_member::StartHuntPredator()
{
    // Seuls les adultes partent exterminer les predateurs (les enfants/anciens sont trop vulnerables).
    if (_owner->stage != LifeStage::Adult)
        return false;

    Creature* predator = sClanMgr->FindNearestPredator(me);
    if (!predator)
        return false;

    _actionTarget = predator->GetGUID();
    _huntTimerMs = HUNT_TIMEOUT_MS;

    me->Attack(predator, true);
    me->GetMotionMaster()->MoveChase(predator);
    return true;
}

bool npc_clan_member::StartRemember()
{
    // On ne se recueille que sur la tombe d'un ancetre (mere/pere). Aucune tombe d'ancetre
    // a portee (ou fondateur sans parents) -> l'action ne demarre pas.
    Position grave;
    if (!sClanMgr->FindAncestorGrave(_owner, me, grave))
        return false;

    _actionTarget.Clear(); // La tombe est une position, pas un objet cible
    _gravePoint = grave;   // donc il faut garder la position en memoire

    Position const dest = GetFacingPosition(me, _gravePoint, 2.5f);
    if (!MoveAlongRoad(dest, MOVE_TO_GRAVE))
        me->GetMotionMaster()->MovePoint(MOVE_TO_GRAVE, dest, true, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
    return true;
}

bool npc_clan_member::StartDrink()
{
    GameObject* water = sClanMgr->FindNearestResourceObject(me, ResourceType::WaterWell);
    if (!water)
        return false;

    _actionTarget = water->GetGUID();
    Position const dest = GetFacingPosition(me, water);
    if (!MoveAlongRoad(dest, MOVE_TO_WELL))
        me->GetMotionMaster()->MovePoint(MOVE_TO_WELL, GetFacingPosition(me, water), true, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
    return true;
}

bool npc_clan_member::StartSleep()
{
    // Priorite : lit attribue > lit le plus proche > maison du clan > position d'origine.
    GameObject* bed = nullptr;
    if (_owner->bedSpawnId)
        bed = me->GetMap()->GetGameObjectBySpawnId(_owner->bedSpawnId);

    if (!bed)
        bed = sClanMgr->FindNearestResourceObject(me, ResourceType::Bed);

    Position dest;
    _actionTarget.Clear();
    if (bed)
    {
        dest = bed->GetPosition();
        _actionTarget = bed->GetGUID(); // le handler s'en sert pour coucher le dormeur SUR le matelas
    }
    else if (_owner->houseSpawnId)
    {
        GameObject* house = me->GetMap()->GetGameObjectBySpawnId(_owner->houseSpawnId);
        dest = house ? house->GetPosition() : _owner->home;
    }
    else
    {
        dest = _owner->home;
    }

    if (!MoveAlongRoad(dest, MOVE_TO_HOME))
        me->GetMotionMaster()->MovePoint(MOVE_TO_HOME, dest, true, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
    return true;
}

void npc_clan_member::ApproachMate(ObjectGuid initiator, Position const& meetPos)
{
    // Ne pas interrompre un reflexe predateur (fuite / defense) : la survie prime.
    if (!_owner || _reflex != Reflex::None)
        return;

    // Interrompt l'interaction ponctuelle en cours (boire/cuire/dormir/recolte/errer)
    // sans toucher la boucle de decision : le mate met sa vie en pause pour venir au RV.
    _scheduler.CancelGroup(GROUP_ACTION);
    _busy = true;
    _currentAction = ActionType::SeekMate;
    _targetNeed = NeedType::Repro;
    _actionTarget = initiator;

    // Se lever, cesser tout mouvement / combat residuel.
    me->SetEmoteState(EMOTE_STATE_NONE);
    if (me->IsInCombat())
        me->CombatStop(true);

    me->AttackStop();

    // Marche vers le point de rencontre dans la maison (l'accouplement est pilote par
    // l'initiateur). L'orientation finale, portee par meetPos, place le partenaire face a lui.
    if (!MoveAlongRoad(meetPos, MOVE_TO_MATE_JOIN, false))
        me->GetMotionMaster()->MovePoint(MOVE_TO_MATE_JOIN, meetPos, true, meetPos.GetOrientation());

    // Garde-fou : si l'initiateur ne nous libere jamais (mort, predateur...), on se
    // libere nous-memes pour ne pas rester bloque en pause indefiniment.
    _scheduler.Schedule(Milliseconds(MATE_APPROACH_TIMEOUT_MS + MATE_DURATION_MS + 2000), GROUP_ACTION, [this](TaskContext /*task*/)
    {
        if (_busy && _currentAction == ActionType::SeekMate && _reflex == Reflex::None)
            ReleaseMate();
    });
}

void npc_clan_member::ReleaseMate()
{
    if (!_owner)
        return;

    // Ne rien faire si le partenaire est deja passe a autre chose (reflexe, action finie).
    if (_reflex != Reflex::None || _currentAction != ActionType::SeekMate)
        return;

    _scheduler.CancelGroup(GROUP_ACTION);
    me->SetEmoteState(EMOTE_STATE_NONE);
    me->GetMotionMaster()->MoveIdle();
    ResetActionState(); // Libere _busy -> la boucle de decision reprend
}

bool npc_clan_member::StartSeekMate()
{
    // Toute impossibilite de s'accoupler APAISE le besoin au lieu de le laisser en l'etat.
    //
    // Sans ca, un besoin urgent que rien ne peut satisfaire (veuvage sans celibataire eligible,
    // population saturee, aucune maison attribuee, partenaire non apparu) fait re-choisir
    // SeekMate a chaque decision : l'action echoue, le besoin reste au maximum, et le membre
    // tourne en rond sans jamais rien faire d'autre -- il fallait le debloquer a la main via
    // `.clan need`.
    //
    // Ce n'est PAS un abandon definitif : le besoin recroit avec le temps, donc le membre
    // retentera plus tard, quand la situation aura peut-etre change (partenaire apparu, place
    // liberee dans la population). On n'applique volontairement PAS de reproCooldownDays ici :
    // aucun accouplement n'a eu lieu, ce serait une double peine.
    //
    // Le pendant de ce garde-fou existe deja cote ClanMgr::Reproduce, qui apaise lui aussi le
    // besoin quand la naissance echoue (pop max, aucun template d'enfant, enfant deja vivant).
    // Ce qui manquait, c'etait le cas ou l'action ne DEMARRE meme pas.
    auto abortMating = [this]() -> bool
    {
        _owner->needs.Satisfy(NeedType::Repro, NEED_MAX);
        return false;
    };

    MemberState* mate = sClanMgr->FindMate(_owner);
    if (!mate || !mate->IsSpawned())
        return abortMating();

    // Reproduction UNIQUEMENT dans la maison : sans maison attribuee, pas d'accouplement.
    // Le rendez-vous se tient dans la maison de l'initiateur (les deux partenaires s'y
    // rejoignent, y compris pour un couple inter-clan).
    GameObject* house = _owner->houseSpawnId ? me->GetMap()->GetGameObjectBySpawnId(_owner->houseSpawnId) : nullptr;
    if (!house)
        return abortMating();

    Creature* mateCreature = ObjectAccessor::GetCreature(*me, mate->liveGuid);
    if (!mateCreature)
        return abortMating();

    npc_clan_member* mateAI = dynamic_cast<npc_clan_member*>(mateCreature->AI());
    if (!mateAI)
        return abortMating();

    _actionTarget = mate->liveGuid;

    // Deux emplacements FACE A FACE dans la maison : on aligne le couple sur l'orientation
    // de la maison, un partenaire de chaque cote du centre (distants de 2*MATE_APPROACH_RANGE),
    // chacun oriente vers l'autre des l'arrivee.
    Position center = house->GetPosition();
    float axis = center.GetOrientation();
    float dx = std::cos(axis) * MATE_APPROACH_RANGE;
    float dy = std::sin(axis) * MATE_APPROACH_RANGE;

    Position myPoint = center;
    myPoint.m_positionX += dx;
    myPoint.m_positionY += dy;

    Position matePoint = center;
    matePoint.m_positionX -= dx;
    matePoint.m_positionY -= dy;

    // Orientation finale : chacun regarde l'emplacement de l'autre (face a face).
    myPoint.SetOrientation(myPoint.GetAbsoluteAngle(&matePoint));
    matePoint.SetOrientation(matePoint.GetAbsoluteAngle(&myPoint));

    // Rendez-vous evenementiel : chacun bascule son drapeau a l'arrivee (aucun sondage).
    // On purge d'abord toute tache d'action residuelle (ex. garde-fou de timeout d'un SeekMate
    // precedent avorte) : sans ca, elle pourrait avorter cette nouvelle rencontre.
    _scheduler.CancelGroup(GROUP_ACTION);
    _selfAtMeet = false;
    _mateAtMeet = false;

    mateAI->ApproachMate(me->GetGUID(), matePoint);

    if (!MoveAlongRoad(myPoint, MOVE_TO_MATE, false))
        me->GetMotionMaster()->MovePoint(MOVE_TO_MATE, myPoint, true, myPoint.GetOrientation());

    // Garde-fou : si la rencontre n'aboutit pas dans le delai (partenaire bloque, initiateur
    // n'arrive jamais...), on libere le partenaire et on abandonne. Annule par TryBeginMating
    // des que l'accouplement demarre.
    _scheduler.Schedule(Milliseconds(MATE_APPROACH_TIMEOUT_MS), GROUP_ACTION, [this](TaskContext /*task*/)
    {
        if (!_busy || _currentAction != ActionType::SeekMate || _reflex != Reflex::None)
            return;

        if (Creature* mateCreature = ObjectAccessor::GetCreature(*me, _actionTarget))
            if (npc_clan_member* mateAI = dynamic_cast<npc_clan_member*>(mateCreature->AI()))
                mateAI->ReleaseMate();

        FinishAction(false); // Rencontre echouee
    });
    return true;
}

void npc_clan_member::TryBeginMating()
{
    // Tant que l'un des deux partenaires n'est pas au point de rencontre, on ne fait rien :
    // l'autre evenement d'arrivee rappellera cette methode.
    if (!_owner || _currentAction != ActionType::SeekMate || _reflex != Reflex::None)
        return;

    if (!_selfAtMeet || !_mateAtMeet)
        return;

    Creature* mate = ObjectAccessor::GetCreature(*me, _actionTarget);
    if (!mate || !mate->IsAlive())
    {
        FinishAction(false); // Partenaire perdu (mort / despawn) pendant l'approche
        return;
    }

    // Exigence conservee : l'accouplement n'a lieu QUE si les deux sont a la maison ET proches.
    // Par construction ils se tiennent a leurs points de RV (dans la maison) ; ce controle ne
    // fait qu'ecarter le cas ou l'un aurait ete repousse.
    GameObject* house = _owner->houseSpawnId ? me->GetMap()->GetGameObjectBySpawnId(_owner->houseSpawnId) : nullptr;

    bool bothAtHome = house && me->IsWithinDist(house, MATE_HOUSE_RADIUS) && mate->IsWithinDist(house, MATE_HOUSE_RADIUS);
    if (!bothAtHome)
    {
        if (npc_clan_member* mateAI = dynamic_cast<npc_clan_member*>(mate->AI()))
            mateAI->ReleaseMate();

        FinishAction(false);
        return;
    }

    // Les deux sont reunis : on annule le garde-fou de timeout, on se fait face, effet RP.
    _scheduler.CancelGroup(GROUP_ACTION);
    SetFacingAction();
    PlayCustomFx();
    PlayCustomFxTarget();

    // Accouplement proprement dit, puis naissance a la fin de la duree.
    _scheduler.Schedule(Milliseconds(MATE_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
    {
        if (MemberState* mate = sClanMgr->GetStateByLiveGuid(_actionTarget))
            sClanMgr->Reproduce(_owner, mate);

        _owner->needs.Satisfy(NeedType::Repro, NEED_MAX);

        // Le partenaire reprend sa vie (son besoin de repro a ete apaise par Reproduce).
        if (Creature* mateCreature = ObjectAccessor::GetCreature(*me, _actionTarget))
            if (npc_clan_member* mateAI = dynamic_cast<npc_clan_member*>(mateCreature->AI()))
                mateAI->ReleaseMate();

        FinishAction(true);
    });
}

void npc_clan_member::NotifyMateArrived(ObjectGuid mate)
{
    // Signal envoye par le partenaire quand il atteint son point de rencontre.
    if (!_owner || _currentAction != ActionType::SeekMate || _reflex != Reflex::None)
        return;

    if (mate != _actionTarget) // Pas le partenaire que l'on attend
        return;

    _mateAtMeet = true;
    TryBeginMating();
}

bool npc_clan_member::StartGatherWood()
{
    // Sac plein de bois : plus rien a ramasser. Le retour est l'affaire de StoreHome.
    if (IsItemFull(ItemType::Wood))
        return false;

    // Le bois alimente le STOCK DE LA MAISON (rallumage par les femmes). On ne ramasse pas
    // sans maison ou si le stock de bois est plein.
    HouseState* house = MyHouse();
    if (!house || house->Get(ItemType::Wood) >= HOUSE_STOCK_MAX)
        return false;

    GameObject* wood = sClanMgr->FindNearestAvailableNode(me, ResourceType::Wood);
    if (!wood)
        return false;

    _actionTarget = wood->GetGUID();
    MoveCloserTo(MOVE_TO_WOOD, wood, 2.0f);
    return true;
}

bool npc_clan_member::StartMineRock()
{
    // Sac plein de pierre : plus rien a extraire. Le retour est l'affaire de StoreHome.
    if (IsItemFull(ItemType::Stone))
        return false;

    // La pierre alimente le STOCK DE LA MAISON (rallumage). On ne mine pas sans maison ou si
    // le stock de pierre est plein.
    HouseState* house = MyHouse();
    if (!house || house->Get(ItemType::Stone) >= HOUSE_STOCK_MAX)
        return false;

    GameObject* rock = sClanMgr->FindNearestAvailableNode(me, ResourceType::Rock);
    if (!rock)
        return false;

    _actionTarget = rock->GetGUID();
    MoveCloserTo(MOVE_TO_ROCK, rock, 2.0f);
    return true;
}

bool npc_clan_member::StartLightFire()
{
    // Rallumer le FOYER DE LA MAISON consomme du bois ET une pierre DU STOCK partage.
    HouseState* house = MyHouse();
    if (!house || house->Get(ItemType::Wood) == 0 || house->Get(ItemType::Stone) == 0)
        return false;

    GameObject* fire = sClanMgr->FindHouseFire(me, _owner->houseSpawnId, false); // Le foyer, eteint
    if (!fire)
        return false;

    _actionTarget = fire->GetGUID();
    Position const dest = GetFacingPosition(me, fire, 3.6f);
    if (!MoveAlongRoad(dest, MOVE_TO_FIRE_LIGHT))
        me->GetMotionMaster()->MovePoint(MOVE_TO_FIRE_LIGHT, dest, true, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
    return true;
}

bool npc_clan_member::StartCook()
{
    // Cuisiner puise la viande crue DU STOCK et produit un repas (ajoute au stock). Inutile
    // si pas de viande, si le stock de repas est plein, ou sans foyer allume a la maison.
    HouseState* house = MyHouse();
    if (!house || house->Get(ItemType::RawFood) == 0 || house->meals >= HOUSE_MEALS_MAX)
        return false;

    GameObject* fire = sClanMgr->FindHouseFire(me, _owner->houseSpawnId, true); // Le foyer, allume
    if (!fire)
        return false;

    _actionTarget = fire->GetGUID();
    Position const dest = GetFacingPosition(me, fire, 3.6f);
    if (!MoveAlongRoad(dest, MOVE_TO_FIRE_COOK))
        me->GetMotionMaster()->MovePoint(MOVE_TO_FIRE_COOK, dest, true, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
    return true;
}

bool npc_clan_member::StartEat()
{
    // On ne depense un repas du stock partage que si on a VRAIMENT faim : sinon on affame
    // les autres (ex. un blesse qui mange en boucle pour se soigner, le ventre plein).
    if (_owner->needs.hunger < EAT_HUNGER_MIN)
        return false;

    // Manger un repas du stock de la maison : tout membre affame, tant qu'il reste un repas.
    HouseState* house = MyHouse();
    if (!house || house->meals == 0)
        return false;

    GameObject* home = MyHouseObject();
    Position const dest = home ? home->GetRandomNearPosition(1.0f) : _owner->home;
    if (!MoveAlongRoad(dest, MOVE_TO_EAT))
        me->GetMotionMaster()->MovePoint(MOVE_TO_EAT, dest, true, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
    return true;
}

bool npc_clan_member::StartShopping()
{
    // Courses (femmes) : aller chez un vendeur et rapporter des repas au stock de la maison.
    if (_shopCdMs > 0)
        return false;

    HouseState* house = MyHouse();
    if (!house || house->meals >= HOUSE_MEALS_MAX)
        return false;

    Creature* vendor = sClanMgr->FindNearestVendor(me);
    if (!vendor)
        return false;

    _actionTarget = vendor->GetGUID();

    // Le vendeur est sur la route du village : on l'emprunte si elle s'applique, sinon
    // deplacement direct comme avant.
    Position const dest = GetFacingPosition(me, vendor);
    if (!MoveAlongRoad(dest, MOVE_TO_VENDOR))
        me->GetMotionMaster()->MovePoint(MOVE_TO_VENDOR, dest, true, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
    return true;
}

Position npc_clan_member::HomeAnchoredDestination(float radius) const
{
    Position anchor = _owner->home;
    if (GameObject* home = MyHouseObject())
        anchor = home->GetPosition();

    // Deja trop loin : on renvoie le foyer (le membre rentre).
    if (me->GetExactDist2d(anchor) > radius)
        return anchor;

    // Sinon, point aleatoire dans le rayon (le pathfinding gere relief/murs).
    float angle = frand(0.0f, 2.0f * float(M_PI));
    float d = frand(2.0f, radius);
    anchor.m_positionX += d * std::cos(angle);
    anchor.m_positionY += d * std::sin(angle);
    return anchor;
}

bool npc_clan_member::StartPlay()
{
    // Joue l'effet si declare.
    PlayCustomFx();

    // Jeu (enfants) : reste dans un rayon autour du FOYER, jamais l'aventure au loin.
    // (Le seul eloignement autorise est la visite au medecin, geree par SeekDoctor.)
    Position const dest = HomeAnchoredDestination(CHILD_HOME_RADIUS);
    if (!MoveAlongRoad(dest, MOVE_TO_PLAY))
        me->GetMotionMaster()->MovePoint(MOVE_TO_PLAY, dest, true, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);

    _scheduler.Schedule(Milliseconds(PLAY_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
    {
        if (_busy && _currentAction == ActionType::Play)
            FinishAction(true, REWARD_PLAY);
    });
    return true;
}

bool npc_clan_member::StartStoreHome()
{
    // Rien a livrer : l'action n'a pas lieu d'etre.
    uint32 carried = 0;
    for (uint8 t = 0; t < uint8(ItemType::Count); ++t)
        carried += GetItemCount(ItemType(t));

    if (carried == 0)
        return false;

    // Aucune maison attribuee (donnee manquante) : impossible de livrer quoi que ce soit. On
    // abandonne la charge sur place plutot que de la garder : le sac resterait plein, la regle
    // deterministe de DecisionTick reclamerait le retour a chaque tick, et le membre boucherait
    // sur un echec punitif jusqu'a sa mort au lieu de reprendre une vie normale.
    if (!MyHouse())
    {
        for (uint8 t = 0; t < uint8(ItemType::Count); ++t)
            ConsumeItem(ItemType(t), GetItemCount(ItemType(t)));

        FinishAction(true, REWARD_TIME_PENALTY); // Ni prime ni punition : le temps perdu, rien de plus
        return true;
    }

    // Deja sur le pas de la porte : on depose sans faire un pas (evite un MovePoint sur une
    // distance nulle, dont le MovementInform n'arriverait pas toujours).
    Position anchor = _owner->home;
    if (GameObject* home = MyHouseObject())
        anchor = home->GetPosition();

    if (me->GetExactDist2d(anchor) <= STORE_REACH_DIST)
    {
        float reward = DepositAllCarried();
        FinishAction(reward > 0.0f, reward);
        return true;
    }

    return GoDepositHome();
}

bool npc_clan_member::GoDepositHome()
{
    // Rentrer deposer la recolte portee au stock de la maison. Si le GameObject maison n'est pas
    // charge (non spawne, hors de portee du grid), on se rabat sur le point de foyer : un membre
    // doit TOUJOURS pouvoir livrer. Sinon chaque tentative echouait, la punition s'accumulait,
    // et il finissait par ne plus oser recolter du tout -- sa recolte bloquee a vie dans son sac.
    Position dest = _owner->home;
    if (GameObject* home = MyHouseObject())
        dest = GetFacingPosition(me, home, 2.0f);

    // Nettoyage du rendu transitoire de la recolte AVANT de marcher : sans ca, le PNJ rentre
    // en gardant l'emote de travail (agenouille a depecer, hache/pioche en main). FinishAction
    // refera ce nettoyage a l'arrivee (idempotent : ResetEquipment est garde par _equipDirty).
    me->SetEmoteState(EMOTE_STATE_NONE);
    ResetEquipment();

    // La maison est sur la route du village : on l'emprunte si elle s'applique, sinon
    // deplacement direct comme avant.
    if (!MoveAlongRoad(dest, MOVE_TO_STORE))
        me->GetMotionMaster()->MovePoint(MOVE_TO_STORE, dest, true, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
    return true;
}

bool npc_clan_member::StartFillTrough(bool water)
{
    // Sac plein : on ne part pas remplir une auge sans avoir livre la recolte au foyer, sinon
    // on gele le stock personnel. Regle deja imposee par DecisionTick pour l'homme, mais on
    // double le garde-fou ici pour couvrir les invocations directes (ForceAction).
    if (IsBagFull())
        return false;

    if (_fillCdMs > 0)
        return false;

    GameObject* trough = sClanMgr->FindFarmEmptyTrough(me);
    if (!trough)
        return false;

    // Un homme sans maison ne devrait pas prendre soin de la ferme du clan (le stock de lait /
    // viande ira dans une maison qui n'existe pas). Sans ce garde-fou, l'action reussit et
    // rien n'est produit.
    if (!MyHouse())
        return false;

    _actionTarget = trough->GetGUID();
    // Deux ids distincts pour eau/paille : evite d'avoir a caser un booleen d'intention dans un
    // champ deja utilise ailleurs (piege classique quand une action est interrompue puis reprise).
    // MoveCloserTo (et non MovePoint direct) : cale le Z sur le sol reel du membre, comme pour
    // toute approche d'un GameObject (cf. le bug MoveCloserAndStop -> PNJ sous la map), et prend
    // automatiquement la route si l'action FillWater/FillStraw en declare une dans custom_clan_path.
    uint32 pointId = water ? MOVE_TO_TROUGH_WATER : MOVE_TO_TROUGH_STRAW;
    MoveCloserTo(pointId, trough, 2.0f);
    return true;
}

bool npc_clan_member::StartButcher()
{
    // Sac plein de viande : plus rien a prelever (comme la chasse).
    if (IsItemFull(ItemType::RawFood))
        return false;

    HouseState* house = MyHouse();
    if (!house || house->Get(ItemType::RawFood) >= HOUSE_STOCK_MAX)
        return false;

    Creature* animal = sClanMgr->FindFarmAnimal(me, false); // vache OU poulet
    if (!animal)
        return false;

    if (!sClanMgr->ClaimAnimal(animal, me))
        return false;

    _actionTarget = animal->GetGUID();
    MoveCloserTo(MOVE_TO_BUTCHER, animal, 1.5f);
    return true;
}

bool npc_clan_member::StartMilkCow()
{
    if (_milkCdMs > 0)
        return false;

    HouseState* house = MyHouse();
    if (!house || house->Get(ItemType::Milk) >= HOUSE_MILK_MAX)
        return false;

    Creature* cow = sClanMgr->FindFarmAnimal(me, true);
    if (!cow)
        return false;

    if (!sClanMgr->ClaimAnimal(cow, me))
        return false;

    _actionTarget = cow->GetGUID();
    MoveCloserTo(MOVE_TO_MILK, cow, 1.5f);
    return true;
}

bool npc_clan_member::StartDrinkMilk()
{
    HouseState* house = MyHouse();
    if (!house || house->Get(ItemType::Milk) == 0)
        return false;

    // On rentre a la maison boire (le lait est un stock du foyer). MEME chemin que Eat :
    // c'est un repli tranquille, on ne prend pas de route de village.
    GameObject* home = MyHouseObject();
    Position dest = home ? home->GetRandomNearPosition(1.0f) : _owner->home;
    if (!MoveAlongRoad(dest, MOVE_TO_DRINK_MILK))
        me->GetMotionMaster()->MovePoint(MOVE_TO_DRINK_MILK, dest, true, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
    return true;
}

Position npc_clan_member::PickWanderDestination()
{
    // Plusieurs directions tirees au hasard : chaque candidat est tronque par un raycast
    // au premier mur (GetFirstCollisionPosition), donc toujours en espace ouvert et en
    // ligne de vue directe. On garde la direction la plus degagee (le rayon le plus long).
    Position best = me->GetPosition();
    float bestDist = 0.0f;

    for (uint8 i = 0; i < WANDER_SAMPLES; ++i)
    {
        float angle = frand(0.0f, 2.0f * float(M_PI));
        float dist = frand(WANDER_MIN_DIST, WANDER_MAX_DIST);
        Position candidate = me->GetFirstCollisionPosition(dist, angle);

        float reached = me->GetExactDist2d(candidate);
        if (reached > bestDist)
        {
            bestDist = reached;
            best = candidate;
        }
    }
    return best;
}

void npc_clan_member::StartWander()
{
    Milliseconds duration = Milliseconds(WANDER_DURATION_MS);

    // Un enfant ne s'eloigne jamais du foyer : meme son errance (choisie par exploration) reste
    // dans le rayon de la maison. Seule la visite au medecin l'en eloigne (SeekDoctor).
    if (_owner->stage == LifeStage::Child)
    {
        me->GetMotionMaster()->MovePoint(MOVE_TO_WANDER, HomeAnchoredDestination(CHILD_HOME_RADIUS), true, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
    }
    // On n'erre qu'en exterieur. A l'interieur (maison, grotte...), on ne vagabonde pas
    // sur place : on rejoint d'abord sa position d'origine (_home), naturellement dehors.
    else if (me->IsOutdoors())
    {
        // Saut vers un point ouvert (raycast anti-mur) plutot que MoveRandom, qui vise un
        // point navmesh parfois colle a un mur ou dans un recoin -> plus de rasage de murs.
        me->GetMotionMaster()->MovePoint(MOVE_TO_WANDER, PickWanderDestination(), true, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
    }
    else
    {
        Position indoorDest = _owner->home;
        if (_owner->houseSpawnId)
            if (GameObject* house = me->GetMap()->GetGameObjectBySpawnId(_owner->houseSpawnId))
                indoorDest = house->GetPosition();

        me->GetMotionMaster()->MovePoint(MOVE_TO_HOME_WANDER, indoorDest, true, {}, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
    }

    _scheduler.Schedule(duration, GROUP_ACTION, [this](TaskContext /*task*/)
    {
        if (_busy && _currentAction == ActionType::Wander)
            FinishAction(true);
    });
}

void npc_clan_member::MoveCloserTo(uint32 pointId, WorldObject* target, float distance, bool forceWalk /*= true*/)
{
    if (!target)
        return;

    MovementWalkRunSpeedSelectionMode const speed = forceWalk
        ? MovementWalkRunSpeedSelectionMode::ForceWalk
        : MovementWalkRunSpeedSelectionMode::ForceRun;

    // Deja assez pres : on se contente de faire face, sans deplacement (meme comportement que
    // MoveCloserAndStop, mais via SetFacing pour ne pas emettre de mouvement inutile).
    float const travel = me->GetExactDist2d(target) - distance;
    if (travel <= 0.0f)
    {
        me->SetFacingToObject(target);
        me->GetMotionMaster()->MovePoint(pointId, me->GetPosition(), false, {}, {}, speed);
        return;
    }

    // Point d'arret sur l'axe qui nous separe de la cible.
    float const angle = me->GetAbsoluteAngle(target);
    float const x = me->GetPositionX() + travel * std::cos(angle);
    float const y = me->GetPositionY() + travel * std::sin(angle);

    // LE point critique. MoveCloserAndStop passe ici target->GetPositionZ() : le Z de la cible
    // alors que (x,y) se situe bien avant elle. Une proie en contrebas donnait donc une
    // destination sous le terrain -- et le pathfinding, ne trouvant pas de polygone la-bas,
    // repliait sur un trajet direct : le PNJ traversait le sol.
    //
    // On part du Z du MEMBRE (l'altitude d'ou il vient, donc du bon ordre de grandeur), puis
    // on le recale sur le sol reel a l'aplomb de (x,y).
    float z = me->GetPositionZ();
    me->UpdateAllowedPositionZ(x, y, z);
    Position const dest(x, y, z);

    // Route declaree pour l'action courante ? On la prend, sinon deplacement direct.
    // Sans ce test, une chasse / un abattage / une traite ne pouvait JAMAIS emprunter la route
    // du village meme si elle s'y appliquait -- MoveCloserTo court-circuitait MoveAlongRoad.
    // La route s'arrete a son point de sortie, puis WaypointPathEnded relance MovePoint(pointId,
    // dest) : le switch de MovementInform reste inchange, forceWalk aussi (retenu par _roadWalk).
    if (MoveAlongRoad(dest, pointId, forceWalk))
        return;

    me->GetMotionMaster()->MovePoint(pointId, dest, true, {}, {}, speed);
}

// ---------------------------------------------------------------------------------------
// Suivi de route (custom_clan_path)
// ---------------------------------------------------------------------------------------
// Id du WaypointPath construit a la volee. Ne doit correspondre a AUCUN chemin de la table
// waypoint_data : il sert uniquement a reconnaitre nos routes dans WaypointPathEnded.
static constexpr uint32 CLAN_ROAD_PATH_ID = 5300100;

// Les 3 phases doivent partager la MEME allure, sinon le changement en cours de trajet
// declenche un recalcul de spline et produit des a-coups.
static MovementWalkRunSpeedSelectionMode RoadSpeedMode(bool walk)
{
    return walk ? MovementWalkRunSpeedSelectionMode::ForceWalk : MovementWalkRunSpeedSelectionMode::ForceRun;
}

// Le trajet se fait en TROIS phases, car aucun mode de MovePath ne convient seul :
//
//   ExactSplinePath ON  -> spline continue et fluide, mais CreateMergedPath n'appelle jamais
//                          PathGenerator : le PNJ irait tout droit, a travers les murs.
//   ExactSplinePath OFF -> CreateSingularPointPath pathfinde vers chaque noeud, mais un noeud
//                          a la fois : le micro-arret revient a chaque point de passage.
//
// D'ou le decoupage : on pathfinde la ou le decor gene (entree/sortie), et on utilise la
// spline exacte la ou elle est sure (la route, declaree a la main en terrain degage).
//
//   Phase 1  MovePoint(MOVE_TO_ROAD_ENTRY)  navmesh ON   sortir de la maison, rejoindre la route
//   Phase 2  MovePath(ExactSplinePath)      navmesh OFF  longer la route, en fluide
//   Phase 3  MovePoint(pointId)             navmesh ON   quitter la route vers la destination
bool npc_clan_member::MoveAlongRoad(Position const& dest, uint32 pointId, bool forceWalk /*= true*/)
{
    // Le choix du point d'entree interroge le navmesh : on passe donc l'unite, pas sa position.
    std::vector<Position> route = Road::BuildRoute(me, _currentAction, dest);

    // Aucune route applicable (action non liee, route trop loin, trajet trop court) :
    // l'appelant fera son MovePoint habituel.
    if (route.empty())
        return false;

    _roadPendingId = pointId;
    _roadPendingDest = dest;
    _roadNodes = std::move(route);
    _roadWalk = forceWalk;

    // Phase 1 : rejoindre l'entree de la route AVEC pathfinding. C'est le seul troncon ou le
    // PNJ peut avoir du decor devant lui (il part potentiellement de l'interieur d'une maison),
    // et donc le seul qui exige le navmesh.
    Position const entry = _roadNodes.front();
    me->GetMotionMaster()->MovePoint(MOVE_TO_ROAD_ENTRY, entry, true, {}, {}, RoadSpeedMode(_roadWalk));
    return true;
}

void npc_clan_member::BeginRoadTravel()
{
    // Phase 2 : la route proprement dite. Le premier noeud est deja atteint (phase 1).
    if (_roadNodes.size() < 2)
    {
        // Route reduite a son seul point d'entree : rien a longer, on enchaine la phase 3.
        WaypointPathEnded(0, CLAN_ROAD_PATH_ID);
        return;
    }

    // Le PNJ est a l'arret a l'entree de la route : c'est le seul moment ou il peut invoquer
    // sa monture sans que le deplacement interrompe l'incantation.
    Milliseconds const castTime = TryMountForRoad();
    if (castTime > Milliseconds(0))
    {
        // On laisse l'incantation s'achever avant de partir. GROUP_ACTION : si l'action est
        // abandonnee entre-temps, la tache est annulee avec elle et le trajet ne demarre pas.
        _scheduler.Schedule(castTime, GROUP_ACTION, [this](TaskContext /*task*/)
        {
            StartRoadPath();
        });
        return;
    }

    StartRoadPath();
}

Milliseconds npc_clan_member::TryMountForRoad()
{
    _roadMountSpell = 0;

    // Un enfant a cheval serait incoherent : ils vont a pied.
    if (_owner->stage == LifeStage::Child)
        return Milliseconds(0);

    // Route trop courte : monter puis demonter couterait plus que le trajet ne fait gagner.
    float length = 0.0f;
    for (std::size_t i = 1; i < _roadNodes.size(); ++i)
        length += _roadNodes[i - 1].GetExactDist2d(_roadNodes[i]);

    if (length < ROAD_MOUNT_MIN_LENGTH)
        return Milliseconds(0);

    uint32 const spellId = ROAD_MOUNT_SPELLS[urand(0, uint32(std::size(ROAD_MOUNT_SPELLS)) - 1)];
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!info)
    {
        TC_LOG_ERROR("scripts", "Clans: sort de monture {} introuvable, trajet a pied.", spellId);
        return Milliseconds(0);
    }

    me->CastSpell(me, spellId, false);
    _roadMountSpell = spellId;

    return Milliseconds(info->CalcCastTime() + ROAD_MOUNT_CAST_MARGIN_MS);
}

void npc_clan_member::StartRoadPath()
{
    if (_roadNodes.size() < 2)
        return;

    std::vector<WaypointNode> nodes;
    nodes.reserve(_roadNodes.size() - 1);
    for (std::size_t i = 1; i < _roadNodes.size(); ++i)
        nodes.emplace_back(uint32(nodes.size()), _roadNodes[i].GetPositionX(),
            _roadNodes[i].GetPositionY(), _roadNodes[i].GetPositionZ());

    // ExactSplinePath : CreateMergedPath fusionne tous les noeuds en UNE spline continue, sans
    // arret intermediaire. Sans navmesh -- ce qui est acceptable ICI et seulement ici : les
    // noeuds sont declares a la main sur la route, donc les segments droits qui les relient
    // sont degages par construction.
    // A cheval on ne va pas au pas : la monture impose l'allure, quelle que soit celle demandee
    // pour le trajet. Seule la phase 2 est concernee -- les phases 1 et 3 se font a pied et
    // conservent _roadWalk.
    bool const walk = _roadWalk && !_roadMountSpell;

    WaypointPath path(CLAN_ROAD_PATH_ID, std::move(nodes),
        walk ? WaypointMoveType::Walk : WaypointMoveType::Run, WaypointPathFlags::ExactSplinePath);

    // L'allure doit etre passee EXPLICITEMENT et etre la meme qu'en phase 1 : laissee a Default,
    // elle pouvait differer de celle du MovePoint precedent, et ce changement d'allure en cours
    // de route positionne MOVEMENTGENERATOR_FLAG_SPEED_UPDATE_PENDING -> StartMove(relaunch),
    // qui recalcule la spline depuis la position courante. D'ou les a-coups en courbe.
    //
    // MovePath copie le chemin (make_unique<WaypointPath>) : lui passer un objet local est sur.
    //
    // 'walk' et NON _roadWalk : le mode de vitesse doit s'accorder au WaypointMoveType ci-dessus,
    // sinon on recree exactement le desaccord d'allure decrit au-dessus -- monte, la course est
    // imposee, et le laisser a ForceWalk relancerait la spline.
    me->GetMotionMaster()->MovePath(path, false, {}, {}, RoadSpeedMode(walk));
}

void npc_clan_member::WaypointReached(uint32 /*waypointId*/, uint32 pathId)
{
    if (pathId != CLAN_ROAD_PATH_ID)
        return;

    // Un noeud franchi = le trajet progresse. Le garde-fou d'action mesure l'ABSENCE DE
    // PROGRES, pas la duree : sans ce reset, une longue route depasse ACTION_TIMEOUT_MS et
    // l'action est soldee en echec avant meme d'arriver (cas de Shopping).
    _actionTimerMs = 0;
}

void npc_clan_member::WaypointPathEnded(uint32 /*waypointId*/, uint32 pathId)
{
    // Le WaypointMovementGenerator n'emet pas nos id de points dans MovementInform (il emet
    // WAYPOINT_MOTION_TYPE avec l'id du noeud) : la fin d'une route ne se voit que'ici.
    if (pathId != CLAN_ROAD_PATH_ID || !_roadPendingId)
        return;

    uint32 const pointId = _roadPendingId;
    Position const dest = _roadPendingDest;
    bool const walk = _roadWalk;
    ClearRoad(); // avant le MovePoint : la route est finie, on n'y revient pas

    // La route vient de s'achever : le trajet progresse, on relance le garde-fou d'action.
    _actionTimerMs = 0;

    // Phase 3 : quitter la route vers la destination reelle, avec pathfinding (le vendeur ou le
    // medecin peut etre en retrait). C'est CE mouvement qui declenche le case MOVE_TO_*.
    //
    // Note : DoFinalize du WaypointMovementGenerator appelle owner->SetWalk(false) en fin de
    // chemin -- d'ou l'allure explicite ici, sans quoi le PNJ finirait le trajet en courant.
    me->GetMotionMaster()->MovePoint(pointId, dest, true, {}, {}, RoadSpeedMode(walk));
}

void npc_clan_member::ClearRoad()
{
    _roadPendingId = 0;
    _roadNodes.clear();

    // On retire l'aura exacte qui a ete lancee (et pas un Dismount() generique) : c'est le
    // pendant symetrique du CastSpell, et ca laisse intacte une eventuelle monture posee
    // ailleurs. Ici aussi bien en fin de route qu'en cas d'interruption -- sans quoi un membre
    // interrompu resterait a cheval pour de bon.
    if (_roadMountSpell)
    {
        me->RemoveAurasDueToSpell(_roadMountSpell);
        _roadMountSpell = 0;
    }

    // Clear() ne touche PAS MOTION_SLOT_DEFAULT (cf. MotionMaster.h), or MovePath s'y installe.
    // Sans ce retrait explicite, une route en cours survit a l'abandon de l'action et le PNJ
    // repart la longer des que le mouvement actif se termine.
    me->GetMotionMaster()->Remove(WAYPOINT_MOTION_TYPE, MOTION_SLOT_DEFAULT);
}

bool npc_clan_member::StartSeekDoctor()
{
    // Action apprise : inutile (et penalisant) d'aller au medecin si on n'est pas afflige.
    if (!sClanMgr->IsDiseased(me))
        return false;

    Creature* doctor = sClanMgr->FindNearestDoctor(me);
    if (!doctor)
        return false; // Pas de medecin en vue

    _actionTarget = doctor->GetGUID();

    // Le medecin est sur la route du village : on l'emprunte si elle s'applique, sinon
    // deplacement direct comme avant.
    Position const dest = GetFacingPosition(me, doctor, 1.8f);
    if (!MoveAlongRoad(dest, MOVE_TO_DOCTOR, false))
        me->GetMotionMaster()->MovePoint(MOVE_TO_DOCTOR, dest);
    return true;
}

void npc_clan_member::MovementInform(uint32 /*type*/, uint32 id)
{
    if (!_owner)
        return;

    // Fin de fuite : le point sur est atteint.
    if (id == MOVE_TO_FLEE)
    {
        EndReflex();
        return;
    }

    if (!_busy)
        return;

    switch (id)
    {
        case MOVE_TO_ROAD_ENTRY: // Entree de la route atteinte -> on la longe (phase 2)
            _actionTimerMs = 0;  // le trajet progresse : on relance le garde-fou d'action
            BeginRoadTravel();
            break;
        case MOVE_TO_WELL: // Arrive a un point d'eau : on boit un moment
        {
            SetFacingAction();
            PlayCustomFx();
            _scheduler.Schedule(Milliseconds(DRINK_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                SetRandomDeceased(AfflictionType::Poison, DISEASE_CHANCE_DRINK);
                _owner->needs.Satisfy(NeedType::Thirst, NEED_MAX);
                FinishAction(true);
            });
            break;
        }
        case MOVE_TO_HOME: // Arrive au lit / a la maison : on dort
        {
            // Dans un lit : se coucher SUR le matelas. On ne peut pas viser un Z eleve via
            // MovePoint (le mouvement replaque sur le navmesh) -> on teleporte le dormeur et on
            // coupe la gravite pour l'y maintenir, oriente dans l'axe du lit.
            if (GameObject* bed = ObjectAccessor::GetGameObject(*me, _actionTarget))
            {
                Position onBed = bed->GetPosition();
                onBed.SetOrientation(bed->GetOrientation());
                me->SetDisableGravity(true);
                me->NearTeleportTo(onBed);
                _sleepElevated = true;
            }
            else
                SetFacingAction();

            PlayCustomFx();
            _scheduler.Schedule(Milliseconds(SLEEP_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                _owner->needs.Satisfy(NeedType::Energy, NEED_MAX);
                FinishAction(true); // FinishAction repose le dormeur au sol (RestoreSleepPosture)
            });
            break;
        }
        case MOVE_TO_MATE: // Initiateur arrive a son point de rencontre
        {
            // Evenementiel : on note notre arrivee. TryBeginMating accouplera quand le
            // partenaire aura lui aussi signale la sienne (plus de sondage de distance).
            _selfAtMeet = true;
            TryBeginMating();
            break;
        }
        case MOVE_TO_MATE_JOIN: // Partenaire arrive au point de rencontre
        {
            SetFacingAction();
            PlayCustomFx();
            // On signale notre arrivee a l'initiateur (_actionTarget = son GUID). C'est lui qui
            // declenche l'accouplement une fois les deux reunis, puis nous libere via ReleaseMate.
            // Le garde-fou d'ApproachMate nous libere si l'initiateur disparait.
            if (Creature* initiator = ObjectAccessor::GetCreature(*me, _actionTarget))
                if (npc_clan_member* initiatorAI = dynamic_cast<npc_clan_member*>(initiator->AI()))
                    initiatorAI->NotifyMateArrived(me->GetGUID());
            break;
        }
        case MOVE_TO_DOCTOR: // Arrive chez le medecin : soin (retrait des afflictions)
        {
            SetFacingAction();
            CastSpellTarget(me);
            _scheduler.Schedule(Milliseconds(DOCTOR_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                sClanMgr->CureDiseases(me);
                FinishAction(true, REWARD_CURE); // Action apprise : soin recompense
            });
            break;
        }
        case MOVE_TO_PREY: // A portee de tir : on abat la proie a l'arme a feu
        {
            Creature* prey = ObjectAccessor::GetCreature(*me, _actionTarget);
            if (!prey || !prey->IsAlive())
            {
                FinishAction(false); // Proie perdue pendant l'approche
                break;
            }

            // L'action est engagee : on sort l'outil du metier s'il est declare.
            me->SetFacingToObject(prey);
            me->SetEmoteState(EMOTE_STATE_HOLD_RIFLE);
            PlayCustomFx();

            // Le coup porte : la proie s'effondre, puis on va la chercher.
            _scheduler
                .Schedule(Milliseconds(HUNT_KILL_DELAY_MS), GROUP_ACTION, [this](TaskContext /*task*/)
                {
                    // Garde OBLIGATOIRE, comme dans l'etape suivante : sans elle, une chasse
                    // avortee (garde-fou de temps, reflexe predateur) verrait quand meme cette
                    // tache appeler FinishAction sur une action deja terminee -> la Q-table
                    // apprendrait sur "Idle", et ResetActionState effacerait le reflexe en cours.
                    if (!_busy || _currentAction != ActionType::Hunt)
                        return;

                    me->SetEmoteState(EMOTE_ONESHOT_ATTACK_RIFLE);

                    Creature* prey = ObjectAccessor::GetCreature(*me, _actionTarget);
                    if (!prey)
                    {
                        FinishAction(false);
                        return;
                    }

                    // KillSelf : la proie meurt sans nous mettre en combat.
                    prey->KillSelf();

                    // La proie est abattue : la chasse est desormais "engagee". On repart d'une
                    // fenetre de garde-fou complete pour la seule phase de prelevement (l'approche
                    // a pu en consommer une grande partie) et on marque le kill : meme si ce
                    // nouveau delai expirait pendant la marche vers la depouille, le garde-fou
                    // recuperera la viande au lieu de gaspiller la proie (cf. UpdateAI).
                    _huntPreyKilled = true;
                    _huntTimerMs = HUNT_TIMEOUT_MS;
                })
                .Schedule(Milliseconds(HUNT_SHOT_DELAY_MS), GROUP_ACTION, [this](TaskContext /*task*/)
                {
                    me->SetEmoteState(EMOTE_STATE_NONE);

                    if (!_busy || _currentAction != ActionType::Hunt)
                        return; // La chasse a ete interrompue entre-temps (garde-fou, reflexe...)

                    Creature* prey = ObjectAccessor::GetCreature(*me, _actionTarget);
                    if (!prey)
                    {
                        FinishAction(false);
                        return;
                    }

                    MoveCloserTo(MOVE_TO_CARCASS, prey, 0.5f);
                });
            break;
        }
        case MOVE_TO_CARCASS: // Arrive sur la depouille : on s'agenouille et on preleve la viande
        {
            SetFacingAction();
            me->SetEmoteState(EMOTE_STATE_LOOT);
            _scheduler.Schedule(Milliseconds(HUNT_LOOT_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                if (!_busy || _currentAction != ActionType::Hunt)
                    return;

                if (Creature* prey = ObjectAccessor::GetCreature(*me, _actionTarget))
                    prey->DespawnOrUnsummon();

                // On garde la viande dans le sac ; la livraison (de tout le sac) se fera quand
                // il sera plein, via l'action StoreHome. La prime est ponderee par ce qui manque
                // au foyer : chasser paie moins si le garde-manger deborde deja de viande, ce
                // qui pousse a aller chercher le bois ou la pierre qui font defaut.
                float mult = ScarcityMult(ItemType::RawFood);
                AddItem(ItemType::RawFood);
                FinishAction(true, REWARD_RAWFOOD * mult);
            });
            break;
        }
        case MOVE_TO_GRAVE: // Arrive sur la tombe d'un ancetre : on se recueille (tradition)
        {
            SetFacingAction(_gravePoint);
            PlayCustomFx(); // Effet de deuil declare (emote/aura) si present
            _scheduler.Schedule(Milliseconds(REMEMBER_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                // Recompense gardee par un cooldown : le camping de tombe ne rapporte plus rien,
                // mais un recueillement espace reste recompense (tradition).
                if (_rememberCdMs == 0)
                {
                    _rememberCdMs = REMEMBER_COOLDOWN_MS;
                    FinishAction(true, REWARD_REMEMBER);
                }
                else
                {
                    // Cooldown actif : le recueillement ne RAPPORTE rien, mais il ne doit pas
                    // couter double. FinishAction applique deja REWARD_TIME_PENALTY a toute
                    // action menee a bien ; repasser cette meme constante en 'shapedReward'
                    // l'appliquait une seconde fois (-0.10 au lieu de -0.05), au point que le
                    // bilan de l'action devenait durablement negatif et que l'agent finissait
                    // par desapprendre completement la tradition.
                    FinishAction(true, 0.0f);
                }
            });
            break;
        }
        case MOVE_TO_WOOD: // Arrive au bois : on ramasse (le noeud s'epuise)
        {
            // Un autre membre a pu prendre le noeud pendant le trajet.
            if (!ObjectAccessor::GetGameObject(*me, _actionTarget))
            {
                FinishAction(false);
                break;
            }
            SetFacingAction();
            PlayCustomFx();
            _scheduler.Schedule(Milliseconds(WOOD_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                GameObject* wood = ObjectAccessor::GetGameObject(*me, _actionTarget);
                if (!wood) // Disparu pendant la recolte : on ne recolte rien
                {
                    FinishAction(false);
                    return;
                }
                sClanMgr->DepleteNode(wood, WOOD_RESPAWN_MS);
                // On garde le bois ; livraison de tout le sac quand il sera plein (StoreHome).
                float mult = ScarcityMult(ItemType::Wood);
                AddItem(ItemType::Wood);
                FinishAction(true, REWARD_WOOD * mult);
            });
            break;
        }
        case MOVE_TO_ROCK: // Arrive a la roche : on mine (le noeud s'epuise)
        {
            // Un autre membre a pu prendre le noeud pendant le trajet.
            if (!ObjectAccessor::GetGameObject(*me, _actionTarget))
            {
                FinishAction(false);
                break;
            }
            SetFacingAction();
            PlayCustomFx();
            _scheduler.Schedule(Milliseconds(STONE_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                GameObject* rock = ObjectAccessor::GetGameObject(*me, _actionTarget);
                if (!rock) // Disparu pendant l'extraction : on ne mine rien
                {
                    FinishAction(false);
                    return;
                }
                sClanMgr->DepleteNode(rock, ROCK_RESPAWN_MS);
                // On garde la pierre ; livraison de tout le sac quand il sera plein (StoreHome).
                float mult = ScarcityMult(ItemType::Stone);
                AddItem(ItemType::Stone);
                FinishAction(true, REWARD_STONE * mult);
            });
            break;
        }
        case MOVE_TO_FIRE_LIGHT: // Arrive au foyer eteint : on le rallume (consomme bois + pierre DU STOCK)
        {
            GameObject* fire = ObjectAccessor::GetGameObject(*me, _actionTarget);
            HouseState* house = MyHouse();
            if (fire && house && house->Get(ItemType::Wood) > 0 && house->Get(ItemType::Stone) > 0)
            {
                SetFacingAction();
                PlayCustomFx();
                house->Take(ItemType::Wood, 1);  // Le rallumage consomme une unite de chaque
                house->Take(ItemType::Stone, 1);
                sClanMgr->LightFire(fire);
                FinishAction(true, REWARD_LIGHT);
            }
            else
                FinishAction(false);
            break;
        }
        case MOVE_TO_FIRE_COOK: // Arive au feu : on ne cuit QUE si le feu est TOUJOURS allume
        {
            // Le feu se consume (FIRE_BURN_DURATION_MS) et a pu s'eteindre pendant le trajet :
            // sans ce controle, le membre "cuisait" sur un foyer mort et se rassasiait quand meme.
            if (!ObjectAccessor::GetGameObject(*me, _actionTarget) || !sClanMgr->IsFireLit(_actionTarget))
            {
                FinishAction(false); // Feu eteint / disparu : cuisson impossible
                break;
            }

            SetFacingAction();
            PlayCustomFx();
            _scheduler.Schedule(Milliseconds(COOK_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                // Re-verification en fin de cuisson : le feu doit etre reste allume tout du long.
                if (!ObjectAccessor::GetGameObject(*me, _actionTarget) || !sClanMgr->IsFireLit(_actionTarget))
                {
                    FinishAction(false); // Le feu s'est eteint pendant la cuisson : repas rate
                    return;
                }

                // La cuisson transforme une viande crue DU STOCK en un repas (ajoute au stock) ;
                // elle ne rassasie plus directement : on mange ensuite via l'action Eat.
                HouseState* house = MyHouse();
                if (!house || house->Get(ItemType::RawFood) == 0)
                {
                    FinishAction(false); // Plus de viande au stock (consommee entre-temps)
                    return;
                }

                SetRandomDeceased(AfflictionType::Disease, DISEASE_CHANCE_COOK);
                house->Take(ItemType::RawFood, 1);
                house->AddMeal(1);
                FinishAction(true, REWARD_MEAL);
            });
            break;
        }
        case MOVE_TO_STORE: // Rentre a la maison : on vide TOUT le sac au stock partage
        {
            // Les TROIS types sont deposes en un seul voyage : le membre revient les bras
            // charges, il n'y a aucune raison de lui faire refaire le trajet par ressource.
            float reward = DepositAllCarried();
            if (reward > 0.0f)
                FinishAction(true, reward);
            else
                FinishAction(false); // Rien a deposer / stock plein / pas de maison
            break;
        }
        case MOVE_TO_EAT: // Rentre a la maison : on mange un repas du stock (rassasie la faim)
        {
            PlayCustomFx(); // Effet du repas (soin PV) si declare (custom_clan_action_fx, action Eat)
            _scheduler.Schedule(Milliseconds(EAT_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                HouseState* house = MyHouse();
                if (!house || !house->TakeMeal(1))
                {
                    FinishAction(false); // Plus de repas (mange par un autre entre-temps)
                    return;
                }
                _owner->needs.Satisfy(NeedType::Hunger, NEED_MAX);
                FinishAction(true, REWARD_EAT);
            });
            break;
        }
        case MOVE_TO_VENDOR: // (Femmes) Arrive chez le vendeur : on achete des repas pour le stock
        {
            SetFacingAction();
            PlayCustomFx();
            PlayCustomFxTarget();
            _scheduler.Schedule(Milliseconds(SHOP_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                if (HouseState* house = MyHouse())
                    house->AddMeal(SHOP_FOOD_AMOUNT);
                _shopCdMs = SHOP_COOLDOWN_MS; // Anti-farm
                FinishAction(true, REWARD_SHOP);
            });
            break;
        }
        case MOVE_TO_PLAY: // (Enfants) Point de jeu atteint
        {
            // La fin de l'action est pilotee par le minuteur arme dans StartPlay.
            break;
        }
        case MOVE_TO_TROUGH_WATER: // (Hommes) Auge vide atteinte : on remplit d'eau
        case MOVE_TO_TROUGH_STRAW: // (Hommes) Auge vide atteinte : on remplit de paille
        {
            GameObject* trough = ObjectAccessor::GetGameObject(*me, _actionTarget);
            if (!trough)
            {
                FinishAction(false);
                break;
            }
            SetFacingAction();
            PlayCustomFx();

            bool const water = (id == MOVE_TO_TROUGH_WATER);
            _scheduler.Schedule(Milliseconds(FILL_TROUGH_DURATION_MS), GROUP_ACTION, [this, water](TaskContext /*task*/)
            {
                GameObject* trough = ObjectAccessor::GetGameObject(*me, _actionTarget);
                if (!trough)
                {
                    FinishAction(false);
                    return;
                }
                sClanMgr->FillTrough(trough, water);
                _fillCdMs = FARM_FILL_COOLDOWN_MS;
                FinishAction(true, REWARD_FILL_TROUGH);
            });
            break;
        }
        case MOVE_TO_BUTCHER: // (Hommes) Bete de la ferme atteinte : abattage et depe�age
        {
            Creature* animal = ObjectAccessor::GetCreature(*me, _actionTarget);
            if (!animal || !animal->IsAlive())
            {
                sClanMgr->ReleaseAnimalClaim(_actionTarget);
                FinishAction(false);
                break;
            }
            SetFacingAction();
            me->SetEmoteState(EMOTE_STATE_LOOT); // s'agenouille pour depecer
            PlayCustomFx();

            _scheduler.Schedule(Milliseconds(BUTCHER_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                Creature* animal = ObjectAccessor::GetCreature(*me, _actionTarget);
                if (!animal)
                {
                    sClanMgr->ReleaseAnimalClaim(_actionTarget);
                    FinishAction(false);
                    return;
                }

                AnimalKind kind = sClanMgr->GetAnimalKind(animal);
                uint32 yield = (kind == AnimalKind::Cow) ? BUTCHER_YIELD_COW : BUTCHER_YIELD_CHICKEN;

                // Tue la bete puis la fait respawn avec un vrai delai (comme un noeud). Le
                // registre d'animaux vivants sera refait quand elle reapparaitra (JustAppeared).
                sClanMgr->UnregisterFarmAnimal(animal);
                sClanMgr->ReleaseAnimalClaim(animal->GetGUID());
                animal->KillSelf();
                animal->DespawnOrUnsummon(0ms, Seconds(FARM_CARCASS_RESPAWN_MS / 1000));

                float mult = ScarcityMult(ItemType::RawFood);
                for (uint32 i = 0; i < yield; ++i)
                    if (!AddItem(ItemType::RawFood))
                        break; // sac plein : on abandonne le reste sur place plutot que de rien recolter
                FinishAction(true, REWARD_BUTCHER * mult);
            });
            break;
        }
        case MOVE_TO_MILK: // (Femmes) Vache atteinte : traite
        {
            Creature* cow = ObjectAccessor::GetCreature(*me, _actionTarget);
            if (!cow || !cow->IsAlive())
            {
                sClanMgr->ReleaseAnimalClaim(_actionTarget);
                FinishAction(false);
                break;
            }
            SetFacingAction();
            PlayCustomFx();

            _scheduler.Schedule(Milliseconds(MILK_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                Creature* cow = ObjectAccessor::GetCreature(*me, _actionTarget);
                sClanMgr->ReleaseAnimalClaim(_actionTarget);
                if (!cow || !cow->IsAlive())
                {
                    FinishAction(false);
                    return;
                }

                // Le lait va DIRECTEMENT dans le stock du foyer : c'est le trait qui compte
                // pour la Q-table (pas un aller-retour depot). Un aller-retour ferait aussi
                // sens realiste, mais decouplerait la trayeuse de la recompense et complexifierait
                // la chaine. On peut y revenir si l'on veut differencier "sac perso" et "foyer".
                HouseState* house = MyHouse();
                if (!house)
                {
                    FinishAction(false);
                    return;
                }
                for (uint32 i = 0; i < MILK_YIELD_PER_MILKING; ++i)
                    if (!house->Add(ItemType::Milk, 1))
                        break;
                _milkCdMs = FARM_MILK_COOLDOWN_MS;
                FinishAction(true, REWARD_MILK);
            });
            break;
        }
        case MOVE_TO_DRINK_MILK: // Rentre boire une ration de lait
        {
            PlayCustomFx();
            _scheduler.Schedule(Milliseconds(DRINK_MILK_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                HouseState* house = MyHouse();
                if (!house || !house->Take(ItemType::Milk, 1))
                {
                    FinishAction(false);
                    return;
                }
                _owner->needs.Satisfy(NeedType::Thirst, NEED_MAX);
                FinishAction(true);
            });
            break;
        }
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Reflexes face aux predateurs (loups / ours)
// ---------------------------------------------------------------------------

void npc_clan_member::JustEngagedWith(Unit* who)
{
    OnThreat(who);
}

void npc_clan_member::DamageTaken(Unit* attacker, uint32& /*damage*/, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/)
{
    // Morsure / griffe d'un animal sauvage : chance d'infliger une affliction (saignement).
    // On evite l'empilement (une affliction a la fois) et on ignore notre propre proie.
    if (_owner && attacker && me->IsHostileTo(attacker)
        && !(_currentAction == ActionType::Hunt && attacker->GetGUID() == _actionTarget)
        && !sClanMgr->IsDiseased(me)
        && roll_chance(DISEASE_CHANCE_PRED))
    {
        if (uint32 aura = sClanMgr->GetRandomDisease(AfflictionType::Bleed))
            me->AddAura(aura, me);
    }

    OnThreat(attacker);
}

void npc_clan_member::OnThreat(Unit* attacker)
{
    if (!_owner || !attacker || _reflex != Reflex::None)
        return;
    if (!me->IsHostileTo(attacker))
        return;
    // Ne pas confondre avec notre propre traque (proie ou predateur qui riposte).
    if ((_currentAction == ActionType::Hunt || _currentAction == ActionType::HuntPredator)
        && attacker->GetGUID() == _actionTarget)
        return;

    // Interrompt l'interaction en cours (boire/cuire/dormir/recolte) sans toucher la boucle de decision.
    _scheduler.CancelGroup(GROUP_ACTION);
    _busy = true;

    // Reveille en sursaut : si on dormait sur un lit, redescendre au sol avant de fuir/combattre
    // (sinon on flotterait en l'air, gravite encore coupee).
    RestoreSleepPosture();

    // Coupe une eventuelle emote de travail
    me->SetEmoteState(EMOTE_STATE_NONE);

    // Adultes : le choix "se defendre / fuir" est APPRI. Enfants/anciens : fuite systematique.
    bool defend = false;
    if (_owner->stage == LifeStage::Adult)
    {
        defend = _owner->mind.ChooseDefend();
        _combatLearned = true;
    }
    else
    {
        _combatLearned = false;
    }

    if (defend)
    {
        _reflex = Reflex::Defend;
        _actionTarget = attacker->GetGUID();
        me->Attack(attacker, true);
        me->GetMotionMaster()->MoveChase(attacker);
    }
    else
    {
        // Fuite vers un feu allume, sinon vers la maison du clan, sinon vers le home.
        _reflex = Reflex::Flee;
        GameObject* fire = sClanMgr->FindNearestLitFire(me);
        Position safe;
        if (fire)
            safe = GetFacingPosition(me, fire);
        else if (_owner->houseSpawnId)
        {
            GameObject* house = me->GetMap()->GetGameObjectBySpawnId(_owner->houseSpawnId);
            safe = house ? house->GetPosition() : _owner->home;
        }
        else
            safe = _owner->home;
        me->GetMotionMaster()->MovePoint(MOVE_TO_FLEE, safe);
    }
}

void npc_clan_member::EndReflex()
{
    // Apprentissage du combat (uniquement pour un choix d'adulte).
    if (_combatLearned && _owner)
    {
        float reward;
        if (_reflex == Reflex::Defend)
            reward = (me->GetHealthPct() >= DEFEND_HURT_HP_PCT) ? REWARD_DEFEND_WIN : REWARD_DEFEND_HURT;
        else
            reward = REWARD_FLEE_SAFE;

        _owner->mind.LearnCombat(_reflex == Reflex::Defend, reward);
        _owner->dirty = true;
        _combatLearned = false;
    }

    _reflex = Reflex::None;
    if (me->IsInCombat())
        me->CombatStop(true);
    me->AttackStop();
    me->GetMotionMaster()->MoveIdle(); // Stoppe la poursuite / la fuite residuelle
    ResetActionState(); // Libere _busy -> la boucle de decision reprend
}

void npc_clan_member::JustDied(Unit* killer)
{
    // Cause de la mort : si personne ne l'a deja renseignee (famine/maladie via le tick de
    // survie, vieillesse via AgingTick), c'est qu'un tiers nous a tue.
    if (_owner && _owner->deathCause == DeathCause::Unknown && killer && killer != me)
        _owner->deathCause = DeathCause::Predator;

    // Mort definitive : on interrompt tout et on retire l'individu de la simulation.
    SpawnGravestone();

    // (Un membre place reapparaitra en frais individu au respawn du coeur ; un
    //  nouveau-ne ne sera pas re-summon.)
    _scheduler.CancelAll();
    if (_owner)
    {
        sClanMgr->KillMember(_owner);
        _owner = nullptr; // L'etat vient d'etre detruit : ne plus y toucher
    }
}
