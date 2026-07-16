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

#include "ClanMgr.h"
#include "ClanDatabase.h"
#include "ClanMemberAI.h"
#include "Creature.h"
#include "Duration.h"
#include "GameTime.h"
#include "GameObject.h"
#include "Log.h"
#include "Map.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "Random.h"
#include "ScriptedCreature.h"
#include "TemporarySummon.h"
#include "Weather.h"
#include <algorithm>
#include <list>

using namespace Clan;

namespace
{
    // Les id de naissance vivent au-dessus de cet offset pour ne jamais entrer en
    // collision avec un spawnId de la table creature.
    constexpr uint64 BIRTH_ID_BASE = 0x0000000100000000ULL;
    // Garde-fou anti-explosion demographique.
    constexpr size_t MAX_POPULATION = 40;

    // Vrai si l'un est le parent direct de l'autre (interdit la reproduction parent-enfant).
    // Les fondateurs ont motherId/fatherId = 0, qui ne peut egaler aucun dbId reel.
    bool IsParentChild(Clan::MemberState const* a, Clan::MemberState const* b)
    {
        return b->motherId == a->dbId || b->fatherId == a->dbId
            || a->motherId == b->dbId || a->fatherId == b->dbId;
    }

    // Emplacements fixes des tombes (cimetiere). A la mort, le corps est deplace vers
    // le premier emplacement libre avant l'apparition de la pierre tombale.
    constexpr Position GRAVEYARD_SLOTS[] =
    {
        { 1638.13f, 661.80f, 105.22f, 2.298f },
        { 1640.43f, 665.52f, 105.56f, 2.679f },
        { 1642.45f, 669.59f, 105.88f, 2.679f },
        { 1644.31f, 673.31f, 106.03f, 2.679f },
        { 1636.17f, 672.34f, 106.47f, 2.721f },
        { 1637.95f, 676.31f, 106.76f, 2.721f },
        { 1634.26f, 667.78f, 105.99f, 2.715f },
        { 1631.40f, 664.91f, 105.65f, 2.184f },
        { 1631.84f, 676.62f, 106.93f, 2.754f },
        { 1628.12f, 672.69f, 106.78f, 2.222f },
        { 1623.11f, 670.51f, 106.62f, 1.813f },
    };
}

ClanMgr* ClanMgr::instance()
{
    static ClanMgr instance;
    return &instance;
}

// ---------------------------------------------------------------------------
// Registres
// ---------------------------------------------------------------------------
void ClanMgr::AddResourceEntry(uint32 entry, ResourceType type, ObjectKind kind)
{
    _resourceByEntry[entry] = { type, kind };
}

void ClanMgr::AddMemberTemplate(uint32 entry, ClanId clan, Gender gender, LifeStage stage)
{
    _memberTemplates[entry] = { clan, gender, stage };
}

void ClanMgr::AddBedAssignment(uint32 entry, uint64 bedSpawnId)
{
    if (entry && bedSpawnId)
        _bedByEntry[entry] = bedSpawnId;
}

uint64 ClanMgr::GetAssignedBed(uint32 entry) const
{
    auto it = _bedByEntry.find(entry);
    return it != _bedByEntry.end() ? it->second : 0;
}

void ClanMgr::AddDisplaySet(uint32 entry, uint32 child, uint32 adult, uint32 elder)
{
    _displaysByEntry[entry] = { child, adult, elder };
}

void ClanMgr::AddPhrase(uint8 action, std::string text)
{
    _phrasesByAction[action].push_back(std::move(text));
}

std::string const* ClanMgr::GetRandomPhrase(ActionType action) const
{
    auto it = _phrasesByAction.find(uint8(action));
    if (it == _phrasesByAction.end() || it->second.empty())
        return nullptr;
    return &it->second[urand(0, uint32(it->second.size()) - 1)];
}

void ClanMgr::AddActionFx(uint8 action, uint32 aura, uint32 spell, uint32 emote)
{
    _actionFx[action] = { aura, spell, emote };
}

