#include "Helper.h"
#include "Globals.h"
#include "StringUtils.h"
#include "AppLog.h"

#include <windows.h>
#include <psapi.h>
#include <shlobj.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ===========================================================================
// INTERNAL HELPERS
// ===========================================================================

namespace
{
    // Returns the exe name (lowercase) of the foreground process, e.g. "chrome.exe"
    std::wstring GetActiveProcessName()
    {
        HWND hwnd = GetForegroundWindow();
        if (!hwnd)
            return L"";

        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);

        HANDLE hProcess = OpenProcess(
            PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);

        if (!hProcess)
        {
            AppLog::Warn(L"IsAppAllowed: could not open process (pid "
                + std::to_wstring(pid) + L")");
            return L"";
        }

        wchar_t name[MAX_PATH] = {};
        GetModuleBaseNameW(hProcess, nullptr, name, MAX_PATH);
        CloseHandle(hProcess);

        std::wstring exe = name;
        for (auto& c : exe)
            c = towlower(c);

        return exe;
    }
}

// ===========================================================================
// PUBLIC API
// ===========================================================================

std::wstring GetAppDataDir()
{
    wchar_t path[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path);

    std::wstring dir = std::wstring(path) + L"\\EX-Expander";
    CreateDirectoryW(dir.c_str(), nullptr);  // no-op if already exists

    return dir;
}

void HandleSettings(const std::string& jsonStr)
{
    try
    {
        auto j = json::parse(jsonStr);

        // Scope mode: "allow" or "block"
        g_ScopeMode = Utf8ToWide(j.value("scopeMode", "allow").c_str());

        // Rebuild the app list from scratch
        g_ScopeApps.clear();

        if (j.contains("scopeApps") && j["scopeApps"].is_array())
        {
            for (const auto& a : j["scopeApps"])
            {
                if (!a.is_string()) continue;

                // Use proper UTF-8 → wide conversion (handles non-ASCII app names)
                std::wstring w = Utf8ToWide(a.get<std::string>().c_str());
                for (auto& c : w)
                    c = towlower(c);

                g_ScopeApps.push_back(std::move(w));
            }
        }

        AppLog::Info(L"Settings applied: scopeMode=" + g_ScopeMode
            + L", apps=" + std::to_wstring(g_ScopeApps.size()));
    }
    catch (const json::exception& e)
    {
        AppLog::Error(L"HandleSettings: failed to parse JSON: "
            + Utf8ToWide(e.what()));
    }
}

bool IsAppAllowed()
{
    // No filter configured – allow everything
    if (g_ScopeApps.empty())
        return true;

    const std::wstring exe = GetActiveProcessName();

    bool listed = false;
    for (const auto& app : g_ScopeApps)
    {
        if (exe == app)
        {
            listed = true;
            break;
        }
    }

    if (g_ScopeMode == L"allow")
        return listed;

    if (g_ScopeMode == L"block")
        return !listed;

    return true;    // unknown mode – fail open
}