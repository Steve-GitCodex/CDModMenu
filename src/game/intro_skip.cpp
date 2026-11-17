#include "intro_skip.h"

#include <Windows.h>
#include <objbase.h>
#include <mfmediaengine.h>
#include <cmath>
#include <cstdio>
#include <mutex>

#include <MinHook.h>

#include "../core/logger.h"
#include "../core/state.h"

#pragma comment(lib, "mfuuid.lib")

namespace trinity::game::IntroSkip
{
    namespace
    {
        struct Pair { IMFMediaEngineNotify* notify; IMFMediaEngine* engine; };
        constexpr int kMaxPairs = 16;
        Pair       g_pairs[kMaxPairs]{};
        int        g_pairNext = 0;
        std::mutex g_lock;

        void Remember(IMFMediaEngineNotify* n, IMFMediaEngine* e)
        {
            std::lock_guard<std::mutex> lock(g_lock);
            for (auto& p : g_pairs)
                if (p.notify == n) { p.engine = e; return; }
            g_pairs[g_pairNext] = { n, e };
            g_pairNext = (g_pairNext + 1) % kMaxPairs;
        }

        IMFMediaEngine* EngineFor(IMFMediaEngineNotify* n)
        {
            std::lock_guard<std::mutex> lock(g_lock);
            for (const auto& p : g_pairs)
                if (p.notify == n) return p.engine;
            return nullptr;
        }

        void Note(const char* fmt, double a, double b)
        {
            FILE* f = fopen("CDModMenu_introskip.txt", "a");
            if (!f) return;
            fprintf(f, fmt, a, b);
            fputc('\n', f);
            fclose(f);
        }

        using EventNotify_t = HRESULT(STDMETHODCALLTYPE*)(IMFMediaEngineNotify*, DWORD, DWORD_PTR, DWORD);
        EventNotify_t oEventNotify = nullptr;
        void*         g_notifyTarget = nullptr;

        HRESULT STDMETHODCALLTYPE hkEventNotify(IMFMediaEngineNotify* self, DWORD event, DWORD_PTR p1, DWORD p2)
        {
            if (event == MF_MEDIA_ENGINE_EVENT_CANPLAY && State::Get().skipIntroVideos)
            {
                if (IMFMediaEngine* engine = EngineFor(self))
                {
                    const bool loop = engine->GetLoop() != FALSE;
                    const double duration = engine->GetDuration();
                    const bool skip = !loop && std::isfinite(duration) && duration > 0.5;
                    if (skip) engine->SetCurrentTime(duration);
                    Note(skip ? "skipped video (%.2fs, loop=%.0f)" : "kept video (%.2fs, loop=%.0f)", duration, loop ? 1.0 : 0.0);
                }
            }
            return oEventNotify(self, event, p1, p2);
        }

        void HookNotify(IMFMediaEngineNotify* notify)
        {
            void* fn = (*reinterpret_cast<void***>(notify))[3];
            if (g_notifyTarget) return;
            if (MH_CreateHook(fn, reinterpret_cast<void*>(&hkEventNotify), reinterpret_cast<void**>(&oEventNotify)) == MH_OK &&
                MH_EnableHook(fn) == MH_OK)
                g_notifyTarget = fn;
        }

        using CreateInstance_t = HRESULT(STDMETHODCALLTYPE*)(IMFMediaEngineClassFactory*, DWORD, IMFAttributes*, IMFMediaEngine**);
        CreateInstance_t oCreateInstance = nullptr;
        void*            g_factoryTarget = nullptr;

        HRESULT STDMETHODCALLTYPE hkCreateInstance(IMFMediaEngineClassFactory* self, DWORD flags,
                                                   IMFAttributes* attrs, IMFMediaEngine** out)
        {
            const HRESULT hr = oCreateInstance(self, flags, attrs, out);
            if (FAILED(hr) || !attrs || !out || !*out) return hr;

            IMFMediaEngineNotify* notify = nullptr;
            if (SUCCEEDED(attrs->GetUnknown(MF_MEDIA_ENGINE_CALLBACK, IID_PPV_ARGS(&notify))) && notify)
            {
                HookNotify(notify);
                Remember(notify, *out);
                notify->Release();
            }
            return hr;
        }

        using CoCreateInstance_t = HRESULT(WINAPI*)(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID*);
        CoCreateInstance_t oCoCreateInstance = nullptr;

        HRESULT WINAPI hkCoCreateInstance(REFCLSID clsid, LPUNKNOWN outer, DWORD ctx, REFIID iid, LPVOID* out)
        {
            const HRESULT hr = oCoCreateInstance(clsid, outer, ctx, iid, out);
            if (SUCCEEDED(hr) && out && *out && !g_factoryTarget && IsEqualCLSID(clsid, CLSID_MFMediaEngineClassFactory))
            {
                void* fn = (*reinterpret_cast<void***>(*out))[3];
                if (MH_CreateHook(fn, reinterpret_cast<void*>(&hkCreateInstance), reinterpret_cast<void**>(&oCreateInstance)) == MH_OK &&
                    MH_EnableHook(fn) == MH_OK)
                    g_factoryTarget = fn;
            }
            return hr;
        }
    }

    void Install()
    {
        if (MH_CreateHookApi(L"combase", "CoCreateInstance", reinterpret_cast<void*>(&hkCoCreateInstance),
                             reinterpret_cast<void**>(&oCoCreateInstance)) != MH_OK)
        {
            LOG_WARN("intro-skip: could not hook CoCreateInstance - intro videos will play.");
            return;
        }
        MH_EnableHook(MH_ALL_HOOKS);
    }
}
