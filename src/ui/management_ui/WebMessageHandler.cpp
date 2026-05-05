#include "WebMessageHandler.h"
#include <StringUtils.h>
#include <Database.h>
#include <WebSettings.h>
#include <Helper.h>
#include <WebViewManager.h>
#include <WebJsonBuilder.h>

// ---------------------------------------------------------------------------
// MESSAGE HANDLER  (JS → C++)
//
// Protocol (pipe-delimited):
//   getAll
//   insert|token|value|tags|type
//   update|id|token|value|tags|type
//   delete|id
//   search|query
//   settings|{jsonObject}
// ---------------------------------------------------------------------------

void HandleMessage(const std::wstring& msg)
{
    if (msg.empty()) return;

    // ---- GET ALL ----
    if (msg == L"getAll")
    {
        SendAllExpansions();
        return;
    }

    // ---- INSERT  (insert|token|value|tags|type) ----
    if (msg.rfind(L"insert|", 0) == 0)
    {
        // Split into exactly 5 parts from position 7 onward:
        // [token, value, tags, type]
        auto parts = SplitW(msg.substr(7), L'|', 4);
        if (parts.size() < 4) return;

        std::wstring token = parts[0];
        std::wstring value = parts[1];
        std::wstring tagsStr = parts[2];
        std::wstring type = parts[3];

        if (!token.empty() && !value.empty())
        {
            auto tags = ParseTags(tagsStr);
            DB_AddExpansion(token, value, type, tags);
            SendAllExpansions();
        }
        return;
    }

    // ---- DELETE  (delete|id) ----
    if (msg.rfind(L"delete|", 0) == 0)
    {
        int id = _wtoi(msg.substr(7).c_str());
        if (id > 0)
        {
            DB_DeleteExpansion(id);
            SendAllExpansions();
        }
        return;
    }

    // ---- UPDATE  (update|id|token|value|tags|type) ----
    if (msg.rfind(L"update|", 0) == 0)
    {
        // Split into 5 parts from position 7: [id, token, value, tags, type]
        auto parts = SplitW(msg.substr(7), L'|', 5);
        if (parts.size() < 5) return;

        int          id = _wtoi(parts[0].c_str());
        std::wstring token = parts[1];
        std::wstring value = parts[2];
        std::wstring tagsStr = parts[3];
        std::wstring type = parts[4];

        if (id > 0 && !token.empty() && !value.empty())
        {
            auto tags = ParseTags(tagsStr);
            DB_UpdateExpansion(id, token, value, type, tags);
            SendAllExpansions();
        }
        return;
    }

    // ---- SEARCH  (search|query  or  search|query|section) ----
    if (msg.rfind(L"search|", 0) == 0)
    {
        // Section filtering is done client-side; we only need the query
        std::wstring rest = msg.substr(7);
        size_t       pipe = rest.find(L'|');
        std::wstring query = (pipe != std::wstring::npos)
            ? rest.substr(0, pipe)
            : rest;

        auto results = DB_Search(query);
        SafeExecuteScript(L"window.loadFromNative(" + BuildJson(results) + L");");
        return;
    }

    // ---- SETTINGS  (settings|{jsonObject}) ----
    if (msg.rfind(L"settings|", 0) == 0)
    {
        std::wstring jsonStr = msg.substr(9);

        bool         autoStart = JsonBoolW(jsonStr, L"autoStart");
        std::wstring emojiSymbol = JsonStringW(jsonStr, L"emojiSymbol");
        int          maxPopup = JsonIntW(jsonStr, L"maxPopup");
        std::wstring insertTrigger = JsonStringW(jsonStr, L"insertTrigger");
        std::wstring popupPosition = JsonStringW(jsonStr, L"popupPosition");

        if (emojiSymbol.empty())   emojiSymbol = L":";
        if (maxPopup <= 0)         maxPopup = 5;
        if (insertTrigger.empty()) insertTrigger = L"space";
        if (popupPosition.empty()) popupPosition = L"fixed";

        ApplySettings(autoStart, emojiSymbol, maxPopup, insertTrigger, popupPosition);

        // ← ADD THIS: also handle scope (was in Helper.cpp's HandleSettings)
        std::string narrowJson(jsonStr.begin(), jsonStr.end());
        HandleSettings(narrowJson);  // updates g_ScopeMode + g_ScopeApps

        return;
    }
}

// ---------------------------------------------------------------------------
// SAFE SCRIPT EXECUTION
// ---------------------------------------------------------------------------

void SafeExecuteScript(const std::wstring& script)
{
    if (g_webview)
        g_webview->ExecuteScript(script.c_str(), nullptr);
}

// ---------------------------------------------------------------------------
// SEND ALL EXPANSIONS
// ---------------------------------------------------------------------------

void SendAllExpansions()
{
    auto data = DB_GetAllExpansions();
    SafeExecuteScript(L"window.loadFromNative(" + BuildJson(data) + L");");
}
