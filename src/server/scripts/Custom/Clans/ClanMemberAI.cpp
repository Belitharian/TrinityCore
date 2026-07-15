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
#include "MotionMaster.h"
#include "MovementDefines.h"
#include "ObjectAccessor.h"
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
    _state(nullptr), _currentAction(ActionType::Idle), _targetNeed(NeedType::None),
    _needBefore(0.0f), _huntTimerMs(0), _busy(false), _reflex(Reflex::None),
    _hasRawFood(false), _hasWood(false), _hasStone(false), _talkCdMs(0), _starveTimerMs(0)
{
}

void npc_clan_member::JustAppeared()
{
    // Les nouveau-nes recoivent leur etat via BindState (appele par ClanMgr) ;
    // les membres places par l'admin s'enregistrent ici.
    if (!_state && me->GetSpawnId())
        BindState(sClanMgr->RegisterPlacedMember(me));
    else if (_state)
        ResetActionState();
}

void npc_clan_member::BindState(MemberState* state)
{
    _state = state;
    if (!_state)
        return;

    _state->liveGuid = me->GetGUID();

    // Rendu visuel de l'etape de vie : modele (displayId) + echelle.
    if (!_state->displayId)
        _state->displayId = sClanMgr->GetDisplayId(_state->clan, _state->gender, _state->stage);
    if (_state->displayId)
        me->SetDisplayId(_state->displayId);
    me->SetObjectScale(_state->stage == LifeStage::Child ? CHILD_SCALE : 1.0f);

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
}

bool npc_clan_member::IsNightNow()
{
    if (WowTime const* t = GameTime::GetWowTime())
        return Clan::IsNight(uint8(t->GetHour()));
    return false;
}

MindState npc_clan_member::CurrentMindState() const
{
    if (!_state)
        return MindState();
    return BuildState();
}

MindState npc_clan_member::BuildState() const
{
    MindState s;
    s.urgentNeed = _state->needs.MostUrgent();
    s.night = IsNightNow();
    s.hasRawFood = _hasRawFood;
    s.hasWood = _hasWood;
    s.hasStone = _hasStone;
    s.litFireNearby = sClanMgr->FindNearestLitFire(me) != nullptr;
    return s;
}

