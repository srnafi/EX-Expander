#include "WebSettings.h"
#include <algorithm>
#include <Database.h>
#include "Globals.h"
#include <StringUtils.h>

// ---------------------------------------------------------------------------
// APPLY SETTINGS  (persist to DB + update runtime globals)
// ---------------------------------------------------------------------------

void ApplySettings(bool autoStart,
    const std::wstring& emojiSymbol,
    int maxPopup,
    const std::wstring& insertTrigger,
    const std::wstring& popupPosition)
{
    // Clamp maxPopup
    maxPopup = max(3,min(10, maxPopup));

    // Persist to DB
    DB_SetSetting(L"emojiSymbol", emojiSymbol);
    DB_SetSetting(L"maxPopup", std::to_wstring(maxPopup));
    DB_SetSetting(L"insertTrigger", insertTrigger);
    DB_SetSetting(L"autoStart", autoStart ? L"1" : L"0");
    DB_SetSetting(L"popupPosition", popupPosition);

    // Update runtime globals immediately
    g_TriggerChar = (!emojiSymbol.empty() && emojiSymbol[0] == L';') ? L';' : L':';
    g_MaxPopupItems = maxPopup;
    g_InsertOnSpace = (insertTrigger != L"symbol");
    g_PopupPosition = popupPosition;

    // Windows Registry autostart
    SetAutoStart(autoStart);
}

// ---------------------------------------------------------------------------
// BUILD SETTINGS SCRIPT  (pushes stored settings into the webview UI)
// ---------------------------------------------------------------------------

std::wstring BuildSettingsScript()
{
    std::wstring sym = DB_GetSetting(L"emojiSymbol", L":");
    std::wstring maxStr = DB_GetSetting(L"maxPopup", L"5");
    std::wstring trigger = DB_GetSetting(L"insertTrigger", L"space");
    std::wstring placement = DB_GetSetting(L"popupPosition", L"fixed");
    bool         autoSt = GetAutoStartEnabled();

    int maxVal = max(1, min(10, _wtoi(maxStr.c_str())));

    std::wstring script = L"if(window.applySettings){window.applySettings({";
    script += L"\"autoStart\":" + std::wstring(autoSt ? L"true" : L"false") + L",";
    script += L"\"emojiSymbol\":\"" + JsonEscape(sym) + L"\",";
    script += L"\"maxPopup\":" + std::to_wstring(maxVal) + L",";
    script += L"\"insertTrigger\":\"" + JsonEscape(trigger) + L"\",";
    script += L"\"popupPosition\":\"" + JsonEscape(placement) + L"\"";
    script += L"});}";

    return script;
}

// ---------------------------------------------------------------------------
// AUTOSTART HELPERS
// ---------------------------------------------------------------------------

bool GetAutoStartEnabled()
{
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
        return false;

    bool enabled = (RegQueryValueExW(hKey, L"EX-Expander",
        nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS);
    RegCloseKey(hKey);
    return enabled;
}



void SetAutoStart(bool enable)
{
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
        return;

    if (enable)
    {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        RegSetValueExW(hKey, L"EX-Expander", 0, REG_SZ,
            (BYTE*)path, (DWORD)((wcslen(path) + 1) * sizeof(wchar_t)));
    }
    else
    {
        RegDeleteValueW(hKey, L"EX-Expander");
    }

    RegCloseKey(hKey);
}