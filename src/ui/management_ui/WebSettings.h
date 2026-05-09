#pragma once
#include <string>

// ---------------------------------------------------------------------------
// WebSettings
//
// Manages user preferences (emoji symbol, popup size, insertion mode, etc.).
//
// Three layers:
//   - Database     – persistent storage (survives app restart)
//   - Runtime      – global variables (used during app execution)
//   - Registry     – Windows autostart flag (checked by Windows at boot)
//
// Functions:
//   - ApplySettings        : save to all layers
//   - BuildSettingsScript  : generate JS to populate UI with current settings
//   - GetAutoStartEnabled  : read from Registry (Windows startup)
//   - SetAutoStart         : write to Registry (Windows startup)
// ---------------------------------------------------------------------------

// Apply user settings: persist to database, update runtime globals,
// and configure Windows autostart registry entry.
void ApplySettings(bool autoStart,
    const std::wstring& emojiSymbol,
    int maxPopup,
    const std::wstring& insertTrigger,
    const std::wstring& popupPosition);

// Build a JavaScript snippet that calls window.applySettings() with
// current settings. Reads from database, formats as JSON.
// Returns empty string on error.
std::wstring BuildSettingsScript();

// Check if Windows autostart is enabled for this app.
// Reads from HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run
bool GetAutoStartEnabled();

// Enable or disable Windows autostart.
// On enable: adds registry entry pointing to this exe
// On disable: removes registry entry
void SetAutoStart(bool enable);