void npc_clan_member::UpdateAI(uint32 diff)
{
    _scheduler.Update(diff);

    if (!_state)
        return;

    // Les besoins croissent en continu (seuls les adultes ont un besoin de reproduction).
    _state->needs.Decay(diff, IsNightNow(), _state->stage == LifeStage::Adult);

    // Cooldown de parole.
    if (_talkCdMs > diff)
        _talkCdMs -= diff;
    else
        _talkCdMs = 0;

    // Famine : une faim critique ronge les PV (et peut tuer). La regeneration est
    // suspendue tant que le membre a faim, puis retablie une fois rassasie.
    _starveTimerMs += diff;
    if (_starveTimerMs >= STARVE_TICK_MS)
    {
        _starveTimerMs = 0;
        if (_state->needs.hunger >= HUNGER_STARVE_THRESHOLD && me->IsAlive())
        {
            me->SetRegenerateHealth(false);
            uint64 dmg = me->CountPctFromMaxHealth(STARVE_DAMAGE_PCT);
            if (!dmg)
                dmg = 1;

            if (me->GetHealth() > dmg)
                me->SetHealth(me->GetHealth() - dmg);
            else
            {
                me->KillSelf(); // mort de faim -> JustDied -> ClanMgr::OnMemberKilled
                return;         // _state est desormais libere : on ne touche plus a rien
            }
        }
        else
        {
            me->SetRegenerateHealth(true);
        }
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

    // Suivi d'une chasse en cours (le combat n'a pas de MovementInform d'arrivee).
    if (_busy && _currentAction == ActionType::Hunt)
    {
        Creature* prey = ObjectAccessor::GetCreature(*me, _actionTarget);
        if (!prey || !prey->IsAlive())
        {
            bool killed = prey && !prey->IsAlive();
            if (killed)
                _hasRawFood = true; // la proie donne de la viande crue (a cuire)
            FinishAction(killed, killed ? REWARD_RAWFOOD : 0.0f);
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
    if (!_state || _busy || _reflex != Reflex::None)
        return;

    _decisionState = BuildState();
    ActionType action = _state->mind.ChooseAction(_decisionState);
    BeginAction(action);
}

void npc_clan_member::BeginAction(ActionType action)
{
    _busy = true;
    _currentAction = action;
    _targetNeed = _decisionState.urgentNeed;
    _needBefore = _state->needs.Get(_targetNeed);

    // On tente d'abord de demarrer l'action ; on ne parle qu'ensuite, si elle a
    // reellement commence (evite d'annoncer "je cuisine" alors que le feu est eteint).
    bool started;
    switch (action)
    {
        case ActionType::Hunt:       started = StartHunt();                          break;
        case ActionType::DrinkRiver: started = StartDrink(ResourceType::WaterRiver); break;
        case ActionType::DrinkWell:  started = StartDrink(ResourceType::WaterWell);  break;
        case ActionType::Sleep:      started = StartSleep();                         break;
        case ActionType::SeekMate:   started = StartSeekMate();                      break;
        case ActionType::GatherWood: started = StartGatherWood();                    break;
        case ActionType::MineRock:   started = StartMineRock();                      break;
        case ActionType::LightFire:  started = StartLightFire();                     break;
        case ActionType::Cook:       started = StartCook();                          break;
        case ActionType::Wander:     StartWander(); started = true;                  break;
        case ActionType::Idle:
        default:                     started = true;                                 break;
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
        reward = (_needBefore - _state->needs.Get(_targetNeed)) / NEED_MAX + shapedReward + REWARD_TIME_PENALTY;
    else
        reward = REWARD_FAIL;

    // Le cerveau apprend de l'experience.
    _state->mind.Learn(_decisionState, _currentAction, reward, BuildState());
    _state->dirty = true;

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

bool npc_clan_member::StartDrink(ResourceType type)
{
    GameObject* water = sClanMgr->FindNearestResourceObject(me, type);
    if (!water)
        return false;

    _actionTarget = water->GetGUID();
    me->GetMotionMaster()->MovePoint(MOVE_TO_RESOURCE, water->GetPosition());
    return true;
}

bool npc_clan_member::StartSleep()
{
    // On prefere un lit/campement declare, sinon on rentre a la position d'origine.
    GameObject* bed = sClanMgr->FindNearestResourceObject(me, ResourceType::Bed);
    Position dest = bed ? bed->GetPosition() : _state->home;
    me->GetMotionMaster()->MovePoint(MOVE_TO_HOME, dest);
    return true;
}

bool npc_clan_member::StartSeekMate()
{
    MemberState* mate = sClanMgr->FindMate(_state);
    if (!mate || !mate->IsSpawned())
        return false;

    Creature* mateCreature = ObjectAccessor::GetCreature(*me, mate->liveGuid);
    if (!mateCreature)
        return false;

    _actionTarget = mate->liveGuid;
    me->GetMotionMaster()->MovePoint(MOVE_TO_MATE, mateCreature->GetPosition(),
        true, {}, {},
        MovementWalkRunSpeedSelectionMode::ForceWalk,
        0.8f);
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
        1.5f);
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
        1.5f);
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
    me->GetMotionMaster()->MovePoint(MOVE_TO_FIRE_LIGHT, fire->GetPosition(),
        true, {}, {},
        MovementWalkRunSpeedSelectionMode::ForceWalk,
        1.5f);
    return true;
}

bool npc_clan_member::StartCook()
{
    // Il faut de la viande crue ET un feu allume.
    if (!_hasRawFood)
        return false;

    GameObject* fire = sClanMgr->FindNearestLitFire(me);
    if (!fire)
        return false;

    _actionTarget = fire->GetGUID();
    me->GetMotionMaster()->MovePoint(MOVE_TO_FIRE_COOK, fire->GetPosition(),
        true, {}, {},
        MovementWalkRunSpeedSelectionMode::ForceWalk,
        1.5f);
    return true;
}

void npc_clan_member::StartWander()
{
    me->GetMotionMaster()->MoveRandom(8.0f);
    _scheduler.Schedule(Milliseconds(2500), GROUP_ACTION, [this](TaskContext /*task*/)
    {
        if (_busy && _currentAction == ActionType::Wander)
            FinishAction(true);
    });
}

void npc_clan_member::MovementInform(uint32 type, uint32 id)
{
    if (type != POINT_MOTION_TYPE || !_state)
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
            me->SetStandState(UNIT_STAND_STATE_KNEEL);
            _scheduler.Schedule(Milliseconds(INTERACT_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                _state->needs.Satisfy(NeedType::Thirst, NEED_MAX);
                FinishAction(true);
            });
            break;
        }
        case MOVE_TO_HOME: // arrive au lit / a la maison : on dort
        {
            me->SetStandState(UNIT_STAND_STATE_SLEEP);
            _scheduler.Schedule(Milliseconds(INTERACT_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                _state->needs.Satisfy(NeedType::Energy, NEED_MAX);
                FinishAction(true);
            });
            break;
        }
        case MOVE_TO_MATE: // arrive pres du partenaire : reproduction
        {
            me->HandleEmoteCommand(EMOTE_ONESHOT_KISS);
            _scheduler.Schedule(Milliseconds(INTERACT_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                if (MemberState* mate = sClanMgr->GetStateByLiveGuid(_actionTarget))
                    sClanMgr->Reproduce(_state, mate);
                _state->needs.Satisfy(NeedType::Repro, NEED_MAX);
                FinishAction(true);
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
            me->SetEmoteState(EMOTE_STATE_WORK_CHOPWOOD_LUMBER_AXE2);
            _scheduler.Schedule(Milliseconds(INTERACT_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                me->SetEmoteState(EMOTE_STATE_NONE);
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
            me->SetEmoteState(EMOTE_STATE_WORK_MINING_NO_COMBAT);
            _scheduler.Schedule(Milliseconds(INTERACT_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                me->SetEmoteState(EMOTE_STATE_NONE);
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
            me->SetStandState(UNIT_STAND_STATE_KNEEL);
            _scheduler.Schedule(Milliseconds(COOK_DURATION_MS), GROUP_ACTION, [this](TaskContext /*task*/)
            {
                _state->needs.Satisfy(NeedType::Hunger, NEED_MAX);
                _hasRawFood = false;

                // Manger restaure explicitement les PV (lien faim -> PV, sans dependre
                // de la regeneration passive du coeur, qui est conditionnelle).
                me->SetRegenerateHealth(true);
                me->SetFullHealth();

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
    OnThreat(attacker);
}

void npc_clan_member::OnThreat(Unit* attacker)
{
    if (!_state || !attacker || _reflex != Reflex::None)
        return;
    if (!me->IsHostileTo(attacker))
        return;
    // Ne pas confondre avec notre propre chasse (la proie qui riposte).
    if (_currentAction == ActionType::Hunt && attacker->GetGUID() == _actionTarget)
        return;

    // Interrompt l'interaction en cours (boire/cuire/dormir/recolte) sans toucher la boucle de decision.
    _scheduler.CancelGroup(GROUP_ACTION);
    _busy = true;
    if (me->GetStandState() != UNIT_STAND_STATE_STAND)
        me->SetStandState(UNIT_STAND_STATE_STAND);
    me->SetEmoteState(EMOTE_STATE_NONE); // coupe une eventuelle emote de travail

    if (_state->stage == LifeStage::Adult)
    {
        // Les adultes se defendent.
        _reflex = Reflex::Defend;
        _actionTarget = attacker->GetGUID();
        me->Attack(attacker, true);
        me->GetMotionMaster()->MoveChase(attacker);
    }
    else
    {
        // Enfants et anciens fuient vers un feu allume, sinon vers la maison.
        _reflex = Reflex::Flee;
        GameObject* fire = sClanMgr->FindNearestLitFire(me);
        Position safe = fire ? fire->GetPosition() : _state->home;
        me->GetMotionMaster()->MovePoint(MOVE_TO_FLEE, safe);
    }
}

void npc_clan_member::EndReflex()
{
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
    // (Un membre place reapparaitra en frais individu au respawn du coeur ; un
    //  nouveau-ne ne sera pas re-summon.)
    _scheduler.CancelAll();
    if (_state)
    {
        sClanMgr->OnMemberKilled(_state);
        _state = nullptr; // l'etat vient d'etre detruit : ne plus y toucher
    }
}