ActionFx const* ClanMgr::GetActionFx(ActionType action) const
{
    auto it = _actionFx.find(uint8(action));
    return it != _actionFx.end() ? &it->second : nullptr;
}

// ---------------------------------------------------------------------------
// Maladies / medecin
// ---------------------------------------------------------------------------
void ClanMgr::AddDisease(uint32 aura, uint8 type)
{
    if (!aura)
        return;
    _allDiseases.push_back(aura);
    _diseasesByType[type].push_back(aura);
}

uint32 ClanMgr::GetRandomDisease(AfflictionType type) const
{
    auto it = _diseasesByType.find(uint8(type));
    if (it == _diseasesByType.end() || it->second.empty())
        return 0;
    return it->second[urand(0, uint32(it->second.size()) - 1)];
}

bool ClanMgr::IsDiseased(Unit* who) const
{
    if (!who)
        return false;
    for (uint32 aura : _allDiseases)
        if (who->HasAura(aura))
            return true;
    return false;
}

void ClanMgr::CureDiseases(Unit* who) const
{
    if (!who)
        return;
    for (uint32 aura : _allDiseases)
        who->RemoveAurasDueToSpell(aura);
}

uint32 ClanMgr::GetAfflictionMask(Unit* who) const
{
    if (!who)
        return 0;

    uint32 mask = 0;
    for (auto const& [type, auras] : _diseasesByType)
        for (uint32 aura : auras)
            if (who->HasAura(aura))
            {
                mask |= (1u << type);
                break;
            }
    return mask;
}

WorldSummary ClanMgr::GetWorldSummary() const
{
    WorldSummary sum;
    for (auto const& ptr : _states)
    {
        MemberState* m = ptr.get();
        ++sum.population;
        switch (m->stage)
        {
            case LifeStage::Child: ++sum.children; break;
            case LifeStage::Elder: ++sum.elders;   break;
            default:               ++sum.adults;   break;
        }
        if (Creature* c = ResolveLive(m))
            if (IsDiseased(c))
                ++sum.sick;
    }
    return sum;
}

Creature* ClanMgr::FindNearestDoctor(Creature* from) const
{
    Creature* best = nullptr;
    float bestDist = RESOURCE_SEARCH_RANGE;

    for (auto const& [entry, res] : _resourceByEntry)
    {
        if (res.type != ResourceType::Doctor || res.kind != ObjectKind::Creature)
            continue;

        if (Creature* doc = GetClosestCreatureWithEntry(from, entry, SIZE_OF_GRIDS, true))
            best = doc;
    }
    return best;
}

uint32 ClanMgr::GetDisplayId(uint32 entry, LifeStage stage) const
{
    auto it = _displaysByEntry.find(entry);
    return it != _displaysByEntry.end() ? it->second.Get(stage) : 0;
}

MemberState* ClanMgr::AddLoadedState(std::unique_ptr<MemberState> state)
{
    MemberState* raw = state.get();
    _states.push_back(std::move(state));

    if (raw->isBirth)
    {
        _byDbId[raw->dbId] = raw;
        _nextBirthId = std::max(_nextBirthId, raw->dbId);
    }
    else
    {
        // Membre place : on attend que sa creature apparaisse pour la lier.
        _pendingBySpawn[raw->dbId] = raw;
    }
    return raw;
}

// ---------------------------------------------------------------------------
// Cycle de vie serveur
// ---------------------------------------------------------------------------
void ClanMgr::LoadFromDB()
{
    _states.clear();
    _byDbId.clear();
    _pendingBySpawn.clear();
    _resourceByEntry.clear();
    _memberTemplates.clear();
    _displaysByEntry.clear();
    _fires.clear();
    _nodeClaims.clear();
    _phrasesByAction.clear();
    _actionFx.clear();
    _allDiseases.clear();
    _diseasesByType.clear();
    _bedByEntry.clear();
    _nextBirthId = BIRTH_ID_BASE;

    // (Re)initialise le cimetiere : tous les emplacements redeviennent libres. Les tombes
    // sont des GameObjects temporaires (non persistes), donc coherent au (re)chargement.
    _graveyardSlots.clear();
    for (Position const& p : GRAVEYARD_SLOTS)
        _graveyardSlots.push_back({ p, false });

    ClanDatabase::LoadRegistries();
    ClanDatabase::LoadMembers();

    TC_LOG_INFO("scripts", "ClanMgr: {} membre(s) charge(s), {} entry(s) de ressource, {} gabarit(s).",
        _states.size(), _resourceByEntry.size(), _memberTemplates.size());
}

