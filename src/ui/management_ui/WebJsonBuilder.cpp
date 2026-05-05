#include "WebJsonBuilder.h"
#include <nlohmann/json.hpp>
#include <StringUtils.h>
#include "Database.h"
// ---------------------------------------------------------------------------
// JSON BUILDER  (now includes type + tags array)
// ---------------------------------------------------------------------------

std::wstring BuildJson(const std::vector<Expansion>& data)
{
    std::wstring json = L"[";
    json.reserve(data.size() * 96);

    for (size_t i = 0; i < data.size(); ++i)
    {
        const auto& e = data[i];

        json += L"{";
        json += L"\"id\":" + std::to_wstring(e.id) + L",";
        json += L"\"token\":\"" + JsonEscape(e.token) + L"\",";
        json += L"\"value\":\"" + JsonEscape(e.value) + L"\",";
        json += L"\"type\":\"" + JsonEscape(e.type) + L"\",";
        json += L"\"tags\":[";

        for (size_t j = 0; j < e.tags.size(); ++j)
        {
            json += L"\"" + JsonEscape(e.tags[j]) + L"\"";
            if (j != e.tags.size() - 1) json += L",";
        }

        json += L"]}";

        if (i != data.size() - 1) json += L",";
    }

    json += L"]";
    return json;
}
