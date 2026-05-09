#pragma once
#include <string>
#include <shlobj.h>

// Returns (and creates if needed) the app data directory for EX-Expander
std::wstring GetAppDataDir();

// Parses a JSON settings payload and applies it to the global scope config
void HandleSettings(const std::string& jsonStr);

// Returns true if the currently active application should be processed
bool IsAppAllowed();