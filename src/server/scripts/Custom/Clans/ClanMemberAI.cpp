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
#include "Creature.h"
#include "GameObject.h"
#include "GameTime.h"
#include "Map.h"
#include "MotionMaster.h"
#include "MovementDefines.h"
#include "ObjectAccessor.h"
#include "Random.h"
#include "SpellDefines.h"
#include "SpellInfo.h"
#include "WowTime.h"
#include <string>

using namespace Clan;

namespace
{
    // Groupe de la boucle de decision (repetitif, ne doit pas etre annule par un reflexe).
    constexpr uint32 GROUP_DECISION = 1;
    // Groupe des interactions ponctuelles (boire/dormir/cuire/errer) : annulable par un reflexe.
    constexpr uint32 GROUP_ACTION = 2;
}

npc_clan_member::npc_clan_member(Creature* creature) : ScriptedAI(creature),
    _owner(nullptr), _currentAction(ActionType::Idle), _targetNeed(NeedType::None),
    _needBefore(0.0f), _huntTimerMs(0), _busy(false), _reflex(Reflex::None),
    _hasRawFood(false), _hasWood(false), _hasStone(false), _talkCdMs(0), _starveTimerMs(0),
    _diseaseTimerMs(0), _mateWaitMs(0), _rememberCdMs(0), _combatLearned(false)
{
}

void npc_clan_member::JustAppeared()
{
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

    // Lit attribue (registre custom_clan_bed, par entry) : couvre placés et nouveau-nes,
    // et se re-resout a chaque (re)spawn, donc survit a la mort du membre.
    _owner->bedSpawnId = sClanMgr->GetAssignedBed(_owner->entry);
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

    // Decouverte proactive des feux alentour : ils sont ainsi allumes des le depart
    // (sinon ils gardent leur etat de spawn jusqu'a ce qu'un membre aille cuisiner).
    sClanMgr->FindNearestLitFire(me);

    ResetActionState();

    // (Re)arme la boucle de decision.
    _scheduler.CancelGroup(GROUP_DECISION);
    _scheduler.Schedule(Milliseconds(DECISION_INTERVAL_MS), GROUP_DECISION, [this](TaskContext task)
    {
        DecisionTick();
        task.Repeat(Milliseconds(DECISION_INTERVAL_MS));
    });
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
    _actionTarget.Clear();
    _huntTimerMs = 0;
    _reflex = Reflex::None;
    _combatLearned = false;
}

void npc_clan_member::SetFacingAction()
{
    if (_actionTarget.IsCreature())
    {
        Creature* creature = ObjectAccessor::GetCreature(*me, _actionTarget);
        if (!creature)
            return;

        me->SetFacingToObject(creature);
        creature->SetFacingToObject(me);
    }
    else if (WorldObject* actionTarget = ObjectAccessor::GetWorldObject(*me, _actionTarget))
        me->SetFacingToObject(actionTarget);
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
        if (uint32 aura = sClanMgr->GetRandomDisease(type))
        {
            me->AddAura(aura, me);
            return true;
        }

    return false;
}

void npc_clan_member::PlayCustomEmote()
{
    if (Clan::ActionFx const* fx = sClanMgr->GetActionFx(_currentAction))
        if (fx->emote)
            me->SetEmoteState(Emote(fx->emote));
}

void npc_clan_member::CastCustomSpell()
{
    _CastCustomSpell(me);
}

void npc_clan_member::CastCustomSpellTarget()
{
    if (!_actionTarget.IsCreature())
        return;

    Creature* actionTarget = ObjectAccessor::GetCreature(*me, _actionTarget);
    if (!actionTarget)
        return;

    _CastCustomSpell(actionTarget);
}

