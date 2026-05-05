#pragma once
#include <string>

// JS → C++ message handler
void HandleMessage(const std::wstring& msg);

// Safe JS execution helper
void SafeExecuteScript(const std::wstring& script);

// Send all expansions to UI
void SendAllExpansions();