#include "InstalledApps.h"
#include "AppLog.h"

#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <filesystem>
#include <unordered_set>

#pragma comment(lib, "shell32.lib")

namespace
{
    std::wstring ToLower(const std::wstring& s)
    {
        std::wstring r = s;
        for (auto& c : r)
            c = towlower(c);
        return r;
    }

    // Resolves a .lnk shortcut to the full path of its target exe.
    // COM must already be initialised on the calling thread.
    std::wstring ResolveShortcut(const std::wstring& lnkPath)
    {
        IShellLinkW* psl = nullptr;
        if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr,
            CLSCTX_INPROC_SERVER, IID_IShellLinkW,
            reinterpret_cast<void**>(&psl))))
            return {};

        std::wstring result;

        IPersistFile* ppf = nullptr;
        if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile,
            reinterpret_cast<void**>(&ppf))))
        {
            if (SUCCEEDED(ppf->Load(lnkPath.c_str(), STGM_READ)))
            {
                wchar_t path[MAX_PATH] = {};
                if (SUCCEEDED(psl->GetPath(path, MAX_PATH, nullptr, 0)))
                    result = path;
            }
            ppf->Release();
        }

        psl->Release();
        return result;
    }

    void ScanFolder(const std::wstring& folder,
        std::vector<AppInfo>& out,
        std::unordered_set<std::wstring>& seen)
    {
        namespace fs = std::filesystem;

        try
        {
            for (const auto& entry :
                fs::recursive_directory_iterator(
                    folder,
                    fs::directory_options::skip_permission_denied))
            {
                if (!entry.is_regular_file()) continue;
                if (entry.path().extension() != L".lnk") continue;

                std::wstring exePath = ResolveShortcut(entry.path().wstring());
                if (exePath.empty()) continue;

                // Extract exe filename
                size_t sep = exePath.find_last_of(L"\\/");
                std::wstring exeName =
                    (sep == std::wstring::npos)
                    ? exePath
                    : exePath.substr(sep + 1);

                std::wstring id = ToLower(exeName);

                // Skip duplicates (same exe found in both start menus)
                if (!seen.insert(id).second)
                    continue;

                AppInfo app;
                app.id = id;
                app.name = entry.path().stem().wstring();
                app.path = exePath;

                out.push_back(std::move(app));
            }
        }
        catch (const std::exception& e)
        {
            AppLog::Warn(L"ScanFolder error in: " + folder);
            (void)e;
        }
    }
}

std::vector<AppInfo> GetInstalledApps()
{
    // Initialise COM once for this call
    HRESULT hrCom = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool comInit = SUCCEEDED(hrCom);

    std::vector<AppInfo>          apps;
    std::unordered_set<std::wstring> seen;

    wchar_t userStart[MAX_PATH] = {};
    wchar_t commonStart[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_STARTMENU, nullptr, 0, userStart);
    SHGetFolderPathW(nullptr, CSIDL_COMMON_STARTMENU, nullptr, 0, commonStart);

    ScanFolder(userStart, apps, seen);
    ScanFolder(commonStart, apps, seen);

    AppLog::Info(L"GetInstalledApps: found " +
        std::to_wstring(apps.size()) + L" apps");

    if (comInit)
        CoUninitialize();

    return apps;
}