#pragma once
#include "../core/state.h"

namespace cdmm::game::StorageSlots
{
    inline constexpr int kSteps[] = {1, 10, 100, 1000};
    inline constexpr const char* kStepNames[] = {"1", "10", "100", "1000"};

    int Step();

    State::StorageSlots* Find(const char* key);
    State::StorageSlots* FindOrAdd(const char* key, int initialValue);

    int TargetFor(const char* key);
}