void ClanMgr::RespawnBirths()
{
    for (auto const& state : _states)
    {
        if (!state->isBirth || state->IsSpawned() || !state->birthEntry)
            continue;

        Map* map = sMapMgr->FindMap(state->mapId, 0);
        if (!map)
            continue; // la carte n'est pas chargee : on retentera plus tard

        if (TempSummon* child = map->SummonCreature(state->birthEntry, state->home))
            SpawnBirth(state.get(), child);
    }
}

void ClanMgr::Update(uint32 diff)
{
    _dayTimerMs += diff;
    uint32 const dayLenMs = REAL_SECONDS_PER_SIM_DAY * 1000;
    while (_dayTimerMs >= dayLenMs)
    {
        _dayTimerMs -= dayLenMs;
        AgingTick();
        TryReproductionRound();
    }

    _saveTimerMs += diff;
    if (_saveTimerMs >= SAVE_INTERVAL_MS)
    {
        _saveTimerMs = 0;
        SaveAll(false);
    }

    // Re-tente d'apparaitre les nouveau-nes dont la carte n'etait pas encore chargee.
    _respawnTimerMs += diff;
    if (_respawnTimerMs >= 15000)
    {
        _respawnTimerMs = 0;
        RespawnBirths();
    }

    UpdateFires(diff);
}

void ClanMgr::SaveAll(bool direct)
{
    for (auto const& state : _states)
    {
        if (!state->dirty)
            continue;
        ClanDatabase::SaveMember(*state, direct);
        state->dirty = false;
    }
}

// ---------------------------------------------------------------------------
// Enregistrement des membres places
// ---------------------------------------------------------------------------
MemberState* ClanMgr::RegisterPlacedMember(Creature* creature)
{
    uint64 spawnId = creature->GetSpawnId();
    if (!spawnId)
        return nullptr;

    // Etat charge depuis la base en attente de sa creature.
    if (auto it = _pendingBySpawn.find(spawnId); it != _pendingBySpawn.end())
    {
        MemberState* state = it->second;
        _pendingBySpawn.erase(it);
        _byDbId[spawnId] = state;
        state->liveGuid = creature->GetGUID();
        state->entry = creature->GetEntry(); // entry reelle du spawn (cle des modeles)
        state->mapId = creature->GetMapId();
        state->home = creature->GetHomePosition();
        return state;
    }

    // Deja lie (respawn).
    if (auto it = _byDbId.find(spawnId); it != _byDbId.end())
    {
        it->second->liveGuid = creature->GetGUID();
        return it->second;
    }

    // Premier contact : on classe la creature via le registre de gabarits.
    auto tmpl = _memberTemplates.find(creature->GetEntry());
    if (tmpl == _memberTemplates.end())
    {
        TC_LOG_ERROR("scripts", "ClanMgr: creature entry {} (spawnId {}) a le ScriptName clan "
            "mais n'est declaree dans aucun gabarit (custom_clan_member_template).",
            creature->GetEntry(), spawnId);
        return nullptr;
    }

    auto state = std::make_unique<MemberState>();
    state->dbId = spawnId;
    state->entry = creature->GetEntry();
    state->clan = tmpl->second.clan;
    state->gender = tmpl->second.gender;
    state->stage = tmpl->second.stage;
    state->mapId = creature->GetMapId();
    state->home = creature->GetHomePosition();
    state->liveGuid = creature->GetGUID();
    state->isBirth = false;
    state->dirty = true;

    // Age initial coherent avec l'etape declaree.
    switch (state->stage)
    {
        case LifeStage::Child: state->ageDays = 0; break;
        case LifeStage::Elder: state->ageDays = AGE_ADULT_TO_ELDER_DAYS; break;
        default:               state->ageDays = AGE_CHILD_TO_ADULT_DAYS; break;
    }

    // Modele correspondant a l'etape (0 = garde le modele du creature_template).
    state->displayId = GetDisplayId(state->entry, state->stage);

    MemberState* raw = state.get();
    _states.push_back(std::move(state));
    _byDbId[spawnId] = raw;
    ClanDatabase::SaveMember(*raw, false);
    return raw;
}

