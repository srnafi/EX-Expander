#include "WebViewBridge.h"
#include "Database.h"
#include "InstalledApps.h"
#include "StringUtils.h"
#include "AppLog.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ===========================================================================
// NLOHMANN SERIALIZER FOR EXPANSION
//
// Teaches nlohmann how to convert an Expansion struct to JSON.
// ===========================================================================

namespace nlohmann
{
    template <>
    struct adl_serializer<Expansion>
    {
        static void to_json(json& j, const Expansion& expansion)
        {
            // Convert wide-string tags to UTF-8 array
            json::array_t tagsArray;
            for (const auto& tag : expansion.tags)
                tagsArray.push_back(WideToUtf8(tag));

            j = json{
                { "id",    expansion.id },
                { "token", WideToUtf8(expansion.token) },
                { "value", WideToUtf8(expansion.value) },
                { "type",  WideToUtf8(expansion.type) },
                { "tags",  tagsArray }
            };
        }
    };
}

// ===========================================================================
// BUILD JSON  –  expansion list → JSON array
// ===========================================================================

std::wstring BuildJson(const std::vector<Expansion>& data)
{
    try
    {
        // Use nlohmann to build JSON array (handles all escaping)
        json j = json::array();

        for (const auto& expansion : data)
            j.push_back(expansion);  // uses adl_serializer above

        // Serialize to UTF-8
        std::string jsonUtf8 = j.dump();

        // Convert to wide string for WebView2 ExecuteScript
        std::wstring result = Utf8ToWide(jsonUtf8.c_str());

        AppLog::Info(L"BuildJson: serialized " + std::to_wstring(data.size()) + L" expansions");

        return result;
    }
    catch (const json::exception& e)
    {
        AppLog::Error(L"BuildJson: JSON serialization failed: " + Utf8ToWide(e.what()));
        return L"[]";
    }
}

// ===========================================================================
// SEND INSTALLED APPS  –  system apps → WebView
// ===========================================================================

bool SendInstalledAppsToUI(ICoreWebView2* webview)
{
    if (!webview)
    {
        AppLog::Error(L"SendInstalledAppsToUI: webview is null");
        return false;
    }

    // Query system for installed apps (scans Start Menu shortcuts)
    auto apps = GetInstalledApps();

    AppLog::Info(L"Sending " + std::to_wstring(apps.size()) + L" installed apps to WebView");

    // Build JSON array
    json j = json::array();

    for (const auto& app : apps)
    {
        j.push_back({
            { "id",   WideToUtf8(app.id) },
            { "name", WideToUtf8(app.name) },
            { "path", WideToUtf8(app.path) }
            });
    }

    std::string jsonUtf8 = j.dump();
    std::wstring jsonWide = Utf8ToWide(jsonUtf8.c_str());

    // Execute JavaScript: window.setInstalledApps([...])
    std::wstring script = L"window.setInstalledApps(" + jsonWide + L");";

    HRESULT hr = webview->ExecuteScript(script.c_str(), nullptr);

    if (FAILED(hr))
    {
        AppLog::Error(L"SendInstalledAppsToUI: ExecuteScript failed with HRESULT "
            + std::to_wstring(hr));
        return false;
    }

    AppLog::Info(L"Installed apps sent to WebView successfully");
    return true;
}