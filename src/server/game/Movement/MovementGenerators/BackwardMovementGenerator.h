#ifndef TRINITY_BACKWARDMOVEMENTGENERATOR_H
#define TRINITY_BACKWARDMOVEMENTGENERATOR_H

#include "MovementGenerator.h"
#include "Position.h"

class Unit;

class BackwardMovementGenerator : public MovementGenerator
{
    public:
        explicit BackwardMovementGenerator(uint32 id, float x, float y, float z,
            Optional<float> speed = {},
            MovementWalkRunSpeedSelectionMode speedSelectionMode = MovementWalkRunSpeedSelectionMode::ForceWalk,
            Unit const* faceTarget = nullptr);
        BackwardMovementGenerator(BackwardMovementGenerator const&) = delete;
        BackwardMovementGenerator(BackwardMovementGenerator&&) = delete;
        BackwardMovementGenerator& operator=(BackwardMovementGenerator const&) = delete;
        BackwardMovementGenerator& operator=(BackwardMovementGenerator&&) = delete;
        ~BackwardMovementGenerator();

        MovementGeneratorType GetMovementGeneratorType() const override;

        void Initialize(Unit* owner) override;
        void Reset(Unit* owner) override;
        bool Update(Unit* owner, uint32 diff) override;
        void Deactivate(Unit* owner) override;
        void Finalize(Unit* owner, bool active, bool movementInform) override;

        uint32 GetId() const { return _movementId; }

    private:
        void MovementInform(Unit* owner);

        uint32 _movementId;
        Position _destination;
        Optional<float> _speed;
        Unit const* _faceTarget;
        MovementWalkRunSpeedSelectionMode _speedSelectionMode;
};

#endif