void ClanMgr::UnbindLive(MemberState* state)
{
    if (state)
        state->liveGuid.Clear();
}

MemberState* ClanMgr::GetStateByLiveGuid(ObjectGuid guid) const
{
    for (auto const& state : _states)
        if (state->liveGuid == guid)
            return state.get();
    return nullptr;
}

std::vector<ObjectGuid> ClanMgr::GetLiveMemberGuids() const
{
    std::vector<ObjectGuid> out;
    out.reserve(_states.size());
    for (auto const& state : _states)
        if (state->IsSpawned())
            out.push_back(state->liveGuid);
    return out;
}

// ---------------------------------------------------------------------------
// Perception
// ---------------------------------------------------------------------------
Creature* ClanMgr::FindNearestPrey(Creature* from) const
{
    Creature* best = nullptr;
    float bestDist = RESOURCE_SEARCH_RANGE;

    for (auto const& [entry, res] : _resourceByEntry)
    {
        if (res.type != ResourceType::Prey || res.kind != ObjectKind::Creature)
            continue;

        if (Creature* prey = GetClosestCreatureWithEntry(from, entry, RESOURCE_SEARCH_RANGE, true))
        {
            float dist = from->GetDistance(prey);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = prey;
            }
        }
    }
    return best;
}

Creature* ClanMgr::FindNearestPredator(Creature* from) const
{
    Creature* best = nullptr;
    float bestDist = RESOURCE_SEARCH_RANGE;

    for (auto const& [entry, res] : _resourceByEntry)
    {
        if (res.type != ResourceType::Predator || res.kind != ObjectKind::Creature)
            continue;

        if (Creature* pred = GetClosestCreatureWithEntry(from, entry, RESOURCE_SEARCH_RANGE, true))
        {
            float dist = from->GetDistance(pred);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = pred;
            }
        }
    }
    return best;
}

GameObject* ClanMgr::FindNearestResourceObject(Creature* from, ResourceType type) const
{
    GameObject* best = nullptr;
    float bestDist = RESOURCE_SEARCH_RANGE;

    for (auto const& [entry, res] : _resourceByEntry)
    {
        if (res.type != type || res.kind != ObjectKind::GameObject)
            continue;

        if (GameObject* go = GetClosestGameObjectWithEntry(from, entry, RESOURCE_SEARCH_RANGE))
        {
            float dist = from->GetDistance(go);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = go;
            }
        }
    }
    return best;
}

GameObject* ClanMgr::FindNearestAvailableNode(Creature* from, ResourceType type)
{
    ObjectGuid seeker = from->GetGUID();
    uint32 now = GameTime::GetGameTimeMS();

    GameObject* best = nullptr;
    float bestDist = RESOURCE_SEARCH_RANGE;

    for (auto const& [entry, res] : _resourceByEntry)
    {
        if (res.type != type || res.kind != ObjectKind::GameObject)
            continue;

        // GetGameObjectListWithEntryInGrid ne renvoie que les objets presents dans le monde :
        // un noeud epuise (despawne) est donc automatiquement ignore.
        std::list<GameObject*> nodes;
        GetGameObjectListWithEntryInGrid(nodes, from, entry, RESOURCE_SEARCH_RANGE);
        for (GameObject* node : nodes)
        {
            // Ignore les noeuds reserves par un AUTRE membre (reservation non expiree).
            auto claim = _nodeClaims.find(node->GetGUID());
            if (claim != _nodeClaims.end() && claim->second.by != seeker
                && (now - claim->second.atMs) < NODE_CLAIM_TTL_MS)
                continue;

            float dist = from->GetDistance(node);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = node;
            }
        }
    }

    // Reserve le noeud choisi pour ce membre (les autres l'ignoreront le temps du trajet).
    if (best)
        _nodeClaims[best->GetGUID()] = { seeker, now };

    return best;
}

