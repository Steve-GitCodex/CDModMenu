#include "stat_tuning.h"

#include <Windows.h>
#include <atomic>
#include <mutex>

#include "offsets.h"
#include "../mem/safe_memory.h"

namespace cdmm::game::StatTuning::Drain
{
    using mem::Read64;
    using mem::Write64;

    namespace
    {
        constexpr uintptr_t kOff_Rate   = 0x10;
        constexpr uintptr_t kOff_Anchor = 0x38;
        constexpr int       kMaxEntries = 64;
        constexpr int64_t   kCommitHeadroomSec = 10;

        struct EntryState { uintptr_t entry; double regenAcc; bool regenActive; int64_t settleTo; };
        EntryState g_states[kMaxEntries]{};
        int        g_count = 0;
        std::mutex g_lock;

        std::atomic<int64_t> g_gameMinusWall{ 0 };
        std::atomic<bool>    g_haveClock{ false };

        EntryState& StateFor(uintptr_t e)
        {
            for (int i = 0; i < g_count; ++i)
                if (g_states[i].entry == e) return g_states[i];
            if (g_count >= kMaxEntries) g_count = 0;
            g_states[g_count] = { e, 0.0, false, -1 };
            return g_states[g_count++];
        }

        struct Gauge { int64_t cur, rate, anchor, full; };

        bool ReadGauge(uintptr_t e, Gauge* g)
        {
            uint64_t cur = 0, rate = 0, anchor = 0, base = 0, cap = 0;
            if (!Read64(e + kOff_StatEntry_Current, &cur) || !Read64(e + kOff_Rate, &rate) ||
                !Read64(e + kOff_Anchor, &anchor) || !Read64(e + kOff_StatEntry_Base, &base))
                return false;
            Read64(e + kOff_StatEntry_Cap, &cap);
            g->cur = static_cast<int64_t>(cur);
            g->rate = static_cast<int64_t>(rate);
            g->anchor = static_cast<int64_t>(anchor);
            g->full = static_cast<int64_t>(cap > base ? cap : base);
            return g->full > 0;
        }

        int64_t Elapsed(const Gauge& g, int64_t now)
        {
            return now > g.anchor ? now - g.anchor : 0;
        }

        // Shift only the stored value so the game's own rate and anchor stay untouched;
        // the game later re-derives value = cur + rate * elapsed, landing on `target`.
        void SetEffective(uintptr_t e, const Gauge& g, int64_t now, int64_t target)
        {
            const int64_t cur = target - g.rate * Elapsed(g, now) / 1000;
            Write64(e + kOff_StatEntry_Current, static_cast<uint64_t>(cur < 0 ? 0 : cur));
        }

        void TickEntry(const State::StatTune& cfg, uintptr_t e, int64_t now, double dt)
        {
            Gauge g{};
            if (!ReadGauge(e, &g)) return;

            int64_t eff = g.cur + g.rate * Elapsed(g, now) / 1000;
            if (eff < 0) eff = 0;
            if (eff > g.full) eff = g.full;

            EntryState& s = StateFor(e);
            int64_t target = eff;
            // Commits arrive in pairs (rate zeroed in between) and the exhaustion timer is taken
            // from the second one, so settle only once the game has re-applied a drain rate.
            if (s.settleTo >= 0)
            {
                if (g.rate == 0) return;
                const int64_t settle = s.settleTo;
                s.settleTo = -1;
                if (g.rate < 0 && cfg.floorOn && settle < eff) SetEffective(e, g, now, settle);
                return;
            }

            if (cfg.regenOn && eff > 0)
            {
                if (eff >= g.full) s.regenActive = false;
                else if (!s.regenActive && eff * 100 <= g.full * cfg.regenStartPct) s.regenActive = true;
                if (s.regenActive)
                {
                    s.regenAcc += static_cast<double>(g.full) * cfg.regenPct / 100.0 * dt;
                    const int64_t add = static_cast<int64_t>(s.regenAcc);
                    s.regenAcc -= static_cast<double>(add);
                    target = eff + add > g.full ? g.full : eff + add;
                }
                else s.regenAcc = 0.0;
            }
            else
            {
                s.regenActive = false;
                s.regenAcc = 0.0;
            }

            if (cfg.floorOn)
            {
                const int64_t floor = g.full * cfg.floorPct / 100;
                if (target < floor) target = floor;
            }

            if (target != eff) SetEffective(e, g, now, target);
        }
    }

    void NoteCommitTime(int64_t gameTime)
    {
        g_gameMinusWall.store(gameTime - static_cast<int64_t>(GetTickCount64()), std::memory_order_relaxed);
        g_haveClock.store(true, std::memory_order_relaxed);
    }

    int64_t ClampTarget(const State::StatTune& cfg, uintptr_t entry, int64_t target)
    {
        if (!cfg.floorOn) return target;
        Gauge g{};
        if (!ReadGauge(entry, &g)) return target;
        // The game schedules exhaustion at commit time + committed value / drain rate and ends
        // the drain then regardless of the live value. Committing extra headroom pushes that
        // event past the next periodic commit; the tick settles the value back down right after.
        const int64_t floor = g.full * cfg.floorPct / 100;
        const int64_t natural = target < floor ? floor : target;
        if (g.rate >= 0) return natural;
        int64_t headroom = -g.rate * kCommitHeadroomSec;
        if (headroom > g.full) headroom = g.full;
        if (headroom <= natural) return natural;
        std::lock_guard<std::mutex> lock(g_lock);
        StateFor(entry).settleTo = natural;
        return headroom;
    }

    void Tick(const State::StatTune& cfg, const std::atomic<uintptr_t>* set, int n, double dt)
    {
        if ((!cfg.floorOn && !cfg.regenOn) || !g_haveClock.load(std::memory_order_relaxed)) return;
        const int64_t now = static_cast<int64_t>(GetTickCount64()) + g_gameMinusWall.load(std::memory_order_relaxed);

        std::lock_guard<std::mutex> lock(g_lock);
        for (int i = 0; i < n; ++i)
        {
            const uintptr_t e = set[i].load(std::memory_order_relaxed);
            if (e) TickEntry(cfg, e, now, dt);
        }
    }
}
