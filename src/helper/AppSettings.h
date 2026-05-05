#pragma once
#include <string>
#include <vector>

// We use an Enum for ScopeMode instead of "allow"/"block" strings [38]
enum class ScopeMode {
    Allow,
    Block,
    Default
};

class AppSettings {
private:
    wchar_t m_triggerChar = L':';
    int m_maxPopupItems = 5;
    ScopeMode m_scopeMode = ScopeMode::Default;
    std::vector<std::wstring> m_scopeApps;

public:
    AppSettings() = default;

    // GETTERS (Const ensures we don't accidentally change settings while reading)
    wchar_t GetTriggerChar() const { return m_triggerChar; }
    int GetMaxPopupItems() const { return m_maxPopupItems; }
    ScopeMode GetScopeMode() const { return m_scopeMode; }
    const std::vector<std::wstring>& GetScopeApps() const { return m_scopeApps; }

    // SETTERS (Where we put validation logic)
    void SetTriggerChar(wchar_t c) { m_triggerChar = c; }
    void SetMaxPopupItems(int items) {
        // Clamp items between 3 and 10 as per business logic [25]
        if (items < 3) m_maxPopupItems = 3;
        else if (items > 10) m_maxPopupItems = 10;
        else m_maxPopupItems = items;
    }
    void SetScopeMode(ScopeMode mode) { m_scopeMode = mode; }
    void SetScopeApps(const std::vector<std::wstring>& apps) { m_scopeApps = apps; }
};