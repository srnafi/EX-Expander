#include "WebMessageHandler.h"
#include "Database.h"
#include "WebSettings.h"
#include "Helper.h"
#include "WebViewManager.h"
#include "WebViewBridge.h"
#include "StringUtils.h"
#include "AppLog.h"
#include "Globals.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ===========================================================================
// CONSTANTS
// ===========================================================================

namespace
{
    // JavaScript callback function names (change here if you rename in JS)
    constexpr const wchar_t* kJsLoadFromNative = L"window.loadFromNative";

    // Settings validation bounds
    constexpr int kMaxPopupMin = 1;
    constexpr int kMaxPopupMax = 20;
}

// ===========================================================================
// MESSAGE HANDLER  (JS → C++)
// ===========================================================================

void HandleMessage(const std::wstring& msg)
{
    if (msg.empty())
    {
        AppLog::Warn(L"HandleMessage: received empty message");
        return;
    }

    // -----------------------------------------------------------------------
    // GET ALL  –  send entire expansion list to UI
    // -----------------------------------------------------------------------
    if (msg == L"getAll")
    {
        AppLog::Info(L"HandleMessage: getAll");
        SendAllExpansions();
        return;
    }

    // -----------------------------------------------------------------------
    // INSERT  –  add new expansion
    //   Format: insert|token|value|tags|type
    // -----------------------------------------------------------------------
    if (msg.rfind(L"insert|", 0) == 0)
    {
        auto parts = SplitW(msg.substr(7), L'|', 4);

        if (parts.size() < 4)
        {
            AppLog::Warn(L"HandleMessage: insert command malformed (expected 4 parts)");
            return;
        }

        std::wstring token = parts[0];
        std::wstring value = parts[1];
        std::wstring tagsStr = parts[2];
        std::wstring type = parts[3];

        if (token.empty() || value.empty())
        {
            AppLog::Warn(L"HandleMessage: insert failed — token or value is empty");
            return;
        }

        auto tags = ParseTags(tagsStr);

        if (DB_AddExpansion(token, value, type, tags))
        {
            AppLog::Info(L"HandleMessage: inserted expansion '" + token + L"'");
            SendAllExpansions();
        }
        else
        {
            AppLog::Error(L"HandleMessage: DB_AddExpansion failed for '" + token + L"'");
        }

        return;
    }

    // -----------------------------------------------------------------------
    // UPDATE  –  edit existing expansion
    //   Format: update|id|token|value|tags|type
    // -----------------------------------------------------------------------
    if (msg.rfind(L"update|", 0) == 0)
    {
        auto parts = SplitW(msg.substr(7), L'|', 5);

        if (parts.size() < 5)
        {
            AppLog::Warn(L"HandleMessage: update command malformed (expected 5 parts)");
            return;
        }

        int          id = _wtoi(parts[0].c_str());
        std::wstring token = parts[1];
        std::wstring value = parts[2];
        std::wstring tagsStr = parts[3];
        std::wstring type = parts[4];

        if (id <= 0)
        {
            AppLog::Warn(L"HandleMessage: update failed — invalid id: " + parts[0]);
            return;
        }

        if (token.empty() || value.empty())
        {
            AppLog::Warn(L"HandleMessage: update failed — token or value is empty");
            return;
        }

        auto tags = ParseTags(tagsStr);

        if (DB_UpdateExpansion(id, token, value, type, tags))
        {
            AppLog::Info(L"HandleMessage: updated expansion id=" + std::to_wstring(id));
            SendAllExpansions();
        }
        else
        {
            AppLog::Error(L"HandleMessage: DB_UpdateExpansion failed for id=" + std::to_wstring(id));
        }

        return;
    }

    // -----------------------------------------------------------------------
    // DELETE  –  remove expansion by id
    //   Format: delete|id
    // -----------------------------------------------------------------------
    if (msg.rfind(L"delete|", 0) == 0)
    {
        int id = _wtoi(msg.substr(7).c_str());

        if (id <= 0)
        {
            AppLog::Warn(L"HandleMessage: delete failed — invalid id: " + msg.substr(7));
            return;
        }

        if (DB_DeleteExpansion(id))
        {
            AppLog::Info(L"HandleMessage: deleted expansion id=" + std::to_wstring(id));
            SendAllExpansions();
        }
        else
        {
            AppLog::Error(L"HandleMessage: DB_DeleteExpansion failed for id=" + std::to_wstring(id));
        }

        return;
    }

    // -----------------------------------------------------------------------
    // SEARCH  –  query expansions and send filtered results
    //   Format: search|query  (optional second pipe for section, ignored)
    // -----------------------------------------------------------------------
    if (msg.rfind(L"search|", 0) == 0)
    {
        std::wstring rest = msg.substr(7);
        size_t       pipe = rest.find(L'|');
        std::wstring query = (pipe != std::wstring::npos)
            ? rest.substr(0, pipe)
            : rest;

        AppLog::Info(L"HandleMessage: search query='" + query + L"'");

        auto results = DB_Search(query);
        SafeExecuteScript(std::wstring(kJsLoadFromNative) + L"(" + BuildJson(results) + L");");

        return;
    }

    // -----------------------------------------------------------------------
    // SETTINGS  –  apply user preferences
    //   Format: settings|{jsonObject}
    // -----------------------------------------------------------------------
    if (msg.rfind(L"settings|", 0) == 0)
    {
        std::wstring jsonStr = msg.substr(9);

        try
        {
            // Parse JSON once
            auto j = json::parse(WideToUtf8(jsonStr));

            // Extract general settings
            bool         autoStart = j.value("autoStart", false);
            std::wstring emojiSymbol = Utf8ToWide(j.value("emojiSymbol", std::string(":")).c_str());
            int          maxPopup = j.value("maxPopup", 5);
            std::wstring insertTrigger = Utf8ToWide(j.value("insertTrigger", std::string("space")).c_str());
            std::wstring popupPosition = Utf8ToWide(j.value("popupPosition", std::string("fixed")).c_str());

            // Validate and clamp
            if (emojiSymbol.empty())   emojiSymbol = L":";
            if (insertTrigger.empty()) insertTrigger = L"space";
            if (popupPosition.empty()) popupPosition = L"fixed";

            maxPopup = std::clamp(maxPopup, kMaxPopupMin, kMaxPopupMax);

            // Apply general settings (calls DB_SetSetting, updates globals)
            ApplySettings(autoStart, emojiSymbol, maxPopup, insertTrigger, popupPosition);

            // Extract scope settings (app allow/block list)
            std::wstring scopeMode = Utf8ToWide(j.value("scopeMode", std::string("allow")).c_str());
            g_ScopeMode = scopeMode;

            g_ScopeApps.clear();
            if (j.contains("scopeApps") && j["scopeApps"].is_array())
            {
                for (const auto& app : j["scopeApps"])
                {
                    if (!app.is_string()) continue;

                    std::wstring appName = Utf8ToWide(app.get<std::string>().c_str());
                    for (auto& c : appName)
                        c = towlower(c);

                    g_ScopeApps.push_back(appName);
                }
            }

            AppLog::Info(L"Settings applied: scopeMode=" + scopeMode
                + L", apps=" + std::to_wstring(g_ScopeApps.size()));
        }
        catch (const json::exception& e)
        {
            AppLog::Error(L"Settings JSON parse failed: " + Utf8ToWide(e.what()));
        }

        return;
    }

    // -----------------------------------------------------------------------
    // UNKNOWN COMMAND
    // -----------------------------------------------------------------------
    AppLog::Warn(L"HandleMessage: unknown command: " + msg.substr(0, 20));
}

// ===========================================================================
// SAFE SCRIPT EXECUTION
// ===========================================================================

bool SafeExecuteScript(const std::wstring& script)
{
    if (!g_webview)
    {
        AppLog::Error(L"SafeExecuteScript: g_webview is null");
        return false;
    }

    HRESULT hr = g_webview->ExecuteScript(script.c_str(), nullptr);

    if (FAILED(hr))
    {
        AppLog::Error(L"SafeExecuteScript: ExecuteScript failed with HRESULT "
            + std::to_wstring(hr));
        return false;
    }

    return true;
}

// ===========================================================================
// SEND ALL EXPANSIONS
// ===========================================================================

void SendAllExpansions()
{
    auto data = DB_GetAllExpansions();
    SafeExecuteScript(std::wstring(kJsLoadFromNative) + L"(" + BuildJson(data) + L");");
}