void ClanMgr::DepleteNode(GameObject* node, uint32 respawnMs)
{
    if (!node)
        return;

    _nodeClaims.erase(node->GetGUID()); // la reservation n'a plus lieu d'etre

    // On fait REELLEMENT disparaitre l'objet (valable pour tout type, y compris GENERIC),
    // puis le coeur le fait reapparaitre apres le delai. SetGoState ne suffisait pas :
    // sur un GameObject de type generique, il n'a aucun effet visuel.
    node->DespawnOrUnsummon(0ms, Seconds(respawnMs / 1000));
}

// ---------------------------------------------------------------------------
// Feux
// ---------------------------------------------------------------------------
FireState& ClanMgr::RegisterFire(GameObject* fire, bool outdoor)
{
    auto it = _fires.find(fire->GetGUID());
    if (it == _fires.end())
    {
        FireState fs;
        fs.lit = true; // un feu decouvert est considere allume au depart
        fs.burnMs = FIRE_BURN_DURATION_MS;
        fs.outdoor = outdoor;
        fs.mapId = fire->GetMapId();
        fs.zoneId = fire->GetZoneId();
        it = _fires.emplace(fire->GetGUID(), fs).first;

        // On force l'apparence "allume" des la decouverte (sinon le GO garde son etat
        // de spawn, souvent eteint).
        ApplyFireVisual(fire, true);
    }
    return it->second;
}

GameObject* ClanMgr::FindNearestFire(Creature* from, bool wantLit)
{
    GameObject* best = nullptr;
    float bestDist = RESOURCE_SEARCH_RANGE;

    for (auto const& [entry, res] : _resourceByEntry)
    {
        if (res.type != ResourceType::FireIndoor && res.type != ResourceType::FireOutdoor)
            continue;

        bool outdoor = (res.type == ResourceType::FireOutdoor);
        std::list<GameObject*> fires;
        GetGameObjectListWithEntryInGrid(fires, from, entry, RESOURCE_SEARCH_RANGE);
        for (GameObject* fire : fires)
        {
            FireState& fs = RegisterFire(fire, outdoor);
            if (fs.lit != wantLit)
                continue;

            float dist = from->GetDistance(fire);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = fire;
            }
        }
    }
    return best;
}

GameObject* ClanMgr::FindNearestLitFire(Creature* from)   { return FindNearestFire(from, true); }
GameObject* ClanMgr::FindNearestUnlitFire(Creature* from) { return FindNearestFire(from, false); }

void ClanMgr::LightFire(GameObject* fire)
{
    if (!fire)
        return;

    // Le feu a normalement deja ete enregistre par FindNearestUnlitFire ; sinon on
    // l'enregistre en repli (exterieur inconnu -> false).
    auto it = _fires.find(fire->GetGUID());
    if (it == _fires.end())
        it = _fires.emplace(fire->GetGUID(),
            FireState{ true, FIRE_BURN_DURATION_MS, false, fire->GetMapId(), fire->GetZoneId() }).first;

    it->second.lit = true;
    it->second.burnMs = FIRE_BURN_DURATION_MS;
    ApplyFireVisual(fire, true);
}

void ClanMgr::ApplyFireVisual(GameObject* fire, bool lit) const
{
    // SetGoState FIXE l'etat (contrairement a UseDoorOrButton qui bascule et se
    // reinitialise). Si TON GameObject de feu ne change pas d'apparence via GoState,
    // il ne gere pas d'etat visuel : utilise alors un GO de feu qui a un etat
    // "allume/eteint", ou deux GO distincts a swapper.
    fire->SetGoState(GOState(lit ? FIRE_GOSTATE_LIT : FIRE_GOSTATE_OUT));
}

