#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "Database.h"

// ---------------------------------------------------------------------------
// Application-wide globals
// ---------------------------------------------------------------------------

extern HINSTANCE                    g_hInstance;
extern std::vector<Expansion>       g_FilteredExpansions;

// ---------------------------------------------------------------------------
// Runtime settings (applied from DB on startup; updated live from webUI)
// ---------------------------------------------------------------------------

extern wchar_t						g_TriggerChar;			// ':' or ';'  (default ':')
extern int							g_MaxPopupItems;		// 1-10        (default 5)
extern bool							g_InsertOnSpace;		// true=Space, false=trigger symbol
extern std::wstring					g_PopupPosition;		// true=Fixed, false=cursor

extern std::wstring					g_ScopeMode;			// "allow" or "block"
extern std::vector<std::wstring>	g_ScopeApps;			// exe names
