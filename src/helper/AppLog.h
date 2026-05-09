#pragma once
#include <string>
#include <vector>
#include <functional>

// ---------------------------------------------------------------------------
//  Log levels
// ---------------------------------------------------------------------------

enum class LogLevel
{
    Info = 0,
    Warning = 1,
    Error = 2
};

// ---------------------------------------------------------------------------
//  A single log entry
// ---------------------------------------------------------------------------

struct LogEntry
{
    LogLevel     level;
    std::wstring message;
    std::wstring timestamp;   // e.g. "14:23:01"
};

// ---------------------------------------------------------------------------
//  AppLog  –  simple singleton logger
//
//  Usage:
//      AppLog::Info(L"Expansion added: " + token);
//      AppLog::Warn(L"Duplicate token skipped");
//      AppLog::Error(L"Database failed to open");
//
//  WebView2 hook (call once at startup):
//      AppLog::SetListener([&](const LogEntry& e) {
//          // post e to your WebView2 here
//      });
// ---------------------------------------------------------------------------

class AppLog
{
public:
    // Log methods – use these everywhere in the app
    static void Info(const std::wstring& msg);
    static void Warn(const std::wstring& msg);
    static void Error(const std::wstring& msg);

    // Returns all entries collected so far (for displaying in WebView2)
    static const std::vector<LogEntry>& GetAll();

    // Clear the in-memory log
    static void Clear();

    // Optional: set a callback that fires on every new entry
    // Perfect for forwarding to WebView2
    static void SetListener(std::function<void(const LogEntry&)> callback);

private:
    static void Add(LogLevel level, const std::wstring& msg);
};