void npc_clan_member::_CastCustomSpell(Creature* creature) const
{
    if (Clan::ActionFx const* fx = sClanMgr->GetActionFx(_currentAction))
    {
        if (fx->spell)
            creature->CastSpell(creature, fx->spell,
                CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS
                    | TRIGGERED_IGNORE_POWER_COST
                    | TRIGGERED_IGNORE_CASTER_AURASTATE
                    | TRIGGERED_DONT_REPORT_CAST_ERROR)
            );
        if (fx->aura)
            creature->AddAura(fx->aura, creature);
    }
}

void npc_clan_member::SpawnGravestone()
{
    // Deplace d'abord le corps vers un emplacement de cimetiere libre (si disponible),
    // pour que la tombe apparaisse au cimetiere et non sur le lieu de la mort. Faute de
    // place, on retombe sur le comportement d'origine (tombe sur place).
    // Destination de la tombe : par defaut le lieu de la mort. S'il reste un emplacement
    // libre, on prend SA position (connue directement, sans dependre de l'etat post-teleport)
    // et on y deplace le corps. Dans les deux cas la tombe apparait.
    Position dest = me->GetPosition();
    if (GraveyardSlot* slot = sClanMgr->AcquireGraveyardSlot())
    {
        dest = slot->position;
        // Estampille la tombe avec l'identite du defunt : ses descendants pourront venir
        // s'y recueillir (action apprise "Remember"). _owner est encore valide ici (JustDied).
        if (_owner)
            slot->deceasedId = _owner->dbId;
        me->NearTeleportTo(dest);
    }

    me->SetDisplayId(ASHES_DISPLAY_ID);

    uint32 randomEntry = GRAVESTONES[urand(0, GRAVESTONE_COUNT - 1)];
    QuaternionData rot = QuaternionData::fromEulerAnglesZYX(dest.GetOrientation(), 0.0f, 0.0f);
    if (GameObject* gravestone = me->SummonGameObject(randomEntry, dest, rot, 0s))
    {
        Creature* worldFx = gravestone->SummonCreature(WORLD_TRIGGER, gravestone->GetPosition());
        if (!worldFx)
            return;

        worldFx->CastSpell(worldFx, GRAVESTONE_SPOT);
    }
}

