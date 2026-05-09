#include "WebResources.h"
#include "resource.h"
#include "AppLog.h"

#include <windows.h>
#include <string>

// ===========================================================================
// RESOURCE LOADING  –  extract UTF-8 text from compiled resources
// ===========================================================================

std::wstring LoadResourceText(int resourceId)
{
    HMODULE hModule = GetModuleHandle(nullptr);
    if (!hModule)
    {
        AppLog::Error(L"LoadResourceText: GetModuleHandle failed");
        return L"";
    }

    // Find the resource by ID (type = RT_RCDATA)
    HRSRC hRes = FindResource(hModule, MAKEINTRESOURCE(resourceId), RT_RCDATA);
    if (!hRes)
    {
        AppLog::Error(L"LoadResourceText: FindResource failed for ID "
            + std::to_wstring(resourceId));
        return L"";
    }

    // Load the resource into memory
    HGLOBAL hData = LoadResource(hModule, hRes);
    if (!hData)
    {
        AppLog::Error(L"LoadResourceText: LoadResource failed for ID "
            + std::to_wstring(resourceId));
        return L"";
    }

    DWORD size = SizeofResource(hModule, hRes);
    const char* ptr = static_cast<const char*>(LockResource(hData));

    if (!ptr || size == 0)
    {
        AppLog::Error(L"LoadResourceText: LockResource returned null or zero size for ID "
            + std::to_wstring(resourceId));
        return L"";
    }

    // Convert UTF-8 to wide string
    int wlen = MultiByteToWideChar(CP_UTF8, 0, ptr, static_cast<int>(size), nullptr, 0);
    if (wlen == 0)
    {
        AppLog::Error(L"LoadResourceText: MultiByteToWideChar size calculation failed for ID "
            + std::to_wstring(resourceId));
        return L"";
    }

    std::wstring result(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, ptr, static_cast<int>(size), result.data(), wlen);

    return result;
}

// ===========================================================================
// HTML BUILDER  –  combine HTML, CSS, JS into one document
// ===========================================================================

std::wstring BuildHTML()
{
    // Load the three components
    std::wstring html = LoadResourceText(IDR_HTML);
    std::wstring css = LoadResourceText(IDR_CSS);
    std::wstring js = LoadResourceText(IDR_JS);

    // Validate base HTML
    if (html.empty())
    {
        AppLog::Error(L"BuildHTML: base HTML resource (IDR_HTML) is empty or failed to load");
        return L"";
    }

    // CSS defaults to empty if missing (warn but don't fail)
    if (css.empty())
    {
        AppLog::Warn(L"BuildHTML: CSS resource (IDR_CSS) is empty or failed to load");
    }

    // JS defaults to empty if missing (warn but don't fail)
    if (js.empty())
    {
        AppLog::Warn(L"BuildHTML: JS resource (IDR_JS) is empty or failed to load");
    }

    // Inject CSS into <head> (or prepend if no </head> tag)
    size_t headPos = html.find(L"</head>");
    if (headPos != std::wstring::npos)
    {
        html.insert(headPos, L"<style>" + css + L"</style>");
    }
    else
    {
        AppLog::Warn(L"BuildHTML: </head> tag not found, prepending CSS");
        html = L"<style>" + css + L"</style>" + html;
    }

    // Inject JS before </body> (or append if no </body> tag)
    size_t bodyPos = html.find(L"</body>");
    if (bodyPos != std::wstring::npos)
    {
        html.insert(bodyPos, L"<script>" + js + L"</script>");
    }
    else
    {
        AppLog::Warn(L"BuildHTML: </body> tag not found, appending JS");
        html += L"<script>" + js + L"</script>";
    }

    AppLog::Info(L"BuildHTML: HTML built successfully, length: " + std::to_wstring(html.size()));

    return html;
}