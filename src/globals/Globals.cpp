#include "Globals.h"

// ---------------------------------------------------------------------------
// Application-wide global definitions
// ---------------------------------------------------------------------------

HINSTANCE               g_hInstance = nullptr;
std::vector<Expansion>  g_FilteredExpansions;

// ---------------------------------------------------------------------------
// Runtime settings – defaults match DEFAULT_SETTINGS in app.js
// ---------------------------------------------------------------------------

wchar_t						g_TriggerChar = L':';    // Shift+; on US layout
int							g_MaxPopupItems = 5;
bool						g_InsertOnSpace = true;
std::wstring				g_PopupPosition = L"fixed";

std::wstring				g_ScopeMode = L"block";
std::vector<std::wstring>	g_ScopeApps;