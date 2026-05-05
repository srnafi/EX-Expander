#include <Helper.h>
#include "Globals.h"
#include <nlohmann/json.hpp>
#include <Psapi.h>

std::wstring GetAppDataDir()
{
    wchar_t path[MAX_PATH];

    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path);

    std::wstring dir = std::wstring(path) + L"\\EX-Expander";

    CreateDirectoryW(dir.c_str(), nullptr); // create if not exists

    return dir;
}

void HandleSettings(const std::string& jsonStr)
{
    auto j = nlohmann::json::parse(jsonStr);

    // mode
    std::string mode = j["scopeMode"].get<std::string>();
    g_ScopeMode = std::wstring(mode.begin(), mode.end());

    // 🔴 clear old list
    g_ScopeApps.clear();

    // load new apps
    for (auto& a : j["scopeApps"])
    {
        std::string s = a.get<std::string>();

        std::wstring w(s.begin(), s.end());
        for (auto& c : w) c = towlower(c);

        g_ScopeApps.push_back(w);
    }
}

std::wstring GetActiveProcessName()
{
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return L"";

    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) return L"";

    wchar_t name[MAX_PATH] = L"";
    GetModuleBaseNameW(hProcess, nullptr, name, MAX_PATH);

    CloseHandle(hProcess);

    std::wstring exe = name;
    for (auto& c : exe) c = towlower(c);

    return exe; // chrome.exe
}

bool IsAppAllowed()
{
    // 🔴 default → allow everything
    if (g_ScopeApps.empty())
        return true;

    std::wstring exe = GetActiveProcessName(); 
    bool listed = false;

    for (const auto& app : g_ScopeApps)
    {
        if (exe == app)
        {
            listed = true;
            break;
        }
    }

    // -----------------------------
    // MODE LOGIC
    // -----------------------------

    if (g_ScopeMode == L"allow")
    {
        return listed;      // only selected apps allowed
    }

    if (g_ScopeMode == L"block")
    {
        return !listed;     // block selected apps
    }

    return true; // fallback safety
}