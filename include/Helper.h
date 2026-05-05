#pragma once
#include <shlobj.h>
#include <string>
std::wstring GetAppDataDir();
void HandleSettings(const std::string& jsonStr);
bool IsAppAllowed();