void ClanMgr::UpdateFires(uint32 diff)
{
    _rainTimerMs += diff;
    bool checkRain = false;
    if (_rainTimerMs >= RAIN_CHECK_INTERVAL_MS)
    {
        _rainTimerMs = 0;
        checkRain = true;
    }

    for (auto& [guid, fs] : _fires)
    {
        if (!fs.lit)
            continue;

        bool extinguish = false;

        // Combustion : seuls les feux EXTERIEURS s'eteignent avec le temps.
        // La cheminee (feu interieur) reste toujours allumee -> il y a toujours ou cuire.
        if (fs.outdoor)
        {
            if (fs.burnMs <= diff)
                extinguish = true;
            else
                fs.burnMs -= diff;
        }

        // Pluie : eteint les feux exterieurs.
        if (!extinguish && checkRain && fs.outdoor)
        {
            if (Map* map = sMapMgr->FindMap(fs.mapId, 0))
            {
                if (Weather* weather = map->GetOrGenerateZoneDefaultWeather(fs.zoneId))
                {
                    WeatherState ws = weather->GetWeatherState();
                    if (ws == WEATHER_STATE_LIGHT_RAIN || ws == WEATHER_STATE_MEDIUM_RAIN
                        || ws == WEATHER_STATE_HEAVY_RAIN || ws == WEATHER_STATE_BLACKRAIN)
                        extinguish = true;
                }
            }
        }

        if (extinguish)
        {
            fs.lit = false;
            if (Map* map = sMapMgr->FindMap(fs.mapId, 0))
                if (GameObject* fire = map->GetGameObject(guid))
                    ApplyFireVisual(fire, false);
        }
    }
}

// ---------------------------------------------------------------------------
// Reproduction
// ---------------------------------------------------------------------------
MemberState* ClanMgr::FindMate(MemberState* self) const
{
    if (!self || self->stage != LifeStage::Adult || self->reproCooldownDays > 0 || !self->needs.IsWellFed())
        return nullptr;
    if (_states.size() >= MAX_POPULATION)     // population saturee : inutile de chercher
        return nullptr;

    for (auto const& other : _states)
    {
        MemberState* mate = other.get();
        if (mate == self || !mate->IsSpawned())
            continue;
        if (mate->stage != LifeStage::Adult || mate->reproCooldownDays > 0 || !mate->needs.IsWellFed())
            continue;
        if (mate->gender == self->gender)     // il faut des genres opposes
            continue;
        if (IsParentChild(self, mate))        // pas de reproduction parent-enfant
            continue;
        return mate;
    }
    return nullptr;
}

uint32 ClanMgr::PickBirthEntry(ClanId clan, Gender gender) const
{
    for (auto const& [entry, tmpl] : _memberTemplates)
        if (tmpl.clan == clan && tmpl.gender == gender && tmpl.stage == LifeStage::Child)
            return entry;
    return 0;
}

uint64 ClanMgr::AllocateBirthId()
{
    if (_nextBirthId < BIRTH_ID_BASE)
        _nextBirthId = BIRTH_ID_BASE;
    return ++_nextBirthId;
}

