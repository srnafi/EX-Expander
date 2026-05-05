#include <windows.h>
#include "StringUtils.h"
#include <stringapiset.h>
#include <sstream>
#include <wrl.h>
#include <WebView2.h>



// ---------------------------------------------------------------------------
// UTF CONVERSION HELPERS
// ---------------------------------------------------------------------------

std::string WideToUtf8(const std::wstring& wide)
{
    if (wide.empty()) return {};

    int size = WideCharToMultiByte(
        CP_UTF8, 0,
        wide.c_str(), -1,
        nullptr, 0,
        nullptr, nullptr);

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

// ---------------------------------------------------------------------------
// JSON ESCAPE
// ---------------------------------------------------------------------------

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

// Split str by delimiter; if maxParts > 0, stop after that many parts
std::vector<std::wstring> SplitW(const std::wstring& str,
    wchar_t delim,
    int maxParts)
{
    std::vector<std::wstring> parts;
    size_t start = 0;

    while (start <= str.size())
    {
        if (maxParts > 0 && (int)parts.size() >= maxParts - 1)
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

// Parse comma-delimited tag string into a vector, trimming whitespace
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

// ---------------------------------------------------------------------------
// SIMPLE JSON HELPERS  (wide-string based, for known settings format)
// ---------------------------------------------------------------------------

bool JsonBoolW(const std::wstring& json, const std::wstring& key)
{
    return json.find(L"\"" + key + L"\":true") != std::wstring::npos;
}

std::wstring JsonStringW(const std::wstring& json, const std::wstring& key)
{
    std::wstring search = L"\"" + key + L"\":\"";
    size_t pos = json.find(search);
    if (pos == std::wstring::npos) return L"";
    pos += search.size();
    size_t end = json.find(L'"', pos);
    return (end != std::wstring::npos) ? json.substr(pos, end - pos) : L"";
}

int JsonIntW(const std::wstring& json, const std::wstring& key)
{
    std::wstring search = L"\"" + key + L"\":";
    size_t pos = json.find(search);
    if (pos == std::wstring::npos) return -1;
    return _wtoi(json.c_str() + pos + search.size());
}