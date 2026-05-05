#pragma once
#include <string>
#include <WebView2.h>

// Send installed apps to WebView
void SendInstalledAppsToUI(ICoreWebView2* webview);

// JSON escape helper (kept same behavior)
std::wstring EscapeJson(const std::wstring& s);