#include "settings.h"

#include <Windows.h>
#include "core/version.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "logger.h"
#include "mod.h"
#include "state.h"

namespace cdmm
{
    // Set once by ClaimOwnership() in the process that presents the game. See
    // settings.h - only that process may write CDModMenu.ini.
    static bool g_owner = false;

    // CDModMenu.ini lives next to CDModMenu.asi so the whole install stays one
    // folder that can be copied or deleted as a unit. `suffix` appends to the
    // file name for the temp file Save() writes through.
    static bool IniPath(char* buf, size_t cap, const char* suffix = "")
    {
        const DWORD n = GetModuleFileNameA(Mod::Get().Module(), buf, static_cast<DWORD>(cap));
        if (n == 0 || n >= cap)
            return false;

        char* slash = strrchr(buf, '\\');
        if (!slash)
            return false;

        const size_t left = cap - static_cast<size_t>(slash + 1 - buf);
        return snprintf(slash + 1, left, MOD_NAME ".ini%s", suffix) < static_cast<int>(left);
    }

    static float ClampF(float v, float lo, float hi)
    {
        return v < lo ? lo : v > hi ? hi : v;
    }

    static int ClampI(int v, int lo, int hi)
    {
        return v < lo ? lo : v > hi ? hi : v;
    }

    void Settings::Load()
    {
        char path[MAX_PATH];
        if (!IniPath(path, sizeof(path)))
            return;

        FILE* f = fopen(path, "r");
        if (!f)
            return; // first run - nothing saved yet

        // Parse onto a default-constructed State so missing/garbled keys keep
        // their defaults, then apply in one step below.
        State vals;
        char  line[128];
        while (fgets(line, sizeof(line), f))
        {
            char* eq = strchr(line, '=');
            if (!eq)
                continue;
            *eq = 0;
            const char* key = line;
            const char* val = eq + 1;

            if      (!strcmp(key, "openKeyVk"))      vals.openKeyVk     = atoi(val);
            else if (!strcmp(key, "openPadMask"))    vals.openPadMask   = static_cast<unsigned int>(strtoul(val, nullptr, 0));
            else if (!strcmp(key, "flyUpKeyVk"))     vals.flyUpKeyVk    = atoi(val);
            else if (!strcmp(key, "flyDownKeyVk"))   vals.flyDownKeyVk  = atoi(val);
            else if (!strcmp(key, "flyUpPadMask"))   vals.flyUpPadMask  = static_cast<unsigned int>(strtoul(val, nullptr, 0));
            else if (!strcmp(key, "flyDownPadMask")) vals.flyDownPadMask = static_cast<unsigned int>(strtoul(val, nullptr, 0));
            else if (!strcmp(key, "autoSave"))      vals.autoSave      = atoi(val) != 0;
            else if (!strcmp(key, "godMode"))       vals.godMode       = atoi(val) != 0;
            else if (!strcmp(key, "infStamina"))    vals.infStamina    = atoi(val) != 0;
            else if (!strcmp(key, "infSpirit"))     vals.infSpirit     = atoi(val) != 0;
            else if (!strcmp(key, "dmgOutMult"))    vals.dmgOutMult    = strtof(val, nullptr);
            else if (!strcmp(key, "dmgInMult"))     vals.dmgInMult     = strtof(val, nullptr);
            else if (!strcmp(key, "dmgOutMode"))    vals.dmgOutMode    = atoi(val);
            else if (!strcmp(key, "dmgInMode"))     vals.dmgInMode     = atoi(val);
            else if (!strcmp(key, "dmgOutFixed"))   vals.dmgOutFixed   = atoi(val);
            else if (!strcmp(key, "dmgInFixed"))    vals.dmgInFixed    = atoi(val);
            else if (!strcmp(key, "gameSpeed"))     vals.gameSpeed     = atoi(val) != 0;
            else if (!strcmp(key, "gameSpeedMult")) vals.gameSpeedMult = strtof(val, nullptr);
            else if (!strcmp(key, "timeFrozen"))    vals.timeFrozen    = atoi(val) != 0;
            else if (!strcmp(key, "superRun"))      vals.superRun      = atoi(val) != 0;
            else if (!strcmp(key, "superRunMult"))  vals.superRunMult  = strtof(val, nullptr);
            else if (!strcmp(key, "superJump"))     vals.superJump     = atoi(val) != 0;
            else if (!strcmp(key, "superJumpMult")) vals.superJumpMult = strtof(val, nullptr);
            else if (!strcmp(key, "freeFlight"))    vals.freeFlight    = atoi(val) != 0;
            else if (!strcmp(key, "flightSpeed"))   vals.flightSpeed   = strtof(val, nullptr);
            else if (!strcmp(key, "trustMult"))     vals.trustMult     = atoi(val) != 0;
            else if (!strcmp(key, "trustMultVal"))  vals.trustMultVal  = strtof(val, nullptr);
            else if (!strcmp(key, "invSlotSize"))    vals.invSlotSize    = atoi(val) != 0;
            else if (!strcmp(key, "invSlotSizeVal")) vals.invSlotSizeVal = atoi(val);
            else if (!strcmp(key, "invStackSize"))    vals.invStackSize    = atoi(val) != 0;
            else if (!strcmp(key, "invStackSizeVal")) vals.invStackSizeVal = atoi(val);
            else if (!strcmp(key, "invStackMultMode")) vals.invStackMultMode = atoi(val) != 0;
            else if (!strcmp(key, "invStackMult"))     vals.invStackMult     = atoi(val);
            else if (!strcmp(key, "invStepIdx"))       vals.invStepIdx       = atoi(val);
            else if (!strcmp(key, "invStorage") && vals.invStorageSlotCount < State::kMaxStorageSlots)
            {
                State::StorageSlots& e = vals.invStorageSlots[vals.invStorageSlotCount];
                int on = 0;
                if (sscanf(val, "%47[^,],%d,%d", e.key, &on, &e.value) == 3)
                {
                    e.on    = on != 0;
                    e.value = ClampI(e.value, 1, 9999);
                    ++vals.invStorageSlotCount;
                }
            }
            else if (!strcmp(key, "showFps"))       vals.showFps       = atoi(val) != 0;
            else if (!strcmp(key, "cannotDie"))    { int on = 0, pct = 1;
                                                       if (sscanf(val, "%d,%d", &on, &pct) == 2) { vals.cannotDie = on != 0; vals.cannotDiePct = ClampI(pct, 1, 10); } }
            else if (!strcmp(key, "skipIntroVideos")) vals.skipIntroVideos = atoi(val) != 0;
            else if (!strcmp(key, "turboHold"))       vals.turboHold       = atoi(val) != 0;
            else if (!strcmp(key, "turboMult"))       vals.turboMult       = strtof(val, nullptr);
            else if (!strcmp(key, "statTune"))
            {
                int i = 0, fo = 0, fp = 0, ro = 0, rp = 0, mo = 0, mp = 0, rs = 30;
                const int got = sscanf(val, "%d,%d,%d,%d,%d,%d,%d,%d", &i, &fo, &fp, &ro, &rp, &mo, &mp, &rs);
                if (got == 6) rs = mo;
                if (got >= 6 &&
                    i >= 0 && i < State::kStatTuneCount)
                    vals.statTune[i] = { fo != 0, ClampI(fp, 1, 100), ro != 0, ClampI(rp, 1, 100),
                                         ClampI(rs, 1, 100) };
            }
        }
        fclose(f);

        State& st  = State::Get();
        st.autoSave = vals.autoSave;

        // Every key/pad bind persists regardless of Auto Save - a rebind you
        // can't keep between sessions is a bug, not a "feature value". A garbled
        // key falls back to the default; a 0 pad mask legitimately means "no
        // controller bind", so it is honoured as-is (fly binds use 0 to mean
        // "that direction disabled on the pad").
        if (vals.openKeyVk > 0 && vals.openKeyVk <= 0xFF)
            st.openKeyVk = vals.openKeyVk;
        st.openPadMask = vals.openPadMask & 0xFFFF;
        if (vals.flyUpKeyVk >= 0 && vals.flyUpKeyVk <= 0xFF)
            st.flyUpKeyVk = vals.flyUpKeyVk;
        if (vals.flyDownKeyVk >= 0 && vals.flyDownKeyVk <= 0xFF)
            st.flyDownKeyVk = vals.flyDownKeyVk;
        st.flyUpPadMask   = vals.flyUpPadMask   & 0x3FFFF;
        st.flyDownPadMask = vals.flyDownPadMask & 0x3FFFF;

        st.invSlotSize      = vals.invSlotSize;
        st.invSlotSizeVal   = ClampI(vals.invSlotSizeVal, 1, 9999);
        st.invStackSize     = vals.invStackSize;
        st.invStackSizeVal  = ClampI(vals.invStackSizeVal, 1, 999999999);
        st.invStackMultMode = vals.invStackMultMode;
        st.invStackMult     = ClampI(vals.invStackMult, 2, 1000);
        st.invStepIdx       = ClampI(vals.invStepIdx, 0, 3);
        st.invStorageSlotCount = vals.invStorageSlotCount;
        for (int i = 0; i < vals.invStorageSlotCount; ++i)
            st.invStorageSlots[i] = vals.invStorageSlots[i];

        for (int i = 0; i < State::kStatTuneCount; ++i)
            st.statTune[i] = vals.statTune[i];
        st.skipIntroVideos = vals.skipIntroVideos;
        st.turboHold       = vals.turboHold;
        st.turboMult       = ClampF(vals.turboMult, 1.0f, 10.0f);
        st.cannotDie    = vals.cannotDie;
        st.cannotDiePct = vals.cannotDiePct;

        if (!st.autoSave)
            return; // remembered the preference, but features start clean

        // Clamp the floats to the same ranges the menu rows enforce, in case
        // the file was hand-edited.
        st.godMode       = vals.godMode;
        st.infStamina    = vals.infStamina;
        st.infSpirit     = vals.infSpirit;
        st.dmgOutMult    = ClampF(vals.dmgOutMult, 0.0f, 20.0f);
        st.dmgInMult     = ClampF(vals.dmgInMult, 0.0f, 10.0f);
        st.dmgOutMode    = ClampI(vals.dmgOutMode, 0, 1);
        st.dmgInMode     = ClampI(vals.dmgInMode, 0, 1);
        st.dmgOutFixed   = ClampI(vals.dmgOutFixed, 0, 10000000);
        st.dmgInFixed    = ClampI(vals.dmgInFixed, 0, 10000000);
        st.gameSpeed     = vals.gameSpeed;
        st.gameSpeedMult = ClampF(vals.gameSpeedMult, 0.1f, 5.0f);
        st.timeFrozen    = vals.timeFrozen;
        st.superRun      = vals.superRun;
        st.superRunMult  = ClampF(vals.superRunMult, 1.0f, 10.0f);
        st.superJump     = vals.superJump;
        st.superJumpMult = ClampF(vals.superJumpMult, 1.0f, 10.0f);
        st.freeFlight    = vals.freeFlight;
        st.flightSpeed   = ClampF(vals.flightSpeed, 1.0f, 40.0f);
        st.trustMult     = vals.trustMult;
        st.trustMultVal  = ClampF(vals.trustMultVal, 1.0f, 25.0f);
        st.showFps       = vals.showFps;
        LOG_OK(MOD_NAME ".ini loaded - restored feature settings from last session.");
    }

