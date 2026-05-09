#pragma once
#include <string>
#include <vector>
#include <WebView2.h>

struct Expansion;

// ---------------------------------------------------------------------------
// WebViewBridge
//
// Serializes C++ data structures to JSON and sends them to the WebView2 UI.
//
// Two main functions:
//   - BuildJson          : converts expansion list to JSON array
//   - SendInstalledApps  : queries system apps and sends to UI
//
// These are called by WebMessageHandler when the UI requests data.
// ---------------------------------------------------------------------------

// Serialize a list of expansions to a JSON array string.
// Returns L"[]" on error.
//
// Output format:
//   [
//     { "id": 1, "token": ":smile:", "value": "😊", "type": "emoji", "tags": ["happy"] },
//     ...
//   ]
std::wstring BuildJson(const std::vector<Expansion>& data);

// Query installed applications (via Start Menu shortcuts) and send to WebView.
// Calls JavaScript: window.setInstalledApps([{ id, name, path }, ...])
// Returns false on failure.
bool SendInstalledAppsToUI(ICoreWebView2* webview);