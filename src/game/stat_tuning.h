#pragma once

#include <atomic>
#include <cstdint>

#include "../core/state.h"

namespace trinity::game::StatTuning
{
    enum Kind { Health = 0, Stamina = 1, Spirit = 2 };

    bool NeedsTick(const State& st);
    int64_t AdjustTarget(Kind kind, uintptr_t entry, int64_t time, int64_t target);

    namespace Drain
    {
        void    NoteCommitTime(int64_t gameTime);
        int64_t ClampTarget(const State::StatTune& cfg, uintptr_t entry, int64_t target);
        void    Tick(const State::StatTune& cfg, const std::atomic<uintptr_t>* set, int n, double dt);
    }
    void OnCommit(Kind kind, uintptr_t entry);
    void Tick(Kind kind, const std::atomic<uintptr_t>* set, int n, double dtSec);
    void Forget();
}