MindState npc_clan_member::BuildState() const
{
    MindState s;
    s.urgentNeed = _owner->needs.MostUrgent();
    s.night = IsNightNow();
    s.hasRawFood = _hasRawFood;
    s.hasWood = _hasWood;
    s.hasStone = _hasStone;
    s.litFireNearby = sClanMgr->FindNearestLitFire(me) != nullptr;
    s.diseased = sClanMgr->IsDiseased(me);
    s.predatorNearby = sClanMgr->FindNearestPredator(me) != nullptr;
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

    // Famine : une faim critique ronge les PV (et peut tuer). La regeneration est
    // suspendue tant que le membre a faim, puis retablie une fois rassasie.
    _starveTimerMs += diff;
    if (_starveTimerMs >= STARVE_TICK_MS)
    {
        _starveTimerMs = 0;
        if (_owner->needs.hunger >= HUNGER_STARVE_THRESHOLD && me->IsAlive())
        {
            uint64 dmg = me->CountPctFromMaxHealth(STARVE_DAMAGE_PCT);
            if (!dmg)
                dmg = 1;

            if (me->GetHealth() > dmg)
                me->SetHealth(me->GetHealth() - dmg);
            else
            {
                me->KillSelf(); // mort de faim -> JustDied
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

    // Suivi d'une traque en cours (chasse au gibier OU extermination d'un predateur).
    if (_busy && (_currentAction == ActionType::Hunt || _currentAction == ActionType::HuntPredator))
    {
        Creature* target = ObjectAccessor::GetCreature(*me, _actionTarget);
        if (!target || !target->IsAlive())
        {
            bool killed = target && !target->IsAlive();
            if (killed && _currentAction == ActionType::Hunt)
            {
                _hasRawFood = true; // la proie donne de la viande crue (a cuire)
                FinishAction(true, REWARD_RAWFOOD);
            }
            else if (killed) // predateur extermine
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
}

void npc_clan_member::DecisionTick()
{
    if (!_owner || _busy || _reflex != Reflex::None)
        return;

    _decisionState = BuildState();
    ActionType action = _owner->mind.ChooseAction(_decisionState);
    BeginAction(action);
}

void npc_clan_member::BeginAction(ActionType action)
{
    _busy = true;
    _currentAction = action;
    _targetNeed = _decisionState.urgentNeed;
    _needBefore = _owner->needs.Get(_targetNeed);

    // On tente d'abord de demarrer l'action ; on ne parle qu'ensuite, si elle a
    // reellement commence (evite d'annoncer "je cuisine" alors que le feu est eteint).
    bool started;
    switch (action)
    {
        case ActionType::Hunt:          started = StartHunt();                          break;
        case ActionType::DrinkRiver:    started = StartDrink(ResourceType::WaterRiver); break;
        case ActionType::DrinkWell:     started = StartDrink(ResourceType::WaterWell);  break;
        case ActionType::Sleep:         started = StartSleep();                         break;
        case ActionType::SeekMate:      started = StartSeekMate();                      break;
        case ActionType::GatherWood:    started = StartGatherWood();                    break;
        case ActionType::MineRock:      started = StartMineRock();                      break;
        case ActionType::LightFire:     started = StartLightFire();                     break;
        case ActionType::Cook:          started = StartCook();                          break;
        case ActionType::SeekDoctor:    started = StartSeekDoctor();                    break;
        case ActionType::HuntPredator:  started = StartHuntPredator();                  break;
        case ActionType::Remember:      started = StartRemember();                      break;

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
        // L'action n'a pas pu demarrer : echec, et AUCUNE phrase prononcee.
        FinishAction(false);
        return;
    }

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
}

void npc_clan_member::FinishAction(bool reachedGoal, float shapedReward)
{
    float reward;
    if (reachedGoal)
        reward = (_needBefore - _owner->needs.Get(_targetNeed)) / NEED_MAX + shapedReward + REWARD_TIME_PENALTY;
    else
        reward = REWARD_FAIL;

    // Le cerveau apprend de l'experience.
    _owner->mind.Learn(_decisionState, _currentAction, reward, BuildState());
    _owner->dirty = true;

    // Nettoyage : sortir du combat, arreter le mouvement d'action, se relever.
    if (me->IsInCombat())
        me->CombatStop(true);
    me->AttackStop();
    if (me->GetStandState() != UNIT_STAND_STATE_STAND)
        me->SetStandState(UNIT_STAND_STATE_STAND);
    me->SetEmoteState(EMOTE_STATE_NONE); // coupe une eventuelle emote de travail (bucheron/mineur)

    // IMPORTANT : on arrete tout mouvement residuel (notamment le MoveRandom du Wander,
    // qui tournerait sinon indefiniment). Le PNJ ne bouge que pour une action deliberee.
    me->GetMotionMaster()->MoveIdle();

    ResetActionState();
}

bool npc_clan_member::StartHunt()
{
    // Inutile de chasser si on porte deja de la viande crue (on ne cuisine qu'une piece).
    if (_hasRawFood)
        return false;

    Creature* prey = sClanMgr->FindNearestPrey(me);
    if (!prey)
        return false;

    _actionTarget = prey->GetGUID();
    _huntTimerMs = HUNT_TIMEOUT_MS;
    me->Attack(prey, true);
    me->GetMotionMaster()->MoveChase(prey);
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

    _actionTarget.Clear(); // la tombe est une position, pas un objet cible
    me->GetMotionMaster()->MovePoint(MOVE_TO_GRAVE, GetFacingPosition(grave, 1.5f),
        true, me->GetAbsoluteAngle(grave), {},
        MovementWalkRunSpeedSelectionMode::ForceWalk);
    return true;
}

bool npc_clan_member::StartDrink(ResourceType type)
{
    GameObject* water = sClanMgr->FindNearestResourceObject(me, type);
    if (!water)
        return false;

    _actionTarget = water->GetGUID();
    me->GetMotionMaster()->MovePoint(MOVE_TO_RESOURCE, GetFacingPosition(water, 2.5f),
        true, {}, {},
        MovementWalkRunSpeedSelectionMode::ForceWalk);
    return true;
}

bool npc_clan_member::StartSleep()
{
    // Priorite au lit attribue (bedSpawnId) s'il est present dans le monde ; a defaut, le
    // lit declare le plus proche ; en dernier recours, on rentre a la position d'origine.
    GameObject* bed = nullptr;
    if (_owner->bedSpawnId)
        bed = me->GetMap()->GetGameObjectBySpawnId(_owner->bedSpawnId);
    if (!bed)
        bed = sClanMgr->FindNearestResourceObject(me, ResourceType::Bed);

    Position dest = bed ? bed->GetPosition() : _owner->home;
    me->GetMotionMaster()->MovePoint(MOVE_TO_HOME, dest,
        true, dest.GetAbsoluteAngle(me), {},
        MovementWalkRunSpeedSelectionMode::ForceWalk);
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
    if (me->GetStandState() != UNIT_STAND_STATE_STAND)
        me->SetStandState(UNIT_STAND_STATE_STAND);
    me->SetEmoteState(EMOTE_STATE_NONE);
    if (me->IsInCombat())
        me->CombatStop(true);
    me->AttackStop();

    // Marche vers le point de rencontre (l'accouplement est pilote par l'initiateur).
    me->GetMotionMaster()->MovePoint(MOVE_TO_MATE_JOIN, meetPos,
        true, {}, {},
        MovementWalkRunSpeedSelectionMode::ForceWalk);

    // Garde-fou : si l'initiateur ne nous libere jamais (mort, predateur...), on se
    // libere nous-memes pour ne pas rester bloque en pause indefiniment.
    _scheduler.Schedule(Milliseconds(MATE_APPROACH_TIMEOUT_MS + MATE_DURATION_MS + 2000), GROUP_ACTION,
        [this](TaskContext /*task*/)
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
    ResetActionState(); // libere _busy -> la boucle de decision reprend
}

bool npc_clan_member::StartSeekMate()
{
    MemberState* mate = sClanMgr->FindMate(_owner);
    if (!mate || !mate->IsSpawned())
        return false;

    Creature* mateCreature = ObjectAccessor::GetCreature(*me, mate->liveGuid);
    if (!mateCreature)
        return false;

    // Le partenaire doit etre pilote par notre IA (pour qu'il vienne au RV).
    npc_clan_member* mateAI = dynamic_cast<npc_clan_member*>(mateCreature->AI());
    if (!mateAI)
        return false;

    _actionTarget = mate->liveGuid;

    // Point de rencontre a mi-chemin : les deux partenaires marchent l'un vers l'autre.
    Position meet;
    meet.m_positionX = (me->GetPositionX() + mateCreature->GetPositionX()) / 2.0f;
    meet.m_positionY = (me->GetPositionY() + mateCreature->GetPositionY()) / 2.0f;
    meet.m_positionZ = (me->GetPositionZ() + mateCreature->GetPositionZ()) / 2.0f;

    // Le partenaire vient nous y rejoindre.
    mateAI->ApproachMate(me->GetGUID(), meet);

    // On marche nous aussi vers le point de rencontre.
    _mateWaitMs = MATE_APPROACH_TIMEOUT_MS;
    me->GetMotionMaster()->MovePoint(MOVE_TO_MATE, meet,
        true, {}, {},
        MovementWalkRunSpeedSelectionMode::ForceWalk);
    return true;
}

bool npc_clan_member::StartGatherWood()
{
    // Deja du bois en poche : inutile d'aller en chercher.
    if (_hasWood)
        return false;

    GameObject* wood = sClanMgr->FindNearestAvailableNode(me, ResourceType::Wood);
    if (!wood)
        return false;

    _actionTarget = wood->GetGUID();
    me->GetMotionMaster()->MovePoint(MOVE_TO_WOOD, wood->GetPosition(),
        true, {}, {},
        MovementWalkRunSpeedSelectionMode::ForceWalk,
        2.f);
    return true;
}

bool npc_clan_member::StartMineRock()
{
    // Deja une pierre en poche : inutile de miner.
    if (_hasStone)
        return false;

    GameObject* rock = sClanMgr->FindNearestAvailableNode(me, ResourceType::Rock);
    if (!rock)
        return false;

    _actionTarget = rock->GetGUID();
    me->GetMotionMaster()->MovePoint(MOVE_TO_ROCK, rock->GetPosition(),
        true, {}, {},
        MovementWalkRunSpeedSelectionMode::ForceWalk,
        2.f);
    return true;
}

bool npc_clan_member::StartLightFire()
{
    // Il faut du bois ET une pierre pour rallumer.
    if (!_hasWood || !_hasStone)
        return false;

    GameObject* fire = sClanMgr->FindNearestUnlitFire(me);
    if (!fire)
        return false;

    _actionTarget = fire->GetGUID();
    me->GetMotionMaster()->MovePoint(MOVE_TO_FIRE_LIGHT, GetFacingPosition(fire, 3.6f),
        true, {}, {},
        MovementWalkRunSpeedSelectionMode::ForceWalk);
    return true;
}

bool npc_clan_member::StartCook()
{
    if (!_hasRawFood)
        return false;

    GameObject* fire = sClanMgr->FindNearestLitFire(me);
    if (!fire)
        return false;

    _actionTarget = fire->GetGUID();
    me->GetMotionMaster()->MovePoint(MOVE_TO_FIRE_COOK, GetFacingPosition(fire, 3.6f),
        true, {}, {},
        MovementWalkRunSpeedSelectionMode::ForceWalk
    );
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
        float dist  = frand(WANDER_MIN_DIST, WANDER_MAX_DIST);
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

    // On n'erre qu'en exterieur. A l'interieur (maison, grotte...), on ne vagabonde pas
    // sur place : on rejoint d'abord sa position d'origine (_home), naturellement dehors.
    if (me->IsOutdoors())
    {
        // Saut vers un point ouvert (raycast anti-mur) plutot que MoveRandom, qui vise un
        // point navmesh parfois colle a un mur ou dans un recoin -> plus de rasage de murs.
        me->GetMotionMaster()->MovePoint(MOVE_TO_WANDER, PickWanderDestination(),
            true, {}, {},
            MovementWalkRunSpeedSelectionMode::ForceWalk);
    }
    else
    {
        me->GetMotionMaster()->MovePoint(MOVE_TO_HOME_WANDER, _owner->home,
            true, {}, {},
            MovementWalkRunSpeedSelectionMode::ForceWalk);
    }

    _scheduler.Schedule(duration, GROUP_ACTION, [this](TaskContext /*task*/)
    {
        if (_busy && _currentAction == ActionType::Wander)
            FinishAction(true);
    });
}

bool npc_clan_member::StartSeekDoctor()
{
    // Action apprise : inutile (et penalisant) d'aller au medecin si on n'est pas afflige.
    if (!sClanMgr->IsDiseased(me))
        return false;

    Creature* doctor = sClanMgr->FindNearestDoctor(me);
    if (!doctor)
        return false; // pas de medecin en vue

    _actionTarget = doctor->GetGUID();
    me->GetMotionMaster()->MoveCloserAndStop(MOVE_TO_DOCTOR, doctor, 1.8f);
    return true;
}

void npc_clan_member::MovementInform(uint32 type, uint32 id)
{
    if (type != POINT_MOTION_TYPE || !_owner)
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
        case MOVE_TO_RESOURCE: // arrive a un point d'eau : on boit un moment
        {
            SetFacingAction();
            CastCustomSpell();
            _scheduler.Schedule(Milliseconds(INTERACT_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                SetRandomDeceased(AfflictionType::Poison, DISEASE_CHANCE_DRINK);
                _owner->needs.Satisfy(NeedType::Thirst, NEED_MAX);
                FinishAction(true);
            });
            break;
        }
        case MOVE_TO_HOME: // arrive au lit / a la maison : on dort
        {
            SetFacingAction();
            CastCustomSpell();
            _scheduler.Schedule(Milliseconds(SLEEP_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                _owner->needs.Satisfy(NeedType::Energy, NEED_MAX);
                FinishAction(true);
            });
            break;
        }
        case MOVE_TO_MATE: // arrive au point de rencontre : on attend le partenaire, PUIS on s'accouple
        {
            // Verification periodique : l'accouplement ne demarre qu'une fois les DEUX reunis.
            _scheduler.Schedule(Milliseconds(MATE_POLL_MS), GROUP_ACTION, [this](TaskContext task)
            {
                Creature* mate = ObjectAccessor::GetCreature(*me, _actionTarget);
                if (!mate || !mate->IsAlive())
                {
                    FinishAction(false); // partenaire perdu (mort / despawn)
                    return;
                }

                // Pas encore assez proches : on patiente (dans la limite du garde-fou).
                if (!me->IsWithinDist(mate, INTERACT_RANGE))
                {
                    if (_mateWaitMs <= MATE_POLL_MS)
                    {
                        // Le partenaire n'est jamais arrive : on le libere et on abandonne.
                        if (npc_clan_member* mateAI = dynamic_cast<npc_clan_member*>(mate->AI()))
                            mateAI->ReleaseMate();
                        FinishAction(false);
                        return;
                    }
                    _mateWaitMs -= MATE_POLL_MS;
                    task.Repeat(Milliseconds(MATE_POLL_MS));
                    return;
                }

                // Les deux partenaires sont reunis : on se fait face et on joue l'effet RP.
                SetFacingAction();
                CastCustomSpell();
                CastCustomSpellTarget();

                // Accouplement proprement dit, puis naissance a la fin de la duree.
                _scheduler.Schedule(Milliseconds(MATE_DURATION_MS), GROUP_ACTION, [this](TaskContext /*inner*/)
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
            });
            break;
        }
        case MOVE_TO_MATE_JOIN: // partenaire : arrive au point de rencontre, on se tourne vers l'initiateur et on attend
        {
            SetFacingAction();
            CastCustomSpell();
            // On reste en pause (_busy) : c'est l'initiateur qui declenche l'accouplement
            // une fois reunis, puis nous libere via ReleaseMate. Le garde-fou d'ApproachMate
            // nous libere si l'initiateur disparait.
            break;
        }
        case MOVE_TO_DOCTOR: // arrive chez le medecin : soin (retrait des afflictions)
        {
            SetFacingAction();
            if (Creature* doctor = ObjectAccessor::GetCreature(*me, _actionTarget))
                doctor->CastSpell(me, SPELL_HEAL_DOCTOR, CastSpellExtraArgs(TRIGGERED_IGNORE_POWER_COST | TRIGGERED_IGNORE_CASTER_AURAS));
            _scheduler.Schedule(Milliseconds(DOCTOR_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                if (Creature* doctor = ObjectAccessor::GetCreature(*me, _actionTarget))
                    doctor->SetFacingTo(doctor->GetHomePosition().GetOrientation());
                sClanMgr->CureDiseases(me);
                FinishAction(true, REWARD_CURE); // action apprise : soin recompense
            });
            break;
        }
        case MOVE_TO_GRAVE: // arrive sur la tombe d'un ancetre : on se recueille (tradition)
        {
            CastCustomSpell(); // effet de deuil declare (emote/aura) si present
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
                    FinishAction(true, REWARD_TIME_PENALTY);
            });
            break;
        }
        case MOVE_TO_WOOD: // arrive au bois : on ramasse (le noeud s'epuise)
        {
            // Un autre membre a pu prendre le noeud pendant le trajet.
            if (!ObjectAccessor::GetGameObject(*me, _actionTarget))
            {
                FinishAction(false);
                break;
            }
            SetFacingAction();
            CastCustomSpell();
            _scheduler.Schedule(Milliseconds(WOOD_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                GameObject* wood = ObjectAccessor::GetGameObject(*me, _actionTarget);
                if (!wood) // disparu pendant la recolte : on ne recolte rien
                {
                    FinishAction(false);
                    return;
                }
                sClanMgr->DepleteNode(wood, WOOD_RESPAWN_MS);
                _hasWood = true;
                FinishAction(true, REWARD_WOOD);
            });
            break;
        }
        case MOVE_TO_ROCK: // arrive a la roche : on mine (le noeud s'epuise)
        {
            // Un autre membre a pu prendre le noeud pendant le trajet.
            if (!ObjectAccessor::GetGameObject(*me, _actionTarget))
            {
                FinishAction(false);
                break;
            }
            SetFacingAction();
            CastCustomSpell();
            _scheduler.Schedule(Milliseconds(STONE_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                GameObject* rock = ObjectAccessor::GetGameObject(*me, _actionTarget);
                if (!rock) // disparu pendant l'extraction : on ne mine rien
                {
                    FinishAction(false);
                    return;
                }
                sClanMgr->DepleteNode(rock, ROCK_RESPAWN_MS);
                _hasStone = true;
                FinishAction(true, REWARD_STONE);
            });
            break;
        }
        case MOVE_TO_FIRE_LIGHT: // arrive au feu eteint : on le rallume (consomme bois + pierre)
        {
            if (GameObject* fire = ObjectAccessor::GetGameObject(*me, _actionTarget))
            {
                SetFacingAction();
                CastCustomSpell();
                sClanMgr->LightFire(fire);
                _hasWood = false;
                _hasStone = false;
                FinishAction(true, REWARD_LIGHT);
            }
            else
                FinishAction(false);
            break;
        }
        case MOVE_TO_FIRE_COOK: // arrive au feu allume : on cuit puis on mange
        {
            SetFacingAction();
            CastCustomSpell();
            _scheduler.Schedule(Milliseconds(COOK_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                SetRandomDeceased(AfflictionType::Disease, DISEASE_CHANCE_COOK);
                _owner->needs.Satisfy(NeedType::Hunger, NEED_MAX);
                _hasRawFood = false;
                FinishAction(true, REWARD_COOK);
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
    if (me->GetStandState() != UNIT_STAND_STATE_STAND)
        me->SetStandState(UNIT_STAND_STATE_STAND);
    me->SetEmoteState(EMOTE_STATE_NONE); // coupe une eventuelle emote de travail

    // Adultes : le choix "se defendre / fuir" est APPRIS. Enfants/anciens : fuite systematique.
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
        // Fuite vers un feu allume, sinon vers la maison.
        _reflex = Reflex::Flee;
        GameObject* fire = sClanMgr->FindNearestLitFire(me);
        Position safe = fire ? GetFacingPosition(fire) : _owner->home;
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
    me->GetMotionMaster()->MoveIdle(); // stoppe la poursuite / la fuite residuelle
    ResetActionState(); // libere _busy -> la boucle de decision reprend
}

void npc_clan_member::JustDied(Unit* /*killer*/)
{
    // Mort definitive : on interrompt tout et on retire l'individu de la simulation.
    SpawnGravestone();
     
    // (Un membre place reapparaitra en frais individu au respawn du coeur ; un
    //  nouveau-ne ne sera pas re-summon.)
    _scheduler.CancelAll();
    if (_owner)
    {
        sClanMgr->KillMember(_owner);
        _owner = nullptr; // l'etat vient d'etre detruit : ne plus y toucher
    }
}
