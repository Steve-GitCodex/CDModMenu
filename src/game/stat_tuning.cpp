#include "stat_tuning.h"

#include <Windows.h>
#include <cstdio>

#include "offsets.h"
#include "../mem/safe_memory.h"

namespace cdmm::game::StatTuning
{
    using mem::Read64;
    using mem::Write64;

    namespace
    {
        constexpr uintptr_t kOff_StatEntry_LiveMax = 0x58;
        constexpr int kMaxTracked = 64;

        struct Tracked { uintptr_t entry; double regenAcc; bool regenActive; bool maxSaved; uint64_t origBase, origLive; int dumps; };
        Tracked g_tracked[kMaxTracked]{};
        int     g_trackedCount = 0;

        Tracked* Track(uintptr_t e)
        {
            for (int i = 0; i < g_trackedCount; ++i)
                if (g_tracked[i].entry == e) return &g_tracked[i];
            if (g_trackedCount >= kMaxTracked) g_trackedCount = 0;
            g_tracked[g_trackedCount] = { e, 0.0, false, false, 0, 0, 0 };
            return &g_tracked[g_trackedCount++];
        }

        bool IsInfinite(const State& st, Kind k)
        {
            return k == Health ? st.godMode : k == Stamina ? st.infStamina : st.infSpirit;
        }

        struct Gauge { uint64_t base, cur, full; };

        bool ReadGauge(uintptr_t e, Gauge* g)
        {
            uint64_t cap = 0;
            if (!Read64(e + kOff_StatEntry_Base, &g->base)) return false;
            if (!Read64(e + kOff_StatEntry_Current, &g->cur)) return false;
            Read64(e + kOff_StatEntry_Cap, &cap);
            g->full = cap > g->base ? cap : g->base;
            return g->full > 0;
        }

        void WriteCurrent(uintptr_t e, const Gauge& g, uint64_t v)
        {
            Write64(e + kOff_StatEntry_Current, v);
            Write64(e + kOff_StatEntry_Norm, v > g.base ? v - g.base : 0);
        }

        void ApplyFloor(uintptr_t e, int pct, bool allowFromZero)
        {
            Gauge g{};
            if (!ReadGauge(e, &g)) return;
            if (g.cur == 0 && !allowFromZero) return;
            const uint64_t floor = g.full * static_cast<uint64_t>(pct) / 100;
            if (g.cur < floor) WriteCurrent(e, g, floor);
        }

        bool AnyMaxSaved()
        {
            for (int i = 0; i < g_trackedCount; ++i)
                if (g_tracked[i].maxSaved) return true;
            return false;
        }

        void DumpEntry(const char* tag, uintptr_t e)
        {
            FILE* f = fopen("CDModMenu_health.txt", "a");
            if (!f) return;
            fprintf(f, "%s entry=%llx", tag, static_cast<unsigned long long>(e));
            for (uintptr_t off = 0; off < kSizeof_StatEntry; off += 8)
            {
                uint64_t v = 0;
                Read64(e + off, &v);
                fprintf(f, " +%02llx=%lld", static_cast<unsigned long long>(off), static_cast<long long>(v));
            }
            fputc('\n', f);
            fclose(f);
        }

