#include "resource.h"
#include <wrl.h>
#include <sstream>
#include <WebView2.h>
using namespace Microsoft::WRL;

// ---------------------------------------------------------------------------
// RESOURCE LOADING
// ---------------------------------------------------------------------------

std::wstring LoadResourceText(int id)
{
    HMODULE hModule = GetModuleHandle(nullptr);

    HRSRC  res = FindResource(hModule, MAKEINTRESOURCE(id), RT_RCDATA);
    if (!res) return L"";

    HGLOBAL data = LoadResource(hModule, res);
    if (!data) return L"";

    DWORD       size = SizeofResource(hModule, res);
    const char* ptr = (const char*)LockResource(data);

    if (!ptr || size == 0) return L"";

    int          wlen = MultiByteToWideChar(CP_UTF8, 0, ptr, size, nullptr, 0);
    std::wstring wide(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, ptr, size, wide.data(), wlen);

    return wide;
}

// ---------------------------------------------------------------------------
// HTML BUILDER
// ---------------------------------------------------------------------------

std::wstring BuildHTML()
{
    std::wstring html = LoadResourceText(IDR_HTML);
    std::wstring css = LoadResourceText(IDR_CSS);
    std::wstring js = LoadResourceText(IDR_JS);

    if (html.find(L"</head>") != std::wstring::npos)
        html.insert(html.find(L"</head>"), L"<style>" + css + L"</style>");
    else
        html = L"<style>" + css + L"</style>" + html;

    if (html.find(L"</body>") != std::wstring::npos)
        html.insert(html.find(L"</body>"), L"<script>" + js + L"</script>");
    else
        html += L"<script>" + js + L"</script>";

    return html;
}