    void Settings::ClaimOwnership()
    {
        g_owner = true;
    }

    void Settings::Save()
    {
        // Never let a process without a menu write its startup snapshot back.
        if (!g_owner)
            return;

        char path[MAX_PATH];
        char tmp[MAX_PATH];
        if (!IniPath(path, sizeof(path)) || !IniPath(tmp, sizeof(tmp), ".tmp"))
            return;

        // Write through a temp file and swap it in, so an interrupted save (the
        // shutdown one runs while the process is already tearing down) can never
        // leave a truncated CDModMenu.ini behind - the old file survives instead.
        const State& st = State::Get();
        FILE* f = fopen(tmp, "w");
        if (!f)
        {
            LOG_WARN("Could not write %s - feature settings not saved.", tmp);
            return;
        }

        fprintf(f,
                "; CDModMenu feature settings - managed from the in-game SYSTEM tab.\n"
                "; *KeyVk = Win32 virtual-key code; *PadMask = XInput button mask.\n"
                "openKeyVk=%d\n"
                "openPadMask=%u\n"
                "flyUpKeyVk=%d\n"
                "flyDownKeyVk=%d\n"
                "flyUpPadMask=%u\n"
                "flyDownPadMask=%u\n"
                "autoSave=%d\n"
                "godMode=%d\n"
                "infStamina=%d\n"
                "infSpirit=%d\n"
                "dmgOutMult=%.3f\n"
                "dmgInMult=%.3f\n"
                "gameSpeed=%d\n"
                "gameSpeedMult=%.3f\n"
                "timeFrozen=%d\n"
                "superRun=%d\n"
                "superRunMult=%.3f\n"
                "superJump=%d\n"
                "superJumpMult=%.3f\n"
                "freeFlight=%d\n"
                "flightSpeed=%.3f\n"
                "trustMult=%d\n"
                "trustMultVal=%.3f\n"
                "invSlotSize=%d\n"
                "invSlotSizeVal=%d\n"
                "invStackSize=%d\n"
                "invStackSizeVal=%d\n"
                "invStackMultMode=%d\n"
                "invStackMult=%d\n"
                "showFps=%d\n",
                st.openKeyVk,
                st.openPadMask,
                st.flyUpKeyVk,
                st.flyDownKeyVk,
                st.flyUpPadMask,
                st.flyDownPadMask,
                st.autoSave ? 1 : 0,
                st.godMode ? 1 : 0,
                st.infStamina ? 1 : 0,
                st.infSpirit ? 1 : 0,
                st.dmgOutMult,
                st.dmgInMult,
                st.gameSpeed ? 1 : 0,
                st.gameSpeedMult,
                st.timeFrozen ? 1 : 0,
                st.superRun ? 1 : 0,
                st.superRunMult,
                st.superJump ? 1 : 0,
                st.superJumpMult,
                st.freeFlight ? 1 : 0,
                st.flightSpeed,
                st.trustMult ? 1 : 0,
                st.trustMultVal,
                st.invSlotSize ? 1 : 0,
                st.invSlotSizeVal,
                st.invStackSize ? 1 : 0,
                st.invStackSizeVal,
                st.invStackMultMode ? 1 : 0,
                st.invStackMult,
                st.showFps ? 1 : 0);
        fprintf(f, "invStepIdx=%d\n", st.invStepIdx);
        for (int i = 0; i < st.invStorageSlotCount; ++i)
        {
            const State::StorageSlots& e = st.invStorageSlots[i];
            fprintf(f, "invStorage=%s,%d,%d\n", e.key, e.on ? 1 : 0, e.value);
        }
        fprintf(f, "dmgOutMode=%d\ndmgInMode=%d\ndmgOutFixed=%d\ndmgInFixed=%d\n", st.dmgOutMode, st.dmgInMode, st.dmgOutFixed, st.dmgInFixed);
        fprintf(f, "turboHold=%d\nturboMult=%.2f\n", st.turboHold ? 1 : 0, st.turboMult);
        fprintf(f, "skipIntroVideos=%d\n", st.skipIntroVideos ? 1 : 0);
        fprintf(f, "cannotDie=%d,%d\n", st.cannotDie ? 1 : 0, st.cannotDiePct);
        for (int i = 0; i < State::kStatTuneCount; ++i)
        {
            const State::StatTune& t = st.statTune[i];
            fprintf(f, "statTune=%d,%d,%d,%d,%d,%d\n", i, t.floorOn ? 1 : 0, t.floorPct,
                    t.regenOn ? 1 : 0, t.regenPct, t.regenStartPct);
        }
        const bool ok = fflush(f) == 0;
        fclose(f);

        if (!ok || !MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING))
        {
            LOG_WARN("Could not update %s - feature settings not saved.", path);
            DeleteFileA(tmp);
        }
    }

    void Settings::ResetFeatures()
    {
        // Copy the defaults straight off a fresh State so this can never
        // drift from the initializers in state.h. Menu/session state
        // (menuOpen, textCapture, autoSave) is deliberately left alone.
        const State def;
        State&      st = State::Get();
        st.godMode       = def.godMode;
        st.infStamina    = def.infStamina;
        st.infSpirit     = def.infSpirit;
        st.dmgOutMult    = def.dmgOutMult;
        st.dmgInMult     = def.dmgInMult;
        st.dmgOutMode    = def.dmgOutMode;
        st.dmgInMode     = def.dmgInMode;
        st.dmgOutFixed   = def.dmgOutFixed;
        st.dmgInFixed    = def.dmgInFixed;
        st.gameSpeed     = def.gameSpeed;
        st.gameSpeedMult = def.gameSpeedMult;
        st.timeFrozen    = def.timeFrozen;
        st.superRun      = def.superRun;
        st.superRunMult  = def.superRunMult;
        st.superJump     = def.superJump;
        st.superJumpMult = def.superJumpMult;
        st.freeFlight    = def.freeFlight;
        st.flightSpeed   = def.flightSpeed;
        st.trustMult     = def.trustMult;
        st.trustMultVal  = def.trustMultVal;
        st.invSlotSize     = def.invSlotSize;
        st.invSlotSizeVal  = def.invSlotSizeVal;
        st.invStackSize    = def.invStackSize;
        st.invStackSizeVal = def.invStackSizeVal;
        st.invStackMultMode = def.invStackMultMode;
        st.invStackMult     = def.invStackMult;
        st.showFps       = def.showFps;
        st.oneHitKill    = def.oneHitKill;
        st.cannotDie     = def.cannotDie;
        st.cannotDiePct  = def.cannotDiePct;
        for (int i = 0; i < State::kStatTuneCount; ++i)
            st.statTune[i] = def.statTune[i];
    }

    void Settings::ResetBinds()
    {
        const State def;
        State&      st = State::Get();
        st.openKeyVk      = def.openKeyVk;
        st.openPadMask    = def.openPadMask;
        st.flyUpKeyVk     = def.flyUpKeyVk;
        st.flyDownKeyVk   = def.flyDownKeyVk;
        st.flyUpPadMask   = def.flyUpPadMask;
        st.flyDownPadMask = def.flyDownPadMask;
    }
}