void ClanMgr::Reproduce(MemberState* a, MemberState* b)
{
    if (!a || !b || a == b)
        return;
    if (a->reproCooldownDays > 0 || b->reproCooldownDays > 0)
    {
        TC_LOG_DEBUG("scripts", "ClanMgr::Reproduce annulee : cooldown actif (a={} b={}).", a->dbId, b->dbId);
        return;
    }
    if (a->gender == b->gender)
    {
        TC_LOG_DEBUG("scripts", "ClanMgr::Reproduce annulee : meme genre (a={} b={}).", a->dbId, b->dbId);
        return;
    }
    if (IsParentChild(a, b)) // interdit la reproduction parent-enfant
    {
        TC_LOG_DEBUG("scripts", "ClanMgr::Reproduce annulee : lien parent-enfant (a={} b={}).", a->dbId, b->dbId);
        return;
    }
    if (_states.size() >= MAX_POPULATION)
    {
        TC_LOG_DEBUG("scripts", "ClanMgr::Reproduce annulee : population max atteinte.");
        return;
    }

    // Un enfant herite d'un clan parental (choix aleatoire) et d'un genre aleatoire.
    ClanId childClan = urand(0, 1) ? a->clan : b->clan;
    Gender childGender = urand(0, 1) ? Gender::Male : Gender::Female;

    uint32 entry = PickBirthEntry(childClan, childGender);
    if (!entry)
    {
        TC_LOG_ERROR("scripts", "ClanMgr: aucun gabarit enfant declare pour clan {} genre {} ; "
            "naissance annulee.", uint32(childClan), uint32(childGender));
        return;
    }

    MemberState* mother = (a->gender == Gender::Female) ? a : b;
    MemberState* father = (a->gender == Gender::Male) ? a : b;

    auto child = std::make_unique<MemberState>();
    child->dbId = AllocateBirthId();
    child->clan = childClan;
    child->gender = childGender;
    child->stage = LifeStage::Child;
    child->ageDays = 0;
    child->entry = entry;                                       // l'enfant garde cette entry en grandissant
    child->displayId = GetDisplayId(entry, LifeStage::Child);
    child->isBirth = true;
    child->birthEntry = entry;
    child->motherId = mother->dbId;
    child->fatherId = father->dbId;
    child->mapId = mother->mapId;
    child->home = mother->home;
    child->mind.InheritFrom(a->mind, b->mind); // heritage genetique de l'apprentissage
    child->dirty = true;

    MemberState* raw = child.get();
    _states.push_back(std::move(child));
    _byDbId[raw->dbId] = raw;

    // Cooldowns / apaisement du besoin de reproduction.
    a->reproCooldownDays = REPRO_COOLDOWN_DAYS;
    b->reproCooldownDays = REPRO_COOLDOWN_DAYS;
    a->needs.reproUrge = 0.0f;
    b->needs.reproUrge = 0.0f;
    a->dirty = b->dirty = true;

    // Apparition immediate si la mere est dans le monde.
    if (Creature* motherCreature = ResolveLive(mother))
        SpawnBirth(raw, motherCreature);

    ClanDatabase::SaveMember(*raw, false);

    TC_LOG_INFO("scripts", "ClanMgr: naissance (dbId {}) clan {} de mere {} et pere {}.",
        raw->dbId, uint32(childClan), mother->dbId, father->dbId);
}

void ClanMgr::SpawnBirth(MemberState* state, WorldObject* summoner)
{
    if (!state || !summoner)
        return;

    if (TempSummon* child = summoner->SummonCreature(state->birthEntry, summoner->GetPosition()))
    {
        state->home = child->GetPosition();
        state->mapId = child->GetMapId();
        if (npc_clan_member* ai = dynamic_cast<npc_clan_member*>(child->AI()))
            ai->BindState(state);
    }
}

// ---------------------------------------------------------------------------
// Cimetiere
// ---------------------------------------------------------------------------
GraveyardSlot* ClanMgr::AcquireGraveyardSlot()
{
    for (GraveyardSlot& slot : _graveyardSlots)
        if (!slot.full)
        {
            slot.full = true;
            return &slot;
        }
    return nullptr; // tous les emplacements sont occupes
}

