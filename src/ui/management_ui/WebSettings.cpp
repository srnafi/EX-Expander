#include "WebSettings.h"
#include "Database.h"
#include "Globals.h"
#include "StringUtils.h"
#include "AppLog.h"

#include <windows.h>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ===========================================================================
// CONSTANTS
// ===========================================================================

namespace SettingsConst
{
    // Registry path for Windows autostart
    constexpr const wchar_t* kRegistryRunPath =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

    // Registry value name for this app
    constexpr const wchar_t* kRegistryAppName = L"EX-Expander";

    // Popup items clamping bounds
    constexpr int kMaxPopupMin = 3;
    constexpr int kMaxPopupMax = 10;
}

// ===========================================================================
// APPLY SETTINGS  –  persist to DB + update runtime globals + configure OS
// ===========================================================================

void ApplySettings(bool autoStart,
    const std::wstring& emojiSymbol,
    int maxPopup,
    const std::wstring& insertTrigger,
    const std::wstring& popupPosition)
{
    // Validate and clamp maxPopup
    maxPopup = std::clamp(maxPopup, SettingsConst::kMaxPopupMin, SettingsConst::kMaxPopupMax);

    AppLog::Info(L"ApplySettings: maxPopup=" + std::to_wstring(maxPopup) +
        L", insertTrigger=" + insertTrigger +
        L", popupPosition=" + popupPosition);

    // -----------------------------------------------------------------------
    // DATABASE  –  persistent storage
    // -----------------------------------------------------------------------
    DB_SetSetting(L"emojiSymbol", emojiSymbol);
    DB_SetSetting(L"maxPopup", std::to_wstring(maxPopup));
    DB_SetSetting(L"insertTrigger", insertTrigger);
    DB_SetSetting(L"popupPosition", popupPosition);

    // -----------------------------------------------------------------------
    // RUNTIME GLOBALS  –  used immediately during app execution
    // -----------------------------------------------------------------------
    g_TriggerChar = (!emojiSymbol.empty() && emojiSymbol[0] == L';') ? L';' : L':';
    g_MaxPopupItems = maxPopup;
    g_InsertOnSpace = (insertTrigger != L"symbol");
    g_PopupPosition = popupPosition;

    // -----------------------------------------------------------------------
    // WINDOWS REGISTRY  –  autostart (checked by Windows at boot)
    // -----------------------------------------------------------------------
    SetAutoStart(autoStart);

    AppLog::Info(L"ApplySettings: settings applied successfully");
}

// ===========================================================================
// BUILD SETTINGS SCRIPT  –  read DB and generate JS for WebView
// ===========================================================================

std::wstring BuildSettingsScript()
{
    try
    {
        // Read settings from database with sensible defaults
        std::wstring sym = DB_GetSetting(L"emojiSymbol", L":");
        std::wstring maxStr = DB_GetSetting(L"maxPopup", L"5");
        std::wstring trigger = DB_GetSetting(L"insertTrigger", L"space");
        std::wstring placement = DB_GetSetting(L"popupPosition", L"fixed");
        bool         autoStart = GetAutoStartEnabled();

        // Parse and validate maxPopup
        int maxVal = _wtoi(maxStr.c_str());
        maxVal = std::clamp(maxVal,
            SettingsConst::kMaxPopupMin,
            SettingsConst::kMaxPopupMax);

        // Build JSON object using nlohmann (safe escaping)
        json settings = {
            { "autoStart", autoStart },
            { "emojiSymbol", WideToUtf8(sym) },
            { "maxPopup", maxVal },
            { "insertTrigger", WideToUtf8(trigger) },
            { "popupPosition", WideToUtf8(placement) }
        };

        // Generate JS that calls the UI function
        std::string jsonStr = settings.dump();
        std::wstring script =
            L"if(window.applySettings){"
            L"  window.applySettings(" + Utf8ToWide(jsonStr.c_str()) + L");"
            L"}else{"
            L"  console.warn('window.applySettings not found in UI');"
            L"}";

        AppLog::Info(L"BuildSettingsScript: generated script successfully");
        return script;
    }
    catch (const json::exception& e)
    {
        AppLog::Error(L"BuildSettingsScript: JSON serialization failed: "
            + Utf8ToWide(e.what()));
        return L"";
    }
}

// ===========================================================================
// REGISTRY HELPERS  –  Windows autostart
// ===========================================================================

bool GetAutoStartEnabled()
{
    HKEY hKey = nullptr;

    // Open the Run registry key
    LONG result = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        SettingsConst::kRegistryRunPath,
        0,
        KEY_QUERY_VALUE,
        &hKey);

    if (result != ERROR_SUCCESS)
    {
        AppLog::Warn(L"GetAutoStartEnabled: RegOpenKeyEx failed");
        return false;
    }

    // Check if our app's registry value exists AND points to a valid path
    wchar_t storedPath[MAX_PATH] = {};
    DWORD size = sizeof(storedPath);

    bool enabled = false;

    result = RegQueryValueExW(
        hKey,
        SettingsConst::kRegistryAppName,
        nullptr,
        nullptr,
        reinterpret_cast<LPBYTE>(storedPath),
        &size);

    if (result == ERROR_SUCCESS)
    {
        // Value exists — verify it points to this exe
        wchar_t currentPath[MAX_PATH] = {};
        if (GetModuleFileNameW(nullptr, currentPath, MAX_PATH) > 0)
        {
            enabled = (wcscmp(storedPath, currentPath) == 0);

            if (!enabled)
            {
                AppLog::Warn(L"GetAutoStartEnabled: registry path doesn't match current exe");
            }
        }
    }

    RegCloseKey(hKey);
    return enabled;
}

void SetAutoStart(bool enable)
{
    HKEY hKey = nullptr;

    // Open the Run registry key (create if doesn't exist)
    LONG result = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        SettingsConst::kRegistryRunPath,
        0,
        KEY_SET_VALUE,
        &hKey);

    if (result != ERROR_SUCCESS)
    {
        AppLog::Error(L"SetAutoStart: RegOpenKeyEx failed");
        return;
    }

    if (enable)
    {
        // Get current executable path
        wchar_t exePath[MAX_PATH] = {};
        DWORD pathLen = GetModuleFileNameW(nullptr, exePath, MAX_PATH);

        if (pathLen == 0 || pathLen == MAX_PATH)
        {
            AppLog::Error(L"SetAutoStart: GetModuleFileNameW failed or path too long");
            RegCloseKey(hKey);
            return;
        }

        // Write exe path to registry
        DWORD dataSize = (pathLen + 1) * sizeof(wchar_t);

        result = RegSetValueExW(
            hKey,
            SettingsConst::kRegistryAppName,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(exePath),
            dataSize);

        if (result != ERROR_SUCCESS)
        {
            AppLog::Error(L"SetAutoStart: RegSetValueEx failed with code " +
                std::to_wstring(result));
        }
        else
        {
            AppLog::Info(L"SetAutoStart: enabled (registered exe path)");
        }
    }
    else
    {
        // Delete the registry value
        result = RegDeleteValueW(hKey, SettingsConst::kRegistryAppName);

        if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND)
        {
            AppLog::Error(L"SetAutoStart: RegDeleteValue failed with code " +
                std::to_wstring(result));
        }
        else
        {
            AppLog::Info(L"SetAutoStart: disabled (registry entry removed)");
        }
    }

    RegCloseKey(hKey);
}