        void ApplyHealthMax(Tracked& t, bool on, int pct)
        {
            if (on)
            {
                if (!t.maxSaved)
                {
                    if (!Read64(t.entry + kOff_StatEntry_Base, &t.origBase)) return;
                    Read64(t.entry + kOff_StatEntry_LiveMax, &t.origLive);
                    t.maxSaved = true;
                    DumpEntry("BEFORE", t.entry);
                }
                const uint64_t wantBase = t.origBase * static_cast<uint64_t>(pct) / 100;
                const uint64_t wantLive = t.origLive * static_cast<uint64_t>(pct) / 100;
                uint64_t base = 0, live = 0;
                Read64(t.entry + kOff_StatEntry_Base, &base);
                Read64(t.entry + kOff_StatEntry_LiveMax, &live);
                if (t.dumps < 3 && (base != wantBase || live != wantLive)) { DumpEntry("RESET-BY-GAME", t.entry); ++t.dumps; }
                if (base != wantBase) Write64(t.entry + kOff_StatEntry_Base, wantBase);
                if (t.origLive && live != wantLive) Write64(t.entry + kOff_StatEntry_LiveMax, wantLive);
                return;
            }
            if (!t.maxSaved) return;
            Write64(t.entry + kOff_StatEntry_Base, t.origBase);
            if (t.origLive) Write64(t.entry + kOff_StatEntry_LiveMax, t.origLive);
            const uint64_t full = t.origLive ? t.origLive : t.origBase;
            uint64_t cur = 0;
            if (Read64(t.entry + kOff_StatEntry_Current, &cur) && cur > full)
                Write64(t.entry + kOff_StatEntry_Current, full);
            t.maxSaved = false;
            t.dumps = 0;
        }

        void ApplyRegen(Tracked& t, int pct, int startPct, double dt)
        {
            Gauge g{};
            if (!ReadGauge(t.entry, &g) || g.cur == 0) { t.regenAcc = 0.0; return; }
            if (g.cur >= g.full) { t.regenActive = false; t.regenAcc = 0.0; return; }
            if (!t.regenActive && g.cur * 100 <= g.full * static_cast<uint64_t>(startPct)) t.regenActive = true;
            if (!t.regenActive) return;
            t.regenAcc += static_cast<double>(g.full) * pct / 100.0 * dt;
            const uint64_t add = static_cast<uint64_t>(t.regenAcc);
            if (!add) return;
            t.regenAcc -= static_cast<double>(add);
            const uint64_t v = g.cur + add;
            WriteCurrent(t.entry, g, v > g.full ? g.full : v);
        }
    }

    int64_t AdjustTarget(Kind kind, uintptr_t entry, int64_t time, int64_t target)
    {
        Drain::NoteCommitTime(time);
        const State& st = State::Get();
        if (kind == Health || IsInfinite(st, kind)) return target;
        return Drain::ClampTarget(st.statTune[kind], entry, target);
    }

    bool NeedsTick(const State& st)
    {
        if (st.oneHitKill || st.cannotDie || st.hpMaxOn || AnyMaxSaved()) return true;
        for (const auto& t : st.statTune)
            if (t.floorOn || t.regenOn) return true;
        return false;
    }

    void OnCommit(Kind kind, uintptr_t entry)
    {
        const State& st = State::Get();
        const State::StatTune& t = st.statTune[kind];
        if (IsInfinite(st, kind)) return;
        if (kind == Health && t.floorOn) ApplyFloor(entry, t.floorPct, true);
        if (kind == Health && st.cannotDie) ApplyFloor(entry, st.cannotDiePct, true);
    }

    void Tick(Kind kind, const std::atomic<uintptr_t>* set, int n, double dtSec)
    {
        const State& st = State::Get();
        const State::StatTune& cfg = st.statTune[kind];
        const bool hpMax = kind == Health && (st.hpMaxOn || AnyMaxSaved());
        if (kind != Health)
        {
            if (!IsInfinite(st, kind)) Drain::Tick(cfg, set, n, dtSec);
            return;
        }
        const bool tune = !IsInfinite(st, kind) && (cfg.regenOn || cfg.floorOn);
        if (!tune && !hpMax) return;

        for (int i = 0; i < n; ++i)
        {
            const uintptr_t e = set[i].load(std::memory_order_relaxed);
            if (!e) continue;
            Tracked* t = Track(e);
            if (!t) continue;
            if (hpMax) ApplyHealthMax(*t, st.hpMaxOn, st.hpMaxPct);
            if (!tune) continue;
            if (cfg.regenOn) ApplyRegen(*t, cfg.regenPct, cfg.regenStartPct, dtSec);
            if (cfg.floorOn) ApplyFloor(e, cfg.floorPct, false);
        }
    }

    void Forget()
    {
        g_trackedCount = 0;
    }
}
