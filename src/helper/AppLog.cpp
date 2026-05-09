#include "AppLog.h"
#include <windows.h>
#include <mutex>
#include <ctime>

// ---------------------------------------------------------------------------
//  Internal state
// ---------------------------------------------------------------------------

namespace
{
    std::vector<LogEntry>                    g_entries;
    std::function<void(const LogEntry&)>     g_listener;
    std::mutex                               g_mutex;

    // Max entries kept in memory  (prevents unbounded growth)
    constexpr size_t kMaxEntries = 500;

    std::wstring CurrentTime()
    {
        SYSTEMTIME st;
        GetLocalTime(&st);

        wchar_t buf[16];
        swprintf_s(buf, L"%02d:%02d:%02d",
            st.wHour, st.wMinute, st.wSecond);

        return buf;
    }

    const wchar_t* LevelPrefix(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Info:    return L"[INFO]  ";
        case LogLevel::Warning: return L"[WARN]  ";
        case LogLevel::Error:   return L"[ERROR] ";
        }
        return L"";
    }
}
 
// ---------------------------------------------------------------------------
//  Private: core add
// ---------------------------------------------------------------------------

void AppLog::Add(LogLevel level, const std::wstring& msg)
{
    LogEntry entry;
    entry.level = level;
    entry.message = msg;
    entry.timestamp = CurrentTime();

    // Debug output window (visible in VS Output panel)
    std::wstring line =
        entry.timestamp + L" " + LevelPrefix(level) + msg + L"\n";
    OutputDebugStringW(line.c_str());

    std::lock_guard<std::mutex> lock(g_mutex);

    // Ring-buffer behaviour – drop oldest when full
    if (g_entries.size() >= kMaxEntries)
        g_entries.erase(g_entries.begin());

    g_entries.push_back(entry);

    // Fire listener outside the critical section copy to avoid deadlock
    auto listener = g_listener;

    if (listener)
        listener(entry);
}

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------

void AppLog::Info(const std::wstring& msg) { Add(LogLevel::Info, msg); }
void AppLog::Warn(const std::wstring& msg) { Add(LogLevel::Warning, msg); }
void AppLog::Error(const std::wstring& msg) { Add(LogLevel::Error, msg); }

const std::vector<LogEntry>& AppLog::GetAll()
{
    return g_entries;
}

void AppLog::Clear()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_entries.clear();
}

void AppLog::SetListener(std::function<void(const LogEntry&)> callback)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_listener = std::move(callback);
}