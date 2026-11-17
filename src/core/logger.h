#pragma once
#include <Windows.h>
#include "core/version.h"
#include <cstdarg>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>

namespace cdmm
{
    // Console logger. The console is created lazily (only the process that
    // actually renders the game calls EnableConsole), so the launcher process
    // that also loads the ASI stays silent. Lines logged before the console
    // exists are buffered and flushed into it on creation.
    class Logger
    {
    public:
        enum Level { Info, Good, Warn, Error };

        static void EnableConsole()
        {
#ifdef NDEBUG
            return;
#endif
            std::lock_guard<std::mutex> lock(Mutex());
            if (s_console)
                return;

            AllocConsole();
            freopen_s(&s_conFp, "CONOUT$", "w", stdout);
            SetConsoleTitleA(MOD_NAME);
            s_console = true;

            for (const auto& line : s_buffer)
                Emit(line);
            s_buffer.clear();
        }

        static void Shutdown()
        {
            std::lock_guard<std::mutex> lock(Mutex());
            if (s_conFp)   { fclose(s_conFp); s_conFp = nullptr; }
            if (s_console) { FreeConsole(); s_console = false; }
        }

        static void Log(Level lvl, const char* fmt, ...)
        {
            char msg[1024];
            va_list ap;
            va_start(ap, fmt);
            vsnprintf(msg, sizeof(msg), fmt, ap);
            va_end(ap);

            Line line;
            line.lvl = lvl;
            SYSTEMTIME st;
            GetLocalTime(&st);
            char stamp[16];
            snprintf(stamp, sizeof(stamp), "%02u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
            line.stamp = stamp;
            line.text  = msg;

            std::lock_guard<std::mutex> lock(Mutex());
            if (s_console)
            {
                Emit(line);
            }
            else
            {
                s_buffer.emplace_back(std::move(line));
                if (s_buffer.size() > 256)
                    s_buffer.pop_front();
            }
        }

    private:
        struct Line { Level lvl = Info; std::string stamp, text; };

        static void Emit(const Line& l)
        {
            HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
            WORD body = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
            switch (l.lvl)
            {
            case Good:  body = FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
            case Warn:  body = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
            case Error: body = FOREGROUND_RED | FOREGROUND_INTENSITY; break;
            default: break;
            }

            SetConsoleTextAttribute(h, FOREGROUND_INTENSITY);
            std::printf("%s ", l.stamp.c_str());
            SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_INTENSITY);
            std::printf(MOD_NAME " ");
            SetConsoleTextAttribute(h, body);
            std::printf("%s\n", l.text.c_str());
            SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
            std::fflush(stdout);
        }

        static std::mutex& Mutex()
        {
            static std::mutex m;
            return m;
        }

        static inline FILE*            s_conFp   = nullptr;
        static inline bool             s_console = false;
        static inline std::deque<Line> s_buffer;
    };
}

#define LOG(...)      ::cdmm::Logger::Log(::cdmm::Logger::Info,  __VA_ARGS__)
#define LOG_OK(...)   ::cdmm::Logger::Log(::cdmm::Logger::Good,  __VA_ARGS__)
#define LOG_WARN(...) ::cdmm::Logger::Log(::cdmm::Logger::Warn,  __VA_ARGS__)
#define LOG_ERR(...)  ::cdmm::Logger::Log(::cdmm::Logger::Error, __VA_ARGS__)