bool ClanMgr::FindAncestorGrave(MemberState const* seeker, Creature* from, Position& out) const
{
    if (!seeker || !from)
        return false;
    // Un fondateur (parents = 0) n'a pas d'ancetre a honorer.
    if (!seeker->motherId && !seeker->fatherId)
        return false;

    GraveyardSlot const* best = nullptr;
    float bestDist = RESOURCE_SEARCH_RANGE;

    for (GraveyardSlot const& slot : _graveyardSlots)
    {
        if (!slot.full)
            continue;
        if (slot.deceasedId != seeker->motherId && slot.deceasedId != seeker->fatherId)
            continue;

        float dist = from->GetDistance(slot.position);
        if (dist < bestDist)
        {
            bestDist = dist;
            best = &slot;
        }
    }

    if (!best)
        return false;

    out = best->position;
    return true;
}

// ---------------------------------------------------------------------------
// Vieillissement
// ---------------------------------------------------------------------------
void ClanMgr::AgingTick()
{
    std::vector<MemberState*> dying;

    for (auto const& ptr : _states)
    {
        MemberState* state = ptr.get();
        ++state->ageDays;
        if (state->reproCooldownDays > 0)
            --state->reproCooldownDays;
        state->dirty = true;

        // Transitions d'etape : on met a jour l'etape, le modele (displayId) et l'echelle.
        LifeStage previous = state->stage;
        if (state->stage == LifeStage::Child && state->ageDays >= AGE_CHILD_TO_ADULT_DAYS)
            state->stage = LifeStage::Adult;
        else if (state->stage == LifeStage::Adult && state->ageDays >= AGE_ADULT_TO_ELDER_DAYS)
            state->stage = LifeStage::Elder;

        if (state->stage != previous)
        {
            state->displayId = GetDisplayId(state->entry, state->stage);
            if (Creature* c = ResolveLive(state))
            {
                if (state->displayId)
                    c->SetDisplayId(state->displayId);
                c->SetObjectScale(state->stage == LifeStage::Child ? CHILD_SCALE : 1.0f);
            }
        }

        // Presage : un Ancien qui approche de sa mort annonce (une fois) que sa fin est proche.
        if (state->stage == LifeStage::Elder && !state->deathOmenSaid
            && state->ageDays >= AGE_DEATH_DAYS - AGE_DEATH_WARNING_DAYS
            && state->ageDays < AGE_DEATH_DAYS)
        {
            state->deathOmenSaid = true;
            if (Creature* c = ResolveLive(state))
                if (std::string const* phrase = GetRandomPhrase(ActionType(PHRASE_DEATH_OMEN)))
                    c->Say(*phrase, LANG_UNIVERSAL);
        }

        if (state->ageDays >= AGE_DEATH_DAYS)
            dying.push_back(state);
    }

    for (MemberState* state : dying)
    {
        if (Creature* c = ResolveLive(state))
            c->KillSelf();
    }
}

void ClanMgr::RemoveMemberState(uint64 dbId)
{
    _byDbId.erase(dbId);
    _pendingBySpawn.erase(dbId);
    ClanDatabase::DeleteMember(dbId, false);

    _states.erase(std::remove_if(_states.begin(), _states.end(),
        [dbId](std::unique_ptr<MemberState> const& s) { return s->dbId == dbId; }), _states.end());
}

void ClanMgr::KillMember(MemberState* state)
{
    uint64 dbId = state->dbId;
    RemoveMemberState(dbId);
    TC_LOG_INFO("scripts", "ClanMgr: membre {} est en mort definitive.", dbId);
}

void ClanMgr::TryReproductionRound()
{
    if (_states.size() >= MAX_POPULATION)
        return;

    // Appariement global de secours : un enfant au plus par tour.
    for (auto const& ptrA : _states)
    {
        MemberState* a = ptrA.get();
        if (!a->IsSpawned() || a->stage != LifeStage::Adult || a->reproCooldownDays > 0 || !a->needs.IsWellFed())
            continue;

        if (MemberState* b = FindMate(a))
        {
            Reproduce(a, b);
            return;
        }
    }
}

Creature* ClanMgr::ResolveLive(MemberState const* state) const
{
    if (!state || state->liveGuid.IsEmpty())
        return nullptr;

    Map* map = sMapMgr->FindMap(state->mapId, 0);
    return map ? map->GetCreature(state->liveGuid) : nullptr;
}
