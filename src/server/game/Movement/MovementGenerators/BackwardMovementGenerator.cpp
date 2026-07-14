#include "BackwardMovementGenerator.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "MotionMaster.h"
#include "MovementDefines.h"
#include "MoveSpline.h"
#include "MoveSplineInit.h"
#include "Unit.h"

BackwardMovementGenerator::BackwardMovementGenerator(uint32 id, float x, float y, float z,
    Optional<float> speed /*= {}*/,
    MovementWalkRunSpeedSelectionMode speedSelectionMode /*= MovementWalkRunSpeedSelectionMode::Default*/,
    Unit const* faceTarget /*= nullptr*/)
    : _movementId(id), _speed(speed), _speedSelectionMode(speedSelectionMode),
    _destination(x, y, z), _faceTarget(faceTarget)
{
    this->Priority = MOTION_PRIORITY_NORMAL;
    this->Flags = MOVEMENTGENERATOR_FLAG_INITIALIZATION_PENDING;
    this->BaseUnitState = UNIT_STATE_ROAMING;
}

BackwardMovementGenerator::~BackwardMovementGenerator() = default;

MovementGeneratorType BackwardMovementGenerator::GetMovementGeneratorType() const
{
    // Reuse POINT_MOTION_TYPE so existing MovementInform handlers keep working
    return POINT_MOTION_TYPE;
}

bool BackwardMovementGenerator::Initialize(Unit* owner)
{
    RemoveFlag(MOVEMENTGENERATOR_FLAG_INITIALIZATION_PENDING | MOVEMENTGENERATOR_FLAG_TRANSITORY | MOVEMENTGENERATOR_FLAG_DEACTIVATED);
    AddFlag(MOVEMENTGENERATOR_FLAG_INITIALIZED);

    if (owner->HasUnitState(UNIT_STATE_NOT_MOVE) || owner->IsMovementPreventedByCasting())
    {
        AddFlag(MOVEMENTGENERATOR_FLAG_INTERRUPTED);
        owner->StopMoving();
        return false;
    }

    owner->AddUnitState(UNIT_STATE_ROAMING_MOVE);

    Movement::MoveSplineInit init(owner);
    init.MoveTo(_destination.GetPositionX(), _destination.GetPositionY(), _destination.GetPositionZ(), false);
    init.SetBackward();

    if (_speed)
        init.SetVelocity(*_speed);

    switch (_speedSelectionMode)
    {
        case MovementWalkRunSpeedSelectionMode::Default:
            break;
        case MovementWalkRunSpeedSelectionMode::ForceRun:
            init.SetWalk(false);
            break;
        case MovementWalkRunSpeedSelectionMode::ForceWalk:
            init.SetWalk(true);
            break;
        default:
            break;
    }

    if (_faceTarget)
        init.SetFacing(_faceTarget);

    int32 duration = init.Launch();
    return duration > 0;
}

bool BackwardMovementGenerator::Reset(Unit* owner)
{
    if (!owner)
        return false;

    RemoveFlag(MOVEMENTGENERATOR_FLAG_TRANSITORY | MOVEMENTGENERATOR_FLAG_DEACTIVATED);
    Initialize(owner);
    return true;
}

bool BackwardMovementGenerator::Update(Unit* owner, uint32 /*diff*/)
{
    if (!owner)
        return false;

    if (owner->HasUnitState(UNIT_STATE_NOT_MOVE) || owner->IsMovementPreventedByCasting())
    {
        AddFlag(MOVEMENTGENERATOR_FLAG_INTERRUPTED);
        owner->StopMoving();
        return true;
    }

    if (HasFlag(MOVEMENTGENERATOR_FLAG_INTERRUPTED) && owner->movespline->Finalized())
    {
        RemoveFlag(MOVEMENTGENERATOR_FLAG_INTERRUPTED);

        owner->AddUnitState(UNIT_STATE_ROAMING_MOVE);

        Movement::MoveSplineInit init(owner);
        init.MoveTo(_destination.GetPositionX(), _destination.GetPositionY(), _destination.GetPositionZ(), false);
        init.SetBackward();
        init.SetWalk(true);
        if (_faceTarget)
            init.SetFacing(_faceTarget);
        init.Launch();
    }

    if (owner->movespline->Finalized())
    {
        RemoveFlag(MOVEMENTGENERATOR_FLAG_TRANSITORY);
        AddFlag(MOVEMENTGENERATOR_FLAG_INFORM_ENABLED);
        return false;
    }
    return true;
}

void BackwardMovementGenerator::Deactivate(Unit* owner)
{
    AddFlag(MOVEMENTGENERATOR_FLAG_DEACTIVATED);
    owner->ClearUnitState(UNIT_STATE_ROAMING_MOVE);
}

void BackwardMovementGenerator::Finalize(Unit* owner, bool active, bool movementInform)
{
    AddFlag(MOVEMENTGENERATOR_FLAG_FINALIZED);
    if (active)
        owner->ClearUnitState(UNIT_STATE_ROAMING_MOVE);

    if (movementInform && HasFlag(MOVEMENTGENERATOR_FLAG_INFORM_ENABLED))
        MovementInform(owner);
}

void BackwardMovementGenerator::MovementInform(Unit* owner)
{
    if (Creature* creature = owner->ToCreature())
        if (creature->AI())
            creature->AI()->MovementInform(POINT_MOTION_TYPE, _movementId);
}
