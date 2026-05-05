#pragma once
#include <string>
#include <vector>

struct AppInfo
{
    std::wstring id;    // exe name (e.g. chrome.exe)
    std::wstring name;  // display name
    std::wstring path;  // full exe path
};

std::vector<AppInfo> GetInstalledApps();