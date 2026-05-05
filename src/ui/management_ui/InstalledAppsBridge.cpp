#include "InstalledAppsBridge.h"
#include "InstalledApps.h"
void SendInstalledAppsToUI(ICoreWebView2* webview)
{
    auto apps = GetInstalledApps();

    std::wstring json = L"[";

    for (size_t i = 0; i < apps.size(); ++i)
    {
        const auto& a = apps[i];

        json += L"{";
        json += L"\"id\":\"" + EscapeJson(a.id) + L"\",";
        json += L"\"name\":\"" + EscapeJson(a.name) + L"\",";
        json += L"\"path\":\"" + EscapeJson(a.path) + L"\"";
        json += L"}";

        if (i != apps.size() - 1)
            json += L",";
    }

    json += L"]";

    std::wstring script =
        L"window.setInstalledApps(" + json + L");";

    webview->ExecuteScript(script.c_str(), nullptr);
}

static std::wstring EscapeJson(const std::wstring& s)
{
    std::wstring out;
    for (auto c : s)
    {
        if (c == L'"') out += L"\\\"";
        else if (c == L'\\') out += L"\\\\";
        else out += c;
    }
    return out;
}
