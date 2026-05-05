#include "InstalledApps.h"

#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <filesystem>
#include <vector>
#include <string>

#pragma comment(lib, "shell32.lib")

static std::wstring ToLower(const std::wstring& s)
{
    std::wstring r = s;
    for (auto& c : r) c = towlower(c);
    return r;
}

// Resolve .lnk → actual .exe path
static std::wstring ResolveShortcut(const std::wstring& lnkPath)
{
    std::wstring result;

    CoInitialize(nullptr);

    IShellLinkW* psl;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
        IID_IShellLinkW, (LPVOID*)&psl)))
    {
        IPersistFile* ppf;
        if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf)))
        {
            if (SUCCEEDED(ppf->Load(lnkPath.c_str(), STGM_READ)))
            {
                wchar_t path[MAX_PATH];
                if (SUCCEEDED(psl->GetPath(path, MAX_PATH, nullptr, 0)))
                {
                    result = path;
                }
            }
            ppf->Release();
        }
        psl->Release();
    }

    CoUninitialize();
    return result;
}

static void ScanFolder(const std::wstring& folder, std::vector<AppInfo>& out)
{
    namespace fs = std::filesystem;

    for (const auto& entry : fs::recursive_directory_iterator(folder))
    {
        if (!entry.is_regular_file()) continue;

        if (entry.path().extension() == L".lnk")
        {
            std::wstring exePath = ResolveShortcut(entry.path().wstring());
            if (exePath.empty()) continue;

            std::wstring exeName =
                exePath.substr(exePath.find_last_of(L"\\/") + 1);

            AppInfo app;
            app.id = ToLower(exeName);             // IMPORTANT
            app.name = entry.path().stem().wstring();
            app.path = exePath;

            out.push_back(app);
        }
    }
}

std::vector<AppInfo> GetInstalledApps()
{
    std::vector<AppInfo> apps;

    wchar_t userStart[MAX_PATH];
    wchar_t commonStart[MAX_PATH];

    SHGetFolderPathW(nullptr, CSIDL_STARTMENU, nullptr, 0, userStart);
    SHGetFolderPathW(nullptr, CSIDL_COMMON_STARTMENU, nullptr, 0, commonStart);

    ScanFolder(userStart, apps);
    ScanFolder(commonStart, apps);

    return apps;
}