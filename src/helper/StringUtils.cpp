#include "StringUtils.h"
#include <windows.h>
#include <stringapiset.h>
#include <sstream>

// Removed: <wrl.h>, <WebView2.h>  – nothing here needs them

std::string WideToUtf8(const std::wstring& wide)
{
    if (wide.empty()) return {};

    int size = WideCharToMultiByte(CP_UTF8, 0,
        wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};

    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1,
        result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring Utf8ToWide(const char* utf8)
{
    if (!utf8) return {};

    int size = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (size <= 1) return {};

    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, result.data(), size);
    return result;
}

std::wstring JsonEscape(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size());

    for (wchar_t c : s)
    {
        switch (c)
        {
        case L'"':  out += L"\\\""; break;
        case L'\\': out += L"\\\\"; break;
        case L'\n': out += L"\\n";  break;
        case L'\r': out += L"\\r";  break;
        case L'\t': out += L"\\t";  break;
        default:    out += c;
        }
    }
    return out;
}

std::vector<std::wstring> SplitW(const std::wstring& str,
    wchar_t delim,
    int maxParts)
{
    std::vector<std::wstring> parts;
    size_t start = 0;

    while (start <= str.size())
    {
        if (maxParts > 0 && static_cast<int>(parts.size()) >= maxParts - 1)
        {
            parts.push_back(str.substr(start));
            break;
        }

        size_t pos = str.find(delim, start);
        if (pos == std::wstring::npos)
        {
            parts.push_back(str.substr(start));
            break;
        }

        parts.push_back(str.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}

std::vector<std::wstring> ParseTags(const std::wstring& csv)
{
    std::vector<std::wstring> out;
    if (csv.empty()) return out;

    std::wstringstream ss(csv);
    std::wstring item;

    while (std::getline(ss, item, L','))
    {
        size_t s = item.find_first_not_of(L" \t");
        if (s == std::wstring::npos) continue;
        size_t e = item.find_last_not_of(L" \t");
        out.push_back(item.substr(s, e - s + 1));
    }
    return out;
}

// JsonBoolW, JsonStringW, JsonIntW  –  DELETED.
// Use nlohmann::json directly at every call site instead.
// These were fragile (no space tolerance, prefix collisions)
// and you already depend on nlohmann everywhere else.