#pragma once
#include <string>

// Apply settings (DB + runtime + registry)
void ApplySettings(bool autoStart,
    const std::wstring& emojiSymbol,
    int maxPopup,
    const std::wstring& insertTrigger,
    const std::wstring& popupPosition);

// Build JS script to push settings into UI
std::wstring BuildSettingsScript();

// Registry helpers
bool GetAutoStartEnabled();
void SetAutoStart(bool enable);